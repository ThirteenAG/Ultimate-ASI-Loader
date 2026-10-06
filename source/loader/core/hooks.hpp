// MinHook hooks installed in batches. One MH_ApplyQueued per batch means one thread
// suspension instead of one per hook, which matters in processes with many threads.
#pragma once
#include <windows.h>
#include <initializer_list>
#include <span>

namespace ual
{
    struct HookSpec
    {
        const wchar_t* module;
        const char* function;
        void* detour;
        void** original;         // receives the trampoline, unless already set
    };

    // Hooks only entries whose *original is null. Returns how many are active afterwards.
    int InstallHooks(std::span<const HookSpec> hooks);
    inline int InstallHooks(std::initializer_list<HookSpec> hooks) { return InstallHooks(std::span<const HookSpec>(hooks.begin(), hooks.size())); }

    // Trampolines stay valid, so calls already inside a detour still reach the original.
    void DisableHooks(std::span<const HookSpec> hooks);
}
