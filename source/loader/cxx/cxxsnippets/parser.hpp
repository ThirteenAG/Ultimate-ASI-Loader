#pragma once
#include "ast.hpp"
#include "lexer.hpp"
namespace cxxsnippets
{
void Parse(TranslationUnit &unit, const std::vector<Token> &tokens);
}
