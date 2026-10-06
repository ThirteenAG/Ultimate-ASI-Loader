// 06 - Hooking functions exported by DLLs (the game's or Windows')
//
// Use for: game DLLs that export their functions (Unreal Engine 1/2 games:
// Engine.dll, Core.dll, D3DDrv.dll, ...), and Windows functions the game
// calls (screen size, cursor clipping, window style, file access, ...).
//
// GetModuleHandle gives the DLL; GetProcAddress gives the function. For C++
// exports use the decorated name shown by a DLL viewer, e.g.
//     ?SetRes@UD3DRenderDevice@@UAEHPAVUViewport@@HHH@Z
//
// Windows functions that the built-in <windows.h> does not declare: declare
// them yourself in an extern "C" block, with WINAPI. The name is looked up in
// the DLLs loaded into the game (kernel32, user32, gdi32, ... are always
// there). Put such declarations in #ifdef __CXXSNIPPETS__: the real
// <windows.h> of a Visual Studio build already has them.
#include <windows.h>
#include <cstdint>
#include <safetyhook.hpp>

#ifdef __CXXSNIPPETS__
struct RECT
{
    LONG left, top, right, bottom;
};
extern "C" {
int WINAPI GetSystemMetrics(int index);
BOOL WINAPI ClipCursor(RECT const *rect);
LONG WINAPI SetWindowLongA(HWND window, int index, LONG value);
}
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1
#define GWL_STYLE (-16)
#endif

SafetyHookInline metricsHook{};
SafetyHookInline clipHook{};
SafetyHookInline setWindowLongHook{};
SafetyHookInline gameSetRes{};

// Windows: fake a screen size, call the original for everything else.
int WINAPI GetSystemMetricsHook(int index)
{
    if (index == SM_CXSCREEN)
        return 2560;
    if (index == SM_CYSCREEN)
        return 1080;
    return metricsHook.stdcall<int>(index);
}

// Windows: do nothing at all (full replacement), so the game can't lock the cursor.
BOOL WINAPI ClipCursorHook(RECT const *rect)
{
    return TRUE;
}

// Windows: change one argument.
LONG WINAPI SetWindowLongHook(HWND window, int index, LONG value)
{
    if (index == GWL_STYLE)
        value &= ~0x00C00000; // no WS_CAPTION: borderless
    return setWindowLongHook.stdcall<LONG>(window, index, value);
}

// Game DLL: int UD3DRenderDevice::SetRes(UViewport*, int width, int height, int fullscreen)
#ifdef _WIN64
int SetRes(void *self, void *viewport, int width, int height, int fullscreen)
#else
int __fastcall SetRes(void *self, void *edx, void *viewport, int width, int height, int fullscreen)
#endif
{
    return gameSetRes.thiscall<int>(self, viewport, width, height, fullscreen);
}

void Init()
{
    // Windows functions: the address from the declaration above.
    metricsHook = safetyhook::create_inline(GetSystemMetrics, GetSystemMetricsHook);
    clipHook = safetyhook::create_inline(ClipCursor, ClipCursorHook);
    setWindowLongHook = safetyhook::create_inline(SetWindowLongA, SetWindowLongHook);

    // Or by name from a module handle (the same for any DLL).
    //     GetProcAddress(GetModuleHandleA("user32.dll"), "GetSystemMetrics")

    // A game DLL.
    HMODULE d3ddrv = GetModuleHandleA("D3DDrv.dll");
    if (d3ddrv)
    {
        void *setRes = (void *)GetProcAddress(d3ddrv, "?SetRes@UD3DRenderDevice@@UAEHPAVUViewport@@HHH@Z");
        if (setRes)
            gameSetRes = safetyhook::create_inline(setRes, SetRes);
    }
}
