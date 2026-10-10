#pragma once

#include <windows.h>
#include <intrin.h>
#include <cstdint>
#include <string>
#include <vector>

namespace wndmode
{
    struct Config
    {
        // Defaults match the legacy DLL and apply to missing keys.
        bool UseWindowMode = true;
        bool UseGDI = true;
        bool UseDirect3D = true;
        bool UseDirectInput = false;
        bool UseDirectDraw = true;
        bool UseDDrawEmulate = false;
        bool UseDDrawFlipBlt = false;
        bool UseDDrawColorConvert = true;
        bool UseDDrawPrimaryBlt = true;
        bool UseDDrawPrimaryLost = false;
        bool UseDDrawAutoBlt = false;
        int DDrawBltWait = -1;
        bool UseDDrawColorEmulate = true;
        bool UseForegroundControl = false;
        bool UseFGCGetForegroundWindow = false;
        bool UseFGCGetActiveWindow = false;
        bool UseFGCFixedWindowPosition = false;
        bool EnableExtraKey = false;
        bool UseCursorMsg = false;
        bool UseCursorSet = false;
        bool UseCursorGet = false;
        bool UseCursorClip = false;
        bool UseSpeedHack = false;
        int SpeedHackMultiple = 10; // 10 = 1.0x
        bool UseBackgroundPriority = false;
        bool UseBackgroundResize = false;
        bool ShowFps = false;
        bool Border = true;
        bool UseFakeScreenMetrics = true; // GetSystemMetrics/GetDeviceCaps/EnumDisplaySettings report the emulated mode
        int DpiAware = 0;                 // 0 = leave as is, 1 = system aware, 2 = per-monitor aware
        std::string MenuId; // ANSI resource name
        std::vector<std::wstring> SubModules;
    };

    extern Config g_cfg;

    // Case-insensitive and read once, like TMemIniFile. The active section is "WINDOWMODE" plus
    // the profile suffix listed under [wndmode.ini] for the full path of exePath.
    bool LoadConfig(const std::wstring& iniPath, const std::wstring& exePath, Config& out);
    // Parses from memory, for unit tests.
    Config ParseConfig(const std::string& text, const std::wstring& exePath);

    void Log(const char* fmt, ...);

    // Hooks are deferred until the module loads. The loader's own proxy DLLs, which export the
    // same names, are skipped.
    void HookExport(const wchar_t* module, const char* name, void* detour, void** original);
    void InstallPendingHooks();  // batch-enable everything registered so far
    void RemoveAllHooks();

    // Each vtable slot is patched once and the original is looked up per object at call time,
    // so one detour serves several interface versions and implementations.
    void PatchVtable(void* object, int slot, void* detour);
    void* Original(void* object, int slot);           // falls back to the current entry
    void SetVtableTag(void* object, int tag);          // e.g. interface version
    int VtableTag(void* object);                       // 0 if unknown

    template<class F>
    F Orig(void* object, int slot)
    {
        return reinterpret_cast<F>(Original(object, slot));
    }

    // Hooks the code a vtable slot points to instead of the slot. d3d9 keeps a copy of the vtable in
    // each device and rebuilds it (d3dx9 effects do that), which drops slot patches.
    void HookMethod(void* object, int slot, void* detour);
    void* MethodOriginal(void* object, int slot, void* detour);

    template<class F>
    F OrigMethod(void* object, int slot, F detour)
    {
        return reinterpret_cast<F>(MethodOriginal(object, slot, (void*)detour));
    }

    void SubclassWindow(HWND hwnd);
    // Restyles, sets the client size and centres the window on monitor (default: its own).
    // 0 keeps the client size. Posted to the window's thread when called from another.
    void SetMainWindow(HWND hwnd, int width, int height, HMONITOR monitor = nullptr);
    void PostDisplayChange(HWND hwnd, int width, int height, int bpp);
    HWND MainWindow();
    int MainWidth();
    int MainHeight();
    bool IsAppActive();
    void InstallWindowHooks();
    void ApplyDpiAwareness();

    // While a device is created or reset, size/style/position messages caused
    // by the window changes go to DefWindowProc instead of the game.
    void BeginDeviceChange();
    void EndDeviceChange();
    struct DeviceChangeScope
    {
        DeviceChangeScope() { BeginDeviceChange(); }
        ~DeviceChangeScope() { EndDeviceChange(); }
    };

    // The display mode the game believes is set (screen metrics report it).
    void SetEmulatedMode(int width, int height, int bpp);
    void ClearEmulatedMode();
    bool GetEmulatedMode(int& width, int& height, int& bpp);
    void InstallMetricsHooks();
    int RealSystemMetrics(int index);
    int RealDeviceCaps(HDC dc, int index);
    bool RealCurrentMode(DEVMODEW& dm);

    struct Fps
    {
        HWND hwnd = nullptr;
        wchar_t title[256] = {};
        ULONGLONG last = 0;
        int frames = 0;
    };
    void FpsAttach(Fps& fps, HWND hwnd);
    void FpsTick(Fps& fps);

    void InstallSpeedHack();
    void SetSpeedHackThread();          // the calling thread gets scaled time
    bool SpeedHackOnThisThread();
    void WaitVerticalBlankEmulated();   // 60 Hz pacing (used instead of vsync on the speed hack thread)
    DWORD RealTickCount();              // unscaled milliseconds

    void InstallDisplayHooks();
    LONG RealChangeDisplaySettings(DEVMODEW* dm, DWORD flags); // bypasses the emulation
    void InstallD3DHooks();
    void InstallDInputHooks();
    void InstallDDrawHooks();
    void DDrawAutoPresent();            // called from the window procedure (UseDDrawAutoBlt)
    bool DDrawEmulating8Bit();
    void DDrawShutdown();               // undoes real mode/gamma changes

    struct ScopedCs
    {
        CRITICAL_SECTION& cs;
        explicit ScopedCs(CRITICAL_SECTION& c) : cs(c) { EnterCriticalSection(&cs); }
        ~ScopedCs() { LeaveCriticalSection(&cs); }
    };

    // Per-thread re-entrancy flag. Avoids thread_local because custom module mappers may not
    // set up static TLS.
    class ThreadFlag
    {
    public:
        ThreadFlag() : index(TlsAlloc()) {}
        bool Get() const { return index != TLS_OUT_OF_INDEXES && TlsGetValue(index) != nullptr; }
        void Set(bool v) const { if (index != TLS_OUT_OF_INDEXES) TlsSetValue(index, v ? (void*)1 : nullptr); }
    private:
        DWORD index;
    };

    struct FlagGuard
    {
        const ThreadFlag& f;
        bool was;
        explicit FlagGuard(const ThreadFlag& flag) : f(flag), was(flag.Get()) { f.Set(true); }
        ~FlagGuard() { f.Set(was); }
    };

    // Lower-case file name of the module containing addr.
    std::wstring ModuleNameOf(const void* addr);
    // True for return addresses in graphics runtimes and system UI DLLs, which see the real
    // display. Code in this directory calls the Real* functions instead.
    bool CalledFromRuntime(const void* ret);
}
