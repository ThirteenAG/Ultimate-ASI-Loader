// 05 - Mid hooks: run code in the middle of a function and edit registers
//
// Use for: the value you need is in a register at some instruction (a
// width, a pointer to a game object, a float about to be stored), and
// there is no function boundary to hook.
//
// safetyhook::create_mid(address, yourFunction) runs yourFunction(regs) just
// before the instruction at `address`. Then the game continues with the
// registers you changed. The hook needs 5 bytes of complete instructions at
// the address. They are moved, so you don't lose them.
//
// regs fields:
//   x86: eax ebx ecx edx esi edi ebp esp eip eflags, xmm0..xmm7
//   x64: rax rbx rcx rdx rsi rdi rbp rsp r8..r15 rip rflags, xmm0..xmm15
//   xmm registers: .f32[0..3] (float), .f64[0..1] (double), .u32[], .u64[]
// The x86 stack: regs.esp points at what is on the stack at that instruction.
// Arguments of the current function (with a standard ebp frame) are at
// regs.ebp + 8, + 12, ...
//
// A captureless lambda works too: safetyhook::create_mid(address, [](SafetyHookContext& regs) { ... });
#include <windows.h>
#include <cstdint>
#include <safetyhook.hpp>
#include <Hooking.Patterns.h>

SafetyHookMid widthHook{};
SafetyHookMid fovHook{};
SafetyHookMid playerHook{};

void *player = nullptr; // a game object captured from a register

// Integer register: the game is about to use the width in ecx.
void OnWidth(SafetyHookContext &regs)
{
#ifdef _WIN64
    if (regs.rcx < 1280)
        regs.rcx = 1920;
#else
    if (regs.ecx < 1280)
        regs.ecx = 1920;
#endif
}

// Float in an SSE register: movss [rsi+0x40], xmm0 is about to store the FOV.
void OnFov(SafetyHookContext &regs)
{
    regs.xmm0.f32[0] *= 1.2f;
}

void Init()
{
    // 8B 4E 10 = mov ecx, [esi+10]: hook the next instruction, where ecx holds the width.
    hook::pattern width("8B 4E 10 3B CF 7D ? 8B CF");
    if (!width.empty())
        widthHook = safetyhook::create_mid(width.get_first(3), OnWidth);

    hook::pattern fov("F3 0F 11 46 40 F3 0F 10 46 44");
    if (!fov.empty())
        fovHook = safetyhook::create_mid(fov.get_first(), OnFov);

    // Read a pointer to a game object, and stack values on x86, with a lambda.
    hook::pattern update("8B 8E ? ? ? ? 85 C9 74 ? E8");
    if (!update.empty())
        playerHook = safetyhook::create_mid(update.get_first(), [](SafetyHookContext &regs) {
#ifdef _WIN64
            player = (void *)regs.rsi;
#else
            player = (void *)regs.esi;
            int firstArgument = *(int *)(regs.ebp + 8); // with a standard ebp frame
            int topOfStack = *(int *)regs.esp;
            (void)firstArgument;
            (void)topOfStack;
#endif
        });
}
