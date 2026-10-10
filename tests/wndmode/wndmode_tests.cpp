// Tests for source/compat/wndmode. Config parsing runs in-process, behaviour runs in a child
// (--child <scenario> <ini>) since the hooks are process-wide and installed once. Scenarios
// verify the hook is in place before asking for fullscreen, so a broken build can't change the real display mode.
#include "../common/testlib.hpp"
#include "wndmode.hpp"
#include "internal.hpp"
#include <d3d9.h>
#include <ddraw.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "ddraw.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")

namespace fs = std::filesystem;

// --- child side

namespace child
{
    int g_failures = 0;

    void Check(bool ok, const char* what, const std::string& detail = {})
    {
        printf("%s\t%s\t%s\n", ok ? "PASS" : "FAIL", what, detail.c_str());
        fflush(stdout);
        if (!ok) ++g_failures;
    }

    void Skip(const char* why)
    {
        printf("SKIP\t%s\n", why);
        fflush(stdout);
        ExitProcess(0);
    }

    std::string Str(long long v) { return std::to_string(v); }

    // MinHook writes a jmp, or a short jmp back into the padding of a hot-patchable "mov edi, edi" prologue
    bool IsDetoured(const void* fn)
    {
        auto p = (const BYTE*)fn;
        return p && (p[0] == 0xE9 || (p[0] == 0xEB && p[1] == 0xF9 && p[-5] == 0xE9));
    }

    const void* Export(const wchar_t* dll, const char* name)
    {
        HMODULE m = GetModuleHandleW(dll);
        return m ? (const void*)GetProcAddress(m, name) : nullptr;
    }

    bool InThisModule(const void* p)
    {
        return wndmode::ModuleNameOf(p) == wndmode::ModuleNameOf((void*)&IsDetoured);
    }

    volatile LONG g_sizeMessages = 0; // WM_SIZE seen by the "game"

    LRESULT CALLBACK GameWndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
    {
        if (msg == WM_SIZE) InterlockedIncrement(&g_sizeMessages);
        return DefWindowProcW(hwnd, msg, wp, lp);
    }

    HWND MakeWindow(int w = 640, int h = 480)
    {
        WNDCLASSW wc{};
        wc.lpfnWndProc = GameWndProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"ual_wndmode_test";
        RegisterClassW(&wc);
        HWND hwnd = CreateWindowExW(WS_EX_TOPMOST, wc.lpszClassName, L"wndmode test", WS_POPUP | WS_VISIBLE, 0, 0, w, h, nullptr, nullptr, wc.hInstance, nullptr);
        SetForegroundWindow(hwnd);
        SetFocus(hwnd);
        return hwnd;
    }

    void Pump()
    {
        MSG m;
        while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&m);
            DispatchMessageW(&m);
        }
    }

    SIZE ClientSize(HWND hwnd)
    {
        Pump();
        RECT r{};
        GetClientRect(hwnd, &r);
        return { r.right, r.bottom };
    }

    // Real mode. EnumDisplaySettings reports the emulated one to the game.
    DEVMODEW CurrentMode()
    {
        DEVMODEW dm{};
        wndmode::RealCurrentMode(dm);
        return dm;
    }

    void CheckReportedScreen(int w, int h, const char* what)
    {
        DEVMODEW dm{ .dmSize = sizeof(DEVMODEW) };
        EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &dm);
        HDC dc = GetDC(nullptr);
        int hr = GetDeviceCaps(dc, HORZRES), vr = GetDeviceCaps(dc, VERTRES);
        ReleaseDC(nullptr, dc);
        int cx = GetSystemMetrics(SM_CXSCREEN), cy = GetSystemMetrics(SM_CYSCREEN);
        Check(cx == w && cy == h && hr == w && vr == h && (int)dm.dmPelsWidth == w && (int)dm.dmPelsHeight == h, what,
              "metrics " + Str(cx) + "x" + Str(cy) + ", caps " + Str(hr) + "x" + Str(vr) + ", current mode " + Str(dm.dmPelsWidth) + "x" + Str(dm.dmPelsHeight));
    }

    POINT WindowPos(HWND hwnd)
    {
        RECT r{};
        GetWindowRect(hwnd, &r);
        return { r.left, r.top };
    }

    void CheckDesktopUnchanged(const DEVMODEW& before)
    {
        auto now = CurrentMode();
        Check(now.dmPelsWidth == before.dmPelsWidth && now.dmPelsHeight == before.dmPelsHeight && now.dmBitsPerPel == before.dmBitsPerPel,
              "the desktop display mode is unchanged", Str(now.dmPelsWidth) + "x" + Str(now.dmPelsHeight) + "x" + Str(now.dmBitsPerPel));
    }

    void CheckWindowed(HWND hwnd, int w, int h)
    {
        auto sz = ClientSize(hwnd);
        LONG style = GetWindowLongW(hwnd, GWL_STYLE);
        Check(sz.cx == w && sz.cy == h, "the window client area has the requested size", Str(sz.cx) + "x" + Str(sz.cy));
        Check(!(style & WS_POPUP), "the window is no longer a popup");
        Check(!(GetWindowLongW(hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST), "the window is no longer topmost");
    }

    // --- D3D9

    D3DPRESENT_PARAMETERS FullscreenPP(HWND hwnd, int w, int h)
    {
        D3DPRESENT_PARAMETERS pp{};
        pp.BackBufferWidth = w;
        pp.BackBufferHeight = h;
        pp.BackBufferFormat = D3DFMT_X8R8G8B8;
        pp.BackBufferCount = 1;
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.hDeviceWindow = hwnd;
        pp.Windowed = FALSE;
        pp.FullScreen_RefreshRateInHz = 60;
        pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
        return pp;
    }

    IDirect3DDevice9* CreateD3D9Fullscreen(IDirect3D9* d3d, HWND hwnd, int w, int h)
    {
        if (InThisModule((*(void***)d3d)[16]) == false) { Check(false, "IDirect3D9::CreateDevice is patched"); ExitProcess(1); }
        auto pp = FullscreenPP(hwnd, w, h);
        IDirect3DDevice9* dev = nullptr;
        HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &dev);
        if (hr == D3DERR_NOTAVAILABLE || hr == D3DERR_INVALIDCALL && !dev)
            if (FAILED(d3d->CheckDeviceType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, TRUE))) Skip("no Direct3D 9 HAL device");
        Check(SUCCEEDED(hr) && dev, "a fullscreen Direct3D 9 device is created", "hr=" + Str(hr));
        return dev;
    }

    void ScenarioD3D9()
    {
        HWND hwnd = MakeWindow();
        auto before = CurrentMode();
        Check(IsDetoured(Export(L"d3d9.dll", "Direct3DCreate9")), "Direct3DCreate9 is hooked");
        IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
        if (!d3d) Skip("Direct3DCreate9 failed");
        Pump();
        g_sizeMessages = 0;
        IDirect3DDevice9* dev = CreateD3D9Fullscreen(d3d, hwnd, 640, 480);
        if (!dev) ExitProcess(1);
        Check(g_sizeMessages == 0, "the game sees no WM_SIZE caused by the window change during CreateDevice", Str(g_sizeMessages));
        // d3d9 keeps a vtable copy per device and rebuilds it (d3dx9 effects do), so a patched slot doesn't last
        const void* reset = (*(void***)dev)[16];
        Check(!InThisModule(reset) && IsDetoured(reset), "IDirect3DDevice9::Reset is hooked in d3d9's code, not in the device's vtable");
        IDirect3DSwapChain9* sc = nullptr;
        D3DPRESENT_PARAMETERS actual{};
        if (SUCCEEDED(dev->GetSwapChain(0, &sc)))
        {
            sc->GetPresentParameters(&actual);
            sc->Release();
        }
        Check(actual.Windowed == TRUE, "the device runs windowed");
        CheckWindowed(hwnd, 640, 480);
        CheckDesktopUnchanged(before);
        CheckReportedScreen(640, 480, "the game sees a 640x480 screen");
        Check(SUCCEEDED(dev->Present(nullptr, nullptr, nullptr, nullptr)), "Present works");

        // the game's back buffer format (alpha here) survives if windowed devices support it
        auto pp = FullscreenPP(hwnd, 800, 600);
        pp.BackBufferFormat = D3DFMT_A8R8G8B8;
        D3DDISPLAYMODE desktop{};
        d3d->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &desktop);
        bool alphaOk = SUCCEEDED(d3d->CheckDeviceType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, desktop.Format, D3DFMT_A8R8G8B8, TRUE));
        HRESULT hr = dev->Reset(&pp);
        Check(SUCCEEDED(hr), "Reset to another fullscreen mode works", "hr=" + Str(hr));
        CheckWindowed(hwnd, 800, 600);
        CheckDesktopUnchanged(before);
        CheckReportedScreen(800, 600, "the game sees an 800x600 screen after Reset");
        if (alphaOk && SUCCEEDED(dev->GetSwapChain(0, &sc)))
        {
            sc->GetPresentParameters(&actual);
            sc->Release();
            Check(actual.BackBufferFormat == D3DFMT_A8R8G8B8, "the game's back buffer format is kept", "format=" + Str(actual.BackBufferFormat));
        }

        // Reset to the same size, as after Alt+Tab, doesn't move the window back
        SetWindowPos(hwnd, nullptr, 7, 9, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        Pump();
        POINT moved = WindowPos(hwnd);
        pp = FullscreenPP(hwnd, 800, 600);
        hr = dev->Reset(&pp);
        Pump();
        POINT after = WindowPos(hwnd);
        Check(SUCCEEDED(hr) && after.x == moved.x && after.y == moved.y, "Reset to the same size leaves the window where the user put it",
              Str(after.x) + "," + Str(after.y) + " vs " + Str(moved.x) + "," + Str(moved.y));

        D3DDISPLAYMODE dm{};
        dev->GetDisplayMode(0, &dm);
        IDirect3DSurface9* shot = nullptr;
        if (SUCCEEDED(dev->CreateOffscreenPlainSurface(800, 600, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &shot, nullptr)))
        {
            hr = dev->GetFrontBufferData(0, shot);
            Check(SUCCEEDED(hr) || dm.Width <= 800, "GetFrontBufferData works with a back-buffer sized surface", "hr=" + Str(hr));
            shot->Release();
        }
        dev->Release();
        d3d->Release();
    }

    // A render thread creates the device while the window thread waits, pumping only posted
    // messages like many engines. The window must be changed on its own thread without deadlock.
    void ScenarioD3D9Thread()
    {
        HWND hwnd = MakeWindow();
        IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
        if (!d3d) Skip("Direct3DCreate9 failed");
        IDirect3DDevice9* dev = nullptr;
        std::thread worker([&] {
            dev = CreateD3D9Fullscreen(d3d, hwnd, 800, 600);
        });
        HANDLE h = worker.native_handle();
        for (;;)
        {
            DWORD r = MsgWaitForMultipleObjects(1, &h, FALSE, 30000, QS_POSTMESSAGE | QS_SENDMESSAGE);
            if (r != WAIT_OBJECT_0 + 1) break;
            MSG m;
            while (PeekMessageW(&m, nullptr, 0, 0, PM_REMOVE | PM_QS_POSTMESSAGE)) DispatchMessageW(&m);
        }
        bool finished = WaitForSingleObject(h, 0) == WAIT_OBJECT_0;
        Check(finished, "device creation on a render thread does not deadlock");
        if (!finished) ExitProcess(1);
        worker.join();
        if (!dev) ExitProcess(1);
        CheckWindowed(hwnd, 800, 600);
        dev->Release();
        d3d->Release();
    }

    void ScenarioD3D9Ex()
    {
        HWND hwnd = MakeWindow();
        auto before = CurrentMode();
        IDirect3D9Ex* d3d = nullptr;
        if (FAILED(Direct3DCreate9Ex(D3D_SDK_VERSION, &d3d)) || !d3d) Skip("Direct3DCreate9Ex failed");
        Check(InThisModule((*(void***)d3d)[20]), "IDirect3D9Ex::CreateDeviceEx is patched");
        if (!InThisModule((*(void***)d3d)[20])) ExitProcess(1);
        auto pp = FullscreenPP(hwnd, 640, 480);
        D3DDISPLAYMODEEX mode{ sizeof(mode), 640, 480, 60, D3DFMT_X8R8G8B8, D3DSCANLINEORDERING_PROGRESSIVE };
        IDirect3DDevice9Ex* dev = nullptr;
        HRESULT hr = d3d->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &mode, &dev);
        if (FAILED(hr) && FAILED(d3d->CheckDeviceType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, TRUE))) Skip("no Direct3D 9 HAL device");
        Check(SUCCEEDED(hr) && dev, "a fullscreen Direct3D 9Ex device is created (windowed)", "hr=" + Str(hr));
        CheckWindowed(hwnd, 640, 480);
        CheckDesktopUnchanged(before);
        if (dev)
        {
            pp = FullscreenPP(hwnd, 800, 600);
            hr = dev->ResetEx(&pp, &mode);
            Check(SUCCEEDED(hr), "ResetEx to fullscreen works", "hr=" + Str(hr));
            CheckWindowed(hwnd, 800, 600);
            dev->Release();
        }
        d3d->Release();
    }

    // --- D3D8 (no SDK header)

    void ScenarioD3D8()
    {
        HMODULE d3d8 = LoadLibraryW(L"d3d8.dll");
        if (!d3d8) Skip("d3d8.dll not available");
        auto create = (void*(WINAPI*)(UINT))GetProcAddress(d3d8, "Direct3DCreate8");
        Check(IsDetoured((void*)create), "Direct3DCreate8 is hooked (late-loaded d3d8.dll)");
        void* d3d = create ? create(220) : nullptr;
        if (!d3d) Skip("Direct3DCreate8 failed");
        void** vt = *(void***)d3d;
        if (!InThisModule(vt[15])) { Check(false, "IDirect3D8::CreateDevice is patched"); ExitProcess(1); }
        HWND hwnd = MakeWindow();
        auto before = CurrentMode();
        struct PP8 { UINT w, h; DWORD fmt; UINT count; DWORD ms, swap; HWND wnd; BOOL windowed, autoDepth; DWORD depthFmt, flags; UINT refresh, interval; };
        PP8 pp{ 640, 480, 22 /* X8R8G8B8 */, 1, 0, 1 /* DISCARD */, hwnd, FALSE, FALSE, 0, 0, 60, 1 };
        void* dev = nullptr;
        using CreateDevice = HRESULT(WINAPI*)(void*, UINT, DWORD, HWND, DWORD, PP8*, void**);
        HRESULT hr = ((CreateDevice)vt[15])(d3d, 0, 1 /* HAL */, hwnd, 0x20 /* SOFTWARE_VERTEXPROCESSING */, &pp, &dev);
        if (FAILED(hr)) Skip("no Direct3D 8 HAL device");
        Check(SUCCEEDED(hr) && dev, "a fullscreen Direct3D 8 device is created (windowed)");
        CheckWindowed(hwnd, 640, 480);
        CheckDesktopUnchanged(before);
        if (dev) ((ULONG(WINAPI*)(void*))(*(void***)dev)[2])(dev);
        ((ULONG(WINAPI*)(void*))vt[2])(d3d);
    }

    // --- GDI

    void ScenarioDisplayChange()
    {
        HWND hwnd = MakeWindow();
        auto before = CurrentMode();
        bool hooked = IsDetoured(Export(L"user32.dll", "ChangeDisplaySettingsA")) && IsDetoured(Export(L"user32.dll", "ChangeDisplaySettingsExW"));
        Check(hooked, "ChangeDisplaySettings* are hooked");
        if (!hooked) ExitProcess(1);
        DEVMODEA dm{ .dmSize = sizeof(DEVMODEA) };
        dm.dmPelsWidth = 640;
        dm.dmPelsHeight = 480;
        dm.dmBitsPerPel = 16;
        dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
        Check(ChangeDisplaySettingsA(&dm, CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL, "ChangeDisplaySettingsA reports success");
        MSG m{};
        bool got = false;
        for (int i = 0; i < 20 && !got; ++i)
        {
            got = PeekMessageW(&m, hwnd, WM_DISPLAYCHANGE, WM_DISPLAYCHANGE, PM_REMOVE);
            if (!got) Sleep(10);
        }
        Check(got && LOWORD(m.lParam) == 640 && HIWORD(m.lParam) == 480 && m.wParam == 16, "the window receives WM_DISPLAYCHANGE for the requested mode");
        CheckDesktopUnchanged(before);
        CheckWindowed(hwnd, 640, 480);
        CheckReportedScreen(640, 480, "GetSystemMetrics / GetDeviceCaps / EnumDisplaySettings report the emulated mode");
        DEVMODEW cur{ .dmSize = sizeof(DEVMODEW) };
        EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &cur);
        Check(cur.dmBitsPerPel == 16, "the emulated colour depth is reported", Str(cur.dmBitsPerPel));
        Check(ChangeDisplaySettingsW(nullptr, 0) == DISP_CHANGE_SUCCESSFUL, "restoring the mode reports success");
        Check(GetSystemMetrics(SM_CXSCREEN) == wndmode::RealSystemMetrics(SM_CXSCREEN) && GetSystemMetrics(SM_CYSCREEN) == wndmode::RealSystemMetrics(SM_CYSCREEN),
              "after restoring, the real screen is reported again", Str(GetSystemMetrics(SM_CXSCREEN)) + "x" + Str(GetSystemMetrics(SM_CYSCREEN)));
        Check(ChangeDisplaySettingsExW(nullptr, nullptr, nullptr, CDS_FULLSCREEN, nullptr) == DISP_CHANGE_SUCCESSFUL, "CDS_FULLSCREEN with a NULL mode does not crash");
        Check(ChangeDisplaySettingsA(&dm, CDS_TEST) == DISP_CHANGE_SUCCESSFUL, "CDS_TEST succeeds");
        CheckDesktopUnchanged(before);
    }

    // --- DirectDraw

    void ScenarioDDraw(int bpp, bool dd7)
    {
        HWND hwnd = MakeWindow();
        auto before = CurrentMode();
        Check(IsDetoured(Export(L"ddraw.dll", "DirectDrawCreate")) && IsDetoured(Export(L"ddraw.dll", "DirectDrawCreateEx")), "DirectDrawCreate(Ex) are hooked");
        IDirectDraw* dd1 = nullptr;
        IDirectDraw7* dd = nullptr;
        if (dd7)
        {
            if (FAILED(DirectDrawCreateEx(nullptr, (void**)&dd, IID_IDirectDraw7, nullptr))) Skip("DirectDraw not available");
        }
        else
        {
            if (FAILED(DirectDrawCreate(nullptr, &dd1, nullptr))) Skip("DirectDraw not available");
            dd1->QueryInterface(IID_IDirectDraw7, (void**)&dd); // patched through QueryInterface
            dd1->Release();
        }
        if (!dd) { Check(false, "IDirectDraw7"); ExitProcess(1); }
        void** vt = *(void***)dd;
        if (!InThisModule(vt[20]) || !InThisModule(vt[21])) { Check(false, "IDirectDraw7 SetCooperativeLevel/SetDisplayMode are patched"); ExitProcess(1); }

        Check(SUCCEEDED(dd->SetCooperativeLevel(hwnd, DDSCL_FULLSCREEN | DDSCL_EXCLUSIVE | DDSCL_ALLOWREBOOT)), "fullscreen exclusive cooperative level");
        Check(SUCCEEDED(dd->SetDisplayMode(640, 480, bpp, 0, 0)), "SetDisplayMode succeeds");
        CheckDesktopUnchanged(before);
        CheckWindowed(hwnd, 640, 480);
        DDSURFACEDESC2 mode{ sizeof(mode) };
        dd->GetDisplayMode(&mode);
        Check(mode.dwWidth == 640 && mode.dwHeight == 480 && (int)mode.ddpfPixelFormat.dwRGBBitCount == bpp, "GetDisplayMode reports the emulated mode",
              Str(mode.dwWidth) + "x" + Str(mode.dwHeight) + "x" + Str(mode.ddpfPixelFormat.dwRGBBitCount));

        DDSURFACEDESC2 sd{ sizeof(sd) };
        sd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
        sd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
        sd.dwBackBufferCount = 1;
        IDirectDrawSurface7* primary = nullptr;
        HRESULT hr = dd->CreateSurface(&sd, &primary, nullptr);
        Check(SUCCEEDED(hr) && primary, "a flipping primary surface is created", "hr=" + Str(hr));
        if (!primary) ExitProcess(1);
        DDSURFACEDESC2 pd{ sizeof(pd) };
        primary->GetSurfaceDesc(&pd);
        Check(pd.dwWidth == 640 && pd.dwHeight == 480 && (int)pd.ddpfPixelFormat.dwRGBBitCount == bpp, "the primary has the emulated size and depth",
              Str(pd.dwWidth) + "x" + Str(pd.dwHeight) + "x" + Str(pd.ddpfPixelFormat.dwRGBBitCount));
        DDSCAPS2 caps{};
        primary->GetCaps(&caps);
        Check((caps.dwCaps & DDSCAPS_PRIMARYSURFACE) != 0, "the primary reports DDSCAPS_PRIMARYSURFACE");
        DDSCAPS2 backCaps{ DDSCAPS_BACKBUFFER };
        IDirectDrawSurface7* back = nullptr;
        Check(SUCCEEDED(primary->GetAttachedSurface(&backCaps, &back)) && back, "the back buffer is attached");

        if (bpp == 8)
        {
            PALETTEENTRY pe[256];
            for (int i = 0; i < 256; ++i) pe[i] = { (BYTE)i, 0, (BYTE)(255 - i), 0 };
            IDirectDrawPalette* pal = nullptr;
            Check(SUCCEEDED(dd->CreatePalette(DDPCAPS_8BIT | DDPCAPS_ALLOW256, pe, &pal, nullptr)) && pal, "an 8-bit palette is created");
            if (pal)
            {
                Check(SUCCEEDED(primary->SetPalette(pal)), "the palette is set on the primary");
                pe[1] = { 255, 255, 0, 0 };
                Check(SUCCEEDED(pal->SetEntries(0, 1, 1, &pe[1])), "palette animation works");
                pal->Release();
            }
            HDC screen = GetDC(nullptr);
            Check(GetDeviceCaps(screen, BITSPIXEL) == 8, "GDI reports the emulated 8-bit depth");
            ReleaseDC(nullptr, screen);
        }

        if (back)
        {
            DDSURFACEDESC2 lock{ sizeof(lock) };
            hr = back->Lock(nullptr, &lock, DDLOCK_WAIT | DDLOCK_WRITEONLY, nullptr);
            Check(SUCCEEDED(hr), "the back buffer can be locked");
            if (SUCCEEDED(hr))
            {
                memset(lock.lpSurface, 0x55, (size_t)lock.lPitch * 10);
                back->Unlock(nullptr);
            }
            Check(SUCCEEDED(primary->Flip(nullptr, DDFLIP_WAIT)), "Flip works");
            back->Release();
        }
        DDBLTFX fx{ sizeof(fx) };
        fx.dwFillColor = 1;
        Check(SUCCEEDED(primary->Blt(nullptr, nullptr, nullptr, DDBLT_COLORFILL | DDBLT_WAIT, &fx)), "Blt to the primary works");

        DDSURFACEDESC2 od{ sizeof(od) };
        od.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
        od.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;
        od.dwWidth = od.dwHeight = 100;
        IDirectDrawSurface7* sprite = nullptr;
        if (SUCCEEDED(dd->CreateSurface(&od, &sprite, nullptr)) && sprite)
        {
            RECT edge{ 600, 440, 700, 540 }, outside{ 700, 500, 800, 600 };
            hr = primary->Blt(&edge, sprite, nullptr, DDBLT_WAIT, nullptr);
            Check(SUCCEEDED(hr), "a Blt reaching past the screen edge draws its visible part", "hr=" + Str(hr));
            hr = primary->Blt(&outside, sprite, nullptr, DDBLT_WAIT, nullptr);
            Check(SUCCEEDED(hr), "a Blt entirely off screen is a no-op", "hr=" + Str(hr));
            hr = primary->BltFast(600, 440, sprite, nullptr, DDBLTFAST_WAIT);
            Check(SUCCEEDED(hr), "a BltFast reaching past the screen edge is clipped", "hr=" + Str(hr));
            sprite->Release();
        }
        Check(SUCCEEDED(primary->IsLost()), "the primary is not lost");
        Check(primary->Release() == 0, "the primary is released");
        Check(SUCCEEDED(dd->RestoreDisplayMode()), "RestoreDisplayMode works");
        CheckDesktopUnchanged(before);
        dd->Release();
    }

    void ScenarioDDrawNormal()
    {
        HWND hwnd = MakeWindow();
        IDirectDraw7* dd = nullptr;
        if (FAILED(DirectDrawCreateEx(nullptr, (void**)&dd, IID_IDirectDraw7, nullptr))) Skip("DirectDraw not available");
        if (!InThisModule((*(void***)dd)[6])) { Check(false, "CreateSurface is patched"); ExitProcess(1); }
        Check(SUCCEEDED(dd->SetCooperativeLevel(hwnd, DDSCL_NORMAL)), "normal cooperative level");
        DDSURFACEDESC2 sd{ sizeof(sd) };
        sd.dwFlags = DDSD_CAPS;
        sd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
        IDirectDrawSurface7* primary = nullptr;
        Check(SUCCEEDED(dd->CreateSurface(&sd, &primary, nullptr)) && primary, "the primary is created");
        if (!primary) ExitProcess(1);
        // The real primary is the whole desktop and can't be resized, the fake one is off-screen memory
        DDSURFACEDESC2 pd{ sizeof(pd) };
        primary->GetSurfaceDesc(&pd);
        Check((pd.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) && !(pd.ddsCaps.dwCaps & DDSCAPS_OFFSCREENPLAIN),
              "an already windowed game gets the real primary", "caps=" + Str(pd.ddsCaps.dwCaps));
        RECT client{};
        GetClientRect(hwnd, &client);
        Check(client.right == 640 && client.bottom == 480, "its window is left alone", Str(client.right) + "x" + Str(client.bottom));
        primary->Release();
        dd->Release();
    }

    // --- other features

    void ScenarioSpeedHack()
    {
        // Speed hack applies to the thread that set the graphics mode
        HWND hwnd = MakeWindow();
        IDirectDraw7* dd = nullptr;
        if (FAILED(DirectDrawCreateEx(nullptr, (void**)&dd, IID_IDirectDraw7, nullptr))) Skip("DirectDraw not available");
        if (!InThisModule((*(void***)dd)[21])) { Check(false, "SetDisplayMode is patched"); ExitProcess(1); }
        dd->SetCooperativeLevel(hwnd, DDSCL_FULLSCREEN | DDSCL_EXCLUSIVE);
        dd->SetDisplayMode(640, 480, 32, 0, 0);
        Check(IsDetoured(Export(L"kernel32.dll", "GetTickCount")) && IsDetoured(Export(L"kernel32.dll", "QueryPerformanceCounter")), "the timers are hooked");

        DWORD t0 = GetTickCount();
        LARGE_INTEGER q0, q1, f;
        QueryPerformanceCounter(&q0);
        ULONGLONG r0 = GetTickCount64();
        Sleep(400);
        DWORD t1 = GetTickCount();
        QueryPerformanceCounter(&q1);
        QueryPerformanceFrequency(&f);
        double real = (double)(GetTickCount64() - r0);
        double tick = t1 - t0, qpc = (q1.QuadPart - q0.QuadPart) * 1000.0 / f.QuadPart;
        Check(tick / real > 1.7 && tick / real < 2.3, "GetTickCount runs at 2x on the render thread", Str((long long)tick) + " vs " + Str((long long)real));
        Check(qpc / real > 1.7 && qpc / real < 2.3, "QueryPerformanceCounter runs at 2x on the render thread", Str((long long)qpc) + " vs " + Str((long long)real));

        double other = 0;
        std::thread([&] {
            DWORD a = GetTickCount();
            ULONGLONG b = GetTickCount64();
            Sleep(300);
            other = (double)(GetTickCount() - a) / (double)(GetTickCount64() - b);
        }).join();
        Check(other > 0.8 && other < 1.2, "other threads see real time", std::to_string(other));
        dd->Release();
    }

    void ScenarioCursorGet()
    {
        HWND hwnd = MakeWindow();
        DEVMODEA dm{ .dmSize = sizeof(DEVMODEA) };
        dm.dmPelsWidth = 640;
        dm.dmPelsHeight = 480;
        dm.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
        ChangeDisplaySettingsA(&dm, CDS_FULLSCREEN); // makes hwnd the game window
        Pump();
        CURSORINFO ci{ sizeof(ci) };
        GetCursorInfo(&ci); // not hooked: screen coordinates
        POINT expected = ci.ptScreenPos;
        ScreenToClient(hwnd, &expected);
        POINT got{};
        GetCursorPos(&got);
        Check(abs(got.x - expected.x) <= 2 && abs(got.y - expected.y) <= 2, "GetCursorPos returns client coordinates",
              Str(got.x) + "," + Str(got.y) + " vs " + Str(expected.x) + "," + Str(expected.y));
    }

    LRESULT CALLBACK LLHook(int c, WPARAM w, LPARAM l) { return CallNextHookEx(nullptr, c, w, l); }

    void ScenarioExtraKey()
    {
        HHOOK h = SetWindowsHookExW(WH_KEYBOARD_LL, LLHook, GetModuleHandleW(nullptr), 0);
        Check(h != nullptr, "a low-level keyboard hook request reports success");
        Check(UnhookWindowsHookEx(h) != FALSE, "and can be removed");
        HHOOK k = SetWindowsHookExW(WH_KEYBOARD, LLHook, nullptr, 0); // global hook without a DLL is only valid as a thread hook
        Check(k != nullptr, "a global keyboard hook becomes a thread hook");
        if (k) UnhookWindowsHookEx(k);
    }

    void ScenarioFps()
    {
        HWND hwnd = MakeWindow();
        IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
        if (!d3d) Skip("Direct3DCreate9 failed");
        IDirect3DDevice9* dev = CreateD3D9Fullscreen(d3d, hwnd, 640, 480);
        if (!dev) ExitProcess(1);
        ULONGLONG end = GetTickCount64() + 1500;
        while (GetTickCount64() < end)
        {
            dev->Present(nullptr, nullptr, nullptr, nullptr);
            Pump();
            Sleep(10);
        }
        wchar_t title[256];
        GetWindowTextW(hwnd, title, 256);
        Check(wcsstr(title, L"fps)") != nullptr, "the window title shows the frame rate", ut::narrow(title));
        dev->Release();
        d3d->Release();
    }

    void ScenarioDInput()
    {
        HWND hwnd = MakeWindow();
        IDirectInput8W* di = nullptr;
        if (FAILED(DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W, (void**)&di, nullptr))) Skip("DirectInput8 not available");
        IDirectInputDevice8W* kb = nullptr;
        if (FAILED(di->CreateDevice(GUID_SysKeyboard, &kb, nullptr))) Skip("no keyboard device");
        Check(InThisModule((*(void***)kb)[13]), "the device is patched");
        kb->SetDataFormat(&c_dfDIKeyboard);
        // Exclusive background keyboard access is invalid in DirectInput and gets rewritten to non-exclusive
        HRESULT hr = kb->SetCooperativeLevel(hwnd, DISCL_BACKGROUND | DISCL_EXCLUSIVE);
        Check(SUCCEEDED(hr), "DISCL_BACKGROUND|DISCL_EXCLUSIVE is accepted (rewritten)", "hr=" + Str(hr));
        BYTE keys[256];
        Check(SUCCEEDED(kb->GetDeviceState(sizeof(keys), keys)), "GetDeviceState works");
        kb->Release();
        di->Release();
    }

    int Run(const std::string& scenario, const std::wstring& ini)
    {
        if (!wndmode::Initialize(ini)) { Check(false, "Initialize"); return 1; }
        if (scenario == "d3d9") ScenarioD3D9();
        else if (scenario == "d3d9thread") ScenarioD3D9Thread();
        else if (scenario == "ddrawnormal") ScenarioDDrawNormal();
        else if (scenario == "d3d9ex") ScenarioD3D9Ex();
        else if (scenario == "d3d8") ScenarioD3D8();
        else if (scenario == "display") ScenarioDisplayChange();
        else if (scenario == "ddraw16") ScenarioDDraw(16, false);
        else if (scenario == "ddraw8") ScenarioDDraw(8, false);
        else if (scenario == "ddraw32_7") ScenarioDDraw(32, true);
        else if (scenario == "speedhack") ScenarioSpeedHack();
        else if (scenario == "cursorget") ScenarioCursorGet();
        else if (scenario == "extrakey") ScenarioExtraKey();
        else if (scenario == "fps") ScenarioFps();
        else if (scenario == "dinput") ScenarioDInput();
        else { Check(false, "unknown scenario"); }
        printf("DONE\n");
        return g_failures;
    }
}

// --- parent side

namespace
{
    struct ChildResult
    {
        DWORD exitCode = 0;
        bool timedOut = false;
        std::string output;
    };

    ChildResult RunChild(const std::string& scenario, const std::string& ini)
    {
        auto dir = fs::temp_directory_path() / L"ual-wndmode" / (std::to_wstring(GetCurrentProcessId()) + L"-" + ut::widen(scenario));
        std::error_code ec;
        fs::create_directories(dir, ec);
        auto iniPath = dir / L"wndmode.ini";
        std::ofstream(iniPath, std::ios::binary) << ini;

        wchar_t self[MAX_PATH];
        GetModuleFileNameW(nullptr, self, MAX_PATH);
        std::wstring cmd = L"\"" + std::wstring(self) + L"\" --child " + ut::widen(scenario) + L" \"" + iniPath.wstring() + L"\"";

        SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
        HANDLE rd, wr;
        CreatePipe(&rd, &wr, &sa, 0);
        SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
        STARTUPINFOW si{ sizeof(si) };
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = si.hStdError = wr;
        PROCESS_INFORMATION pi{};
        ChildResult r;
        if (!CreateProcessW(self, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, dir.c_str(), &si, &pi))
            FAIL("cannot start the child process");
        CloseHandle(wr);
        char buf[4096];
        DWORD n;
        ULONGLONG deadline = GetTickCount64() + 60000;
        for (;;)
        {
            DWORD avail = 0;
            if (PeekNamedPipe(rd, nullptr, 0, nullptr, &avail, nullptr) && avail && ReadFile(rd, buf, (std::min)((DWORD)sizeof(buf), avail), &n, nullptr))
                r.output.append(buf, n);
            if (WaitForSingleObject(pi.hProcess, 20) == WAIT_OBJECT_0)
            {
                while (ReadFile(rd, buf, sizeof(buf), &n, nullptr) && n) r.output.append(buf, n);
                break;
            }
            if (GetTickCount64() > deadline)
            {
                TerminateProcess(pi.hProcess, 0xDEAD);
                r.timedOut = true;
                break;
            }
        }
        GetExitCodeProcess(pi.hProcess, &r.exitCode);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(rd);
        if (!ut::options().keep) fs::remove_all(dir, ec);
        return r;
    }

    void ExpectChild(const std::string& scenario, const std::string& ini)
    {
        auto r = RunChild(scenario, ini);
        std::istringstream in(r.output);
        std::string line;
        bool done = false;
        int checks = 0;
        while (std::getline(in, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.rfind("SKIP\t", 0) == 0) SKIP(line.substr(5));
            if (line == "DONE") done = true;
            if (line.rfind("PASS\t", 0) == 0) ++checks;
            if (line.rfind("FAIL\t", 0) == 0)
            {
                ++checks;
                FAIL_CHECK(line.substr(5));
            }
        }
        if (r.timedOut) FAIL("the scenario timed out\n" + r.output);
        if (!done) FAIL("the scenario did not complete (exit code " + std::to_string(r.exitCode) + ")\n" + r.output);
        CHECK(checks > 0);
    }

    const char* kBaseIni = "[WINDOWMODE]\r\nUseWindowMode=1\r\nUseGDI=1\r\nUseDirect3D=1\r\nUseDirectDraw=1\r\nBorder=1\r\n";
}

TEST_CASE("Direct3D 9: fullscreen devices are created windowed, Reset resizes the window", "[wndmode][d3d9]")
{
    ExpectChild("d3d9", kBaseIni);
}

TEST_CASE("Direct3D 9: a device created on a render thread restyles the window on its own thread", "[wndmode][d3d9]")
{
    ExpectChild("d3d9thread", kBaseIni);
}

TEST_CASE("Direct3D 9Ex: CreateDeviceEx / ResetEx with a fullscreen mode run windowed", "[wndmode][d3d9]")
{
    ExpectChild("d3d9ex", kBaseIni);
}

TEST_CASE("Direct3D 8: fullscreen devices are created windowed (d3d8.dll loaded late)", "[wndmode][d3d8]")
{
    ExpectChild("d3d8", kBaseIni);
}

TEST_CASE("GDI: ChangeDisplaySettings resizes the window instead of the desktop", "[wndmode][gdi]")
{
    ExpectChild("display", kBaseIni);
}

TEST_CASE("DirectDraw: 16-bit fullscreen mode is emulated in a window", "[wndmode][ddraw]")
{
    ExpectChild("ddraw16", kBaseIni);
}

TEST_CASE("DirectDraw: 8-bit palettised mode is emulated in a window", "[wndmode][ddraw]")
{
    ExpectChild("ddraw8", kBaseIni);
}

TEST_CASE("DirectDraw 7: 32-bit fullscreen mode is emulated in a window", "[wndmode][ddraw]")
{
    ExpectChild("ddraw32_7", kBaseIni);
}

TEST_CASE("DirectDraw: an already windowed (DDSCL_NORMAL) game keeps the real primary", "[wndmode][ddraw]")
{
    ExpectChild("ddrawnormal", kBaseIni);
}

TEST_CASE("UseSpeedHack scales the render thread's timers", "[wndmode][speedhack]")
{
    ExpectChild("speedhack", std::string(kBaseIni) + "UseSpeedHack=1\r\nSpeedHackMultiple=20\r\n");
}

TEST_CASE("UseCursorGet: GetCursorPos returns client coordinates", "[wndmode][cursor]")
{
    ExpectChild("cursorget", std::string(kBaseIni) + "UseCursorGet=1\r\n");
}

TEST_CASE("EnableExtraKey: games cannot install system-wide keyboard hooks", "[wndmode][keys]")
{
    ExpectChild("extrakey", std::string(kBaseIni) + "EnableExtraKey=1\r\n");
}

TEST_CASE("ShowFps: the frame rate is shown in the window title", "[wndmode][fps]")
{
    ExpectChild("fps", std::string(kBaseIni) + "ShowFps=1\r\n");
}

TEST_CASE("UseDirectInput: exclusive cooperative levels become non-exclusive", "[wndmode][dinput]")
{
    ExpectChild("dinput", std::string(kBaseIni) + "UseDirectInput=1\r\n");
}

// --- configuration

TEST_CASE("config: missing keys use the legacy code defaults", "[wndmode][config]")
{
    auto c = wndmode::ParseConfig("", L"C:\\game.exe");
    CHECK(c.UseWindowMode);
    CHECK(c.UseGDI);
    CHECK(c.UseDirect3D);
    CHECK(!c.UseDirectInput);
    CHECK(c.UseDirectDraw);
    CHECK(c.UseDDrawColorConvert);
    CHECK(c.UseDDrawPrimaryBlt);
    CHECK(c.UseDDrawColorEmulate);
    CHECK(c.Border);
    CHECK_EQ(c.SpeedHackMultiple, 10);
    CHECK_EQ(c.DDrawBltWait, -1);
    CHECK(c.UseFakeScreenMetrics);
    CHECK_EQ(c.DpiAware, 0);
}

TEST_CASE("config: UseFakeScreenMetrics and DpiAware", "[wndmode][config]")
{
    auto c = wndmode::ParseConfig("[WINDOWMODE]\nUseFakeScreenMetrics=0\nDpiAware=2\n", L"x");
    CHECK(!c.UseFakeScreenMetrics);
    CHECK_EQ(c.DpiAware, 2);
}

TEST_CASE("config: the shipped wndmode.ini parses as documented", "[wndmode][config]")
{
    std::ifstream f(fs::path(ut::widen(__FILE__)).parent_path().parent_path().parent_path() / L"source/loader/resources/wndmode.ini", std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    REQUIRE(!ss.str().empty());
    auto c = wndmode::ParseConfig(ss.str(), L"C:\\game.exe");
    CHECK(c.UseWindowMode);
    CHECK(!c.UseDirectDraw);
    CHECK(!c.Border);
    CHECK(!c.ShowFps);
    CHECK_EQ(c.SpeedHackMultiple, 10);
}

TEST_CASE("config: keys and sections are case-insensitive, the first occurrence wins, ; comments", "[wndmode][config]")
{
    auto c = wndmode::ParseConfig("; comment\r\n[windowMode]\r\nuseGDI = 0\r\nUSEGDI=1\r\nBorder=0 \r\n", L"x.exe");
    CHECK(!c.UseGDI);
    CHECK(!c.Border);
}

TEST_CASE("config: integers accept decimal, $hex and 0x hex; invalid values use the default", "[wndmode][config]")
{
    CHECK_EQ(wndmode::ParseConfig("[WINDOWMODE]\nSpeedHackMultiple=$14\n", L"x").SpeedHackMultiple, 20);
    CHECK_EQ(wndmode::ParseConfig("[WINDOWMODE]\nSpeedHackMultiple=0x1E\n", L"x").SpeedHackMultiple, 30);
    CHECK_EQ(wndmode::ParseConfig("[WINDOWMODE]\nSpeedHackMultiple=fast\n", L"x").SpeedHackMultiple, 10);
    CHECK_EQ(wndmode::ParseConfig("[WINDOWMODE]\nDDrawBltWait=-1\n", L"x").DDrawBltWait, -1);
    CHECK_EQ(wndmode::ParseConfig("[WINDOWMODE]\nSpeedHackMultiple=0\n", L"x").SpeedHackMultiple, 10);
}

TEST_CASE("config: per-executable profiles select [WINDOWMODE<suffix>]", "[wndmode][config]")
{
    std::string ini = "[wndmode.ini]\r\nC:\\Games\\Foo\\foo.exe=_FOO\r\n[WINDOWMODE]\r\nBorder=1\r\n[WINDOWMODE_FOO]\r\nBorder=0\r\nShowFps=1\r\n";
    auto foo = wndmode::ParseConfig(ini, L"c:\\games\\foo\\FOO.EXE");
    CHECK(!foo.Border);
    CHECK(foo.ShowFps);
    auto other = wndmode::ParseConfig(ini, L"C:\\Games\\Bar\\bar.exe");
    CHECK(other.Border);
    CHECK(!other.ShowFps);
}

TEST_CASE("config: SubModule0..N and MenuId", "[wndmode][config]")
{
    auto c = wndmode::ParseConfig("[WINDOWMODE]\nSubModule0=a.dll\nSubModule1=dir\\b.dll\nSubModule3=skipped.dll\nMenuId=MAINMENU\n", L"x");
    REQUIRE_EQ(c.SubModules.size(), (size_t)2);
    CHECK(c.SubModules[0] == L"a.dll");
    CHECK(c.SubModules[1] == L"dir\\b.dll");
    CHECK_EQ(c.MenuId, std::string("MAINMENU"));
}

TEST_CASE("config: UTF-8 files with BOM", "[wndmode][config]")
{
    auto c = wndmode::ParseConfig("\xEF\xBB\xBF[WINDOWMODE]\nShowFps=1\n", L"x");
    CHECK(c.ShowFps);
}

int wmain(int argc, wchar_t** argv)
{
    if (argc >= 4 && std::wstring(argv[1]) == L"--child")
    {
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
        return child::Run(ut::narrow(argv[2]), argv[3]);
    }
    return ut::run(argc, argv, "wndmode");
}
