#include "types.hpp"
#include "ast.hpp"

namespace cxxsnippets
{
Type *TypeTable::Const(Type *type)
{
    if (type->isConst || type->IsRef() || type->IsFunction())
        return type;
    auto &slot = constTypes[type];
    if (!slot)
    {
        slot = arena.New<Type>(*type);
        slot->isConst = true;
        unqualified[slot] = type;
    }
    return slot;
}
Type *TypeTable::Unqualified(Type *type)
{
    auto it = unqualified.find(type);
    return it == unqualified.end() ? type : it->second;
}
TypeTable::TypeTable(Target t) : target(t)
{
    for (int k = 0; k <= (int)TK::AnyAddress; ++k)
    {
        Type *ty = arena.New<Type>();
        ty->kind = (TK)k;
        basics.push_back(ty);
    }
}

Type *TypeTable::Pointer(Type *to)
{
    auto &slot = derived[{(int)TK::Pointer, to}];
    if (!slot)
    {
        slot = arena.New<Type>();
        slot->kind = TK::Pointer;
        slot->base = to;
    }
    return slot;
}

Type *TypeTable::Ref(Type *to)
{
    if (to->kind == TK::Ref)
        return to; // reference collapsing
    auto &slot = derived[{(int)TK::Ref, to}];
    if (!slot)
    {
        slot = arena.New<Type>();
        slot->kind = TK::Ref;
        slot->base = to;
    }
    return slot;
}

Type *TypeTable::Array(Type *elem, size_t count)
{
    auto &slot = arrays[{elem, count}];
    if (!slot)
    {
        slot = arena.New<Type>();
        slot->kind = TK::Array;
        slot->base = elem;
        slot->count = count;
    }
    return slot;
}

Type *TypeTable::Function(const FuncSig &sigIn)
{
    FuncSig sig = sigIn;
    if (target.x64 || sig.conv == CallConv::Default)
        sig.conv = CallConv::Cdecl;
    for (Type *f : functions)
    {
        const FuncSig &o = f->sig;
        if (o.ret == sig.ret && o.params == sig.params && o.variadic == sig.variadic && o.conv == sig.conv)
            return f;
    }
    Type *t = arena.New<Type>();
    t->kind = TK::Function;
    t->sig = sig;
    functions.push_back(t);
    return t;
}

Type *TypeTable::NewStruct(StructInfo *info)
{
    Type *t = arena.New<Type>();
    t->kind = TK::Struct;
    t->st = info;
    return t;
}

Type *TypeTable::NewEnum(EnumInfo *info)
{
    Type *t = arena.New<Type>();
    t->kind = TK::Enum;
    t->en = info;
    t->base = info->underlying;
    return t;
}

Type *TypeTable::Closure(FuncDecl *fn)
{
    auto &slot = closures[fn];
    if (!slot)
    {
        slot = arena.New<Type>();
        slot->kind = TK::Closure;
        slot->closure = fn;
    }
    return slot;
}

Type *TypeTable::MemberFn(FuncDecl *fn)
{
    auto &slot = memberFns[fn];
    if (!slot)
    {
        slot = arena.New<Type>();
        slot->kind = TK::MemberFn;
        slot->closure = fn;
    }
    return slot;
}

size_t TypeTable::SizeOf(const Type *t) const
{
    switch (t->kind)
    {
    case TK::Void:
        return 0;
    case TK::Bool:
    case TK::Char:
    case TK::SChar:
    case TK::UChar:
        return 1;
    case TK::WChar:
    case TK::Char16:
    case TK::Short:
    case TK::UShort:
        return 2;
    case TK::Char32:
    case TK::Int:
    case TK::UInt:
    case TK::Long:
    case TK::ULong:
    case TK::Float:
        return 4;
    case TK::LongLong:
    case TK::ULongLong:
    case TK::Double:
    case TK::LongDouble:
        return 8;
    case TK::Nullptr:
    case TK::Pointer:
    case TK::Ref:
    case TK::MemberFn:
    case TK::AnyAddress:
        return target.ptrSize();
    case TK::Array:
        return SizeOf(t->base) * t->count;
    case TK::Function:
        return 0;
    case TK::Struct:
        return t->st->size;
    case TK::Enum:
        return SizeOf(t->en->underlying);
    case TK::Closure:
        return 1;
    }
    return 0;
}

size_t TypeTable::AlignOf(const Type *t) const
{
    switch (t->kind)
    {
    case TK::Array:
        return AlignOf(t->base);
    case TK::Struct:
        return t->st->align;
    case TK::Enum:
        return AlignOf(t->en->underlying);
    case TK::Closure:
    case TK::Void:
    case TK::Function:
        return 1;
    default:
        return SizeOf(t);
    }
}

bool TypeTable::IsSigned(const Type *t) const
{
    switch (t->kind)
    {
    case TK::Char:
    case TK::SChar:
    case TK::Short:
    case TK::Int:
    case TK::Long:
    case TK::LongLong:
        return true;
    case TK::Enum:
        return IsSigned(t->en->underlying);
    default:
        return false;
    }
}

static std::string ConvName(CallConv c)
{
    switch (c)
    {
    case CallConv::Stdcall:
        return "__stdcall ";
    case CallConv::Fastcall:
        return "__fastcall ";
    case CallConv::Thiscall:
        return "__thiscall ";
    default:
        return "";
    }
}

std::string TypeTable::Name(const Type *t)
{
    if (t->isConst)
    {
        Type plain = *t;
        plain.isConst = false;
        return "const " + Name(&plain);
    }
    switch (t->kind)
    {
    case TK::Void:
        return "void";
    case TK::Bool:
        return "bool";
    case TK::Char:
        return "char";
    case TK::SChar:
        return "signed char";
    case TK::UChar:
        return "unsigned char";
    case TK::WChar:
        return "wchar_t";
    case TK::Char16:
        return "char16_t";
    case TK::Char32:
        return "char32_t";
    case TK::Short:
        return "short";
    case TK::UShort:
        return "unsigned short";
    case TK::Int:
        return "int";
    case TK::UInt:
        return "unsigned int";
    case TK::Long:
        return "long";
    case TK::ULong:
        return "unsigned long";
    case TK::LongLong:
        return "__int64";
    case TK::ULongLong:
        return "unsigned __int64";
    case TK::Float:
        return "float";
    case TK::Double:
        return "double";
    case TK::LongDouble:
        return "long double";
    case TK::Nullptr:
        return "std::nullptr_t";
    case TK::Pointer:
        if (t->base->kind == TK::Function)
        {
            std::string s = Name(t->base->sig.ret) + " (" + ConvName(t->base->sig.conv) + "*)(";
            for (size_t i = 0; i < t->base->sig.params.size(); ++i)
                s += (i ? "," : "") + Name(t->base->sig.params[i]);
            if (t->base->sig.variadic)
                s += t->base->sig.params.empty() ? "..." : ",...";
            return s + ")";
        }
        return Name(t->base) + " *";
    case TK::Ref:
        return Name(t->base) + " &";
    case TK::Array:
        return Name(t->base) + " [" + (t->count ? std::to_string(t->count) : "") + "]";
    case TK::Function: {
        std::string s = Name(t->sig.ret) + " " + ConvName(t->sig.conv) + "(";
        for (size_t i = 0; i < t->sig.params.size(); ++i)
            s += (i ? "," : "") + Name(t->sig.params[i]);
        if (t->sig.variadic)
            s += t->sig.params.empty() ? "..." : ",...";
        return s + ")";
    }
    case TK::Struct:
        return t->st->name;
    case TK::Enum:
        return t->en->name;
    case TK::Closure:
        return "lambda";
    case TK::MemberFn:
        return "pointer to member function";
    case TK::AnyAddress:
        return "address";
    }
    return "?";
}

CallConv EffectiveConv(CallConv c, bool x64, bool member)
{
    if (x64)
        return CallConv::Cdecl;
    if (c == CallConv::Default)
        return member ? CallConv::Thiscall : CallConv::Cdecl;
    return c;
}
} // namespace cxxsnippets
