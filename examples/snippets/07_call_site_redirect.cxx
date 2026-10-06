// 07 - Redirecting one call or jump (MakeCALL / MakeJMP)
//
// Use for: changing what happens at ONE place in the game, while other
// callers of the same function keep the original behaviour. For example the
// HUD calls GetScreenWidth() and you want only the HUD to get a 4:3 width.
// (An inline hook, 04_inline_hooks.cxx, changes the function for everyone.)
//
// MakeCALL(address, yourFunction) rewrites the 5-byte instruction at
// `address` into "call yourFunction". Read where it called before first
// (E8 rel32: target = address + 5 + rel32) to call the original yourself.
// MakeJMP does the same with "jmp".
//
// Your function must match the called function exactly (calling convention,
// parameters, return type), like with inline hooks.
//
// x64: a rel32 call reaches only +-2 GB, and the snippet's code is usually
// farther from the game. MakeCALL/MakeJMP then go through a small jump stub
// placed next to the game code automatically, so the same line works on both
// architectures. MakeCALLTrampoline/MakeJMPTrampoline (the names the
// widescreen fixes use) are accepted too and do the same.
//
// Only use it on a 5-byte "E8 rel32" call / "E9 rel32" jmp, or bytes you
// don't need (pad the rest with MakeNOP). Indirect calls (FF 15 / FF 50 xx)
// are not 5 bytes: use a mid hook (05_mid_hooks.cxx) for those.
#include <windows.h>
#include <cstdint>
#include <injector/injector.hpp>
#include <Hooking.Patterns.h>

// The target of a call/jmp rel32 at `instruction`.
uintptr_t CallTarget(void *instruction)
{
    return (uintptr_t)instruction + 5 + *(int32_t *)((uintptr_t)instruction + 1);
}

typedef int (*GetScreenWidth_t)();
GetScreenWidth_t originalGetScreenWidth = nullptr;
int screenHeight = 1080;

// Only the HUD call site comes here.
int HudGetScreenWidth()
{
    int width = originalGetScreenWidth();
    int hudWidth = screenHeight * 4 / 3;
    return hudWidth < width ? hudWidth : width;
}

// A jmp to your own code: replace the end of a function (or a whole function).
// Game: void __cdecl LimitFramerate(int fps)
void NoFramerateLimit(int fps)
{
}

void Init()
{
    // call GetScreenWidth ; mov [esp+10], eax  <- inside the HUD drawing code
    hook::pattern hud("E8 ? ? ? ? 89 44 24 10 DB 44 24 10");
    if (!hud.empty())
    {
        originalGetScreenWidth = (GetScreenWidth_t)CallTarget(hud.get_first());
        injector::MakeCALL(hud.get_first(), HudGetScreenWidth, true);
    }

    hook::pattern limiter("E9 ? ? ? ? CC CC CC 55 8B EC 83 EC 08 A1");
    if (!limiter.empty())
        injector::MakeJMP(limiter.get_first(), NoFramerateLimit, true);

    // Remove a call completely (e.g. a call to a function that shows an intro video).
    // 5 bytes = the size of "E8 rel32".
    hook::pattern intro("E8 ? ? ? ? 84 C0 75 ? 6A 01 E8");
    if (!intro.empty())
        injector::MakeNOP(intro.get_first(), 5, true);
}
