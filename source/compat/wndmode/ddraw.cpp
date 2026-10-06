// UseDirectDraw: runs DirectDraw fullscreen games in a window.
// SetCooperativeLevel is downgraded to DDSCL_NORMAL and SetDisplayMode only sizes the window.
// An exclusive-mode game's primary is replaced by an off-screen fake primary of the emulated size
// and format. Updates to it (Blt, BltFast, Flip, Unlock, ReleaseDC, palette changes) are presented
// through a real primary with a clipper, at most once per timer tick and never while locked; a timer
// presents the last skipped write. GDI converts colour depths, including 8-bit palettised modes.
// DDSCL_NORMAL games keep the real primary. Works by patching the IDirectDraw*, IDirectDrawSurface*
// and IDirectDrawPalette vtables; other surfaces pass through.
#include "internal.hpp"
#include <initguid.h>
#include <ddraw.h>
#include <algorithm>

namespace wndmode
{
    namespace
    {
        // ---- vtable slots
        enum { DD_QueryInterface = 0, DD_CreateClipper = 4, DD_CreatePalette = 5, DD_CreateSurface = 6, DD_GetDisplayMode = 12, DD_Initialize = 18,
               DD_RestoreDisplayMode = 19, DD_SetCooperativeLevel = 20, DD_SetDisplayMode = 21, DD_WaitForVerticalBlank = 22 };
        enum { S_QueryInterface = 0, S_AddRef = 1, S_Release = 2, S_Blt = 5, S_BltFast = 7, S_Flip = 11, S_GetAttachedSurface = 12, S_GetCaps = 14,
               S_GetClipper = 15, S_GetDC = 17, S_GetSurfaceDesc = 22, S_IsLost = 24, S_Lock = 25, S_ReleaseDC = 26, S_Restore = 27, S_SetClipper = 28,
               S_SetPalette = 31, S_Unlock = 32 };
        enum { P_GetEntries = 4, P_SetEntries = 6 };
        enum { C_SetHWnd = 8 };

        struct Emulation
        {
            CRITICAL_SECTION cs;
            // display mode set by the game
            bool modeSet = false;
            int width = 0, height = 0, bpp = 0;
            HWND coopHwnd = nullptr;
            bool exclusive = false;          // the game asked for exclusive/fullscreen access
            bool realModeChanged = false;    // UseDDrawColorEmulate=0 changed the real colour depth
            int desktopBpp = 0;
            // primary surface emulation
            void* dd = nullptr;              // DirectDraw interface that created the primary
            void* realPrimary = nullptr;     // real primary (presents into the window)
            void* clipper = nullptr;
            std::vector<void*> fake;         // interface pointers of the fake primary
            void* backBuffer = nullptr;      // emulated flip chain back buffer (when the driver refuses an off-screen chain)
            bool ownBackBuffer = false;
            void* palette = nullptr;         // palette currently set on the fake primary
            void* defaultPalette = nullptr;
            ULONGLONG lastPresent = 0;
            ULONGLONG lastFlip = 0;
            volatile LONG locks = 0;         // Lock/GetDC on the fake primary still outstanding
            volatile bool dirty = false;     // a write was not presented yet
            PTP_TIMER timer = nullptr;
            bool gammaSaved = false;
            WORD gamma[3][256] = {};         // desktop gamma before the game changed it
            Fps fps;
        } g;

        constexpr ULONGLONG kMinWriteInterval = 10; // ms between presents caused by writes

        SRWLOCK g_fakeLock = SRWLOCK_INIT;
        ThreadFlag g_presenting;

        decltype(&RealizePalette) oRealizePalette;
        decltype(&GetSystemPaletteEntries) oGetSystemPaletteEntries;
        using DDCreateFn = HRESULT(WINAPI*)(GUID*, LPDIRECTDRAW*, IUnknown*);
        using DDCreateExFn = HRESULT(WINAPI*)(GUID*, LPVOID*, REFIID, IUnknown*);
        using CoCreateFn = HRESULT(WINAPI*)(REFCLSID, LPUNKNOWN, DWORD, REFIID, LPVOID*);
        DDCreateFn oDirectDrawCreate;
        DDCreateExFn oDirectDrawCreateEx;
        CoCreateFn oCoCreateInstance;

        template<class R, class... A>
        R OrigCall(void* obj, int slot, A... a)
        {
            return Orig<R(STDMETHODCALLTYPE*)(void*, A...)>(obj, slot)(obj, a...);
        }

        bool IsFake(void* s)
        {
            if (!s) return false;
            AcquireSRWLockShared(&g_fakeLock);
            bool r = std::find(g.fake.begin(), g.fake.end(), s) != g.fake.end();
            ReleaseSRWLockShared(&g_fakeLock);
            return r;
        }

        void AddFake(void* s)
        {
            AcquireSRWLockExclusive(&g_fakeLock);
            if (std::find(g.fake.begin(), g.fake.end(), s) == g.fake.end()) g.fake.push_back(s);
            ReleaseSRWLockExclusive(&g_fakeLock);
        }

        bool RemoveFake(void* s) // true if it was the last one
        {
            AcquireSRWLockExclusive(&g_fakeLock);
            g.fake.erase(std::remove(g.fake.begin(), g.fake.end(), s), g.fake.end());
            bool empty = g.fake.empty();
            ReleaseSRWLockExclusive(&g_fakeLock);
            return empty;
        }

        void* FakePrimary()
        {
            AcquireSRWLockShared(&g_fakeLock);
            void* p = g.fake.empty() ? nullptr : g.fake.front();
            ReleaseSRWLockShared(&g_fakeLock);
            return p;
        }

        int DesktopBpp()
        {
            HDC dc = GetDC(nullptr);
            int bpp = RealDeviceCaps(dc, BITSPIXEL);
            ReleaseDC(nullptr, dc);
            return bpp;
        }

        void FillPixelFormat(DDPIXELFORMAT& pf, int bpp)
        {
            pf = {};
            pf.dwSize = sizeof(pf);
            pf.dwFlags = DDPF_RGB;
            pf.dwRGBBitCount = bpp;
            if (bpp == 8) pf.dwFlags |= DDPF_PALETTEINDEXED8;
            else if (bpp == 16) pf.dwRBitMask = 0xF800, pf.dwGBitMask = 0x07E0, pf.dwBBitMask = 0x001F;
            else pf.dwRBitMask = 0xFF0000, pf.dwGBitMask = 0x00FF00, pf.dwBBitMask = 0x0000FF;
        }

        // Size and bit depth of any surface (any interface version).
        bool SurfaceInfo(void* s, int& w, int& h, int& bpp)
        {
            IDirectDrawSurface* s1 = nullptr;
            if (FAILED(OrigCall<HRESULT>(s, S_QueryInterface, &IID_IDirectDrawSurface, (void**)&s1)) || !s1) return false;
            DDSURFACEDESC d{ sizeof(d) };
            HRESULT hr = OrigCall<HRESULT>(s1, S_GetSurfaceDesc, &d);
            OrigCall<ULONG>(s1, S_Release);
            if (FAILED(hr)) return false;
            w = (int)d.dwWidth, h = (int)d.dwHeight, bpp = (int)d.ddpfPixelFormat.dwRGBBitCount;
            return true;
        }

        const IID* SurfaceIidForTag(int tag)
        {
            switch (tag)
            {
            case 2: return &IID_IDirectDrawSurface2;
            case 3: return &IID_IDirectDrawSurface3;
            case 4: return &IID_IDirectDrawSurface4;
            case 7: return &IID_IDirectDrawSurface7;
            default: return &IID_IDirectDrawSurface;
            }
        }

        int SurfaceTagForIid(REFIID iid)
        {
            if (IsEqualGUID(iid, IID_IDirectDrawSurface)) return 1;
            if (IsEqualGUID(iid, IID_IDirectDrawSurface2)) return 2;
            if (IsEqualGUID(iid, IID_IDirectDrawSurface3)) return 3;
            if (IsEqualGUID(iid, IID_IDirectDrawSurface4)) return 4;
            if (IsEqualGUID(iid, IID_IDirectDrawSurface7)) return 7;
            return 0;
        }

        int DDTagForIid(REFIID iid)
        {
            if (IsEqualGUID(iid, IID_IDirectDraw)) return 1;
            if (IsEqualGUID(iid, IID_IDirectDraw2)) return 2;
            if (IsEqualGUID(iid, IID_IDirectDraw4)) return 4;
            if (IsEqualGUID(iid, IID_IDirectDraw7)) return 7;
            return 0;
        }

        // ---- present

        // Copies src (srcRect, or all of it) into the window. dstGame is in emulated-mode
        // coordinates, null for the whole window.
        void Present(void* src, const RECT* srcRect, const RECT* dstGame)
        {
            if (g_presenting.Get() || !src) return;
            FlagGuard guard(g_presenting);
            ScopedCs lock(g.cs);
            HWND hwnd = g.coopHwnd ? g.coopHwnd : MainWindow();
            if (!g.realPrimary || !hwnd || !IsWindow(hwnd) || IsIconic(hwnd)) return;
            if (g_cfg.DDrawBltWait > 0) Sleep((DWORD)g_cfg.DDrawBltWait);
            FpsTick(g.fps);

            int sw, sh, sbpp;
            if (!SurfaceInfo(src, sw, sh, sbpp)) return;
            RECT client;
            if (!GetClientRect(hwnd, &client) || client.right <= 0 || client.bottom <= 0) return;
            RECT dst = client;
            if (dstGame && g.width > 0 && g.height > 0)
                dst = { dstGame->left * client.right / g.width, dstGame->top * client.bottom / g.height,
                        dstGame->right * client.right / g.width, dstGame->bottom * client.bottom / g.height };
            RECT srect = srcRect ? *srcRect : RECT{ 0, 0, sw, sh };
            srect.right = (std::min)(srect.right, (LONG)sw);
            srect.bottom = (std::min)(srect.bottom, (LONG)sh);
            if (srect.right <= srect.left || srect.bottom <= srect.top) return;

            if (sbpp == g.desktopBpp)
            {
                RECT screen = dst;
                MapWindowPoints(hwnd, nullptr, (POINT*)&screen, 2);
                HRESULT hr = OrigCall<HRESULT>(g.realPrimary, S_Blt, &screen, src, &srect, (DWORD)DDBLT_WAIT, (LPDDBLTFX) nullptr);
                if (hr == DDERR_SURFACELOST && SUCCEEDED(OrigCall<HRESULT>(g.realPrimary, S_Restore)))
                    OrigCall<HRESULT>(g.realPrimary, S_Blt, &screen, src, &srect, (DWORD)DDBLT_WAIT, (LPDDBLTFX) nullptr);
            }
            else if (g_cfg.UseDDrawColorConvert)
            {
                // GDI converts any depth (and applies the 8-bit palette of the surface)
                HDC sdc = nullptr;
                if (SUCCEEDED(OrigCall<HRESULT>(src, S_GetDC, &sdc)) && sdc)
                {
                    HDC wdc = GetDC(hwnd);
                    SetStretchBltMode(wdc, COLORONCOLOR);
                    StretchBlt(wdc, dst.left, dst.top, dst.right - dst.left, dst.bottom - dst.top, sdc, srect.left, srect.top,
                               srect.right - srect.left, srect.bottom - srect.top, SRCCOPY);
                    ReleaseDC(hwnd, wdc);
                    OrigCall<HRESULT>(src, S_ReleaseDC, sdc);
                }
            }
            g.lastPresent = GetTickCount64();
        }

        void PresentFake()
        {
            ScopedCs lock(g.cs); // the fake primary cannot be released meanwhile
            if (void* f = FakePrimary())
            {
                g.dirty = false;
                Present(f, nullptr, nullptr);
            }
        }

        VOID CALLBACK CatchUp(PTP_CALLBACK_INSTANCE, PVOID, PTP_TIMER);

        void ArmTimer()
        {
            if (!g.timer) g.timer = CreateThreadpoolTimer(CatchUp, nullptr, nullptr);
            if (!g.timer) return;
            LARGE_INTEGER due;
            due.QuadPart = -(LONGLONG)kMinWriteInterval * 2 * 10000; // relative, 100 ns units
            FILETIME ft{ due.LowPart, (DWORD)due.HighPart };
            SetThreadpoolTimer(g.timer, &ft, 0, 5);
        }

        VOID CALLBACK CatchUp(PTP_CALLBACK_INSTANCE, PVOID, PTP_TIMER)
        {
            if (!g.dirty) return;
            if (g.locks > 0) { ArmTimer(); return; }
            PresentFake();
        }

        // Presents after the game wrote to the fake primary. Games that draw
        // sprite by sprite straight to the primary would otherwise present
        // hundreds of times per frame.
        void PresentWrite(void* s)
        {
            if (g.locks > 0 || GetTickCount64() - g.lastPresent < kMinWriteInterval)
            {
                g.dirty = true;
                ArmTimer();
                return;
            }
            g.dirty = false;
            Present(s, nullptr, nullptr);
        }

        // Clips a blit to the fake primary like the clipper of a real primary
        // would: rectangles reaching past the screen edge draw their visible
        // part instead of failing with DDERR_INVALIDRECT.
        bool ClipToScreen(RECT& dst, RECT* src)
        {
            if (g.width <= 0 || g.height <= 0) return true;
            RECT o = dst;
            RECT c{ (std::max)(o.left, 0L), (std::max)(o.top, 0L), (std::min)(o.right, (LONG)g.width), (std::min)(o.bottom, (LONG)g.height) };
            if (c.right <= c.left || c.bottom <= c.top) return false;
            LONG dw = o.right - o.left, dh = o.bottom - o.top;
            if (src && dw > 0 && dh > 0)
            {
                LONG sw = src->right - src->left, sh = src->bottom - src->top;
                *src = { src->left + (c.left - o.left) * sw / dw, src->top + (c.top - o.top) * sh / dh,
                         src->right - (o.right - c.right) * sw / dw, src->bottom - (o.bottom - c.bottom) * sh / dh };
            }
            dst = c;
            return true;
        }

        void Teardown()
        {
            ScopedCs lock(g.cs);
            if (g.ownBackBuffer && g.backBuffer) OrigCall<ULONG>(g.backBuffer, S_Release);
            if (g.defaultPalette) OrigCall<ULONG>(g.defaultPalette, 2);
            if (g.clipper) OrigCall<ULONG>(g.clipper, 2);
            if (g.realPrimary) OrigCall<ULONG>(g.realPrimary, S_Release);
            g.backBuffer = g.defaultPalette = g.clipper = g.realPrimary = g.palette = nullptr;
            g.ownBackBuffer = false;
        }

        // ---- surface hooks

        HRESULT STDMETHODCALLTYPE S_QueryInterfaceHook(void* s, REFIID iid, void** out);
        ULONG STDMETHODCALLTYPE S_ReleaseHook(void* s);
        HRESULT STDMETHODCALLTYPE S_BltHook(void* s, LPRECT dst, void* src, LPRECT srcRect, DWORD flags, LPDDBLTFX fx);
        HRESULT STDMETHODCALLTYPE S_BltFastHook(void* s, DWORD x, DWORD y, void* src, LPRECT srcRect, DWORD flags);
        HRESULT STDMETHODCALLTYPE S_FlipHook(void* s, void* target, DWORD flags);
        HRESULT STDMETHODCALLTYPE S_GetAttachedSurfaceHook(void* s, LPDDSCAPS caps, void** out);
        HRESULT STDMETHODCALLTYPE S_GetCapsHook(void* s, LPDDSCAPS caps);
        HRESULT STDMETHODCALLTYPE S_GetClipperHook(void* s, LPDIRECTDRAWCLIPPER* out);
        HRESULT STDMETHODCALLTYPE S_IsLostHook(void* s);
        HRESULT STDMETHODCALLTYPE S_LockHook(void* s, LPRECT rect, LPVOID desc, DWORD flags, HANDLE ev);
        HRESULT STDMETHODCALLTYPE S_GetDCHook(void* s, HDC* dc);
        HRESULT STDMETHODCALLTYPE S_ReleaseDCHook(void* s, HDC dc);
        HRESULT STDMETHODCALLTYPE S_RestoreHook(void* s);
        HRESULT STDMETHODCALLTYPE S_SetClipperHook(void* s, LPDIRECTDRAWCLIPPER c);
        HRESULT STDMETHODCALLTYPE S_SetPaletteHook(void* s, LPDIRECTDRAWPALETTE p);
        HRESULT STDMETHODCALLTYPE S_UnlockHook(void* s, LPVOID rect);

        void PatchSurface(void* s, int tag)
        {
            if (!s) return;
            SetVtableTag(s, tag);
            PatchVtable(s, S_QueryInterface, (void*)S_QueryInterfaceHook);
            PatchVtable(s, S_Release, (void*)S_ReleaseHook);
            PatchVtable(s, S_Blt, (void*)S_BltHook);
            PatchVtable(s, S_BltFast, (void*)S_BltFastHook);
            PatchVtable(s, S_Flip, (void*)S_FlipHook);
            PatchVtable(s, S_GetAttachedSurface, (void*)S_GetAttachedSurfaceHook);
            PatchVtable(s, S_GetCaps, (void*)S_GetCapsHook);
            PatchVtable(s, S_GetClipper, (void*)S_GetClipperHook);
            PatchVtable(s, S_IsLost, (void*)S_IsLostHook);
            PatchVtable(s, S_Lock, (void*)S_LockHook);
            PatchVtable(s, S_GetDC, (void*)S_GetDCHook);
            PatchVtable(s, S_ReleaseDC, (void*)S_ReleaseDCHook);
            PatchVtable(s, S_Restore, (void*)S_RestoreHook);
            PatchVtable(s, S_SetClipper, (void*)S_SetClipperHook);
            PatchVtable(s, S_SetPalette, (void*)S_SetPaletteHook);
            PatchVtable(s, S_Unlock, (void*)S_UnlockHook);
        }

        HRESULT STDMETHODCALLTYPE S_QueryInterfaceHook(void* s, REFIID iid, void** out)
        {
            if (IsFake(s) && g.realPrimary && (IsEqualGUID(iid, IID_IDirectDrawGammaControl) || IsEqualGUID(iid, IID_IDirectDrawColorControl)))
            {
                // gamma goes to the real primary, i.e. the whole desktop: keep
                // the desktop ramp to put it back when the game ends
                if (!g.gammaSaved)
                {
                    HDC dc = GetDC(nullptr);
                    g.gammaSaved = GetDeviceGammaRamp(dc, g.gamma) != FALSE;
                    ReleaseDC(nullptr, dc);
                }
                return OrigCall<HRESULT>(g.realPrimary, S_QueryInterface, &iid, out);
            }
            HRESULT hr = OrigCall<HRESULT>(s, S_QueryInterface, &iid, out);
            if (SUCCEEDED(hr) && out && *out)
                if (int tag = SurfaceTagForIid(iid))
                {
                    PatchSurface(*out, tag);
                    if (IsFake(s)) AddFake(*out);
                }
            return hr;
        }

        ULONG STDMETHODCALLTYPE S_ReleaseHook(void* s)
        {
            if (!IsFake(s)) return OrigCall<ULONG>(s, S_Release);
            ScopedCs lock(g.cs); // no catch-up present on a dying surface
            ULONG r = OrigCall<ULONG>(s, S_Release);
            if (r == 0 && RemoveFake(s))
            {
                g.dirty = false;
                g.locks = 0;
                Teardown();
            }
            return r;
        }

        HRESULT STDMETHODCALLTYPE S_BltHook(void* s, LPRECT dst, void* src, LPRECT srcRect, DWORD flags, LPDDBLTFX fx)
        {
            if (!IsFake(s)) return OrigCall<HRESULT>(s, S_Blt, dst, src, srcRect, flags, fx);
            RECT d, sr;
            if (dst)
            {
                d = *dst;
                LPRECT srp = nullptr;
                int sw, sh, sbpp;
                if (src && (srcRect || SurfaceInfo(src, sw, sh, sbpp)))
                {
                    sr = srcRect ? *srcRect : RECT{ 0, 0, sw, sh };
                    srp = &sr;
                }
                if (!ClipToScreen(d, srp)) return DD_OK; // entirely off screen
                dst = &d;
                if (srp) srcRect = srp;
            }
            if (!g_cfg.UseDDrawPrimaryBlt && src && !IsFake(src))
            {
                Present(src, srcRect, dst); // straight to the window, the fake primary keeps its content
                return DD_OK;
            }
            HRESULT hr = OrigCall<HRESULT>(s, S_Blt, dst, src, srcRect, flags, fx);
            if (SUCCEEDED(hr)) PresentWrite(s);
            return hr;
        }

        HRESULT STDMETHODCALLTYPE S_BltFastHook(void* s, DWORD x, DWORD y, void* src, LPRECT srcRect, DWORD flags)
        {
            if (!IsFake(s)) return OrigCall<HRESULT>(s, S_BltFast, x, y, src, srcRect, flags);
            int sw, sh, sbpp;
            RECT sr;
            if (src && (srcRect || SurfaceInfo(src, sw, sh, sbpp)))
            {
                sr = srcRect ? *srcRect : RECT{ 0, 0, sw, sh };
                RECT d{ (LONG)x, (LONG)y, (LONG)x + (sr.right - sr.left), (LONG)y + (sr.bottom - sr.top) };
                if (!ClipToScreen(d, &sr)) return DD_OK;
                x = (DWORD)d.left, y = (DWORD)d.top;
                srcRect = &sr;
            }
            HRESULT hr = OrigCall<HRESULT>(s, S_BltFast, x, y, src, srcRect, flags);
            if (SUCCEEDED(hr)) PresentWrite(s);
            return hr;
        }

        HRESULT STDMETHODCALLTYPE S_FlipHook(void* s, void* target, DWORD flags)
        {
            if (!IsFake(s)) return OrigCall<HRESULT>(s, S_Flip, target, flags);
            if (!(flags & DDFLIP_NOVSYNC))
            {
                if (SpeedHackOnThisThread()) WaitVerticalBlankEmulated();
                else if (g.dd)
                {
                    // a game that is already slower than the display does not
                    // wait for the next refresh on top of that
                    static int refresh = 0;
                    if (!refresh)
                    {
                        HDC dc = GetDC(nullptr);
                        refresh = RealDeviceCaps(dc, VREFRESH);
                        ReleaseDC(nullptr, dc);
                        if (refresh <= 1) refresh = 60;
                    }
                    if (GetTickCount64() - g.lastFlip < (ULONGLONG)(1000 / refresh))
                        OrigCall<HRESULT>(g.dd, DD_WaitForVerticalBlank, (DWORD)DDWAITVB_BLOCKBEGIN, (HANDLE) nullptr);
                }
            }
            g.lastFlip = GetTickCount64();
            HRESULT hr = DD_OK;
            if (g.ownBackBuffer || g_cfg.UseDDrawFlipBlt)
            {
                // emulated flip: copy the back buffer to the front
                void* back = target ? target : g.backBuffer;
                if (back) hr = OrigCall<HRESULT>(s, S_Blt, (LPRECT) nullptr, back, (LPRECT) nullptr, (DWORD)DDBLT_WAIT, (LPDDBLTFX) nullptr);
            }
            else
                hr = OrigCall<HRESULT>(s, S_Flip, target, flags & ~DDFLIP_WAIT);
            g.dirty = false;
            Present(s, nullptr, nullptr);
            return hr;
        }

        HRESULT STDMETHODCALLTYPE S_GetAttachedSurfaceHook(void* s, LPDDSCAPS caps, void** out)
        {
            if (IsFake(s) && g.ownBackBuffer && g.backBuffer && caps && out && (caps->dwCaps & (DDSCAPS_BACKBUFFER | DDSCAPS_FLIP)))
                return OrigCall<HRESULT>(g.backBuffer, S_QueryInterface, SurfaceIidForTag(VtableTag(s)), out);
            return OrigCall<HRESULT>(s, S_GetAttachedSurface, caps, out);
        }

        HRESULT STDMETHODCALLTYPE S_GetCapsHook(void* s, LPDDSCAPS caps)
        {
            HRESULT hr = OrigCall<HRESULT>(s, S_GetCaps, caps);
            if (SUCCEEDED(hr) && caps && IsFake(s))
                caps->dwCaps = (caps->dwCaps & ~(DDSCAPS_OFFSCREENPLAIN | DDSCAPS_BACKBUFFER)) | DDSCAPS_PRIMARYSURFACE | DDSCAPS_FRONTBUFFER | DDSCAPS_VISIBLE;
            return hr;
        }

        HRESULT STDMETHODCALLTYPE S_GetClipperHook(void* s, LPDIRECTDRAWCLIPPER* out)
        {
            return OrigCall<HRESULT>(IsFake(s) && g.realPrimary ? g.realPrimary : s, S_GetClipper, out);
        }

        HRESULT STDMETHODCALLTYPE S_SetClipperHook(void* s, LPDIRECTDRAWCLIPPER c)
        {
            if (IsFake(s) && g.realPrimary)
                return c ? OrigCall<HRESULT>(g.realPrimary, S_SetClipper, c) : DD_OK; // keep our window clipper otherwise
            return OrigCall<HRESULT>(s, S_SetClipper, c);
        }

        HRESULT STDMETHODCALLTYPE S_IsLostHook(void* s)
        {
            if (IsFake(s))
            {
                if (g_cfg.UseDDrawPrimaryLost) return DDERR_SURFACELOST;
                if (g.realPrimary && OrigCall<HRESULT>(g.realPrimary, S_IsLost) == DDERR_SURFACELOST) return DDERR_SURFACELOST;
            }
            return OrigCall<HRESULT>(s, S_IsLost);
        }

        HRESULT STDMETHODCALLTYPE S_RestoreHook(void* s)
        {
            if (IsFake(s))
            {
                if (g.realPrimary) OrigCall<HRESULT>(g.realPrimary, S_Restore);
                if (g.ownBackBuffer && g.backBuffer) OrigCall<HRESULT>(g.backBuffer, S_Restore);
            }
            return OrigCall<HRESULT>(s, S_Restore);
        }

        HRESULT STDMETHODCALLTYPE S_LockHook(void* s, LPRECT rect, LPVOID desc, DWORD flags, HANDLE ev)
        {
            HRESULT hr = OrigCall<HRESULT>(s, S_Lock, rect, desc, flags, ev);
            if (SUCCEEDED(hr) && IsFake(s)) InterlockedIncrement(&g.locks);
            return hr;
        }

        HRESULT STDMETHODCALLTYPE S_GetDCHook(void* s, HDC* dc)
        {
            HRESULT hr = OrigCall<HRESULT>(s, S_GetDC, dc);
            if (SUCCEEDED(hr) && IsFake(s)) InterlockedIncrement(&g.locks);
            return hr;
        }

        void Unlocked()
        {
            if (InterlockedDecrement(&g.locks) < 0) g.locks = 0;
        }

        HRESULT STDMETHODCALLTYPE S_ReleaseDCHook(void* s, HDC dc)
        {
            HRESULT hr = OrigCall<HRESULT>(s, S_ReleaseDC, dc);
            if (SUCCEEDED(hr) && IsFake(s))
            {
                Unlocked();
                PresentWrite(s);
            }
            return hr;
        }

        HRESULT STDMETHODCALLTYPE S_SetPaletteHook(void* s, LPDIRECTDRAWPALETTE p)
        {
            HRESULT hr = OrigCall<HRESULT>(s, S_SetPalette, p);
            if (SUCCEEDED(hr) && IsFake(s))
            {
                g.palette = p;
                PresentWrite(s);
            }
            return hr;
        }

        HRESULT STDMETHODCALLTYPE S_UnlockHook(void* s, LPVOID rect)
        {
            HRESULT hr = OrigCall<HRESULT>(s, S_Unlock, rect);
            if (SUCCEEDED(hr) && IsFake(s))
            {
                Unlocked();
                PresentWrite(s);
            }
            return hr;
        }

        // ---- palette hook

        HRESULT STDMETHODCALLTYPE P_SetEntriesHook(void* p, DWORD flags, DWORD start, DWORD count, LPPALETTEENTRY entries)
        {
            HRESULT hr = OrigCall<HRESULT>(p, P_SetEntries, flags, start, count, entries);
            if (SUCCEEDED(hr) && p == g.palette) // palette animation
                if (void* f = FakePrimary()) PresentWrite(f);
            return hr;
        }

        // ---- DirectDraw hooks

        void PatchDD(void* dd, int tag);

        HRESULT STDMETHODCALLTYPE DD_QueryInterfaceHook(void* dd, REFIID iid, void** out)
        {
            HRESULT hr = OrigCall<HRESULT>(dd, DD_QueryInterface, &iid, out);
            if (SUCCEEDED(hr) && out && *out)
                if (int tag = DDTagForIid(iid)) PatchDD(*out, tag);
            return hr;
        }

        HRESULT STDMETHODCALLTYPE DD_InitializeHook(void* dd, GUID* guid)
        {
            if (g_cfg.UseDDrawEmulate) guid = (GUID*)DDCREATE_EMULATIONONLY;
            return OrigCall<HRESULT>(dd, DD_Initialize, guid);
        }

        HRESULT STDMETHODCALLTYPE DD_SetCooperativeLevelHook(void* dd, HWND hwnd, DWORD flags)
        {
            if (hwnd) g.coopHwnd = hwnd;
            // only exclusive (fullscreen) games get the emulated primary; a game
            // that already runs in a window keeps the real one
            g.exclusive = !(flags & DDSCL_NORMAL) && (flags & (DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN));
            if (flags & DDSCL_NORMAL) return OrigCall<HRESULT>(dd, DD_SetCooperativeLevel, hwnd, flags);
            if ((flags & ~(DDSCL_NOWINDOWCHANGES | DDSCL_EXCLUSIVE)) == 0) return DD_OK;
            if (flags & DDSCL_FULLSCREEN)
            {
                flags = (flags & ~(DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT | DDSCL_NOWINDOWCHANGES | DDSCL_EXCLUSIVE | DDSCL_ALLOWMODEX)) | DDSCL_NORMAL;
                return OrigCall<HRESULT>(dd, DD_SetCooperativeLevel, hwnd, flags);
            }
            return OrigCall<HRESULT>(dd, DD_SetCooperativeLevel, hwnd, flags & ~(DDSCL_NOWINDOWCHANGES | DDSCL_EXCLUSIVE));
        }

        // returns true if the mode change is emulated (no real change)
        bool EmulateMode(int w, int h, int bpp)
        {
            {
                ScopedCs lock(g.cs);
                g.modeSet = true;
                g.width = w, g.height = h, g.bpp = bpp;
                if (!g.desktopBpp) g.desktopBpp = DesktopBpp();
            }
            SetSpeedHackThread();
            SetEmulatedMode(w, h, g_cfg.UseDDrawColorEmulate ? bpp : 0);
            if (g.coopHwnd)
            {
                SubclassWindow(g.coopHwnd);
                SetMainWindow(g.coopHwnd, w, h);
            }
            if (g_cfg.UseDDrawColorEmulate)
            {
                PostDisplayChange(g.coopHwnd, w, h, bpp);
                return true;
            }
            return false;
        }

        HRESULT RealModeChanged(HRESULT hr)
        {
            if (SUCCEEDED(hr)) g.realModeChanged = true;
            else Log("real SetDisplayMode failed: 0x%08lX (the cooperative level is NORMAL; set UseDDrawColorEmulate=1)", hr);
            return hr;
        }

        HRESULT STDMETHODCALLTYPE DD_SetDisplayMode1Hook(void* dd, DWORD w, DWORD h, DWORD bpp)
        {
            if (EmulateMode((int)w, (int)h, (int)bpp)) return DD_OK;
            // only the colour depth is changed, at desktop resolution
            return RealModeChanged(OrigCall<HRESULT>(dd, DD_SetDisplayMode, (DWORD)RealSystemMetrics(SM_CXSCREEN), (DWORD)RealSystemMetrics(SM_CYSCREEN), bpp));
        }

        HRESULT STDMETHODCALLTYPE DD_SetDisplayMode2Hook(void* dd, DWORD w, DWORD h, DWORD bpp, DWORD refresh, DWORD flags)
        {
            if (EmulateMode((int)w, (int)h, (int)bpp)) return DD_OK;
            return RealModeChanged(OrigCall<HRESULT>(dd, DD_SetDisplayMode, (DWORD)RealSystemMetrics(SM_CXSCREEN), (DWORD)RealSystemMetrics(SM_CYSCREEN), bpp, (DWORD)0, flags));
        }

        void RestoreRealMode()
        {
            if (!g.realModeChanged) return;
            g.realModeChanged = false;
            RealChangeDisplaySettings(nullptr, 0);
        }

        HRESULT STDMETHODCALLTYPE DD_RestoreDisplayModeHook(void* dd)
        {
            if (g.modeSet)
            {
                g.modeSet = false;
                ClearEmulatedMode();
                PostDisplayChange(g.coopHwnd, 0, 0, 0);
            }
            HRESULT hr = OrigCall<HRESULT>(dd, DD_RestoreDisplayMode);
            if (g.realModeChanged)
            {
                RestoreRealMode(); // a normal cooperative level does not restore it
                hr = DD_OK;
            }
            return hr;
        }

        HRESULT STDMETHODCALLTYPE DD_GetDisplayModeHook(void* dd, LPDDSURFACEDESC2 desc)
        {
            HRESULT hr = OrigCall<HRESULT>(dd, DD_GetDisplayMode, desc);
            if (SUCCEEDED(hr) && desc && g.modeSet)
            {
                desc->dwWidth = g.width;
                desc->dwHeight = g.height;
                desc->lPitch = g.width * ((g.bpp + 7) / 8);
                FillPixelFormat(desc->ddpfPixelFormat, g.bpp);
            }
            return hr;
        }

        HRESULT STDMETHODCALLTYPE DD_WaitForVerticalBlankHook(void* dd, DWORD flags, HANDLE ev)
        {
            if (SpeedHackOnThisThread())
            {
                WaitVerticalBlankEmulated();
                return DD_OK;
            }
            return OrigCall<HRESULT>(dd, DD_WaitForVerticalBlank, flags, ev);
        }

        HRESULT STDMETHODCALLTYPE DD_CreatePaletteHook(void* dd, DWORD flags, LPPALETTEENTRY entries, void** out, IUnknown* outer)
        {
            HRESULT hr = OrigCall<HRESULT>(dd, DD_CreatePalette, flags, entries, out, outer);
            if (SUCCEEDED(hr) && out && *out) PatchVtable(*out, P_SetEntries, (void*)P_SetEntriesHook);
            return hr;
        }

        // Creates the real primary (with a clipper on the game window) and the
        // fake primary the game draws into.
        HRESULT CreateEmulatedPrimary(void* dd, int tag, DDSURFACEDESC2& desc, void** out)
        {
            ScopedCs lock(g.cs);
            HWND hwnd = g.coopHwnd;
            if (!g.desktopBpp) g.desktopBpp = DesktopBpp();
            int w = g.modeSet ? g.width : RealSystemMetrics(SM_CXSCREEN);
            int h = g.modeSet ? g.height : RealSystemMetrics(SM_CYSCREEN);
            int bpp = g.modeSet ? g.bpp : g.desktopBpp;
            if (!g.modeSet) g.width = w, g.height = h, g.bpp = bpp;
            SetSpeedHackThread();
            if (FakePrimary()) // the game recreates its primary
            {
                AcquireSRWLockExclusive(&g_fakeLock);
                g.fake.clear();
                ReleaseSRWLockExclusive(&g_fakeLock);
                Teardown();
            }

            DDSURFACEDESC2 real{};
            real.dwSize = desc.dwSize;
            real.dwFlags = DDSD_CAPS;
            real.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
            HRESULT hr = OrigCall<HRESULT>(dd, DD_CreateSurface, &real, &g.realPrimary, (IUnknown*)nullptr);
            if (FAILED(hr)) return hr;
            PatchSurface(g.realPrimary, tag);
            if (SUCCEEDED(OrigCall<HRESULT>(dd, DD_CreateClipper, (DWORD)0, &g.clipper, (IUnknown*)nullptr)) && g.clipper)
            {
                OrigCall<HRESULT>(g.clipper, C_SetHWnd, (DWORD)0, hwnd);
                OrigCall<HRESULT>(g.realPrimary, S_SetClipper, g.clipper);
            }

            DDSURFACEDESC2 fake = desc;
            fake.dwFlags |= DDSD_WIDTH | DDSD_HEIGHT;
            fake.dwWidth = w;
            fake.dwHeight = h;
            fake.ddsCaps.dwCaps &= ~(DDSCAPS_PRIMARYSURFACE | DDSCAPS_OWNDC | DDSCAPS_VISIBLE | DDSCAPS_FRONTBUFFER | DDSCAPS_PALETTE | DDSCAPS_MODEX);
            if (bpp != g.desktopBpp)
            {
                fake.dwFlags |= DDSD_PIXELFORMAT;
                FillPixelFormat(fake.ddpfPixelFormat, bpp);
                fake.ddsCaps.dwCaps = (fake.ddsCaps.dwCaps & ~(DDSCAPS_VIDEOMEMORY | DDSCAPS_LOCALVIDMEM | DDSCAPS_NONLOCALVIDMEM)) | DDSCAPS_SYSTEMMEMORY;
            }
            if (!(fake.ddsCaps.dwCaps & DDSCAPS_FLIP)) fake.ddsCaps.dwCaps |= DDSCAPS_OFFSCREENPLAIN;

            void* surface = nullptr;
            hr = OrigCall<HRESULT>(dd, DD_CreateSurface, &fake, &surface, (IUnknown*)nullptr);
            if (FAILED(hr) && (fake.ddsCaps.dwCaps & DDSCAPS_FLIP))
            {
                // no off-screen flip chains here: front + separate back buffer, Flip copies
                fake.dwFlags &= ~DDSD_BACKBUFFERCOUNT;
                fake.ddsCaps.dwCaps = (fake.ddsCaps.dwCaps & ~(DDSCAPS_FLIP | DDSCAPS_COMPLEX | DDSCAPS_BACKBUFFER)) | DDSCAPS_OFFSCREENPLAIN;
                hr = OrigCall<HRESULT>(dd, DD_CreateSurface, &fake, &surface, (IUnknown*)nullptr);
                if (SUCCEEDED(hr) && SUCCEEDED(OrigCall<HRESULT>(dd, DD_CreateSurface, &fake, &g.backBuffer, (IUnknown*)nullptr)))
                {
                    g.ownBackBuffer = true;
                    PatchSurface(g.backBuffer, tag);
                }
            }
            if (FAILED(hr))
            {
                Teardown();
                return hr;
            }
            PatchSurface(surface, tag);
            AddFake(surface);
            g.dd = dd;

            if (bpp == 8)
            {
                PALETTEENTRY pe[256];
                for (int i = 0; i < 256; ++i) // grey ramp until the game sets its own palette
                    pe[i] = { (BYTE)i, (BYTE)i, (BYTE)i, 0 };
                if (SUCCEEDED(DD_CreatePaletteHook(dd, DDPCAPS_8BIT | DDPCAPS_ALLOW256, pe, &g.defaultPalette, nullptr)) && g.defaultPalette)
                {
                    OrigCall<HRESULT>(surface, S_SetPalette, g.defaultPalette);
                    g.palette = g.defaultPalette;
                }
            }

            if (hwnd)
            {
                DeviceChangeScope quiet;
                SubclassWindow(hwnd);
                SetMainWindow(hwnd, w, h);
                FpsAttach(g.fps, hwnd);
            }
            *out = surface;
            return DD_OK;
        }

        HRESULT STDMETHODCALLTYPE DD_CreateSurfaceHook(void* dd, LPDDSURFACEDESC2 desc, void** out, IUnknown* outer)
        {
            int tag = VtableTag(dd);
            if (!desc || !out || outer || !g.coopHwnd || (!g.exclusive && !g.modeSet) || desc->dwSize < sizeof(DDSURFACEDESC) || !(desc->dwFlags & DDSD_CAPS))
                return OrigCall<HRESULT>(dd, DD_CreateSurface, desc, out, outer);

            DDSURFACEDESC2 d{};
            memcpy(&d, desc, (std::min)((size_t)desc->dwSize, sizeof(d)));
            if (d.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
                return CreateEmulatedPrimary(dd, tag >= 4 ? tag : 1, d, out);

            DWORD& caps = d.ddsCaps.dwCaps;
            if (g.modeSet && g.desktopBpp && g.bpp != g.desktopBpp && !(caps & (DDSCAPS_TEXTURE | DDSCAPS_ZBUFFER)) && !(d.dwFlags & DDSD_PIXELFORMAT))
            {
                d.dwFlags |= DDSD_PIXELFORMAT;
                FillPixelFormat(d.ddpfPixelFormat, g.bpp);
                if (!(caps & (DDSCAPS_VIDEOMEMORY | DDSCAPS_SYSTEMMEMORY))) caps |= DDSCAPS_SYSTEMMEMORY;
            }
            if (!(caps & (DDSCAPS_BACKBUFFER | DDSCAPS_FRONTBUFFER | DDSCAPS_OFFSCREENPLAIN | DDSCAPS_TEXTURE | DDSCAPS_ZBUFFER | DDSCAPS_OVERLAY | DDSCAPS_3DDEVICE)))
                caps |= DDSCAPS_OFFSCREENPLAIN;
            HRESULT hr = OrigCall<HRESULT>(dd, DD_CreateSurface, &d, out, outer);
            if (SUCCEEDED(hr) && *out)
            {
                PatchSurface(*out, tag >= 4 ? tag : 1);
                if (g.modeSet && g.bpp == 8 && g.palette && !(caps & (DDSCAPS_TEXTURE | DDSCAPS_ZBUFFER)))
                    OrigCall<HRESULT>(*out, S_SetPalette, g.palette);
            }
            return hr;
        }

        void PatchDD(void* dd, int tag)
        {
            SetVtableTag(dd, tag);
            PatchVtable(dd, DD_QueryInterface, (void*)DD_QueryInterfaceHook);
            PatchVtable(dd, DD_CreatePalette, (void*)DD_CreatePaletteHook);
            PatchVtable(dd, DD_CreateSurface, (void*)DD_CreateSurfaceHook);
            PatchVtable(dd, DD_GetDisplayMode, (void*)DD_GetDisplayModeHook);
            PatchVtable(dd, DD_Initialize, (void*)DD_InitializeHook);
            PatchVtable(dd, DD_RestoreDisplayMode, (void*)DD_RestoreDisplayModeHook);
            PatchVtable(dd, DD_SetCooperativeLevel, (void*)DD_SetCooperativeLevelHook);
            PatchVtable(dd, DD_SetDisplayMode, tag == 1 ? (void*)DD_SetDisplayMode1Hook : (void*)DD_SetDisplayMode2Hook);
            PatchVtable(dd, DD_WaitForVerticalBlank, (void*)DD_WaitForVerticalBlankHook);
        }

        HRESULT WINAPI hkDirectDrawCreate(GUID* guid, LPDIRECTDRAW* out, IUnknown* outer)
        {
            if (g_cfg.UseDDrawEmulate) guid = (GUID*)DDCREATE_EMULATIONONLY;
            HRESULT hr = oDirectDrawCreate(guid, out, outer);
            if (SUCCEEDED(hr) && out && *out) PatchDD(*out, 1);
            return hr;
        }

        HRESULT WINAPI hkDirectDrawCreateEx(GUID* guid, LPVOID* out, REFIID iid, IUnknown* outer)
        {
            if (g_cfg.UseDDrawEmulate) guid = (GUID*)DDCREATE_EMULATIONONLY;
            HRESULT hr = oDirectDrawCreateEx(guid, out, iid, outer);
            if (SUCCEEDED(hr) && out && *out) PatchDD(*out, (std::max)(DDTagForIid(iid), 7));
            return hr;
        }

        HRESULT WINAPI hkCoCreateInstance(REFCLSID clsid, LPUNKNOWN outer, DWORD ctx, REFIID iid, LPVOID* out)
        {
            HRESULT hr = oCoCreateInstance(clsid, outer, ctx, iid, out);
            if (SUCCEEDED(hr) && out && *out && (IsEqualGUID(clsid, CLSID_DirectDraw) || IsEqualGUID(clsid, CLSID_DirectDraw7)))
                if (int tag = DDTagForIid(iid)) PatchDD(*out, tag);
            return hr;
        }

        // ---- GDI palette emulation

        bool ScreenOrGameDC(HDC dc)
        {
            HWND w = WindowFromDC(dc);
            return !w || w == GetDesktopWindow() || w == MainWindow() || w == g.coopHwnd;
        }

        UINT WINAPI hkRealizePalette(HDC dc)
        {
            if (DDrawEmulating8Bit() && g.palette && ScreenOrGameDC(dc))
            {
                PALETTEENTRY pe[256];
                if (HPALETTE pal = (HPALETTE)GetCurrentObject(dc, OBJ_PAL))
                    if (UINT n = GetPaletteEntries(pal, 0, 256, pe))
                        P_SetEntriesHook(g.palette, 0, 0, n, pe);
            }
            return oRealizePalette(dc);
        }

        UINT WINAPI hkGetSystemPaletteEntries(HDC dc, UINT start, UINT count, LPPALETTEENTRY entries)
        {
            if (DDrawEmulating8Bit() && g.palette)
            {
                if (!entries) return 256;
                count = (std::min)(count, 256u - (std::min)(start, 256u));
                if (count && SUCCEEDED(OrigCall<HRESULT>(g.palette, P_GetEntries, (DWORD)0, (DWORD)start, (DWORD)count, entries)))
                    return count;
            }
            return oGetSystemPaletteEntries(dc, start, count, entries);
        }
    }

    bool DDrawEmulating8Bit()
    {
        return g.modeSet && g.bpp == 8 && g.desktopBpp != 8;
    }

    void DDrawAutoPresent()
    {
        if (FakePrimary() && g.locks <= 0 && GetTickCount64() - g.lastPresent >= 16) PresentFake();
    }

    void DDrawShutdown()
    {
        if (!g_cfg.UseDirectDraw) return;
        if (g.timer)
        {
            SetThreadpoolTimer(g.timer, nullptr, 0, 0);
            WaitForThreadpoolTimerCallbacks(g.timer, TRUE);
        }
        RestoreRealMode();
        if (g.gammaSaved)
        {
            g.gammaSaved = false;
            HDC dc = GetDC(nullptr);
            SetDeviceGammaRamp(dc, g.gamma);
            ReleaseDC(nullptr, dc);
        }
    }

    void InstallDDrawHooks()
    {
        if (!g_cfg.UseDirectDraw) return;
        InitializeCriticalSection(&g.cs);
        HookExport(L"ddraw.dll", "DirectDrawCreate", (void*)hkDirectDrawCreate, (void**)&oDirectDrawCreate);
        HookExport(L"ddraw.dll", "DirectDrawCreateEx", (void*)hkDirectDrawCreateEx, (void**)&oDirectDrawCreateEx);
        HookExport(L"ole32.dll", "CoCreateInstance", (void*)hkCoCreateInstance, (void**)&oCoCreateInstance);
        HookExport(L"gdi32.dll", "RealizePalette", (void*)hkRealizePalette, (void**)&oRealizePalette);
        HookExport(L"gdi32.dll", "GetSystemPaletteEntries", (void*)hkGetSystemPaletteEntries, (void**)&oGetSystemPaletteEntries);
    }
}
