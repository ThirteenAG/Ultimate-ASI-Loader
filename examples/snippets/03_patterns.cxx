// 03 - Patterns: find code by its bytes instead of a fixed address
//
// Use for: patches that keep working across game versions, and code in
// DLLs (GetModuleHandle + module_pattern).
//
// A pattern is a list of hex bytes; ? matches any byte. Patterns search the
// executable sections (code) of a module.
//
// Always check that a pattern was found before using it: get_first() on a
// pattern without exactly one match does not return a usable address.
#include <windows.h>
#include <cstdint>
#include <injector/injector.hpp>
#include <Hooking.Patterns.h>

// Relative addresses in instructions point from the end of the instruction:
//   E8 rel32          call function       -> target = (address of rel32) + 4 + rel32
//   E9 rel32          jmp function        -> same
//   48 8B 05 disp32   mov rax, [rip+disp] -> data = (address of disp32) + 4 + disp32 (x64)
// On x86, data is referenced by absolute addresses instead:
//   A1 addr           mov eax, [addr]     -> data = *(uint32_t*)(address of addr)
uintptr_t FollowRelative(void *rel32)
{
    return (uintptr_t)rel32 + 4 + *(int32_t *)rel32;
}

int patched = 0;

void Init()
{
    // 1. The executable. get_first(offset) = address of the first matched byte + offset.
    //    Here: mov dword ptr [????], 1.0f  ->  write a new float to the 4 bytes of the immediate.
    hook::pattern aspect("C7 05 ? ? ? ? 00 00 80 3F");
    if (!aspect.empty())
        injector::WriteMemory<float>(aspect.get_first(6), 1.25f, true);

    // 2. A DLL of the game: same thing with its module handle.
    HMODULE engine = GetModuleHandleA("Engine.dll");
    if (engine)
    {
        hook::pattern check(engine, "74 ? 8B 4D 08 85 C9");
        if (check.size() == 1)
            injector::WriteMemory<uint8_t>(check.get_first(), 0xEB, true); // jz -> jmp
    }

    // 3. Every match: patch all places that load the same constant.
    hook::pattern("D9 05 ? ? ? ? D8 0D ? ? ? ? D9 5C 24").for_each_result([](hook::pattern_match match) {
        injector::MakeNOP(match.get<void>(0), 6, true);
        ++patched;
    });

    // 4. Exactly N matches, then each one by index.
    hook::pattern pair("8B 0D ? ? ? ? 6A 00 6A 01");
    if (pair.size() == 2)
    {
        injector::MakeNOP(pair.get(0).get<void>(6), 4, true);
        injector::MakeNOP(pair.get(1).get<void>(6), 4, true);
    }

    // 5. Find a function through a call to it: E8 rel32.
    hook::pattern call("E8 ? ? ? ? 83 C4 08 84 C0 74");
    if (!call.empty())
    {
        uintptr_t function = FollowRelative(call.get_first(1));
        injector::WriteMemory<uint8_t>(function, 0xC3, true); // ret: the function does nothing now
    }

    // 6. Find a global variable through an instruction that uses it.
#ifdef _WIN64
    hook::pattern load("48 8B 05 ? ? ? ? 48 85 C0 74 ? 8B 48 10");
    if (!load.empty())
    {
        void **global = (void **)FollowRelative(load.get_first(3)); // rip-relative
        (void)global;
    }
#else
    hook::pattern load("A1 ? ? ? ? 85 C0 74 ? 8B 48 10");
    if (!load.empty())
    {
        void **global = *(void ***)load.get_first(1); // absolute address in the instruction
        (void)global;
    }
#endif

    // 7. Short form when you are sure the pattern exists exactly once:
    //    void* address = hook::get_pattern("74 10 53 53 6A 1B");                    // executable
    //    void* address = hook::module_pattern(GetModuleHandleA("X.dll"), "...").get_first(2);  // DLL, with offset
}
