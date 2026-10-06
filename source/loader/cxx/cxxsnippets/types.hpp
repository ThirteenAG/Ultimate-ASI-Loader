#pragma once
#include "common.hpp"
#include <map>
#include <tuple>

namespace cxxsnippets
{
enum class TK
{
    Void,
    Bool,
    Char, // char (signed on MSVC)
    SChar,
    UChar,
    WChar, // wchar_t (unsigned 16-bit)
    Char16,
    Char32,
    Short,
    UShort,
    Int,
    UInt,
    Long, // 32-bit on Windows
    ULong,
    LongLong,
    ULongLong,
    Float,
    Double,
    LongDouble, // same as double on MSVC
    Nullptr,
    Pointer,
    Ref,
    Array,
    Function,
    Struct,
    Enum,
    Closure,   // type of a lambda expression
    MemberFn,  // &Class::method
    AnyAddress // [[cxxsnippets::address]] parameter type: any pointer, integer, function or closure
};

enum class CallConv
{
    Default, // cdecl for free functions, thiscall for member functions (x86)
    Cdecl,
    Stdcall,
    Fastcall,
    Thiscall,
};

struct Type;
struct StructInfo;
struct EnumInfo;
struct FuncDecl;

struct FuncSig
{
    Type *ret = nullptr;
    std::vector<Type *> params;
    bool variadic = false;
    CallConv conv = CallConv::Default;
};

struct Type
{
    TK kind;
    Type *base = nullptr; // pointee / referee / element / underlying
    size_t count = 0;     // array length (0 = unknown bound)
    FuncSig sig;          // Function
    StructInfo *st = nullptr;
    EnumInfo *en = nullptr;
    FuncDecl *closure = nullptr; // Closure: the lambda's function
    bool isConst = false;

    bool IsVoid() const
    {
        return kind == TK::Void;
    }
    bool IsBool() const
    {
        return kind == TK::Bool;
    }
    bool IsInteger() const
    {
        return kind >= TK::Bool && kind <= TK::ULongLong;
    } // includes bool and chars
    bool IsFloating() const
    {
        return kind == TK::Float || kind == TK::Double || kind == TK::LongDouble;
    }
    bool IsArithmetic() const
    {
        return IsInteger() || IsFloating();
    }
    bool IsEnum() const
    {
        return kind == TK::Enum;
    }
    bool IsPointer() const
    {
        return kind == TK::Pointer;
    }
    bool IsRef() const
    {
        return kind == TK::Ref;
    }
    bool IsArray() const
    {
        return kind == TK::Array;
    }
    bool IsFunction() const
    {
        return kind == TK::Function;
    }
    bool IsStruct() const
    {
        return kind == TK::Struct;
    }
    bool IsFuncPtr() const
    {
        return kind == TK::Pointer && base->kind == TK::Function;
    }
    bool IsScalar() const
    {
        return IsArithmetic() || IsEnum() || IsPointer() || kind == TK::Nullptr;
    }
};

struct FieldInfo
{
    std::string name;
    Type *type;
    size_t offset;
    struct Expr *init = nullptr; // default member initializer
    Loc loc;
};

struct StructInfo
{
    std::string name; // qualified
    bool complete = false;
    bool isUnion = false;
    size_t size = 0;
    size_t align = 1;
    std::vector<FieldInfo> fields;
    std::vector<FuncDecl *> methods;
    std::vector<FuncDecl *> constructors; // only for built-in handle/value classes
    FuncDecl *boolOp = nullptr;           // explicit operator bool
    struct Namespace *scope = nullptr;    // member names
    // built-in classes bound to host objects (pointer sized)
    bool handle = false; // owns a host object: release function called on reassign/discard/unload
    bool value = false;  // trivially copyable host value (pattern results)
    std::string release; // host symbol of the release function (handle)
};

struct EnumInfo
{
    std::string name;
    Type *underlying;
    bool scoped = false;
};

// Creates and interns types. Types are compared by pointer.
class TypeTable
{
  public:
    explicit TypeTable(Target t);

    Target target;
    Type *Basic(TK k)
    {
        return basics[(int)k];
    }
    Type *Void()
    {
        return Basic(TK::Void);
    }
    Type *Bool()
    {
        return Basic(TK::Bool);
    }
    Type *Int()
    {
        return Basic(TK::Int);
    }
    Type *UInt()
    {
        return Basic(TK::UInt);
    }
    Type *LongLong()
    {
        return Basic(TK::LongLong);
    }
    Type *ULongLong()
    {
        return Basic(TK::ULongLong);
    }
    Type *Double()
    {
        return Basic(TK::Double);
    }
    Type *Float()
    {
        return Basic(TK::Float);
    }
    Type *Char()
    {
        return Basic(TK::Char);
    }
    Type *SizeT()
    {
        return target.x64 ? ULongLong() : UInt();
    }
    Type *PtrDiffT()
    {
        return target.x64 ? LongLong() : Int();
    }
    Type *UIntPtr()
    {
        return SizeT();
    }

    Type *Pointer(Type *to);
    Type *Ref(Type *to);
    Type *Array(Type *elem, size_t count);
    Type *Function(const FuncSig &sig);
    Type *NewStruct(StructInfo *info);
    Type *NewEnum(EnumInfo *info);
    Type *Closure(FuncDecl *fn);
    Type *MemberFn(FuncDecl *fn);
    Type *AnyAddress()
    {
        return Basic(TK::AnyAddress);
    }
    Type *Const(Type *type);
    Type *Unqualified(Type *type);

    size_t SizeOf(const Type *t) const;
    size_t AlignOf(const Type *t) const;
    bool IsSigned(const Type *t) const;

    static std::string Name(const Type *t);

  private:
    Arena arena;
    std::vector<Type *> basics;
    std::map<std::pair<int, Type *>, Type *> derived;
    std::map<std::pair<Type *, size_t>, Type *> arrays;
    std::vector<Type *> functions;
    std::map<FuncDecl *, Type *> closures, memberFns;
    std::map<Type *, Type *> constTypes, unqualified;
};

// canonical calling convention of a function type on the target
CallConv EffectiveConv(CallConv c, bool x64, bool member);
} // namespace cxxsnippets
