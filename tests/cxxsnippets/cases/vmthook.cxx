#include <windows.h>
#include <safetyhook.hpp>
SafetyHookVmt vmt{};
SafetyHookVm method{};
#ifdef _WIN64
int HookMethod(void *self, int x)
{
    return method.thiscall<int>(self, x) + 1337;
}
#else
int __fastcall HookMethod(void *self, void *unused, int x)
{
    return method.thiscall<int>(self, x) + 1337;
}
#endif
void Init()
{
    auto get = reinterpret_cast<void *(*)()>(GetProcAddress(GetModuleHandleA(nullptr), "case_object"));
    vmt = safetyhook::create_vmt(get());
    method = safetyhook::create_vm(vmt, 0, HookMethod);
}
void Shutdown()
{
    method = {};
    vmt = {};
}
