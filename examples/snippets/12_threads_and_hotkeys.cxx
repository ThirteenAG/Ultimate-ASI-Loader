// 12 - Threads and hotkeys
//
// Use for: work that has to happen later or repeatedly, e.g. waiting until
// the game has created an object, or toggling a feature with a key.
//
// Rules:
//   - Init() runs before the game starts: never wait in it for the game to
//     do something (it would wait forever). Start a thread instead.
//   - Stop your threads in Shutdown(), and wait until they have stopped:
//     after Shutdown the snippet's code may be replaced (hot reload).
//
// Often better than a thread: hook a function the game calls every frame
// (05_mid_hooks.cxx, 08_virtual_methods.cxx) and do the work there.
#include <windows.h>
#include <cstdint>
#include <injector/injector.hpp>

#ifdef __CXXSNIPPETS__ // already in the real <windows.h>
extern "C" {
short WINAPI GetAsyncKeyState(int key);
}
#define VK_F5 0x74
#endif

HANDLE worker = nullptr;
volatile bool stopping = false;
volatile bool featureOn = true;

float *fov = nullptr;

// Wait for a game object, then poll a hotkey.
DWORD WINAPI Worker(void *parameter)
{
    // 1. Wait until the game has created what you need (here: a pointer at a fixed RVA).
    uintptr_t exe = (uintptr_t)GetModuleHandleA(nullptr);
    while (!stopping && !fov)
    {
        float **global = (float **)(exe + 0x123456);
        fov = *global;
        Sleep(100);
    }

    // 2. Toggle with F5 (the low bit: "pressed since the last call").
    while (!stopping)
    {
        if (GetAsyncKeyState(VK_F5) & 1)
            featureOn = !featureOn;
        if (fov && featureOn)
            *fov = 90.0f;
        Sleep(16);
    }
    return 0;
}

void Init()
{
    stopping = false;
    worker = CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr);
}

void Shutdown()
{
    stopping = true;
    if (worker)
    {
        WaitForSingleObject(worker, INFINITE);
        CloseHandle(worker);
        worker = nullptr;
    }
}
