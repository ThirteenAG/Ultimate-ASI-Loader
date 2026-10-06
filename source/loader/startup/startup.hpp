// Plugins load when game code starts running, not from DllMain. Common kernel32 imports of the exe
// and nearby game DLLs go to wrappers that load everything on the first call from the game, then get restored.
#pragma once
#include <windows.h>

namespace ual::startup
{
    // False if nothing usable was patched or another loader already did it. The caller then loads the original DLL right away.
    bool HookEntryPoints();

    // True if the exe starts in a protection stub (Origin / EA, ...) that resolves the game's imports itself
    // and loaded us that way. Its import directory sits in the entry point section after the game's code and doesn't
    // list us. The imports HookEntryPoints would patch are the stub's, already used up.
    bool LoadedByProtectionStub();

    // For stub-protected exes: inline-hooks the kernel32 functions until game code (not the stub) calls one.
    // Loads right away if nothing could be hooked. False if another loader copy already hooks.
    bool HookEntryPointsInline();

    // On DLL unload.
    void RestoreImports();

    // Loads the original DLL, sets up file overloading and loads plugins. Runs once.
    void LoadEverything();
}
