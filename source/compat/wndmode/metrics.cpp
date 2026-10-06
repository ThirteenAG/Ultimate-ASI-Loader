// The game's display mode is only emulated, so GetSystemMetrics (SM_CXSCREEN/SM_CYSCREEN),
// GetDeviceCaps (HORZRES/VERTRES/BITSPIXEL, palette caps in 8-bit modes) and EnumDisplaySettings
// for the current mode report it. Graphics runtimes and system DLLs still see the real display.
#include "internal.hpp"
#include <cwctype>

namespace wndmode
{
    namespace
    {
        SRWLOCK g_lock = SRWLOCK_INIT;
        bool g_set = false;
        int g_w = 0, g_h = 0, g_bpp = 0;

        decltype(&GetSystemMetrics) oGetSystemMetrics;
        decltype(&GetDeviceCaps) oGetDeviceCaps;
        decltype(&EnumDisplaySettingsA) oEnumDisplaySettingsA;
        decltype(&EnumDisplaySettingsW) oEnumDisplaySettingsW;
        decltype(&EnumDisplaySettingsExA) oEnumDisplaySettingsExA;
        decltype(&EnumDisplaySettingsExW) oEnumDisplaySettingsExW;

        bool Fake(const void* ret, int& w, int& h, int& bpp)
        {
            return g_cfg.UseFakeScreenMetrics && GetEmulatedMode(w, h, bpp) && !CalledFromRuntime(ret);
        }

        // the primary display (games pass NULL or the name of the adapter they run on)
        template<class C>
        bool PrimaryDevice(const C* name)
        {
            if (!name || !name[0]) return true;
            MONITORINFOEXW mi{};
            mi.cbSize = sizeof(mi);
            if (!GetMonitorInfoW(MonitorFromWindow(MainWindow(), MONITOR_DEFAULTTOPRIMARY), &mi)) return false;
            const wchar_t* d = mi.szDevice;
            for (; *name && *d; ++name, ++d)
                if (towupper((wchar_t)*name) != towupper(*d)) return false;
            return *name == 0 && *d == 0;
        }

        template<class DM>
        void Patch(DM* dm, int w, int h, int bpp)
        {
            dm->dmPelsWidth = w;
            dm->dmPelsHeight = h;
            if (bpp) dm->dmBitsPerPel = bpp;
        }

        int WINAPI hkGetSystemMetrics(int index)
        {
            int w, h, bpp;
            if ((index == SM_CXSCREEN || index == SM_CYSCREEN) && Fake(_ReturnAddress(), w, h, bpp))
                return index == SM_CXSCREEN ? w : h;
            return oGetSystemMetrics(index);
        }

        bool DisplayDC(HDC dc)
        {
            if (oGetDeviceCaps(dc, TECHNOLOGY) != DT_RASDISPLAY) return false; // printers, metafiles
            HWND wnd = WindowFromDC(dc);
            return !wnd || wnd == GetDesktopWindow() || wnd == MainWindow() || GetAncestor(wnd, GA_ROOT) == MainWindow();
        }

        int WINAPI hkGetDeviceCaps(HDC dc, int index)
        {
            int r = oGetDeviceCaps(dc, index);
            if (CalledFromRuntime(_ReturnAddress()) || !DisplayDC(dc)) return r;
            if (DDrawEmulating8Bit())
            {
                switch (index)
                {
                case BITSPIXEL: return 8;
                case NUMCOLORS: return 20;
                case SIZEPALETTE: return 256;
                case NUMRESERVED: return 20;
                case RASTERCAPS: return r | RC_PALETTE;
                }
            }
            int w, h, bpp;
            if (g_cfg.UseFakeScreenMetrics && GetEmulatedMode(w, h, bpp))
            {
                if (index == HORZRES) return w;
                if (index == VERTRES) return h;
                if (index == BITSPIXEL && bpp) return bpp;
            }
            return r;
        }

        BOOL WINAPI hkEnumDisplaySettingsA(LPCSTR dev, DWORD mode, DEVMODEA* dm)
        {
            BOOL ok = oEnumDisplaySettingsA(dev, mode, dm);
            int w, h, bpp;
            if (ok && dm && mode == ENUM_CURRENT_SETTINGS && PrimaryDevice(dev) && Fake(_ReturnAddress(), w, h, bpp)) Patch(dm, w, h, bpp);
            return ok;
        }

        BOOL WINAPI hkEnumDisplaySettingsW(LPCWSTR dev, DWORD mode, DEVMODEW* dm)
        {
            BOOL ok = oEnumDisplaySettingsW(dev, mode, dm);
            int w, h, bpp;
            if (ok && dm && mode == ENUM_CURRENT_SETTINGS && PrimaryDevice(dev) && Fake(_ReturnAddress(), w, h, bpp)) Patch(dm, w, h, bpp);
            return ok;
        }

        BOOL WINAPI hkEnumDisplaySettingsExA(LPCSTR dev, DWORD mode, DEVMODEA* dm, DWORD flags)
        {
            BOOL ok = oEnumDisplaySettingsExA(dev, mode, dm, flags);
            int w, h, bpp;
            if (ok && dm && mode == ENUM_CURRENT_SETTINGS && PrimaryDevice(dev) && Fake(_ReturnAddress(), w, h, bpp)) Patch(dm, w, h, bpp);
            return ok;
        }

        BOOL WINAPI hkEnumDisplaySettingsExW(LPCWSTR dev, DWORD mode, DEVMODEW* dm, DWORD flags)
        {
            BOOL ok = oEnumDisplaySettingsExW(dev, mode, dm, flags);
            int w, h, bpp;
            if (ok && dm && mode == ENUM_CURRENT_SETTINGS && PrimaryDevice(dev) && Fake(_ReturnAddress(), w, h, bpp)) Patch(dm, w, h, bpp);
            return ok;
        }
    }

    void SetEmulatedMode(int width, int height, int bpp)
    {
        if (width <= 0 || height <= 0) return;
        AcquireSRWLockExclusive(&g_lock);
        g_set = true;
        g_w = width, g_h = height, g_bpp = bpp > 0 ? bpp : 0;
        ReleaseSRWLockExclusive(&g_lock);
    }

    void ClearEmulatedMode()
    {
        AcquireSRWLockExclusive(&g_lock);
        g_set = false;
        ReleaseSRWLockExclusive(&g_lock);
    }

    bool GetEmulatedMode(int& width, int& height, int& bpp)
    {
        AcquireSRWLockShared(&g_lock);
        bool set = g_set;
        width = g_w, height = g_h, bpp = g_bpp;
        ReleaseSRWLockShared(&g_lock);
        return set;
    }

    int RealSystemMetrics(int index)
    {
        return oGetSystemMetrics ? oGetSystemMetrics(index) : GetSystemMetrics(index);
    }

    int RealDeviceCaps(HDC dc, int index)
    {
        return oGetDeviceCaps ? oGetDeviceCaps(dc, index) : GetDeviceCaps(dc, index);
    }

    bool RealCurrentMode(DEVMODEW& dm)
    {
        dm = {};
        dm.dmSize = sizeof(dm);
        return (oEnumDisplaySettingsW ? oEnumDisplaySettingsW : &EnumDisplaySettingsW)(nullptr, ENUM_CURRENT_SETTINGS, &dm) != FALSE;
    }

    void InstallMetricsHooks()
    {
        if (!g_cfg.UseWindowMode) return;
        if (g_cfg.UseFakeScreenMetrics || g_cfg.UseDirectDraw)
            HookExport(L"gdi32.dll", "GetDeviceCaps", (void*)hkGetDeviceCaps, (void**)&oGetDeviceCaps);
        if (!g_cfg.UseFakeScreenMetrics) return;
        HookExport(L"user32.dll", "GetSystemMetrics", (void*)hkGetSystemMetrics, (void**)&oGetSystemMetrics);
        HookExport(L"user32.dll", "EnumDisplaySettingsA", (void*)hkEnumDisplaySettingsA, (void**)&oEnumDisplaySettingsA);
        HookExport(L"user32.dll", "EnumDisplaySettingsW", (void*)hkEnumDisplaySettingsW, (void**)&oEnumDisplaySettingsW);
        HookExport(L"user32.dll", "EnumDisplaySettingsExA", (void*)hkEnumDisplaySettingsExA, (void**)&oEnumDisplaySettingsExA);
        HookExport(L"user32.dll", "EnumDisplaySettingsExW", (void*)hkEnumDisplaySettingsExW, (void**)&oEnumDisplaySettingsExW);
    }
}
