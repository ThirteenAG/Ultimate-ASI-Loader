// 10 - Patching a DLL that the game loads later
//
// Use for: DLLs that are not loaded yet when snippets start (renderer or
// game DLLs loaded with LoadLibrary, plugins, middleware).
// GetModuleHandle returns null for them in Init().
//
// Hook LoadLibraryExW (every LoadLibrary variant ends up there), let it load
// the DLL, then patch the DLL once it is in memory.
#include <windows.h>
#include <cstdint>
#include <injector/injector.hpp>
#include <safetyhook.hpp>
#include <Hooking.Patterns.h>

SafetyHookInline loadLibraryHook{};
bool rendererPatched = false;

// Runs once, as soon as the DLL is loaded.
void PatchRenderer(HMODULE renderer)
{
    hook::pattern aspect(renderer, "C7 44 24 ? 39 8E E3 3F");
    if (!aspect.empty())
        injector::WriteMemory<float>(aspect.get_first(4), 2.3703704f, true); // 21:9 instead of 16:9
}

// Patches whatever is loaded now; called from Init and after each LoadLibrary.
void PatchLoadedModules()
{
    if (!rendererPatched)
    {
        HMODULE renderer = GetModuleHandleA("Renderer.dll");
        if (renderer)
        {
            rendererPatched = true;
            PatchRenderer(renderer);
        }
    }
}

HMODULE WINAPI LoadLibraryExWHook(LPCWSTR name, HANDLE file, DWORD flags)
{
    HMODULE module = loadLibraryHook.stdcall<HMODULE>(name, file, flags);
    if (module)
        PatchLoadedModules();
    return module;
}

void Init()
{
    PatchLoadedModules(); // already loaded?
    if (rendererPatched)
        return;

    // kernelbase.dll has the implementation; kernel32's export forwards to it.
    HMODULE kernelbase = GetModuleHandleA("kernelbase.dll");
    void *target = kernelbase ? (void *)GetProcAddress(kernelbase, "LoadLibraryExW") : nullptr;
    if (!target)
        target = (void *)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryExW");
    loadLibraryHook = safetyhook::create_inline(target, LoadLibraryExWHook);
}
