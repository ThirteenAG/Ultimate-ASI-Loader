// Runs inside a crashed process, so it avoids the heap where it can. Text goes into a buffer
// allocated at install time, module data into static tables.
#pragma once
#include <windows.h>
#include <dbghelp.h>
#include <cstdarg>
#include <cstdint>

namespace crashdump::detail
{
#ifdef _WIN64
    #define CRASH_PTR "0x%016llX"
#else
    #define CRASH_PTR "0x%08X"
#endif

    // Fixed-capacity UTF-8 text with CRLF line endings. Never allocates.
    class Writer
    {
    public:
        void Reset(char* buffer, size_t capacity);
        void Printf(_Printf_format_string_ const char* fmt, ...);
        void Line(_Printf_format_string_ const char* fmt, ...);
        void NewLine() { Raw("\r\n"); }
        void Raw(const char* s);
        void Wide(const wchar_t* s, int count = -1);
        void Heading(const char* title);                          // blank line, title, underline; flushes the previous section
        const char* Data() const { return buf; }
        size_t Size() const { return len; }
        bool Truncated() const { return truncated; }

        // Hands finished text to the sink as the report is built, so what was written survives when a later
        // section blocks (a heap lock held by the crashing thread) or faults.
        using Sink = void (*)(const char* text, size_t size, void* context);
        void SetSink(Sink sink, void* context) { this->sink = sink; sinkContext = context; }
        void Flush();

    private:
        void VPrintf(const char* fmt, va_list ap);
        char* buf = nullptr;
        size_t cap = 0;
        size_t len = 0;
        size_t flushed = 0;
        bool truncated = false;
        Sink sink = nullptr;
        void* sinkContext = nullptr;
    };

    // Always System32's dbghelp.dll, never a copy shipped with the game.
    struct DbgHelp
    {
        HMODULE dll = nullptr;
        decltype(&::MiniDumpWriteDump) MiniDumpWriteDump = nullptr;
        decltype(&::SymInitializeW) SymInitializeW = nullptr;
        decltype(&::SymCleanup) SymCleanup = nullptr;
        decltype(&::SymLoadModuleExW) SymLoadModuleExW = nullptr;
        decltype(&::SymSetOptions) SymSetOptions = nullptr;
        decltype(&::SymFromAddrW) SymFromAddrW = nullptr;
        decltype(&::SymGetLineFromAddrW64) SymGetLineFromAddrW64 = nullptr;
        decltype(&::StackWalk64) StackWalk64 = nullptr;
        decltype(&::SymFunctionTableAccess64) SymFunctionTableAccess64 = nullptr;
        decltype(&::SymGetModuleBase64) SymGetModuleBase64 = nullptr;
        decltype(&::UnDecorateSymbolName) UnDecorateSymbolName = nullptr;

        bool Load();
        bool CanWalk() const { return SymInitializeW && StackWalk64 && SymFunctionTableAccess64 && SymGetModuleBase64; }
    };

    enum class ModuleKind
    {
        Other,
        Game,
        Loader,
        Plugin, // .asi files and DLLs in scripts / plugins
        System, // below the Windows directory
    };

    struct Environment
    {
        HMODULE loader = nullptr;
        wchar_t loaderDir[MAX_PATH] = {}; // with trailing backslash
        wchar_t exeDir[MAX_PATH] = {};
        wchar_t windowsDir[MAX_PATH] = {};
        DbgHelp dbghelp;
        bool fullMemory = false;
        bool zip = true;
        int maxReports = 10;
    };

    struct CrashInfo
    {
        EXCEPTION_POINTERS* ep = nullptr;
        DWORD threadId = 0;
        SYSTEMTIME localTime{};
    };

    void WriteLog(Writer& w, const Environment& env, const CrashInfo& crash);

    const char* ModuleKindName(ModuleKind k);

    extern bool (*volatile g_codeDescriber)(const void* address, char* out, size_t size);
}
