#pragma once
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace cxxsnippets
{
struct Loc
{
    int file = -1; // index into SourceManager files
    int line = 0;
    int col = 0;
};

struct CompileError
{
    Loc loc;
    std::string message;
};

[[noreturn]] inline void Error(const Loc &loc, std::string message)
{
    throw CompileError{loc, std::move(message)};
}

struct SourceFile
{
    std::string name; // as displayed in diagnostics
    std::string dir;  // directory for relative includes ("" for built-in headers)
    std::string text;
    bool builtin = false;
};

struct SourceManager
{
    std::vector<std::unique_ptr<SourceFile>> files;

    int Add(std::string name, std::string dir, std::string text, bool builtin)
    {
        files.push_back(
            std::make_unique<SourceFile>(SourceFile{std::move(name), std::move(dir), std::move(text), builtin}));
        return (int)files.size() - 1;
    }
    const SourceFile &Get(int i) const
    {
        return *files[i];
    }
};

// Owns heap objects of any type for the lifetime of a compilation.
class Arena
{
    struct Holder
    {
        virtual ~Holder() = default;
    };
    template <class T> struct HolderT : Holder
    {
        T value;
        template <class... A> HolderT(A &&...a) : value(std::forward<A>(a)...) {}
    };
    std::vector<std::unique_ptr<Holder>> items;

  public:
    template <class T, class... A> T *New(A &&...a)
    {
        auto h = std::make_unique<HolderT<T>>(std::forward<A>(a)...);
        T *p = &h->value;
        items.push_back(std::move(h));
        return p;
    }
};

struct Target
{
    bool x64 = sizeof(void *) == 8;
    int ptrSize() const
    {
        return x64 ? 8 : 4;
    }
};
} // namespace cxxsnippets
