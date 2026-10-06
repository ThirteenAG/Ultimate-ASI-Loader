// 04 - Inline hooks: run your function instead of a game function
//
// Use for: changing the arguments or the result of a function, logging
// calls, or replacing a function completely.
//
// safetyhook::create_inline(target, yourFunction) redirects every call of
// `target` to `yourFunction`. Inside, hook.call/stdcall/thiscall/fastcall<Ret>(args...)
// runs the original function. Don't call it to replace the function completely.
//
// Your function must have the same calling convention and parameters as the
// original. On x64 there is only one calling convention. On x86:
//
//   game function                    your function
//   int __cdecl f(int a)             int f(int a)                                   hook.call<int>(a)
//   int __stdcall f(int a)           int __stdcall f(int a) (WINAPI)                hook.stdcall<int>(a)
//   int __fastcall f(int a, int b)   int __fastcall f(int a, int b)                 hook.fastcall<int>(a, b)
//   int Class::f(int a) (__thiscall) int __fastcall f(void* self, void* edx, int a) hook.thiscall<int>(self, a)
//
// A __thiscall method gets `this` in ecx, which __fastcall also uses for its
// first parameter. The second (edx) is unused but must be in the list.
//
// Hook handles must be globals. Hooks are removed when the snippet is unloaded.
#include <windows.h>
#include <cstdint>
#include <safetyhook.hpp>
#include <Hooking.Patterns.h>

SafetyHookInline setResolution{};
SafetyHookInline getAspect{};
SafetyHookInline isDebugAllowed{};

// A partial change: modify the arguments, then call the original.
// Game: void __thiscall Renderer::SetResolution(int width, int height)
#ifdef _WIN64
void SetResolution(void *self, int width, int height)
#else
void __fastcall SetResolution(void *self, void *edx, int width, int height)
#endif
{
    if (width < 1280)
    {
        width = 1920;
        height = 1080;
    }
    setResolution.thiscall<void>(self, width, height);
}

// Changing the result: call the original, then return something else.
// Game: float __cdecl GetAspectRatio()
float GetAspectRatio()
{
    float original = getAspect.call<float>();
    return original < 1.5f ? 16.0f / 9.0f : original;
}

// Full replacement: the original never runs.
// Game: bool __stdcall IsDebugMenuAllowed(int player)
bool __stdcall IsDebugMenuAllowed(int player)
{
    return true;
}

void Init()
{
    // The target is any code address: from a pattern, GetProcAddress, an RVA, ...
    hook::pattern renderer("55 8B EC 83 EC 10 8B 45 08 3D 00 05 00 00");
    if (!renderer.empty())
        setResolution = safetyhook::create_inline(renderer.get_first(), SetResolution);

    hook::pattern aspect("D9 05 ? ? ? ? C3 CC CC");
    if (!aspect.empty())
        getAspect = safetyhook::create_inline(aspect.get_first(), GetAspectRatio);

    hook::pattern debug("83 3D ? ? ? ? 00 74 ? 32 C0 C2 04 00");
    if (!debug.empty())
        isDebugAllowed = safetyhook::create_inline(debug.get_first(), IsDebugMenuAllowed);
}

// Optional: hooks are removed automatically. To remove one early, assign {}:
//     setResolution = {};
// or switch it off and on: setResolution.disable(); setResolution.enable();
