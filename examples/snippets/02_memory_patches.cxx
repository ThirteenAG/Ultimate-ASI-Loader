// 02 - Memory patches: change values, bytes and instructions
//
// Use for: changing a constant (FOV, aspect ratio, resolution limit),
// flipping a jump, removing an instruction, writing a few bytes of code.
//
// Every write made with injector:: is undone when the snippet is unloaded.
// A plain pointer write (*p = x) is not: undo it yourself in Shutdown().
#include <windows.h>
#include <cstdint>
#include <injector/injector.hpp>

// Addresses from a disassembler (IDA, Ghidra, x64dbg) are usually shown for the
// default image base. Store them as an offset from the module start (RVA),
// because the game may be loaded somewhere else (ASLR):
//     RVA = address in IDA - image base in IDA (0x400000 for most 32-bit games,
//                                                0x140000000 for 64-bit ones)
uintptr_t Exe(uintptr_t rva)
{
    return (uintptr_t)GetModuleHandleA(nullptr) + rva;
}

// Same for a DLL of the game.
uintptr_t InModule(char const *dll, uintptr_t rva)
{
    HMODULE module = GetModuleHandleA(dll);
    return module ? (uintptr_t)module + rva : 0;
}

int previousLimit = 0;

void Init()
{
    // A value of any type. The last argument (true) makes the memory writable first.
    injector::WriteMemory<float>(Exe(0x1A2B30), 16.0f / 9.0f, true);
    injector::WriteMemory<int>(Exe(0x1A2B40), 1920, true);
    injector::WriteMemory<double>(Exe(0x1A2B48), 90.0, true);

    // One byte: turn a conditional jump into an unconditional one.
    //   74 xx (jz short) -> EB xx (jmp short)
    //   0F 84 xx xx xx xx (jz near) -> 90 E9 xx xx xx xx (nop; jmp near)
    injector::WriteMemory<uint8_t>(Exe(0x1000), 0xEB, true);
    injector::WriteMemory<uint16_t>(Exe(0x1100), 0xE990, true); // bytes 90 E9 (little endian)

    // Several bytes at once, e.g. make a function return true immediately.
    uint8_t returnTrue[] = {0xB0, 0x01, 0xC3}; // mov al, 1; ret
    injector::WriteMemoryRaw(Exe(0x2000), returnTrue, sizeof(returnTrue), true);

    // Remove an instruction: replace its bytes with NOPs (count = instruction length).
    injector::MakeNOP(Exe(0x3000), 6, true);

    // Read a value (e.g. to log it, or to compute the new one from it).
    previousLimit = injector::ReadMemory<int>(Exe(0x4000), true);
    injector::WriteMemory<int>(Exe(0x4000), previousLimit * 2, true);

    // In a DLL of the game.
    uintptr_t engine = InModule("Engine.dll", 0x5000);
    if (engine)
        injector::WriteMemory<float>(engine, 1.0f, true);
}
