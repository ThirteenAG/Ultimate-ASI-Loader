#include <windows.h>
#include <safetyhook.hpp>
SafetyHookInline g_hook{};
int HookAdd(int a, int b)
{
    return g_hook.call<int>(a * 2, b * 2);
}
void Init()
{
    g_hook = safetyhook::create_inline(GetProcAddress(GetModuleHandleA(nullptr), "case_add"), HookAdd);
}
void Shutdown()
{
    g_hook = {};
}
