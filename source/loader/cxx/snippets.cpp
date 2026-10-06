#include "snippets.hpp"
#include "cxxsnippets/cxxsnippets.hpp"
#include "../core/loader.hpp"
#include "../core/log.hpp"
#include "../core/paths.hpp"
#include "../core/strings.hpp"
#include "../crash/crashdump.hpp"
#include "../ui/dialogs.hpp"
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <thread>
#include <vector>

namespace ual::cxx
{
    namespace
    {
        struct Snippet
        {
            std::wstring path;
            std::unique_ptr<cxxsnippets::Module> module;              // running version, may be null
            std::vector<std::unique_ptr<cxxsnippets::Module>> retired; // unloaded, but their code stays mapped
        };

        // Leaked on purpose. Snippets aren't stopped at process exit, when their hooks and DLLs may already be gone.
        SRWLOCK g_lock = SRWLOCK_INIT;
        auto& g_snippets = *new std::vector<std::unique_ptr<Snippet>>();
        auto& g_folders = *new std::vector<std::wstring>(); // watched with CxxHotReload

        std::wstring ShortName(const std::wstring& path)
        {
            const auto& game = Self().exeDir;
            return IStartsWith(path, game) ? path.substr(game.size()) : path;
        }

        std::wstring Describe(const cxxsnippets::SourceLocation& where)
        {
            if (where.file.empty()) return L"an unknown line";
            std::wstring text = ShortName(Utf8ToWide(where.file)) + L" line " + std::to_wstring(where.line);
            if (!where.function.empty()) text += L", in " + Utf8ToWide(where.function);
            return text;
        }

        void ReportErrors(const std::wstring& path, const std::vector<cxxsnippets::Diagnostic>& errors, bool reload)
        {
            std::wstring details;
            for (const auto& e : errors)
            {
                if (!details.empty()) details += L"\n";
                details += ShortName(Utf8ToWide(e.file)) + L"(" + std::to_wstring(e.line) + L"," + std::to_wstring(e.column) + L"): " + Utf8ToWide(e.message);
            }
            log::EnsureOpen(Self().dir + Self().stem + L".log");
            UAL_LOG("snippet %ls: %s", ShortName(path).c_str(), WideToUtf8(details).c_str());
            std::wstring header = (reload ? L"Unable to reload " : L"Unable to load ") + FileNameOf(path);
            if (reload) details += L"\n\nThe previous version keeps running.";
            if (!reload) return ui::ShowError(header, details);
            // don't block the reload thread
            std::thread([header, details] { ui::ShowError(header, details); }).detach();
        }

        void ReportCrash(const std::wstring& path, const char* where, const cxxsnippets::Crash& crash)
        {
            wchar_t code[16];
            swprintf_s(code, L"0x%08lX", crash.code);
            std::wstring details = std::wstring(L"Exception ") + code + L" at " + Describe(crash.where) + L".\n\nThe snippet was unloaded.";
            log::EnsureOpen(Self().dir + Self().stem + L".log");
            UAL_LOG("snippet %ls crashed in %s: %s", ShortName(path).c_str(), where, WideToUtf8(details).c_str());
            ui::ShowError(FileNameOf(path) + L" crashed in " + Utf8ToWide(where), details);
        }

        // called from crash reports and the exception handler, so it must not wait
        bool Locate(const void* address, std::wstring& path, cxxsnippets::SourceLocation& where)
        {
            if (!TryAcquireSRWLockShared(&g_lock)) return false;
            bool found = false;
            for (const auto& s : g_snippets)
            {
                if (s->module && s->module->Locate(address, where)) found = true;
                for (const auto& r : s->retired)
                    if (!found && r->Locate(address, where)) found = true;
                if (found)
                {
                    path = s->path;
                    break;
                }
            }
            ReleaseSRWLockShared(&g_lock);
            return found;
        }

        bool DescribeForCrashReport(const void* address, char* out, size_t size)
        {
            std::wstring path;
            cxxsnippets::SourceLocation where;
            if (!Locate(address, path, where)) return false;
            snprintf(out, size, "%s", WideToUtf8(Describe(where)).c_str());
            return true;
        }

        LONG CALLBACK FaultLogger(EXCEPTION_POINTERS* e)
        {
            switch (e->ExceptionRecord->ExceptionCode)
            {
            case EXCEPTION_ACCESS_VIOLATION:
            case EXCEPTION_ILLEGAL_INSTRUCTION:
            case EXCEPTION_PRIV_INSTRUCTION:
            case EXCEPTION_INT_DIVIDE_BY_ZERO:
            case EXCEPTION_STACK_OVERFLOW:
            case EXCEPTION_DATATYPE_MISALIGNMENT:
            case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: break;
            default: return EXCEPTION_CONTINUE_SEARCH;
            }
            static thread_local bool inside = false; // guards against a fault while logging
            if (inside) return EXCEPTION_CONTINUE_SEARCH;
            inside = true;
            std::wstring path;
            cxxsnippets::SourceLocation where;
            if (Locate(e->ExceptionRecord->ExceptionAddress, path, where))
            {
                log::EnsureOpen(Self().dir + Self().stem + L".log");
                UAL_LOG("exception 0x%08lX in snippet code: %s", e->ExceptionRecord->ExceptionCode, WideToUtf8(Describe(where)).c_str());
            }
            inside = false;
            return EXCEPTION_CONTINUE_SEARCH; // the game or the crash handler decides
        }

        void InstallCrashReporting()
        {
            static bool installed = false;
            if (installed) return;
            installed = true;
            crashdump::SetCodeDescriber(DescribeForCrashReport);
            AddVectoredExceptionHandler(0, FaultLogger);
        }

        cxxsnippets::Options MakeOptions()
        {
            cxxsnippets::Options o;
            o.defines.push_back({ "UAL_SNIPPET", "1" });
            return o;
        }

        // Null on failure, which is already reported.
        std::unique_ptr<cxxsnippets::Module> Start(const std::wstring& path, bool reload)
        {
            std::vector<cxxsnippets::Diagnostic> errors;
            auto module = cxxsnippets::CompileFile(WideToUtf8(path), MakeOptions(), errors);
            if (!module)
            {
                ReportErrors(path, errors, reload);
                return nullptr;
            }
            cxxsnippets::Crash crash;
            if (!module->RunInit(&crash) && crash.code)
            {
                module->Unload();
                ReportCrash(path, "Init", crash);
                return nullptr;
            }
            UAL_LOG("snippet %ls: %s (%zu bytes of code)", ShortName(path).c_str(), reload ? "reloaded" : "loaded", module->CodeSize());
            return module;
        }

        void Stop(Snippet& s)
        {
            if (!s.module) return;
            cxxsnippets::Crash crash;
            s.module->RunShutdown(&crash);
            if (crash.code) ReportCrash(s.path, "Shutdown", crash);
            s.module->Unload();
            s.retired.push_back(std::move(s.module)); // keep mapped, game threads may still be in it
        }

        void LoadFile(const std::wstring& path)
        {
            InstallCrashReporting();
            auto module = Start(path, false);
            AcquireSRWLockExclusive(&g_lock);
            auto s = std::make_unique<Snippet>();
            s->path = path;
            s->module = std::move(module);
            g_snippets.push_back(std::move(s));
            ReleaseSRWLockExclusive(&g_lock);
        }

        Snippet* Find(const std::wstring& path)
        {
            for (auto& s : g_snippets)
                if (IEquals(s->path, path)) return s.get();
            return nullptr;
        }

        // on the watcher thread
        void Reload(const std::wstring& path)
        {
            bool exists = FileExists(path);
            Snippet* existing = nullptr;
            AcquireSRWLockShared(&g_lock);
            existing = Find(path);
            ReleaseSRWLockShared(&g_lock);
            if (!exists)
            {
                if (!existing || !existing->module) return;
                UAL_LOG("snippet %ls: removed, unloading", ShortName(path).c_str());
                AcquireSRWLockExclusive(&g_lock);
                Stop(*existing);
                ReleaseSRWLockExclusive(&g_lock);
                return;
            }
            if (!existing)
            {
                LoadFile(path);
                return;
            }
            // don't touch the running version until the new one compiles
            std::vector<cxxsnippets::Diagnostic> errors;
            if (!cxxsnippets::CheckFile(WideToUtf8(path), MakeOptions(), errors))
            {
                ReportErrors(path, errors, existing->module != nullptr);
                return;
            }
            AcquireSRWLockExclusive(&g_lock);
            Stop(*existing);
            ReleaseSRWLockExclusive(&g_lock);
            auto module = Start(path, true);
            AcquireSRWLockExclusive(&g_lock);
            existing->module = std::move(module);
            ReleaseSRWLockExclusive(&g_lock);
        }

        bool IsSnippetFile(const std::wstring& name)
        {
            return IEndsWith(name, L".cxx");
        }

        bool IsHeaderFile(const std::wstring& name)
        {
            return IEndsWith(name, L".h") || IEndsWith(name, L".hpp") || IEndsWith(name, L".inl");
        }

        void Watch()
        {
            struct Watched
            {
                std::wstring dir;
                HANDLE handle;
                OVERLAPPED ov{};
                alignas(DWORD) BYTE buffer[16 * 1024];
            };
            std::vector<std::unique_ptr<Watched>> watched;
            std::vector<HANDLE> events;
            auto arm = [](Watched& w) {
                ResetEvent(w.ov.hEvent);
                return ReadDirectoryChangesW(w.handle, w.buffer, sizeof(w.buffer), FALSE, FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SIZE,
                                             nullptr, &w.ov, nullptr) != FALSE;
            };
            AcquireSRWLockShared(&g_lock);
            auto folders = g_folders;
            ReleaseSRWLockShared(&g_lock);
            for (const auto& dir : folders)
            {
                auto w = std::make_unique<Watched>();
                w->dir = dir;
                w->handle = CreateFileW(dir.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
                if (w->handle == INVALID_HANDLE_VALUE) continue;
                w->ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
                if (!arm(*w)) continue;
                events.push_back(w->ov.hEvent);
                watched.push_back(std::move(w));
            }
            UAL_LOG("snippet hot reload: watching %zu folder(s)", watched.size());
            while (!events.empty())
            {
                DWORD r = WaitForMultipleObjects((DWORD)events.size(), events.data(), FALSE, INFINITE);
                if (r >= WAIT_OBJECT_0 + events.size()) return;
                // editors save in several steps, so collect changes until it's quiet
                std::vector<std::wstring> changed;
                for (;;)
                {
                    for (auto& w : watched)
                    {
                        if (WaitForSingleObject(w->ov.hEvent, 0) != WAIT_OBJECT_0) continue;
                        DWORD bytes = 0;
                        if (GetOverlappedResult(w->handle, &w->ov, &bytes, FALSE) && bytes)
                            for (auto info = (FILE_NOTIFY_INFORMATION*)w->buffer;; info = (FILE_NOTIFY_INFORMATION*)((BYTE*)info + info->NextEntryOffset))
                            {
                                std::wstring name(info->FileName, info->FileNameLength / sizeof(wchar_t));
                                std::wstring full = w->dir + L"\\" + name;
                                if (IsSnippetFile(name))
                                {
                                    if (std::none_of(changed.begin(), changed.end(), [&](const std::wstring& c) { return IEquals(c, full); })) changed.push_back(full);
                                }
                                else if (IsHeaderFile(name)) // reload every snippet in the folder
                                {
                                    AcquireSRWLockShared(&g_lock);
                                    for (const auto& s : g_snippets)
                                        if (IEquals(ParentDirectory(s->path), w->dir + L"\\") &&
                                            std::none_of(changed.begin(), changed.end(), [&](const std::wstring& c) { return IEquals(c, s->path); }))
                                            changed.push_back(s->path);
                                    ReleaseSRWLockShared(&g_lock);
                                }
                                if (!info->NextEntryOffset) break;
                            }
                        arm(*w);
                    }
                    if (WaitForMultipleObjects((DWORD)events.size(), events.data(), FALSE, 300) == WAIT_TIMEOUT) break;
                }
                for (const auto& path : changed)
                {
                    // the editor may still hold the file
                    for (int i = 0; i < 20; ++i)
                    {
                        HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
                        if (h != INVALID_HANDLE_VALUE || GetLastError() == ERROR_FILE_NOT_FOUND)
                        {
                            if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
                            break;
                        }
                        Sleep(50);
                    }
                    Reload(path);
                }
            }
        }

    }

    void StartHotReload()
    {
        static bool started = false;
        if (started || !GetSettings().cxxHotReload) return;
        started = true;
        // plugins::LoadAll calls this after the last folder, so Watch() sees every folder
        std::thread(Watch).detach();
    }

    void LoadFolder(const std::wstring& dir)
    {
        std::wstring base = dir;
        while (!base.empty() && (base.back() == L'\\' || base.back() == L'/')) base.pop_back();
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileExW((base + L"\\*.cxx").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
        if (h == INVALID_HANDLE_VALUE)
        {
            if (GetSettings().cxxHotReload && DirectoryExists(base)) // watch for snippets added later
            {
                AcquireSRWLockExclusive(&g_lock);
                if (std::none_of(g_folders.begin(), g_folders.end(), [&](const std::wstring& f) { return IEquals(f, base); })) g_folders.push_back(base);
                ReleaseSRWLockExclusive(&g_lock);
            }
            return;
        }
        std::vector<std::wstring> names;
        do
        {
            // "*.cxx" also matches longer extensions through 8.3 names
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && IEndsWith(fd.cFileName, L".cxx")) names.push_back(fd.cFileName);
        } while (FindNextFileW(h, &fd));
        FindClose(h);
        std::sort(names.begin(), names.end(), [](const std::wstring& a, const std::wstring& b) { return CompareStringOrdinal(a.c_str(), -1, b.c_str(), -1, TRUE) == CSTR_LESS_THAN; });
        for (const auto& name : names)
        {
            auto path = base + L"\\" + name;
            AcquireSRWLockShared(&g_lock);
            bool known = Find(path) != nullptr;
            ReleaseSRWLockShared(&g_lock);
            if (!known) LoadFile(path);
        }
        AcquireSRWLockExclusive(&g_lock);
        if (std::none_of(g_folders.begin(), g_folders.end(), [&](const std::wstring& f) { return IEquals(f, base); })) g_folders.push_back(base);
        ReleaseSRWLockExclusive(&g_lock);
    }
}
