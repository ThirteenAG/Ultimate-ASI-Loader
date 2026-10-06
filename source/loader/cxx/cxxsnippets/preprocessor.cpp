#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "preprocessor.hpp"
#include <algorithm>
#include <deque>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <windows.h>

namespace cxxsnippets
{
namespace
{
struct Macro
{
    bool function = false;
    std::vector<std::string> params;
    bool variadic = false;
    std::vector<Token> body;
};

struct Cond
{
    bool active; // this branch is being compiled
    bool taken;  // some branch of this #if was taken
    bool parentActive;
    bool seenElse = false;
    Loc loc;
};

struct Frame
{
    std::vector<Token> toks;
    size_t pos = 0;
    int file;
    size_t condBase; // conditional stack depth at entry
    std::string key;
};

std::string NormalizeHeaderName(std::string s)
{
    for (auto &c : s)
    {
        if (c == '\\')
            c = '/';
        c = (char)tolower((unsigned char)c);
    }
    return s;
}

// Paths are UTF-8; the file system is accessed through the wide (UTF-16) APIs.
std::wstring WidePath(const std::string &utf8)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), w.data(), n);
    return w;
}

std::string Utf8Path(const std::wstring &wide)
{
    int n = WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(), s.data(), n, nullptr, nullptr);
    return s;
}

bool ReadFile(const std::string &path, std::string &out)
{
    std::ifstream f(std::filesystem::path(WidePath(path)), std::ios::binary);
    if (!f)
        return false;
    std::stringstream ss;
    ss << f.rdbuf();
    out = ss.str();
    return true;
}

std::string DirOf(const std::string &path)
{
    auto p = path.find_last_of("\\/");
    return p == std::string::npos ? std::string() : path.substr(0, p + 1);
}

std::string FullPath(const std::string &p)
{
    std::wstring w = WidePath(p);
    DWORD n = GetFullPathNameW(w.c_str(), 0, nullptr, nullptr);
    if (!n)
        return p;
    std::wstring full(n, L'\0');
    n = GetFullPathNameW(w.c_str(), n, full.data(), nullptr);
    full.resize(n);
    return n ? Utf8Path(full) : p;
}

class Preprocessor
{
  public:
    Preprocessor(SourceManager &sm, const PreprocessorConfig &cfg) : sm(sm), cfg(cfg)
    {
        for (auto &[k, v] : cfg.headers)
            headers[NormalizeHeaderName(k)] = v;
        Define("_WIN32", "1");
        Define("_MSC_VER", "1940");
        Define("_MSC_FULL_VER", "194033808");
        Define("__cplusplus", "202002L");
        Define("_MSVC_LANG", "202002L");
        Define("__CXXSNIPPETS__", "1");
        Define("NDEBUG", "1");
        if (cfg.target.x64)
        {
            Define("_WIN64", "1");
            Define("_M_X64", "100");
            Define("_M_AMD64", "100");
        }
        else
            Define("_M_IX86", "600");
        for (auto &[k, v] : cfg.defines)
            Define(k, v);
    }

    std::vector<Token> Run(int mainFile)
    {
        Push(mainFile);
        Loop();
        Token end;
        end.kind = Tok::End;
        end.loc = {mainFile, sm.Get(mainFile).text.empty() ? 1 : lastLine, 1};
        out.push_back(end);
        return std::move(out);
    }

  private:
    SourceManager &sm;
    const PreprocessorConfig &cfg;
    std::map<std::string, std::string> headers;
    std::map<std::string, Macro> macros;
    std::vector<std::string> onceFiles;
    std::vector<Frame> frames;
    std::vector<Cond> conds;
    std::deque<Token> pending;
    std::vector<Token> out;
    int lastLine = 1;

    void Define(const std::string &name, const std::string &value)
    {
        int f = sm.Add("<command line>", "", value, true);
        auto toks = Lex(sm, f);
        toks.pop_back();
        Macro m;
        m.body = std::move(toks);
        macros[name] = std::move(m);
    }

    void Push(int file, std::string key = {})
    {
        if (frames.size() > 64)
            Error(frames.back().toks[frames.back().pos].loc, "#include nested too deeply");
        Frame f;
        f.toks = Lex(sm, file);
        f.file = file;
        f.condBase = conds.size();
        f.key = key.empty() ? NormalizeHeaderName(FullPath(sm.Get(file).name)) : std::move(key);
        frames.push_back(std::move(f));
    }

    bool Active() const
    {
        return conds.empty() || conds.back().active;
    }

    // ---- token sources

    // next token of the active source text (directives handled here)
    bool SourceNext(Token &t)
    {
        while (!frames.empty())
        {
            Frame &f = frames.back();
            Token &cur = f.toks[f.pos];
            if (cur.kind == Tok::End)
            {
                if (conds.size() > f.condBase)
                    Error(conds.back().loc, "unterminated conditional directive");
                frames.pop_back();
                continue;
            }
            if (cur.bol && cur.Is("#"))
            {
                Directive(f);
                continue;
            }
            ++f.pos;
            if (!Active())
                continue;
            t = cur;
            lastLine = t.loc.line;
            return true;
        }
        return false;
    }

    const Token *SourcePeek()
    {
        // only peeks within the current file; a directive or end of file is "not a token"
        if (frames.empty())
            return nullptr;
        Frame &f = frames.back();
        Token &cur = f.toks[f.pos];
        if (cur.kind == Tok::End || (cur.bol && cur.Is("#")) || !Active())
            return nullptr;
        return &cur;
    }

    // ---- main loop

    void Loop()
    {
        for (;;)
        {
            Token t;
            if (!pending.empty())
            {
                t = pending.front();
                pending.pop_front();
            }
            else if (!SourceNext(t))
                break;
            if (t.kind == Tok::Ident)
            {
                if (ExpandSpecial(t))
                    continue;
                if (TryExpand(t, pending, true))
                    continue;
            }
            out.push_back(std::move(t));
        }
    }

    bool ExpandSpecial(Token &t)
    {
        if (t.text == "__LINE__")
        {
            t.kind = Tok::Int;
            t.ival = (uint64_t)t.loc.line;
            t.text = std::to_string(t.loc.line);
            out.push_back(t);
            return true;
        }
        if (t.text == "__FILE__")
        {
            t.kind = Tok::String;
            t.str = sm.Get(t.loc.file).name;
            t.charWidth = 1;
            out.push_back(t);
            return true;
        }
        return false;
    }

    static bool Hidden(const Token &t, const std::string &name)
    {
        return t.hide && t.hide->count(name);
    }

    // Expands macro `t` if it is one. Tokens of the expansion are put in
    // front of `in`. With `source`, arguments may continue in the source.
    bool TryExpand(const Token &t, std::deque<Token> &in, bool source)
    {
        auto it = macros.find(t.text);
        if (it == macros.end() || Hidden(t, t.text))
            return false;
        const Macro &m = it->second;
        std::vector<std::vector<Token>> args;
        if (m.function)
        {
            // needs '(' next
            const Token *next = !in.empty() ? &in.front() : (source ? SourcePeek() : nullptr);
            if (!next || !next->Is("("))
                return false;
            auto take = [&](Token &x) -> bool {
                if (!in.empty())
                {
                    x = in.front();
                    in.pop_front();
                    return true;
                }
                return source && SourceNext(x);
            };
            Token lp;
            take(lp);
            int depth = 0;
            args.emplace_back();
            for (;;)
            {
                Token x;
                if (!take(x))
                    Error(t.loc, "unterminated argument list invoking macro '" + t.text + "'");
                if (x.Is("("))
                    ++depth;
                else if (x.Is(")"))
                {
                    if (depth == 0)
                        break;
                    --depth;
                }
                // once the __VA_ARGS__ slot (the one after the named parameters) is being filled, commas belong to it
                else if (x.Is(",") && depth == 0 && !(m.variadic && args.size() == m.params.size() + 1))
                {
                    args.emplace_back();
                    continue;
                }
                args.back().push_back(x);
            }
            if (args.size() == 1 && args[0].empty() && m.params.empty() && !m.variadic)
                args.clear();
            size_t expected = m.params.size() + (m.variadic ? 1 : 0);
            if (m.variadic && args.size() == m.params.size())
                args.emplace_back(); // empty __VA_ARGS__
            if (args.size() != expected)
                Error(t.loc, "macro '" + t.text + "' requires " + std::to_string(expected) + " arguments, but " +
                                 std::to_string(args.size()) + " given");
        }

        std::vector<Token> result = Substitute(m, args, t);
        auto hide = std::make_shared<std::set<std::string>>(t.hide ? *t.hide : std::set<std::string>{});
        hide->insert(t.text);
        for (auto &r : result)
        {
            if (r.hide)
                for (auto &h : *r.hide)
                    hide->insert(h);
        }
        std::shared_ptr<const std::set<std::string>> shared = hide;
        for (size_t k = result.size(); k-- > 0;)
        {
            Token r = result[k];
            r.hide = shared;
            r.loc = t.loc;
            r.bol = false;
            if (k == 0)
                r.space = t.space;
            in.push_front(r);
        }
        return true;
    }

    int ParamIndex(const Macro &m, const Token &t) const
    {
        if (t.kind != Tok::Ident)
            return -1;
        for (size_t k = 0; k < m.params.size(); ++k)
            if (m.params[k] == t.text)
                return (int)k;
        if (m.variadic && t.text == "__VA_ARGS__")
            return (int)m.params.size();
        return -1;
    }

    std::vector<Token> ExpandIsolated(const std::vector<Token> &toks)
    {
        std::deque<Token> in(toks.begin(), toks.end());
        std::vector<Token> res;
        while (!in.empty())
        {
            Token t = in.front();
            in.pop_front();
            if (t.kind == Tok::Ident && TryExpand(t, in, false))
                continue;
            res.push_back(std::move(t));
        }
        return res;
    }

    static std::string Spell(const Token &t)
    {
        return t.text;
    }

    std::vector<Token> Substitute(const Macro &m, const std::vector<std::vector<Token>> &args, const Token &at)
    {
        std::vector<Token> res;
        const auto &b = m.body;
        for (size_t k = 0; k < b.size(); ++k)
        {
            const Token &t = b[k];
            if (m.function && t.Is("#") && k + 1 < b.size() && ParamIndex(m, b[k + 1]) >= 0)
            {
                // stringize
                const auto &arg = args[ParamIndex(m, b[k + 1])];
                std::string s;
                for (size_t a = 0; a < arg.size(); ++a)
                {
                    if (a && arg[a].space)
                        s += ' ';
                    s += Spell(arg[a]);
                }
                Token st;
                st.kind = Tok::String;
                st.loc = at.loc;
                st.str = s;
                st.text = "\"" + s + "\"";
                res.push_back(st);
                ++k;
                continue;
            }
            bool pasteNext = k + 1 < b.size() && b[k + 1].Is("##");
            bool pastePrev = k > 0 && b[k - 1].Is("##");
            int p = m.function ? ParamIndex(m, t) : -1;
            if (t.Is("##"))
            {
                if (res.empty() || k + 1 >= b.size())
                    Error(at.loc, "'##' cannot appear at either end of a macro expansion");
                // paste the last emitted token with the first token of the next item
                const Token &nt = b[k + 1];
                int np = m.function ? ParamIndex(m, nt) : -1;
                std::vector<Token> right = np >= 0 ? args[np] : std::vector<Token>{nt};
                if (!right.empty())
                {
                    std::string pasted = res.back().text + right.front().text;
                    int f = sm.Add("<macro paste>", "", pasted, true);
                    auto lexed = Lex(sm, f);
                    lexed.pop_back();
                    if (lexed.size() != 1)
                        Error(at.loc, "pasting \"" + res.back().text + "\" and \"" + right.front().text +
                                          "\" does not give a valid token");
                    res.back() = lexed[0];
                    res.insert(res.end(), right.begin() + 1, right.end());
                }
                ++k;
                continue;
            }
            if (p >= 0)
            {
                if (pasteNext || pastePrev)
                    res.insert(res.end(), args[p].begin(), args[p].end());
                else
                {
                    auto e = ExpandIsolated(args[p]);
                    for (size_t q = 0; q < e.size(); ++q)
                    {
                        Token x = e[q];
                        if (q == 0)
                            x.space = t.space;
                        res.push_back(x);
                    }
                }
                continue;
            }
            res.push_back(t);
        }
        return res;
    }

    // ---- directives

    std::vector<Token> LineRest(Frame &f)
    {
        std::vector<Token> line;
        while (f.toks[f.pos].kind != Tok::End && !f.toks[f.pos].bol)
            line.push_back(f.toks[f.pos++]);
        return line;
    }

    void Directive(Frame &f)
    {
        Token hash = f.toks[f.pos++];
        if (f.toks[f.pos].kind == Tok::End || f.toks[f.pos].bol)
            return; // null directive
        Token name = f.toks[f.pos++];
        std::vector<Token> rest = LineRest(f);
        const std::string &d = name.text;

        if (d == "if" || d == "ifdef" || d == "ifndef")
        {
            bool parent = Active();
            bool value = false;
            if (parent)
            {
                if (d == "if")
                    value = EvalCondition(rest, name.loc);
                else
                {
                    if (rest.empty() || rest[0].kind != Tok::Ident)
                        Error(name.loc, "macro name expected");
                    value = macros.count(rest[0].text) != 0;
                    if (d == "ifndef")
                        value = !value;
                }
            }
            conds.push_back({parent && value, parent && value, parent, false, hash.loc});
            return;
        }
        if (d == "elif" || d == "elifdef" || d == "elifndef")
        {
            if (conds.size() <= f.condBase)
                Error(name.loc, "#" + d + " without #if");
            Cond &c = conds.back();
            if (c.seenElse)
                Error(name.loc, "#" + d + " after #else");
            if (!c.parentActive || c.taken)
            {
                c.active = false;
                return;
            }
            bool value;
            if (d == "elif")
                value = EvalCondition(rest, name.loc);
            else
            {
                if (rest.empty() || rest[0].kind != Tok::Ident)
                    Error(name.loc, "macro name expected");
                value = macros.count(rest[0].text) != 0;
                if (d == "elifndef")
                    value = !value;
            }
            c.active = value;
            c.taken = value;
            return;
        }
        if (d == "else")
        {
            if (conds.size() <= f.condBase)
                Error(name.loc, "#else without #if");
            Cond &c = conds.back();
            if (c.seenElse)
                Error(name.loc, "#else after #else");
            c.seenElse = true;
            c.active = c.parentActive && !c.taken;
            c.taken = true;
            return;
        }
        if (d == "endif")
        {
            if (conds.size() <= f.condBase)
                Error(name.loc, "#endif without #if");
            conds.pop_back();
            return;
        }
        if (!Active())
            return;

        if (d == "define")
        {
            if (rest.empty() || rest[0].kind != Tok::Ident)
                Error(name.loc, "macro names must be identifiers");
            Macro m;
            size_t k = 1;
            if (rest.size() > 1 && rest[1].Is("(") && !rest[1].space)
            {
                m.function = true;
                k = 2;
                while (k < rest.size() && !rest[k].Is(")"))
                {
                    if (rest[k].Is("..."))
                    {
                        m.variadic = true;
                        ++k;
                        continue;
                    }
                    if (rest[k].kind != Tok::Ident)
                        Error(rest[k].loc, "expected a macro parameter name");
                    m.params.push_back(rest[k].text);
                    ++k;
                    if (k < rest.size() && rest[k].Is(","))
                        ++k;
                }
                if (k >= rest.size())
                    Error(name.loc, "missing ')' in macro parameter list");
                ++k;
            }
            m.body.assign(rest.begin() + k, rest.end());
            macros[rest[0].text] = std::move(m);
            return;
        }
        if (d == "undef")
        {
            if (rest.empty() || rest[0].kind != Tok::Ident)
                Error(name.loc, "macro names must be identifiers");
            macros.erase(rest[0].text);
            return;
        }
        if (d == "include")
        {
            auto [hname, angled] = HeaderName(rest, name.loc, true);
            int file = -1;
            std::string key;
            if (!FindInclude(hname, angled, f.file, file, key))
                Error(name.loc, "cannot open include file '" + hname + "'");
            if (std::find(onceFiles.begin(), onceFiles.end(), key) != onceFiles.end())
                return;
            // the file index of the include is resolved, push it
            Push(file, key);
            return;
        }
        if (d == "pragma")
        {
            if (!rest.empty() && rest[0].Is("once"))
            {
                onceFiles.push_back(f.key);
                return;
            }
            if (!rest.empty() && rest[0].Is("pack"))
                Error(name.loc, "#pragma pack is not supported");
            return; // comment, warning, ... are ignored
        }
        if (d == "error")
        {
            std::string msg;
            for (auto &t : rest)
                msg += (msg.empty() ? "" : " ") + (t.kind == Tok::String ? "\"" + t.str + "\"" : t.text);
            Error(name.loc, "#error " + msg);
        }
        if (d == "warning" || d == "line" || d == "ident")
            return;
        Error(name.loc, "invalid preprocessing directive #" + d);
    }

    std::pair<std::string, bool> HeaderName(const std::vector<Token> &toks, const Loc &at, bool expand)
    {
        if (toks.empty())
            Error(at, "#include expects \"FILENAME\" or <FILENAME>");
        if (toks[0].kind == Tok::String)
            return {toks[0].str, false};
        if (toks[0].Is("<"))
        {
            std::string s;
            size_t k = 1;
            for (; k < toks.size() && !toks[k].Is(">"); ++k)
                s += toks[k].kind == Tok::String ? toks[k].str : toks[k].text;
            if (k >= toks.size())
                Error(at, "missing '>' in #include");
            return {s, true};
        }
        if (expand)
        {
            auto e = ExpandIsolated(toks);
            return HeaderName(e, at, false);
        }
        Error(at, "#include expects \"FILENAME\" or <FILENAME>");
    }

    bool FindInclude(const std::string &name, bool angled, int fromFile, int &file, std::string &key)
    {
        std::string text;
        if (!angled)
        {
            const SourceFile &from = sm.Get(fromFile);
            if (!from.builtin || !from.dir.empty())
            {
                std::string p = FullPath(from.dir + name);
                if (ReadFile(p, text))
                {
                    file = sm.Add(p, DirOf(p), std::move(text), false);
                    key = NormalizeHeaderName(p);
                    return true;
                }
            }
        }
        auto it = headers.find(NormalizeHeaderName(name));
        if (it != headers.end())
        {
            file = sm.Add("<" + name + ">", "", it->second, true);
            key = "<builtin>/" + it->first;
            return true;
        }
        for (auto &dir : cfg.includeDirs)
        {
            std::string d = dir;
            if (!d.empty() && d.back() != '\\' && d.back() != '/')
                d += '\\';
            std::string p = FullPath(d + name);
            if (ReadFile(p, text))
            {
                file = sm.Add(p, DirOf(p), std::move(text), false);
                key = NormalizeHeaderName(p);
                return true;
            }
        }
        return false;
    }

    bool HasInclude(const std::string &name, bool angled)
    {
        if (headers.count(NormalizeHeaderName(name)))
            return true;
        std::vector<std::string> dirs;
        if (!angled && !frames.empty())
            dirs.push_back(sm.Get(frames.back().file).dir);
        for (auto &d : cfg.includeDirs)
            dirs.push_back(d + "\\");
        for (auto &d : dirs)
        {
            DWORD attributes = GetFileAttributesW(WidePath(d + name).c_str());
            if (attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY))
                return true;
        }
        return false;
    }

    // ---- #if expressions

    bool EvalCondition(const std::vector<Token> &line, const Loc &at)
    {
        // defined / __has_include first, before macro expansion
        std::vector<Token> toks;
        for (size_t k = 0; k < line.size(); ++k)
        {
            const Token &t = line[k];
            if (t.Is("defined"))
            {
                bool paren = k + 1 < line.size() && line[k + 1].Is("(");
                size_t n = k + (paren ? 2 : 1);
                if (n >= line.size() || line[n].kind != Tok::Ident)
                    Error(t.loc, "macro name expected after 'defined'");
                if (paren && (n + 1 >= line.size() || !line[n + 1].Is(")")))
                    Error(t.loc, "missing ')' after 'defined'");
                toks.push_back(IntToken(macros.count(line[n].text) ? 1 : 0, t.loc));
                k = n + (paren ? 1 : 0);
                continue;
            }
            if (t.Is("__has_include") || t.Is("__has_include_next"))
            {
                if (k + 1 >= line.size() || !line[k + 1].Is("("))
                    Error(t.loc, "missing '(' after '__has_include'");
                size_t e = k + 2;
                int depth = 0;
                std::vector<Token> inner;
                for (; e < line.size(); ++e)
                {
                    if (line[e].Is("("))
                        ++depth;
                    if (line[e].Is(")"))
                    {
                        if (!depth)
                            break;
                        --depth;
                    }
                    inner.push_back(line[e]);
                }
                if (e >= line.size())
                    Error(t.loc, "missing ')' after '__has_include'");
                auto [n, angled] = HeaderName(inner, t.loc, true);
                toks.push_back(IntToken(HasInclude(n, angled) ? 1 : 0, t.loc));
                k = e;
                continue;
            }
            toks.push_back(t);
        }
        auto e = ExpandIsolated(toks);
        for (auto &t : e)
            if (t.kind == Tok::Ident)
                t = IntToken(t.text == "true" ? 1 : 0, t.loc);
        if (e.empty())
            Error(at, "#if with no expression");
        size_t pos = 0;
        int64_t v = Expr(e, pos, 0, at);
        if (pos != e.size())
            Error(e[pos].loc, "unexpected token '" + e[pos].text + "' in preprocessor expression");
        return v != 0;
    }

    static Token IntToken(int64_t v, const Loc &at)
    {
        Token t;
        t.kind = Tok::Int;
        t.ival = (uint64_t)v;
        t.text = std::to_string(v);
        t.loc = at;
        return t;
    }

    static int Prec(const std::string &op)
    {
        static const std::pair<const char *, int> table[] = {
            {"*", 10}, {"/", 10}, {"%", 10}, {"+", 9},  {"-", 9}, {"<<", 8}, {">>", 8}, {"<", 7},  {">", 7},
            {"<=", 7}, {">=", 7}, {"==", 6}, {"!=", 6}, {"&", 5}, {"^", 4},  {"|", 3},  {"&&", 2}, {"||", 1}};
        for (auto &[s, p] : table)
            if (op == s)
                return p;
        return -1;
    }

    int64_t Primary(const std::vector<Token> &e, size_t &pos, const Loc &at, bool evaluate)
    {
        if (pos >= e.size())
            Error(at, "expected value in preprocessor expression");
        const Token &t = e[pos++];
        if (t.kind == Tok::Int || t.kind == Tok::Char)
            return (int64_t)t.ival;
        if (t.Is("("))
        {
            int64_t v = Expr(e, pos, 0, at, evaluate);
            if (pos >= e.size() || !e[pos].Is(")"))
                Error(t.loc, "missing ')' in preprocessor expression");
            ++pos;
            return v;
        }
        if (t.Is("!"))
            return !Primary(e, pos, at, evaluate);
        if (t.Is("-"))
            return (int64_t)(uint64_t(0) - (uint64_t)Primary(e, pos, at, evaluate));
        if (t.Is("+"))
            return Primary(e, pos, at, evaluate);
        if (t.Is("~"))
            return ~Primary(e, pos, at, evaluate);
        Error(t.loc, "invalid token '" + t.text + "' in preprocessor expression");
    }

    int exprDepth = 0;
    int64_t Expr(const std::vector<Token> &e, size_t &pos, int minPrec, const Loc &at, bool evaluate = true)
    {
        // parentheses recurse through Primary: bound the nesting so a hostile #if cannot overflow the stack
        struct Depth
        {
            int &d;
            Depth(int &d, const Loc &at) : d(d)
            {
                if (++d > 256)
                    Error(at, "#if expression nested too deeply");
            }
            ~Depth()
            {
                --d;
            }
        } depth(exprDepth, at);
        int64_t lhs = Primary(e, pos, at, evaluate);
        for (;;)
        {
            if (pos < e.size() && e[pos].Is("?") && minPrec == 0)
            {
                ++pos;
                int64_t a = Expr(e, pos, 0, at, evaluate && lhs != 0);
                if (pos >= e.size() || !e[pos].Is(":"))
                    Error(at, "expected ':' in preprocessor expression");
                ++pos;
                int64_t b = Expr(e, pos, 0, at, evaluate && lhs == 0);
                lhs = lhs ? a : b;
                continue;
            }
            if (pos >= e.size() || e[pos].kind != Tok::Punct)
                break;
            int p = Prec(e[pos].text);
            if (p < 0 || p < minPrec)
                break;
            std::string op = e[pos++].text;
            int64_t rhs = Expr(e, pos, p + 1, at, evaluate && !(op == "&&" && lhs == 0) && !(op == "||" && lhs != 0));
            if (!evaluate)
            {
                lhs = 0;
                continue;
            }
            if ((op == "/" || op == "%") && rhs == 0)
                Error(at, "division by zero in preprocessor expression");
            if ((op == "<<" || op == ">>") && (rhs < 0 || rhs >= 64))
                Error(at, "invalid shift count in preprocessor expression");
            if (op == "*")
                lhs = (int64_t)((uint64_t)lhs * (uint64_t)rhs);
            else if (op == "/")
                lhs = lhs == INT64_MIN && rhs == -1 ? INT64_MIN : lhs / rhs;
            else if (op == "%")
                lhs = lhs == INT64_MIN && rhs == -1 ? 0 : lhs % rhs;
            else if (op == "+")
                lhs = (int64_t)((uint64_t)lhs + (uint64_t)rhs);
            else if (op == "-")
                lhs = (int64_t)((uint64_t)lhs - (uint64_t)rhs);
            else if (op == "<<")
                lhs = (int64_t)((uint64_t)lhs << rhs);
            else if (op == ">>")
                lhs >>= rhs;
            else if (op == "<")
                lhs = lhs < rhs;
            else if (op == ">")
                lhs = lhs > rhs;
            else if (op == "<=")
                lhs = lhs <= rhs;
            else if (op == ">=")
                lhs = lhs >= rhs;
            else if (op == "==")
                lhs = lhs == rhs;
            else if (op == "!=")
                lhs = lhs != rhs;
            else if (op == "&")
                lhs &= rhs;
            else if (op == "^")
                lhs ^= rhs;
            else if (op == "|")
                lhs |= rhs;
            else if (op == "&&")
                lhs = lhs && rhs;
            else if (op == "||")
                lhs = lhs || rhs;
        }
        return lhs;
    }
};
} // namespace

std::vector<Token> Preprocess(SourceManager &sm, int mainFile, const PreprocessorConfig &cfg)
{
    Preprocessor pp(sm, cfg);
    return pp.Run(mainFile);
}
bool ReadSourceFile(const std::string &path, std::string &out)
{
    return ReadFile(path, out);
}
} // namespace cxxsnippets
