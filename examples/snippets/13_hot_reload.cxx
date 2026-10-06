// 13 - Hot reload: editing a snippet while the game runs
//
// Use for: tuning values and testing patches without restarting the game.
//
// With CxxHotReload=1 in global.ini ([GlobalSets]), saving a .cxx file:
//   1. compiles the new version. If it has errors, they are shown and the
//      old version keeps running;
//   2. stops the old version: Shutdown(), then its hooks are removed and its
//      injector:: writes are restored (newest first);
//   3. starts the new one: globals, then Init().
// Editing a header (.h/.hpp) next to the snippets reloads the snippets of that folder.
//
// Undone automatically:            Your job in Shutdown():
//   SafetyHookInline/Mid/Vmt/Vm     plain pointer writes (*p = x) and memcpy
//   injector::WriteMemory(Raw)      VirtualProtect + direct writes
//   injector::MakeNOP/JMP/CALL      threads you created (12_threads_and_hotkeys.cxx)
//                                   windows, timers, handles, game state you changed
#include <windows.h>
#include <cstdint>
#include <injector/injector.hpp>

float *gameFov = nullptr;
float savedFov = 0.0f;

// Edit, save, and look at the game.
const float kFov = 75.0f;

void Init()
{
    gameFov = (float *)((uintptr_t)GetModuleHandleA(nullptr) + 0x200000);

    // Undone automatically on reload:
    injector::WriteMemory<float>(gameFov, kFov, true);

    // A direct write: remember the old value to restore it in Shutdown.
    // (Here only to show the difference; prefer injector::WriteMemory.)
    savedFov = gameFov[1];
    gameFov[1] = kFov;
}

void Shutdown()
{
    gameFov[1] = savedFov;
}
