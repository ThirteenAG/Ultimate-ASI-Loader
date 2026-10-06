#include "wndmode.hpp"
#include "internal.hpp"

namespace wndmode
{
    namespace
    {
        volatile LONG g_initialized = 0;

        std::wstring Directory(const std::wstring& path)
        {
            auto p = path.find_last_of(L"\\/");
            return p == std::wstring::npos ? std::wstring() : path.substr(0, p + 1);
        }

        void LoadSubModules(const std::wstring& iniPath)
        {
            for (auto& m : g_cfg.SubModules)
            {
                std::wstring path = m;
                bool relative = path.size() < 2 || (path[1] != L':' && path[0] != L'\\' && path[0] != L'/');
                if (relative && GetFileAttributesW((Directory(iniPath) + path).c_str()) != INVALID_FILE_ATTRIBUTES)
                    path = Directory(iniPath) + path;
                if (!LoadLibraryW(path.c_str()))
                    Log("SubModule %ls failed to load (%lu)", m.c_str(), GetLastError());
            }
        }
    }

    bool Initialize(const std::wstring& iniPath)
    {
        if (InterlockedCompareExchange(&g_initialized, 1, 0) != 0) return true;

        wchar_t exe[MAX_PATH * 2];
        DWORD n = GetModuleFileNameW(nullptr, exe, (DWORD)std::size(exe));
        if (!LoadConfig(iniPath, std::wstring(exe, n), g_cfg))
        {
            Log("cannot read %ls", iniPath.c_str());
            g_initialized = 0;
            return false;
        }
        Log("initialized from %ls", iniPath.c_str());

        ApplyDpiAwareness();
        InstallWindowHooks();
        InstallMetricsHooks();
        InstallDisplayHooks();
        InstallD3DHooks();
        InstallDInputHooks();
        InstallDDrawHooks();
        InstallSpeedHack();
        InstallPendingHooks();

        LoadSubModules(iniPath);
        return true;
    }

    void Shutdown()
    {
        if (InterlockedCompareExchange(&g_initialized, 0, 1) != 1) return;
        DDrawShutdown();
        RemoveAllHooks();
    }

    void ShutdownAtExit()
    {
        if (InterlockedCompareExchange(&g_initialized, 0, 1) != 1) return;
        DDrawShutdown();
    }
}
