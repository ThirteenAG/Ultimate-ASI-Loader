#include <windows.h>
#include <safetyhook.hpp>
SafetyHookInline first{}, second{};
int First(int a, int b)
{
    return first.call<int>(a + 1, b);
}
int Second(int a, int b)
{
    return second.call<int>(a, b) + 10;
}
void Init()
{
    auto target = GetProcAddress(GetModuleHandleA(nullptr), "case_add");
    first = safetyhook::create_inline(target, First);
    second = safetyhook::create_inline(target, Second);
}
void Shutdown()
{
    second = {};
    first = {};
}
