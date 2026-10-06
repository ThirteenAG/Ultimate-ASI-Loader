#include "hooks.hpp"
#include <MinHook.h>
#include <mutex>
#include <vector>

namespace ual
{
    namespace
    {
        std::mutex g_lock;

        void* Target(const HookSpec& h)
        {
            HMODULE m = GetModuleHandleW(h.module);
            return m ? (void*)GetProcAddress(m, h.function) : nullptr;
        }
    }

    int InstallHooks(std::span<const HookSpec> hooks)
    {
        std::lock_guard guard(g_lock);
        MH_STATUS init = MH_Initialize();
        if (init != MH_OK && init != MH_ERROR_ALREADY_INITIALIZED) return 0;

        int active = 0;
        struct Pending
        {
            const HookSpec* spec;
            void* target;
        };
        std::vector<Pending> pending;
        for (const auto& h : hooks)
        {
            if (*h.original)
            {
                ++active;
                continue;
            }
            void* target = Target(h);
            if (!target) continue;
            void* original = nullptr;
            MH_STATUS st = MH_CreateHook(target, h.detour, &original);
            if (st != MH_OK) continue;
            if (MH_QueueEnableHook(target) != MH_OK)
            {
                MH_RemoveHook(target);
                continue;
            }
            // publish the trampoline before the hook can be entered
            *h.original = original;
            pending.push_back({ &h, target });
        }
        if (!pending.empty())
        {
            // hooked code jumps into this module from now on, so pin it
            HMODULE self = nullptr;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, (LPCWSTR)&Target, &self);
            if (MH_ApplyQueued() == MH_OK)
                active += (int)pending.size();
            else
            {
                // nothing was enabled (e.g. the thread snapshot failed): undo so the caller can fall back or retry
                for (const auto& p : pending)
                {
                    MH_RemoveHook(p.target);
                    *p.spec->original = nullptr;
                }
            }
        }
        return active;
    }

    void DisableHooks(std::span<const HookSpec> hooks)
    {
        std::lock_guard guard(g_lock);
        bool queued = false;
        for (const auto& h : hooks)
            if (*h.original)
                if (void* target = Target(h); target && MH_QueueDisableHook(target) == MH_OK) queued = true;
        if (queued) MH_ApplyQueued();
    }
}
