// A separate reporter thread writes the report while the crashed thread waits. That works after a
// stack overflow and calls MiniDumpWriteDump from outside the crashed thread, as Microsoft recommends.
// Per crash: <exe>.<yyyymmdd_hhmmss>.log (always kept) and .zip with dump, log and ini files
// (a plain .dmp with CrashDumpZip=0).
#include "crashdump.hpp"
#include "report.hpp"
#include "../core/hooks.hpp"
#include <miniz.h>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace crashdump
{
    namespace
    {
        using namespace detail;

        constexpr size_t kLogCapacity = 2u << 20;

        struct State
        {
            Environment env;
            std::vector<std::wstring> iniPaths;
            wchar_t folder[MAX_PATH * 2] = {}; // with trailing backslash; loaderDir/exeDir + "CrashDumps\" can exceed MAX_PATH
            HANDLE reporter = nullptr;
            DWORD reporterId = 0;
            volatile LONG reporterStarted = 0;
            HANDLE request = nullptr;
            HANDLE done = nullptr;
            volatile LONG crashing = 0;
            CrashInfo crash;
            char* log = nullptr;
            bool installed = false;
            void (*onCrash)() = nullptr;
            void* volatile gameFilter = nullptr; // LPTOP_LEVEL_EXCEPTION_FILTER the game tried to set; returned to it, never called
        } g;

        bool DirExists(const wchar_t* p)
        {
            DWORD a = GetFileAttributesW(p);
            return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
        }

        bool FileExists(const wchar_t* p)
        {
            DWORD a = GetFileAttributesW(p);
            return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
        }

        void DirOf(HMODULE m, wchar_t* out, size_t cap)
        {
            DWORD n = GetModuleFileNameW(m, out, (DWORD)cap);
            if (!n || n >= cap)
            {
                out[0] = 0;
                return;
            }
            wchar_t* slash = wcsrchr(out, L'\\');
            if (slash) slash[1] = 0;
        }

        DWORD Timeout()
        {
            return g.env.fullMemory ? 10 * 60 * 1000 : 2 * 60 * 1000;
        }

        // unique "<folder><exe>.<yyyymmdd_hhmmss>[_n]"
        void MakeBaseName(wchar_t* out, size_t cap, const SYSTEMTIME& t)
        {
            wchar_t exe[MAX_PATH];
            DWORD n = GetModuleFileNameW(nullptr, exe, MAX_PATH);
            const wchar_t* name = n ? wcsrchr(exe, L'\\') : nullptr;
            name = name ? name + 1 : L"game.exe";
            swprintf_s(out, cap, L"%s%s.%04u%02u%02u_%02u%02u%02u", g.folder, name, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
            size_t baseLen = wcslen(out);
            for (int i = 2; i < 100; ++i)
            {
                wchar_t probe[MAX_PATH * 2];
                bool used = false;
                for (const wchar_t* ext : { L".log", L".zip", L".dmp" })
                {
                    swprintf_s(probe, L"%s%s", out, ext);
                    used |= FileExists(probe);
                }
                if (!used) return;
                swprintf_s(out + baseLen, cap - baseLen, L"_%d", i);
            }
        }

        bool WriteAll(const wchar_t* path, const void* data, size_t size)
        {
            HANDLE f = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (f == INVALID_HANDLE_VALUE) return false;
            DWORD written = 0;
            BOOL ok = WriteFile(f, data, (DWORD)size, &written, nullptr) && written == size;
            CloseHandle(f);
            return ok != FALSE;
        }

        bool WriteDump(const wchar_t* path, const char* comment, size_t commentSize)
        {
            auto writeDump = g.env.dbghelp.MiniDumpWriteDump;
            if (!writeDump) return false;
            HANDLE f = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (f == INVALID_HANDLE_VALUE) return false;
            MINIDUMP_EXCEPTION_INFORMATION mei{ g.crash.threadId, g.crash.ep, FALSE };
            MINIDUMP_USER_STREAM stream{ CommentStreamA, (ULONG)commentSize, (void*)comment };
            MINIDUMP_USER_STREAM_INFORMATION streams{ commentSize ? 1ul : 0ul, &stream };
            int common = MiniDumpWithHandleData | MiniDumpWithUnloadedModules | MiniDumpWithThreadInfo | MiniDumpWithFullMemoryInfo |
                         MiniDumpWithProcessThreadData;
            int type = g.env.fullMemory ? common | MiniDumpWithFullMemory
                                        : common | MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory | MiniDumpIgnoreInaccessibleMemory;
            BOOL ok = writeDump(GetCurrentProcess(), GetCurrentProcessId(), f, (MINIDUMP_TYPE)type, &mei, &streams, nullptr);
            if (!ok)
            {
                // older dbghelp versions reject some flags
                SetFilePointer(f, 0, nullptr, FILE_BEGIN);
                SetEndOfFile(f);
                ok = writeDump(GetCurrentProcess(), GetCurrentProcessId(), f, (MINIDUMP_TYPE)(MiniDumpWithDataSegs | MiniDumpWithHandleData), &mei, &streams,
                               nullptr);
            }
            CloseHandle(f);
            if (!ok) DeleteFileW(path);
            return ok != FALSE;
        }

        std::string Utf8(const std::wstring& s)
        {
            int n = WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0, nullptr, nullptr);
            std::string out(n, '\0');
            WideCharToMultiByte(CP_UTF8, 0, s.c_str(), (int)s.size(), out.data(), n, nullptr, nullptr);
            return out;
        }

        std::wstring FileName(const std::wstring& p)
        {
            auto s = p.find_last_of(L"\\/");
            return s == std::wstring::npos ? p : p.substr(s + 1);
        }

        // archive name of an ini file, relative to the loader folder
        std::string ConfigEntryName(const std::wstring& path)
        {
            std::wstring rel = path;
            size_t n = wcslen(g.env.loaderDir);
            if (n && _wcsnicmp(path.c_str(), g.env.loaderDir, n) == 0) rel = path.substr(n);
            else rel = FileName(path);
            std::replace(rel.begin(), rel.end(), L'\\', L'/');
            return "config/" + Utf8(rel);
        }

        // Packs .dmp, .log and ini files into <base>.zip and deletes the .dmp. The .log stays for a quick look.
        bool ZipReport(const std::wstring& base)
        {
            std::wstring dmp = base + L".dmp", log = base + L".log", zip = base + L".zip", tmp = zip + L".tmp";
            if (!FileExists(dmp.c_str()) && !FileExists(log.c_str())) return false;
            FILE* out = nullptr;
            if (_wfopen_s(&out, tmp.c_str(), L"wb") || !out) return false;
            mz_zip_archive z{};
            bool ok = mz_zip_writer_init_cfile(&z, out, 0) != MZ_FALSE;
            auto add = [&](const std::wstring& path, const std::string& name, mz_uint level) {
                FILE* in = nullptr;
                if (_wfopen_s(&in, path.c_str(), L"rb") || !in) return false;
                _fseeki64(in, 0, SEEK_END);
                long long size = _ftelli64(in);
                _fseeki64(in, 0, SEEK_SET);
                bool r = size >= 0 && mz_zip_writer_add_cfile(&z, name.c_str(), in, (mz_uint64)size, nullptr, nullptr, 0, level, nullptr, 0, nullptr, 0);
                fclose(in);
                return r;
            };
            std::string stem = Utf8(FileName(base));
            if (ok && FileExists(dmp.c_str())) ok = add(dmp, stem + ".dmp", MZ_BEST_SPEED);
            if (ok && FileExists(log.c_str())) ok = add(log, stem + ".log", MZ_DEFAULT_LEVEL);
            if (ok)
            {
                std::vector<std::string> added;
                for (auto& ini : g.iniPaths)
                {
                    if (!FileExists(ini.c_str())) continue;
                    std::string name = ConfigEntryName(ini);
                    if (std::find(added.begin(), added.end(), name) != added.end()) continue;
                    added.push_back(name);
                    add(ini, name, MZ_DEFAULT_LEVEL); // failure is not fatal
                }
            }
            ok = ok && mz_zip_writer_finalize_archive(&z);
            mz_zip_writer_end(&z);
            ok = (fclose(out) == 0) && ok;
            if (ok && MoveFileExW(tmp.c_str(), zip.c_str(), MOVEFILE_REPLACE_EXISTING))
            {
                DeleteFileW(dmp.c_str());
                return true;
            }
            DeleteFileW(tmp.c_str());
            return false;
        }

        struct ReportGroup
        {
            std::wstring stem;
            ULONGLONG newest = 0;
            std::vector<std::wstring> files;
            bool hasDump = false, hasZip = false;
        };

        // "<exe>.<yyyymmdd_hhmmss>[_n]" (MakeBaseName) or the old loader's "<exe>.<yyyymmddhhmmss>"; anything
        // else in the folder belongs to the user and is left alone
        bool IsReportStem(const std::wstring& stem)
        {
            size_t dot = stem.find_last_of(L'.');
            if (dot == std::wstring::npos || dot == 0) return false;
            std::wstring_view s(stem.c_str() + dot + 1);
            size_t i = 0;
            auto digits = [&](size_t n) {
                if (s.size() < i + n) return false;
                for (size_t k = 0; k < n; ++k)
                    if (!iswdigit(s[i + k])) return false;
                i += n;
                return true;
            };
            if (!digits(8)) return false;
            if (i < s.size() && s[i] == L'_')
            {
                ++i;
                if (!digits(6)) return false;
            }
            else if (!digits(6))
                return false;
            if (i < s.size())
            {
                if (s[i] != L'_') return false;
                ++i;
                if (i >= s.size()) return false;
                while (i < s.size())
                    if (!iswdigit(s[i++])) return false;
            }
            return true;
        }

        std::vector<ReportGroup> ListReports()
        {
            std::vector<ReportGroup> groups;
            WIN32_FIND_DATAW fd;
            std::wstring pattern = std::wstring(g.folder) + L"*";
            HANDLE h = FindFirstFileW(pattern.c_str(), &fd);
            if (h == INVALID_HANDLE_VALUE) return groups;
            do
            {
                if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
                std::wstring name = fd.cFileName, stem;
                auto ends = [&](const wchar_t* ext) {
                    size_t n = wcslen(ext);
                    if (name.size() > n && _wcsicmp(name.c_str() + name.size() - n, ext) == 0)
                    {
                        stem = name.substr(0, name.size() - n);
                        return true;
                    }
                    return false;
                };
                bool dmp = ends(L".dmp");
                bool zip = !dmp && ends(L".zip");
                if (!dmp && !zip && !ends(L".log") && !ends(L".zip.tmp")) continue;
                if (!IsReportStem(stem)) continue;
                auto it = std::find_if(groups.begin(), groups.end(), [&](const ReportGroup& r) { return _wcsicmp(r.stem.c_str(), stem.c_str()) == 0; });
                if (it == groups.end())
                {
                    groups.push_back({ stem });
                    it = groups.end() - 1;
                }
                ULONGLONG t = ((ULONGLONG)fd.ftLastWriteTime.dwHighDateTime << 32) | fd.ftLastWriteTime.dwLowDateTime;
                it->newest = (std::max)(it->newest, t);
                it->files.push_back(std::wstring(g.folder) + name);
                it->hasDump |= dmp;
                it->hasZip |= zip;
            } while (FindNextFileW(h, &fd));
            FindClose(h);
            return groups;
        }

        void Prune(int keep)
        {
            if (keep <= 0) return;
            auto groups = ListReports();
            if ((int)groups.size() <= keep) return;
            std::sort(groups.begin(), groups.end(), [](const ReportGroup& a, const ReportGroup& b) { return a.newest > b.newest; });
            for (size_t i = (size_t)keep; i < groups.size(); ++i)
                for (auto& f : groups[i].files) DeleteFileW(f.c_str());
        }

        // Zips reports a previous crash left unzipped (zipping inside a crashed process is best effort),
        // removes partial archives and applies the report limit.
        DWORD WINAPI Housekeeping(void*)
        {
            for (auto& r : ListReports())
            {
                for (auto& f : r.files)
                    if (f.size() > 8 && _wcsicmp(f.c_str() + f.size() - 8, L".zip.tmp") == 0) DeleteFileW(f.c_str());
                if (g.env.zip && r.hasDump && !r.hasZip) ZipReport(std::wstring(g.folder) + r.stem);
            }
            Prune(g.env.maxReports);
            return 0;
        }

        void AppendToLogFile(const char* text, size_t size, void* context)
        {
            DWORD written = 0;
            WriteFile((HANDLE)context, text, (DWORD)size, &written, nullptr);
        }

        // Builds the text report; with a file handle, each finished section is written to it right away
        size_t BuildLog(HANDLE logFile)
        {
            Writer w;
            w.Reset(g.log, kLogCapacity);
            if (logFile != INVALID_HANDLE_VALUE) w.SetSink(AppendToLogFile, logFile);
            __try
            {
                WriteLog(w, g.env, g.crash);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                w.NewLine();
                w.Line("[the report is incomplete: exception 0x%08lX while writing it]", GetExceptionCode());
            }
            w.Flush();
            return w.Size();
        }

        bool GuardedDump(const wchar_t* path, size_t logSize)
        {
            __try
            {
                return WriteDump(path, g.log, logSize);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        void Finish(const wchar_t* base)
        {
            if (g.env.zip) ZipReport(base);
            Prune(g.env.maxReports);
        }

        void GuardedFinish(const wchar_t* base)
        {
            __try
            {
                Finish(base);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }

        void Report()
        {
            wchar_t base[MAX_PATH * 2], path[MAX_PATH * 2];
            MakeBaseName(base, MAX_PATH * 2, g.crash.localTime);
            // the log first, it is small and the most useful part; it is written section by section so the summary
            // and the call stack are on disk even if a later section hangs on a lock the crashing thread holds
            swprintf_s(path, L"%s.log", base);
            HANDLE logFile = g.log ? CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr) : INVALID_HANDLE_VALUE;
            size_t logSize = g.log ? BuildLog(logFile) : 0;
            if (logFile != INVALID_HANDLE_VALUE) CloseHandle(logFile);
            else if (logSize) WriteAll(path, g.log, logSize);
            swprintf_s(path, L"%s.dmp", base);
            GuardedDump(path, logSize);
            // best effort, needs the crashed process's heap
            GuardedFinish(base);
        }

        DWORD WINAPI Reporter(void*)
        {
            InterlockedExchange(&g.reporterStarted, 1);
            for (;;)
            {
                if (WaitForSingleObject(g.request, INFINITE) != WAIT_OBJECT_0) return 0;
                Report();
                SetEvent(g.done);
            }
        }

        LONG WINAPI Filter(EXCEPTION_POINTERS* ep)
        {
            if (GetCurrentThreadId() == g.reporterId) return EXCEPTION_CONTINUE_SEARCH;
            if (g.onCrash) g.onCrash(); // e.g. make the file hooks pass through: the reporter must not block on their locks
            if (InterlockedCompareExchange(&g.crashing, 1, 0) != 0)
            {
                // another thread is already reporting, wait for it
                if (g.done) WaitForSingleObject(g.done, Timeout());
                return EXCEPTION_CONTINUE_SEARCH;
            }
            g.crash.ep = ep;
            g.crash.threadId = GetCurrentThreadId();
            GetLocalTime(&g.crash.localTime);
            // the reporter thread can't start under the loader lock (crash during DLL init), so report from here
            bool alive = g.reporter && g.reporterStarted && WaitForSingleObject(g.reporter, 0) == WAIT_TIMEOUT;
            if (alive)
            {
                SetEvent(g.request);
                WaitForSingleObject(g.done, Timeout());
            }
            else
            {
                Report();
                if (g.done) SetEvent(g.done);
            }
            // games clip and hide the cursor
            ClipCursor(nullptr);
            ShowCursor(TRUE);
            return EXCEPTION_CONTINUE_SEARCH;
        }

        // The game's (or a launcher's) SetUnhandledExceptionFilter must not replace ours. Instead of patching the
        // system function's code, it is hooked: the game's filter is accepted and remembered, so the game sees the
        // usual return value, but it stays inactive. The crash still reaches Windows Error Reporting afterwards.
        decltype(&SetUnhandledExceptionFilter) oSetUnhandledExceptionFilter = nullptr;
        LPTOP_LEVEL_EXCEPTION_FILTER WINAPI hkSetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER filter)
        {
            if (filter == Filter) return oSetUnhandledExceptionFilter(filter);
            return (LPTOP_LEVEL_EXCEPTION_FILTER)InterlockedExchangePointer((void* volatile*)&g.gameFilter, (void*)filter);
        }

        void LockFilter()
        {
            ual::InstallHooks({ { L"kernelbase.dll", "SetUnhandledExceptionFilter", (void*)hkSetUnhandledExceptionFilter, (void**)&oSetUnhandledExceptionFilter } });
            if (!oSetUnhandledExceptionFilter) // older Windows: the implementation lives in kernel32
                ual::InstallHooks({ { L"kernel32.dll", "SetUnhandledExceptionFilter", (void*)hkSetUnhandledExceptionFilter, (void**)&oSetUnhandledExceptionFilter } });
        }

        void NameThread(HANDLE t, const wchar_t* name)
        {
            using SetThreadDescriptionFn = HRESULT(WINAPI*)(HANDLE, PCWSTR);
            if (auto f = (SetThreadDescriptionFn)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetThreadDescription")) f(t, name);
        }
    }

    void SetCodeDescriber(CodeDescriber describer)
    {
        detail::g_codeDescriber = describer;
    }

    bool Install(HMODULE loader, const Settings& settings)
    {
        if (settings.disabled || g.installed) return false;

        Environment& env = g.env;
        env.loader = loader;
        DirOf(loader, env.loaderDir, MAX_PATH);
        DirOf(nullptr, env.exeDir, MAX_PATH);
        UINT wn = GetWindowsDirectoryW(env.windowsDir, MAX_PATH - 1);
        if (wn && wn < MAX_PATH - 1 && env.windowsDir[wn - 1] != L'\\') wcscat_s(env.windowsDir, L"\\");

        // a CrashDumps folder next to the loader or the game enables the feature
        wchar_t probe[MAX_PATH * 2];
        bool found = false;
        for (const wchar_t* dir : { (const wchar_t*)env.loaderDir, (const wchar_t*)env.exeDir })
        {
            if (!dir[0]) continue;
            swprintf_s(probe, L"%sCrashDumps", dir);
            if (DirExists(probe))
            {
                swprintf_s(g.folder, L"%s\\", probe);
                found = true;
                break;
            }
        }
        if (!found) return false;

        env.fullMemory = settings.fullMemory;
        env.zip = settings.zip;
        env.maxReports = settings.maxReports;
        g.iniPaths = settings.iniPaths;
        g.onCrash = settings.onCrash;
        env.dbghelp.Load(); // without it the text report still works, minus symbols

        g.log = (char*)VirtualAlloc(nullptr, kLogCapacity, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        g.request = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        g.done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        g.reporter = CreateThread(nullptr, 1024 * 1024, Reporter, nullptr, STACK_SIZE_PARAM_IS_A_RESERVATION, &g.reporterId);
        if (g.reporter) NameThread(g.reporter, L"UAL crash reporter");

        // the filter and reporter thread run our code until process exit
        HMODULE pinned;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, (LPCWSTR)&Filter, &pinned);
        SetUnhandledExceptionFilter(Filter);
        LockFilter();

        if (HANDLE hk = CreateThread(nullptr, 0, Housekeeping, nullptr, CREATE_SUSPENDED, nullptr))
        {
            SetThreadPriority(hk, THREAD_PRIORITY_LOWEST);
            NameThread(hk, L"UAL crash report housekeeping");
            ResumeThread(hk);
            CloseHandle(hk);
        }
        g.installed = true;
        return true;
    }

    const wchar_t* Folder()
    {
        return g.installed ? g.folder : L"";
    }
}
