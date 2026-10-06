#include "parser.hpp"
#include "codegen.hpp"
#include <algorithm>
#include <cstring>
#include <functional>
#include <set>

namespace cxxsnippets
{
struct TemplateDecl
{
    struct Parameter
    {
        std::string name;
        Type *fallback = nullptr;
        bool pack = false;
        Type *marker = nullptr;
    };
    std::vector<Parameter> parameters;
    std::vector<Token> declaration;
    Namespace *scope = nullptr;
    Type *owner = nullptr;
};
namespace
{
class Parser
{
    TranslationUnit &u;
    TypeTable &ty;
    const std::vector<Token> &ts;
    size_t p = 0;
    Namespace *scope;
    FuncDecl *function = nullptr;
    std::vector<std::map<std::string, VarDecl *>> locals;
    std::vector<size_t> localBoundaries;
    int loops = 0, switches = 0;
    // Recursive descent on the game thread: nesting is bounded so a corrupt or hostile file gets a
    // diagnostic instead of overflowing the stack (which no handler can catch).
    int depth = 0;
    struct DepthGuard
    {
        Parser &parser;
        DepthGuard(Parser &parser, const Loc &loc) : parser(parser)
        {
            if (++parser.depth > 256)
                Error(loc, "expression or statement nested too deeply");
        }
        ~DepthGuard()
        {
            --parser.depth;
        }
    };
    Type *autoType;
    struct Declarator
    {
        std::string name;
        Type *type;
        std::vector<VarDecl *> params;
        std::vector<Expr *> defaults;
        Loc loc;
        CallConv conv = CallConv::Default;
    };
    struct Spec
    {
        Type *type = nullptr;
        bool constant = false, internal = false, external = false, alias = false, constexprFunction = false;
        CallConv conv = CallConv::Default;
        std::string link, intrinsic, release;
        bool value = false, address = false;
    };
    struct Deferred
    {
        FuncDecl *fn;
        Namespace *scope;
        size_t first, last;
    };
    std::vector<Deferred> deferred;
    std::map<std::string, Type *> substitutions;
    std::map<Type *, std::vector<Type *>> packs;
    Type *classType = nullptr;
    std::string pendingIntrinsic, pendingRelease;
    bool pendingValue = false, pendingAddress = false;
    const Token &Peek(size_t n = 0) const
    {
        return ts[std::min(p + n, ts.size() - 1)];
    }
    bool Is(const char *s) const
    {
        return Peek().Is(s);
    }
    bool Eat(const char *s)
    {
        if (!Is(s))
            return false;
        ++p;
        return true;
    }
    Token Take()
    {
        if (Peek().kind == Tok::End)
            Error(Peek().loc, "unexpected end of source");
        return ts[p++];
    }
    void Need(const char *s)
    {
        if (!Eat(s))
            Error(Peek().loc, std::string("expected '") + s + "', got '" + Peek().text + "'");
    }
    std::string Ident()
    {
        auto t = Take();
        if (t.kind != Tok::Ident)
            Error(t.loc, "identifier expected");
        return t.text;
    }
    std::string Qualified(const std::string &n) const
    {
        return scope->name.empty() ? n : scope->name + "::" + n;
    }
    Namespace *Child(Namespace *s, const std::string &n)
    {
        auto &v = s->children[n];
        if (!v)
        {
            v = u.arena.New<Namespace>();
            v->parent = s;
            v->name = s->name.empty() ? n : s->name + "::" + n;
        }
        return v;
    }
    template <class T> T *Node()
    {
        return u.arena.New<T>();
    }
    Expr *New(EK kind, Type *t, Loc loc)
    {
        auto e = Node<Expr>();
        e->kind = kind;
        e->type = t;
        e->loc = loc;
        return e;
    }
    Expr *Number(uint64_t v, Type *t, Loc loc)
    {
        auto e = New(EK::Number, t, loc);
        e->value = v;
        return e;
    }
    Stmt *Statement(SK k, Loc l)
    {
        auto s = Node<Stmt>();
        s->kind = k;
        s->loc = l;
        return s;
    }
    struct Name
    {
        Namespace *ns;
        std::string name;
        bool qualified = false;
    };
    Name ReadName()
    {
        bool absolute = Eat("::");
        std::string n = Ident();
        Namespace *ns = absolute ? &u.root : scope;
        bool qualified = absolute;
        while (Eat("::"))
        {
            Namespace *found = nullptr;
            for (auto s = ns; s && !found; s = qualified ? nullptr : s->parent)
            {
                auto it = s->children.find(n);
                if (it != s->children.end())
                    found = it->second;
                if (!found)
                    for (auto imp : s->imports)
                    {
                        auto jt = imp->children.find(n);
                        if (jt != imp->children.end())
                        {
                            if (found && found != jt->second)
                                Error(Peek().loc, "ambiguous namespace or type");
                            found = jt->second;
                        }
                    }
            }
            if (!found)
                Error(Peek().loc, "unknown namespace or type '" + n + "'");
            ns = found;
            n = Ident();
            qualified = true;
        }
        return {ns, n, qualified};
    }
    template <class T, class M> T Lookup(Name n, M member)
    {
        for (auto s = n.ns; s; s = n.qualified ? nullptr : s->parent)
        {
            auto &map = s->*member;
            auto it = map.find(n.name);
            if (it != map.end())
                return it->second;
            T found{};
            for (auto imp : s->imports)
            {
                auto &im = imp->*member;
                auto jt = im.find(n.name);
                if (jt != im.end())
                {
                    if (found != T{} && found != jt->second)
                        Error(Peek().loc, "ambiguous name '" + n.name + "'");
                    found = jt->second;
                }
            }
            if (found != T{})
                return found;
        }
        return T{};
    }
    Type *NamedType()
    {
        if (Peek().kind != Tok::Ident && !Is("::"))
            return nullptr;
        auto sub = substitutions.find(Peek().text);
        if (sub != substitutions.end() && !Peek(1).Is("::"))
        {
            ++p;
            return sub->second;
        }
        size_t save = p;
        auto n = ReadName();
        auto t = Lookup<Type *>(n, &Namespace::types);
        if (!t)
            p = save;
        return t;
    }
    bool TypeStart()
    {
        if (Is("[["))
            return true;
        static const std::set<std::string> words = {
            "void",   "bool",     "char",  "wchar_t", "char16_t", "char32_t",  "short",    "int",    "long",
            "signed", "unsigned", "float", "double",  "__int64",  "const",     "volatile", "auto",   "struct",
            "class",  "union",    "enum",  "static",  "extern",   "constexpr", "inline",   "typedef"};
        if (words.count(Peek().text))
            return true;
        if (Peek().kind != Tok::Ident && !Is("::"))
            return false;
        size_t save = p;
        auto t = NamedType();
        p = save;
        return t != nullptr;
    }
    // `Type(args)...;` that cannot be a declaration is an expression statement, e.g.
    // hook::pattern("..").for_each_result(...); a literal argument, or `.`/`->` after the
    // parentheses, rules out a declarator.
    bool FunctionalCastStatement()
    {
        if (Peek().kind != Tok::Ident && !Is("::"))
            return false;
        size_t save = p;
        bool cast = false;
        if (NamedType() && Is("("))
        {
            auto k = Peek(1).kind;
            cast = k == Tok::Int || k == Tok::Float || k == Tok::Char || k == Tok::String;
            if (!cast)
            {
                size_t depth = 0, n = 0;
                for (;; ++n)
                {
                    auto &t = Peek(n);
                    if (t.kind == Tok::End)
                        break;
                    if (t.Is("("))
                        ++depth;
                    else if (t.Is(")") && --depth == 0)
                        break;
                }
                cast = Peek(n + 1).Is(".") || Peek(n + 1).Is("->");
            }
        }
        p = save;
        return cast;
    }
    CallConv Convention()
    {
        if (Eat("__cdecl"))
            return CallConv::Cdecl;
        if (Eat("__stdcall"))
            return CallConv::Stdcall;
        if (Eat("__fastcall"))
            return CallConv::Fastcall;
        if (Eat("__thiscall"))
            return CallConv::Thiscall;
        return CallConv::Default;
    }
    std::string Attributes()
    {
        std::string link;
        while (Eat("[["))
        {
            Need("cxxsnippets");
            Need("::");
            std::string attr = Ident();
            if (attr == "address")
                pendingAddress = true;
            else if (attr == "value")
                pendingValue = true;
            else
            {
                if (attr != "link" && attr != "intrinsic" && attr != "handle")
                    Error(Peek().loc, "unknown binding attribute '" + attr + "'");
                Need("(");
                auto t = Take();
                if (t.kind != Tok::String)
                    Error(t.loc, "symbol string expected");
                if (attr == "link")
                    link = t.str;
                else if (attr == "intrinsic")
                    pendingIntrinsic = t.str;
                else
                    pendingRelease = t.str;
                Need(")");
            }
            Need("]]");
        }
        return link;
    }
    Spec Specs()
    {
        Spec s;
        s.link = Attributes();
        s.intrinsic = std::move(pendingIntrinsic);
        s.release = std::move(pendingRelease);
        s.value = pendingValue;
        s.address = pendingAddress;
        pendingValue = pendingAddress = false;
        bool uns = false, sign = false;
        int longs = 0;
        bool shortWord = false;
        for (;;)
        {
            if (Eat("constexpr"))
            {
                s.constant = true;
                s.constexprFunction = true;
            }
            else if (Eat("const"))
                s.constant = true;
            else if (Eat("volatile") || Eat("inline"))
            {
            }
            else if (Eat("static"))
                s.internal = true;
            else if (Eat("extern"))
                s.external = true;
            else if (Eat("typedef"))
                s.alias = true;
            else if (Eat("unsigned"))
                uns = true;
            else if (Eat("signed"))
                sign = true;
            else if (Eat("long"))
                ++longs;
            else if (Eat("short"))
                shortWord = true;
            else
            {
                auto cc = Convention();
                if (cc != CallConv::Default)
                    s.conv = cc;
                else
                    break;
            }
        }
        if (Eat("struct") || Eat("class") || Eat("union"))
        {
            bool isUnion = ts[p - 1].Is("union");
            s.type = Record(isUnion, s.release, s.value);
        }
        else if (Eat("enum"))
            s.type = Enumeration();
        else if (Eat("auto"))
            s.type = autoType;
        else if (Eat("void"))
            s.type = ty.Void();
        else if (Eat("bool"))
            s.type = ty.Bool();
        else if (Eat("char"))
            s.type = ty.Basic(uns ? TK::UChar : sign ? TK::SChar : TK::Char);
        else if (Eat("wchar_t"))
            s.type = ty.Basic(TK::WChar);
        else if (Eat("char16_t"))
            s.type = ty.Basic(TK::Char16);
        else if (Eat("char32_t"))
            s.type = ty.Basic(TK::Char32);
        else if (Eat("float"))
            s.type = ty.Float();
        else if (Eat("double"))
            s.type = ty.Basic(longs ? TK::LongDouble : TK::Double);
        else if (Eat("__int64"))
            s.type = uns ? ty.ULongLong() : ty.LongLong();
        else if (Eat("int") || uns || sign || longs || shortWord)
            s.type = ty.Basic(shortWord   ? (uns ? TK::UShort : TK::Short)
                              : longs > 1 ? (uns ? TK::ULongLong : TK::LongLong)
                              : longs     ? (uns ? TK::ULong : TK::Long)
                                          : (uns ? TK::UInt : TK::Int));
        else
            s.type = NamedType();
        if (!s.type)
            Error(Peek().loc, "type expected");
        while (Is("const") || Is("volatile"))
        {
            if (Eat("const"))
                s.constant = true;
            else
                Take();
        }
        if (s.constant)
            s.type = ty.Const(s.type);
        auto cc = Convention();
        if (cc != CallConv::Default)
            s.conv = cc;
        auto a = Attributes();
        if (!a.empty())
            s.link = a;
        if (!pendingIntrinsic.empty())
        {
            s.intrinsic = std::move(pendingIntrinsic);
            pendingIntrinsic.clear();
        }
        s.address = s.address || pendingAddress;
        pendingAddress = false;
        return s;
    }
    Type *Replace(Type *t, Type *placeholder, Type *base)
    {
        if (t == placeholder)
            return base;
        if (t->IsPointer())
            return ty.Pointer(Replace(t->base, placeholder, base));
        if (t->IsRef())
            return ty.Ref(Replace(t->base, placeholder, base));
        if (t->IsArray())
            return ty.Array(Replace(t->base, placeholder, base), t->count);
        if (t->IsFunction())
        {
            auto sig = t->sig;
            sig.ret = Replace(sig.ret, placeholder, base);
            return ty.Function(sig);
        }
        return t;
    }
    Declarator Decl(Type *base, CallConv cc, bool abstract = false)
    {
        Loc loc = Peek().loc;
        auto c = Convention();
        if (c != CallConv::Default)
            cc = c;
        while (Is("*") || Is("&") || Is("&&"))
        {
            bool ptr = Eat("*");
            if (!ptr)
                Take();
            base = ptr ? ty.Pointer(base) : ty.Ref(base);
            while (Is("const") || Is("volatile"))
            {
                if (Eat("const"))
                    base = ty.Const(base);
                else
                    Take();
            }
            c = Convention();
            if (c != CallConv::Default)
                cc = c;
        }
        Declarator d{"", base, {}, {}, loc, cc};
        if (Is("operator"))
            Error(Peek().loc, "operator overloading is not supported");
        Type *placeholder = nullptr;
        Declarator inner;
        if (Is("(") && (Peek(1).Is("*") || Peek(1).Is("&") || Peek(1).Is("(") || Peek(1).Is("__cdecl") ||
                        Peek(1).Is("__stdcall") || Peek(1).Is("__fastcall") || !abstract))
        {
            Take();
            placeholder = Node<Type>();
            placeholder->kind = TK::Void;
            inner = Decl(placeholder, cc, abstract);
            Need(")");
            cc = inner.conv;
        }
        else if (Peek().kind == Tok::Ident)
            d.name = Ident();
        else if (!abstract)
            Error(Peek().loc, "declaration name expected");
        std::vector<size_t> dimensions;
        while (Is("[") || Is("("))
        {
            if (Is("(") && !d.name.empty())
            {
                size_t save = p;
                ++p;
                bool parameters = Is(")") || Is("...") || TypeStart();
                p = save;
                if (!parameters)
                    break;
            }
            if (Eat("["))
            {
                size_t count = 0;
                if (!Eat("]"))
                {
                    auto e = Expression(2);
                    uint64_t n;
                    if (!Constant(e, n) || (int64_t)n < 0)
                        Error(e->loc, "array bound must be a nonnegative constant");
                    count = (size_t)n;
                    Need("]");
                }
                dimensions.push_back(count);
            }
            else
            {
                Take();
                FuncSig sig;
                sig.ret = d.type;
                sig.conv = cc;
                if (!(Eat(")") || (Is("void") && Peek(1).Is(")") && (p += 2))))
                {
                    do
                    {
                        if (Eat("..."))
                        {
                            sig.variadic = true;
                            break;
                        }
                        Spec ps = Specs();
                        bool pack = Eat("...");
                        auto pd = Decl(ps.type, ps.conv, true);
                        pack = Eat("...") || pack;
                        auto v = Node<VarDecl>();
                        v->name = pd.name;
                        v->type = pd.type;
                        v->loc = pd.loc;
                        v->parameter = true;
                        if (ps.address)
                            v->type = ty.AnyAddress();
                        if (v->type->IsArray())
                            v->type = ty.Pointer(v->type->base);
                        if (v->type->IsFunction())
                            v->type = ty.Pointer(v->type);
                        if (pack)
                        {
                            auto it = packs.find(ps.type);
                            if (it == packs.end())
                                Error(pd.loc, "unknown parameter pack");
                            for (auto pt : it->second)
                            {
                                auto pv = Node<VarDecl>();
                                pv->type = pt;
                                pv->loc = pd.loc;
                                sig.params.push_back(pt);
                                d.params.push_back(pv);
                                d.defaults.push_back(nullptr);
                            }
                        }
                        else
                        {
                            sig.params.push_back(ty.Unqualified(v->type));
                            d.params.push_back(v);
                            d.defaults.push_back(Eat("=") ? Expression(2) : nullptr);
                        }
                    } while (Eat(","));
                    Need(")");
                }
                d.type = ty.Function(sig);
            }
        }
        for (auto i = dimensions.rbegin(); i != dimensions.rend(); ++i)
            d.type = ty.Array(d.type, *i);
        if (placeholder)
        {
            inner.type = Replace(inner.type, placeholder, d.type);
            if (inner.params.empty())
            {
                inner.params = d.params;
                inner.defaults = d.defaults;
            }
            return inner;
        }
        return d;
    }
    Type *Record(bool isUnion, const std::string &release = {}, bool value = false)
    {
        Loc loc = Peek().loc;
        std::string name = Peek().kind == Tok::Ident ? Ident() : "$anonymous" + std::to_string(p);
        Type *t = nullptr;
        auto it = scope->types.find(name);
        if (it != scope->types.end())
            t = it->second;
        if (t && !t->IsStruct())
            Error(loc, "name already declares a different type");
        if (!t)
        {
            auto st = Node<StructInfo>();
            st->name = Qualified(name);
            st->isUnion = isUnion;
            st->scope = Child(scope, name);
            t = ty.NewStruct(st);
            scope->types[name] = t;
        }
        if (!Eat("{"))
            return t;
        auto st = t->st;
        if (st->complete)
            Error(loc, "type redefinition");
        st->handle = !release.empty();
        st->release = release;
        st->value = value;
        auto saved = scope;
        auto oldClass = classType;
        classType = t;
        scope = st->scope;
        while (!Eat("}"))
        {
            if (Is("template"))
            {
                Template();
                continue;
            }
            if (st->handle || st->value)
            {
                size_t before = p;
                std::string link = Attributes();
                if (Is(name.c_str()) && Peek(1).Is("("))
                {
                    Spec cs;
                    cs.link = link;
                    cs.intrinsic = std::move(pendingIntrinsic);
                    pendingIntrinsic.clear();
                    auto d = Decl(t, CallConv::Cdecl);
                    auto ctor = DeclareFunction(d, cs);
                    st->constructors.push_back(ctor);
                    Need(";");
                    continue;
                }
                bool explicitWord = Eat("explicit");
                if (Eat("operator"))
                {
                    Need("bool");
                    Need("(");
                    Need(")");
                    Eat("const");
                    Spec cs;
                    cs.link = link;
                    Declarator d;
                    d.name = "$bool";
                    d.type = ty.Function({ty.Bool(), {t}, false, CallConv::Cdecl});
                    d.loc = Peek().loc;
                    auto self = Node<VarDecl>();
                    self->type = t;
                    d.params = {self};
                    d.defaults = {nullptr};
                    st->boolOp = DeclareFunction(d, cs);
                    Need(";");
                    continue;
                }
                if (explicitWord)
                    Error(Peek().loc, "unsupported explicit declaration");
                p = before;
                pendingIntrinsic.clear();
                pendingRelease.clear();
                pendingValue = pendingAddress = false;
            }
            if ((Is("public") || Is("private") || Is("protected")) && Peek(1).Is(":"))
            {
                p += 2;
                continue;
            }
            if (Is("static_assert"))
            {
                StaticAssert();
                continue;
            }
            if (Is("virtual"))
                Error(Peek().loc, "virtual functions are not supported");
            if (Is("~") || (Is(name.c_str()) && Peek(1).Is("(")))
                Error(Peek().loc, "user constructors and destructors are not supported");
            Loc specLoc = Peek().loc;
            auto s = Specs();
            if (Eat(";"))
            {
                // MSVC injects the members of an anonymous union/struct into the enclosing class; silently
                // dropping it would give every following member a wrong offset
                if (s.type && s.type->IsStruct() && s.type->st && s.type->st->name.find("$anonymous") != std::string::npos)
                    Error(specLoc, "anonymous unions and structs inside a class are not supported, give the member a name");
                continue;
            }
            do
            {
                auto d = Decl(s.type, s.conv);
                if (d.type->IsFunction())
                {
                    auto fn = DeclareFunction(d, s);
                    st->methods.push_back(fn);
                    auto self = Node<VarDecl>();
                    self->name = "this";
                    self->type = st->handle || st->value ? t : ty.Pointer(t);
                    self->parameter = true;
                    self->loc = d.loc;
                    fn->params.insert(fn->params.begin(), self);
                    auto sig = fn->type->sig;
                    sig.params.insert(sig.params.begin(), self->type);
                    sig.conv = EffectiveConv(s.conv, ty.target.x64, !(st->handle || st->value));
                    fn->type = ty.Function(sig);
                    fn->defaults.insert(fn->defaults.begin(), nullptr);
                    Eat("const");
                    if (Is("{"))
                    {
                        size_t first = p;
                        int depth = 0;
                        do
                        {
                            auto tok = Take();
                            if (tok.Is("{"))
                                ++depth;
                            if (tok.Is("}"))
                                --depth;
                        } while (depth);
                        deferred.push_back({fn, scope, first, p});
                    }
                    else
                        Need(";");
                    goto nextMember;
                }
                if (d.type == autoType || d.type->IsRef())
                    Error(d.loc, "unsupported field type");
                if (s.internal)
                    Error(d.loc, "static data members are not supported");
                size_t align = ty.AlignOf(d.type);
                if (!align || !ty.SizeOf(d.type))
                    Error(d.loc, "field has incomplete type");
                st->align = std::max(st->align, align);
                size_t offset = isUnion ? 0 : (st->size + align - 1) / align * align;
                if (std::any_of(st->fields.begin(), st->fields.end(),
                                [&](const FieldInfo &f) { return f.name == d.name; }))
                    Error(d.loc, "duplicate field");
                Expr *init = Eat("=") ? Initializer(d.type) : Is("{") ? Initializer(d.type) : nullptr;
                st->fields.push_back({d.name, d.type, offset, init, d.loc});
                st->size = std::max(st->size, offset + ty.SizeOf(d.type));
            } while (Eat(","));
            Need(";");
        nextMember:;
        }
        st->size = st->handle || st->value ? ty.target.ptrSize()
                                           : std::max<size_t>(1, (st->size + st->align - 1) / st->align * st->align);
        if (st->handle || st->value)
            st->align = ty.target.ptrSize();
        st->complete = true;
        scope = saved;
        classType = oldClass;
        return t;
    }
    Type *Enumeration()
    {
        bool scoped = Eat("class") || Eat("struct");
        Loc loc = Peek().loc;
        std::string name = Peek().kind == Tok::Ident ? Ident() : "$enum" + std::to_string(p);
        Type *underlying = Eat(":") ? Specs().type : ty.Int();
        if (!underlying->IsInteger())
            Error(loc, "enum underlying type must be integral");
        auto en = Node<EnumInfo>();
        en->name = Qualified(name);
        en->underlying = underlying;
        en->scoped = scoped;
        auto t = ty.NewEnum(en);
        scope->types[name] = t;
        if (!Eat("{"))
            return t;
        auto es = Child(scope, name);
        uint64_t value = 0;
        while (!Eat("}"))
        {
            auto v = Node<VarDecl>();
            v->loc = Peek().loc;
            v->name = Ident();
            v->type = t; // an unscoped enumerator converts to its underlying type where needed, like in C++
            v->constant = true;
            v->hasConstant = true;
            v->enumerator = true;
            if (Eat("="))
            {
                auto e = Expression(2);
                if (!Constant(e, value))
                    Error(e->loc, "enumerator must be constant");
            }
            v->constantValue = value++;
            es->variables[v->name] = v;
            if (!scoped)
                scope->variables[v->name] = v;
            if (!Eat(","))
            {
                Need("}");
                break;
            }
        }
        return t;
    }
    FuncDecl *DeclareFunction(Declarator d, Spec s)
    {
        auto &overloads = scope->functions[d.name];
        for (auto fn : overloads)
            if (fn->type->sig.params == d.type->sig.params && fn->type->sig.conv == d.type->sig.conv)
            {
                if (fn->type->sig.ret != d.type->sig.ret)
                    Error(d.loc, "conflicting return type");
                if (Is("{"))
                {
                    if (fn->body)
                        Error(d.loc, "function redefinition");
                    fn->params = d.params;
                }
                return fn;
            }
        auto fn = Node<FuncDecl>();
        fn->name = d.name;
        fn->qualified = Qualified(d.name);
        fn->type = d.type;
        fn->params = d.params;
        fn->defaults = d.defaults;
        fn->loc = d.loc;
        fn->internal = s.internal;
        fn->link = s.link;
        fn->intrinsic = s.intrinsic;
        fn->constexprFunction = s.constexprFunction;
        overloads.push_back(fn);
        u.functions.push_back(fn);
        return fn;
    }
    void Body(FuncDecl *fn)
    {
        if (fn->type->sig.ret->IsStruct() && !(fn->type->sig.ret->st->handle || fn->type->sig.ret->st->value))
            Error(fn->loc, "return user aggregates by reference or pointer");
        auto previous = function;
        auto savedScope = scope;
        scope = Node<Namespace>();
        scope->parent = savedScope;
        scope->name = savedScope->name;
        function = fn;
        localBoundaries.push_back(locals.size());
        locals.emplace_back();
        for (auto v : fn->params)
        {
            if (!v->name.empty() && locals.back().count(v->name))
                Error(v->loc, "duplicate parameter");
            locals.back()[v->name] = v;
        }
        fn->body = ParseStatement();
        locals.pop_back();
        localBoundaries.pop_back();
        function = previous;
        scope = savedScope;
        if (fn->type->sig.ret == autoType)
        {
            auto sig = fn->type->sig;
            sig.ret = ty.Void();
            fn->type = ty.Function(sig);
        }
    }
    Expr *Initializer(Type *t)
    {
        DepthGuard guard(*this, Peek().loc);
        if (Eat("{"))
        {
            auto e = New(EK::InitList, t, Peek().loc);
            if (!Eat("}"))
            {
                do
                {
                    e->args.push_back(Is("{") ? Initializer(t) : Expression(2));
                } while (Eat(",") && !Is("}"));
                Need("}");
            }
            if (t->IsScalar() && e->args.size() > 1)
                Error(e->loc, "too many scalar initializers");
            if (t->IsScalar())
                return e->args.empty() ? Number(0, t, e->loc) : Cast(e->args[0], t);
            if (t->IsStruct() && t->st->value && !e->args.empty())
                return Construct(t, e->args, e->loc);
            return e;
        }
        auto e = Expression(2);
        if (t->IsArray() && e->kind == EK::String)
            return e;
        return HasAuto(t) ? e : Cast(e, t);
    }
    bool HasAuto(Type *type)
    {
        if (ty.Unqualified(type) == autoType)
            return true;
        return (type->IsPointer() || type->IsRef() || type->IsArray()) && HasAuto(type->base);
    }
    Type *DeduceAuto(Type *pattern, Type *actual)
    {
        if (ty.Unqualified(pattern) == autoType)
        {
            auto t = Decay(actual);
            return pattern->isConst ? ty.Const(t) : t;
        }
        if (pattern->IsRef())
            return ty.Ref(DeduceAuto(pattern->base, actual->IsRef() ? actual->base : actual));
        if (pattern->IsPointer())
        {
            actual = Decay(actual);
            if (!actual->IsPointer())
                Error(Peek().loc, "auto pointer requires a pointer initializer");
            return ty.Pointer(DeduceAuto(pattern->base, actual->base));
        }
        return pattern;
    }
    bool linkageDeclaration = false; // parsing the declaration of extern "C" / extern "C++" without braces
    std::vector<VarDecl *> Declaration(bool global)
    {
        Spec s = Specs();
        // extern "C" int x; (one declaration, no braces) declares, as extern does
        if (linkageDeclaration)
        {
            s.external = true;
            linkageDeclaration = false;
        }
        std::vector<VarDecl *> result;
        if (Eat(";"))
            return result;
        do
        {
            auto d = Decl(s.type, s.conv);
            if (s.alias)
            {
                scope->types[d.name] = d.type;
                continue;
            }
            if (d.type->IsFunction())
            {
                auto fn = DeclareFunction(d, s);
                if (Is("{"))
                {
                    if (!global)
                        Error(d.loc, "nested function definitions are not supported");
                    if (fn->type->sig.variadic)
                        Error(d.loc, "variadic function definitions are not supported");
                    Body(fn);
                    return result;
                }
                continue;
            }
            auto v = Node<VarDecl>();
            v->name = d.name;
            v->qualified = Qualified(d.name);
            v->type = d.type;
            v->loc = d.loc;
            v->constant = d.type->isConst;
            v->internal = s.internal;
            v->external = s.external;
            v->global = global;
            if (!global && s.internal)
            {
                v->global = true;
                v->staticLocal = true;
                v->qualified = function->qualified + "::$static$" + v->name + std::to_string(u.globals.size());
            }
            if (!global && v->type->IsStruct() && v->type->st->handle)
                Error(d.loc, "hook handles are allowed only as globals or temporaries");
            auto &table = global ? scope->variables : locals.back();
            if (table.count(v->name))
                Error(d.loc, "variable redefinition '" + v->name + "'");
            table[v->name] = v;
            if (Eat("="))
                v->init = Initializer(v->type);
            else if (Is("{"))
                v->init = Initializer(v->type);
            else if (Eat("("))
            {
                std::vector<Expr *> args;
                if (!Eat(")"))
                {
                    do
                    {
                        args.push_back(Expression(2));
                    } while (Eat(","));
                    Need(")");
                }
                v->init = Construct(v->type, args, d.loc);
            }
            if (HasAuto(v->type))
            {
                if (!v->init)
                    Error(d.loc, "auto requires an initializer");
                v->type = DeduceAuto(v->type, v->init->type);
                v->init = Cast(v->init, v->type);
            }
            if (v->type->IsRef() && !v->init)
                Error(d.loc, "reference requires an initializer");
            if (v->type->IsArray() && !v->type->count && v->init)
            {
                size_t n = v->init->kind == EK::String ? v->init->bytes.size() / ty.SizeOf(v->type->base)
                                                       : v->init->args.size();
                v->type = ty.Array(v->type->base, n);
            }
            if (!s.external && !ty.SizeOf(v->type))
                Error(d.loc, "variable has void or incomplete type");
            if (v->constant && v->init)
                v->hasConstant = Constant(v->init, v->constantValue);
            if (v->init && v->type->IsStruct() && v->type->st->handle && v->init->lvalue)
                Error(v->loc, "hook handles cannot be copied");
            if (v->global)
                u.globals.push_back(v);
            else
                function->locals.push_back(v);
            result.push_back(v);
        } while (Eat(","));
        Need(";");
        return result;
    }
    // static_assert(constant, "message"); / static_assert(constant);
    void StaticAssert()
    {
        auto loc = Peek().loc;
        Need("static_assert");
        Need("(");
        auto e = Expression(2);
        std::string message;
        if (Eat(","))
            while (Peek().kind == Tok::String)
                message += Take().str;
        Need(")");
        Need(";");
        uint64_t value = 0;
        if (!Constant(e, value))
            Error(loc, "static_assert requires a constant expression");
        if (!value)
            Error(loc, message.empty() ? "static assertion failed" : "static assertion failed: " + message);
    }
    void Top()
    {
        if (Eat(";"))
            return;
        if (Is("static_assert"))
            return StaticAssert();
        if (Eat("namespace"))
        {
            auto saved = scope;
            scope = Child(scope, Ident());
            while (Eat("::"))
                scope = Child(scope, Ident());
            Need("{");
            while (!Eat("}"))
                Top();
            scope = saved;
            Eat(";");
            return;
        }
        if (Eat("using"))
        {
            if (Eat("namespace"))
            {
                auto n = ReadName();
                auto ns = Lookup<Namespace *>(n, &Namespace::children);
                if (!ns)
                    Error(Peek().loc, "unknown namespace");
                scope->imports.push_back(ns);
            }
            else
            {
                auto name = Ident();
                Need("=");
                auto s = Specs();
                auto d = Decl(s.type, s.conv, true);
                scope->types[name] = d.type;
            }
            Need(";");
            return;
        }
        if (Is("extern") && Peek(1).kind == Tok::String)
        {
            Take();
            auto lang = Take();
            if (lang.str != "C" && lang.str != "C++")
                Error(lang.loc, "unsupported language linkage");
            if (Eat("{"))
            {
                while (!Eat("}"))
                    Top();
            }
            else
            {
                linkageDeclaration = true;
                Declaration(true);
                linkageDeclaration = false;
            }
            return;
        }
        if (Is("template"))
        {
            Template();
            return;
        }
        Declaration(true);
    }
    void Template()
    {
        Loc loc = Peek().loc;
        if (!Peek().builtin)
            Error(loc, "user templates are not supported");
        Need("template");
        Need("<");
        auto templ = Node<TemplateDecl>();
        templ->scope = scope;
        templ->owner = classType;
        do
        {
            if (!(Eat("typename") || Eat("class")))
                Error(Peek().loc, "only type template parameters are supported");
            bool pack = Eat("...");
            std::string name = Ident();
            Type *fallback = nullptr;
            if (Eat("="))
            {
                auto s = Specs();
                fallback = Decl(s.type, s.conv, true).type;
            }
            auto marker = Node<Type>();
            marker->kind = TK::AnyAddress;
            templ->parameters.push_back({name, fallback, pack, marker});
        } while (Eat(","));
        Need(">");
        size_t first = p;
        while (!Eat(";"))
        {
            if (Is("{"))
                Error(Peek().loc, "template definitions are not supported");
            Take();
        }
        templ->declaration.assign(ts.begin() + first, ts.begin() + p);
        templ->declaration.push_back(Token{});
        Parser probe(u, templ->declaration);
        probe.scope = scope;
        for (auto &param : templ->parameters)
        {
            probe.substitutions[param.name] = param.marker;
            if (param.pack)
                probe.packs[param.marker] = {param.marker};
        }
        auto spec = probe.Specs();
        auto d = probe.Decl(spec.type, spec.conv);
        scope->templates[d.name] = templ;
    }
    FuncDecl *Instantiate(Expr *e, const std::vector<Expr *> &input)
    {
        auto templ = e->templ;
        std::map<Type *, Type *> bindings;
        std::map<Type *, std::vector<Type *>> packBindings;
        size_t explicitIndex = 0;
        for (auto &p : templ->parameters)
        {
            if (p.pack)
            {
                while (explicitIndex < e->templateArgs.size())
                    packBindings[p.marker].push_back(e->templateArgs[explicitIndex++]);
            }
            else if (explicitIndex < e->templateArgs.size())
                bindings[p.marker] = e->templateArgs[explicitIndex++];
        }
        if (explicitIndex != e->templateArgs.size())
            Error(e->loc, "too many template arguments");
        Parser probe(u, templ->declaration);
        probe.scope = templ->scope;
        for (auto &p : templ->parameters)
        {
            probe.substitutions[p.name] = p.marker;
            if (p.pack)
                probe.packs[p.marker] = {p.marker};
        }
        auto ps = probe.Specs();
        auto generic = probe.Decl(ps.type, ps.conv);
        size_t start = templ->owner ? 1 : 0;
        std::function<void(Type *, Type *)> deduce = [&](Type *pattern, Type *actual) {
            for (auto &param : templ->parameters)
                if (pattern == param.marker && !param.pack)
                {
                    auto it = bindings.find(pattern);
                    if (it == bindings.end())
                        bindings[pattern] = actual;
                    else if (it->second != actual && e->templateArgs.empty())
                        Error(e->loc, "conflicting template deduction");
                    return;
                }
            if (pattern->IsRef())
            {
                deduce(pattern->base, actual->IsRef() ? actual->base : actual);
                return;
            }
            actual = Decay(actual);
            if (pattern->IsPointer() && actual->IsPointer())
                deduce(pattern->base, actual->base);
        };
        for (size_t i = 0; i < generic.type->sig.params.size() && i + start < input.size(); ++i)
        {
            Type *pt = generic.type->sig.params[i];
            bool isPack = false;
            for (auto &param : templ->parameters)
                if (param.pack && pt == param.marker)
                {
                    if (!packBindings.count(pt))
                        for (size_t j = i + start; j < input.size(); ++j)
                            packBindings[pt].push_back(Decay(input[j]->type));
                    isPack = true;
                    break;
                }
            if (isPack)
                break;
            deduce(pt, input[i + start]->type);
        }
        Parser specialized(u, templ->declaration);
        specialized.scope = templ->scope;
        for (auto &param : templ->parameters)
        {
            if (param.pack)
            {
                specialized.substitutions[param.name] = param.marker;
                specialized.packs[param.marker] = packBindings[param.marker];
            }
            else
            {
                auto t = bindings.count(param.marker) ? bindings[param.marker] : param.fallback;
                if (!t)
                    Error(e->loc, "cannot deduce template argument '" + param.name + "'");
                specialized.substitutions[param.name] = t;
            }
        }
        auto s = specialized.Specs();
        auto d = specialized.Decl(s.type, s.conv);
        if (templ->owner)
        {
            auto self = Node<VarDecl>();
            self->type = templ->owner;
            self->parameter = true;
            d.params.insert(d.params.begin(), self);
            d.defaults.insert(d.defaults.begin(), nullptr);
            auto sig = d.type->sig;
            sig.params.insert(sig.params.begin(), templ->owner);
            d.type = ty.Function(sig);
        }
        d.name += "$" + TypeTable::Name(d.type);
        return specialized.DeclareFunction(d, s);
    }
    Type *Decay(Type *t)
    {
        if (t->IsRef())
            t = t->base;
        if (t->IsArray() || t->IsFunction())
            t = ty.Pointer(t->IsArray() ? t->base : t);
        if (t->kind == TK::Closure)
            t = ty.Pointer(t->closure->type);
        return ty.Unqualified(t);
    }
    Type *Promote(Type *t)
    {
        t = ty.Unqualified(t);
        if (t->IsEnum())
            t = t->base;
        if (t->IsInteger() && ty.SizeOf(t) < 4)
            return ty.Int();
        return t;
    }
    // c ? a : b with pointers: nullptr or 0 takes the other side's pointer type;
    // an object pointer and void* meet at void* (cv-qualifiers kept).
    Type *ConditionalType(Expr *ea, Expr *eb)
    {
        Type *a = Decay(ea->type), *b = Decay(eb->type);
        uint64_t zero = 1;
        auto isNull = [&](Expr *e, Type *t) {
            return t->kind == TK::Nullptr || (t->IsInteger() && Constant(e, zero) && zero == 0);
        };
        if (a->IsPointer() || b->IsPointer() || a->kind == TK::Nullptr || b->kind == TK::Nullptr)
        {
            if (a->IsPointer() && isNull(eb, b))
                return a;
            if (b->IsPointer() && isNull(ea, a))
                return b;
            if (a->kind == TK::Nullptr && b->kind == TK::Nullptr)
                return a;
            if (a->IsPointer() && b->IsPointer())
            {
                if (ty.Unqualified(a->base) == ty.Unqualified(b->base))
                    return a->base->isConst ? a : b;
                if ((a->base->IsVoid() && !b->IsFuncPtr()) || (b->base->IsVoid() && !a->IsFuncPtr()))
                    return ty.Pointer(a->base->isConst || b->base->isConst ? ty.Const(ty.Void()) : ty.Void());
            }
            Error(Peek().loc, "incompatible pointer operands in ?:");
        }
        return Common(ea->type, eb->type);
    }
    Type *Common(Type *a, Type *b)
    {
        if ((a->IsEnum() && a->en->scoped) || (b->IsEnum() && b->en->scoped))
            Error(Peek().loc, "scoped enums require an explicit arithmetic cast");
        a = Promote(Decay(a));
        b = Promote(Decay(b));
        if (a->IsFloating() || b->IsFloating())
        {
            if (a->kind == TK::LongDouble || b->kind == TK::LongDouble)
                return ty.Basic(TK::LongDouble);
            return a->kind == TK::Double || b->kind == TK::Double ? ty.Double() : ty.Float();
        }
        if (!a->IsInteger() || !b->IsInteger())
            Error(Peek().loc, "arithmetic operands required");
        auto rank = [](Type *t) {
            return t->kind == TK::LongLong || t->kind == TK::ULongLong ? 5
                   : t->kind == TK::Long || t->kind == TK::ULong       ? 4
                                                                       : 3;
        };
        if (ty.IsSigned(a) == ty.IsSigned(b))
            return rank(a) >= rank(b) ? a : b;
        Type *sign = ty.IsSigned(a) ? a : b;
        Type *uns = ty.IsSigned(a) ? b : a;
        if (rank(uns) >= rank(sign))
            return uns;
        if (ty.SizeOf(sign) > ty.SizeOf(uns))
            return sign;
        return ty.Basic(sign->kind == TK::LongLong ? TK::ULongLong : sign->kind == TK::Long ? TK::ULong : TK::UInt);
    }
    int Rank(Expr *e, Type *t)
    {
        Expr *overload = e->kind == EK::Unary && e->op == "&" ? e->a : e;
        if (overload->kind == EK::Function && !overload->overloads.empty() && t->IsFuncPtr())
        {
            for (auto f : overload->overloads)
                if (f->type == t->base)
                    return 0;
            return 100;
        }
        if (e->type->kind == TK::Closure && ty.Unqualified(t) == e->type) // deduced template parameter (Pred)
            return 0;
        if (e->type->kind == TK::Closure && t->IsFuncPtr())
        {
            auto &a = e->type->closure->type->sig;
            auto &b = t->base->sig;
            if (a.ret == b.ret && a.params == b.params && !b.variadic)
                return 0;
        }
        if (t->IsRef())
        {
            auto from = e->type->IsRef() ? e->type->base : e->type;
            if (ty.Unqualified(from) == ty.Unqualified(t->base) && (!from->isConst || t->base->isConst) &&
                (e->lvalue || t->base->isConst))
                return from->isConst == t->base->isConst ? 0 : 2;
            return 100;
        }
        if (t->kind == TK::AnyAddress)
        {
            auto from = Decay(e->type);
            return from->IsInteger() || from->IsPointer() || from->kind == TK::Nullptr ? 2 : 100;
        }
        t = ty.Unqualified(t);
        auto f = Decay(e->type);
        if (f == t)
            return 0;
        if (f->IsArithmetic() && t->IsArithmetic())
            return Promote(f) == t || (f == ty.Float() && t == ty.Double()) ? 1 : 2;
        if (t->IsPointer() && (f->kind == TK::Nullptr || (e->kind == EK::Number && e->value == 0)))
            return 2;
        if (f->IsPointer() && t->IsPointer() && (!f->base->isConst || t->base->isConst) &&
            (t->base->IsVoid() || ty.Unqualified(f->base) == ty.Unqualified(t->base)))
            return 2;
        if (f->IsEnum() && !f->en->scoped && t->IsArithmetic())
            return 2;
        if (f->IsStruct() && (f->st->handle || f->st->value) && t->IsBool())
            return 3;
        if (t->IsBool() && f->IsScalar())
            return 2;
        return 100;
    }
    Expr *Cast(Expr *e, Type *t, bool explicitCast = false)
    {
        if (t == autoType || e->type == t)
            return e;
        Expr *overload = e->kind == EK::Unary && e->op == "&" ? e->a : e;
        if (overload->kind == EK::Function && !overload->overloads.empty() && t->IsFuncPtr())
        {
            FuncDecl *selected = nullptr;
            for (auto f : overload->overloads)
                if (f->type == t->base)
                {
                    if (selected)
                        Error(e->loc, "ambiguous overloaded function address");
                    selected = f;
                }
            if (!selected)
                Error(e->loc, "no overload matches function pointer type");
            auto r = New(EK::Function, selected->type, e->loc);
            r->fn = selected;
            return r;
        }
        if (e->type->kind == TK::Closure && t->IsFuncPtr() && Rank(e, t) == 0)
        {
            auto f = e->type->closure;
            if (f->type != t->base)
            {
                auto copy = Node<FuncDecl>();
                *copy = *f;
                copy->type = t->base;
                copy->qualified += "$" + TypeTable::Name(t);
                u.functions.push_back(copy);
                f = copy;
            }
            auto r = New(EK::Function, t->base, e->loc);
            r->fn = f;
            return r;
        }
        if (e->type->IsStruct() && (e->type->st->handle || e->type->st->value) && t->IsBool() && e->type->st->boolOp)
        {
            auto fn = e->type->st->boolOp;
            auto callee = New(EK::Function, fn->type, e->loc);
            callee->overloads = {fn};
            return ResolveCall(callee, {e}, e->loc);
        }
        if (!explicitCast && Rank(e, t) == 100)
            Error(e->loc, "cannot convert '" + TypeTable::Name(e->type) + "' to '" + TypeTable::Name(t) + "'");
        if (explicitCast && !t->IsVoid() && !(Decay(e->type)->IsScalar() && Decay(t)->IsScalar()))
            Error(e->loc, "unsupported cast");
        auto r = New(EK::Cast, t, e->loc);
        r->a = e;
        r->lvalue = t->IsRef();
        return r;
    }
    Expr *Unary(std::string op, Expr *a, Loc loc)
    {
        if (op == "+" && a->type->kind == TK::Closure)
            return Cast(a, ty.Pointer(a->type->closure->type));
        Type *t = Decay(a->type);
        if (op == "&")
        {
            if (!a->lvalue && a->kind != EK::Function)
                Error(loc, "address requires an lvalue or function");
            t = ty.Pointer(a->type->IsRef() ? a->type->base : a->type);
        }
        else if (op == "*")
        {
            if (!t->IsPointer())
                Error(loc, "dereference requires a pointer");
            t = t->base;
        }
        else if (op == "!")
        {
            a = Cast(a, ty.Bool());
            t = ty.Bool();
        }
        else if (op == "++" || op == "--" || op == "post++" || op == "post--")
        {
            Writable(a);
            if (!t->IsScalar() || t->IsBool())
                Error(loc, "increment requires arithmetic or pointer type");
        }
        else
        {
            if (!t->IsArithmetic())
                Error(loc, "arithmetic operand required");
            t = Promote(t);
            a = Cast(a, t);
        }
        auto e = New(EK::Unary, t, loc);
        e->op = op;
        e->a = a;
        e->lvalue = op == "*" || op == "++" || op == "--";
        return e;
    }
    void Writable(Expr *a)
    {
        if (!a->lvalue || a->type->isConst || (a->var && a->var->constant) || a->type->IsArray())
            Error(a->loc, "assignment requires a modifiable lvalue");
    }
    Expr *Binary(std::string op, Expr *a, Expr *b, Loc loc)
    {
        auto x = Decay(a->type), y = Decay(b->type);
        Type *result;
        if (op == ",")
            result = b->type;
        else if (op == "=" ||
                 (op.size() > 1 && op.back() == '=' && op != "==" && op != "!=" && op != "<=" && op != ">="))
        {
            Writable(a);
            result = x;
            if (result->IsStruct() && result->st->handle && b->lvalue)
                Error(loc, "hook handles cannot be copied");
            if (op != "=")
            {
                auto rhs = Binary(op.substr(0, op.size() - 1), a, b, loc);
                auto e = New(EK::Binary, result, loc);
                e->op = op;
                e->a = a;
                e->b = rhs->b;
                e->c = rhs;
                e->lvalue = true;
                return e;
            }
            if (b->kind != EK::InitList)
                b = Cast(b, x);
            op = "=";
        }
        else if (op == "&&" || op == "||")
        {
            a = Cast(a, ty.Bool());
            b = Cast(b, ty.Bool());
            result = ty.Bool();
        }
        else if ((op == "+" || op == "-") && (x->IsPointer() || y->IsPointer()))
        {
            if (y->IsPointer() && !x->IsPointer() && op == "+")
            {
                std::swap(a, b);
                std::swap(x, y);
            }
            if (!x->IsPointer())
                Error(loc, "invalid pointer arithmetic");
            size_t scale = ty.SizeOf(x->base);
            if (!scale)
                Error(loc, "pointer arithmetic on an incomplete type");
            if (y->IsPointer())
            {
                if (op != "-" || x->base != y->base)
                    Error(loc, "incompatible pointer arithmetic");
                result = ty.PtrDiffT();
            }
            else
            {
                if (!y->IsInteger())
                    Error(loc, "pointer offset must be integral");
                b = Cast(b, ty.PtrDiffT());
                result = x;
            }
            auto e = New(EK::Binary, result, loc);
            e->op = y->IsPointer() ? "ptrdiff" : op;
            e->a = a;
            e->b = b;
            e->offset = scale;
            return e;
        }
        else
        {
            bool comparison = op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=";
            Type *common;
            if (comparison && x == y && x->IsEnum())
                common = x;
            else if (comparison && (x->IsPointer() || y->IsPointer()))
            {
                common = x->IsPointer() ? x : y;
                a = Cast(a, common);
                b = Cast(b, common);
            }
            else
            {
                common = (op == "<<" || op == ">>") ? Promote(x) : Common(x, y);
                a = Cast(a, common);
                b = Cast(b, common);
            }
            if ((op == "%" || op == "&" || op == "|" || op == "^" || op == "<<" || op == ">>") && !common->IsInteger())
                Error(loc, "operator requires integral operands");
            result = comparison ? ty.Bool() : common;
        }
        auto e = New(EK::Binary, result, loc);
        e->op = op;
        e->a = a;
        e->b = b;
        e->lvalue = op == "=" || (op == "," && b->lvalue);
        return e;
    }
    Expr *ResolveCall(Expr *callee, std::vector<Expr *> args, Loc loc)
    {
        if (callee->templ)
        {
            auto fn = Instantiate(callee, args);
            callee->overloads = {fn};
            callee->type = fn->type;
        }
        if (!callee->overloads.empty())
        {
            FuncDecl *best = nullptr;
            struct Candidate
            {
                FuncDecl *fn;
                std::vector<int> ranks;
            };
            std::vector<Candidate> candidates;
            for (auto fn : callee->overloads)
            {
                auto &sig = fn->type->sig;
                if (args.size() > sig.params.size() && !sig.variadic)
                    continue;
                bool viable = true;
                std::vector<int> ranks;
                for (size_t i = 0; i < std::max(args.size(), sig.params.size()); ++i)
                {
                    if (i >= args.size())
                    {
                        if (i >= fn->defaults.size() || !fn->defaults[i])
                            viable = false;
                    }
                    else
                    {
                        int rank = i < sig.params.size() ? Rank(args[i], sig.params[i]) : 4;
                        ranks.push_back(rank);
                        if (rank == 100)
                            viable = false;
                    }
                }
                if (!viable)
                    continue;
                candidates.push_back({fn, std::move(ranks)});
            }
            for (auto &candidate : candidates)
            {
                bool dominated = false;
                for (auto &other : candidates)
                {
                    bool better = false, worse = false;
                    for (size_t i = 0; i < candidate.ranks.size(); ++i)
                    {
                        better |= other.ranks[i] < candidate.ranks[i];
                        worse |= other.ranks[i] > candidate.ranks[i];
                    }
                    if (better && !worse)
                    {
                        dominated = true;
                        break;
                    }
                }
                if (!dominated)
                {
                    if (best)
                        Error(loc, "ambiguous overload");
                    best = candidate.fn;
                }
            }
            if (!best)
            {
                std::string types;
                for (auto a : args)
                    types += (types.empty() ? "" : ", ") + TypeTable::Name(a->type);
                auto name = callee->overloads.front()->qualified;
                Error(loc, "no viable overload of '" + name.substr(0, name.find('$')) + "' for (" + types + ")");
            }
            callee->fn = best;
            callee->type = best->type;
            while (args.size() < best->type->sig.params.size())
                args.push_back(best->defaults[args.size()]);
        }
        auto t = Decay(callee->type);
        if (!t->IsFuncPtr())
            Error(loc, "called expression is not a function");
        auto &sig = t->base->sig;
        if (args.size() < sig.params.size() || (args.size() > sig.params.size() && !sig.variadic))
            Error(loc, "wrong number of arguments");
        for (size_t i = 0; i < args.size(); ++i)
        {
            Type *pt = i < sig.params.size()                           ? sig.params[i]
                       : ty.Unqualified(args[i]->type) == ty.Float() ? ty.Double() // const float too
                                                                       : Promote(Decay(args[i]->type));
            if (pt->IsStruct() && !(pt->st->handle || pt->st->value))
                Error(loc, "aggregate arguments are not supported");
            args[i] = Cast(args[i], pt);
        }
        auto e = New(EK::Call, sig.ret, loc);
        e->a = callee;
        e->args = std::move(args);
        e->lvalue = sig.ret->IsRef();
        return e;
    }
    Expr *Construct(Type *type, std::vector<Expr *> args, Loc loc)
    {
        if (type->IsStruct() && (type->st->handle || type->st->value) && !type->st->constructors.empty())
        {
            auto callee = New(EK::Function, type->st->constructors.front()->type, loc);
            callee->overloads = type->st->constructors;
            return ResolveCall(callee, std::move(args), loc);
        }
        if (type->IsScalar() && args.size() <= 1)
            return args.empty() ? Number(0, type, loc) : Cast(args[0], type, true);
        Error(loc, "unsupported construction");
    }
    Expr *Primary()
    {
        auto t = Take();
        Expr *e = nullptr;
        if (t.kind == Tok::Int || t.kind == Tok::Char)
        {
            Type *type = t.kind == Tok::Char ? ty.Basic(t.charWidth == 2 ? TK::WChar : TK::Char)
                                             : ty.Basic(t.longs > 1 ? (t.isUnsigned ? TK::ULongLong : TK::LongLong)
                                                        : t.longs   ? (t.isUnsigned ? TK::ULong : TK::Long)
                                                                    : (t.isUnsigned ? TK::UInt : TK::Int));
            if (t.kind == Tok::Int && ty.SizeOf(type) == 4 && t.ival > (t.isUnsigned ? UINT32_MAX : INT32_MAX))
                type = t.ival <= UINT32_MAX && t.text.size() > 1 && t.text[0] == '0' ? ty.UInt()
                       : t.isUnsigned                                                ? ty.ULongLong()
                                                                                     : ty.LongLong();
            e = Number(t.ival, type, t.loc);
        }
        else if (t.kind == Tok::Float)
        {
            uint64_t bits = 0;
            if (t.floatSuffix)
            {
                float v = (float)t.fval;
                std::memcpy(&bits, &v, 4);
            }
            else
                std::memcpy(&bits, &t.fval, 8);
            e = Number(bits, t.floatSuffix ? ty.Float() : ty.Double(), t.loc);
        }
        else if (t.kind == Tok::String)
        {
            std::string bytes = t.str;
            while (Peek().kind == Tok::String)
            {
                auto next = Take();
                if (next.charWidth != t.charWidth)
                    Error(next.loc, "mixed-width string concatenation is not supported");
                bytes += next.str;
            }
            bytes.append(t.charWidth, '\0');
            e = New(EK::String,
                    ty.Array(ty.Const(ty.Basic(t.charWidth == 2 ? TK::WChar : TK::Char)), bytes.size() / t.charWidth),
                    t.loc);
            e->bytes = std::move(bytes);
            e->lvalue = true;
        }
        else if (t.Is("true") || t.Is("false"))
            e = Number(t.Is("true"), ty.Bool(), t.loc);
        else if (t.Is("nullptr"))
            e = Number(0, ty.Basic(TK::Nullptr), t.loc);
        else if (t.Is("sizeof"))
        {
            Type *type;
            size_t save = p;
            if (Eat("(") && TypeStart())
            {
                auto s = Specs();
                auto d = Decl(s.type, s.conv, true);
                Need(")");
                type = d.type;
            }
            else
            {
                p = save;
                type = Prefix()->type;
                if (type->IsRef())
                    type = type->base;
            }
            if (!ty.SizeOf(type))
                Error(t.loc, "sizeof requires a complete object type");
            e = Number(ty.SizeOf(type), ty.SizeT(), t.loc);
        }
        else if (t.Is("offsetof"))
        {
            // offsetof(Type, member.member[index]...)
            Need("(");
            auto s = Specs();
            Type *type = Decl(s.type, s.conv, true).type;
            Need(",");
            size_t offset = 0;
            for (bool first = true;; first = false)
            {
                if (!first && Eat("["))
                {
                    if (!type->IsArray())
                        Error(t.loc, "offsetof: subscript of a non-array member");
                    uint64_t index = 0;
                    if (!Constant(Expression(), index))
                        Error(t.loc, "offsetof: constant index required");
                    Need("]");
                    type = type->base;
                    offset += (size_t)index * ty.SizeOf(type);
                    continue;
                }
                if (!first && !Eat("."))
                    break;
                auto name = Ident();
                type = ty.Unqualified(type);
                if (!type->IsStruct() || !type->st->complete)
                    Error(t.loc, "offsetof requires a complete struct");
                const FieldInfo *field = nullptr;
                for (auto &f : type->st->fields)
                    if (f.name == name)
                        field = &f;
                if (!field)
                    Error(t.loc, "offsetof: no member named '" + name + "'");
                offset += field->offset;
                type = field->type;
            }
            Need(")");
            e = Number(offset, ty.SizeT(), t.loc);
        }
        else if (t.Is("static_cast") || t.Is("reinterpret_cast") || t.Is("const_cast"))
        {
            Need("<");
            auto s = Specs();
            auto d = Decl(s.type, s.conv, true);
            Need(">");
            Need("(");
            e = Cast(Expression(), d.type, true);
            Need(")");
        }
        else if (t.Is("("))
        {
            size_t save = p;
            if (TypeStart())
            {
                auto s = Specs();
                auto d = Decl(s.type, s.conv, true);
                if (Eat(")"))
                    e = Cast(Prefix(), d.type, true);
                else
                    p = save;
            }
            if (!e)
            {
                p = save;
                e = Expression();
                Need(")");
            }
        }
        else if (t.Is("["))
        {
            if (!Eat("]"))
                Error(t.loc, "lambda captures are not supported");
            Spec s;
            s.internal = true;
            auto d = Decl(autoType, CallConv::Cdecl, true);
            d.name = "$lambda" + std::to_string(u.functions.size());
            if (!d.type->IsFunction())
                d.type = ty.Function({autoType, {}, false, CallConv::Cdecl});
            Eat("mutable");
            if (Eat("->"))
            {
                auto rs = Specs();
                auto rd = Decl(rs.type, rs.conv, true);
                auto sig = d.type->sig;
                sig.ret = rd.type;
                d.type = ty.Function(sig);
            }
            auto fn = DeclareFunction(d, s);
            Body(fn);
            e = New(EK::Function, ty.Closure(fn), t.loc);
            e->fn = fn;
        }
        else if (t.kind == Tok::Ident || t.Is("::"))
        {
            --p;
            size_t save = p;
            auto type = NamedType();
            if (type && Eat("("))
            {
                std::vector<Expr *> args;
                if (!Eat(")"))
                {
                    do
                    {
                        args.push_back(Expression(2));
                    } while (Eat(","));
                    Need(")");
                }
                e = Construct(type, std::move(args), t.loc);
            }
            else
            {
                p = save;
                auto name = ReadName();
                VarDecl *v = nullptr;
                if (!name.qualified)
                    for (size_t i = locals.size(); i > (localBoundaries.empty() ? 0 : localBoundaries.back()) && !v;)
                    {
                        --i;
                        auto it = locals[i].find(name.name);
                        if (it != locals[i].end())
                            v = it->second;
                    }
                if (!v)
                    v = Lookup<VarDecl *>(name, &Namespace::variables);
                if (v)
                {
                    if (v->enumerator)
                        e = Number(v->constantValue, v->type, t.loc);
                    else
                    {
                        e = New(EK::Variable, v->type->IsRef() ? v->type->base : v->type, t.loc);
                        e->var = v;
                        e->lvalue = true;
                    }
                }
                else
                {
                    auto templ = Lookup<TemplateDecl *>(name, &Namespace::templates);
                    auto fns = Lookup<std::vector<FuncDecl *>>(name, &Namespace::functions);
                    if (templ)
                    {
                        e = New(EK::Function, ty.Void(), t.loc);
                        e->templ = templ;
                        return e;
                    }
                    if (fns.empty())
                    {
                        // Unqualified field access in a member function.
                        VarDecl *self = nullptr;
                        for (auto i = locals.rbegin(); i != locals.rend() && !self; ++i)
                        {
                            auto it = i->find("this");
                            if (it != i->end())
                                self = it->second;
                        }
                        if (self && !name.qualified)
                        {
                            auto base = New(EK::Variable, self->type, t.loc);
                            base->var = self;
                            base->lvalue = true;
                            e = Member(base, name.name, true, t.loc);
                        }
                        else
                            Error(t.loc, "unknown name '" + name.name + "'");
                    }
                    else
                    {
                        e = New(EK::Function, fns.front()->type, t.loc);
                        e->overloads = std::move(fns);
                        if (e->overloads.size() == 1)
                            e->fn = e->overloads.front();
                        if (!name.qualified && !e->overloads.front()->params.empty() &&
                            e->overloads.front()->params[0]->name == "this")
                        {
                            VarDecl *self = nullptr;
                            // no boundary while parsing class-scope expressions (default member initializers)
                            for (size_t i = locals.size(); i > (localBoundaries.empty() ? 0 : localBoundaries.back()) && !self;)
                            {
                                --i;
                                auto it = locals[i].find("this");
                                if (it != locals[i].end())
                                    self = it->second;
                            }
                            if (self)
                            {
                                auto receiver = New(EK::Variable, self->type, t.loc);
                                receiver->var = self;
                                receiver->lvalue = true;
                                e->a = receiver;
                            }
                        }
                    }
                }
            }
        }
        else
            Error(t.loc, "expression expected, got '" + t.text + "'");
        return e;
    }
    Expr *Member(Expr *base, std::string name, bool arrow, Loc loc)
    {
        auto bt = base->type;
        if (bt->IsRef())
            bt = bt->base;
        if (arrow)
        {
            if (!Decay(bt)->IsPointer())
                Error(loc, "-> requires a pointer");
            base = Unary("*", base, loc);
            bt = base->type;
        }
        if (!bt->IsStruct())
            Error(loc, "member access requires a struct");
        for (auto &f : bt->st->fields)
            if (f.name == name)
            {
                auto e = New(EK::Member, bt->isConst ? ty.Const(f.type) : f.type, loc);
                e->a = base;
                e->offset = f.offset;
                e->lvalue = true;
                return e;
            }
        auto it = bt->st->scope->functions.find(name);
        auto templ = bt->st->scope->templates.find(name);
        if (it != bt->st->scope->functions.end() || templ != bt->st->scope->templates.end())
        {
            auto e =
                New(EK::Function, it != bt->st->scope->functions.end() ? it->second.front()->type : ty.Void(), loc);
            e->a = bt->st->handle || bt->st->value ? base : Unary("&", base, loc);
            if (it != bt->st->scope->functions.end())
                e->overloads = it->second;
            if (templ != bt->st->scope->templates.end())
                e->templ = templ->second;
            return e;
        }
        Error(loc, "unknown member '" + name + "'");
    }
    Expr *Prefix()
    {
        DepthGuard guard(*this, Peek().loc);
        if (Is("+") || Is("-") || Is("!") || Is("~") || Is("*") || Is("&") || Is("++") || Is("--"))
        {
            auto t = Take();
            return Unary(t.text, Prefix(), t.loc);
        }
        auto e = Primary();
        for (;;)
        {
            if (e->templ && Eat("<"))
            {
                do
                {
                    auto s = Specs();
                    auto d = Decl(s.type, s.conv, true);
                    e->templateArgs.push_back(d.type);
                } while (Eat(","));
                Need(">");
                continue;
            }
            if (Eat("("))
            {
                std::vector<Expr *> args;
                if (e->a && e->kind == EK::Function)
                    args.push_back(e->a);
                if (!Eat(")"))
                {
                    do
                    {
                        args.push_back(Expression(2));
                    } while (Eat(","));
                    Need(")");
                }
                e = ResolveCall(e, std::move(args), e->loc);
            }
            else if (Eat("["))
            {
                auto index = Expression();
                Need("]");
                e = Unary("*", Binary("+", e, index, e->loc), e->loc);
            }
            else if (Is(".") || Is("->"))
            {
                bool arrow = Take().Is("->");
                e = Member(e, Ident(), arrow, e->loc);
            }
            else if (Eat("++"))
                e = Unary("post++", e, e->loc);
            else if (Eat("--"))
                e = Unary("post--", e, e->loc);
            else
                break;
        }
        return e;
    }
    static int Precedence(const std::string &op)
    {
        static const std::map<std::string, int> ps = {
            {",", 1},   {"=", 2},   {"+=", 2},  {"-=", 2},  {"*=", 2}, {"/=", 2}, {"%=", 2},  {"&=", 2},
            {"|=", 2},  {"^=", 2},  {"<<=", 2}, {">>=", 2}, {"?", 3},  {"||", 4}, {"&&", 5},  {"|", 6},
            {"^", 7},   {"&", 8},   {"==", 9},  {"!=", 9},  {"<", 10}, {">", 10}, {"<=", 10}, {">=", 10},
            {"<<", 11}, {">>", 11}, {"+", 12},  {"-", 12},  {"*", 13}, {"/", 13}, {"%", 13}};
        auto i = ps.find(op);
        return i == ps.end() ? 0 : i->second;
    }
    Expr *Expression(int min = 1)
    {
        DepthGuard guard(*this, Peek().loc);
        auto e = Prefix();
        while (int prec = Precedence(Peek().text))
        {
            if (prec < min)
                break;
            auto t = Take();
            if (t.Is("?"))
            {
                auto a = Expression();
                Need(":");
                auto b = Expression(2);
                Type *type = a->type == b->type ? a->type : ConditionalType(a, b);
                auto r = New(EK::Conditional, type, t.loc);
                r->a = Cast(e, ty.Bool());
                r->b = Cast(a, type);
                r->c = Cast(b, type);
                e = r;
            }
            else
                e = Binary(t.text, e,
                           t.Is("=") && Is("{") ? Initializer(e->type) : Expression(prec + (prec == 2 ? 0 : 1)), t.loc);
        }
        return e;
    }
    Stmt *ParseStatement()
    {
        Loc loc = Peek().loc;
        DepthGuard guard(*this, loc);
        if (Eat("{"))
        {
            auto s = Statement(SK::Block, loc);
            auto saved = scope;
            scope = Node<Namespace>();
            scope->parent = saved;
            scope->name = saved->name;
            locals.emplace_back();
            while (!Eat("}"))
                s->body.push_back(ParseStatement());
            locals.pop_back();
            scope = saved;
            return s;
        }
        if (Eat("goto"))
        {
            auto s = Statement(SK::Goto, loc);
            s->label = Ident();
            for (size_t i = localBoundaries.back(); i < locals.size(); ++i)
                for (auto &v : locals[i])
                    s->live.push_back(v.second);
            Need(";");
            return s;
        }
        if (!Is("default") && Peek().kind == Tok::Ident && Peek(1).Is(":"))
        {
            auto s = Statement(SK::Label, loc);
            s->label = Ident();
            Need(":");
            for (size_t i = localBoundaries.back(); i < locals.size(); ++i)
                for (auto &v : locals[i])
                    s->live.push_back(v.second);
            s->a = ParseStatement();
            return s;
        }
        if (Is("using") || Is("static_assert"))
        {
            Top();
            return Statement(SK::Empty, loc);
        }
        if (Eat(";"))
            return Statement(SK::Empty, loc);
        if (Is("throw") || Is("try") || Is("catch"))
            Error(loc, "exceptions are not supported");
        if (Eat("return"))
        {
            auto s = Statement(SK::Return, loc);
            if (!Eat(";"))
            {
                s->expr = Expression();
                Need(";");
            }
            auto ret = function->type->sig.ret;
            if (ret == autoType)
            {
                auto sig = function->type->sig;
                sig.ret = s->expr ? Decay(s->expr->type) : ty.Void();
                function->type = ty.Function(sig);
                ret = sig.ret;
            }
            if (s->expr)
            {
                if (ret->IsVoid())
                    Error(loc, "value returned from void function");
                if (ret->IsStruct() && ret->st->handle && s->expr->lvalue)
                    Error(loc, "hook handles cannot be copied");
                s->expr = Cast(s->expr, ret);
            }
            else if (!ret->IsVoid())
                Error(loc, "return value required");
            return s;
        }
        if (Eat("if"))
        {
            auto s = Statement(SK::If, loc);
            Need("(");
            s->condition = Cast(Expression(), ty.Bool());
            Need(")");
            s->a = ParseStatement();
            if (Eat("else"))
                s->b = ParseStatement();
            return s;
        }
        if (Eat("while"))
        {
            auto s = Statement(SK::While, loc);
            Need("(");
            s->condition = Cast(Expression(), ty.Bool());
            Need(")");
            ++loops;
            s->a = ParseStatement();
            --loops;
            return s;
        }
        if (Eat("do"))
        {
            auto s = Statement(SK::Do, loc);
            ++loops;
            s->a = ParseStatement();
            --loops;
            Need("while");
            Need("(");
            s->condition = Cast(Expression(), ty.Bool());
            Need(")");
            Need(";");
            return s;
        }
        if (Eat("for"))
        {
            auto s = Statement(SK::For, loc);
            Need("(");
            locals.emplace_back();
            s->a = ParseStatement();
            if (!Eat(";"))
            {
                s->condition = Cast(Expression(), ty.Bool());
                Need(";");
            }
            if (!Eat(")"))
            {
                s->step = Expression();
                Need(")");
            }
            ++loops;
            s->b = ParseStatement();
            --loops;
            locals.pop_back();
            return s;
        }
        if (Eat("break"))
        {
            if (!loops && !switches)
                Error(loc, "break outside loop or switch");
            Need(";");
            return Statement(SK::Break, loc);
        }
        if (Eat("continue"))
        {
            if (!loops)
                Error(loc, "continue outside loop");
            Need(";");
            return Statement(SK::Continue, loc);
        }
        if (Eat("switch"))
        {
            auto s = Statement(SK::Switch, loc);
            Need("(");
            s->expr = Expression();
            if (!Decay(s->expr->type)->IsInteger() && !s->expr->type->IsEnum())
                Error(loc, "switch requires an integral expression");
            Need(")");
            ++switches;
            s->a = ParseStatement();
            --switches;
            return s;
        }
        if (Is("case") || Is("default"))
        {
            if (!switches)
                Error(loc, "case outside switch");
            auto s = Statement(SK::Case, loc);
            s->isDefault = Eat("default");
            if (!s->isDefault)
            {
                Need("case");
                auto e = Expression(2);
                if (!Constant(e, s->value))
                    Error(loc, "case must be constant");
            }
            Need(":");
            s->a = ParseStatement();
            return s;
        }
        if (TypeStart() && !FunctionalCastStatement())
        {
            auto vs = Declaration(false);
            auto s = Statement(SK::Block, loc);
            for (auto v : vs)
            {
                auto d = Statement(SK::Declaration, v->loc);
                d->var = v;
                s->body.push_back(d);
            }
            return s;
        }
        auto s = Statement(SK::Expr, loc);
        s->expr = Expression();
        Need(";");
        return s;
    }

  public:
    Parser(TranslationUnit &unit, const std::vector<Token> &tokens)
        : u(unit), ty(unit.types), ts(tokens), scope(&u.root)
    {
        autoType = Node<Type>();
        autoType->kind = TK::Void;
    }
    void Run()
    {
        while (Peek().kind != Tok::End)
            Top();
        for (size_t i = 0; i < deferred.size(); ++i)
        {
            auto d = deferred[i];
            p = d.first;
            scope = d.scope;
            Body(d.fn);
            if (p != d.last)
                Error(d.fn->loc, "invalid member body");
        }
    }
};
} // namespace
void Parse(TranslationUnit &unit, const std::vector<Token> &tokens)
{
    Parser(unit, tokens).Run();
}

bool Constant(Expr *e, uint64_t &r)
{
    if (!e)
        return false;
    if (e->kind == EK::Call)
        return ConstantCall(e, r);
    if (e->kind == EK::Number)
    {
        r = e->value;
        return true;
    }
    if (e->kind == EK::Variable && e->var->hasConstant)
    {
        r = e->var->constantValue;
        return true;
    }
    uint64_t a, b;
    if (e->kind == EK::Cast && Constant(e->a, a))
    {
        r = ConvertValue(a, (int)e->a->type->kind, (int)e->type->kind);
        return true;
    }
    if (e->kind == EK::Unary && Constant(e->a, a))
    {
        if (e->op == "+")
        {
            r = a;
            return true;
        }
        if (e->op == "-")
        {
            r = e->type->IsFloating() ? a ^ (e->type->kind == TK::Float ? 0x80000000ULL : 0x8000000000000000ULL)
                                      : Calculate(0, a, 1, (int)e->type->kind);
            return true;
        }
        if (e->op == "!")
        {
            r = !a;
            return true;
        }
        if (e->op == "~")
        {
            r = ~a;
            return true;
        }
    }
    if (e->kind == EK::Conditional && Constant(e->a, a))
        return Constant(a ? e->b : e->c, r);
    if (e->kind == EK::Binary && Constant(e->a, a))
    {
        if (e->op == "&&" && !a)
        {
            r = 0;
            return true;
        }
        if (e->op == "||" && a)
        {
            r = 1;
            return true;
        }
        if (!Constant(e->b, b))
            return false;
        static const std::vector<std::string> ops = {"+", "-",  "*",  "/", "%", "<<", ">>", "&",  "|",
                                                     "^", "==", "!=", "<", ">", "<=", ">=", "&&", "||"};
        auto i = std::find(ops.begin(), ops.end(), e->op);
        if (i == ops.end())
            return false;
        if ((e->op == "/" || e->op == "%") && !b)
            Error(e->loc, "division by zero in constant expression");
        r = Calculate(a, b, (int)(i - ops.begin()), (int)e->a->type->kind);
        return true;
    }
    return false;
}
} // namespace cxxsnippets
