// UseGDI: the game's display mode changes are faked. The window is resized to the mode and gets
// the WM_DISPLAYCHANGE it expects. Changes made by d3d8/d3d9/ddraw themselves pass through.
#include "internal.hpp"

namespace wndmode
{
    namespace
    {
        decltype(&ChangeDisplaySettingsA) oChangeDisplaySettingsA;
        decltype(&ChangeDisplaySettingsW) oChangeDisplaySettingsW;
        decltype(&ChangeDisplaySettingsExA) oChangeDisplaySettingsExA;
        decltype(&ChangeDisplaySettingsExW) oChangeDisplaySettingsExW;
        ThreadFlag g_inside; // A -> ExA -> ExW chains inside user32 are decided once

        bool FromGraphicsRuntime(const void* ret)
        {
            auto m = ModuleNameOf(ret);
            return m.empty() || m == L"d3d8.dll" || m == L"d3d8d.dll" || m == L"d3d9.dll" || m == L"ddraw.dll" || m == L"ddrawex.dll" ||
                   m == L"d3d8thk.dll" || m == L"dxgi.dll";
        }

        HWND TargetWindow()
        {
            GUITHREADINFO gti{ sizeof(gti) };
            if (GetGUIThreadInfo(GetCurrentThreadId(), &gti))
            {
                if (gti.hwndFocus) return GetAncestor(gti.hwndFocus, GA_ROOT);
                if (gti.hwndActive) return gti.hwndActive;
            }
            return MainWindow();
        }

        // returns true if handled (result in *out)
        bool Fake(const void* ret, DWORD width, DWORD height, DWORD bpp, DWORD fields, bool hasMode, DWORD flags, LONG* out)
        {
            if (!g_cfg.UseWindowMode || g_inside.Get() || FromGraphicsRuntime(ret)) return false;
            if (flags & CDS_TEST)
            {
                *out = DISP_CHANGE_SUCCESSFUL; // any mode "works" in a window
                return true;
            }
            HWND hwnd = TargetWindow();
            if (hasMode && (fields & (DM_PELSWIDTH | DM_PELSHEIGHT)) == (DM_PELSWIDTH | DM_PELSHEIGHT))
            {
                SetEmulatedMode((int)width, (int)height, (fields & DM_BITSPERPEL) ? (int)bpp : 0);
                if (hwnd)
                {
                    SubclassWindow(hwnd);
                    SetMainWindow(hwnd, (int)width, (int)height);
                }
                PostDisplayChange(hwnd, (int)width, (int)height, (fields & DM_BITSPERPEL) ? (int)bpp : 32);
                *out = DISP_CHANGE_SUCCESSFUL;
                return true;
            }
            if (!hasMode) // restore the registry mode: nothing was changed
            {
                ClearEmulatedMode();
                PostDisplayChange(hwnd, 0, 0, 0);
                *out = DISP_CHANGE_SUCCESSFUL;
                return true;
            }
            return false;
        }

        LONG WINAPI hkChangeDisplaySettingsA(DEVMODEA* dm, DWORD flags)
        {
            LONG r;
            if (Fake(_ReturnAddress(), dm ? dm->dmPelsWidth : 0, dm ? dm->dmPelsHeight : 0, dm ? dm->dmBitsPerPel : 0, dm ? dm->dmFields : 0, dm != nullptr, flags, &r)) return r;
            FlagGuard g(g_inside);
            return oChangeDisplaySettingsA(dm, flags);
        }

        LONG WINAPI hkChangeDisplaySettingsW(DEVMODEW* dm, DWORD flags)
        {
            LONG r;
            if (Fake(_ReturnAddress(), dm ? dm->dmPelsWidth : 0, dm ? dm->dmPelsHeight : 0, dm ? dm->dmBitsPerPel : 0, dm ? dm->dmFields : 0, dm != nullptr, flags, &r)) return r;
            FlagGuard g(g_inside);
            return oChangeDisplaySettingsW(dm, flags);
        }

        LONG WINAPI hkChangeDisplaySettingsExA(LPCSTR dev, DEVMODEA* dm, HWND hwnd, DWORD flags, LPVOID param)
        {
            LONG r;
            if (Fake(_ReturnAddress(), dm ? dm->dmPelsWidth : 0, dm ? dm->dmPelsHeight : 0, dm ? dm->dmBitsPerPel : 0, dm ? dm->dmFields : 0, dm != nullptr, flags, &r)) return r;
            FlagGuard g(g_inside);
            return oChangeDisplaySettingsExA(dev, dm, hwnd, flags, param);
        }

        LONG WINAPI hkChangeDisplaySettingsExW(LPCWSTR dev, DEVMODEW* dm, HWND hwnd, DWORD flags, LPVOID param)
        {
            LONG r;
            if (Fake(_ReturnAddress(), dm ? dm->dmPelsWidth : 0, dm ? dm->dmPelsHeight : 0, dm ? dm->dmBitsPerPel : 0, dm ? dm->dmFields : 0, dm != nullptr, flags, &r)) return r;
            FlagGuard g(g_inside);
            return oChangeDisplaySettingsExW(dev, dm, hwnd, flags, param);
        }
    }

    LONG RealChangeDisplaySettings(DEVMODEW* dm, DWORD flags)
    {
        FlagGuard g(g_inside);
        return oChangeDisplaySettingsW ? oChangeDisplaySettingsW(dm, flags) : ChangeDisplaySettingsW(dm, flags);
    }

    void InstallDisplayHooks()
    {
        if (!g_cfg.UseGDI) return;
        HookExport(L"user32.dll", "ChangeDisplaySettingsA", (void*)hkChangeDisplaySettingsA, (void**)&oChangeDisplaySettingsA);
        HookExport(L"user32.dll", "ChangeDisplaySettingsW", (void*)hkChangeDisplaySettingsW, (void**)&oChangeDisplaySettingsW);
        HookExport(L"user32.dll", "ChangeDisplaySettingsExA", (void*)hkChangeDisplaySettingsExA, (void**)&oChangeDisplaySettingsExA);
        HookExport(L"user32.dll", "ChangeDisplaySettingsExW", (void*)hkChangeDisplaySettingsExW, (void**)&oChangeDisplaySettingsExW);
    }
}
