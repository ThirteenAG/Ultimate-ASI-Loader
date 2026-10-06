// Identity of the loader module and the settings read from its ini files.
#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace ual
{
    struct LoaderInfo
    {
        HMODULE module = nullptr;
        uintptr_t imageBegin = 0, imageEnd = 0;
        std::wstring path;
        std::wstring name;     // e.g. "dinput8.dll", the proxied DLL
        std::wstring stem;     // "dinput8"
        std::wstring dir;      // trailing backslash
        std::wstring exePath;
        std::wstring exeDir;   // game folder, trailing backslash
    };

    const LoaderInfo& Self();
    void InitSelf(HMODULE module);

    // True for calls made by the loader itself.
    inline bool IsSelfAddress(const void* addr)
    {
        auto a = (uintptr_t)addr;
        const auto& s = Self();
        return a >= s.imageBegin && a < s.imageEnd;
    }

    // True if this thread holds the loader lock, e.g. inside DllMain or a TLS callback.
    bool LoaderLockHeldByThisThread();

    struct Settings
    {
        // [GlobalSets]
        bool loadPlugins = true;
        bool loadFromScriptsOnly = false;
        bool loadRecursively = true;
        bool dontLoadFromDllMain = true;
        std::wstring loadFromAPI;
        std::wstring loadExtraPlugins = L"modloader\\modloader.asi";
        bool useD3D8to9 = false;
        bool d3d8DisableMaximizedWindowedModeShim = false;
        bool modernUI = true; // own dialog window instead of task dialogs
        bool debugLog = false; // <loader name>.log next to the loader
        bool cxxHotReload = false; // reload .cxx snippets when their files change
        bool disableCrashDumps = false;
        bool crashDumpFullMemory = false;
        bool crashDumpZip = true;
        int crashDumpMaxReports = 10;
        // [FileLoader]
        std::wstring overloadFromFolder = L"update";
    };

    // <loader>.ini, global.ini, scripts\global.ini, plugins\global.ini, update\global.ini; later files win.
    const std::vector<std::wstring>& IniPaths();
    const Settings& GetSettings();
    void LoadSettings();

    // Last ini file that has the key wins.
    int IniInt(const wchar_t* section, const wchar_t* key, int defaultValue);
    std::wstring IniString(const wchar_t* section, const wchar_t* key, const std::wstring& defaultValue);
    int IniInt(const std::vector<std::wstring>& files, const wchar_t* section, const wchar_t* key, int defaultValue);
    std::wstring IniString(const std::vector<std::wstring>& files, const wchar_t* section, const wchar_t* key, const std::wstring& defaultValue);
}
