#include "log.hpp"
#include "strings.hpp"
#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <locale.h>

namespace ual::log
{
    namespace
    {
        std::atomic<bool> g_enabled{ false };
        HANDLE g_file = INVALID_HANDLE_VALUE;
        ULONGLONG g_start = 0;
        SRWLOCK g_lock = SRWLOCK_INIT;

        // The static CRT stays in the "C" locale, where %ls fails for characters above U+00FF and
        // vsnprintf returns -1 for the whole line. Format with a UTF-8 locale instead, so paths come out as UTF-8.
        _locale_t Utf8Locale()
        {
            static _locale_t loc = _create_locale(LC_ALL, ".UTF8");
            return loc;
        }

        int FormatLine(char* out, size_t size, const char* format, va_list args)
        {
            if (_locale_t loc = Utf8Locale()) return _vsnprintf_l(out, size, format, loc, args);
            return vsnprintf(out, size, format, args);
        }
    }

    void Open(const std::wstring& path)
    {
        g_file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (g_file == INVALID_HANDLE_VALUE) return;
        g_start = GetTickCount64();
        g_enabled = true;
    }

    void EnsureOpen(const std::wstring& path)
    {
        if (Enabled()) return;
        AcquireSRWLockExclusive(&g_lock);
        if (!Enabled())
        {
            g_file = CreateFileW(path.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (g_file != INVALID_HANDLE_VALUE)
            {
                g_start = GetTickCount64();
                g_enabled = true;
            }
        }
        ReleaseSRWLockExclusive(&g_lock);
    }

    bool Enabled()
    {
        return g_enabled.load(std::memory_order_relaxed);
    }

    void Write(const char* format, ...)
    {
        if (!Enabled()) return;
        char line[1024];
        ULONGLONG ms = GetTickCount64() - g_start;
        int n = snprintf(line, sizeof(line), "%6llu.%03llu [%5lu] ", ms / 1000, ms % 1000, GetCurrentThreadId());
        va_list args;
        va_start(args, format);
        int m = FormatLine(line + n, sizeof(line) - n - 2, format, args);
        va_end(args);
        n = m < 0 ? n : (std::min)((int)sizeof(line) - 3, n + m);
        line[n++] = '\r';
        line[n++] = '\n';
        AcquireSRWLockExclusive(&g_lock);
        DWORD written;
        WriteFile(g_file, line, n, &written, nullptr);
        ReleaseSRWLockExclusive(&g_lock);
    }

    std::string Describe(const void* address)
    {
        HMODULE m = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)address, &m) || !m)
        {
            char s[32];
            snprintf(s, sizeof(s), "%p (no module)", address);
            return s;
        }
        wchar_t path[MAX_PATH];
        DWORD len = GetModuleFileNameW(m, path, MAX_PATH);
        std::wstring_view p(path, len);
        std::string name = WideToUtf8(p.substr(p.find_last_of(L'\\') + 1));
        uintptr_t rva = (uintptr_t)address - (uintptr_t)m;
        std::string section;
        auto dos = (const IMAGE_DOS_HEADER*)m;
        auto nt = (const IMAGE_NT_HEADERS*)((const BYTE*)m + dos->e_lfanew);
        auto sec = IMAGE_FIRST_SECTION(nt);
        for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i)
            if (rva >= sec[i].VirtualAddress && rva < sec[i].VirtualAddress + (std::max)(sec[i].Misc.VirtualSize, sec[i].SizeOfRawData))
                section.assign((const char*)sec[i].Name, strnlen((const char*)sec[i].Name, IMAGE_SIZEOF_SHORT_NAME));
        char s[64];
        snprintf(s, sizeof(s), "+0x%llx (%s)", (unsigned long long)rva, section.empty() ? "?" : section.c_str());
        return name + s;
    }
}
