#include <windows.h>
#include <safetyhook.hpp>
SafetyHookInline g_hook{};
int HookAdd(int a, int b)
{
    return g_hook.call<int>(a, b) + 100;
}
DWORD WINAPI Worker(void *unused)
{
    for (int i = 0; i < 30; ++i)
        Sleep(1);
    return 0;
}
void Init()
{
    auto worker = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
    g_hook = safetyhook::create_inline(GetProcAddress(GetModuleHandleA(nullptr), "case_add"), HookAdd,
                                       SafetyHookInline::StartDisabled);
    (void)g_hook.enable();
    WaitForSingleObject(worker, 0xffffffffU);
    CloseHandle(worker);
}
void Shutdown()
{
    g_hook.reset();
}
