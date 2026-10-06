#pragma once
#include "lexer.hpp"
#include <functional>
#include <map>

namespace cxxsnippets
{
struct PreprocessorConfig
{
    Target target;
    std::vector<std::pair<std::string, std::string>> defines;
    std::vector<std::string> includeDirs;
    // name -> text of in-memory headers (built-in and host supplied)
    std::map<std::string, std::string> headers;
};

// Preprocesses a file already added to `sm` and returns the translation unit's tokens,
// ending with Tok::End.
std::vector<Token> Preprocess(SourceManager &sm, int mainFile, const PreprocessorConfig &cfg);

// Reads a source file; the path is UTF-8.
bool ReadSourceFile(const std::string &path, std::string &out);
} // namespace cxxsnippets
