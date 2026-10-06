// Game code of ual_host_launcher.exe. Loads the loader (dinput8.dll) at run time, then calls
// Sleep through a pre-resolved pointer that only the loader's inline hooks can catch.
#include <windows.h>
#include "../common/report.hpp"

namespace
{
    void Emit(const char* ev, const std::string& extra = {})
    {
        ualtest::Record r;
        r["src"] = "host";
        r["ev"] = ev;
        if (!extra.empty()) r["detail"] = extra;
        ualtest::append_record(ualtest::report_path_from_env(), r);
    }
}

extern "C" __declspec(dllexport) int __stdcall GameMain()
{
    // Resolved before the loader is in, so no import table is involved
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    auto sleep = reinterpret_cast<decltype(&Sleep)>(GetProcAddress(k32, "Sleep"));
    auto getModule = reinterpret_cast<decltype(&GetModuleHandleW)>(GetProcAddress(k32, "GetModuleHandleW"));
    auto getProc = reinterpret_cast<decltype(&GetProcAddress)>(GetProcAddress(k32, "GetProcAddress"));

    HMODULE ual = LoadLibraryW(L"dinput8.dll");
    sleep(0); // first game call after the loader is in

    // Plugins initialized by now? Nothing here goes through a patched import.
    LONG inits = -1;
    if (HMODULE probe = getModule(L"probe.asi"))
        if (auto count = reinterpret_cast<LONG (*)()>(getProc(probe, "ProbeInitCount"))) inits = count();
    Emit("done", std::string(ual ? "loaded" : "not loaded") + " inits=" + std::to_string(inits));
    ExitProcess(0);
}
