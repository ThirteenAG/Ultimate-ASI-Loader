#include "codegen.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>

namespace cxxsnippets
{
namespace
{
bool Floating(TK k)
{
    return k == TK::Float || k == TK::Double || k == TK::LongDouble;
}
bool Signed(TK k)
{
    return k == TK::Char || k == TK::SChar || k == TK::Short || k == TK::Int || k == TK::Long || k == TK::LongLong;
}
int Width(TK k)
{
    switch (k)
    {
    case TK::Bool:
    case TK::Char:
    case TK::SChar:
    case TK::UChar:
        return 8;
    case TK::WChar:
    case TK::Char16:
    case TK::Short:
    case TK::UShort:
        return 16;
    case TK::Char32:
    case TK::Int:
    case TK::UInt:
    case TK::Long:
    case TK::ULong:
    case TK::Float:
        return 32;
    case TK::Pointer:
    case TK::Ref:
    case TK::Nullptr:
        return sizeof(void *) * 8;
    default:
        return 64;
    }
}
uint64_t Normalize(uint64_t v, TK k)
{
    if (k == TK::Bool)
        return v != 0;
    int n = Width(k);
    if (n == 64)
        return v;
    uint64_t mask = (uint64_t(1) << n) - 1;
    v &= mask;
    if (Signed(k) && (v & (uint64_t(1) << (n - 1))))
        v |= ~mask;
    return v;
}
double Real(uint64_t v, TK k)
{
    if (k == TK::Float)
    {
        float f;
        uint32_t b = (uint32_t)v;
        std::memcpy(&f, &b, 4);
        return f;
    }
    double d;
    std::memcpy(&d, &v, 8);
    return d;
}
uint64_t Bits(double d, TK k)
{
    uint64_t v = 0;
    if (k == TK::Float)
    {
        float f = (float)d;
        std::memcpy(&v, &f, 4);
    }
    else
        std::memcpy(&v, &d, 8);
    return v;
}
} // namespace
uint64_t ConvertValue(uint64_t v, int from, int to)
{
    auto f = (TK)from, t = (TK)to;
    if (t == TK::Void)
        return v;
    if (Floating(f))
    {
        double d = Real(v, f);
        if (Floating(t))
            return Bits(d, t);
        if (t == TK::Bool)
            return d != 0;
        return Normalize(Signed(t) ? (uint64_t)(int64_t)d : (uint64_t)d, t);
    }
    v = Normalize(v, f);
    if (Floating(t))
        return Bits(Signed(f) ? (double)(int64_t)v : (double)v, t);
    return Normalize(v, t);
}
uint64_t Calculate(uint64_t a, uint64_t b, int op, int kind)
{
    TK k = (TK)kind;
    if (Floating(k))
    {
        double x = Real(a, k), y = Real(b, k);
        switch (op)
        {
        case 0:
            return Bits(x + y, k);
        case 1:
            return Bits(x - y, k);
        case 2:
            return Bits(x * y, k);
        case 3:
            return Bits(x / y, k);
        case 10:
            return x == y;
        case 11:
            return x != y;
        case 12:
            return x < y;
        case 13:
            return x > y;
        case 14:
            return x <= y;
        case 15:
            return x >= y;
        default:
            return 0;
        }
    }
    a = Normalize(a, k);
    b = Normalize(b, k);
    bool sign = Signed(k);
    int64_t x = (int64_t)a, y = (int64_t)b;
    uint64_t r = 0;
    switch (op)
    {
    case 0:
        r = a + b;
        break;
    case 1:
        r = a - b;
        break;
    case 2:
        r = a * b;
        break;
    case 3:
        if (!b)
            return 0;
        if (sign)
        {
            if (x == INT64_MIN && y == -1)
                r = a;
            else
                r = (uint64_t)(x / y);
        }
        else
            r = a / b;
        break;
    case 4:
        if (!b)
            return 0;
        if (sign)
        {
            if (x == INT64_MIN && y == -1)
                r = 0;
            else
                r = (uint64_t)(x % y);
        }
        else
            r = a % b;
        break;
    case 5:
        r = a << (b & (Width(k) - 1));
        break;
    case 6:
        r = sign ? (uint64_t)(x >> (b & (Width(k) - 1))) : a >> (b & (Width(k) - 1));
        break;
    case 7:
        r = a & b;
        break;
    case 8:
        r = a | b;
        break;
    case 9:
        r = a ^ b;
        break;
    case 10:
        return a == b;
    case 11:
        return a != b;
    case 12:
        return sign ? x < y : a < b;
    case 13:
        return sign ? x > y : a > b;
    case 14:
        return sign ? x <= y : a <= b;
    case 15:
        return sign ? x >= y : a >= b;
    case 16:
        return a && b;
    case 17:
        return a || b;
    }
    return Normalize(r, k);
}
namespace
{
class Generator
{
    TranslationUnit &u;
    TypeTable &ty;
    std::function<void *(const std::string &)> resolve;
    Image image;
    bool x64;
    FuncDecl *fn = nullptr;
    size_t depth = 0;
    std::vector<size_t> returns;
    struct Flow
    {
        std::vector<size_t> breaks, continues;
        bool loop;
    };
    std::vector<Flow> flow;
    std::map<Stmt *, size_t> cases;
    std::map<std::string, std::pair<Stmt *, size_t>> labels;
    std::vector<std::pair<Stmt *, size_t>> gotos;
    std::map<Expr *, size_t> temporarySlots;
    void Transfer(Expr *e)
    {
        if (!e)
            return;
        if (e->type->IsStruct() && e->type->st->handle)
        {
            e->transferred = true;
            if (e->kind == EK::Conditional)
            {
                Transfer(e->b);
                Transfer(e->c);
            }
            else if (e->kind == EK::Cast)
                Transfer(e->a);
            else if (e->kind == EK::Binary && e->op == ",")
                Transfer(e->b);
        }
    }
    void FindTemps(Expr *e, size_t &offset)
    {
        if (!e)
            return;
        if (e->kind == EK::Call && e->a->fn && e->a->fn->intrinsic.rfind("invoke:", 0) == 0 && !e->args[0]->lvalue)
        {
            auto receiver = e->args[0];
            Transfer(receiver);
            if (!temporarySlots.count(receiver))
            {
                offset = (offset + 7) / 8 * 8 + 8;
                temporarySlots[receiver] = offset;
            }
        }
        if (e->kind == EK::Call && e->type->IsStruct() && e->type->st->handle && !e->transferred &&
            !temporarySlots.count(e))
        {
            offset = (offset + 7) / 8 * 8 + 8;
            temporarySlots[e] = offset;
        }
        FindTemps(e->a, offset);
        FindTemps(e->b, offset);
        FindTemps(e->c, offset);
        for (auto a : e->args)
            FindTemps(a, offset);
    }
    void FindTemps(Stmt *s, size_t &offset)
    {
        if (!s)
            return;
        if (s->var && s->var->type->IsStruct() && s->var->type->st->handle)
            Transfer(s->var->init);
        if (s->kind == SK::Return && fn->type->sig.ret->IsStruct() && fn->type->sig.ret->st->handle)
            Transfer(s->expr);
        FindTemps(s->expr, offset);
        FindTemps(s->condition, offset);
        FindTemps(s->step, offset);
        if (s->var)
            FindTemps(s->var->init, offset);
        FindTemps(s->a, offset);
        FindTemps(s->b, offset);
        for (auto b : s->body)
            FindTemps(b, offset);
    }
    void CleanupTemps()
    {
        if (temporarySlots.empty())
            return;
        Push();
        for (auto &entry : temporarySlots)
        {
            FrameAddress(entry.second);
            Load(ty.Pointer(ty.Void()));
            Release(entry.first->type);
            FrameAddress(entry.second);
            if (x64)
                Bytes({0x48, 0xc7, 0x00});
            else
                Bytes({0xc7, 0x00});
            Dword(0);
        }
        if (x64)
            Byte(0x58);
        else
            Bytes({0x58, 0x5a});
        depth -= 8;
    }
    void Byte(uint8_t b)
    {
        image.code.push_back(b);
    }
    void Bytes(std::initializer_list<uint8_t> bs)
    {
        image.code.insert(image.code.end(), bs);
    }
    void Dword(uint32_t v)
    {
        for (int i = 0; i < 4; ++i)
            Byte((uint8_t)(v >> (8 * i)));
    }
    void Qword(uint64_t v)
    {
        for (int i = 0; i < 8; ++i)
            Byte((uint8_t)(v >> (8 * i)));
    }
    size_t Here() const
    {
        return image.code.size();
    }
    void Patch(size_t at, size_t dest)
    {
        int64_t delta = (int64_t)dest - (int64_t)(at + 4);
        if (delta < INT32_MIN || delta > INT32_MAX)
            Error(fn->loc, "branch out of range");
        uint32_t v = (uint32_t)delta;
        std::memcpy(image.code.data() + at, &v, 4);
    }
    size_t Jump(int condition = -1)
    {
        if (condition < 0)
            Byte(0xe9);
        else
            Bytes({0x0f, (uint8_t)(0x80 + condition)});
        size_t at = Here();
        Dword(0);
        return at;
    }
    void Back(size_t to)
    {
        Patch(Jump(), to);
    }
    void Imm(uint64_t v)
    {
        if (x64)
        {
            Bytes({0x48, 0xb8});
            Qword(v);
        }
        else
        {
            Byte(0xb8);
            Dword((uint32_t)v);
            Byte(0xba);
            Dword((uint32_t)(v >> 32));
        }
    }
    void Pointer(Relocation r)
    {
        if (x64)
        {
            Bytes({0x48, 0xb8});
            r.offset = Here();
            Qword(0);
        }
        else
        {
            Byte(0xb8);
            r.offset = Here();
            Dword(0);
            Bytes({0x31, 0xd2});
        }
        image.relocations.push_back(r);
    }
    void Push()
    {
        if (x64)
            Byte(0x50);
        else
            Bytes({0x52, 0x50});
        depth += 8;
    }
    void PopAddress()
    {
        if (x64)
            Byte(0x59);
        else
            Bytes({0x59, 0x83, 0xc4, 0x04});
        depth -= 8;
    }
    void StackAdd(size_t n)
    {
        if (!n)
            return;
        if (x64)
            Byte(0x48);
        Bytes({0x81, 0xc4});
        Dword((uint32_t)n);
        depth -= n;
    }
    void StackSub(size_t n)
    {
        if (!n)
            return;
        if (x64)
            Byte(0x48);
        Bytes({0x81, 0xec});
        Dword((uint32_t)n);
        depth += n;
    }
    void FrameAddress(size_t offset)
    {
        if (x64)
            Byte(0x48);
        Bytes({0x8d, 0x85});
        Dword((uint32_t)-(int32_t)offset);
        if (!x64)
            Bytes({0x31, 0xd2});
    }
    void StackLoad(int reg, size_t offset, bool wide = true)
    {
        if (x64 && wide)
            Byte((uint8_t)(0x48 | (reg >= 8 ? 4 : 0)));
        else if (x64 && reg >= 8)
            Byte(0x44);
        Bytes({0x8b, (uint8_t)(0x84 | ((reg & 7) << 3)), 0x24});
        Dword((uint32_t)offset);
    }
    void StackStore(int reg, size_t offset, bool wide = true)
    {
        if (x64 && wide)
            Byte((uint8_t)(0x48 | (reg >= 8 ? 4 : 0)));
        else if (x64 && reg >= 8)
            Byte(0x44);
        Bytes({0x89, (uint8_t)(0x84 | ((reg & 7) << 3)), 0x24});
        Dword((uint32_t)offset);
    }
    TK Kind(Type *t)
    {
        if (t->IsEnum())
            t = t->base;
        if (t->IsRef())
            t = ty.Pointer(t->base);
        if (t->IsArray() || t->IsFunction() || t->kind == TK::Closure || t->kind == TK::AnyAddress ||
            (t->IsStruct() && (t->st->handle || t->st->value)))
            t = ty.Pointer(ty.Void());
        return t->kind;
    }
    void Load(Type *type)
    {
        size_t n = ty.SizeOf(type);
        bool sign = ty.IsSigned(type);
        if (type->IsArray() || (type->IsStruct() && !(type->st->handle || type->st->value)) || type->IsFunction())
            return;
        if (n == 1)
        {
            if (x64)
                Byte(0x48);
            Bytes({0x0f, (uint8_t)(sign ? 0xbe : 0xb6), 0x00});
        }
        else if (n == 2)
        {
            if (x64)
                Byte(0x48);
            Bytes({0x0f, (uint8_t)(sign ? 0xbf : 0xb7), 0x00});
        }
        else if (n == 4)
        {
            if (x64 && sign)
                Bytes({0x48, 0x63, 0x00});
            else
                Bytes({0x8b, 0x00});
        }
        else if (n == 8)
        {
            if (x64)
                Bytes({0x48, 0x8b, 0x00});
            else
                Bytes({0x8b, 0x50, 0x04, 0x8b, 0x00});
        }
        else
            Error(fn->loc, "unsupported load size");
        if (!x64 && n < 8)
        {
            if (sign)
                Byte(0x99);
            else
                Bytes({0x31, 0xd2});
        }
    }
    void Store(Type *type)
    {
        size_t n = ty.SizeOf(type);
        if (n == 1)
            Bytes({0x88, 0x01});
        else if (n == 2)
            Bytes({0x66, 0x89, 0x01});
        else if (n == 4)
            Bytes({0x89, 0x01});
        else if (n == 8)
        {
            if (x64)
                Bytes({0x48, 0x89, 0x01});
            else
                Bytes({0x89, 0x01, 0x89, 0x51, 0x04});
        }
        else
            Error(fn->loc, "aggregate assignment is not supported");
    }
    void Address(Expr *e)
    {
        if (e->kind == EK::Variable)
        {
            auto v = e->var;
            if (v->global)
            {
                Relocation r{};
                r.variable = v;
                Pointer(r);
            }
            else
                FrameAddress(v->offset);
            if (v->type->IsRef())
                Load(v->type);
            return;
        }
        if (e->kind == EK::Unary && e->op == "*")
        {
            Value(e->a);
            return;
        }
        if (e->kind == EK::Member)
        {
            Address(e->a);
            if (e->offset)
            {
                if (x64)
                    Byte(0x48);
                Byte(0x05);
                Dword((uint32_t)e->offset);
            }
            return;
        }
        if (e->kind == EK::String)
        {
            if (e->bytes.size())
            {
                e->offset = image.data.size();
                image.data.insert(image.data.end(), e->bytes.begin(), e->bytes.end());
                e->bytes.clear();
            }
            Relocation r{};
            r.dataOffset = e->offset;
            Pointer(r);
            return;
        }
        if (e->kind == EK::Cast && e->type->IsRef())
        {
            Address(e->a);
            return;
        }
        if (e->kind == EK::Function)
        {
            if (!e->fn)
                Error(e->loc, "overloaded function requires a call or explicit signature");
            Relocation r{};
            r.function = e->fn;
            Pointer(r);
            return;
        }
        if (e->kind == EK::Call && e->type->IsRef())
        {
            Call(e);
            return;
        }
        Error(e->loc, "expression has no address");
    }
    void RawCall(void *target)
    {
        if (x64)
        {
            Bytes({0x49, 0xbb});
            Qword((uintptr_t)target);
            Bytes({0x41, 0xff, 0xd3});
        }
        else
        {
            Byte(0xb8);
            Dword((uint32_t)(uintptr_t)target);
            Bytes({0xff, 0xd0});
        }
    }
    void Convert(Type *from, Type *to)
    {
        if (Kind(from) == Kind(to) || to->IsVoid())
            return;
        if (x64)
        {
            Bytes({0x48, 0x89, 0xc1});
            Byte(0xba);
            Dword((int)Kind(from));
            Bytes({0x41, 0xb8});
            Dword((int)Kind(to));
            size_t area = 32 + (depth % 16 ? 8 : 0);
            StackSub(area);
            RawCall((void *)ConvertValue);
            StackAdd(area);
        }
        else
        {
            Byte(0x68);
            Dword((int)Kind(to));
            Byte(0x68);
            Dword((int)Kind(from));
            Bytes({0x52, 0x50});
            depth += 16;
            RawCall((void *)ConvertValue);
            StackAdd(16);
        }
    }
    void Arithmetic(int operation, Type *type)
    {
        // The left operand is on the expression stack and the right is in EDX:EAX/RAX.
        if (x64)
        {
            Bytes({0x48, 0x89, 0xc2});
            Byte(0x59);
            depth -= 8;
            Bytes({0x41, 0xb8});
            Dword(operation);
            Bytes({0x41, 0xb9});
            Dword((int)Kind(type));
            size_t area = 32 + (depth % 16 ? 8 : 0);
            StackSub(area);
            RawCall((void *)Calculate);
            StackAdd(area);
        }
        else
        { // cdecl Calculate(uint64_t,uint64_t,int,int)
            Byte(0x68);
            Dword((int)Kind(type));
            Byte(0x68);
            Dword(operation);
            Bytes({0x52, 0x50});
            depth += 16;
            StackLoad(0, 16);
            StackLoad(2, 20);
            Bytes({0x52, 0x50});
            depth += 8;
            RawCall((void *)Calculate);
            StackAdd(32);
        }
    }
    void Test()
    {
        if (x64)
            Bytes({0x48, 0x85, 0xc0});
        else
            Bytes({0x09, 0xd0, 0x85, 0xc0});
    }
    Expr *Num(uint64_t v, Type *type)
    {
        auto e = u.arena.New<Expr>();
        e->type = type;
        e->value = v;
        return e;
    }
    Expr *As(Expr *value, Type *type, bool bits = false)
    {
        auto e = u.arena.New<Expr>();
        e->kind = EK::Cast;
        e->type = type;
        e->a = value;
        if (bits)
            e->op = "bits";
        return e;
    }
    Expr *Host(const std::string &name, Type *ret, std::vector<Expr *> args)
    {
        auto f = u.arena.New<FuncDecl>();
        f->qualified = f->link = name;
        FuncSig sig;
        sig.ret = ret;
        sig.conv = CallConv::Cdecl;
        for (auto a : args)
            sig.params.push_back(a->type);
        f->type = ty.Function(sig);
        auto callee = u.arena.New<Expr>();
        callee->kind = EK::Function;
        callee->fn = f;
        callee->type = f->type;
        auto e = u.arena.New<Expr>();
        e->kind = EK::Call;
        e->type = ret;
        e->a = callee;
        e->args = std::move(args);
        return e;
    }
    void Release(Type *type)
    {
        void *release = resolve(type->st->release);
        if (!release)
            Error(fn->loc, "missing hook release binding");
        if (x64)
        {
            Bytes({0x48, 0x89, 0xc1});
            size_t area = 32 + (depth % 16 ? 8 : 0);
            StackSub(area);
            RawCall(release);
            StackAdd(area);
        }
        else
        {
            Byte(0x50);
            depth += 4;
            RawCall(release);
            StackAdd(4);
        }
    }
    void Discard(Expr *e)
    {
        Value(e);
    }
    void HandleAssign(Expr *dest, Expr *value, Type *type)
    {
        Transfer(value);
        auto address = u.arena.New<Expr>();
        address->kind = EK::Unary;
        address->op = "&";
        address->a = dest;
        address->type = ty.Pointer(type);
        Expr *incoming = value;
        if (value->kind == EK::InitList)
        {
            if (!value->args.empty())
                Error(value->loc, "hook assignment requires a factory or {}");
            incoming = Num(0, type);
        }
        auto release = Num((uintptr_t)resolve(type->st->release), ty.Pointer(ty.Void()));
        Value(Host("cxxsnippets_assign", type, {address, incoming, release}));
    }
    void Intrinsic(Expr *e)
    {
        std::string op = e->a->fn->intrinsic;
        auto args = e->args;
        if (op == "write_memory")
        {
            Type *valueType = args[1]->type;
            auto owner = Num(0, ty.Pointer(ty.Void()));
            owner->op = "owner";
            Value(Host("cxxsnippets_write", ty.Void(),
                       {owner, As(args[0], ty.UIntPtr(), true), As(args[1], ty.ULongLong(), true),
                        Num(ty.SizeOf(valueType), ty.SizeT()), args[2]}));
            return;
        }
        if (op == "read_memory")
        {
            Value(Host("cxxsnippets_read", ty.ULongLong(),
                       {As(args[0], ty.UIntPtr(), true), Num(ty.SizeOf(e->type), ty.SizeT()), args[1]}));
            if (!e->type->IsFloating())
                Convert(ty.ULongLong(), e->type);
            return;
        }
        if (op.rfind("owned:", 0) == 0)
        {
            auto owner = Num(0, ty.Pointer(ty.Void()));
            owner->op = "owner";
            args.insert(args.begin(), owner);
            Value(Host(op.substr(6), e->type, args));
            return;
        }
        if (op == "pattern_each")
        {
            auto callback = args[1];
            auto ctype = callback->type;
            if (ctype->kind == TK::Closure)
                ctype = ty.Pointer(ctype->closure->type);
            Value(Host("cxxsnippets_pattern_each", ty.Void(), {args[0], As(callback, ctype, true)}));
            Value(callback);
            return;
        }
        if (op.rfind("original:", 0) == 0)
        {
            Value(Host(op.substr(9), e->type, args));
            return;
        }
        if (op.rfind("invoke:", 0) == 0)
        {
            auto receiver = args[0];
            if (!receiver->lvalue && temporarySlots.count(receiver))
            {
                Value(receiver);
                Push();
                FrameAddress(temporarySlots[receiver]);
                if (x64)
                    Bytes({0x48, 0x89, 0xc1});
                else
                    Bytes({0x89, 0xc1});
                if (x64)
                    Byte(0x58);
                else
                    Bytes({0x58, 0x5a});
                depth -= 8;
                Store(receiver->type);
                auto v = u.arena.New<VarDecl>();
                v->type = receiver->type;
                v->offset = temporarySlots[receiver];
                auto variable = u.arena.New<Expr>();
                variable->kind = EK::Variable;
                variable->type = v->type;
                variable->var = v;
                variable->lvalue = true;
                args[0] = variable;
            }
            std::string mode = op.substr(7);
            bool vm = mode.rfind("vm_", 0) == 0;
            if (vm)
                mode = mode.substr(3);
            bool unsafe = mode.rfind("unsafe_", 0) == 0;
            if (unsafe)
                mode = mode.substr(7);
            FuncSig sig;
            sig.ret = e->type;
            sig.conv = mode == "stdcall"    ? CallConv::Stdcall
                       : mode == "fastcall" ? CallConv::Fastcall
                       : mode == "thiscall" ? CallConv::Thiscall
                                            : CallConv::Cdecl;
            sig.conv = EffectiveConv(sig.conv, x64, false);
            std::vector<Expr *> callArgs(args.begin() + 1, args.end());
            for (auto a : callArgs)
                sig.params.push_back(a->type);
            auto target = Host(vm       ? "cxxsnippets_vm_original"
                               : unsafe ? "cxxsnippets_inline_original"
                                        : "cxxsnippets_inline_enter",
                               ty.Pointer(ty.Function(sig)), {args[0]});
            auto call = u.arena.New<Expr>();
            call->kind = EK::Call;
            call->a = target;
            call->type = e->type;
            call->args = std::move(callArgs);
            Value(call);
            if (!vm && !unsafe)
            {
                Push();
                Value(Host("cxxsnippets_inline_leave", ty.Void(), {args[0]}));
                if (x64)
                    Byte(0x58);
                else
                    Bytes({0x58, 0x5a});
                depth -= 8;
            }
            return;
        }
        Error(e->loc, "unknown intrinsic '" + op + "'");
    }
    void Call(Expr *e)
    {
        if (e->a->fn && !e->a->fn->intrinsic.empty())
        {
            Intrinsic(e);
            return;
        }
        auto ct = e->a->type;
        if (ct->kind == TK::Closure)
            ct = ct->closure->type;
        if (ct->IsPointer())
            ct = ct->base;
        auto &sig = ct->sig;
        size_t before = depth;
        Value(e->a);
        Push();
        for (auto i = e->args.rbegin(); i != e->args.rend(); ++i)
        {
            Value(*i);
            Push();
        }
        size_t n = e->args.size();
        if (x64)
        {
            size_t area = std::max<size_t>(32, n * 8);
            area += (16 - (depth + area) % 16) % 16;
            StackSub(area);
            for (size_t i = 0; i < n; ++i)
            {
                StackLoad(0, area + i * 8);
                StackStore(0, i * 8);
            }
            static const int regs[] = {1, 2, 8, 9};
            for (size_t i = 0; i < std::min<size_t>(4, n); ++i)
            {
                StackLoad(regs[i], i * 8);
                if (e->args[i]->type->IsFloating())
                {
                    Byte(0xf3);
                    Bytes({0x0f, 0x7e, (uint8_t)(0x84 | (i << 3)), 0x24});
                    Dword((uint32_t)(i * 8));
                }
            }
            StackLoad(11, area + n * 8);
            if (e->a->fn && e->a->fn->body)
            {
                Byte(0xe8);
                Relocation r{};
                r.offset = Here();
                r.function = e->a->fn;
                r.relative = true;
                Dword(0);
                image.relocations.push_back(r);
            }
            else
                Bytes({0x41, 0xff, 0xd3});
            StackAdd(area + (n + 1) * 8);
            if (sig.ret->IsFloating())
                Bytes({0x66, 0x48, 0x0f, 0x7e, 0xc0});
        }
        else
        {
            // Stage in eight-byte cells, then marshal the actual x86 argument widths.
            size_t pushed = 0;
            int regCount = 0;
            std::vector<int> registers(n, -1);
            auto cc = sig.conv;
            for (size_t i = 0; i < n; ++i)
            {
                if (cc == CallConv::Thiscall && i == 0)
                    registers[i] = 1;
                else if (cc == CallConv::Fastcall && regCount < 2 && !e->args[i]->type->IsFloating() &&
                         ty.SizeOf(e->args[i]->type) <= 4)
                    registers[i] = regCount++ == 0 ? 1 : 2;
            }
            for (size_t i = n; i-- > 0;)
            {
                if (registers[i] >= 0)
                    continue;
                size_t width = std::max<size_t>(4, ty.SizeOf(e->args[i]->type));
                if (width == 8)
                {
                    StackLoad(0, pushed + i * 8 + 4);
                    Byte(0x50);
                    pushed += 4;
                    depth += 4;
                }
                StackLoad(0, pushed + i * 8);
                Byte(0x50);
                pushed += 4;
                depth += 4;
            }
            for (size_t i = 0; i < n; ++i)
                if (registers[i] >= 0)
                    StackLoad(registers[i], pushed + i * 8, false);
            if (e->a->fn && e->a->fn->body)
            {
                Byte(0xe8);
                Relocation r{};
                r.offset = Here();
                r.function = e->a->fn;
                r.relative = true;
                Dword(0);
                image.relocations.push_back(r);
            }
            else
            {
                StackLoad(0, pushed + n * 8);
                Bytes({0xff, 0xd0});
            }
            if (cc == CallConv::Cdecl || cc == CallConv::Default)
                StackAdd(pushed);
            else
                depth -= pushed;
            StackAdd((n + 1) * 8);
            if (sig.ret->IsFloating())
            {
                StackSub(8);
                Bytes({0xdd, 0x1c, 0x24});
                StackLoad(0, 0);
                StackLoad(2, 4);
                StackAdd(8);
                if (sig.ret == ty.Float())
                    Convert(ty.Double(), ty.Float());
            }
            else if (ty.SizeOf(sig.ret) < 8)
            {
                if (ty.IsSigned(sig.ret))
                    Byte(0x99);
                else
                    Bytes({0x31, 0xd2});
            }
        }
        if (depth != before)
            Error(e->loc, "internal call stack imbalance");
        // Both ABIs define only AL for a bool result; host code may leave anything in the upper bits.
        if (sig.ret->IsBool())
            Bytes({0x0f, 0xb6, 0xc0}); // movzx eax, al
        if (!sig.ret->IsVoid() && !sig.ret->IsRef() && !sig.ret->IsFloating())
            Convert(ty.ULongLong(), sig.ret);
        auto temp = temporarySlots.find(e);
        if (temp != temporarySlots.end() && !e->transferred)
        {
            Push();
            FrameAddress(temp->second);
            if (x64)
                Bytes({0x48, 0x89, 0xc1});
            else
                Bytes({0x89, 0xc1});
            if (x64)
                Byte(0x58);
            else
                Bytes({0x58, 0x5a});
            depth -= 8;
            Store(e->type);
        }
    }
    void Value(Expr *e)
    {
        switch (e->kind)
        {
        case EK::Number:
            if (e->op == "owner")
            {
                Relocation r{};
                r.owner = true;
                Pointer(r);
            }
            else
                Imm(ConvertValue(e->value, (int)Kind(e->type), (int)Kind(e->type)));
            return;
        case EK::Variable:
        case EK::String:
        case EK::Member:
            Address(e);
            if (e->kind != EK::String)
                Load(e->type);
            return;
        case EK::Function:
            Address(e);
            return;
        case EK::Cast:
            if (e->type->IsRef())
            {
                Address(e->a);
                return;
            }
            Value(e->a);
            if (e->op != "bits")
                Convert(e->a->type, e->type);
            return;
        case EK::Call:
            Call(e);
            if (e->type->IsRef())
                Load(e->type->base);
            return;
        case EK::Conditional: {
            Value(e->a);
            Test();
            size_t no = Jump(4);
            Value(e->b);
            size_t done = Jump();
            Patch(no, Here());
            Value(e->c);
            Patch(done, Here());
            return;
        }
        case EK::Unary: {
            auto op = e->op;
            if (op == "&")
            {
                Address(e->a);
                return;
            }
            if (op == "*")
            {
                Address(e);
                Load(e->type);
                return;
            }
            if (op == "+")
            {
                Value(e->a);
                return;
            }
            if (op == "++" || op == "--" || op == "post++" || op == "post--")
            {
                Address(e->a);
                Push();
                Load(e->type);
                bool post = op.substr(0, 4) == "post";
                if (post)
                    Push();
                Push();
                uint64_t inc = e->type->IsPointer() ? ty.SizeOf(e->type->base) : 1;
                Imm(inc);
                if (e->type->IsFloating())
                    Convert(ty.ULongLong(), e->type);
                Arithmetic(op.find("++") != std::string::npos ? 0 : 1, e->type);
                if (post)
                {
                    if (x64)
                        StackLoad(1, 8);
                    else
                        StackLoad(1, 8);
                    Store(e->type);
                    if (x64)
                        Byte(0x58);
                    else
                        Bytes({0x58, 0x5a});
                    depth -= 8;
                    StackAdd(8);
                }
                else
                {
                    PopAddress();
                    Store(e->type);
                }
                return;
            }
            if (op == "-")
            {
                if (e->type->IsFloating())
                {
                    Value(e->a);
                    Push();
                    Imm(e->type->kind == TK::Float ? 0x80000000ULL : 0x8000000000000000ULL);
                    Arithmetic(9, ty.ULongLong());
                }
                else
                {
                    Imm(0);
                    Push();
                    Value(e->a);
                    Arithmetic(1, e->type);
                }
                return;
            }
            Value(e->a);
            if (op == "!")
            {
                Test();
                Bytes({0x0f, 0x94, 0xc0, 0x0f, 0xb6, 0xc0});
                if (!x64)
                    Bytes({0x31, 0xd2});
            }
            else if (op == "~")
            {
                if (x64)
                    Byte(0x48);
                Bytes({0xf7, 0xd0});
                if (!x64)
                    Bytes({0xf7, 0xd2});
                Convert(ty.ULongLong(), e->type);
            }
            return;
        }
        case EK::Binary: {
            if (e->c)
            {
                Address(e->a);
                Push();
                Load(e->type);
                Convert(e->type, e->c->a->type);
                Push();
                Value(e->b);
                if (e->c->offset)
                {
                    Push();
                    Imm(e->c->offset);
                    Arithmetic(2, ty.PtrDiffT());
                }
                static const std::vector<std::string> ops = {"+", "-", "*", "/", "%", "<<", ">>", "&", "|", "^"};
                auto i = std::find(ops.begin(), ops.end(), e->c->op);
                Arithmetic((int)(i - ops.begin()), e->c->a->type);
                Convert(e->c->type, e->type);
                PopAddress();
                Store(e->type);
                return;
            }
            if (e->op == ",")
            {
                Discard(e->a);
                Value(e->b);
                return;
            }
            if (e->op == "=")
            {
                if (e->type->IsStruct() && e->type->st->handle)
                {
                    HandleAssign(e->a, e->b, e->type);
                    return;
                }
                if (e->type->IsStruct() && !e->type->st->value)
                {
                    CopyAggregate(e->a, e->b, e->type);
                    return;
                }
                Address(e->a);
                Push();
                Value(e->b);
                PopAddress();
                Store(e->type);
                return;
            }
            if (e->op == "&&" || e->op == "||")
            {
                Value(e->a);
                Test();
                size_t done = Jump(e->op == "&&" ? 4 : 5);
                Value(e->b);
                Patch(done, Here());
                return;
            }
            Value(e->a);
            Push();
            Value(e->b);
            if (e->offset && e->op != "ptrdiff")
            {
                Push();
                Imm(e->offset);
                Arithmetic(2, ty.PtrDiffT());
            }
            static const std::vector<std::string> ops = {"+", "-", "*",  "/",  "%", "<<", ">>", "&",
                                                         "|", "^", "==", "!=", "<", ">",  "<=", ">="};
            auto i = std::find(ops.begin(), ops.end(), e->op);
            int op = e->op == "ptrdiff" ? 1 : (int)(i - ops.begin());
            if (op > 15)
                Error(e->loc, "unsupported binary operator");
            Arithmetic(op, e->a->type);
            if (e->op == "ptrdiff")
            {
                Push();
                Imm(e->offset);
                Arithmetic(3, ty.PtrDiffT());
            }
            return;
        }
        case EK::InitList:
            Error(e->loc, "initializer list is not a scalar expression");
        }
    }
    void Init(VarDecl *var, Expr *init, Type *type, size_t offset = 0)
    {
        if (type->IsStruct() && (type->st->handle || type->st->value))
        {
            if (!init)
                return;
            if (init->kind == EK::InitList)
            {
                if (!init->args.empty())
                    Error(init->loc, "host object requires a factory initializer");
                Expr zero;
                zero.type = type;
                InitScalar(var, &zero, type, offset);
                return;
            }
            InitScalar(var, init, type, offset);
            return;
        }
        if (!init && !type->IsStruct())
            return;
        if (type->IsArray())
        {
            if (init && init->kind == EK::String)
            {
                if (init->bytes.size() > ty.SizeOf(type))
                    Error(init->loc, "string initializer exceeds array bound");
                for (size_t i = 0; i < init->bytes.size(); ++i)
                {
                    Expr num;
                    num.type = ty.Basic(TK::UChar);
                    num.value = (uint8_t)init->bytes[i];
                    Init(var, &num, num.type, offset + i);
                }
                return;
            }
            if (init && init->kind != EK::InitList)
                Error(init->loc, "array initializer must be a list");
            if (init && init->args.size() > type->count)
                Error(init->loc, "too many array initializers");
            for (size_t i = 0; i < type->count; ++i)
            {
                Expr zero;
                zero.type = type->base;
                Init(var, init && i < init->args.size() ? init->args[i] : &zero, type->base,
                     offset + i * ty.SizeOf(type->base));
            }
            return;
        }
        if (type->IsStruct())
        {
            if (init && init->kind != EK::InitList)
            {
                if (init->type != type)
                    Error(init->loc, "incompatible aggregate initializer");
                Expr ve;
                ve.kind = EK::Variable;
                ve.var = var;
                ve.type = var->type;
                Expr member;
                member.kind = EK::Member;
                member.a = &ve;
                member.type = type;
                member.offset = offset;
                CopyAggregate(&member, init, type);
                return;
            }
            if (init && init->args.size() > type->st->fields.size())
                Error(init->loc, "too many struct initializers");
            size_t i = 0;
            for (auto &f : type->st->fields)
            {
                Expr zero;
                zero.type = f.type;
                Expr *value = init && i < init->args.size() ? init->args[i] : f.init ? f.init : &zero;
                Init(var, value, f.type, offset + f.offset);
                ++i;
                if (type->st->isUnion)
                    break;
            }
            return;
        }
        InitScalar(var, init, type, offset);
    }
    void InitScalar(VarDecl *var, Expr *init, Type *type, size_t offset)
    {
        if (var->global)
        {
            Relocation r{};
            r.variable = var;
            Pointer(r);
        }
        else
            FrameAddress(var->offset);
        if (offset)
        {
            if (x64)
                Byte(0x48);
            Byte(0x05);
            Dword((uint32_t)offset);
        }
        Push();
        if (type->IsRef())
            Address(init->kind == EK::Cast ? init->a : init);
        else
        {
            Value(init);
            Convert(init->type, type);
        }
        PopAddress();
        Store(type);
    }
    void CopyAggregate(Expr *dest, Expr *source, Type *type)
    {
        auto address = u.arena.New<Expr>();
        address->kind = EK::Unary;
        address->op = "&";
        address->a = dest;
        address->type = ty.Pointer(type);
        Value(Host("memcpy", ty.Pointer(ty.Void()),
                   {address, As(source, ty.Pointer(ty.Void()), true), Num(ty.SizeOf(type), ty.SizeT())}));
    }
    void CollectCases(Stmt *s, std::vector<Stmt *> &out)
    {
        if (!s)
            return;
        if (s->kind == SK::Switch)
            return;
        if (s->kind == SK::Case)
            out.push_back(s);
        CollectCases(s->a, out);
        CollectCases(s->b, out);
        for (auto ch : s->body)
            CollectCases(ch, out);
    }
    void Statement(Stmt *s)
    {
        if (!s)
            return;
        if (s->kind != SK::Block && s->loc.line > 0)
            image.lines.push_back({Here(), s->loc});
        size_t initial = depth;
        switch (s->kind)
        {
        case SK::Empty:
            break;
        case SK::Block:
            for (auto st : s->body)
                Statement(st);
            break;
        case SK::Expr:
            Discard(s->expr);
            break;
        case SK::Declaration:
            if (s->var->staticLocal)
            {
                auto owner = Num(0, ty.Pointer(ty.Void()));
                owner->op = "owner";
                Value(Host("cxxsnippets_static_enter", ty.Bool(), {owner, Num(s->var->offset, ty.SizeT())}));
                Test();
                size_t skip = Jump(4);
                Init(s->var, s->var->init, s->var->type);
                Value(Host("cxxsnippets_static_leave", ty.Void(), {owner, Num(s->var->offset, ty.SizeT())}));
                Patch(skip, Here());
            }
            else
                Init(s->var, s->var->init, s->var->type);
            break;
        case SK::Return:
            if (s->expr)
            {
                if (fn->type->sig.ret->IsRef())
                    Address(s->expr->kind == EK::Cast ? s->expr->a : s->expr);
                else
                    Value(s->expr);
            }
            CleanupTemps();
            returns.push_back(Jump());
            break;
        case SK::If: {
            Value(s->condition);
            CleanupTemps();
            Test();
            size_t no = Jump(4);
            Statement(s->a);
            if (s->b)
            {
                size_t done = Jump();
                Patch(no, Here());
                Statement(s->b);
                Patch(done, Here());
            }
            else
                Patch(no, Here());
            break;
        }
        case SK::While:
        case SK::Do:
        case SK::For: {
            if (s->kind == SK::For)
                Statement(s->a);
            size_t top = Here(), done = 0;
            if (s->kind != SK::Do && s->condition)
            {
                Value(s->condition);
                CleanupTemps();
                Test();
                done = Jump(4);
            }
            flow.push_back({{}, {}, true});
            Statement(s->kind == SK::For ? s->b : s->a);
            size_t next = Here();
            for (auto at : flow.back().continues)
                Patch(at, next);
            if (s->step)
            {
                Value(s->step);
                CleanupTemps();
            }
            if (s->kind == SK::Do)
            {
                Value(s->condition);
                CleanupTemps();
                Test();
                Patch(Jump(5), top);
            }
            else
                Back(top);
            if (done)
                Patch(done, Here());
            for (auto at : flow.back().breaks)
                Patch(at, Here());
            flow.pop_back();
            break;
        }
        case SK::Break:
            flow.back().breaks.push_back(Jump());
            break;
        case SK::Continue: {
            auto i = flow.rbegin();
            while (!i->loop)
                ++i;
            i->continues.push_back(Jump());
            break;
        }
        case SK::Switch: {
            std::vector<Stmt *> labels;
            CollectCases(s->a, labels);
            std::map<uint64_t, bool> values;
            Stmt *def = nullptr;
            std::vector<std::pair<Stmt *, size_t>> branches;
            Value(s->expr);
            CleanupTemps();
            Push();
            for (auto label : labels)
            {
                if (label->isDefault)
                {
                    if (def)
                        Error(label->loc, "duplicate default label");
                    def = label;
                    continue;
                }
                if (values.count(label->value))
                    Error(label->loc, "duplicate case label");
                values[label->value] = true;
                if (x64)
                    StackLoad(0, 0);
                else
                {
                    StackLoad(0, 0);
                    StackLoad(2, 4);
                }
                Push();
                Imm(label->value);
                Arithmetic(10, s->expr->type);
                Test();
                branches.push_back({label, Jump(5)});
            }
            StackAdd(8);
            size_t otherwise = Jump();
            // Drop the saved switch operand before entering the matched body.
            for (auto &b : branches)
            {
                Patch(b.second, Here());
                depth += 8;
                StackAdd(8);
                b.second = Jump();
            }
            flow.push_back({{}, {}, false});
            Statement(s->a);
            size_t end = Here();
            for (auto &b : branches)
                Patch(b.second, cases[b.first]);
            Patch(otherwise, def ? cases[def] : end);
            for (auto at : flow.back().breaks)
                Patch(at, end);
            flow.pop_back();
            break;
        }
        case SK::Case:
            cases[s] = Here();
            Statement(s->a);
            break;
        case SK::Label:
            if (labels.count(s->label))
                Error(s->loc, "duplicate label");
            labels[s->label] = {s, Here()};
            Statement(s->a);
            break;
        case SK::Goto:
            gotos.push_back({s, Jump()});
            break;
        }
        if (s->kind == SK::Expr || s->kind == SK::Declaration)
            CleanupTemps();
        if (depth != initial)
            Error(s->loc, "internal expression stack imbalance");
    }
    void Function(FuncDecl *f)
    {
        fn = f;
        depth = 0;
        returns.clear();
        labels.clear();
        gotos.clear();
        temporarySlots.clear();
        size_t offset = 0;
        for (auto v : f->params)
        {
            offset = (offset + 7) / 8 * 8 + std::max<size_t>(8, ty.SizeOf(v->type));
            v->offset = offset;
        }
        for (auto v : f->locals)
        {
            offset = (offset + ty.AlignOf(v->type) - 1) / ty.AlignOf(v->type) * ty.AlignOf(v->type);
            offset += std::max<size_t>(8, ty.SizeOf(v->type));
            v->offset = offset;
        }
        if (f == u.initializer)
        {
            for (auto v : u.globals)
            {
                if (v->type->IsStruct() && v->type->st->handle)
                    Transfer(v->init);
                FindTemps(v->init, offset);
            }
        }
        else
            FindTemps(f->body, offset);
        size_t registerSave = offset + 8;
        f->stackSize = (offset + (x64 ? 0 : 16) + 15) / 16 * 16;
        f->codeOffset = Here();
        if (f->loc.line > 0)
            image.lines.push_back({Here(), f->loc});
        Byte(0x55);
        if (x64)
            Bytes({0x48, 0x89, 0xe5});
        else
            Bytes({0x89, 0xe5});
        if (f->stackSize > UINT32_MAX)
            Error(f->loc, "function stack frame is too large");
        if (f->stackSize >= 4096)
        {
            if (x64)
            {
                Bytes({0x49, 0x89, 0xe3, 0xb8});
                Dword((uint32_t)((f->stackSize + 4095) / 4096));
                size_t probe = Here();
                Bytes({0x49, 0x81, 0xeb});
                Dword(4096);
                Bytes({0x41, 0xf6, 0x03, 0x00, 0xff, 0xc8});
                Patch(Jump(5), probe);
            }
            else
            {
                Bytes({0x51, 0x8b, 0xc4, 0xb9});
                Dword((uint32_t)((f->stackSize + 4095) / 4096));
                size_t probe = Here();
                Byte(0x2d);
                Dword(4096);
                Bytes({0xf6, 0x00, 0x00, 0xff, 0xc9});
                Patch(Jump(5), probe);
                Byte(0x59);
            }
        }
        if (f->stackSize)
        {
            if (x64)
                Byte(0x48);
            Bytes({0x81, 0xec});
            Dword((uint32_t)f->stackSize);
            f->allocationOffset = (uint8_t)(Here() - f->codeOffset);
        }
        f->prologueSize = (uint8_t)(Here() - f->codeOffset);
        if (!x64)
        {
            Bytes({0x89, 0x8d});
            Dword((uint32_t)-(int32_t)registerSave);
            Bytes({0x89, 0x95});
            Dword((uint32_t)-(int32_t)(registerSave + 4));
        }
        for (auto &temp : temporarySlots)
        {
            Bytes({0xc7, 0x85});
            Dword((uint32_t)-(int32_t)temp.second);
            Dword(0);
            if (x64)
            {
                Bytes({0xc7, 0x85});
                Dword((uint32_t)-(int32_t)(temp.second - 4));
                Dword(0);
            }
        }
        size_t incoming = 8;
        int regCount = 0;
        for (size_t i = 0; i < f->params.size(); ++i)
        {
            auto v = f->params[i];
            if (x64)
            {
                if (i < 4)
                {
                    if (v->type->IsFloating())
                    {
                        Bytes({0x66, 0x48, 0x0f, 0x7e, (uint8_t)(0xc0 | (i << 3))});
                    }
                    else
                    {
                        static const int regs[] = {1, 2, 8, 9};
                        int reg = regs[i];
                        Byte(reg >= 8 ? 0x4c : 0x48);
                        Bytes({0x89, (uint8_t)(0xc0 | ((reg & 7) << 3))});
                    }
                }
                else
                {
                    Bytes({0x48, 0x8b, 0x85});
                    Dword((uint32_t)(16 + i * 8));
                }
            }
            else
            {
                auto cc = f->type->sig.conv;
                int reg = -1;
                if (cc == CallConv::Thiscall && i == 0)
                    reg = 1;
                else if (cc == CallConv::Fastcall && regCount < 2 && !v->type->IsFloating() && ty.SizeOf(v->type) <= 4)
                    reg = regCount++ == 0 ? 1 : 2;
                if (reg >= 0)
                {
                    Bytes({0x8b, 0x85});
                    Dword((uint32_t)-(int32_t)(registerSave + (reg == 2 ? 4 : 0)));
                }
                else
                {
                    Bytes({0x8b, 0x85});
                    Dword((uint32_t)incoming);
                    if (ty.SizeOf(v->type) == 8)
                    {
                        Bytes({0x8b, 0x95});
                        Dword((uint32_t)(incoming + 4));
                    }
                    incoming += std::max<size_t>(4, ty.SizeOf(v->type));
                }
                if (ty.SizeOf(v->type) < 8)
                    Bytes({0x31, 0xd2});
            }
            Push();
            FrameAddress(v->offset);
            if (x64)
                Bytes({0x48, 0x89, 0xc1});
            else
                Bytes({0x89, 0xc1});
            if (x64)
                Byte(0x58);
            else
                Bytes({0x58, 0x5a});
            depth -= 8;
            Store(v->type);
        }
        if (f == u.initializer)
        {
            for (auto v : u.globals)
                if (!v->external && !v->staticLocal)
                {
                    if (v->loc.line > 0)
                        image.lines.push_back({Here(), v->loc});
                    Init(v, v->init, v->type);
                    CleanupTemps();
                }
        }
        else
            Statement(f->body);
        for (auto &jump : gotos)
        {
            auto it = labels.find(jump.first->label);
            if (it == labels.end())
                Error(jump.first->loc, "undefined label '" + jump.first->label + "'");
            for (auto v : it->second.first->live)
                if (v->init && std::find(jump.first->live.begin(), jump.first->live.end(), v) == jump.first->live.end())
                    Error(jump.first->loc, "goto bypasses variable initialization");
            Patch(jump.second, it->second.second);
        }
        Imm(0);
        for (auto at : returns)
            Patch(at, Here());
        auto ret = f->type->sig.ret;
        if (ret->IsFloating())
        {
            if (x64)
                Bytes({0x66, 0x48, 0x0f, 0x6e, 0xc0});
            else
            {
                Push();
                Bytes({ret == ty.Float() ? uint8_t(0xd9) : uint8_t(0xdd), 0x04, 0x24});
                StackAdd(8);
            }
        }
        if (x64)
            Bytes({0x48, 0x89, 0xec});
        else
            Bytes({0x89, 0xec});
        Byte(0x5d);
        if (!x64 && f->type->sig.conv != CallConv::Cdecl && f->type->sig.conv != CallConv::Default)
        {
            Byte(0xc2);
            uint16_t cleanup = (uint16_t)(incoming - 8);
            Byte((uint8_t)cleanup);
            Byte((uint8_t)(cleanup >> 8));
        }
        else
            Byte(0xc3);
        f->codeEnd = Here();
    }

  public:
    Generator(TranslationUnit &unit, std::function<void *(const std::string &)> resolver)
        : u(unit), ty(unit.types), resolve(std::move(resolver)), x64(ty.target.x64)
    {
    }
    Image Run()
    {
        for (auto v : u.globals)
        {
            if (v->external)
                continue;
            size_t align = ty.AlignOf(v->type);
            while (image.data.size() % align)
                image.data.push_back(0);
            v->offset = image.data.size();
            image.data.resize(image.data.size() + ty.SizeOf(v->type));
        }
        for (auto f : u.functions)
            if (f->body)
                Function(f);
        auto init = u.arena.New<FuncDecl>();
        init->qualified = "$initialize";
        init->type = ty.Function({ty.Void(), {}, false, CallConv::Cdecl});
        init->internal = true;
        u.initializer = init;
        u.functions.push_back(init);
        Function(init);
        for (auto &r : image.relocations)
        {
            if (r.function && !r.function->body && r.function != init)
            {
                r.absolute = resolve(r.function->link.empty() ? r.function->qualified : r.function->link);
                if (!r.absolute)
                    Error(r.function->loc, "unresolved symbol '" + r.function->qualified + "'");
                r.function = nullptr;
            }
            if (r.variable && r.variable->external)
            {
                r.absolute = resolve(r.variable->qualified);
                if (!r.absolute)
                    Error(r.variable->loc, "unresolved symbol '" + r.variable->qualified + "'");
                r.variable = nullptr;
            }
        }
        return std::move(image);
    }
};
} // namespace
Image Generate(TranslationUnit &unit, const std::function<void *(const std::string &)> &resolve)
{
    return Generator(unit, resolve).Run();
}
} // namespace cxxsnippets
