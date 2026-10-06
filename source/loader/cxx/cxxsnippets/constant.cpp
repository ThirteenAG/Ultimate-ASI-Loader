#include "ast.hpp"
#include "codegen.hpp"
#include <algorithm>
#include <map>
#include <functional>

namespace cxxsnippets
{
namespace
{
struct NotConstant
{
};
enum class Flow
{
    Normal,
    Return,
    Break,
    Continue
};
class Evaluator
{
    size_t steps = 0, depth = 0;
    std::vector<std::map<VarDecl *, uint64_t>> frames;
    static int Kind(Type *t)
    {
        return (int)(t->IsEnum() ? t->base->kind : t->kind);
    }
    void Step()
    {
        if (++steps > 100000)
            throw NotConstant{};
    }
    uint64_t &Variable(Expr *e)
    {
        if (e->kind != EK::Variable || frames.empty())
            throw NotConstant{};
        auto it = frames.back().find(e->var);
        if (it == frames.back().end())
            throw NotConstant{};
        return it->second;
    }
    static int Operation(const std::string &op)
    {
        static const std::vector<std::string> ops = {"+", "-",  "*",  "/", "%", "<<", ">>", "&",  "|",
                                                     "^", "==", "!=", "<", ">", "<=", ">=", "&&", "||"};
        auto i = std::find(ops.begin(), ops.end(), op);
        if (i == ops.end())
            throw NotConstant{};
        return (int)(i - ops.begin());
    }
    uint64_t ExprValue(Expr *e)
    {
        Step();
        if (!e)
            throw NotConstant{};
        switch (e->kind)
        {
        case EK::Number:
            return ConvertValue(e->value, Kind(e->type), Kind(e->type));
        case EK::Variable:
            if (!frames.empty())
            {
                auto it = frames.back().find(e->var);
                if (it != frames.back().end())
                    return it->second;
            }
            if (e->var->hasConstant)
                return e->var->constantValue;
            throw NotConstant{};
        case EK::Cast:
            if (e->type->IsRef())
                throw NotConstant{};
            return ConvertValue(ExprValue(e->a), Kind(e->a->type), Kind(e->type));
        case EK::Conditional:
            return ExprValue(e->a) ? ExprValue(e->b) : ExprValue(e->c);
        case EK::Call:
            return Call(e);
        case EK::Unary: {
            auto a = ExprValue(e->a);
            auto op = e->op;
            if (op == "+")
                return a;
            if (op == "!")
                return !a;
            if (op == "~")
                return ConvertValue(~a, Kind(e->type), Kind(e->type));
            if (op == "-")
                return e->type->IsFloating() ? a ^ (e->type->kind == TK::Float ? 0x80000000ULL : 0x8000000000000000ULL)
                                             : Calculate(0, a, 1, Kind(e->type));
            if (op == "++" || op == "--" || op == "post++" || op == "post--")
            {
                auto &v = Variable(e->a);
                v = Calculate(v, 1, op.find("++") != std::string::npos ? 0 : 1, Kind(e->a->type));
                return op.rfind("post", 0) == 0 ? a : v;
            }
            throw NotConstant{};
        }
        case EK::Binary: {
            if (e->op == "=")
            {
                auto b = ExprValue(e->b);
                Variable(e->a) = b;
                return b;
            }
            if (e->op == ",")
            {
                ExprValue(e->a);
                return ExprValue(e->b);
            }
            if (e->c)
            {
                auto &v = Variable(e->a);
                auto a = ConvertValue(v, Kind(e->a->type), Kind(e->c->a->type));
                auto b = ExprValue(e->b);
                if ((e->c->op == "/" || e->c->op == "%") && !b)
                    throw NotConstant{};
                v = ConvertValue(Calculate(a, b, Operation(e->c->op), Kind(e->c->a->type)), Kind(e->c->type),
                                 Kind(e->type));
                return v;
            }
            auto a = ExprValue(e->a);
            if (e->op == "&&" && !a)
                return 0;
            if (e->op == "||" && a)
                return 1;
            auto b = ExprValue(e->b);
            if ((e->op == "/" || e->op == "%") && !b)
                throw NotConstant{};
            return Calculate(a, b, Operation(e->op), Kind(e->a->type));
        }
        default:
            throw NotConstant{};
        }
    }
    Flow Statement(Stmt *s, uint64_t &result)
    {
        if (!s)
            return Flow::Normal;
        Step();
        switch (s->kind)
        {
        case SK::Empty:
            return Flow::Normal;
        case SK::Expr:
            ExprValue(s->expr);
            return Flow::Normal;
        case SK::Declaration:
            if (s->var->global || !s->var->type->IsScalar() || !s->var->init)
                throw NotConstant{};
            frames.back()[s->var] = ExprValue(s->var->init);
            return Flow::Normal;
        case SK::Block:
            for (auto c : s->body)
            {
                auto flow = Statement(c, result);
                if (flow != Flow::Normal)
                    return flow;
            }
            return Flow::Normal;
        case SK::Return:
            result = s->expr ? ExprValue(s->expr) : 0;
            return Flow::Return;
        case SK::If:
            return Statement(ExprValue(s->condition) ? s->a : s->b, result);
        case SK::Break:
            return Flow::Break;
        case SK::Continue:
            return Flow::Continue;
        case SK::For:
        case SK::While:
        case SK::Do: {
            if (s->kind == SK::For)
                Statement(s->a, result);
            bool first = true;
            while ((first && s->kind == SK::Do) || !s->condition || ExprValue(s->condition))
            {
                first = false;
                auto flow = Statement(s->kind == SK::For ? s->b : s->a, result);
                if (flow == Flow::Return)
                    return flow;
                if (flow == Flow::Break)
                    break;
                if (s->step)
                    ExprValue(s->step);
                Step();
            }
            return Flow::Normal;
        }
        case SK::Switch: {
            std::vector<Stmt *> rows;
            std::function<void(Stmt *)> flatten = [&](Stmt *node) {
                if (!node)
                    return;
                if (node->kind == SK::Block)
                {
                    for (auto child : node->body)
                        flatten(child);
                }
                else if (node->kind == SK::Case)
                {
                    rows.push_back(node);
                    flatten(node->a);
                }
                else
                    rows.push_back(node);
            };
            flatten(s->a);
            uint64_t selector = ExprValue(s->expr);
            size_t first = rows.size(), fallback = rows.size();
            for (size_t i = 0; i < rows.size(); ++i)
                if (rows[i]->kind == SK::Case)
                {
                    if (rows[i]->isDefault)
                        fallback = i;
                    else if (rows[i]->value == selector && first == rows.size())
                        first = i;
                }
            if (first == rows.size())
                first = fallback;
            for (size_t i = first; i < rows.size(); ++i)
            {
                if (rows[i]->kind == SK::Case)
                    continue;
                auto flow = Statement(rows[i], result);
                if (flow == Flow::Break)
                    return Flow::Normal;
                if (flow != Flow::Normal)
                    return flow;
            }
            return Flow::Normal;
        }
        default:
            throw NotConstant{};
        }
    }

  public:
    uint64_t Call(Expr *call)
    {
        Step();
        auto fn = call->a->fn;
        if (!fn || !fn->body || !fn->constexprFunction || depth >= 128)
            throw NotConstant{};
        std::map<VarDecl *, uint64_t> frame;
        for (size_t i = 0; i < call->args.size(); ++i)
            frame[fn->params[i]] = ExprValue(call->args[i]);
        frames.push_back(std::move(frame));
        ++depth;
        uint64_t result = 0;
        auto flow = Statement(fn->body, result);
        --depth;
        frames.pop_back();
        if (flow != Flow::Return && !fn->type->sig.ret->IsVoid())
            throw NotConstant{};
        return result;
    }
};
} // namespace
bool ConstantCall(Expr *e, uint64_t &result)
{
    try
    {
        result = Evaluator{}.Call(e);
        return true;
    }
    catch (const NotConstant &)
    {
        return false;
    }
}
} // namespace cxxsnippets
