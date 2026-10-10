// UseDirect3D: fullscreen D3D8/9/9Ex devices are created windowed and the window is sized to the
// back buffer. Back buffer format and multisampling are kept when the adapter supports them
// windowed. Windowed D3D8 ignores vsync, so it gets D3DSWAPEFFECT_COPY_VSYNC.
// Patches the IDirect3D8/9 and device vtables instead of wrapping, so the game keeps the real COM objects.
#include "internal.hpp"
#include <d3d9.h>
#include <algorithm>

namespace wndmode
{
    namespace
    {
        // ---- Direct3D 8 (no SDK header needed: only these layouts are used)
        struct PP8
        {
            UINT BackBufferWidth, BackBufferHeight;
            DWORD BackBufferFormat;
            UINT BackBufferCount;
            DWORD MultiSampleType, SwapEffect;
            HWND hDeviceWindow;
            BOOL Windowed, EnableAutoDepthStencil;
            DWORD AutoDepthStencilFormat, Flags;
            UINT FullScreen_RefreshRateInHz, FullScreen_PresentationInterval;
        };
        struct Mode8 { UINT Width, Height, RefreshRate; DWORD Format; };
        struct CreationParams8 { UINT AdapterOrdinal; DWORD DeviceType; HWND hFocusWindow; DWORD BehaviorFlags; };
        struct SurfaceDesc8 { DWORD Format, Type, Usage, Pool; UINT Size; DWORD MultiSampleType; UINT Width, Height; };
        struct LockedRect8 { INT Pitch; void* pBits; };
        constexpr DWORD kFmtA8R8G8B8 = 21, kFmtX8R8G8B8 = 22;
        constexpr DWORD kSwapCopyVsync8 = 4; // D3DSWAPEFFECT_COPY_VSYNC (D3D8 only)
        constexpr UINT kInterval8Immediate = 0x80000000;

        // IDirect3D8 / IDirect3DDevice8 / IDirect3DSurface8 vtable slots
        enum { D3D8_Release = 2, D3D8_GetAdapterDisplayMode = 8, D3D8_CheckDeviceType = 9, D3D8_CheckDeviceMultiSampleType = 11,
               D3D8_GetAdapterMonitor = 14, D3D8_CreateDevice = 15 };
        enum { DEV8_Release = 2, DEV8_GetDirect3D = 6, DEV8_GetDisplayMode = 8, DEV8_GetCreationParameters = 9, DEV8_CreateAdditionalSwapChain = 13, DEV8_Reset = 14,
               DEV8_Present = 15, DEV8_CreateImageSurface = 27, DEV8_GetFrontBuffer = 30 };
        enum { SURF8_Release = 2, SURF8_GetDesc = 8, SURF8_LockRect = 9, SURF8_UnlockRect = 10 };

        // ---- Direct3D 9 vtable slots
        enum { D3D9_CreateDevice = 16, D3D9EX_CreateDeviceEx = 20 };
        enum { DEV9_CreateAdditionalSwapChain = 13, DEV9_Reset = 16, DEV9_Present = 17, DEV9_GetFrontBufferData = 33, DEV9EX_PresentEx = 121, DEV9EX_ResetEx = 132 };

        Fps g_fps;
        volatile bool g_forced = false; // a device was forced into windowed mode
        volatile bool g_wait8 = false;  // D3D8 device that needs vsync but cannot use COPY_VSYNC

        template<class R, class... A>
        R Call(void* obj, int slot, A... a)
        {
            return reinterpret_cast<R(STDMETHODCALLTYPE*)(void*, A...)>((*(void***)obj)[slot])(obj, a...);
        }

        // A windowed device presents into hDeviceWindow (the focus window if
        // there is none); a child device window means its top-level window.
        HWND TargetWindow(HWND focus, HWND device)
        {
            HWND w = device ? device : focus;
            if (w && (GetWindowLongW(w, GWL_STYLE) & WS_CHILD)) w = GetAncestor(w, GA_ROOT);
            return w;
        }

        int BitsOf(DWORD format) // D3DFORMAT, same values in D3D8 and D3D9
        {
            switch (format)
            {
            case D3DFMT_A8R8G8B8: case D3DFMT_X8R8G8B8: case D3DFMT_A2R10G10B10: case D3DFMT_A2B10G10R10: return 32;
            case D3DFMT_R5G6B5: case D3DFMT_X1R5G5B5: case D3DFMT_A1R5G5B5: case D3DFMT_A4R4G4B4: case D3DFMT_X4R4G4B4: return 16;
            }
            return 0;
        }

        void TakeWindow(HWND hwnd, UINT w, UINT h, DWORD format, HMONITOR monitor)
        {
            if (!hwnd) return;
            g_forced = true;
            SetEmulatedMode((int)w, (int)h, BitsOf(format));
            // a Reset to the same size (after every Alt+Tab) leaves the window alone
            if (hwnd == MainWindow() && (int)w == MainWidth() && (int)h == MainHeight()) return;
            SubclassWindow(hwnd);
            SetMainWindow(hwnd, (int)w, (int)h, monitor);
            FpsAttach(g_fps, hwnd);
        }

        // values Direct3D filled in (a size of 0 means "the client area") go back to the game
        template<class PP>
        void CopyBack(PP& game, const PP& used)
        {
            if (!game.BackBufferWidth) game.BackBufferWidth = used.BackBufferWidth;
            if (!game.BackBufferHeight) game.BackBufferHeight = used.BackBufferHeight;
            if (!game.BackBufferCount) game.BackBufferCount = used.BackBufferCount;
        }

        // Copies the client area of the game window out of a desktop-sized
        // front buffer capture into the (window-sized) surface the game passed.
        void CopyClientArea(const BYTE* src, int srcPitch, int srcW, int srcH, BYTE* dst, int dstPitch, int dstW, int dstH, HWND hwnd)
        {
            RECT r{};
            if (!hwnd || !GetClientRect(hwnd, &r)) return;
            MapWindowPoints(hwnd, nullptr, (POINT*)&r, 2);
            MONITORINFO mi{ sizeof(mi) };
            GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi);
            OffsetRect(&r, -mi.rcMonitor.left, -mi.rcMonitor.top);
            int x0 = (std::max)(0L, r.left), y0 = (std::max)(0L, r.top);
            int w = (std::min)({ (int)(r.right - x0), srcW - x0, dstW });
            int h = (std::min)({ (int)(r.bottom - y0), srcH - y0, dstH });
            for (int y = 0; y < h; ++y)
                memcpy(dst + y * dstPitch, src + (y0 + y) * srcPitch + x0 * 4, (size_t)(std::max)(0, w) * 4);
        }

        // ---- Direct3D 9

        using Create9Fn = IDirect3D9*(WINAPI*)(UINT);
        using Create9ExFn = HRESULT(WINAPI*)(UINT, IDirect3D9Ex**);
        Create9Fn oDirect3DCreate9;
        Create9ExFn oDirect3DCreate9Ex;

        // d3d may be null (no format/multisample checks then)
        bool FixPP9(D3DPRESENT_PARAMETERS& pp, IDirect3D9* d3d, UINT adapter, D3DDEVTYPE type)
        {
            if (!g_cfg.UseWindowMode || pp.Windowed) return false;
            SetSpeedHackThread();
            pp.Windowed = TRUE;
            D3DDISPLAYMODE desktop{};
            bool checks = d3d && SUCCEEDED(d3d->GetAdapterDisplayMode(adapter, &desktop));
            // keep the game's format (alpha, 16-bit lockable back buffers) when it works windowed
            if (!checks || pp.BackBufferFormat == D3DFMT_UNKNOWN ||
                FAILED(d3d->CheckDeviceType(adapter, type, desktop.Format, pp.BackBufferFormat, TRUE)))
                pp.BackBufferFormat = D3DFMT_UNKNOWN; // = desktop format
            if (pp.MultiSampleType != D3DMULTISAMPLE_NONE && checks)
            {
                D3DFORMAT fmt = pp.BackBufferFormat == D3DFMT_UNKNOWN ? desktop.Format : pp.BackBufferFormat;
                DWORD levels = 0;
                if (FAILED(d3d->CheckDeviceMultiSampleType(adapter, type, fmt, TRUE, pp.MultiSampleType, &levels)))
                    pp.MultiSampleType = D3DMULTISAMPLE_NONE, pp.MultiSampleQuality = 0;
                else if (levels && pp.MultiSampleQuality >= levels)
                    pp.MultiSampleQuality = levels - 1;
            }
            if (pp.MultiSampleType != D3DMULTISAMPLE_NONE) pp.Flags &= ~D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
            pp.FullScreen_RefreshRateInHz = 0;
            if (pp.PresentationInterval != D3DPRESENT_INTERVAL_IMMEDIATE && pp.PresentationInterval != D3DPRESENT_INTERVAL_ONE)
                pp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
            if (SpeedHackOnThisThread())
                pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE; // the game paces itself with scaled time
            return true;
        }

        // Reset/ResetEx: fixes pp for the device's adapter and window.
        bool FixDevice9(IDirect3DDevice9* dev, const D3DPRESENT_PARAMETERS& game, D3DPRESENT_PARAMETERS& pp)
        {
            D3DDEVICE_CREATION_PARAMETERS cp{};
            dev->GetCreationParameters(&cp);
            IDirect3D9* d3d = nullptr;
            dev->GetDirect3D(&d3d);
            bool fixed = FixPP9(pp, d3d, cp.AdapterOrdinal, cp.DeviceType);
            if (fixed)
                TakeWindow(TargetWindow(cp.hFocusWindow, pp.hDeviceWindow), pp.BackBufferWidth, pp.BackBufferHeight, game.BackBufferFormat,
                           d3d ? d3d->GetAdapterMonitor(cp.AdapterOrdinal) : nullptr);
            if (d3d) d3d->Release();
            return fixed;
        }

        HRESULT STDMETHODCALLTYPE D9_Present(IDirect3DDevice9* dev, const RECT* s, const RECT* d, HWND w, const RGNDATA* r)
        {
            FpsTick(g_fps);
            return OrigMethod(dev, DEV9_Present, D9_Present)(dev, s, d, w, r);
        }

        HRESULT STDMETHODCALLTYPE D9_PresentEx(IDirect3DDevice9Ex* dev, const RECT* s, const RECT* d, HWND w, const RGNDATA* r, DWORD f)
        {
            FpsTick(g_fps);
            return OrigMethod(dev, DEV9EX_PresentEx, D9_PresentEx)(dev, s, d, w, r, f);
        }

        HRESULT STDMETHODCALLTYPE D9_Reset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pp)
        {
            if (!pp) return OrigMethod(dev, DEV9_Reset, D9_Reset)(dev, pp);
            DeviceChangeScope quiet;
            D3DPRESENT_PARAMETERS copy = *pp;
            bool fixed = FixDevice9(dev, *pp, copy);
            HRESULT hr = OrigMethod(dev, DEV9_Reset, D9_Reset)(dev, &copy);
            if (fixed && SUCCEEDED(hr)) CopyBack(*pp, copy);
            return hr;
        }

        HRESULT STDMETHODCALLTYPE D9_ResetEx(IDirect3DDevice9Ex* dev, D3DPRESENT_PARAMETERS* pp, D3DDISPLAYMODEEX* mode)
        {
            if (!pp) return OrigMethod(dev, DEV9EX_ResetEx, D9_ResetEx)(dev, pp, mode);
            DeviceChangeScope quiet;
            D3DPRESENT_PARAMETERS copy = *pp;
            bool fixed = FixDevice9(dev, *pp, copy);
            if (fixed) mode = nullptr; // windowed devices take no fullscreen display mode
            HRESULT hr = OrigMethod(dev, DEV9EX_ResetEx, D9_ResetEx)(dev, &copy, mode);
            if (fixed && SUCCEEDED(hr)) CopyBack(*pp, copy);
            return hr;
        }

        HRESULT STDMETHODCALLTYPE D9_CreateAdditionalSwapChain(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pp, IDirect3DSwapChain9** out)
        {
            if (!pp) return OrigMethod(dev, DEV9_CreateAdditionalSwapChain, D9_CreateAdditionalSwapChain)(dev, pp, out);
            D3DPRESENT_PARAMETERS copy = *pp;
            D3DDEVICE_CREATION_PARAMETERS cp{};
            dev->GetCreationParameters(&cp);
            IDirect3D9* d3d = nullptr;
            dev->GetDirect3D(&d3d);
            FixPP9(copy, d3d, cp.AdapterOrdinal, cp.DeviceType);
            if (d3d) d3d->Release();
            return OrigMethod(dev, DEV9_CreateAdditionalSwapChain, D9_CreateAdditionalSwapChain)(dev, &copy, out);
        }

        // In windowed mode the front buffer is the whole desktop; games pass a
        // surface the size of their back buffer, which would make the call fail.
        HRESULT STDMETHODCALLTYPE D9_GetFrontBufferData(IDirect3DDevice9* dev, UINT swapChain, IDirect3DSurface9* dest)
        {
            auto orig = OrigMethod(dev, DEV9_GetFrontBufferData, D9_GetFrontBufferData);
            if (g_forced && MainWindow() && IsIconic(MainWindow())) return D3D_OK; // nothing of the game is on screen
            D3DSURFACE_DESC dd{};
            D3DDISPLAYMODE dm{};
            if (!g_forced || !dest || FAILED(dest->GetDesc(&dd)) || FAILED(dev->GetDisplayMode(swapChain, &dm)) ||
                (dd.Width >= dm.Width && dd.Height >= dm.Height) || (dd.Format != D3DFMT_A8R8G8B8 && dd.Format != D3DFMT_X8R8G8B8))
                return orig(dev, swapChain, dest);
            IDirect3DSurface9* tmp = nullptr;
            HRESULT hr = dev->CreateOffscreenPlainSurface(dm.Width, dm.Height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &tmp, nullptr);
            if (FAILED(hr)) return orig(dev, swapChain, dest);
            hr = orig(dev, swapChain, tmp);
            if (SUCCEEDED(hr))
            {
                D3DLOCKED_RECT s{}, d{};
                if (SUCCEEDED(tmp->LockRect(&s, nullptr, D3DLOCK_READONLY)))
                {
                    if (SUCCEEDED(dest->LockRect(&d, nullptr, 0)))
                    {
                        CopyClientArea((const BYTE*)s.pBits, s.Pitch, dm.Width, dm.Height, (BYTE*)d.pBits, d.Pitch, dd.Width, dd.Height, MainWindow());
                        dest->UnlockRect();
                    }
                    tmp->UnlockRect();
                }
            }
            tmp->Release();
            return hr;
        }

        void PatchDevice9(IDirect3DDevice9* dev, bool ex)
        {
            HookMethod(dev, DEV9_CreateAdditionalSwapChain, (void*)D9_CreateAdditionalSwapChain);
            HookMethod(dev, DEV9_Reset, (void*)D9_Reset);
            HookMethod(dev, DEV9_Present, (void*)D9_Present);
            HookMethod(dev, DEV9_GetFrontBufferData, (void*)D9_GetFrontBufferData);
            if (ex)
            {
                HookMethod(dev, DEV9EX_PresentEx, (void*)D9_PresentEx);
                HookMethod(dev, DEV9EX_ResetEx, (void*)D9_ResetEx);
            }
        }

        HRESULT STDMETHODCALLTYPE D9_CreateDevice(IDirect3D9* d3d, UINT adapter, D3DDEVTYPE type, HWND focus, DWORD flags, D3DPRESENT_PARAMETERS* pp, IDirect3DDevice9** out)
        {
            auto orig = Orig<decltype(&D9_CreateDevice)>(d3d, D3D9_CreateDevice);
            if (!pp) return orig(d3d, adapter, type, focus, flags, pp, out);
            DeviceChangeScope quiet;
            D3DPRESENT_PARAMETERS copy = *pp;
            bool fixed = FixPP9(copy, d3d, adapter, type);
            if (fixed)
                TakeWindow(TargetWindow(focus, copy.hDeviceWindow), copy.BackBufferWidth, copy.BackBufferHeight, pp->BackBufferFormat, d3d->GetAdapterMonitor(adapter));
            HRESULT hr = orig(d3d, adapter, type, focus, flags, &copy, out);
            if (SUCCEEDED(hr) && out && *out)
            {
                PatchDevice9(*out, false);
                if (fixed) CopyBack(*pp, copy);
            }
            return hr;
        }

        HRESULT STDMETHODCALLTYPE D9_CreateDeviceEx(IDirect3D9Ex* d3d, UINT adapter, D3DDEVTYPE type, HWND focus, DWORD flags, D3DPRESENT_PARAMETERS* pp, D3DDISPLAYMODEEX* mode, IDirect3DDevice9Ex** out)
        {
            auto orig = Orig<decltype(&D9_CreateDeviceEx)>(d3d, D3D9EX_CreateDeviceEx);
            if (!pp) return orig(d3d, adapter, type, focus, flags, pp, mode, out);
            DeviceChangeScope quiet;
            D3DPRESENT_PARAMETERS copy = *pp;
            bool fixed = FixPP9(copy, d3d, adapter, type);
            if (fixed)
            {
                mode = nullptr;
                TakeWindow(TargetWindow(focus, copy.hDeviceWindow), copy.BackBufferWidth, copy.BackBufferHeight, pp->BackBufferFormat, d3d->GetAdapterMonitor(adapter));
            }
            HRESULT hr = orig(d3d, adapter, type, focus, flags, &copy, mode, out);
            if (SUCCEEDED(hr) && out && *out)
            {
                PatchDevice9(*out, true);
                if (fixed) CopyBack(*pp, copy);
            }
            return hr;
        }

        IDirect3D9* WINAPI hkDirect3DCreate9(UINT sdk)
        {
            IDirect3D9* d3d = oDirect3DCreate9(sdk);
            if (d3d) PatchVtable(d3d, D3D9_CreateDevice, (void*)D9_CreateDevice);
            return d3d;
        }

        HRESULT WINAPI hkDirect3DCreate9Ex(UINT sdk, IDirect3D9Ex** out)
        {
            HRESULT hr = oDirect3DCreate9Ex(sdk, out);
            if (SUCCEEDED(hr) && out && *out)
            {
                PatchVtable(*out, D3D9_CreateDevice, (void*)D9_CreateDevice);
                PatchVtable(*out, D3D9EX_CreateDeviceEx, (void*)D9_CreateDeviceEx);
            }
            return hr;
        }

        // ---- Direct3D 8

        using Create8Fn = void*(WINAPI*)(UINT);
        Create8Fn oDirect3DCreate8;

        bool FixPP8(PP8& pp, void* d3d, UINT adapter, DWORD type, bool* needWait = nullptr)
        {
            if (!g_cfg.UseWindowMode || pp.Windowed) return false;
            SetSpeedHackThread();
            Mode8 desktop{};
            bool checks = d3d && SUCCEEDED(Call<HRESULT>(d3d, D3D8_GetAdapterDisplayMode, adapter, &desktop));
            if (checks && (!pp.BackBufferFormat ||
                FAILED(Call<HRESULT>(d3d, D3D8_CheckDeviceType, adapter, type, desktop.Format, pp.BackBufferFormat, (BOOL)TRUE))))
                pp.BackBufferFormat = desktop.Format; // D3D8 has no "unknown = desktop" for windowed devices
            if (pp.MultiSampleType && checks &&
                FAILED(Call<HRESULT>(d3d, D3D8_CheckDeviceMultiSampleType, adapter, type, pp.BackBufferFormat, (BOOL)TRUE, pp.MultiSampleType)))
                pp.MultiSampleType = 0;
            if (pp.MultiSampleType) pp.Flags &= ~1u; // D3DPRESENTFLAG_LOCKABLE_BACKBUFFER

            // Windowed D3D8 ignores the presentation interval, so games that pace on vsync
            // would run uncapped. COPY_VSYNC is D3D8's windowed vsync.
            bool vsync = pp.FullScreen_PresentationInterval != kInterval8Immediate && !SpeedHackOnThisThread();
            bool wait = false;
            if (vsync)
            {
                if (pp.BackBufferCount <= 1 && !pp.MultiSampleType)
                    pp.SwapEffect = kSwapCopyVsync8, pp.BackBufferCount = 1;
                else
                    wait = true;
            }
            if (needWait) *needWait = wait;
            pp.Windowed = TRUE;
            pp.FullScreen_RefreshRateInHz = 0;
            pp.FullScreen_PresentationInterval = 0; // must be DEFAULT when windowed
            return true;
        }

        struct Device8Info
        {
            void* d3d = nullptr;
            CreationParams8 cp{};
            ~Device8Info() { if (d3d) Call<ULONG>(d3d, D3D8_Release); }
            explicit Device8Info(void* dev)
            {
                Call<HRESULT>(dev, DEV8_GetCreationParameters, &cp);
                Call<HRESULT>(dev, DEV8_GetDirect3D, &d3d);
            }
        };

        // vsync for D3D8 devices that cannot use COPY_VSYNC
        void WaitForVsync()
        {
            using DwmFlushFn = HRESULT(WINAPI*)();
            static DwmFlushFn flush = [] {
                HMODULE dwm = LoadLibraryW(L"dwmapi.dll");
                return dwm ? (DwmFlushFn)GetProcAddress(dwm, "DwmFlush") : nullptr;
            }();
            if (!flush || FAILED(flush())) WaitVerticalBlankEmulated();
        }

        HRESULT STDMETHODCALLTYPE D8_Present(void* dev, const RECT* s, const RECT* d, HWND w, const RGNDATA* r)
        {
            FpsTick(g_fps);
            HRESULT hr = Orig<decltype(&D8_Present)>(dev, DEV8_Present)(dev, s, d, w, r);
            if (g_wait8) WaitForVsync();
            return hr;
        }

        HRESULT STDMETHODCALLTYPE D8_Reset(void* dev, PP8* pp)
        {
            if (!pp) return Orig<decltype(&D8_Reset)>(dev, DEV8_Reset)(dev, pp);
            DeviceChangeScope quiet;
            PP8 copy = *pp;
            Device8Info info(dev);
            bool wait = false;
            bool fixed = FixPP8(copy, info.d3d, info.cp.AdapterOrdinal, info.cp.DeviceType, &wait);
            if (fixed)
                TakeWindow(TargetWindow(info.cp.hFocusWindow, copy.hDeviceWindow), copy.BackBufferWidth, copy.BackBufferHeight, pp->BackBufferFormat,
                           info.d3d ? Call<HMONITOR>(info.d3d, D3D8_GetAdapterMonitor, info.cp.AdapterOrdinal) : nullptr);
            HRESULT hr = Orig<decltype(&D8_Reset)>(dev, DEV8_Reset)(dev, &copy);
            if (fixed && SUCCEEDED(hr))
            {
                g_wait8 = wait;
                CopyBack(*pp, copy);
            }
            return hr;
        }

        HRESULT STDMETHODCALLTYPE D8_CreateAdditionalSwapChain(void* dev, PP8* pp, void** out)
        {
            if (!pp) return Orig<decltype(&D8_CreateAdditionalSwapChain)>(dev, DEV8_CreateAdditionalSwapChain)(dev, pp, out);
            PP8 copy = *pp;
            Device8Info info(dev);
            FixPP8(copy, info.d3d, info.cp.AdapterOrdinal, info.cp.DeviceType);
            return Orig<decltype(&D8_CreateAdditionalSwapChain)>(dev, DEV8_CreateAdditionalSwapChain)(dev, &copy, out);
        }

        HRESULT STDMETHODCALLTYPE D8_GetFrontBuffer(void* dev, void* dest)
        {
            auto orig = Orig<decltype(&D8_GetFrontBuffer)>(dev, DEV8_GetFrontBuffer);
            if (g_forced && MainWindow() && IsIconic(MainWindow())) return D3D_OK; // nothing of the game is on screen
            SurfaceDesc8 dd{};
            Mode8 dm{};
            if (!g_forced || !dest || FAILED(Call<HRESULT>(dest, SURF8_GetDesc, &dd)) || FAILED(Call<HRESULT>(dev, DEV8_GetDisplayMode, &dm)) ||
                (dd.Width >= dm.Width && dd.Height >= dm.Height) || (dd.Format != kFmtA8R8G8B8 && dd.Format != kFmtX8R8G8B8))
                return orig(dev, dest);
            void* tmp = nullptr;
            if (FAILED(Call<HRESULT>(dev, DEV8_CreateImageSurface, dm.Width, dm.Height, kFmtA8R8G8B8, &tmp)) || !tmp)
                return orig(dev, dest);
            HRESULT hr = orig(dev, tmp);
            if (SUCCEEDED(hr))
            {
                LockedRect8 s{}, d{};
                if (SUCCEEDED(Call<HRESULT>(tmp, SURF8_LockRect, &s, (const RECT*)nullptr, (DWORD)D3DLOCK_READONLY)))
                {
                    if (SUCCEEDED(Call<HRESULT>(dest, SURF8_LockRect, &d, (const RECT*)nullptr, (DWORD)0)))
                    {
                        CopyClientArea((const BYTE*)s.pBits, s.Pitch, dm.Width, dm.Height, (BYTE*)d.pBits, d.Pitch, dd.Width, dd.Height, MainWindow());
                        Call<HRESULT>(dest, SURF8_UnlockRect);
                    }
                    Call<HRESULT>(tmp, SURF8_UnlockRect);
                }
            }
            Call<ULONG>(tmp, SURF8_Release);
            return hr;
        }

        HRESULT STDMETHODCALLTYPE D8_CreateDevice(void* d3d, UINT adapter, DWORD type, HWND focus, DWORD flags, PP8* pp, void** out)
        {
            auto orig = Orig<decltype(&D8_CreateDevice)>(d3d, D3D8_CreateDevice);
            if (!pp) return orig(d3d, adapter, type, focus, flags, pp, out);
            DeviceChangeScope quiet;
            PP8 copy = *pp;
            bool wait = false;
            bool fixed = FixPP8(copy, d3d, adapter, type, &wait);
            if (fixed)
                TakeWindow(TargetWindow(focus, copy.hDeviceWindow), copy.BackBufferWidth, copy.BackBufferHeight, pp->BackBufferFormat,
                           Call<HMONITOR>(d3d, D3D8_GetAdapterMonitor, adapter));
            HRESULT hr = orig(d3d, adapter, type, focus, flags, &copy, out);
            if (SUCCEEDED(hr) && out && *out)
            {
                if (fixed)
                {
                    g_wait8 = wait;
                    CopyBack(*pp, copy);
                }
                PatchVtable(*out, DEV8_CreateAdditionalSwapChain, (void*)D8_CreateAdditionalSwapChain);
                PatchVtable(*out, DEV8_Reset, (void*)D8_Reset);
                PatchVtable(*out, DEV8_Present, (void*)D8_Present);
                PatchVtable(*out, DEV8_GetFrontBuffer, (void*)D8_GetFrontBuffer);
            }
            return hr;
        }

        void* WINAPI hkDirect3DCreate8(UINT sdk)
        {
            void* d3d = oDirect3DCreate8(sdk);
            if (d3d) PatchVtable(d3d, D3D8_CreateDevice, (void*)D8_CreateDevice);
            return d3d;
        }
    }

    void InstallD3DHooks()
    {
        if (!g_cfg.UseDirect3D) return;
        HookExport(L"d3d9.dll", "Direct3DCreate9", (void*)hkDirect3DCreate9, (void**)&oDirect3DCreate9);
        HookExport(L"d3d9.dll", "Direct3DCreate9Ex", (void*)hkDirect3DCreate9Ex, (void**)&oDirect3DCreate9Ex);
        HookExport(L"d3d8.dll", "Direct3DCreate8", (void*)hkDirect3DCreate8, (void**)&oDirect3DCreate8);
    }
}
