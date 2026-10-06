#include "report.hpp"
#include <tlhelp32.h>
#include <psapi.h>
#include <intrin.h>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <locale.h>
#include <winternl.h>
#include <type_traits>
#include <initializer_list>
#include <ctime>

namespace crashdump::detail
{
    bool (*volatile g_codeDescriber)(const void* address, char* out, size_t size) = nullptr;

    // Writer

    void Writer::Reset(char* buffer, size_t capacity)
    {
        buf = buffer;
        cap = capacity;
        len = 0;
        flushed = 0;
        truncated = false;
        if (cap) buf[0] = '\0';
    }

    void Writer::VPrintf(const char* fmt, va_list ap)
    {
        if (!buf || len + 1 >= cap)
        {
            truncated = true;
            return;
        }
        // The static CRT is in the "C" locale, where %ls fails for characters above U+00FF and the whole
        // call returns -1. A UTF-8 locale makes %ls produce UTF-8 like Wide() does.
        static _locale_t utf8 = _create_locale(LC_ALL, ".UTF8");
        int n = utf8 ? _vsnprintf_s_l(buf + len, cap - len, _TRUNCATE, fmt, utf8, ap) : _vsnprintf_s(buf + len, cap - len, _TRUNCATE, fmt, ap);
        if (n < 0)
        {
            // truncated, or an encoding error: keep what was written and only declare the buffer full when it is
            len += strnlen(buf + len, cap - len);
            if (len + 1 >= cap)
            {
                len = cap - 1;
                truncated = true;
            }
        }
        else
            len += (size_t)n;
    }

    void Writer::Printf(const char* fmt, ...)
    {
        va_list ap;
        va_start(ap, fmt);
        VPrintf(fmt, ap);
        va_end(ap);
    }

    void Writer::Line(const char* fmt, ...)
    {
        va_list ap;
        va_start(ap, fmt);
        VPrintf(fmt, ap);
        va_end(ap);
        NewLine();
    }

    void Writer::Raw(const char* s)
    {
        Printf("%s", s);
    }

    void Writer::Wide(const wchar_t* s, int count)
    {
        if (!s || !buf) return;
        if (len + 1 >= cap)
        {
            truncated = true;
            return;
        }
        int n = WideCharToMultiByte(CP_UTF8, 0, s, count, buf + len, (int)(cap - len - 1), nullptr, nullptr);
        if (n <= 0 && (count != 0 && (count > 0 || *s)))
        {
            truncated = GetLastError() == ERROR_INSUFFICIENT_BUFFER;
            return;
        }
        if (count < 0 && n > 0) --n; // drop the terminating null
        len += (size_t)n;
        buf[len] = '\0';
    }

    void Writer::Flush()
    {
        if (sink && len > flushed) sink(buf + flushed, len - flushed, sinkContext);
        flushed = len;
    }

    void Writer::Heading(const char* title)
    {
        Flush();
        NewLine();
        Line("%s", title);
        size_t n = strlen(title);
        char line[128];
        n = n < sizeof(line) - 1 ? n : sizeof(line) - 1;
        memset(line, '-', n);
        line[n] = '\0';
        Line("%s", line);
    }

    // DbgHelp

    bool DbgHelp::Load()
    {
        wchar_t path[MAX_PATH];
        UINT n = GetSystemDirectoryW(path, MAX_PATH);
        if (!n || n + 13 >= MAX_PATH) return false;
        wcscat_s(path, L"\\dbghelp.dll");
        dll = LoadLibraryExW(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!dll) return false;
        auto get = [&](auto& fn, const char* name) { fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(GetProcAddress(dll, name)); };
        get(MiniDumpWriteDump, "MiniDumpWriteDump");
        get(SymInitializeW, "SymInitializeW");
        get(SymCleanup, "SymCleanup");
        get(SymLoadModuleExW, "SymLoadModuleExW");
        get(SymSetOptions, "SymSetOptions");
        get(SymFromAddrW, "SymFromAddrW");
        get(SymGetLineFromAddrW64, "SymGetLineFromAddrW64");
        get(StackWalk64, "StackWalk64");
        get(SymFunctionTableAccess64, "SymFunctionTableAccess64");
        get(SymGetModuleBase64, "SymGetModuleBase64");
        get(UnDecorateSymbolName, "UnDecorateSymbolName");
        return MiniDumpWriteDump != nullptr;
    }

    const char* ModuleKindName(ModuleKind k)
    {
        switch (k)
        {
        case ModuleKind::Game: return "game";
        case ModuleKind::Loader: return "loader";
        case ModuleKind::Plugin: return "plugin";
        case ModuleKind::System: return "system";
        default: return "";
        }
    }

    namespace
    {
        // modules

        struct ModuleInfo
        {
            uintptr_t base;
            uintptr_t end;
            wchar_t path[MAX_PATH];
            int nameOffset; // offset, not a pointer, because the table gets sorted
            ModuleKind kind;

            const wchar_t* Name() const { return path + nameOffset; }
        };

        constexpr int kMaxModules = 1024;
        ModuleInfo g_modules[kMaxModules];
        int g_moduleCount = 0;

        bool StartsWithI(const wchar_t* s, const wchar_t* prefix)
        {
            size_t n = wcslen(prefix);
            return n && _wcsnicmp(s, prefix, n) == 0;
        }

        bool EndsWithI(const wchar_t* s, const wchar_t* suffix)
        {
            size_t a = wcslen(s), b = wcslen(suffix);
            return a >= b && _wcsicmp(s + a - b, suffix) == 0;
        }

        ModuleKind Classify(const ModuleInfo& m, const Environment& env)
        {
            if ((HMODULE)m.base == env.loader) return ModuleKind::Loader;
            if ((HMODULE)m.base == GetModuleHandleW(nullptr)) return ModuleKind::Game;
            if (EndsWithI(m.path, L".asi")) return ModuleKind::Plugin;
            wchar_t dir[MAX_PATH * 2];
            for (const wchar_t* sub : { L"scripts\\", L"plugins\\" })
            {
                swprintf_s(dir, L"%s%s", env.loaderDir, sub);
                if (StartsWithI(m.path, dir)) return ModuleKind::Plugin;
                swprintf_s(dir, L"%s%s", env.exeDir, sub);
                if (StartsWithI(m.path, dir)) return ModuleKind::Plugin;
            }
            if (StartsWithI(m.path, env.windowsDir)) return ModuleKind::System;
            return ModuleKind::Other;
        }

        void CollectModules(const Environment& env)
        {
            g_moduleCount = 0;
            auto add = [&](uintptr_t base, size_t size, const wchar_t* path) {
                if (g_moduleCount >= kMaxModules) return;
                ModuleInfo& m = g_modules[g_moduleCount++];
                m.base = base;
                m.end = base + size;
                wcsncpy_s(m.path, path, _TRUNCATE);
                const wchar_t* slash = wcsrchr(m.path, L'\\');
                m.nameOffset = slash ? (int)(slash + 1 - m.path) : 0;
                m.kind = Classify(m, env);
            };
            // the snapshot fails with ERROR_BAD_LENGTH while another thread loads or unloads a module
            HANDLE snap = INVALID_HANDLE_VALUE;
            for (int attempt = 0; attempt < 10 && snap == INVALID_HANDLE_VALUE; ++attempt)
            {
                snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
                if (snap == INVALID_HANDLE_VALUE && GetLastError() != ERROR_BAD_LENGTH) break;
            }
            if (snap != INVALID_HANDLE_VALUE)
            {
                MODULEENTRY32W me{ sizeof(me) };
                for (BOOL ok = Module32FirstW(snap, &me); ok; ok = Module32NextW(snap, &me)) add((uintptr_t)me.modBaseAddr, me.modBaseSize, me.szExePath);
                CloseHandle(snap);
            }
            else
            {
                static HMODULE modules[kMaxModules];
                DWORD needed = 0;
                if (K32EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &needed))
                    for (DWORD i = 0; i < needed / sizeof(HMODULE) && i < kMaxModules; ++i)
                    {
                        MODULEINFO info{};
                        wchar_t path[MAX_PATH];
                        if (K32GetModuleInformation(GetCurrentProcess(), modules[i], &info, sizeof(info)) && GetModuleFileNameW(modules[i], path, MAX_PATH))
                            add((uintptr_t)info.lpBaseOfDll, info.SizeOfImage, path);
                    }
            }
            // insertion sort by base, no allocations
            for (int i = 1; i < g_moduleCount; ++i)
                for (int j = i; j > 0 && g_modules[j - 1].base > g_modules[j].base; --j)
                {
                    ModuleInfo t = g_modules[j];
                    g_modules[j] = g_modules[j - 1];
                    g_modules[j - 1] = t;
                }
        }

        const ModuleInfo* FindModule(uintptr_t addr)
        {
            for (int i = 0; i < g_moduleCount; ++i)
                if (addr >= g_modules[i].base && addr < g_modules[i].end) return &g_modules[i];
            return nullptr;
        }

        // read under __try, the module may be partly unmapped
        struct ImageInfo
        {
            DWORD timestamp = 0;
            VS_FIXEDFILEINFO version{};
            bool hasVersion = false;
            char pdb[MAX_PATH] = {};
        };

        IMAGE_NT_HEADERS* NtHeaders(uintptr_t base)
        {
            auto dos = (IMAGE_DOS_HEADER*)base;
            if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
            auto nt = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
            return nt->Signature == IMAGE_NT_SIGNATURE ? nt : nullptr;
        }

        // Walks the resource directory by hand. FindResource could need the loader lock, which the crashed thread may hold.
        bool ReadVersion(uintptr_t base, IMAGE_NT_HEADERS* nt, VS_FIXEDFILEINFO& out)
        {
            auto& dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE];
            if (!dd.VirtualAddress) return false;
            uintptr_t root = base + dd.VirtualAddress;
            auto findId = [&](uintptr_t dir, WORD id, bool first) -> const IMAGE_RESOURCE_DIRECTORY_ENTRY* {
                auto d = (IMAGE_RESOURCE_DIRECTORY*)dir;
                auto e = (IMAGE_RESOURCE_DIRECTORY_ENTRY*)(d + 1);
                int n = d->NumberOfNamedEntries + d->NumberOfIdEntries;
                for (int i = 0; i < n; ++i)
                    if (first || (!e[i].NameIsString && e[i].Id == id)) return &e[i];
                return nullptr;
            };
            auto type = findId(root, 16 /* RT_VERSION */, false);
            if (!type || !type->DataIsDirectory) return false;
            auto name = findId(root + type->OffsetToDirectory, 0, true);
            if (!name || !name->DataIsDirectory) return false;
            auto lang = findId(root + name->OffsetToDirectory, 0, true);
            if (!lang || lang->DataIsDirectory) return false;
            auto data = (IMAGE_RESOURCE_DATA_ENTRY*)(root + lang->OffsetToData);
            auto p = (const BYTE*)(base + data->OffsetToData);
            for (DWORD i = 0; i + sizeof(VS_FIXEDFILEINFO) <= data->Size && i < 256; i += 4)
                if (*(const DWORD*)(p + i) == 0xFEEF04BD)
                {
                    memcpy(&out, p + i, sizeof(out));
                    return true;
                }
            return false;
        }

        void ReadPdbName(uintptr_t base, IMAGE_NT_HEADERS* nt, char* out, size_t cap)
        {
            auto& dd = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
            if (!dd.VirtualAddress) return;
            auto dbg = (IMAGE_DEBUG_DIRECTORY*)(base + dd.VirtualAddress);
            for (DWORD i = 0; i < dd.Size / sizeof(IMAGE_DEBUG_DIRECTORY); ++i)
            {
                if (dbg[i].Type != IMAGE_DEBUG_TYPE_CODEVIEW || !dbg[i].AddressOfRawData) continue;
                auto cv = (const char*)(base + dbg[i].AddressOfRawData);
                if (memcmp(cv, "RSDS", 4) != 0) continue;
                const char* name = cv + 24;
                const char* slash = strrchr(name, '\\');
                strncpy_s(out, cap, slash ? slash + 1 : name, _TRUNCATE);
                return;
            }
        }

        bool ReadImageInfo(uintptr_t base, ImageInfo& info)
        {
            __try
            {
                auto nt = NtHeaders(base);
                if (!nt) return false;
                info.timestamp = nt->FileHeader.TimeDateStamp;
                info.hasVersion = ReadVersion(base, nt, info.version);
                ReadPdbName(base, nt, info.pdb, sizeof(info.pdb));
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        void WriteVersion(Writer& w, const ImageInfo& ii)
        {
            if (!ii.hasVersion)
            {
                w.Raw("-");
                return;
            }
            w.Printf("%u.%u.%u.%u", HIWORD(ii.version.dwFileVersionMS), LOWORD(ii.version.dwFileVersionMS), HIWORD(ii.version.dwFileVersionLS),
                     LOWORD(ii.version.dwFileVersionLS));
        }

        // Reproducible builds (all current system DLLs) store a hash here, so only plausible values are shown as a date.
        void WriteBuildDate(Writer& w, DWORD t)
        {
            __time64_t tt = t, now = _time64(nullptr);
            struct tm tmv;
            if (t && tt <= now + 86400 && _gmtime64_s(&tmv, &tt) == 0 && tmv.tm_year >= 95)
                w.Printf("built %04d-%02d-%02d", tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);
            else
                w.Printf("timestamp %08lX", t);
        }

        // symbols

        HANDLE g_symProcess = nullptr;
        bool g_symbols = false;

        void InitSymbols(const Environment& env)
        {
            if (!env.dbghelp.SymInitializeW) return;
            // a separate process handle gives us our own DbgHelp session even if the game uses DbgHelp
            if (!DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), GetCurrentProcess(), &g_symProcess, 0, FALSE, DUPLICATE_SAME_ACCESS))
                g_symProcess = GetCurrentProcess();
            if (env.dbghelp.SymSetOptions)
                env.dbghelp.SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS |
                                          SYMOPT_INCLUDE_32BIT_MODULES);
            wchar_t search[MAX_PATH * 5];
            swprintf_s(search, L"%s;%s;%sscripts;%splugins", env.exeDir, env.loaderDir, env.loaderDir, env.loaderDir);
            g_symbols = env.dbghelp.SymInitializeW(g_symProcess, search, TRUE) != FALSE;
            if (g_symbols || !env.dbghelp.SymLoadModuleExW) return;
            // enumeration can fail while another thread loads or unloads a module, so register them from our table
            if (env.dbghelp.SymCleanup) env.dbghelp.SymCleanup(g_symProcess);
            g_symbols = env.dbghelp.SymInitializeW(g_symProcess, search, FALSE) != FALSE;
            if (!g_symbols) return;
            for (int i = 0; i < g_moduleCount; ++i)
            {
                const auto& m = g_modules[i];
                env.dbghelp.SymLoadModuleExW(g_symProcess, nullptr, m.path, nullptr, m.base, (DWORD)(m.end - m.base), nullptr, 0);
            }
        }

        void CleanupSymbols(const Environment& env)
        {
            if (g_symbols && env.dbghelp.SymCleanup) env.dbghelp.SymCleanup(g_symProcess);
            if (g_symProcess && g_symProcess != GetCurrentProcess()) CloseHandle(g_symProcess);
            g_symProcess = nullptr;
            g_symbols = false;
        }

        bool SymbolName(const Environment& env, uintptr_t addr, wchar_t* out, size_t cap)
        {
            if (!g_symbols || !env.dbghelp.SymFromAddrW) return false;
            alignas(8) char buffer[sizeof(SYMBOL_INFOW) + 256 * sizeof(wchar_t)];
            auto sym = (SYMBOL_INFOW*)buffer;
            memset(sym, 0, sizeof(SYMBOL_INFOW));
            sym->SizeOfStruct = sizeof(SYMBOL_INFOW);
            sym->MaxNameLen = 255;
            DWORD64 disp = 0;
            if (!env.dbghelp.SymFromAddrW(g_symProcess, addr, &disp, sym)) return false;
            wcsncpy_s(out, cap, sym->Name, _TRUNCATE);
            return true;
        }

        // code outside any module, e.g. .cxx snippets
        bool DescribeCode(uintptr_t addr, char* out, size_t size)
        {
            auto describe = g_codeDescriber;
            if (!describe) return false;
            __try
            {
                return describe((const void*)addr, out, size);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        // "module+0x1234 (Function+0x12) [file.cpp:42]"
        void WriteLocation(Writer& w, const Environment& env, uintptr_t addr, bool symbols = true)
        {
            const ModuleInfo* m = FindModule(addr);
            if (m) w.Printf("%ls+0x%IX", m->Name(), (size_t)(addr - m->base));
            else
            {
                char text[512];
                if (DescribeCode(addr, text, sizeof(text)))
                {
                    w.Raw("snippet ");
                    w.Raw(text);
                    return;
                }
                w.Raw("<unknown module>");
            }
            if (!symbols || !g_symbols || !env.dbghelp.SymFromAddrW) return;
            alignas(8) static char symBuffer[sizeof(SYMBOL_INFOW) + 512 * sizeof(wchar_t)];
            auto sym = (SYMBOL_INFOW*)symBuffer;
            memset(sym, 0, sizeof(SYMBOL_INFOW));
            sym->SizeOfStruct = sizeof(SYMBOL_INFOW);
            sym->MaxNameLen = 511;
            DWORD64 disp = 0;
            if (env.dbghelp.SymFromAddrW(g_symProcess, addr, &disp, sym))
            {
                w.Raw(" (");
                w.Wide(sym->Name);
                w.Printf("+0x%llX)", (unsigned long long)disp);
                IMAGEHLP_LINEW64 line{ sizeof(line) };
                DWORD ldisp = 0;
                if (env.dbghelp.SymGetLineFromAddrW64 && env.dbghelp.SymGetLineFromAddrW64(g_symProcess, addr, &ldisp, &line) && line.FileName)
                {
                    const wchar_t* file = wcsrchr(line.FileName, L'\\');
                    w.Raw(" [");
                    w.Wide(file ? file + 1 : line.FileName);
                    w.Printf(":%lu]", line.LineNumber);
                }
            }
        }

        // exception decoding

        struct CodeName
        {
            DWORD code;
            const char* name;
            const char* text;
        };

        const CodeName kCodes[] = {
            { 0xC0000005, "EXCEPTION_ACCESS_VIOLATION", "access violation" },
            { 0xC0000006, "EXCEPTION_IN_PAGE_ERROR", "the page could not be read from disk (file or drive gone?)" },
            { 0xC00000FD, "EXCEPTION_STACK_OVERFLOW", "the thread ran out of stack" },
            { 0xC0000094, "EXCEPTION_INT_DIVIDE_BY_ZERO", "integer division by zero" },
            { 0xC0000095, "EXCEPTION_INT_OVERFLOW", "integer overflow" },
            { 0xC000001D, "EXCEPTION_ILLEGAL_INSTRUCTION", "illegal instruction (corrupt code or unsupported CPU feature)" },
            { 0xC0000096, "EXCEPTION_PRIV_INSTRUCTION", "privileged instruction" },
            { 0xC0000025, "EXCEPTION_NONCONTINUABLE_EXCEPTION", "execution continued after a non-continuable exception" },
            { 0xC0000026, "EXCEPTION_INVALID_DISPOSITION", "invalid exception disposition" },
            { 0xC000008C, "EXCEPTION_ARRAY_BOUNDS_EXCEEDED", "array bounds exceeded" },
            { 0xC000008D, "EXCEPTION_FLT_DENORMAL_OPERAND", "floating point: denormal operand" },
            { 0xC000008E, "EXCEPTION_FLT_DIVIDE_BY_ZERO", "floating point: division by zero" },
            { 0xC000008F, "EXCEPTION_FLT_INEXACT_RESULT", "floating point: inexact result" },
            { 0xC0000090, "EXCEPTION_FLT_INVALID_OPERATION", "floating point: invalid operation" },
            { 0xC0000091, "EXCEPTION_FLT_OVERFLOW", "floating point: overflow" },
            { 0xC0000092, "EXCEPTION_FLT_STACK_CHECK", "floating point: stack check" },
            { 0xC0000093, "EXCEPTION_FLT_UNDERFLOW", "floating point: underflow" },
            { 0xC00002B4, "STATUS_FLOAT_MULTIPLE_FAULTS", "floating point: multiple faults" },
            { 0xC00002B5, "STATUS_FLOAT_MULTIPLE_TRAPS", "floating point: multiple traps" },
            { 0x80000003, "EXCEPTION_BREAKPOINT", "breakpoint (int 3) without a debugger" },
            { 0x80000004, "EXCEPTION_SINGLE_STEP", "single step without a debugger" },
            { 0x80000001, "EXCEPTION_GUARD_PAGE", "guard page accessed" },
            { 0xC0000008, "EXCEPTION_INVALID_HANDLE", "invalid handle" },
            { 0xC0000374, "STATUS_HEAP_CORRUPTION", "heap corruption" },
            { 0xC0000409, "STATUS_STACK_BUFFER_OVERRUN", "stack buffer overrun or fail-fast" },
            { 0xC0000417, "STATUS_INVALID_CRUNTIME_PARAMETER", "invalid parameter passed to a C runtime function" },
            { 0xC0000420, "STATUS_ASSERTION_FAILURE", "assertion failure" },
            { 0xC0000135, "STATUS_DLL_NOT_FOUND", "a required DLL was not found" },
            { 0xC0000139, "STATUS_ENTRYPOINT_NOT_FOUND", "a required DLL export was not found" },
            { 0xC0000142, "STATUS_DLL_INIT_FAILED", "a DLL failed to initialize" },
            { 0xC000041D, "STATUS_FATAL_USER_CALLBACK_EXCEPTION", "an exception in a window procedure / callback" },
            { 0xE06D7363, "C++ exception", "unhandled C++ exception" },
            { 0xE0434352, "CLR exception", "unhandled .NET exception" },
            { 0x0EEDFADE, "Delphi exception", "unhandled Delphi exception" },
            { 0x40010005, "DBG_CONTROL_C", "Ctrl+C" },
            { 0x406D1388, "MS_VC_EXCEPTION", "thread naming exception" },
        };

        const CodeName* FindCode(DWORD code)
        {
            for (auto& c : kCodes)
                if (c.code == code) return &c;
            return nullptr;
        }

        const char* FastFailName(ULONG_PTR code)
        {
            switch (code)
            {
            case 2: return "FAST_FAIL_STACK_COOKIE_CHECK_FAILURE (stack buffer overrun)";
            case 3: return "FAST_FAIL_CORRUPT_LIST_ENTRY";
            case 5: return "FAST_FAIL_INVALID_ARG";
            case 7: return "FAST_FAIL_FATAL_APP_EXIT (abort)";
            case 10: return "FAST_FAIL_INVALID_SET_OF_CONTEXT";
            case 13: return "FAST_FAIL_GUARD_ICALL_CHECK_FAILURE (control flow guard)";
            case 14: return "FAST_FAIL_INVALID_IAT";
            case 15: return "FAST_FAIL_RANGE_CHECK_FAILURE";
            case 23: return "FAST_FAIL_INVALID_FILE_OPERATION";
            default: return nullptr;
            }
        }

        // MSVC C++ exception: thrown type name and std::exception::what()
        bool DecodeCppException(const EXCEPTION_RECORD* er, const Environment& env, char* type, size_t typeCap, char* what, size_t whatCap)
        {
            type[0] = what[0] = '\0';
            if (er->ExceptionCode != 0xE06D7363 || er->NumberParameters < 3) return false;
            __try
            {
                uintptr_t object = er->ExceptionInformation[1];
                uintptr_t throwInfo = er->ExceptionInformation[2];
#ifdef _WIN64
                uintptr_t imageBase = er->NumberParameters >= 4 ? er->ExceptionInformation[3] : 0;
#else
                uintptr_t imageBase = 0;
#endif
                if (!throwInfo) return false; // "throw;" with no active exception
                // ThrowInfo { attributes, pmfnUnwind, pForwardCompat, pCatchableTypeArray } (RVAs on x64)
                uintptr_t cta = imageBase + (uintptr_t)(uint32_t)((const int*)throwInfo)[3];
                int count = *(const int*)cta;
                uintptr_t stdException = 0;
                for (int i = 0; i < count && i < 64; ++i)
                {
                    // CatchableType { properties, pType, PMD { mdisp, pdisp, vdisp }, sizeOrOffset, copyFunction }
                    auto ct = (const int*)(imageBase + (uintptr_t)(uint32_t)((const int*)(cta + 4))[i]);
                    uintptr_t td = imageBase + (uintptr_t)(uint32_t)ct[1];
                    const char* name = (const char*)(td + 2 * sizeof(void*)); // TypeDescriptor { vftable, spare, name[] }
                    if (i == 0)
                    {
                        char und[256];
                        if (name[0] == '.' && env.dbghelp.UnDecorateSymbolName &&
                            env.dbghelp.UnDecorateSymbolName(name + 1, und, sizeof(und), UNDNAME_NO_ARGUMENTS | UNDNAME_32_BIT_DECODE))
                        {
                            const char* t = und;
                            if (!strncmp(t, "class ", 6)) t += 6;
                            else if (!strncmp(t, "struct ", 7)) t += 7;
                            strncpy_s(type, typeCap, t, _TRUNCATE);
                        }
                        else
                            strncpy_s(type, typeCap, name, _TRUNCATE);
                    }
                    if (strcmp(name, ".?AVexception@std@@") == 0 && ct[3] == -1 /* pdisp: no virtual base */)
                        stdException = object + ct[2];
                }
                if (stdException)
                {
                    void** vtable = *(void***)stdException;
#ifdef _WIN64
                    using WhatFn = const char* (*)(void*);
#else
                    using WhatFn = const char*(__thiscall*)(void*);
#endif
                    const char* msg = ((WhatFn)vtable[1])((void*)stdException);
                    if (msg) strncpy_s(what, whatCap, msg, _TRUNCATE);
                }
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return type[0] != '\0';
            }
        }

        void WriteExceptionText(Writer& w, const EXCEPTION_RECORD* er, const Environment& env)
        {
            const CodeName* c = FindCode(er->ExceptionCode);
            w.Printf("%s (0x%08X)", c ? c->name : "unknown exception", er->ExceptionCode);
            if ((er->ExceptionCode == 0xC0000005 || er->ExceptionCode == 0xC0000006) && er->NumberParameters >= 2)
            {
                ULONG_PTR op = er->ExceptionInformation[0];
                uintptr_t at = er->ExceptionInformation[1];
                w.Printf(": %s address " CRASH_PTR, op == 0 ? "reading" : op == 1 ? "writing" : op == 8 ? "executing (DEP) at" : "accessing", at);
                if (at < 0x10000) w.Raw(" (null pointer)");
                else if (const ModuleInfo* m = FindModule(at)) w.Printf(" (%ls+0x%IX)", m->Name(), (size_t)(at - m->base));
                if (er->ExceptionCode == 0xC0000006 && er->NumberParameters >= 3) w.Printf(", NTSTATUS 0x%08IX", (size_t)er->ExceptionInformation[2]);
            }
            else if (er->ExceptionCode == 0xE06D7363)
            {
                char type[256], what[512];
                if (DecodeCppException(er, env, type, sizeof(type), what, sizeof(what)))
                {
                    w.Printf(": %s", type[0] ? type : "unknown type");
                    if (what[0]) w.Printf(": \"%s\"", what);
                }
                else
                    w.Raw(" (rethrow or unknown type)");
            }
            else if (er->ExceptionCode == 0xC0000409 && er->NumberParameters >= 1)
            {
                const char* ff = FastFailName(er->ExceptionInformation[0]);
                if (ff) w.Printf(": %s", ff);
                else w.Printf(": fail-fast code %Iu", (size_t)er->ExceptionInformation[0]);
            }
            else if (c)
                w.Printf(": %s", c->text);
            else
            {
                // let ntdll describe unknown NTSTATUS codes
                char msg[256] = {};
                if (FormatMessageA(FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK, GetModuleHandleW(L"ntdll.dll"),
                                   er->ExceptionCode, 0, msg, sizeof(msg), nullptr))
                {
                    size_t n = strlen(msg);
                    while (n && (msg[n - 1] == ' ' || msg[n - 1] == '\r' || msg[n - 1] == '\n')) msg[--n] = '\0';
                    w.Printf(": %s", msg);
                }
            }
        }

        // stack walking

        constexpr int kMaxFrames = 64;
        uintptr_t g_frames[kMaxFrames];
        int g_frameCount = 0;

        void WalkStack(const Environment& env, const CONTEXT* ctx)
        {
            g_frameCount = 0;
            if (!env.dbghelp.CanWalk() || !g_symbols)
            {
#ifdef _WIN64
                g_frames[g_frameCount++] = ctx->Rip;
#else
                g_frames[g_frameCount++] = ctx->Eip;
#endif
                return;
            }
            CONTEXT c = *ctx; // StackWalk64 modifies it
            STACKFRAME64 sf{};
#ifdef _WIN64
            DWORD machine = IMAGE_FILE_MACHINE_AMD64;
            sf.AddrPC.Offset = c.Rip;
            sf.AddrFrame.Offset = c.Rbp;
            sf.AddrStack.Offset = c.Rsp;
#else
            DWORD machine = IMAGE_FILE_MACHINE_I386;
            sf.AddrPC.Offset = c.Eip;
            sf.AddrFrame.Offset = c.Ebp;
            sf.AddrStack.Offset = c.Esp;
#endif
            sf.AddrPC.Mode = sf.AddrFrame.Mode = sf.AddrStack.Mode = AddrModeFlat;
            DWORD64 lastSp = 0;
            while (g_frameCount < kMaxFrames)
            {
                if (!env.dbghelp.StackWalk64(machine, g_symProcess, GetCurrentThread(), &sf, &c, nullptr, env.dbghelp.SymFunctionTableAccess64,
                                             env.dbghelp.SymGetModuleBase64, nullptr))
                    break;
                if (!sf.AddrPC.Offset) break;
                if (g_frameCount && sf.AddrStack.Offset == lastSp && (uintptr_t)sf.AddrPC.Offset == g_frames[g_frameCount - 1]) break; // no progress
                lastSp = sf.AddrStack.Offset;
                g_frames[g_frameCount++] = (uintptr_t)sf.AddrPC.Offset;
            }
            if (!g_frameCount)
            {
#ifdef _WIN64
                g_frames[g_frameCount++] = ctx->Rip;
#else
                g_frames[g_frameCount++] = ctx->Eip;
#endif
            }
        }

        bool IsExecutable(uintptr_t addr)
        {
            MEMORY_BASIC_INFORMATION mbi;
            if (!VirtualQuery((void*)addr, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT) return false;
            return (mbi.Protect & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
        }

        // instruction before addr looks like a call (E8 rel32 or FF /2)
        bool AfterCall(uintptr_t addr)
        {
            __try
            {
                auto p = (const BYTE*)addr;
                if (p[-5] == 0xE8) return true;
                for (int k = 2; k <= 7; ++k)
                    if (p[-k] == 0xFF && (p[-k + 1] & 0x38) == 0x10) return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
            return false;
        }

        uintptr_t StackTop(uintptr_t sp)
        {
            MEMORY_BASIC_INFORMATION mbi;
            if (!VirtualQuery((void*)sp, &mbi, sizeof(mbi)) || mbi.State != MEM_COMMIT) return sp;
            return (uintptr_t)mbi.BaseAddress + mbi.RegionSize;
        }

        bool ReadPtr(uintptr_t addr, uintptr_t& out)
        {
            __try
            {
                out = *(const uintptr_t*)addr;
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        bool ReadByte(uintptr_t addr, BYTE& out)
        {
            __try
            {
                out = *(const BYTE*)addr;
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        // sections

        void WriteRedactedCommandLine(Writer& w, const wchar_t* cmd);

        void WriteSystem(Writer& w, const Environment& env, const CrashInfo& crash)
        {
            w.Heading("System");
            TIME_ZONE_INFORMATION tzi;
            DWORD tz = GetTimeZoneInformation(&tzi);
            LONG bias = -(tzi.Bias + (tz == TIME_ZONE_ID_DAYLIGHT ? tzi.DaylightBias : 0));
            const SYSTEMTIME& t = crash.localTime;
            w.Line("Time:          %04u-%02u-%02u %02u:%02u:%02u (UTC%c%02ld:%02ld)", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond,
                   bias < 0 ? '-' : '+', labs(bias) / 60, labs(bias) % 60);

            wchar_t path[MAX_PATH * 2];
            DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH * 2);
            w.Raw("Executable:    ");
            w.Wide(path, (int)n);
            BOOL wow64 = FALSE;
            IsWow64Process(GetCurrentProcess(), &wow64);
            w.Line(" (PID %lu, %s%s)", GetCurrentProcessId(), sizeof(void*) == 8 ? "64-bit" : "32-bit", wow64 ? " on WoW64" : "");
            ImageInfo exeInfo;
            if (ReadImageInfo((uintptr_t)GetModuleHandleW(nullptr), exeInfo))
            {
                w.Raw("Exe version:   ");
                WriteVersion(w, exeInfo);
                w.Raw(", ");
                WriteBuildDate(w, exeInfo.timestamp);
                w.NewLine();
            }
            w.Raw("Command line:  ");
            WriteRedactedCommandLine(w, GetCommandLineW());
            w.NewLine();

            n = GetModuleFileNameW(env.loader, path, MAX_PATH * 2);
            w.Raw("Loader:        ");
            w.Wide(path, (int)n);
            ImageInfo li;
            if (ReadImageInfo((uintptr_t)env.loader, li))
            {
                w.Raw(" (");
                WriteVersion(w, li);
                w.Raw(", ");
                WriteBuildDate(w, li.timestamp);
                w.Raw(")");
            }
            w.NewLine();

            FILETIME created, exited, kernel, user, now;
            if (GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
            {
                GetSystemTimeAsFileTime(&now);
                ULONGLONG secs = (((ULARGE_INTEGER&)now).QuadPart - ((ULARGE_INTEGER&)created).QuadPart) / 10000000ULL;
                w.Line("Uptime:        %llu:%02llu:%02llu", secs / 3600, (secs / 60) % 60, secs % 60);
            }

            using RtlGetVersionFn = LONG(WINAPI*)(RTL_OSVERSIONINFOW*);
            HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
            RTL_OSVERSIONINFOW osv{ sizeof(osv) };
            if (auto rgv = (RtlGetVersionFn)GetProcAddress(ntdll, "RtlGetVersion")) rgv(&osv);
            wchar_t product[128] = L"Windows", display[64] = L"";
            DWORD size = sizeof(product), ubr = 0, ubrSize = sizeof(ubr);
            const wchar_t* key = L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
            RegGetValueW(HKEY_LOCAL_MACHINE, key, L"ProductName", RRF_RT_REG_SZ, nullptr, product, &size);
            size = sizeof(display);
            RegGetValueW(HKEY_LOCAL_MACHINE, key, L"DisplayVersion", RRF_RT_REG_SZ, nullptr, display, &size);
            RegGetValueW(HKEY_LOCAL_MACHINE, key, L"UBR", RRF_RT_REG_DWORD, nullptr, &ubr, &ubrSize);
            if (osv.dwBuildNumber >= 22000 && !wcsncmp(product, L"Windows 10", 10)) product[9] = L'1'; // registry still says "Windows 10" on 11
            w.Raw("OS:            ");
            w.Wide(product);
            if (display[0])
            {
                w.Raw(" ");
                w.Wide(display);
            }
            w.Printf(" (%lu.%lu.%lu.%lu)", osv.dwMajorVersion, osv.dwMinorVersion, osv.dwBuildNumber, ubr);
            using WineVersionFn = const char*(__cdecl*)();
            if (auto wine = (WineVersionFn)GetProcAddress(ntdll, "wine_get_version")) w.Printf(", Wine %s", wine());
            w.NewLine();

            int regs[4];
            char brand[49] = {};
            __cpuid(regs, 0x80000000);
            if ((unsigned)regs[0] >= 0x80000004)
            {
                for (int i = 0; i < 3; ++i)
                {
                    __cpuid(regs, 0x80000002 + i);
                    memcpy(brand + i * 16, regs, 16);
                }
            }
            const char* b = brand;
            while (*b == ' ') ++b;
            SYSTEM_INFO si;
            GetNativeSystemInfo(&si);
            w.Line("CPU:           %s (%lu logical processors)", *b ? b : "unknown", si.dwNumberOfProcessors);

            MEMORYSTATUSEX ms{ sizeof(ms) };
            if (GlobalMemoryStatusEx(&ms))
            {
                w.Line("RAM:           %llu MB free of %llu MB (%lu%% in use)", ms.ullAvailPhys >> 20, ms.ullTotalPhys >> 20, ms.dwMemoryLoad);
                w.Line("Address space: %llu MB free of %llu MB", ms.ullAvailVirtual >> 20, ms.ullTotalVirtual >> 20);
            }
            PROCESS_MEMORY_COUNTERS_EX pmc{ sizeof(pmc) };
            if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
                w.Line("Process:       %Iu MB private, %Iu MB working set (peak %Iu MB)", pmc.PrivateUsage >> 20, pmc.WorkingSetSize >> 20,
                       pmc.PeakWorkingSetSize >> 20);
            DWORD handles = 0;
            if (GetProcessHandleCount(GetCurrentProcess(), &handles)) w.Line("Handles:       %lu", handles);
        }

        void WriteRegister(Writer& w, const char* name, uintptr_t v)
        {
            w.Printf("%-6s " CRASH_PTR, name, v);
            if (const ModuleInfo* m = FindModule(v)) w.Printf("  -> %ls+0x%IX", m->Name(), (size_t)(v - m->base));
            w.NewLine();
        }

        void WriteRegisters(Writer& w, const CONTEXT* c)
        {
            w.Heading("Registers");
#ifdef _WIN64
            const char* names[] = { "RAX", "RBX", "RCX", "RDX", "RSI", "RDI", "RBP", "RSP", "R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15", "RIP" };
            uintptr_t values[] = { c->Rax, c->Rbx, c->Rcx, c->Rdx, c->Rsi, c->Rdi, c->Rbp, c->Rsp, c->R8, c->R9, c->R10, c->R11, c->R12, c->R13, c->R14, c->R15, c->Rip };
#else
            const char* names[] = { "EAX", "EBX", "ECX", "EDX", "ESI", "EDI", "EBP", "ESP", "EIP" };
            uintptr_t values[] = { c->Eax, c->Ebx, c->Ecx, c->Edx, c->Esi, c->Edi, c->Ebp, c->Esp, c->Eip };
#endif
            for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) WriteRegister(w, names[i], values[i]);
            DWORD f = c->EFlags;
            w.Line("EFLAGS 0x%08lX  [%s%s%s%s%s%s%s ]", f, f & 0x001 ? " CF" : "", f & 0x004 ? " PF" : "", f & 0x010 ? " AF" : "", f & 0x040 ? " ZF" : "",
                   f & 0x080 ? " SF" : "", f & 0x400 ? " DF" : "", f & 0x800 ? " OF" : "");
#ifndef _WIN64
            w.Line("CS %04lX  DS %04lX  ES %04lX  FS %04lX  GS %04lX  SS %04lX", c->SegCs, c->SegDs, c->SegEs, c->SegFs, c->SegGs, c->SegSs);
            if (c->ContextFlags & CONTEXT_EXTENDED_REGISTERS)
            {
                // FXSAVE layout: XMM0-7 at offset 160, 16 bytes each
                for (int i = 0; i < 8; ++i)
                {
                    const float* x = (const float*)(c->ExtendedRegisters + 160 + i * 16);
                    w.Line("XMM%d   %08X %08X %08X %08X  [ %g %g %g %g ]", i, ((const unsigned*)x)[0], ((const unsigned*)x)[1], ((const unsigned*)x)[2],
                           ((const unsigned*)x)[3], x[0], x[1], x[2], x[3]);
                }
            }
#else
            for (int i = 0; i < 16; ++i)
            {
                const float* x = (const float*)(&c->Xmm0 + i);
                w.Line("XMM%-2d  %08X %08X %08X %08X  [ %g %g %g %g ]", i, ((const unsigned*)x)[0], ((const unsigned*)x)[1], ((const unsigned*)x)[2],
                       ((const unsigned*)x)[3], x[0], x[1], x[2], x[3]);
            }
#endif
        }

        void WriteCodeBytes(Writer& w, uintptr_t ip)
        {
            w.Heading("Code around the crash address");
            for (int row = -1; row < 2; ++row)
            {
                uintptr_t start = ip + row * 16;
                w.Printf(CRASH_PTR " ", start);
                for (int i = 0; i < 16; ++i)
                {
                    BYTE b;
                    bool here = start + i == ip;
                    if (ReadByte(start + i, b)) w.Printf(here ? "[%02X]" : " %02X ", b);
                    else w.Raw(here ? "[??]" : " ?? ");
                }
                w.NewLine();
            }
        }

        void WriteStackMemory(Writer& w, uintptr_t sp)
        {
            w.Heading("Stack memory");
            uintptr_t top = StackTop(sp);
            for (int i = 0; i < 64 && sp + i * sizeof(uintptr_t) + sizeof(uintptr_t) <= top; ++i)
            {
                uintptr_t at = sp + i * sizeof(uintptr_t), v;
                if (!ReadPtr(at, v)) break;
                w.Printf(CRASH_PTR "  " CRASH_PTR, at, v);
                if (const ModuleInfo* m = FindModule(v)) w.Printf("  %ls+0x%IX", m->Name(), (size_t)(v - m->base));
                w.NewLine();
            }
        }

        // works without symbols or frame pointers
        void WriteStackScan(Writer& w, const Environment& env, uintptr_t sp)
        {
            w.Heading("Possible return addresses (stack scan)");
            w.Line("(code addresses found on the stack, newest first, system DLLs left out; may include stale values)");
            uintptr_t top = StackTop(sp);
            int found = 0;
            for (uintptr_t at = sp; at + sizeof(uintptr_t) <= top && at < sp + 64 * 1024 && found < 32; at += sizeof(uintptr_t))
            {
                uintptr_t v;
                if (!ReadPtr(at, v)) break;
                const ModuleInfo* m = FindModule(v);
                if (!m || m->kind == ModuleKind::System || !IsExecutable(v) || !AfterCall(v)) continue;
                w.Printf("[" CRASH_PTR "] ", at);
                WriteLocation(w, env, v);
                const char* kind = ModuleKindName(m->kind);
                if (m->kind == ModuleKind::Plugin || m->kind == ModuleKind::Loader) w.Printf("  <%s>", kind);
                w.NewLine();
                ++found;
            }
            if (!found) w.Line("(none found)");
        }

        // Launchers pass session tokens and passwords on the command line (-AUTH_PASSWORD=..., -token ...),
        // and users upload these reports. Values of such arguments are replaced.
        void WriteRedactedCommandLine(Writer& w, const wchar_t* cmd)
        {
            constexpr size_t kMax = 1024;
            size_t total = wcslen(cmd), n = total > kMax ? kMax : total;
            auto lowerEq = [](wchar_t c) { return c >= L'A' && c <= L'Z' ? wchar_t(c - L'A' + L'a') : c; };
            auto sensitive = [&](const wchar_t* s, size_t len) {
                wchar_t low[64];
                size_t m = len < 63 ? len : 63;
                for (size_t i = 0; i < m; ++i) low[i] = lowerEq(s[i]);
                low[m] = 0;
                for (const wchar_t* word : { L"token", L"password", L"passwd", L"secret", L"auth_", L"apikey", L"api_key", L"sessionid", L"session_id", L"ticket", L"credential" })
                    if (wcsstr(low, word)) return true;
                return false;
            };
            size_t i = 0;
            bool redactNext = false;
            while (i < n)
            {
                while (i < n && cmd[i] == L' ') w.Wide(L" ", 1), ++i;
                if (i >= n) break;
                // one argument, quotes included
                size_t start = i;
                bool quoted = false;
                for (; i < n; ++i)
                {
                    if (cmd[i] == L'"') quoted = !quoted;
                    else if (cmd[i] == L' ' && !quoted) break;
                }
                size_t len = i - start;
                if (redactNext)
                {
                    w.Raw("<redacted>");
                    redactNext = false;
                    continue;
                }
                if (sensitive(cmd + start, len))
                {
                    size_t sep = 0;
                    while (sep < len && cmd[start + sep] != L'=' && cmd[start + sep] != L':') ++sep;
                    if (sep < len)
                    {
                        w.Wide(cmd + start, (int)(sep + 1));
                        w.Raw("<redacted>");
                    }
                    else
                    {
                        w.Wide(cmd + start, (int)len);
                        redactNext = true; // -password value
                    }
                    continue;
                }
                w.Wide(cmd + start, (int)len);
            }
            if (total > kMax) w.Raw(" ...");
        }

        void WriteThreads(Writer& w, const Environment& env, DWORD crashThread)
        {
            w.Heading("Other threads");
            // GetThreadDescription allocates on the process heap, which the crashing thread may hold; the
            // underlying query writes into a buffer of ours instead
            using NtQueryInformationThreadFn = LONG(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
            auto queryThread = (NtQueryInformationThreadFn)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationThread");
            constexpr ULONG kThreadNameInformation = 38;
            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
            if (snap == INVALID_HANDLE_VALUE) return;
            THREADENTRY32 te{ sizeof(te) };
            DWORD pid = GetCurrentProcessId(), self = GetCurrentThreadId();
            int count = 0;
            for (BOOL ok = Thread32First(snap, &te); ok && count < 256; ok = Thread32Next(snap, &te))
            {
                if (te.th32OwnerProcessID != pid || te.th32ThreadID == self || te.th32ThreadID == crashThread) continue;
                ++count;
                w.Printf("Thread %5lu  ", te.th32ThreadID);
                HANDLE th = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_LIMITED_INFORMATION, FALSE, te.th32ThreadID);
                if (!th)
                {
                    w.Line("(cannot open)");
                    continue;
                }
                CONTEXT c{};
                c.ContextFlags = CONTEXT_CONTROL;
                bool got = false;
                if (SuspendThread(th) != (DWORD)-1)
                {
                    got = GetThreadContext(th, &c) != FALSE;
                    ResumeThread(th);
                }
                if (got)
                {
#ifdef _WIN64
                    WriteLocation(w, env, c.Rip);
#else
                    WriteLocation(w, env, c.Eip);
#endif
                }
                else
                    w.Raw("(no context)");
                struct
                {
                    UNICODE_STRING name;
                    wchar_t text[256];
                } desc{};
                if (queryThread && queryThread(th, kThreadNameInformation, &desc, sizeof(desc), nullptr) >= 0 && desc.name.Buffer && desc.name.Length)
                {
                    w.Raw("  \"");
                    w.Wide(desc.name.Buffer, desc.name.Length / sizeof(wchar_t));
                    w.Raw("\"");
                }
                w.NewLine();
                CloseHandle(th);
            }
            CloseHandle(snap);
            if (!count) w.Line("(none)");
            w.Line("(full call stacks of all threads are in the minidump)");
        }

        void WriteModules(Writer& w, const Environment& env, bool pluginsOnly)
        {
            w.Heading(pluginsOnly ? "Plugins" : "Loaded modules");
            w.Line("%-*s  %-6s  %-16s %-8s  %s", (int)(sizeof(void*) * 4 + 5), "address range", "kind", "file version", "pe time", "path");
            int shown = 0;
            for (int i = 0; i < g_moduleCount; ++i)
            {
                const ModuleInfo& m = g_modules[i];
                if (pluginsOnly && m.kind != ModuleKind::Plugin) continue;
                ++shown;
                ImageInfo ii;
                bool img = ReadImageInfo(m.base, ii);
                w.Printf(CRASH_PTR "-" CRASH_PTR "  %-6s  ", m.base, m.end, ModuleKindName(m.kind));
                char ver[48] = "-";
                if (img && ii.hasVersion)
                    sprintf_s(ver, "%u.%u.%u.%u", HIWORD(ii.version.dwFileVersionMS), LOWORD(ii.version.dwFileVersionMS), HIWORD(ii.version.dwFileVersionLS),
                              LOWORD(ii.version.dwFileVersionLS));
                w.Printf("%-16s ", ver);
                if (img) w.Printf("%08lX", ii.timestamp); // symbol server key, together with the image size
                else w.Raw("        ");
                w.Raw("  ");
                w.Wide(m.path);
                if (img && ii.pdb[0]) w.Printf("  (pdb: %s)", ii.pdb);
                w.NewLine();
            }
            if (!shown) w.Line("(none)");
            (void)env;
        }
    }

    // report

    void WriteLog(Writer& w, const Environment& env, const CrashInfo& crash)
    {
        const EXCEPTION_RECORD* er = crash.ep->ExceptionRecord;
        const CONTEXT* ctx = crash.ep->ContextRecord;
#ifdef _WIN64
        uintptr_t ip = ctx->Rip, sp = ctx->Rsp;
#else
        uintptr_t ip = ctx->Eip, sp = ctx->Esp;
#endif
        CollectModules(env);
        InitSymbols(env);
        WalkStack(env, ctx);

        uintptr_t faultAddr = (uintptr_t)er->ExceptionAddress;
        const ModuleInfo* faultModule = FindModule(faultAddr);
        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        const wchar_t* exeName = wcsrchr(exe, L'\\') ? wcsrchr(exe, L'\\') + 1 : exe;

        w.Line("Ultimate ASI Loader crash report");
        w.Line("================================");

        w.Heading("Summary");
        w.Raw("Crash:     ");
        w.Wide(exeName);
        w.Raw(" stopped with ");
        WriteExceptionText(w, er, env);
        w.NewLine();
        w.Raw("Location:  ");
        WriteLocation(w, env, faultAddr);
        w.Printf("  (" CRASH_PTR ")", faultAddr);
        if (faultModule && faultModule->kind != ModuleKind::Other) w.Printf("  <%s>", ModuleKindName(faultModule->kind));
        w.NewLine();
        // Crashed inside Windows (RaiseException for C++ exceptions, bad pointer passed to an API).
        // Blame the first frame outside the system DLLs.
        if (!faultModule || faultModule->kind == ModuleKind::System)
        {
            for (int i = 0; i < g_frameCount; ++i)
            {
                const ModuleInfo* m = FindModule(g_frames[i]);
                if (!m || m->kind == ModuleKind::System) continue;
                wchar_t sym[128];
                if (SymbolName(env, g_frames[i], sym, 128) && (!wcscmp(sym, L"_CxxThrowException") || !wcscmp(sym, L"CxxThrowException"))) continue;
                w.Raw("Caller:    ");
                WriteLocation(w, env, g_frames[i]);
                if (m->kind != ModuleKind::Other) w.Printf("  <%s>", ModuleKindName(m->kind));
                w.NewLine();
                break;
            }
        }
        {
            // a .cxx snippet crashed or called what crashed
            char text[512];
            bool found = DescribeCode(faultAddr, text, sizeof(text));
            for (int i = 0; i < g_frameCount && !found; ++i)
                found = DescribeCode(g_frames[i], text, sizeof(text));
            if (found)
            {
                w.Raw("Snippet:   ");
                w.Raw(text);
                w.NewLine();
            }
        }
        w.Printf("Thread:    %lu", crash.threadId);
        if (crash.threadId == GetThreadId(GetCurrentThread())) w.Raw(" (reporter)");
        w.NewLine();

        const ModuleInfo* involved[16];
        int nInvolved = 0;
        auto addInvolved = [&](const ModuleInfo* m) {
            if (!m || m->kind != ModuleKind::Plugin) return;
            for (int i = 0; i < nInvolved; ++i)
                if (involved[i] == m) return;
            if (nInvolved < 16) involved[nInvolved++] = m;
        };
        addInvolved(faultModule);
        for (int i = 0; i < g_frameCount; ++i) addInvolved(FindModule(g_frames[i]));
        if (faultModule && faultModule->kind == ModuleKind::Plugin)
        {
            w.Raw("Cause:     the crash happened inside the plugin ");
            w.Wide(faultModule->path + (StartsWithI(faultModule->path, env.loaderDir) ? wcslen(env.loaderDir) : 0));
            w.Line(". Update or remove it to confirm.");
        }
        else if (nInvolved)
        {
            w.Raw("Involved:  plugins on the call stack (most recent first): ");
            for (int i = 0; i < nInvolved; ++i)
            {
                if (i) w.Raw(", ");
                w.Wide(involved[i]->Name());
            }
            w.NewLine();
        }
        if (er->ExceptionCode == 0xC00000FD) w.Line("Hint:      a stack overflow is usually caused by endless recursion.");
        MEMORYSTATUSEX ms{ sizeof(ms) };
        if (GlobalMemoryStatusEx(&ms) && ms.ullAvailVirtual < (64ull << 20))
            w.Line("Hint:      the process is almost out of address space (%llu MB left); try fewer or lighter mods%s.", ms.ullAvailVirtual >> 20,
                   sizeof(void*) == 4 ? " or a large address aware executable" : "");

        WriteSystem(w, env, crash);

        w.Heading("Exception");
        int depth = 0;
        for (const EXCEPTION_RECORD* r = er; r && depth < 8; r = r->ExceptionRecord, ++depth)
        {
            if (depth) w.Raw("Caused by: ");
            WriteExceptionText(w, r, env);
            w.NewLine();
            w.Printf("Address:   " CRASH_PTR "  ", (uintptr_t)r->ExceptionAddress);
            WriteLocation(w, env, (uintptr_t)r->ExceptionAddress);
            w.NewLine();
            w.Printf("Flags:     0x%08lX%s", r->ExceptionFlags, (r->ExceptionFlags & EXCEPTION_NONCONTINUABLE) ? " (non-continuable)" : "");
            w.NewLine();
            for (DWORD i = 0; i < r->NumberParameters && i < EXCEPTION_MAXIMUM_PARAMETERS; ++i)
                w.Line("Param[%lu]:  " CRASH_PTR, i, (uintptr_t)r->ExceptionInformation[i]);
        }
        if (faultModule)
        {
            w.Raw("Module:    ");
            w.Wide(faultModule->path);
            ImageInfo ii;
            if (ReadImageInfo(faultModule->base, ii))
            {
                w.Raw(" (version ");
                WriteVersion(w, ii);
                w.Raw(", ");
                WriteBuildDate(w, ii.timestamp);
                w.Raw(")");
            }
            w.NewLine();
        }

        w.Heading("Call stack");
        if (!g_symbols) w.Line("(symbols unavailable: dbghelp.dll could not be initialized)");
        for (int i = 0; i < g_frameCount; ++i)
        {
            w.Printf("#%02d " CRASH_PTR "  ", i, g_frames[i]);
            WriteLocation(w, env, g_frames[i]);
            const ModuleInfo* m = FindModule(g_frames[i]);
            if (m && (m->kind == ModuleKind::Plugin || m->kind == ModuleKind::Loader)) w.Printf("  <%s>", ModuleKindName(m->kind));
            w.NewLine();
        }

        WriteStackScan(w, env, sp);
        WriteRegisters(w, ctx);
        WriteCodeBytes(w, ip);
        WriteStackMemory(w, sp);
        WriteThreads(w, env, crash.threadId);
        WriteModules(w, env, true);
        WriteModules(w, env, false);

        w.NewLine();
        if (w.Truncated()) w.Line("[report truncated]");
        w.Line("End of report.");
        CleanupSymbols(env);
    }
}
