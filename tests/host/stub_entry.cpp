// ual_host_stub.exe imitates an Origin/EA protection stub. The entry point and import directory
// share a section after the code, and the loader is loaded at run time via "load:<dll>".
// The entry is a const jump to the CRT entry so it lands in .rdata, which the project makes executable.
#include <cstdint>

extern "C" int wmainCRTStartup();

#pragma pack(push, 1)
struct StubCode
{
#ifdef _WIN64
    uint8_t code[6]; // jmp qword ptr [rip+0]
#else
    uint8_t code[9]; // call $+5; pop eax; jmp dword ptr [eax+4]
#endif
    int (*target)();
};
#pragma pack(pop)

#ifdef _WIN64
extern "C" const StubCode StubEntry = { { 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 }, wmainCRTStartup };
#else
extern "C" const StubCode StubEntry = { { 0xE8, 0x00, 0x00, 0x00, 0x00, 0x58, 0xFF, 0x60, 0x04 }, wmainCRTStartup };
#endif

// Calls fn(0) from the stub section, like a stub before it unpacks the game. No relocations needed.
#ifdef _WIN64
// sub rsp,28h; mov rax,rcx; xor ecx,ecx; call rax; add rsp,28h; ret
extern "C" const unsigned char StubCallWithZero[] = { 0x48, 0x83, 0xEC, 0x28, 0x48, 0x89, 0xC8, 0x33, 0xC9, 0xFF, 0xD0, 0x48, 0x83, 0xC4, 0x28, 0xC3 };
#else
// mov eax,[esp+4]; push 0; call eax (stdcall: the callee pops the argument); ret
extern "C" const unsigned char StubCallWithZero[] = { 0x8B, 0x44, 0x24, 0x04, 0x6A, 0x00, 0xFF, 0xD0, 0xC3 };
#endif

#include <windows.h>

namespace host
{
    extern void (*g_stubCall)();
}

namespace
{
    // Goes through the exe's Sleep import at call time: the loader's wrapper if imports are patched, else kernel32 (hooked inline)
    void CallSleepFromStub()
    {
        reinterpret_cast<void(__cdecl*)(void*)>((void*)StubCallWithZero)(reinterpret_cast<void*>(&Sleep));
    }

    const int g_registered = (host::g_stubCall = CallSleepFromStub, 0);
}
