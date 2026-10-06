#include "plugins.hpp"
#include "../core/log.hpp"
#include "../core/loader.hpp"
#include "../core/paths.hpp"
#include "../core/strings.hpp"
#include "../ui/dialogs.hpp"
#include "../cxx/snippets.hpp"
#include <windows.h>
#include <algorithm>
#include <unordered_set>
#ifndef _WIN64
#include "../../compat/wndmode/wndmode.hpp"
#endif

namespace ual::plugins
{
    namespace
    {
        std::unordered_set<HMODULE> g_initialized; // modules whose InitializeASI was called

        void CallInitializeASI(HMODULE m)
        {
            if (auto init = (void(*)())GetProcAddress(m, "InitializeASI")) init();
        }

        // dir has a trailing backslash; not recursive
        void LoadAsiFiles(const std::wstring& dir)
        {
            WIN32_FIND_DATAW fd;
            HANDLE h = FindFirstFileExW((dir + L"*.asi").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
            if (h == INVALID_HANDLE_VALUE) return;
            std::vector<std::wstring> names;
            do
            {
                // "*.asi" also matches "*.asix" through 8.3 names
                if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && IEndsWith(fd.cFileName, L".asi")) names.push_back(fd.cFileName);
            } while (FindNextFileW(h, &fd));
            FindClose(h);

            for (const auto& name : names)
            {
                auto path = dir + name;
                if (HMODULE loaded = GetModuleHandleW(path.c_str()))
                {
                    // mapped already as another plugin's import (or by a plugin manager): its DllMain saw the loader on
                    // the stack and left the work to InitializeASI, so call it, once per module
                    if (g_initialized.insert(loaded).second)
                    {
                        UAL_LOG("plugin %ls: already loaded, calling InitializeASI", path.c_str());
                        SetCurrentDirectoryW(dir.c_str());
                        CallInitializeASI(loaded);
                    }
                    continue;
                }
                SetCurrentDirectoryW(dir.c_str());            // plugins expect their own folder
                HMODULE m = LoadLibraryW(path.c_str());
                DWORD error = m ? 0 : GetLastError();
                SetCurrentDirectoryW(dir.c_str()); // the plugin may have changed it
                UAL_LOG("plugin %ls: %s%s", path.c_str(), m ? "loaded" : "error ", m ? "" : std::to_string(error).c_str());
                if (m)
                {
                    g_initialized.insert(m);
                    CallInitializeASI(m);
                }
                else if (error != ERROR_DLL_INIT_FAILED && error != ERROR_BAD_EXE_FORMAT) // DllMain returned FALSE / other architecture
                    ui::ShowPluginLoadError(name, error);
            }
        }

        // *.asi first, then *.cxx snippets
        void LoadFolder(std::wstring dir)
        {
            if (!dir.empty() && dir.back() != L'\\' && dir.back() != L'/') dir += L'\\';
            LoadAsiFiles(dir);
            SetCurrentDirectoryW(dir.c_str()); // snippets expect their own folder too
            cxx::LoadFolder(dir);
        }

        // with LoadRecursively, direct sub folders load before dir itself
        void LoadTree(const std::wstring& dir, bool recursive)
        {
            if (!DirectoryExists(dir)) return;
            if (recursive)
            {
                WIN32_FIND_DATAW fd;
                HANDLE h = FindFirstFileExW((dir + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchLimitToDirectories, nullptr, FIND_FIRST_EX_LARGE_FETCH);
                std::vector<std::wstring> subs;
                if (h != INVALID_HANDLE_VALUE)
                {
                    do
                    {
                        if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && wcscmp(fd.cFileName, L".") && wcscmp(fd.cFileName, L".."))
                            subs.push_back(dir + L"\\" + fd.cFileName);
                    } while (FindNextFileW(h, &fd));
                    FindClose(h);
                }
                for (const auto& s : subs) LoadFolder(s);
            }
            LoadFolder(dir);
        }

#ifndef _WIN64
        // wndmode.ini next to the loader enables windowed mode. An empty file gets the default settings.
        void InitWindowMode()
        {
            constexpr int kDefaultIniResource = 104;
            const auto& self = Self();
            auto ini = self.dir + L"wndmode.ini";
            WIN32_FILE_ATTRIBUTE_DATA a;
            if (!GetFileAttributesExW(ini.c_str(), GetFileExInfoStandard, &a) || (a.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) return;
            if (a.nFileSizeLow == 0 && a.nFileSizeHigh == 0)
                if (HRSRC res = FindResourceW(self.module, MAKEINTRESOURCEW(kDefaultIniResource), RT_RCDATA))
                    if (HGLOBAL data = LoadResource(self.module, res))
                    {
                        HANDLE f = CreateFileW(ini.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
                        if (f != INVALID_HANDLE_VALUE)
                        {
                            DWORD written;
                            WriteFile(f, LockResource(data), SizeofResource(self.module, res), &written, nullptr);
                            CloseHandle(f);
                        }
                    }
            wndmode::Initialize(ini);
        }
#endif
    }

    void LoadAll(const std::vector<std::wstring>& extraFolders)
    {
        const auto& self = Self();
        const auto& s = GetSettings();
        auto previousDir = CurrentDirectory();
        auto selfDir = self.dir.substr(0, self.dir.size() - 1);

#ifndef _WIN64
        InitWindowMode();
#endif
        if (s.loadPlugins)
        {
            // relative to the loader's folder, without the per-plugin directory change (old behaviour)
            for (const auto& extra : SplitList(s.loadExtraPlugins))
            {
                SetCurrentDirectoryW(selfDir.c_str());
                // once per module: the same file is skipped when its folder is scanned below
                if (HMODULE m = LoadLibraryW(extra.c_str()))
                    if (g_initialized.insert(m).second) CallInitializeASI(m);
            }
            if (!s.loadFromScriptsOnly) LoadFolder(selfDir);
            LoadTree(self.dir + L"scripts", s.loadRecursively);
            LoadTree(self.dir + L"plugins", s.loadRecursively);
            for (const auto& dir : extraFolders) LoadTree(dir, s.loadRecursively);
        }
        cxx::StartHotReload(); // now that every snippet folder is known
        SetCurrentDirectoryW(previousDir.c_str());
    }
}
