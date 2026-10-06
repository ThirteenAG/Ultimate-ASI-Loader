#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "builtins.hpp"
#include <windows.h>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <safetyhook.hpp>
#include <injector/injector.hpp>
#include <Hooking.Patterns.h>

namespace cxxsnippets
{
void AddBindingHeaders(std::map<std::string, std::string> &h)
{
    h["injector/injector.hpp"] = R"(#pragma once
#include <cstdint>
#include <cstddef>
namespace injector {
template<typename T> [[cxxsnippets::intrinsic("write_memory")]] void WriteMemory([[cxxsnippets::address]] void* address,T value,bool vp=true);
template<typename T> [[cxxsnippets::intrinsic("read_memory")]] T ReadMemory([[cxxsnippets::address]] void* address,bool vp=true);
[[cxxsnippets::intrinsic("owned:cxxsnippets_write_raw")]] void WriteMemoryRaw([[cxxsnippets::address]] void* address,void const* source,size_t size,bool vp);
[[cxxsnippets::link("cxxsnippets_read_raw")]] void ReadMemoryRaw([[cxxsnippets::address]] void* address,void* destination,size_t size,bool vp);
[[cxxsnippets::intrinsic("owned:cxxsnippets_nop")]] void MakeNOP([[cxxsnippets::address]] void* address,size_t count=1,bool vp=true);
[[cxxsnippets::intrinsic("owned:cxxsnippets_jmp")]] void* MakeJMP([[cxxsnippets::address]] void* address,[[cxxsnippets::address]] void* destination,bool vp=true);
[[cxxsnippets::intrinsic("owned:cxxsnippets_call")]] void* MakeCALL([[cxxsnippets::address]] void* address,[[cxxsnippets::address]] void* destination,bool vp=true);
// MakeJMP/MakeCALL already use a jump stub on x64 when the destination is out of rel32 range;
// these names keep code written for the native helpers compiling.
[[cxxsnippets::intrinsic("owned:cxxsnippets_jmp")]] void* MakeJMPTrampoline([[cxxsnippets::address]] void* address,[[cxxsnippets::address]] void* destination,bool vp=true);
[[cxxsnippets::intrinsic("owned:cxxsnippets_call")]] void* MakeCALLTrampoline([[cxxsnippets::address]] void* address,[[cxxsnippets::address]] void* destination,bool vp=true);
}
)";
    h["Hooking.Patterns.h"] = R"(#pragma once
#include <cstdint>
#include <cstddef>
namespace hook {
[[cxxsnippets::value]] struct pattern_match {
    template<typename T> [[cxxsnippets::link("cxxsnippets_match_get")]] T* get(ptrdiff_t offset=0);
};
[[cxxsnippets::value]] struct pattern {
    [[cxxsnippets::intrinsic("owned:cxxsnippets_pattern_new")]] pattern(char const* text);
    [[cxxsnippets::intrinsic("owned:cxxsnippets_pattern_module")]] pattern(void* module,char const* text);
    [[cxxsnippets::intrinsic("owned:cxxsnippets_pattern_range")]] pattern(uintptr_t first,uintptr_t last,char const* text);
    [[cxxsnippets::link("cxxsnippets_pattern_count")]] pattern count(uint32_t expected);
    [[cxxsnippets::link("cxxsnippets_pattern_clear")]] pattern clear(void* module=nullptr);
    [[cxxsnippets::link("cxxsnippets_pattern_size")]] size_t size();
    [[cxxsnippets::link("cxxsnippets_pattern_empty")]] bool empty();
    [[cxxsnippets::link("cxxsnippets_pattern_get")]] pattern_match get(size_t index);
    [[cxxsnippets::link("cxxsnippets_pattern_one")]] pattern_match get_one();
    template<typename T=void> [[cxxsnippets::link("cxxsnippets_pattern_first")]] T* get_first(ptrdiff_t offset=0);
    template<typename Pred> [[cxxsnippets::intrinsic("pattern_each")]] Pred for_each_result(Pred callback);
};
template<typename T=void> [[cxxsnippets::link("cxxsnippets_get_pattern")]] T* get_pattern(char const* text,ptrdiff_t offset=0);
template<typename T=void> [[cxxsnippets::link("cxxsnippets_module_pattern")]] T* module_pattern(void* module,char const* text,ptrdiff_t offset=0);
template<typename T=void> [[cxxsnippets::link("cxxsnippets_range_pattern")]] T* range_pattern(uintptr_t first,uintptr_t last,char const* text,ptrdiff_t offset=0);
}
)";
    std::string s = R"(#pragma once
#include <cstdint>
#include <cstddef>
#define SAFETYHOOK_NOINLINE
#define SAFETYHOOK_CCALL __cdecl
#define SAFETYHOOK_STDCALL __stdcall
#define SAFETYHOOK_FASTCALL __fastcall
#define SAFETYHOOK_THISCALL __thiscall
#define SAFETYHOOK_OS_WINDOWS 1
#ifdef _WIN64
#define SAFETYHOOK_ARCH_X86_64 1
#define SAFETYHOOK_ARCH_X86_32 0
#else
#define SAFETYHOOK_ARCH_X86_64 0
#define SAFETYHOOK_ARCH_X86_32 1
#endif
namespace safetyhook {
union Xmm { uint8_t u8[16]; uint16_t u16[8]; uint32_t u32[4]; uint64_t u64[2]; float f32[4]; double f64[2]; };
struct Context32 { Xmm xmm0,xmm1,xmm2,xmm3,xmm4,xmm5,xmm6,xmm7; uintptr_t eflags,edi,esi,edx,ecx,ebx,eax,ebp,esp,trampoline_esp,eip; };
struct Context64 { Xmm xmm0,xmm1,xmm2,xmm3,xmm4,xmm5,xmm6,xmm7,xmm8,xmm9,xmm10,xmm11,xmm12,xmm13,xmm14,xmm15; uintptr_t rflags,r15,r14,r13,r12,r11,r10,r9,r8,rdi,rsi,rdx,rcx,rbx,rax,rbp,rsp,trampoline_rsp,rip; };
#ifdef _WIN64
using Context=Context64;
#else
using Context=Context32;
#endif
}
using SafetyHookContext=safetyhook::Context;
)";
    for (auto entry : {std::pair{"SafetyHookInline", "inline"}, std::pair{"SafetyHookMid", "mid"},
                       std::pair{"SafetyHookVmt", "vmt"}, std::pair{"SafetyHookVm", "vm"}})
    {
        std::string name = entry.first, kind = entry.second;
        s += "[[cxxsnippets::handle(\"cxxsnippets_" + kind + "_release\")]] struct " + name + " {\n";
        if (kind == "inline" || kind == "mid")
            s += "enum Flags { Default=0,StartDisabled=1 };\n";
        s += "[[cxxsnippets::link(\"cxxsnippets_" + kind + "_reset\")]] void reset();\n";
        s += "[[cxxsnippets::link(\"cxxsnippets_" + kind + "_bool\")]] explicit operator bool() const;\n";
        if (kind == "inline" || kind == "mid")
        {
            for (auto m : {"enable", "disable", "enabled"})
                s += "[[cxxsnippets::link(\"cxxsnippets_" + kind + "_" + m + "\")]] bool " + m + "();\n";
            s += "[[cxxsnippets::link(\"cxxsnippets_" + kind + "_target\")]] uintptr_t target_address();\n";
        }
        if (kind == "inline" || kind == "vm")
        {
            s += "template<typename T> [[cxxsnippets::intrinsic(\"original:cxxsnippets_" + kind + "_original\")]] T original();\n";
            for (auto m : {"call", "ccall", "stdcall", "thiscall", "fastcall", "unsafe_call", "unsafe_ccall",
                           "unsafe_stdcall", "unsafe_thiscall", "unsafe_fastcall"})
                s += std::string("template<typename RetT=void,typename... Args> [[cxxsnippets::intrinsic(\"invoke:") +
                     (kind == "vm" ? "vm_" : "") + m + "\")]] RetT " + m + "(Args... args);\n";
        }
        s += "};\n";
    }
    s += R"(namespace safetyhook {
using InlineHook=::SafetyHookInline; using MidHook=::SafetyHookMid; using VmtHook=::SafetyHookVmt; using VmHook=::SafetyHookVm;
[[cxxsnippets::link("cxxsnippets_inline_new")]] SafetyHookInline create_inline([[cxxsnippets::address]] void* target,[[cxxsnippets::address]] void* destination,int flags=0);
[[cxxsnippets::link("cxxsnippets_mid_new")]] SafetyHookMid create_mid([[cxxsnippets::address]] void* target,void (*destination)(SafetyHookContext&),int flags=0);
[[cxxsnippets::link("cxxsnippets_vmt_new")]] SafetyHookVmt create_vmt([[cxxsnippets::address]] void* object);
[[cxxsnippets::link("cxxsnippets_vm_new")]] SafetyHookVm create_vm(SafetyHookVmt& vmt,size_t index,[[cxxsnippets::address]] void* destination);
}
)";
    h["safetyhook.hpp"] = std::move(s);
}
namespace
{
struct Inline
{
    SafetyHookInline hook;
    std::recursive_mutex mutex;
};
void *InlineNew(void *target, void *dest, int flags)
{
    auto p = std::make_unique<Inline>();
    p->hook = safetyhook::create_inline(target, dest, (SafetyHookInline::Flags)flags);
    return p->hook ? p.release() : nullptr;
}
void InlineRelease(void *handle)
{
    auto p = static_cast<Inline *>(handle);
    if (!p)
        return;
    // a thread between inline_enter and inline_leave (calling the original) still owns the mutex: wait for it
    p->mutex.lock();
    p->mutex.unlock();
    delete p;
}
void InlineReset(Inline *p)
{
    if (p)
    {
        std::lock_guard<std::recursive_mutex> lock(p->mutex);
        p->hook.reset();
    }
}
bool InlineBool(Inline *p)
{
    return p && bool(p->hook);
}
bool InlineEnable(Inline *p)
{
    return p && bool(p->hook.enable());
}
bool InlineDisable(Inline *p)
{
    return p && bool(p->hook.disable());
}
bool InlineEnabled(Inline *p)
{
    return p && p->hook.enabled();
}
uintptr_t InlineTarget(Inline *p)
{
    return p ? p->hook.target_address() : 0;
}
void *InlineOriginal(Inline *p)
{
    return p ? p->hook.original<void *>() : nullptr;
}
void *InlineEnter(Inline *p)
{
    if (!p)
        return nullptr;
    p->mutex.lock();
    return InlineOriginal(p);
}
void InlineLeave(Inline *p)
{
    if (p)
        p->mutex.unlock();
}
SafetyHookMid *MidNew(void *target, void (*dest)(SafetyHookContext &), int flags)
{
    auto p = std::make_unique<SafetyHookMid>(safetyhook::create_mid(target, dest, (SafetyHookMid::Flags)flags));
    return *p ? p.release() : nullptr;
}
void MidRelease(void *p)
{
    delete static_cast<SafetyHookMid *>(p);
}
void MidReset(SafetyHookMid *p)
{
    if (p)
        p->reset();
}
bool MidBool(SafetyHookMid *p)
{
    return p && bool(*p);
}
bool MidEnable(SafetyHookMid *p)
{
    return p && bool(p->enable());
}
bool MidDisable(SafetyHookMid *p)
{
    return p && bool(p->disable());
}
bool MidEnabled(SafetyHookMid *p)
{
    return p && p->enabled();
}
uintptr_t MidTarget(SafetyHookMid *p)
{
    return p ? p->target_address() : 0;
}
SafetyHookVmt *VmtNew(void *object)
{
    auto result = SafetyHookVmt::create(object);
    if (!result)
        return nullptr;
    return new SafetyHookVmt(std::move(*result));
}
void VmtRelease(void *p)
{
    delete static_cast<SafetyHookVmt *>(p);
}
void VmtReset(SafetyHookVmt *p)
{
    if (p)
        p->reset();
}
bool VmtBool(SafetyHookVmt *p)
{
    return p != nullptr;
}
SafetyHookVm *VmNew(SafetyHookVmt **vmt, size_t index, void *dest)
{
    if (!vmt || !*vmt)
        return nullptr;
    auto result = (*vmt)->hook_method(index, dest);
    if (!result)
        return nullptr;
    return new SafetyHookVm(std::move(*result));
}
void VmRelease(void *p)
{
    delete static_cast<SafetyHookVm *>(p);
}
void VmReset(SafetyHookVm *p)
{
    if (p)
        p->reset();
}
bool VmBool(SafetyHookVm *p)
{
    return p && p->original<void *>();
}
void *VmOriginal(SafetyHookVm *p)
{
    return p ? p->original<void *>() : nullptr;
}
void *Assign(void **slot, void *value, void (*release)(void *))
{
    if (*slot != value)
    {
        if (*slot)
            release(*slot);
        *slot = value;
    }
    return value;
}
// Builtins run inside snippet code, often on a game thread inside a hook, where a C++ exception has no
// handler and would terminate the game: problems are reported to the debugger and the call does nothing.
static void Complain(const char *what)
{
    OutputDebugStringA("cxxsnippets: ");
    OutputDebugStringA(what);
    OutputDebugStringA("\n");
}
// static locals held by this thread, so a crash inside an initializer can release them (ReleaseStaticLocks)
static thread_local int t_staticDepth = 0;
bool StaticEnter(BuiltinOwner *owner, size_t key)
{
    owner->initializationMutex.lock();
    if (owner->initialized[key])
    {
        owner->initializationMutex.unlock();
        return false;
    }
    ++t_staticDepth;
    return true;
}
void StaticLeave(BuiltinOwner *owner, size_t key)
{
    owner->initialized[key] = true;
    --t_staticDepth;
    owner->initializationMutex.unlock();
}
} // namespace
void BuiltinOwner::ReleaseStaticLocks()
{
    while (t_staticDepth > 0)
    {
        --t_staticDepth;
        initializationMutex.unlock();
    }
}
namespace
{
void Write(BuiltinOwner *owner, uintptr_t address, uint64_t value, size_t size, bool vp)
{
    if (size > 8)
        return Complain("WriteMemory supports scalar values up to eight bytes");
    owner->Remember(address, size);
    injector::WriteMemoryRaw(address, &value, size, vp);
    FlushInstructionCache(GetCurrentProcess(), (void *)address, size);
}
uint64_t Read(uintptr_t address, size_t size, bool vp)
{
    uint64_t value = 0;
    if (size > 8)
    {
        Complain("ReadMemory supports scalar values up to eight bytes");
        return 0;
    }
    injector::ReadMemoryRaw(address, &value, size, vp);
    return value;
}
void WriteRaw(BuiltinOwner *owner, uintptr_t address, void *value, size_t size, bool vp)
{
    owner->Remember(address, size);
    injector::WriteMemoryRaw(address, value, size, vp);
    FlushInstructionCache(GetCurrentProcess(), (void *)address, size);
}
void ReadRaw(uintptr_t address, void *value, size_t size, bool vp)
{
    injector::ReadMemoryRaw(address, value, size, vp);
}
void Nop(BuiltinOwner *owner, uintptr_t address, size_t size, bool vp)
{
    owner->Remember(address, size);
    injector::MakeNOP(address, size, vp);
    FlushInstructionCache(GetCurrentProcess(), (void *)address, size);
}
// rel32 reaches +-2 GB and x64 snippet code can be anywhere, so farther destinations go through a
// stub near the patched instruction, jmp qword ptr [rip+0] + dq destination (like MakeCALLTrampoline).
uintptr_t Reachable(BuiltinOwner *owner, uintptr_t address, uintptr_t dest)
{
#ifdef _WIN64
    int64_t distance = (int64_t)dest - (int64_t)(address + 5);
    if (distance == (int32_t)distance)
        return dest;
    auto allocation = safetyhook::Allocator::global()->allocate_near({(uint8_t *)address}, 14);
    if (!allocation)
    {
        Complain("no free memory within 2 GB of the patched address for a jump stub");
        return 0;
    }
    uint8_t stub[14] = {0xFF, 0x25, 0, 0, 0, 0};
    std::memcpy(stub + 6, &dest, 8);
    std::memcpy(allocation->data(), stub, sizeof(stub));
    FlushInstructionCache(GetCurrentProcess(), allocation->data(), sizeof(stub));
    auto kept = new safetyhook::Allocation(std::move(*allocation));
    owner->Retain(kept, [](void *p) { delete static_cast<safetyhook::Allocation *>(p); });
    return kept->address();
#else
    (void)owner;
    (void)address;
    return dest;
#endif
}
void *Jmp(BuiltinOwner *owner, uintptr_t address, uintptr_t dest, bool vp)
{
    dest = Reachable(owner, address, dest);
    if (!dest)
        return nullptr;
    owner->Remember(address, 5); // E9 rel32
    void *previous = injector::MakeJMP(address, dest, vp).get_raw<void>();
    FlushInstructionCache(GetCurrentProcess(), (void *)address, 5);
    return previous;
}
void *Call(BuiltinOwner *owner, uintptr_t address, uintptr_t dest, bool vp)
{
    dest = Reachable(owner, address, dest);
    if (!dest)
        return nullptr;
    owner->Remember(address, 5); // E8 rel32
    void *previous = injector::MakeCALL(address, dest, vp).get_raw<void>();
    FlushInstructionCache(GetCurrentProcess(), (void *)address, 5);
    return previous;
}
void PatternDelete(void *p)
{
    delete static_cast<hook::pattern *>(p);
}
hook::pattern *PatternNew(BuiltinOwner *owner, char const *text)
{
    auto p = std::make_unique<hook::pattern>(text);
    owner->Own(p.get(), PatternDelete);
    return p.release();
}
hook::pattern *PatternModule(BuiltinOwner *owner, void *module, char const *text)
{
    auto p = std::make_unique<hook::pattern>(module, text);
    owner->Own(p.get(), PatternDelete);
    return p.release();
}
hook::pattern *PatternRange(BuiltinOwner *owner, uintptr_t first, uintptr_t last, char const *text)
{
    auto p = std::make_unique<hook::pattern>(first, last, text);
    owner->Own(p.get(), PatternDelete);
    return p.release();
}
hook::pattern *PatternCount(hook::pattern *p, uint32_t expected)
{
    p->count(expected);
    return p;
}
hook::pattern *PatternClear(hook::pattern *p, void *module)
{
    p->clear(module);
    return p;
}
size_t PatternSize(hook::pattern *p)
{
    return p->size();
}
bool PatternEmpty(hook::pattern *p)
{
    return p->empty();
}
void *PatternGet(hook::pattern *p, size_t index)
{
    if (index >= p->size())
    {
        Complain("pattern result index is out of range");
        return nullptr;
    }
    return p->get(index).get<void>();
}
void *PatternOne(hook::pattern *p)
{
    return p->get_one().get<void>();
}
void *PatternFirst(hook::pattern *p, ptrdiff_t offset)
{
    return p->get_first<void>(offset);
}
void *MatchGet(void *match, ptrdiff_t offset)
{
    return (char *)match + offset;
}
void PatternEach(hook::pattern *p, void (*callback)(void *))
{
    for (size_t i = 0; i < p->size(); ++i)
        callback(p->get(i).get<void>());
}
void *GetPattern(char const *text, ptrdiff_t offset)
{
    hook::pattern p(text);
    return p.get_first<void>(offset);
}
void *ModulePattern(void *module, char const *text, ptrdiff_t offset)
{
    hook::pattern p(module, text);
    return p.get_first<void>(offset);
}
void *RangePattern(uintptr_t first, uintptr_t last, char const *text, ptrdiff_t offset)
{
    hook::pattern p(first, last, text);
    return p.get_first<void>(offset);
}
} // namespace

void BuiltinOwner::Remember(uintptr_t address, size_t size)
{
    // A local of the calling thread is gone by the time the module unloads; restoring it then would overwrite
    // whatever frame lives at that address (a saved register of the unloading code, for instance).
    auto tib = reinterpret_cast<NT_TIB *>(NtCurrentTeb());
    if (address < (uintptr_t)tib->StackBase && address + size > (uintptr_t)tib->StackLimit)
        return;
    std::lock_guard<std::mutex> lock(mutex);
    for (auto &w : writes) // already saved: the oldest bytes are what Revert restores
        if (address >= w.address && address + size <= w.address + w.original.size())
            return;
    WriteRecord record{address, std::vector<uint8_t>(size)};
    injector::ReadMemoryRaw(address, record.original.data(), size, true);
    writes.push_back(std::move(record));
}

void BuiltinOwner::RevertWrites()
{
    std::vector<WriteRecord> list;
    {
        std::lock_guard<std::mutex> lock(mutex);
        list.swap(writes);
    }
    for (auto i = list.rbegin(); i != list.rend(); ++i)
    {
        injector::WriteMemoryRaw(i->address, i->original.data(), i->original.size(), true);
        FlushInstructionCache(GetCurrentProcess(), (void *)i->address, i->original.size());
    }
}

void *HostSymbol(const std::string &name)
{
#define BIND(symbol, fn)                                                                                               \
    {                                                                                                                  \
        symbol, (void *)fn                                                                                             \
    }
    static const std::map<std::string, void *> symbols = {BIND("cxxsnippets_assign", Assign),
                                                          BIND("cxxsnippets_write", Write),
                                                          BIND("cxxsnippets_read", Read),
                                                          BIND("cxxsnippets_write_raw", WriteRaw),
                                                          BIND("cxxsnippets_read_raw", ReadRaw),
                                                          BIND("cxxsnippets_nop", Nop),
                                                          BIND("cxxsnippets_jmp", Jmp),
                                                          BIND("cxxsnippets_call", Call),
                                                          BIND("cxxsnippets_static_enter", StaticEnter),
                                                          BIND("cxxsnippets_static_leave", StaticLeave),
                                                          BIND("cxxsnippets_inline_new", InlineNew),
                                                          BIND("cxxsnippets_inline_release", InlineRelease),
                                                          BIND("cxxsnippets_inline_reset", InlineReset),
                                                          BIND("cxxsnippets_inline_bool", InlineBool),
                                                          BIND("cxxsnippets_inline_enable", InlineEnable),
                                                          BIND("cxxsnippets_inline_disable", InlineDisable),
                                                          BIND("cxxsnippets_inline_enabled", InlineEnabled),
                                                          BIND("cxxsnippets_inline_target", InlineTarget),
                                                          BIND("cxxsnippets_inline_original", InlineOriginal),
                                                          BIND("cxxsnippets_inline_enter", InlineEnter),
                                                          BIND("cxxsnippets_inline_leave", InlineLeave),
                                                          BIND("cxxsnippets_mid_new", MidNew),
                                                          BIND("cxxsnippets_mid_release", MidRelease),
                                                          BIND("cxxsnippets_mid_reset", MidReset),
                                                          BIND("cxxsnippets_mid_bool", MidBool),
                                                          BIND("cxxsnippets_mid_enable", MidEnable),
                                                          BIND("cxxsnippets_mid_disable", MidDisable),
                                                          BIND("cxxsnippets_mid_enabled", MidEnabled),
                                                          BIND("cxxsnippets_mid_target", MidTarget),
                                                          BIND("cxxsnippets_vmt_new", VmtNew),
                                                          BIND("cxxsnippets_vmt_release", VmtRelease),
                                                          BIND("cxxsnippets_vmt_reset", VmtReset),
                                                          BIND("cxxsnippets_vmt_bool", VmtBool),
                                                          BIND("cxxsnippets_vm_new", VmNew),
                                                          BIND("cxxsnippets_vm_release", VmRelease),
                                                          BIND("cxxsnippets_vm_reset", VmReset),
                                                          BIND("cxxsnippets_vm_bool", VmBool),
                                                          BIND("cxxsnippets_vm_original", VmOriginal),
                                                          BIND("cxxsnippets_pattern_new", PatternNew),
                                                          BIND("cxxsnippets_pattern_module", PatternModule),
                                                          BIND("cxxsnippets_pattern_range", PatternRange),
                                                          BIND("cxxsnippets_pattern_count", PatternCount),
                                                          BIND("cxxsnippets_pattern_clear", PatternClear),
                                                          BIND("cxxsnippets_pattern_size", PatternSize),
                                                          BIND("cxxsnippets_pattern_empty", PatternEmpty),
                                                          BIND("cxxsnippets_pattern_get", PatternGet),
                                                          BIND("cxxsnippets_pattern_one", PatternOne),
                                                          BIND("cxxsnippets_pattern_first", PatternFirst),
                                                          BIND("cxxsnippets_pattern_each", PatternEach),
                                                          BIND("cxxsnippets_match_get", MatchGet),
                                                          BIND("cxxsnippets_get_pattern", GetPattern),
                                                          BIND("cxxsnippets_module_pattern", ModulePattern),
                                                          BIND("cxxsnippets_range_pattern", RangePattern)};
#undef BIND
    auto it = symbols.find(name);
    return it == symbols.end() ? nullptr : it->second;
}
} // namespace cxxsnippets
