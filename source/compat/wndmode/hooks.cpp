#include "internal.hpp"
#include <MinHook.h>
#include <psapi.h>
#include <winternl.h>

namespace wndmode
{
    namespace
    {
        struct ExportHook
        {
            std::wstring module; // lower case base name
            std::string name;
            void* detour;
            void** original;
            void* target = nullptr;
        };

        struct VtableSlot
        {
            void** vtbl;
            int slot;
            void* original;
        };

        CRITICAL_SECTION g_hookCs;
        std::vector<ExportHook> g_exports;
        SRWLOCK g_vtLock = SRWLOCK_INIT;
        std::vector<VtableSlot> g_slots;
        std::vector<std::pair<void**, int>> g_tags;
        void* g_dllCookie = nullptr;
        bool g_init = false;

        // ASCII only. Enough for the names in g_exports, and this runs inside the DLL load
        // notification where calling into user32 is unsafe.
        std::wstring Lower(std::wstring s)
        {
            for (auto& c : s)
                if (c >= L'A' && c <= L'Z') c = wchar_t(c - L'A' + L'a');
            return s;
        }

        // The loader itself may be named d3d9.dll, ddraw.dll etc. Hooks go into the real system DLL,
        // never into the loader's export stubs.
        bool IsLoaderModule(HMODULE m)
        {
            return GetProcAddress(m, "IsUltimateASILoader") != nullptr;
        }

        bool Install(ExportHook& h, HMODULE mod)
        {
            void* target = (void*)GetProcAddress(mod, h.name.c_str());
            if (!target) return false;
            MH_STATUS st = MH_CreateHook(target, h.detour, h.original);
            if (st != MH_OK)
            {
                // ALREADY_CREATED means someone else (another hook set in this process) owns the target; MinHook
                // then leaves *original empty, so the detour could never reach the real function
                Log("hook %s failed: %d", h.name.c_str(), st);
                return false;
            }
            MH_QueueEnableHook(target);
            h.target = target;
            return true;
        }

        // A DLL we hooked was unloaded: forget the hooks so they are installed again if it comes back,
        // and before another module can be mapped at those addresses.
        void ForgetModule(void* base, size_t size)
        {
            bool any = false;
            for (auto& h : g_exports)
            {
                if (!h.target || (uintptr_t)h.target < (uintptr_t)base || (uintptr_t)h.target >= (uintptr_t)base + size) continue;
                MH_RemoveHook(h.target); // the code is gone, this only releases MinHook's entry
                h.target = nullptr;
                *h.original = nullptr;
                any = true;
            }
            if (any) Log("module at %p unloaded, its hooks are pending again", base);
        }

        void InstallForModule(HMODULE mod, const std::wstring& baseName)
        {
            // Runs for every DLL load, and copy protections map DLLs whose export tables
            // GetProcAddress can't read, so only touch DLLs with pending hooks.
            bool wanted = false;
            for (auto& h : g_exports)
                if (!h.target && h.module == baseName)
                    wanted = true;
            if (!wanted || IsLoaderModule(mod)) return;
            bool any = false;
            for (auto& h : g_exports)
                if (!h.target && h.module == baseName)
                    any |= Install(h, mod);
            if (any) MH_ApplyQueued();
        }

        // ntdll LdrRegisterDllNotification
        struct DllNotificationData
        {
            ULONG Flags;
            PCUNICODE_STRING FullDllName;
            PCUNICODE_STRING BaseDllName;
            PVOID DllBase;
            ULONG SizeOfImage;
        };
        using DllNotificationFn = VOID(CALLBACK*)(ULONG reason, const DllNotificationData* data, PVOID ctx);
        using LdrRegisterFn = NTSTATUS(NTAPI*)(ULONG, DllNotificationFn, PVOID, PVOID*);
        using LdrUnregisterFn = NTSTATUS(NTAPI*)(PVOID);

        VOID CALLBACK OnDllNotification(ULONG reason, const DllNotificationData* data, PVOID)
        {
            if (!data) return;
            if (reason == 2 /* unloaded */)
            {
                ScopedCs lock(g_hookCs);
                ForgetModule(data->DllBase, data->SizeOfImage);
                return;
            }
            if (reason != 1 /* loaded */ || !data->BaseDllName) return;
            std::wstring name(data->BaseDllName->Buffer, data->BaseDllName->Length / sizeof(wchar_t));
            ScopedCs lock(g_hookCs);
            InstallForModule((HMODULE)data->DllBase, Lower(name));
        }
    }

    std::wstring ModuleNameOf(const void* addr)
    {
        HMODULE m = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)addr, &m))
            return {};
        wchar_t path[MAX_PATH];
        DWORD n = GetModuleFileNameW(m, path, MAX_PATH);
        std::wstring s(path, n);
        return Lower(s.substr(s.find_last_of(L"\\/") + 1));
    }

    bool CalledFromRuntime(const void* ret)
    {
        HMODULE m = nullptr;
        if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)ret, &m))
            return true; // generated code or unmapped: leave it alone
        auto n = ModuleNameOf(ret);
        return n == L"d3d8.dll" || n == L"d3d8d.dll" || n == L"d3d9.dll" || n == L"ddraw.dll" || n == L"ddrawex.dll" || n == L"d3d8thk.dll" ||
               n == L"dxgi.dll" || n == L"d3d11.dll" || n == L"d3dim.dll" || n == L"d3dim700.dll" || n == L"user32.dll" || n == L"win32u.dll" ||
               n == L"gdi32.dll" || n == L"gdi32full.dll" || n == L"dwmapi.dll" || n == L"uxtheme.dll" || n == L"comctl32.dll";
    }

    static void EnsureInit()
    {
        if (g_init) return;
        g_init = true;
        InitializeCriticalSection(&g_hookCs);
        MH_STATUS st = MH_Initialize();
        if (st != MH_OK && st != MH_ERROR_ALREADY_INITIALIZED)
            Log("MH_Initialize failed: %d", st);
    }

    void HookExport(const wchar_t* module, const char* name, void* detour, void** original)
    {
        EnsureInit();
        ScopedCs lock(g_hookCs);
        g_exports.push_back({ Lower(module), name, detour, original });
    }

    void InstallPendingHooks()
    {
        EnsureInit();
        ScopedCs lock(g_hookCs);
        HMODULE mods[1024];
        DWORD needed = 0;
        if (K32EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &needed))
        {
            for (DWORD i = 0; i < needed / sizeof(HMODULE) && i < 1024; ++i)
            {
                wchar_t path[MAX_PATH];
                DWORD n = GetModuleFileNameW(mods[i], path, MAX_PATH);
                std::wstring s(path, n);
                std::wstring base = Lower(s.substr(s.find_last_of(L"\\/") + 1));
                if (IsLoaderModule(mods[i])) continue;
                for (auto& h : g_exports)
                    if (!h.target && h.module == base)
                        Install(h, mods[i]);
            }
        }
        MH_ApplyQueued(); // one thread freeze for all hooks

        if (!g_dllCookie)
        {
            if (auto reg = (LdrRegisterFn)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "LdrRegisterDllNotification"))
                reg(0, OnDllNotification, nullptr, &g_dllCookie);
        }
    }

    void RemoveAllHooks()
    {
        if (!g_init) return;
        if (g_dllCookie)
        {
            if (auto unreg = (LdrUnregisterFn)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "LdrUnregisterDllNotification"))
                unreg(g_dllCookie);
            g_dllCookie = nullptr;
        }
        {
            ScopedCs lock(g_hookCs);
            for (auto& h : g_exports)
                if (h.target)
                {
                    MH_QueueDisableHook(h.target);
                    h.target = nullptr;
                }
            MH_ApplyQueued();
        }
        AcquireSRWLockExclusive(&g_vtLock);
        for (auto& s : g_slots)
        {
            DWORD old;
            if (VirtualProtect(&s.vtbl[s.slot], sizeof(void*), PAGE_READWRITE, &old))
            {
                s.vtbl[s.slot] = s.original;
                VirtualProtect(&s.vtbl[s.slot], sizeof(void*), old, &old);
            }
        }
        g_slots.clear();
        ReleaseSRWLockExclusive(&g_vtLock);
    }

    void PatchVtable(void* object, int slot, void* detour)
    {
        if (!object) return;
        void** vtbl = *(void***)object;
        AcquireSRWLockExclusive(&g_vtLock);
        bool done = false;
        for (auto& s : g_slots)
            if (s.vtbl == vtbl && s.slot == slot) { done = true; break; }
        if (!done && vtbl[slot] != detour)
        {
            DWORD old;
            if (VirtualProtect(&vtbl[slot], sizeof(void*), PAGE_READWRITE, &old))
            {
                g_slots.push_back({ vtbl, slot, vtbl[slot] });
                vtbl[slot] = detour;
                VirtualProtect(&vtbl[slot], sizeof(void*), old, &old);
                FlushInstructionCache(GetCurrentProcess(), &vtbl[slot], sizeof(void*));
            }
        }
        ReleaseSRWLockExclusive(&g_vtLock);
    }

    void* Original(void* object, int slot)
    {
        void** vtbl = *(void***)object;
        void* result = nullptr;
        AcquireSRWLockShared(&g_vtLock);
        for (auto& s : g_slots)
            if (s.vtbl == vtbl && s.slot == slot) { result = s.original; break; }
        ReleaseSRWLockShared(&g_vtLock);
        return result ? result : vtbl[slot];
    }

    void SetVtableTag(void* object, int tag)
    {
        void** vtbl = *(void***)object;
        AcquireSRWLockExclusive(&g_vtLock);
        bool found = false;
        for (auto& t : g_tags)
            if (t.first == vtbl) { t.second = tag; found = true; }
        if (!found) g_tags.push_back({ vtbl, tag });
        ReleaseSRWLockExclusive(&g_vtLock);
    }

    int VtableTag(void* object)
    {
        void** vtbl = *(void***)object;
        int tag = 0;
        AcquireSRWLockShared(&g_vtLock);
        for (auto& t : g_tags)
            if (t.first == vtbl) { tag = t.second; break; }
        ReleaseSRWLockShared(&g_vtLock);
        return tag;
    }
}
