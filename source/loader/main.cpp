// The loader poses as a DLL the game uses (dinput8.dll, version.dll, ...) and forwards its exports (proxy/).
// Plugins load once game code starts running (startup/), or from DllMain with DontLoadFromDllMain=0.
#include "core/loader.hpp"
#include "core/log.hpp"
#include "core/paths.hpp"
#include "core/strings.hpp"
#include "crash/crashdump.hpp"
#include "proxy/proxy.hpp"
#include "startup/startup.hpp"
#include "vfs/internal.hpp"
#include "vfs/vfs.hpp"
#ifndef _WIN64
#include "../compat/wndmode/wndmode.hpp"
#endif
#include <windows.h>

using namespace ual;

extern "C"
{
    // Lets plugins and other loaders detect UAL.
    bool WINAPI IsUltimateASILoader()
    {
        return true;
    }
}

namespace
{
    // dynamic: loaded with LoadLibrary rather than imported by the exe
    void Init(HMODULE module, bool dynamic)
    {
        InitSelf(module);
        LoadSettings();
        vfs::InitKeys(Self().exeDir);
        const auto& s = GetSettings();
        if (s.debugLog)
        {
            log::Open(Self().dir + Self().stem + L".log");
#ifdef _WIN64
            constexpr const char* arch = "x64";
#else
            constexpr const char* arch = "Win32";
#endif
            UAL_LOG("Ultimate ASI Loader %s (%s), %s", rsc_FileVersion, rsc_GitSHA1, arch);
            UAL_LOG("loader: %ls (%s)", Self().path.c_str(), dynamic ? "loaded with LoadLibrary" : "imported by the executable");
            UAL_LOG("executable: %ls", Self().exePath.c_str());
            UAL_LOG("DontLoadFromDllMain=%d LoadFromAPI=%ls", (int)s.dontLoadFromDllMain, s.loadFromAPI.empty() ? L"(none)" : s.loadFromAPI.c_str());
        }

        crashdump::Settings crash;
        crash.disabled = s.disableCrashDumps;
        crash.fullMemory = s.crashDumpFullMemory;
        crash.zip = s.crashDumpZip;
        crash.maxReports = s.crashDumpMaxReports;
        crash.iniPaths = IniPaths();
        crash.onCrash = vfs::PassThroughForCrash;
        crashdump::Install(module, crash);

        if (!s.dontLoadFromDllMain)
        {
            startup::LoadEverything();
            return;
        }
        if (dynamic && startup::LoadedByProtectionStub())
        {
            if (!startup::HookEntryPointsInline()) proxy::LoadOriginalLibrary();
            return;
        }
        if (!startup::HookEntryPoints()) proxy::LoadOriginalLibrary();
    }
}

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        Init(module, reserved == nullptr);
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
#ifndef _WIN64
        if (reserved) wndmode::ShutdownAtExit(); // the desktop keeps a DirectDraw game's gamma otherwise
        else wndmode::Shutdown();
#endif
        if (!reserved) startup::RestoreImports(); // FreeLibrary only, not process exit
    }
    return TRUE;
}
