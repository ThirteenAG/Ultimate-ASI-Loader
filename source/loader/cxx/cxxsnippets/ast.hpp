#pragma once
#include "types.hpp"
#include <map>

namespace cxxsnippets
{
struct VarDecl;
struct Stmt;
struct TemplateDecl;
struct Namespace
{
    std::string name;
    Namespace *parent = nullptr;
    std::map<std::string, Namespace *> children;
    std::map<std::string, Type *> types;
    std::map<std::string, VarDecl *> variables;
    std::map<std::string, std::vector<FuncDecl *>> functions;
    std::map<std::string, TemplateDecl *> templates;
    std::vector<Namespace *> imports;
};
enum class EK
{
    Number,
    String,
    Variable,
    Function,
    Unary,
    Binary,
    Cast,
    Call,
    Conditional,
    Member,
    InitList
};
struct Expr
{
    EK kind = EK::Number;
    Type *type = nullptr;
    Loc loc;
    std::string op, bytes;
    uint64_t value = 0;
    VarDecl *var = nullptr;
    FuncDecl *fn = nullptr;
    Expr *a = nullptr, *b = nullptr, *c = nullptr;
    std::vector<Expr *> args;
    std::vector<FuncDecl *> overloads;
    TemplateDecl *templ = nullptr;
    std::vector<Type *> templateArgs;
    size_t offset = 0;
    bool lvalue = false;
    bool transferred = false;
};
enum class SK
{
    Block,
    Expr,
    Declaration,
    Return,
    If,
    While,
    Do,
    For,
    Break,
    Continue,
    Switch,
    Case,
    Empty,
    Label,
    Goto
};
struct Stmt
{
    SK kind = SK::Empty;
    Loc loc;
    Expr *expr = nullptr, *condition = nullptr, *step = nullptr;
    Stmt *a = nullptr, *b = nullptr;
    VarDecl *var = nullptr;
    std::vector<Stmt *> body;
    uint64_t value = 0;
    bool isDefault = false;
    std::string label;
    std::vector<VarDecl *> live;
};
struct VarDecl
{
    std::string name, qualified;
    Type *type = nullptr;
    Expr *init = nullptr;
    Loc loc;
    bool global = false, external = false, internal = false, constant = false, parameter = false;
    size_t offset = 0;
    uint64_t constantValue = 0;
    bool hasConstant = false;
    bool enumerator = false;
    bool staticLocal = false;
};
struct FuncDecl
{
    std::string name, qualified, link, intrinsic;
    Type *type = nullptr;
    Loc loc;
    std::vector<VarDecl *> params, locals;
    std::vector<Expr *> defaults;
    Stmt *body = nullptr;
    bool internal = false;
    bool constexprFunction = false;
    size_t codeOffset = 0, codeEnd = 0;
    size_t stackSize = 0;
    uint8_t prologueSize = 0, allocationOffset = 0;
};
struct TranslationUnit
{
    Arena arena;
    TypeTable types;
    Namespace root;
    std::vector<FuncDecl *> functions;
    std::vector<VarDecl *> globals;
    FuncDecl *initializer = nullptr;
    explicit TranslationUnit(Target target) : types(target) {}
};
bool Constant(Expr *e, uint64_t &result);
bool ConstantCall(Expr *e, uint64_t &result);
} // namespace cxxsnippets
