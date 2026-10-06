#include <windows.h>
#include <safetyhook.hpp>
SafetyHookInline a{}, w{};
BOOL WINAPI A(MSG *msg, HWND hwnd, UINT first, UINT last, UINT remove)
{
    return a.stdcall<BOOL>(msg, hwnd, first, last, remove);
}
BOOL WINAPI W(MSG *msg, HWND hwnd, UINT first, UINT last, UINT remove)
{
    return w.stdcall<BOOL>(msg, hwnd, first, last, remove);
}
void Init()
{
    a = safetyhook::create_inline(PeekMessageA, A);
    w = safetyhook::create_inline(PeekMessageW, W);
}
void Shutdown()
{
    w = {};
    a = {};
}
