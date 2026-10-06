// UseSpeedHack: scales GetTickCount, timeGetTime and QueryPerformanceCounter by SpeedHackMultiple/10.
// Like the legacy DLL, only the render thread (the one that created the D3D device or DirectDraw
// primary) is affected.
#include "internal.hpp"

namespace wndmode
{
    namespace
    {
        volatile DWORD g_thread = 0;

        // virtual = baseVirtual + (real - baseReal) * N / 10, starting at real time
        struct Clock
        {
            bool started = false;
            uint64_t baseReal = 0;
            uint64_t baseVirtual = 0;

            uint64_t Scale(uint64_t real)
            {
                if (!started)
                {
                    started = true;
                    baseReal = baseVirtual = real;
                }
                uint64_t delta = real - baseReal;
                return baseVirtual + delta / 10 * (uint64_t)g_cfg.SpeedHackMultiple + delta % 10 * (uint64_t)g_cfg.SpeedHackMultiple / 10;
            }
        };

        // only touched by the speed hack thread
        Clock g_tick, g_time, g_qpc;
        uint64_t g_timeHigh = 0;
        DWORD g_timeLast = 0;

        decltype(&GetTickCount) oGetTickCount;
        using TimeGetTimeFn = DWORD(WINAPI*)();
        TimeGetTimeFn oTimeGetTime;
        decltype(&QueryPerformanceCounter) oQueryPerformanceCounter;

        DWORD WINAPI hkGetTickCount()
        {
            if (!SpeedHackOnThisThread()) return oGetTickCount();
            return (DWORD)g_tick.Scale(GetTickCount64());
        }

        DWORD WINAPI hkTimeGetTime()
        {
            DWORD real = oTimeGetTime();
            if (!SpeedHackOnThisThread()) return real;
            if (real < g_timeLast) g_timeHigh += 0x100000000ull; // 49.7 day wrap
            g_timeLast = real;
            return (DWORD)g_time.Scale(g_timeHigh | real);
        }

        BOOL WINAPI hkQueryPerformanceCounter(LARGE_INTEGER* out)
        {
            // also called directly by WaitVerticalBlankEmulated, before or without the speed hack being installed
            BOOL ok = oQueryPerformanceCounter ? oQueryPerformanceCounter(out) : QueryPerformanceCounter(out);
            if (ok && out && SpeedHackOnThisThread())
                out->QuadPart = (LONGLONG)g_qpc.Scale((uint64_t)out->QuadPart);
            return ok;
        }
    }

    DWORD RealTickCount()
    {
        return (DWORD)GetTickCount64();
    }

    void SetSpeedHackThread()
    {
        if (g_cfg.UseSpeedHack) g_thread = GetCurrentThreadId();
    }

    bool SpeedHackOnThisThread()
    {
        return g_cfg.UseSpeedHack && g_thread == GetCurrentThreadId();
    }

    void WaitVerticalBlankEmulated()
    {
        // 60 Hz in (possibly scaled) game time
        LARGE_INTEGER freq, now;
        QueryPerformanceFrequency(&freq);
        hkQueryPerformanceCounter(&now);
        const LONGLONG period = freq.QuadPart / 60;
        const LONGLONG target = (now.QuadPart / period + 1) * period;
        for (;;)
        {
            hkQueryPerformanceCounter(&now);
            if (now.QuadPart >= target) break;
            Sleep((target - now.QuadPart) * 1000 / freq.QuadPart > 2 ? 1 : 0);
        }
    }

    void InstallSpeedHack()
    {
        if (!g_cfg.UseSpeedHack) return;
        HookExport(L"kernel32.dll", "GetTickCount", (void*)hkGetTickCount, (void**)&oGetTickCount);
        HookExport(L"kernel32.dll", "QueryPerformanceCounter", (void*)hkQueryPerformanceCounter, (void**)&oQueryPerformanceCounter);
        HookExport(L"winmm.dll", "timeGetTime", (void*)hkTimeGetTime, (void**)&oTimeGetTime);
    }
}
