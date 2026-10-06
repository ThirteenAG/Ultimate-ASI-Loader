// Game window handling: restyle/centre, window procedure subclass, FPS in the
// title, foreground control, cursor and keyboard hooks.
#include "internal.hpp"
#include <algorithm>
#include <cwchar>

namespace wndmode
{
    namespace
    {
        struct Subclass
        {
            HWND hwnd;
            WNDPROC prev;
            bool unicode;
            int activations;
        };

        SRWLOCK g_lock = SRWLOCK_INIT;
        std::vector<Subclass> g_subs;
        HWND g_main = nullptr;
        HWND g_frontDone = nullptr; // window already brought to the front once
        HMONITOR g_monitor = nullptr;
        int g_width = 0, g_height = 0;
        volatile bool g_active = true;
        bool g_inSizeMove = false;
        volatile LONG g_deviceChange = 0;
        UINT g_restyleMsg = 0;

        decltype(&GetAsyncKeyState) oGetAsyncKeyState;
        decltype(&SetForegroundWindow) oSetForegroundWindow;
        using SwitchToThisWindowFn = void(WINAPI*)(HWND, BOOL);
        SwitchToThisWindowFn oSwitchToThisWindow;
        decltype(&GetForegroundWindow) oGetForegroundWindow;
        decltype(&GetActiveWindow) oGetActiveWindow;
        decltype(&GetWindowRect) oGetWindowRect;
        decltype(&SetWindowsHookExA) oSetWindowsHookExA;
        decltype(&SetWindowsHookExW) oSetWindowsHookExW;
        decltype(&UnhookWindowsHookEx) oUnhookWindowsHookEx;
        decltype(&SetCursorPos) oSetCursorPos;
        decltype(&GetCursorPos) oGetCursorPos;

        constexpr UINT_PTR kFakeHookTag = 0x7EA70000;
        volatile LONG g_fakeHooks = 0;

        void Lock() { AcquireSRWLockExclusive(&g_lock); }
        void Unlock() { ReleaseSRWLockExclusive(&g_lock); }

        Subclass* FindSub(HWND hwnd)
        {
            for (auto& s : g_subs)
                if (s.hwnd == hwnd) return &s;
            return nullptr;
        }

        bool OwnWindow(HWND h)
        {
            DWORD pid = 0;
            return h && GetWindowThreadProcessId(h, &pid) && pid == GetCurrentProcessId();
        }

        HWND RealForeground()
        {
            return oGetForegroundWindow ? oGetForegroundWindow() : GetForegroundWindow();
        }

        // Only a visible window that really has the focus confines the cursor.
        void ClipToClient(HWND hwnd)
        {
            RECT r;
            if (IsIconic(hwnd) || !IsWindowVisible(hwnd) || RealForeground() != hwnd || !GetClientRect(hwnd, &r)) return;
            MapWindowPoints(hwnd, nullptr, (POINT*)&r, 2);
            ClipCursor(&r);
        }

        BOOL CALLBACK FirstIcon(HMODULE, LPCWSTR, LPWSTR name, LONG_PTR param)
        {
            *(LPWSTR*)param = name;
            return FALSE;
        }

        // Windows with no icon, or the stock application icon, get the exe's icon.
        void FixIcon(HWND hwnd)
        {
            HICON cur = (HICON)GetClassLongPtrW(hwnd, GCLP_HICON);
            if (cur && cur != LoadIconW(nullptr, IDI_APPLICATION) && cur != LoadIconW(nullptr, IDI_WINLOGO))
                return;
            HMODULE exe = GetModuleHandleW(nullptr);
            LPWSTR name = nullptr;
            EnumResourceNamesW(exe, RT_GROUP_ICON, FirstIcon, (LONG_PTR)&name);
            if (!name) return;
            if (HICON icon = LoadIconW(exe, name))
            {
                SetClassLongPtrW(hwnd, GCLP_HICON, (LONG_PTR)icon);
                SetClassLongPtrW(hwnd, GCLP_HICONSM, (LONG_PTR)icon);
            }
        }

        void ApplyMenu(HWND hwnd)
        {
            if (g_cfg.MenuId.empty()) return;
            if (HMENU menu = LoadMenuA(GetModuleHandleW(nullptr), g_cfg.MenuId.c_str()))
            {
                HMENU old = GetMenu(hwnd);
                SetMenu(hwnd, menu);
                if (old) DestroyMenu(old);
            }
        }

        // Windowed style + client size w x h, centred on the monitor.
        void AdjustWindow(HWND hwnd, int w, int h, bool move, HMONITOR monitor = nullptr)
        {
            // a minimised/maximised window would ignore (or later undo) the new size
            if (IsIconic(hwnd) || IsZoomed(hwnd)) ShowWindow(hwnd, SW_RESTORE);

            LONG style = GetWindowLongW(hwnd, GWL_STYLE);
            LONG exstyle = GetWindowLongW(hwnd, GWL_EXSTYLE);
            MONITORINFO mi{ sizeof(mi) };
            if (!monitor || !GetMonitorInfoW(monitor, &mi))
                GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);
            const RECT& work = mi.rcWork;

            RECT r{ 0, 0, w, h };
            if (w <= 0 || h <= 0) GetClientRect(hwnd, &r);
            if (!(style & WS_CHILD))
            {
                style &= ~(WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
                style |= WS_VISIBLE | WS_SYSMENU | WS_MINIMIZEBOX;
                RECT test = r;
                AdjustWindowRectEx(&test, style | WS_CAPTION, GetMenu(hwnd) != nullptr, exstyle);
                if (g_cfg.Border && (test.right - test.left) <= (work.right - work.left) && (test.bottom - test.top) <= (work.bottom - work.top))
                    style |= WS_CAPTION; // a title bar only when the window still fits
                SetWindowLongW(hwnd, GWL_STYLE, style);

                // tool windows and owned windows are missing from the taskbar and Alt+Tab
                LONG ex = exstyle & ~WS_EX_TOOLWINDOW;
                if (GetWindow(hwnd, GW_OWNER)) ex |= WS_EX_APPWINDOW;
                if (ex != exstyle) SetWindowLongW(hwnd, GWL_EXSTYLE, exstyle = ex);
            }
            AdjustWindowRectEx(&r, style, GetMenu(hwnd) != nullptr, exstyle);
            int cw = r.right - r.left, ch = r.bottom - r.top;
            int x = work.left + (std::max)(0L, ((work.right - work.left) - cw) / 2);
            int y = work.top + (std::max)(0L, ((work.bottom - work.top) - ch) / 2);
            UINT flags = SWP_NOACTIVATE | SWP_FRAMECHANGED | SWP_NOOWNERZORDER | SWP_NOSENDCHANGING;
            if (!move) flags |= SWP_NOMOVE;
            if (GetWindowThreadProcessId(hwnd, nullptr) != GetCurrentThreadId())
                flags |= SWP_ASYNCWINDOWPOS; // not subclassed: never block on the window thread
            SetWindowPos(hwnd, HWND_NOTOPMOST, x, y, cw, ch, flags);

            // games started from a launcher would otherwise open behind it
            if (move && g_frontDone != hwnd && hwnd == g_main)
            {
                g_frontDone = hwnd;
                if (RealForeground() != hwnd)
                    oSetForegroundWindow ? oSetForegroundWindow(hwnd) : SetForegroundWindow(hwnd);
            }
        }

        // client size actually in effect (a request of 0 x 0 keeps the current size)
        void StoreClientSize(HWND hwnd)
        {
            RECT r;
            if (!GetClientRect(hwnd, &r) || r.right <= 0 || r.bottom <= 0) return;
            Lock();
            if (g_main == hwnd && (g_width <= 0 || g_height <= 0)) g_width = r.right, g_height = r.bottom;
            Unlock();
        }

        void Restyle(HWND hwnd, int w, int h, HMONITOR monitor)
        {
            ApplyMenu(hwnd);
            FixIcon(hwnd);
            AdjustWindow(hwnd, w, h, true, monitor);
            StoreClientSize(hwnd);
            if (g_cfg.UseCursorClip && g_active) ClipToClient(hwnd);
        }

        bool IsSizeMessage(UINT msg)
        {
            switch (msg)
            {
            case WM_SIZE: case WM_MOVE: case WM_SIZING: case WM_MOVING: case WM_WINDOWPOSCHANGING: case WM_WINDOWPOSCHANGED:
            case WM_STYLECHANGING: case WM_STYLECHANGED: case WM_NCCALCSIZE: case WM_GETMINMAXINFO:
                return true;
            }
            return false;
        }

        LRESULT CallPrev(const Subclass& s, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
        {
            return s.unicode ? CallWindowProcW(s.prev, hwnd, msg, wp, lp) : CallWindowProcA(s.prev, hwnd, msg, wp, lp);
        }

        LRESULT Def(const Subclass& s, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
        {
            return s.unicode ? DefWindowProcW(hwnd, msg, wp, lp) : DefWindowProcA(hwnd, msg, wp, lp);
        }

        LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
        {
            Lock();
            Subclass* found = FindSub(hwnd);
            Subclass s = found ? *found : Subclass{};
            Unlock();
            if (!s.prev) return DefWindowProcW(hwnd, msg, wp, lp);

            if (msg == WM_NCDESTROY)
            {
                Lock();
                for (size_t i = 0; i < g_subs.size(); ++i)
                    if (g_subs[i].hwnd == hwnd) { g_subs.erase(g_subs.begin() + i); break; }
                if (g_main == hwnd) g_main = nullptr;
                Unlock();
                if (g_cfg.UseCursorClip) ClipCursor(nullptr);
                SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)s.prev);
                return CallPrev(s, hwnd, msg, wp, lp);
            }

            if (msg == g_restyleMsg && g_restyleMsg)
            {
                // SetMainWindow called from another thread
                bool quiet = (wp >> 30) & 1;
                if (quiet) BeginDeviceChange();
                Restyle(hwnd, (int)(wp & 0x7FFF), (int)((wp >> 15) & 0x7FFF), (HMONITOR)lp);
                if (quiet) EndDeviceChange();
                return 0;
            }

            if (g_cfg.UseDDrawAutoBlt) DDrawAutoPresent();

            // Hide our window changes from the game while it creates or resets a device,
            // or it would reset again or move the window back.
            if (g_deviceChange > 0)
            {
                if (IsSizeMessage(msg)) return Def(s, hwnd, msg, wp, lp);
                if (msg == WM_ERASEBKGND) return TRUE;
            }

            switch (msg)
            {
            case WM_ACTIVATEAPP:
            {
                bool active = wp != 0;
                g_active = active;
                if (g_cfg.UseCursorClip)
                {
                    if (active) ClipToClient(hwnd);
                    else ClipCursor(nullptr);
                }
                if (g_cfg.UseBackgroundPriority)
                    SetPriorityClass(GetCurrentProcess(), active ? NORMAL_PRIORITY_CLASS : IDLE_PRIORITY_CLASS);
                if (g_cfg.UseBackgroundResize && hwnd == g_main && g_width > 0 && g_height > 0)
                    AdjustWindow(hwnd, active ? g_width : g_width / 2, active ? g_height : g_height / 2, false);
                if (g_cfg.UseForegroundControl && !active)
                    return Def(s, hwnd, msg, wp, lp); // the game never learns it lost focus
                break;
            }
            case WM_NCACTIVATE:
                if (g_cfg.UseForegroundControl && !wp) return Def(s, hwnd, msg, wp, lp);
                break;
            case WM_KILLFOCUS:
                if (g_cfg.UseForegroundControl) return Def(s, hwnd, msg, wp, lp); // many games pause on this alone
                break;
            case WM_SHOWWINDOW:
                if (!wp && g_cfg.UseCursorClip) ClipCursor(nullptr);
                break;
            case WM_PAINT:
                if (IsIconic(hwnd)) return Def(s, hwnd, msg, wp, lp); // some games hang painting a minimised window
                break;
            case WM_SYNCPAINT:
                return Def(s, hwnd, msg, wp, lp);
            case WM_ACTIVATE:
                if (g_cfg.UseForegroundControl)
                {
                    wp = LOWORD(wp); // drop the "minimized" flag
                    bool first = false;
                    Lock();
                    if (auto p = FindSub(hwnd)) first = p->activations++ == 0;
                    Unlock();
                    if (wp == WA_INACTIVE || !first)
                        return Def(s, hwnd, msg, wp, lp);
                }
                break;
            case WM_NCHITTEST:
            {
                // caption and borders work even if the game handles WM_NCHITTEST itself
                LRESULT hit = Def(s, hwnd, msg, wp, lp);
                if (hit != HTCLIENT) return hit;
                break;
            }
            case WM_SYSCOMMAND:
                if ((wp & 0xFFF0) == SC_MOVE) return Def(s, hwnd, msg, wp, lp);
                if ((wp & 0xFFF0) == SC_MINIMIZE && g_cfg.UseCursorClip) ClipCursor(nullptr);
                break;
            case WM_MOVING:
            {
                RECT copy = *(RECT*)lp; // the game cannot alter the drag rectangle
                return CallPrev(s, hwnd, msg, wp, (LPARAM)&copy);
            }
            case WM_ENTERSIZEMOVE:
                g_inSizeMove = true;
                if (g_cfg.UseCursorClip) ClipCursor(nullptr);
                break;
            case WM_EXITSIZEMOVE:
                g_inSizeMove = false;
                if (g_cfg.UseCursorClip && g_active) ClipToClient(hwnd);
                break;
            case WM_MOVE:
                if (IsIconic(hwnd)) return Def(s, hwnd, msg, wp, lp); // the -32000 position of a minimised window
                if (g_cfg.UseCursorClip && g_active && !g_inSizeMove) ClipToClient(hwnd);
                break;
            case WM_SIZE:
                if (wp == SIZE_MINIMIZED)
                {
                    if (g_cfg.UseCursorClip) ClipCursor(nullptr);
                    if (g_cfg.UseForegroundControl) return Def(s, hwnd, msg, wp, lp);
                }
                else if (g_cfg.UseCursorClip && g_active && !g_inSizeMove)
                    ClipToClient(hwnd);
                break;
            default:
                if (msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST)
                {
                    if (!g_active) return Def(s, hwnd, msg, wp, lp); // no clicks into an inactive game
                    // wheel messages already carry screen coordinates
                    if (g_cfg.UseCursorMsg && msg != WM_MOUSEWHEEL && msg != WM_MOUSEHWHEEL)
                    {
                        POINT pt{ (short)LOWORD(lp), (short)HIWORD(lp) };
                        ClientToScreen(hwnd, &pt);
                        lp = MAKELPARAM(pt.x, pt.y);
                    }
                }
                break;
            }
            return CallPrev(s, hwnd, msg, wp, lp);
        }

        // ---- hooks

        SHORT WINAPI hkGetAsyncKeyState(int key)
        {
            if (g_main && !g_active) return 0;
            return oGetAsyncKeyState(key);
        }

        BOOL WINAPI hkSetForegroundWindow(HWND h)
        {
            if (h && h == g_main) return TRUE;
            return oSetForegroundWindow(h);
        }

        void WINAPI hkSwitchToThisWindow(HWND h, BOOL alt)
        {
            if (OwnWindow(h) && h != g_main) oSwitchToThisWindow(h, alt);
        }

        HWND WINAPI hkGetForegroundWindow()
        {
            HWND real = oGetForegroundWindow();
            return OwnWindow(real) || !g_main ? real : g_main;
        }

        HWND WINAPI hkGetActiveWindow()
        {
            HWND real = oGetActiveWindow();
            return OwnWindow(real) || !g_main ? real : g_main;
        }

        BOOL WINAPI hkGetWindowRect(HWND h, LPRECT r)
        {
            if (h && h == g_main && r)
            {
                *r = { 0, 0, g_width, g_height };
                return IsWindow(h);
            }
            return oGetWindowRect(h, r);
        }

        // EnableExtraKey: games cannot install system-wide keyboard/mouse hooks
        // (which block Alt+Tab / the Windows key). Classic hooks become thread
        // hooks; low-level hooks are not installed and get a dummy handle.
        HHOOK ExtraKeyHook(int id, HOOKPROC proc, HINSTANCE mod, DWORD tid, bool unicode)
        {
            if (tid == 0 && (id == WH_KEYBOARD || id == WH_MOUSE))
                tid = GetCurrentThreadId();
            if (id == WH_KEYBOARD_LL || id == WH_MOUSE_LL)
                return (HHOOK)(kFakeHookTag | (InterlockedIncrement(&g_fakeHooks) & 0xFFFF));
            return unicode ? oSetWindowsHookExW(id, proc, mod, tid) : oSetWindowsHookExA(id, proc, mod, tid);
        }
        HHOOK WINAPI hkSetWindowsHookExA(int id, HOOKPROC proc, HINSTANCE mod, DWORD tid) { return ExtraKeyHook(id, proc, mod, tid, false); }
        HHOOK WINAPI hkSetWindowsHookExW(int id, HOOKPROC proc, HINSTANCE mod, DWORD tid) { return ExtraKeyHook(id, proc, mod, tid, true); }
        BOOL WINAPI hkUnhookWindowsHookEx(HHOOK h)
        {
            if (((UINT_PTR)h & ~(UINT_PTR)0xFFFF) == kFakeHookTag) return TRUE;
            return oUnhookWindowsHookEx(h);
        }

        // UseCursorSet / UseCursorGet: the game works in client coordinates
        BOOL WINAPI hkSetCursorPos(int x, int y)
        {
            if (g_main && !g_active) return TRUE;
            POINT pt{ x, y };
            if (g_main) ClientToScreen(g_main, &pt);
            return oSetCursorPos(pt.x, pt.y);
        }

        BOOL WINAPI hkGetCursorPos(LPPOINT p)
        {
            if (!p) return oGetCursorPos(p);
            if (g_main && !g_active)
            {
                *p = { 0, 0 };
                return TRUE;
            }
            BOOL ok = oGetCursorPos(p);
            if (ok && g_main) ScreenToClient(g_main, p);
            return ok;
        }
    }

    HWND MainWindow() { return g_main; }
    int MainWidth() { return g_width; }
    int MainHeight() { return g_height; }
    bool IsAppActive() { return g_active; }

    void SubclassWindow(HWND hwnd)
    {
        if (!hwnd || !IsWindow(hwnd) || !OwnWindow(hwnd)) return;
        Lock();
        bool already = FindSub(hwnd) != nullptr;
        if (!already)
        {
            bool unicode = IsWindowUnicode(hwnd) != FALSE;
            g_subs.push_back({ hwnd, nullptr, unicode, 0 });
        }
        Unlock();
        if (already) return;
        bool unicode = IsWindowUnicode(hwnd) != FALSE;
        WNDPROC prev = (WNDPROC)(unicode ? SetWindowLongPtrW(hwnd, GWLP_WNDPROC, (LONG_PTR)WndProc)
                                         : SetWindowLongPtrA(hwnd, GWLP_WNDPROC, (LONG_PTR)WndProc));
        Lock();
        if (auto s = FindSub(hwnd)) s->prev = prev;
        Unlock();
    }

    void SetMainWindow(HWND hwnd, int width, int height, HMONITOR monitor)
    {
        if (!hwnd || !IsWindow(hwnd)) return;
        Lock();
        if (monitor) g_monitor = monitor;
        else if (g_main != hwnd) g_monitor = nullptr;
        monitor = g_monitor;
        g_main = hwnd;
        g_width = width;
        g_height = height;
        bool subclassed = FindSub(hwnd) != nullptr;
        Unlock();

        // Window changes are made on the window's own thread: restyling a
        // window whose thread is waiting for us (e.g. for the render thread to
        // finish creating the device) would deadlock.
        if (subclassed && g_restyleMsg && GetWindowThreadProcessId(hwnd, nullptr) != GetCurrentThreadId())
        {
            WPARAM wp = ((WPARAM)(width & 0x7FFF)) | ((WPARAM)(height & 0x7FFF) << 15) | ((WPARAM)(g_deviceChange > 0) << 30);
            if (PostMessageW(hwnd, g_restyleMsg, wp, (LPARAM)monitor)) return;
        }
        Restyle(hwnd, width, height, monitor);
    }

    void BeginDeviceChange() { InterlockedIncrement(&g_deviceChange); }
    void EndDeviceChange() { InterlockedDecrement(&g_deviceChange); }

    void ApplyDpiAwareness()
    {
        if (g_cfg.DpiAware <= 0) return;
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        using SetCtxFn = BOOL(WINAPI*)(HANDLE);
        if (auto setCtx = (SetCtxFn)GetProcAddress(user32, "SetProcessDpiAwarenessContext"))
        {
            // DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 (-4) / _SYSTEM_AWARE (-2)
            if (setCtx(g_cfg.DpiAware >= 2 ? (HANDLE)-4 : (HANDLE)-2)) return;
            if (GetLastError() == ERROR_ACCESS_DENIED) return; // already set (manifest): respect it
            if (g_cfg.DpiAware >= 2 && setCtx((HANDLE)-3)) return; // per monitor v1
        }
        using SetAwareFn = BOOL(WINAPI*)();
        if (auto setAware = (SetAwareFn)GetProcAddress(user32, "SetProcessDPIAware")) setAware();
    }

    void PostDisplayChange(HWND hwnd, int width, int height, int bpp)
    {
        if (hwnd) PostMessageW(hwnd, WM_DISPLAYCHANGE, (WPARAM)bpp, MAKELPARAM(width, height));
    }

    void FpsAttach(Fps& fps, HWND hwnd)
    {
        fps.hwnd = hwnd;
        fps.frames = 0;
        fps.last = RealTickCount();
        fps.title[0] = 0;
        if (hwnd) InternalGetWindowText(hwnd, fps.title, (int)std::size(fps.title));
    }

    void FpsTick(Fps& fps)
    {
        if (!g_cfg.ShowFps || !g_cfg.UseWindowMode || !fps.hwnd) return;
        ++fps.frames;
        ULONGLONG now = RealTickCount();
        if (now - fps.last < 1000) return;
        wchar_t text[300];
        swprintf_s(text, L"%s (%2dfps)", fps.title, (int)(fps.frames * 1000 / (now - fps.last)));
        fps.frames = 0;
        fps.last = now;
        DWORD_PTR res;
        SendMessageTimeoutW(fps.hwnd, WM_SETTEXT, 0, (LPARAM)text, SMTO_ABORTIFHUNG | SMTO_BLOCK, 100, &res);
    }

    void InstallWindowHooks()
    {
        g_restyleMsg = RegisterWindowMessageW(L"UltimateASILoader.wndmode.restyle");
        HookExport(L"user32.dll", "GetAsyncKeyState", (void*)hkGetAsyncKeyState, (void**)&oGetAsyncKeyState);
        if (g_cfg.UseForegroundControl)
        {
            HookExport(L"user32.dll", "SetForegroundWindow", (void*)hkSetForegroundWindow, (void**)&oSetForegroundWindow);
            HookExport(L"user32.dll", "SwitchToThisWindow", (void*)hkSwitchToThisWindow, (void**)&oSwitchToThisWindow);
            if (g_cfg.UseFGCGetForegroundWindow)
                HookExport(L"user32.dll", "GetForegroundWindow", (void*)hkGetForegroundWindow, (void**)&oGetForegroundWindow);
            if (g_cfg.UseFGCGetActiveWindow)
                HookExport(L"user32.dll", "GetActiveWindow", (void*)hkGetActiveWindow, (void**)&oGetActiveWindow);
            if (g_cfg.UseFGCFixedWindowPosition)
                HookExport(L"user32.dll", "GetWindowRect", (void*)hkGetWindowRect, (void**)&oGetWindowRect);
        }
        if (g_cfg.EnableExtraKey)
        {
            HookExport(L"user32.dll", "SetWindowsHookExA", (void*)hkSetWindowsHookExA, (void**)&oSetWindowsHookExA);
            HookExport(L"user32.dll", "SetWindowsHookExW", (void*)hkSetWindowsHookExW, (void**)&oSetWindowsHookExW);
            HookExport(L"user32.dll", "UnhookWindowsHookEx", (void*)hkUnhookWindowsHookEx, (void**)&oUnhookWindowsHookEx);
        }
        if (g_cfg.UseCursorSet)
            HookExport(L"user32.dll", "SetCursorPos", (void*)hkSetCursorPos, (void**)&oSetCursorPos);
        if (g_cfg.UseCursorGet)
            HookExport(L"user32.dll", "GetCursorPos", (void*)hkGetCursorPos, (void**)&oGetCursorPos);
    }
}
