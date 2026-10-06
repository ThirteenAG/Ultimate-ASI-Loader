// 08 - Virtual methods: hooking through an object's vtable
//
// Use for: classes whose methods are called through a vtable. Direct3D,
// DirectInput and XAudio devices and other COM objects, and C++ game classes
// with virtual functions.
//
// An object starts with a pointer to its vtable, an array of function
// pointers. Method N is vtable[N]. Indexes come from the SDK headers (the
// order of methods in the interface, counting the base interfaces first)
// or from a disassembler.
//
// Two ways:
//   a) create_vmt(object) + create_vm(vmt, index, yourFunction): hooks the
//      method of THAT object only (it gets a copy of the vtable).
//   b) create_inline(vtable[index], yourFunction): hooks the method's code,
//      so every object of the class is affected.
//
// Calling conventions on x86:
//   COM / DirectX methods are __stdcall, and `this` is the first parameter:
//       HRESULT __stdcall Present(void* device, ...)  -> hook.stdcall<HRESULT>(device, ...)
//   C++ game classes are __thiscall: use the __fastcall form from 04_inline_hooks.cxx.
// On x64 `this` is simply the first parameter.
#include <windows.h>
#include <cstdint>
#include <safetyhook.hpp>
#include <Hooking.Patterns.h>

typedef long HRESULT;

// IDirect3DDevice9: IUnknown (3) + ... ; EndScene is 42, Reset 16, Present 17.
const int kEndScene = 42;
const int kReset = 16;

SafetyHookVmt deviceVmt{};
SafetyHookVm endScene{};
SafetyHookInline reset{};

void **device = nullptr; // IDirect3DDevice9*

int frames = 0;

// a) One object: called every frame for this device.
HRESULT __stdcall EndSceneHook(void *self)
{
    ++frames;
    return endScene.stdcall<HRESULT>(self);
}

// b) All objects of the class: hook the code the vtable points to.
HRESULT __stdcall ResetHook(void *self, void *presentParameters)
{
    return reset.stdcall<HRESULT>(self, presentParameters);
}

// A virtual method of a C++ game class: void __thiscall Camera::Update(float deltaTime) at index 5.
SafetyHookInline cameraUpdate{};
#ifdef _WIN64
void CameraUpdate(void *self, float deltaTime)
#else
void __fastcall CameraUpdate(void *self, void *edx, float deltaTime)
#endif
{
    cameraUpdate.thiscall<void>(self, deltaTime);
}

void Init()
{
    // The game keeps its device in a global: mov ecx, [g_device] ; mov eax, [ecx]
    hook::pattern global("8B 0D ? ? ? ? 8B 01 FF 90 A8 00 00 00");
    if (global.empty())
        return;
    device = **(void ****)global.get_first(2);
    if (!device) // not created yet: hook the code that creates it instead
        return;

    deviceVmt = safetyhook::create_vmt(device);
    endScene = safetyhook::create_vm(deviceVmt, kEndScene, EndSceneHook);

    void **vtable = *(void ***)device;
    reset = safetyhook::create_inline(vtable[kReset], ResetHook);

    // A game class, from an object pointer you got elsewhere (a mid hook, a global, ...).
    void *camera = nullptr;
    if (camera)
        cameraUpdate = safetyhook::create_inline((*(void ***)camera)[5], CameraUpdate);
}

void Shutdown()
{
    // Release the method hook before the vtable hook it belongs to.
    endScene = {};
    deviceVmt = {};
}
