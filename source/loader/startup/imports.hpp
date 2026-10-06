// Import patching that starts loading at the game's entry point. Internal to startup/, exposed for unit tests.
#pragma once
#include "../core/pe.hpp"
#include <string>
#include <string_view>
#include <unordered_map>

namespace ual::startup
{
    struct PatchedSlot
    {
        void** slot;
        void* original;
    };
    using PatchedSlots = std::unordered_map<std::string, PatchedSlot>; // function -> slot

    struct ModuleImports
    {
        HMODULE module = nullptr;
        PatchedSlots kernel32, ole32, vccorlib;
    };

    // Wrapper that starts loading for this kernel32 function, or null.
    void* Kernel32Wrapper(const char* name);

    // Handles API set names, bound imports and thunkless IATs of packed exes.
    void PatchKernel32(const pe::Image& img, ModuleImports& rec);
    void PatchCom(const pe::Image& img, ModuleImports& rec);
    void PatchVccorlib(const pe::Image& img, ModuleImports& rec);
    // Ordinal imports of the DLL the loader stands in for.
    void PatchOrdinals(const pe::Image& img, HMODULE self, std::wstring_view selfName);
    void RestoreSlots(PatchedSlots& slots);
}
