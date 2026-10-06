#pragma once
#include "ast.hpp"
#include <functional>
namespace cxxsnippets
{
struct Relocation
{
    size_t offset;
    FuncDecl *function = nullptr;
    VarDecl *variable = nullptr;
    size_t dataOffset = 0;
    void *absolute = nullptr;
    bool relative = false;
    bool owner = false;
};
// Source of the code from `offset` on (until the next entry).
struct LineEntry
{
    size_t offset;
    Loc loc;
};
struct Image
{
    std::vector<uint8_t> code, data;
    std::vector<Relocation> relocations;
    std::vector<LineEntry> lines; // in code order within each function
};
Image Generate(TranslationUnit &unit, const std::function<void *(const std::string &)> &resolve);
uint64_t Calculate(uint64_t a, uint64_t b, int operation, int kind);
uint64_t ConvertValue(uint64_t value, int from, int to);
} // namespace cxxsnippets
