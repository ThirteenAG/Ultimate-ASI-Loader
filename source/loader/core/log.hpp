// DebugLog=1 writes <loader name>.log next to the loader. When off, each call is one flag check.
// Safe inside the entry point hooks: calls nothing those hooks intercept.
#pragma once
#include <windows.h>
#include <string>

namespace ual::log
{
    void Open(const std::wstring& path);
    // Opens for append even without DebugLog, for problems that are always reported.
    void EnsureOpen(const std::wstring& path);
    bool Enabled();

    // One line, prefixed with time since start and thread id.
    void Write(_Printf_format_string_ const char* format, ...);

    // "module.dll+0x1234 (.text)"
    std::string Describe(const void* address);
}

#define UAL_LOG(...) (::ual::log::Enabled() ? ::ual::log::Write(__VA_ARGS__) : (void)0)
