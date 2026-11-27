// A separate reporter thread writes the report while the crashed thread waits. That works after a
// stack overflow and calls MiniDumpWriteDump from outside the crashed thread, as Microsoft recommends.
// Per crash: <exe>.<yyyymmdd_hhmmss>.log and .dmp, plus a .zip with both to attach to bug reports
// (no .zip with CrashDumpZip=0).
#include "crashdump.hpp"
#include "report.hpp"
#include "../core/hooks.hpp"
#include "../core/pe.hpp"
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
            wchar_t folder[MAX_PATH * 2] = {}; // with trailing backslash; loaderDir/exeDir + "CrashDumps\" can exceed MAX_PATH
            HANDLE reporter = nullptr;
            DWORD reporterId = 0;
            volatile LONG reporterStarted = 0;
            HANDLE housekeeping = nullptr;
            DWORD housekeepingId = 0;
            volatile LONG housekeepingStarted = 0;
            HANDLE request = nullptr;
            HANDLE done = nullptr;
            volatile LONG crashing = 0;
            volatile LONG progress = 0; // bumped by the reporter as it writes, see WaitForReport
            DWORD warmUpThread = 0;     // see WarmUpDump
            HANDLE instance = nullptr;  // see Install
            CrashInfo crash;
            char* log = nullptr;
            bool installed = false;
            void (*onCrash)() = nullptr;
            void* volatile gameFilter = nullptr; // LPTOP_LEVEL_EXCEPTION_FILTER the game set (or the one before ours), called first
        } g;

        // The exception whose game filter is running on this thread; a process exit from in there gets reported first.
        // A Win32 TLS slot, not thread_local: protection code starts threads without thread attach (MTLX.dll's), and
        // those have no thread_local storage, so reading it from them faults inside the crash handler.
        DWORD g_gameFilterSlot = TLS_OUT_OF_INDEXES;

        EXCEPTION_POINTERS* GameFilterException()
        {
            return g_gameFilterSlot == TLS_OUT_OF_INDEXES ? nullptr : (EXCEPTION_POINTERS*)TlsGetValue(g_gameFilterSlot);
        }

        void SetGameFilterException(EXCEPTION_POINTERS* ep)
        {
            if (g_gameFilterSlot != TLS_OUT_OF_INDEXES) TlsSetValue(g_gameFilterSlot, ep);
        }

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

        // Whether this thread is inside the loader: it owns the loader lock (DllMain, TLS callbacks) or the current load
        // (Windows 10+, which sends DLL load notifications with that). Another thread can't load a DLL until it leaves.
        bool InsideLoader()
        {
            auto teb = (const BYTE*)NtCurrentTeb();
#ifdef _WIN64
            auto peb = *(const BYTE* const*)(teb + 0x60);
            auto lock = *(RTL_CRITICAL_SECTION* const*)(peb + 0x110);
            WORD sameTebFlags = *(const WORD*)(teb + 0x17EE);
#else
            auto peb = *(const BYTE* const*)(teb + 0x30);
            auto lock = *(RTL_CRITICAL_SECTION* const*)(peb + 0xA0);
            WORD sameTebFlags = *(const WORD*)(teb + 0xFCA);
#endif
            constexpr WORD kLoadOwner = 0x1000, kLoaderWorker = 0x2000;
            if (lock && (DWORD)(ULONG_PTR)lock->OwningThread == GetCurrentThreadId()) return true;
            return (sameTebFlags & (kLoadOwner | kLoaderWorker)) != 0;
        }

        // Inside the loader the reporter can't recover from a wait on its locks, so it is given up once it makes no
        // progress for a while. Otherwise the usual timeout applies. MiniDumpWriteDump suspends this thread while it
        // collects, which is why nothing in there may load a DLL (see HookDumpLoads, WarmUpDump).
        void WaitForReport()
        {
            if (!g.crash.insideLoader)
            {
                WaitForSingleObject(g.done, Timeout());
                return;
            }
            constexpr DWORD kStall = 15000;
            LONG seen = g.progress;
            ULONGLONG last = GetTickCount64(), start = last;
            while (WaitForSingleObject(g.done, 500) == WAIT_TIMEOUT)
            {
                ULONGLONG now = GetTickCount64();
                if (LONG p = g.progress; p != seen)
                {
                    seen = p;
                    last = now;
                }
                if (now - last > kStall || now - start > Timeout()) return;
            }
        }

        // param: non-null for the warm-up dump, which leaves out all threads and modules
        BOOL CALLBACK DumpProgress(PVOID param, const PMINIDUMP_CALLBACK_INPUT input, PMINIDUMP_CALLBACK_OUTPUT)
        {
            InterlockedIncrement(&g.progress);
            switch (input->CallbackType)
            {
            case IncludeThreadCallback:
            case IncludeModuleCallback:
                return param == nullptr;
            case ThreadCallback:
            case ThreadExCallback:
            case ModuleCallback:
                return TRUE;
            default:
                return FALSE;
            }
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

        // The minidump code calls LoadLibrary on every dump: for modules that are loaded anyway (ntdll.dll, psapi.dll,
        // which Install keeps loaded), for version.dll, powrprof.dll and verifier.dll, and as data files to read version
        // resources. After a crash inside the loader a load of a DLL that isn't loaded yet waits for the crashed thread,
        // which the dump code has suspended: the process hangs for good. Those DLLs only add details (module versions,
        // handle traces), so there such loads fail instead, and during the warm-up too, which leaves no DLL behind.
        bool DumpLoadFails(bool loaded, DWORD flags)
        {
            DWORD self = GetCurrentThreadId();
            if (self != g.warmUpThread && (!g.crash.insideLoader || self != g.reporterId)) return false;
            return !loaded && !(flags & (LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_DATAFILE_EXCLUSIVE | LOAD_LIBRARY_AS_IMAGE_RESOURCE));
        }

        decltype(&LoadLibraryA) oDumpLoadLibraryA = nullptr;
        decltype(&LoadLibraryW) oDumpLoadLibraryW = nullptr;
        decltype(&LoadLibraryExA) oDumpLoadLibraryExA = nullptr;
        decltype(&LoadLibraryExW) oDumpLoadLibraryExW = nullptr;

        HMODULE DumpLoadFailed()
        {
            SetLastError(ERROR_MOD_NOT_FOUND);
            return nullptr;
        }

        HMODULE WINAPI DumpLoadLibraryA(LPCSTR name)
        {
            return DumpLoadFails(name && GetModuleHandleA(name), 0) ? DumpLoadFailed() : oDumpLoadLibraryA(name);
        }

        HMODULE WINAPI DumpLoadLibraryW(LPCWSTR name)
        {
            return DumpLoadFails(name && GetModuleHandleW(name), 0) ? DumpLoadFailed() : oDumpLoadLibraryW(name);
        }

        HMODULE WINAPI DumpLoadLibraryExA(LPCSTR name, HANDLE file, DWORD flags)
        {
            return DumpLoadFails(name && GetModuleHandleA(name), flags) ? DumpLoadFailed() : oDumpLoadLibraryExA(name, file, flags);
        }

        HMODULE WINAPI DumpLoadLibraryExW(LPCWSTR name, HANDLE file, DWORD flags)
        {
            return DumpLoadFails(name && GetModuleHandleW(name), flags) ? DumpLoadFailed() : oDumpLoadLibraryExW(name, file, flags);
        }

        // The import slots of the module that has MiniDumpWriteDump: dbgcore.dll, or dbghelp.dll on older Windows.
        void HookDumpLoads()
        {
            HMODULE m = nullptr;
            if (!g.env.dbghelp.MiniDumpWriteDump ||
                !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)g.env.dbghelp.MiniDumpWriteDump, &m))
                return;
            ual::pe::Image image(m);
            if (!image.Valid()) return;
            ual::pe::ForEachImport(image, [](const ual::pe::ImportSlot& s) {
                if (!s.name) return true;
                struct
                {
                    const char* name;
                    void* hook;
                    void** original;
                } hooks[] = {
                    { "LoadLibraryA", (void*)DumpLoadLibraryA, (void**)&oDumpLoadLibraryA },
                    { "LoadLibraryW", (void*)DumpLoadLibraryW, (void**)&oDumpLoadLibraryW },
                    { "LoadLibraryExA", (void*)DumpLoadLibraryExA, (void**)&oDumpLoadLibraryExA },
                    { "LoadLibraryExW", (void*)DumpLoadLibraryExW, (void**)&oDumpLoadLibraryExW },
                };
                for (auto& h : hooks)
                    if (!strcmp(s.name, h.name) && *s.slot != h.hook)
                    {
                        if (!*h.original) *h.original = *s.slot;
                        ual::pe::WritePointer(s.slot, h.hook);
                    }
                return true;
            });
        }

        MINIDUMP_TYPE DumpType(bool fullMemory)
        {
            int common = MiniDumpWithHandleData | MiniDumpWithUnloadedModules | MiniDumpWithThreadInfo | MiniDumpWithFullMemoryInfo |
                         MiniDumpWithProcessThreadData;
            return (MINIDUMP_TYPE)(fullMemory ? common | MiniDumpWithFullMemory
                                                    : common | MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory | MiniDumpIgnoreInaccessibleMemory);
        }

        // The dump code resolves delay-loaded imports when it first runs (rpcrt4's UuidCreate and others). After a crash
        // inside the loader those loads would wait for the crashed thread, so a dump without threads and modules is
        // written to a temporary file at install. That runs on the installing thread: on another thread a crash during
        // it could leave dbghelp's lock held by a load waiting for the crashed thread.
        void WarmUpDump()
        {
            auto writeDump = g.env.dbghelp.MiniDumpWriteDump;
            wchar_t dir[MAX_PATH], path[MAX_PATH];
            if (!writeDump || !GetTempPathW(MAX_PATH, dir) || !GetTempFileNameW(dir, L"ual", 0, path)) return;
            HANDLE f = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
            if (f == INVALID_HANDLE_VALUE)
            {
                DeleteFileW(path);
                return;
            }
            MINIDUMP_CALLBACK_INFORMATION callback{ DumpProgress, (void*)1 };
            g.warmUpThread = GetCurrentThreadId();
            __try
            {
                writeDump(GetCurrentProcess(), GetCurrentProcessId(), f, DumpType(false), nullptr, nullptr, &callback);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
            g.warmUpThread = 0;
            CloseHandle(f);
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
            MINIDUMP_CALLBACK_INFORMATION callback{ DumpProgress, nullptr };
            BOOL ok = writeDump(GetCurrentProcess(), GetCurrentProcessId(), f, DumpType(g.env.fullMemory), &mei, &streams, &callback);
            if (!ok)
            {
                // older dbghelp versions reject some flags
                SetFilePointer(f, 0, nullptr, FILE_BEGIN);
                SetEndOfFile(f);
                ok = writeDump(GetCurrentProcess(), GetCurrentProcessId(), f, (MINIDUMP_TYPE)(MiniDumpWithDataSegs | MiniDumpWithHandleData), &mei, &streams, &callback);
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

        // Packs .dmp and .log into <base>.zip. Both stay next to it.
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
            ok = ok && mz_zip_writer_finalize_archive(&z);
            mz_zip_writer_end(&z);
            ok = (fclose(out) == 0) && ok;
            if (ok && MoveFileExW(tmp.c_str(), zip.c_str(), MOVEFILE_REPLACE_EXISTING)) return true;
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
            InterlockedExchange(&g.housekeepingStarted, 1);
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
            InterlockedIncrement(&g.progress);
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

        // Writes the report once per process; a later caller waits until it is done.
        void ReportCrash(EXCEPTION_POINTERS* ep)
        {
            if (g.onCrash) g.onCrash(); // e.g. make the file hooks pass through: the reporter must not block on their locks
            if (InterlockedCompareExchange(&g.crashing, 1, 0) != 0)
            {
                // another thread is already reporting (or this one already did), wait for it
                if (g.done) WaitForReport();
                return;
            }
            // A crash right after startup can catch the low-priority housekeeping thread before or during its work;
            // with it suspended there by MiniDumpWriteDump, writing the dump could stall for good (seen with few
            // cores). Let it finish first. If it hasn't started, it may need the loader lock this thread holds (a
            // crash in a DllMain), so the wait is short then.
            if (g.housekeeping && GetCurrentThreadId() != g.housekeepingId)
            {
                SetThreadPriority(g.housekeeping, THREAD_PRIORITY_NORMAL);
                WaitForSingleObject(g.housekeeping, g.housekeepingStarted ? 5000 : 200);
            }
            g.crash.ep = ep;
            g.crash.threadId = GetCurrentThreadId();
            g.crash.insideLoader = InsideLoader();
            GetLocalTime(&g.crash.localTime);
            // the reporter thread can't start under the loader lock (crash during DLL init), so report from here
            bool alive = g.reporter && g.reporterStarted && WaitForSingleObject(g.reporter, 0) == WAIT_TIMEOUT;
            if (alive)
            {
                SetEvent(g.request);
                WaitForReport();
            }
            else
            {
                Report();
                if (g.done) SetEvent(g.done);
            }
            // games clip and hide the cursor
            ClipCursor(nullptr);
            ShowCursor(TRUE);
        }

        LONG CallGameFilter(LPTOP_LEVEL_EXCEPTION_FILTER filter, EXCEPTION_POINTERS* ep)
        {
            EXCEPTION_POINTERS* outer = GameFilterException();
            SetGameFilterException(ep);
            LONG result;
            __try
            {
                result = filter(ep);
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                result = EXCEPTION_CONTINUE_SEARCH; // a broken filter must not hide the crash
            }
            SetGameFilterException(outer);
            return result;
        }

        // The game's filter decides first: DRM and some engines raise exceptions on purpose and continue from their
        // filter. Whatever ends the process gets a report: the filter returning, or exiting from inside (hooks below).
        LONG WINAPI Filter(EXCEPTION_POINTERS* ep)
        {
            if (GetCurrentThreadId() == g.reporterId) return EXCEPTION_CONTINUE_SEARCH;
            // these can't be continued, and calling the game's filter can end the process out of reach (the CRT's
            // filter fail-fasts on C++ exceptions, a second stack overflow kills it): report first
            bool fatal = (ep->ExceptionRecord->ExceptionFlags & EXCEPTION_NONCONTINUABLE) || ep->ExceptionRecord->ExceptionCode == EXCEPTION_STACK_OVERFLOW;
            if (fatal) ReportCrash(ep);
            LONG result = EXCEPTION_CONTINUE_SEARCH;
            if (auto filter = (LPTOP_LEVEL_EXCEPTION_FILTER)g.gameFilter)
            {
                result = CallGameFilter(filter, ep);
                if (result == EXCEPTION_CONTINUE_EXECUTION) return result;
            }
            if (!fatal) ReportCrash(ep);
            return result;
        }

        // The game's (or a launcher's) SetUnhandledExceptionFilter must not replace ours. Its filter is remembered
        // instead and the game sees the usual return value; Filter calls it. Unless it handles the crash, the crash
        // still reaches Windows Error Reporting.
        decltype(&SetUnhandledExceptionFilter) oSetUnhandledExceptionFilter = nullptr;
        LPTOP_LEVEL_EXCEPTION_FILTER WINAPI hkSetUnhandledExceptionFilter(LPTOP_LEVEL_EXCEPTION_FILTER filter)
        {
            if (filter == Filter) return oSetUnhandledExceptionFilter(filter);
            return (LPTOP_LEVEL_EXCEPTION_FILTER)InterlockedExchangePointer((void* volatile*)&g.gameFilter, (void*)filter);
        }

        // A game's filter often shows its own crash message and exits the process. These pass through, except from
        // inside a game filter, where the crash is reported first.
        void ReportFromGameFilter()
        {
            if (EXCEPTION_POINTERS* ep = GameFilterException())
            {
                SetGameFilterException(nullptr);
                ReportCrash(ep);
            }
        }

        decltype(&ExitProcess) oExitProcess = nullptr;
        void WINAPI hkExitProcess(UINT code)
        {
            ReportFromGameFilter();
            oExitProcess(code);
        }

        decltype(&TerminateProcess) oTerminateProcess = nullptr;
        BOOL WINAPI hkTerminateProcess(HANDLE process, UINT code)
        {
            if (GameFilterException() && (process == GetCurrentProcess() || GetProcessId(process) == GetCurrentProcessId())) ReportFromGameFilter();
            return oTerminateProcess(process, code);
        }

        decltype(&RaiseFailFastException) oRaiseFailFastException = nullptr;
        void WINAPI hkRaiseFailFastException(PEXCEPTION_RECORD record, PCONTEXT context, DWORD flags)
        {
            ReportFromGameFilter();
            oRaiseFailFastException(record, context, flags);
        }

        LONG WINAPI ProbeFilter(EXCEPTION_POINTERS*)
        {
            return EXCEPTION_CONTINUE_SEARCH;
        }

        // The import slot kernel32's export jumps through into kernelbase: "jmp [slot]" on x86 (alone or after
        // "mov edi,edi / push ebp / mov ebp,esp / pop ebp"), "jmp [rip+x]" on x64. Null for any other code or a
        // forwarded export.
        void** Kernel32JumpSlot(const char* function)
        {
            auto base = (const BYTE*)GetModuleHandleW(L"kernel32.dll");
            auto code = (const BYTE*)GetProcAddress((HMODULE)base, function);
            if (!code) return nullptr;
            DWORD size = ((const IMAGE_NT_HEADERS*)(base + ((const IMAGE_DOS_HEADER*)base)->e_lfanew))->OptionalHeader.SizeOfImage;
            auto inside = [&](const void* p) { return p >= base && p < base + size; };
            if (!inside(code)) return nullptr;
#ifdef _WIN64
            static const BYTE prologue[] = { 0x48 };
#else
            static const BYTE prologue[] = { 0x8B, 0xFF, 0x55, 0x8B, 0xEC, 0x5D };
#endif
            for (size_t skip : { (size_t)0, sizeof(prologue) })
            {
                if (skip && memcmp(code, prologue, skip)) continue;
                const BYTE* jmp = code + skip;
                if (jmp[0] != 0xFF || jmp[1] != 0x25) continue;
#ifdef _WIN64
                auto slot = (void**)(jmp + 6 + *(const INT32*)(jmp + 2));
#else
                auto slot = *(void***)(jmp + 2);
#endif
                return inside(slot) ? slot : nullptr;
            }
            return nullptr;
        }

        bool ReplaceSlot(void** slot, void* value)
        {
            DWORD protect;
            if (!VirtualProtect(slot, sizeof(void*), PAGE_READWRITE, &protect)) return false;
            InterlockedExchangePointer(slot, value);
            VirtualProtect(slot, sizeof(void*), protect, &protect);
            return true;
        }

        // DRM such as Max Payne 3's rejects a patched SetUnhandledExceptionFilter, so where kernel32's export is a
        // stub, the import slot it jumps through is redirected instead of hooking code. That covers games, which
        // import it from kernel32. A test call makes sure it works; otherwise the export's code is hooked.
        void LockFilter()
        {
            if (void** slot = Kernel32JumpSlot("SetUnhandledExceptionFilter"))
            {
                void* original = *slot;
                oSetUnhandledExceptionFilter = (decltype(oSetUnhandledExceptionFilter))original;
                if (ReplaceSlot(slot, (void*)hkSetUnhandledExceptionFilter))
                {
                    auto exported = (decltype(&SetUnhandledExceptionFilter))GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "SetUnhandledExceptionFilter");
                    LPTOP_LEVEL_EXCEPTION_FILTER previous = exported(ProbeFilter);
                    if (g.gameFilter == (void*)ProbeFilter)
                        g.gameFilter = (void*)previous;
                    else
                    {
                        exported(previous); // the test call replaced ours
                        ReplaceSlot(slot, original);
                        oSetUnhandledExceptionFilter = nullptr;
                    }
                }
                else
                    oSetUnhandledExceptionFilter = nullptr;
            }
            // older Windows: the implementations live in kernel32
            for (const wchar_t* module : { L"kernelbase.dll", L"kernel32.dll" })
            {
                ual::InstallHooks({
                    { L"kernel32.dll", "SetUnhandledExceptionFilter", (void*)hkSetUnhandledExceptionFilter, (void**)&oSetUnhandledExceptionFilter },
                    { module, "TerminateProcess", (void*)hkTerminateProcess, (void**)&oTerminateProcess },
                    { module, "RaiseFailFastException", (void*)hkRaiseFailFastException, (void**)&oRaiseFailFastException },
                    { L"kernel32.dll", "ExitProcess", (void*)hkExitProcess, (void**)&oExitProcess },
                });
            }
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

        // Some installs have the loader under several names (d3d8.dll, dinput.dll, winmm.dll); the first copy reports
        // for the whole process. More would chain their filters and write one report each, one after the other.
        wchar_t instance[64];
        swprintf_s(instance, L"Local\\UltimateASILoader.CrashReports.%lu", GetCurrentProcessId());
        SetLastError(ERROR_SUCCESS);
        g.instance = CreateEventW(nullptr, TRUE, FALSE, instance);
        if (!g.instance) return false;
        if (GetLastError() == ERROR_ALREADY_EXISTS)
        {
            CloseHandle(g.instance);
            g.instance = nullptr;
            return false;
        }

        env.fullMemory = settings.fullMemory;
        env.zip = settings.zip;
        env.maxReports = settings.maxReports;
        g.onCrash = settings.onCrash;
        env.dbghelp.Load(); // without it the text report still works, minus symbols
        // the dump fails without psapi.dll (see DumpLoadFails); it is no proxy name and safe to keep loaded
        wchar_t psapi[MAX_PATH];
        UINT sn = GetSystemDirectoryW(psapi, MAX_PATH);
        if (sn && sn + 11 < MAX_PATH && !wcscat_s(psapi, L"\\psapi.dll")) LoadLibraryExW(psapi, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        HookDumpLoads();
        WarmUpDump();

        g_gameFilterSlot = TlsAlloc();
        g.log = (char*)VirtualAlloc(nullptr, kLogCapacity, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        g.request = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        g.done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        // After a stack overflow the crashed thread runs the exception dispatch and Filter on whatever stack is
        // left, which on x64 can be too little: a second fault there ends the process before the reporter writes
        // anything. Reserve room for that on the installing thread, usually the main thread.
        constexpr ULONG kStackGuarantee = 64 * 1024;
        ULONG guarantee = 0;
        if (SetThreadStackGuarantee(&guarantee) && guarantee < kStackGuarantee)
        {
            guarantee = kStackGuarantee;
            SetThreadStackGuarantee(&guarantee);
        }
        g.reporter = CreateThread(nullptr, 1024 * 1024, Reporter, nullptr, STACK_SIZE_PARAM_IS_A_RESERVATION, &g.reporterId);
        if (g.reporter) NameThread(g.reporter, L"UAL crash reporter");

        // the filter and reporter thread run our code until process exit
        HMODULE pinned;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, (LPCWSTR)&Filter, &pinned);
        // a filter set before ours (a protection stub, another loader) is chained like the game's
        g.gameFilter = (void*)SetUnhandledExceptionFilter(Filter);
        LockFilter();

        if (HANDLE hk = CreateThread(nullptr, 0, Housekeeping, nullptr, CREATE_SUSPENDED, &g.housekeepingId))
        {
            SetThreadPriority(hk, THREAD_PRIORITY_LOWEST);
            NameThread(hk, L"UAL crash report housekeeping");
            g.housekeeping = hk;
            ResumeThread(hk);
        }
        g.installed = true;
        return true;
    }

    const wchar_t* Folder()
    {
        return g.installed ? g.folder : L"";
    }
}
