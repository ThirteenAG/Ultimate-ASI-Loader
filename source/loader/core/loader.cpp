#include "loader.hpp"
#include "paths.hpp"
#include "strings.hpp"
#include <intrin.h>

namespace ual
{
    namespace
    {
        LoaderInfo g_self;
        std::vector<std::wstring> g_iniPaths;
        Settings g_settings;

        // GetPrivateProfileString truncates silently, so grow the buffer until the value fits.
        bool ReadIniString(const std::wstring& file, const wchar_t* section, const wchar_t* key, std::wstring& out)
        {
            static constexpr wchar_t kMissing[] = L"\x01\x02ual-missing\x02\x01";
            std::wstring buf(512, L'\0');
            for (;;)
            {
                DWORD n = GetPrivateProfileStringW(section, key, kMissing, buf.data(), (DWORD)buf.size(), file.c_str());
                if (n + 2 < buf.size() || buf.size() >= (1u << 20))
                {
                    buf.resize(n);
                    break;
                }
                buf.resize(buf.size() * 4);
            }
            if (buf == kMissing) return false;
            out = std::move(buf);
            return true;
        }
    }

    bool LoaderLockHeldByThisThread()
    {
        // PEB->LoaderLock
#ifdef _WIN64
        auto peb = reinterpret_cast<BYTE*>(__readgsqword(0x60));
        auto cs = *reinterpret_cast<RTL_CRITICAL_SECTION**>(peb + 0x110);
#else
        auto peb = reinterpret_cast<BYTE*>(__readfsdword(0x30));
        auto cs = *reinterpret_cast<RTL_CRITICAL_SECTION**>(peb + 0xA0);
#endif
        return cs && (DWORD)(ULONG_PTR)cs->OwningThread == GetCurrentThreadId();
    }

    const LoaderInfo& Self()
    {
        return g_self;
    }

    void InitSelf(HMODULE module)
    {
        g_self.module = module;
        auto dos = (const IMAGE_DOS_HEADER*)module;
        auto nt = (const IMAGE_NT_HEADERS*)((const BYTE*)module + dos->e_lfanew);
        g_self.imageBegin = (uintptr_t)module;
        g_self.imageEnd = g_self.imageBegin + nt->OptionalHeader.SizeOfImage;
        g_self.path = ModulePath(module);
        g_self.name = FileNameOf(g_self.path);
        g_self.stem = g_self.name.substr(0, g_self.name.find_last_of(L'.'));
        g_self.dir = ParentDirectory(g_self.path);
        g_self.exePath = ModulePath(nullptr);
        g_self.exeDir = ParentDirectory(g_self.exePath);

        g_iniPaths = { g_self.dir + g_self.stem + L".ini", g_self.dir + L"global.ini", g_self.dir + L"scripts\\global.ini",
                       g_self.dir + L"plugins\\global.ini", g_self.dir + L"update\\global.ini" };
    }

    const std::vector<std::wstring>& IniPaths()
    {
        return g_iniPaths;
    }

    int IniInt(const std::vector<std::wstring>& files, const wchar_t* section, const wchar_t* key, int defaultValue)
    {
        int v = defaultValue;
        for (const auto& f : files) v = (int)GetPrivateProfileIntW(section, key, v, f.c_str());
        return v;
    }

    std::wstring IniString(const std::vector<std::wstring>& files, const wchar_t* section, const wchar_t* key, const std::wstring& defaultValue)
    {
        std::wstring v = defaultValue;
        for (const auto& f : files) ReadIniString(f, section, key, v);
        return v;
    }

    int IniInt(const wchar_t* section, const wchar_t* key, int defaultValue)
    {
        return IniInt(g_iniPaths, section, key, defaultValue);
    }

    std::wstring IniString(const wchar_t* section, const wchar_t* key, const std::wstring& defaultValue)
    {
        return IniString(g_iniPaths, section, key, defaultValue);
    }

    const Settings& GetSettings()
    {
        return g_settings;
    }

    void LoadSettings()
    {
        Settings s;
        s.loadPlugins = IniInt(L"GlobalSets", L"LoadPlugins", s.loadPlugins) != 0;
        s.loadFromScriptsOnly = IniInt(L"GlobalSets", L"LoadFromScriptsOnly", s.loadFromScriptsOnly) != 0;
        s.loadRecursively = IniInt(L"GlobalSets", L"LoadRecursively", s.loadRecursively) != 0;
        s.dontLoadFromDllMain = IniInt(L"GlobalSets", L"DontLoadFromDllMain", s.dontLoadFromDllMain) != 0;
        s.loadFromAPI = Trim(IniString(L"GlobalSets", L"LoadFromAPI", s.loadFromAPI));
        s.loadExtraPlugins = IniString(L"GlobalSets", L"LoadExtraPlugins", s.loadExtraPlugins);
        s.useD3D8to9 = IniInt(L"GlobalSets", L"UseD3D8to9", s.useD3D8to9) != 0;
        s.d3d8DisableMaximizedWindowedModeShim =
            IniInt(L"GlobalSets", L"Direct3D8DisableMaximizedWindowedModeShim", s.d3d8DisableMaximizedWindowedModeShim) != 0;
        s.modernUI = IniInt(L"GlobalSets", L"ModernUI", s.modernUI) != 0;
        s.debugLog = IniInt(L"GlobalSets", L"DebugLog", s.debugLog) != 0;
        s.cxxHotReload = IniInt(L"GlobalSets", L"CxxHotReload", s.cxxHotReload) != 0;
        s.disableCrashDumps = IniInt(L"GlobalSets", L"DisableCrashDumps", s.disableCrashDumps) != 0;
        s.crashDumpFullMemory = IniInt(L"GlobalSets", L"CrashDumpFullMemory", s.crashDumpFullMemory) != 0;
        s.crashDumpZip = IniInt(L"GlobalSets", L"CrashDumpZip", s.crashDumpZip) != 0;
        s.crashDumpMaxReports = IniInt(L"GlobalSets", L"CrashDumpMaxReports", s.crashDumpMaxReports);
        s.overloadFromFolder = IniString(L"FileLoader", L"OverloadFromFolder", s.overloadFromFolder);
        // ScriptHookV / ScriptHookRDR2 expect plugins to load at this call
        if (s.loadFromAPI.empty())
        {
            auto exe = FileNameOf(g_self.exePath);
            exe = exe.substr(0, exe.find_last_of(L'.'));
            if (IEquals(exe, L"GTA5") || IEquals(exe, L"GTA5_Enhanced") || IEquals(exe, L"RDR2") || IEquals(exe, L"game_win64_master"))
                s.loadFromAPI = L"GetSystemTimeAsFileTime";
        }
        g_settings = std::move(s);
    }
}
