// cxxsnippets: compiles a small subset of C++ to x86/x64 machine code in memory.
// Snippets also build with MSVC, so the same file can ship as a DLL/ASI and be debugged natively.
// Built-in headers (<safetyhook.hpp>, <injector/injector.hpp>, <Hooking.Patterns.h>, <windows.h>,
// <cstdint>, ...) bind to the real libraries linked into the host.
#pragma once
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace cxxsnippets
{
struct Diagnostic
{
    std::string file;
    int line = 0;
    int column = 0;
    std::string message;

    std::string ToString() const; // "file(line,col): error: message"
};

struct Options
{
    // Host functions and variables. Snippet declarations such as extern "C" int add(int, int);
    // link against these by name, after the built-in bindings.
    struct Symbol
    {
        std::string name;
        void *address;
    };
    std::vector<Symbol> symbols;

    // Fallback for names not in `symbols`, returns nullptr if unknown.
    // Exports of the loaded system DLLs are searched after it.
    std::function<void *(const std::string &name)> resolve;

    // Extra #define NAME VALUE.
    std::vector<std::pair<std::string, std::string>> defines;

    // Searched for #include "..." and <...> after the snippet's directory and the built-in headers.
    std::vector<std::string> includeDirs;

    // Extra in-memory headers (name -> contents), e.g. for a host API.
    std::vector<std::pair<std::string, std::string>> headers;
};

// Where an address in a snippet's code comes from.
struct SourceLocation
{
    std::string file; // UTF-8
    int line = 0;
    std::string function; // qualified name; "(global initialization)" for initializers
};

// A structured exception (access violation, ...) raised while the host ran snippet code.
struct Crash
{
    unsigned long code = 0; // 0: no crash
    const void *address = nullptr;
    SourceLocation where; // empty if the address is not in the snippet's code
};

class Module
{
  public:
    virtual ~Module() = default; // runs Shutdown() if defined, releases global hooks, frees the code

    // Address of an external-linkage function or global by C++ name, e.g. "Init", "ns::counter".
    virtual void *Find(const std::string &name) const = 0;

    // Calls `void Init()` if defined. False if it isn't or it crashed. With `crash`, a crash is
    // caught and described there instead of reaching the process.
    virtual bool RunInit(Crash *crash = nullptr) = 0;
    // Calls `void Shutdown()` if defined. Unload and the destructor also do this.
    virtual void RunShutdown(Crash *crash = nullptr) = 0;

    virtual size_t CodeSize() const = 0;
    virtual size_t DataSize() const = 0;

    virtual bool Contains(const void *address) const = 0;
    // Source location of the statement containing address.
    virtual bool Locate(const void *address, SourceLocation &out) const = 0;

    // Runs Shutdown(), releases hooks and owned objects, and restores bytes changed by memory
    // writes (WriteMemory, MakeNOP, MakeJMP, ...), newest first. The code stays mapped so calls
    // still running on other threads can finish; it is freed with the Module.
    virtual void Unload() = 0;
    virtual bool Unloaded() const = 0;
};

// Compiles for the calling process's architecture and runs global initialization. On failure
// returns nullptr and fills `errors`; compilation stops at the first error. A crash during global
// initialization is reported at the crashing line and the snippet's effects are undone.
std::unique_ptr<Module> Compile(const std::string &source, const std::string &fileName, const Options &options,
                                std::vector<Diagnostic> &errors);

// `path` (like every path in Options and Diagnostic) is UTF-8.
std::unique_ptr<Module> CompileFile(const std::string &path, const Options &options, std::vector<Diagnostic> &errors);

// Compiles without loading or running anything. True if the snippet is valid.
bool Check(const std::string &source, const std::string &fileName, const Options &options, std::vector<Diagnostic> &errors);
bool CheckFile(const std::string &path, const Options &options, std::vector<Diagnostic> &errors);

// Names of the built-in headers.
std::vector<std::string> BuiltinHeaders();
} // namespace cxxsnippets
