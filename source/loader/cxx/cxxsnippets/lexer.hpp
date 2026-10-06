#pragma once
#include "common.hpp"
#include <set>

namespace cxxsnippets
{
enum class Tok
{
    Ident,
    Int,
    Float,
    Char,
    String,
    Punct,
    End,
};

struct Token
{
    Tok kind = Tok::End;
    std::string text; // identifier / punctuator spelling, raw literal spelling
    Loc loc;
    bool bol = false;   // first token on its line
    bool space = false; // whitespace before it
    bool builtin = false;

    // literals
    uint64_t ival = 0;
    double fval = 0;
    bool isUnsigned = false;
    int longs = 0; // number of 'l' suffixes
    bool floatSuffix = false;
    int charWidth = 1; // 1 = char, 2 = wchar_t (L"", L'')
    std::string str;   // decoded string literal (code units, little-endian for wide)

    // macro expansion: names of macros this token came from
    std::shared_ptr<const std::set<std::string>> hide;

    bool Is(const char *s) const
    {
        return (kind == Tok::Punct || kind == Tok::Ident) && text == s;
    }
};

// Tokenizes a whole file.
std::vector<Token> Lex(const SourceManager &sm, int file);
} // namespace cxxsnippets
