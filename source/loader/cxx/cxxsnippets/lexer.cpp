#include "lexer.hpp"
#include <cstring>

namespace cxxsnippets
{
namespace
{
struct Lexer
{
    const std::string &s;
    int file;
    size_t i = 0;
    int line = 1;
    size_t lineStart = 0;
    bool bol = true;
    bool space = false;

    Lexer(const std::string &text, int f) : s(text), file(f) {}

    char At(size_t k) const
    {
        return k < s.size() ? s[k] : '\0';
    }
    char Cur() const
    {
        return At(i);
    }
    Loc Here() const
    {
        return {file, line, (int)(i - lineStart) + 1};
    }

    void NewLine()
    {
        ++line;
        lineStart = i;
        bol = true;
    }

    // skips whitespace, comments and line continuations
    void Skip()
    {
        for (;;)
        {
            char c = Cur();
            if (c == '\\' && (At(i + 1) == '\n' || (At(i + 1) == '\r' && At(i + 2) == '\n')))
            {
                i += At(i + 1) == '\r' ? 3 : 2;
                ++line;
                lineStart = i;
                space = true;
            }
            else if (c == '\n')
            {
                ++i;
                NewLine();
                space = true;
            }
            else if (c == ' ' || c == '\t' || c == '\r' || c == '\f' || c == '\v')
            {
                ++i;
                space = true;
            }
            else if (c == '/' && At(i + 1) == '/')
            {
                while (i < s.size() && Cur() != '\n')
                {
                    if (Cur() == '\\' && At(i + 1) == '\n')
                    {
                        i += 2;
                        ++line;
                        lineStart = i;
                        continue;
                    }
                    ++i;
                }
                space = true;
            }
            else if (c == '/' && At(i + 1) == '*')
            {
                Loc start = Here();
                i += 2;
                while (i < s.size() && !(Cur() == '*' && At(i + 1) == '/'))
                {
                    if (Cur() == '\n')
                    {
                        ++i;
                        ++line;
                        lineStart = i;
                        continue;
                    }
                    ++i;
                }
                if (i >= s.size())
                    Error(start, "unterminated comment");
                i += 2;
                space = true;
            }
            else
                return;
        }
    }

    static bool IdentStart(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (unsigned char)c >= 0x80;
    }
    static bool IdentChar(char c)
    {
        return IdentStart(c) || (c >= '0' && c <= '9');
    }

    static void AppendUtf16(std::string &out, uint32_t cp)
    {
        auto unit = [&](uint16_t u) {
            out.push_back((char)(u & 0xFF));
            out.push_back((char)(u >> 8));
        };
        if (cp >= 0x10000)
        {
            cp -= 0x10000;
            unit((uint16_t)(0xD800 + (cp >> 10)));
            unit((uint16_t)(0xDC00 + (cp & 0x3FF)));
        }
        else
            unit((uint16_t)cp);
    }

    static void AppendUtf8(std::string &out, uint32_t cp)
    {
        if (cp < 0x80)
            out.push_back((char)cp);
        else if (cp < 0x800)
        {
            out.push_back((char)(0xC0 | (cp >> 6)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        }
        else if (cp < 0x10000)
        {
            out.push_back((char)(0xE0 | (cp >> 12)));
            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        }
        else
        {
            out.push_back((char)(0xF0 | (cp >> 18)));
            out.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
        }
    }

    // one source code point (UTF-8 decoded)
    uint32_t ReadCodePoint()
    {
        unsigned char c = (unsigned char)Cur();
        ++i;
        if (c < 0x80)
            return c;
        int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
        uint32_t cp = c & (0x3F >> extra);
        for (int k = 0; k < extra && (((unsigned char)Cur()) & 0xC0) == 0x80; ++k, ++i)
            cp = (cp << 6) | (Cur() & 0x3F);
        return cp;
    }

    // escape sequence after the backslash; returns the code point / byte value
    uint32_t Escape(bool &isByte)
    {
        Loc at = Here();
        char c = Cur();
        ++i;
        isByte = false;
        switch (c)
        {
        case 'n':
            return '\n';
        case 't':
            return '\t';
        case 'r':
            return '\r';
        case 'a':
            return '\a';
        case 'b':
            return '\b';
        case 'f':
            return '\f';
        case 'v':
            return '\v';
        case '\\':
            return '\\';
        case '\'':
            return '\'';
        case '"':
            return '"';
        case '?':
            return '?';
        case 'x': {
            uint32_t v = 0;
            int n = 0;
            while (isxdigit((unsigned char)Cur()))
            {
                char d = Cur();
                v = v * 16 + (d <= '9' ? d - '0' : (d | 0x20) - 'a' + 10);
                ++i;
                ++n;
            }
            if (!n)
                Error(at, "\\x used with no following hex digits");
            isByte = true;
            return v;
        }
        case 'u':
        case 'U': {
            int n = c == 'u' ? 4 : 8;
            uint32_t v = 0;
            for (int k = 0; k < n; ++k)
            {
                char d = Cur();
                if (!isxdigit((unsigned char)d))
                    Error(at, "incomplete universal character name");
                v = v * 16 + (d <= '9' ? d - '0' : (d | 0x20) - 'a' + 10);
                ++i;
            }
            return v;
        }
        default:
            if (c >= '0' && c <= '7')
            {
                uint32_t v = c - '0';
                for (int k = 0; k < 2 && Cur() >= '0' && Cur() <= '7'; ++k, ++i)
                    v = v * 8 + (Cur() - '0');
                isByte = true;
                return v;
            }
            Error(at, std::string("unknown escape sequence '\\") + c + "'");
        }
    }

    void Number(Token &t)
    {
        size_t start = i;
        bool isFloat = false;
        // scan the pp-number
        if (Cur() == '0' && (At(i + 1) == 'x' || At(i + 1) == 'X'))
        {
            i += 2;
            while (isxdigit((unsigned char)Cur()) || Cur() == '\'')
                ++i;
        }
        else if (Cur() == '0' && (At(i + 1) == 'b' || At(i + 1) == 'B'))
        {
            i += 2;
            while (Cur() == '0' || Cur() == '1' || Cur() == '\'')
                ++i;
        }
        else
        {
            while (isdigit((unsigned char)Cur()) || Cur() == '\'')
                ++i;
            if (Cur() == '.')
            {
                isFloat = true;
                ++i;
                while (isdigit((unsigned char)Cur()) || Cur() == '\'')
                    ++i;
            }
            if (Cur() == 'e' || Cur() == 'E')
            {
                size_t k = i + 1;
                if (At(k) == '+' || At(k) == '-')
                    ++k;
                if (isdigit((unsigned char)At(k)))
                {
                    isFloat = true;
                    i = k;
                    while (isdigit((unsigned char)Cur()))
                        ++i;
                }
            }
        }
        std::string digits;
        for (size_t k = start; k < i; ++k)
            if (s[k] != '\'')
                digits.push_back(s[k]);
        size_t sufStart = i;
        while (IdentChar(Cur()))
            ++i;
        std::string suffix = s.substr(sufStart, i - sufStart);
        t.text = s.substr(start, i - start);
        if (isFloat)
        {
            t.kind = Tok::Float;
            t.fval = strtod(digits.c_str(), nullptr);
            if (suffix == "f" || suffix == "F")
                t.floatSuffix = true;
            else if (!suffix.empty() && suffix != "l" && suffix != "L")
                Error(t.loc, "invalid suffix '" + suffix + "' on floating constant");
            return;
        }
        t.kind = Tok::Int;
        uint64_t v = 0;
        bool overflow = false;
        auto acc = [&](int base, int d) {
            uint64_t nv = v * base + d;
            if (nv / base != v && v != 0)
                overflow = true;
            v = nv;
        };
        if (digits.size() > 1 && digits[0] == '0' && (digits[1] == 'x' || digits[1] == 'X'))
        {
            if (digits.size() == 2)
                Error(t.loc, "invalid hexadecimal constant");
            for (size_t k = 2; k < digits.size(); ++k)
            {
                char d = digits[k];
                acc(16, d <= '9' ? d - '0' : (d | 0x20) - 'a' + 10);
            }
        }
        else if (digits.size() > 1 && digits[0] == '0' && (digits[1] == 'b' || digits[1] == 'B'))
        {
            for (size_t k = 2; k < digits.size(); ++k)
                acc(2, digits[k] - '0');
        }
        else if (digits.size() > 1 && digits[0] == '0')
        {
            for (size_t k = 1; k < digits.size(); ++k)
            {
                if (digits[k] > '7')
                    Error(t.loc, "invalid digit in octal constant");
                acc(8, digits[k] - '0');
            }
        }
        else
            for (char d : digits)
                acc(10, d - '0');
        if (overflow)
            Error(t.loc, "integer constant is too large");
        t.ival = v;
        for (char c : suffix)
        {
            if (c == 'u' || c == 'U')
                t.isUnsigned = true;
            else if (c == 'l' || c == 'L')
                ++t.longs;
            else
                Error(t.loc, "invalid suffix '" + suffix + "' on integer constant");
        }
        if (t.longs > 2)
            Error(t.loc, "invalid suffix '" + suffix + "' on integer constant");
    }

    void Quoted(Token &t, char quote, int width)
    {
        Loc start = t.loc;
        ++i; // opening quote
        std::string out;
        uint32_t charValue = 0;
        int chars = 0;
        while (Cur() != quote)
        {
            if (i >= s.size() || Cur() == '\n')
                Error(start, quote == '"' ? "missing terminating '\"' character" : "missing terminating ' character");
            uint32_t cp;
            bool isByte = false;
            if (Cur() == '\\')
            {
                ++i;
                cp = Escape(isByte);
            }
            else
                cp = ReadCodePoint();
            ++chars;
            charValue = cp;
            if (width == 2)
                AppendUtf16(out, cp);
            else if (isByte)
                out.push_back((char)cp);
            else
                AppendUtf8(out, cp);
        }
        ++i;
        if (quote == '\'')
        {
            if (chars != 1)
                Error(start,
                      chars ? "multi-character character constants are not supported" : "empty character constant");
            t.kind = Tok::Char;
            t.ival = width == 2 ? (charValue & 0xFFFF) : (out.size() == 1 ? (uint64_t)(int8_t)out[0] : charValue);
            t.charWidth = width;
            if (width == 1 && out.size() != 1)
                Error(start, "character too large for a char constant");
        }
        else
        {
            t.kind = Tok::String;
            t.str = std::move(out);
            t.charWidth = width;
        }
    }

    void RawString(Token &t, int width)
    {
        // at 'R' followed by '"'
        Loc start = t.loc;
        i += 2;
        std::string delim;
        while (Cur() != '(')
        {
            if (i >= s.size() || Cur() == '\n' || delim.size() > 16)
                Error(start, "invalid raw string delimiter");
            delim.push_back(Cur());
            ++i;
        }
        ++i;
        std::string end = ")" + delim + "\"";
        size_t e = s.find(end, i);
        if (e == std::string::npos)
            Error(start, "unterminated raw string");
        std::string body = s.substr(i, e - i);
        for (size_t k = i; k < e; ++k)
            if (s[k] == '\n')
            {
                ++line;
                lineStart = k + 1;
            }
        i = e + end.size();
        t.kind = Tok::String;
        t.charWidth = width;
        if (width == 2)
        {
            size_t k = 0;
            std::string out;
            while (k < body.size())
            {
                // decode utf-8
                unsigned char c = (unsigned char)body[k++];
                uint32_t cp = c;
                int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
                if (extra)
                    cp = c & (0x3F >> extra);
                for (int n = 0; n < extra && k < body.size(); ++n)
                    cp = (cp << 6) | (body[k++] & 0x3F);
                AppendUtf16(out, cp);
            }
            t.str = out;
        }
        else
            t.str = body;
    }

    bool Next(Token &t)
    {
        space = false;
        Skip();
        t = Token{};
        t.loc = Here();
        t.bol = bol;
        t.space = space;
        bol = false;
        if (i >= s.size())
        {
            t.kind = Tok::End;
            return false;
        }
        char c = Cur();
        // string / char prefixes
        if (c == 'L' && (At(i + 1) == '"' || At(i + 1) == '\''))
        {
            ++i;
            Quoted(t, Cur(), 2);
            return true;
        }
        if (c == 'L' && At(i + 1) == 'R' && At(i + 2) == '"')
        {
            ++i;
            RawString(t, 2);
            return true;
        }
        if (c == 'u' && At(i + 1) == '8' && At(i + 2) == '"')
        {
            i += 2;
            Quoted(t, '"', 1);
            return true;
        }
        if (c == 'u' && At(i + 1) == '8' && At(i + 2) == 'R' && At(i + 3) == '"')
        {
            i += 2;
            RawString(t, 1);
            return true;
        }
        if (c == 'R' && At(i + 1) == '"')
        {
            RawString(t, 1);
            return true;
        }
        if (IdentStart(c))
        {
            size_t start = i;
            while (IdentChar(Cur()))
                ++i;
            t.kind = Tok::Ident;
            t.text = s.substr(start, i - start);
            return true;
        }
        if (isdigit((unsigned char)c) || (c == '.' && isdigit((unsigned char)At(i + 1))))
        {
            Number(t);
            return true;
        }
        if (c == '"' || c == '\'')
        {
            Quoted(t, c, 1);
            return true;
        }
        static const char *puncts[] = {"...", "<<=", ">>=", "->*", "::", "->", "++", "--", "<<", ">>",
                                       "<=",  ">=",  "==",  "!=",  "&&", "||", "+=", "-=", "*=", "/=",
                                       "%=",  "&=",  "|=",  "^=",  "##", ".*", "[[", "]]"};
        for (const char *p : puncts)
        {
            size_t n = strlen(p);
            if (s.compare(i, n, p) == 0)
            {
                t.kind = Tok::Punct;
                t.text = p;
                i += n;
                return true;
            }
        }
        if (strchr("+-*/%&|^~!=<>?:;,.()[]{}#", c))
        {
            t.kind = Tok::Punct;
            t.text = std::string(1, c);
            ++i;
            return true;
        }
        Error(t.loc, std::string("unexpected character '") + c + "'");
    }
};
} // namespace

std::vector<Token> Lex(const SourceManager &sm, int file)
{
    const std::string &text = sm.Get(file).text;
    Lexer lx(text, file);
    if (text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB &&
        (unsigned char)text[2] == 0xBF)
        lx.i = 3;
    std::vector<Token> out;
    Token t;
    while (lx.Next(t))
    {
        t.builtin = sm.Get(file).builtin;
        out.push_back(t);
    }
    out.push_back(t); // End
    return out;
}
} // namespace cxxsnippets
