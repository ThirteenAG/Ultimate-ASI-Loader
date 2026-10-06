#include <windows.h>
#include <safetyhook.hpp>
SafetyHookMid g_hook{};
void Mid(SafetyHookContext &ctx)
{
#if SAFETYHOOK_ARCH_X86_64
    ctx.rax = 1337;
#else
    ctx.eax = 1337;
#endif
}
void Init()
{
    auto get = reinterpret_cast<void *(*)()>(GetProcAddress(GetModuleHandleA(nullptr), "case_mid_site"));
    g_hook = safetyhook::create_mid(get(), Mid);
}
void Shutdown()
{
    g_hook = {};
}
