// Instrumented test plugin. Tokens in its own file name (case-insensitive) pick the behaviour:
//   *failinit*  DllMain returns FALSE
//   *chdir*     DllMain changes the current directory, which the loader must restore
//   *crashinit* InitializeASI writes to a null pointer
// Records ev=attach, ev=init (InitializeASI) and ev=detach to UAL_TEST_REPORT.
#include <windows.h>
#include <psapi.h>
#include <intrin.h>
#include <string>
#include <vector>
#include "../common/report.hpp"

static HMODULE g_self = nullptr;
static volatile LONG g_initCount = 0;
static bool g_crashInit = false;

static std::wstring ModulePath(HMODULE m)
{
    std::wstring s(32768, L'\0');
    s.resize(GetModuleFileNameW(m, s.data(), (DWORD)s.size()));
    return s;
}

static std::wstring FileName(const std::wstring& p)
{
    return p.substr(p.find_last_of(L"\\/") + 1);
}

static std::wstring Lower(std::wstring s)
{
    CharLowerBuffW(s.data(), (DWORD)s.size());
    return s;
}

static bool LoaderLockHeldByThisThread()
{
#ifdef _WIN64
    auto peb = reinterpret_cast<BYTE*>(__readgsqword(0x60));
    auto cs = *reinterpret_cast<RTL_CRITICAL_SECTION**>(peb + 0x110);
#else
    auto peb = reinterpret_cast<BYTE*>(__readfsdword(0x30));
    auto cs = *reinterpret_cast<RTL_CRITICAL_SECTION**>(peb + 0xA0);
#endif
    return cs && (DWORD)(ULONG_PTR)cs->OwningThread == GetCurrentThreadId();
}

static int CountUalModules()
{
    HMODULE mods[1024];
    DWORD needed = 0;
    if (!K32EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &needed))
        return -1;
    int n = 0;
    for (DWORD i = 0; i < needed / sizeof(HMODULE) && i < 1024; ++i)
        if (GetProcAddress(mods[i], "IsUltimateASILoader"))
            ++n;
    return n;
}

static long HostMainEntered()
{
    auto p = reinterpret_cast<volatile LONG*>(GetProcAddress(GetModuleHandleW(nullptr), "g_ualHostMainEntered"));
    return p ? *p : -1;
}

static void Emit(const char* ev, std::initializer_list<std::pair<const char*, std::string>> extra = {})
{
    auto report = ualtest::report_path_from_env();
    if (report.empty())
        return;

    wchar_t cwd[MAX_PATH * 4] = {};
    GetCurrentDirectoryW((DWORD)std::size(cwd), cwd);
    auto path = ModulePath(g_self);

    ualtest::Record r;
    r["src"] = "probe";
    r["ev"] = ev;
    r["mod"] = ualtest::utf8(FileName(path));
    r["path"] = ualtest::utf8(path);
    r["pid"] = std::to_string(GetCurrentProcessId());
    r["tid"] = std::to_string(GetCurrentThreadId());
    r["loaderlock"] = LoaderLockHeldByThisThread() ? "1" : "0";
    r["ualcount"] = std::to_string(CountUalModules());
    r["hostmain"] = std::to_string(HostMainEntered());
    r["cwd"] = ualtest::utf8(cwd);
    r["ticks"] = std::to_string(GetTickCount64());
    for (auto& [k, v] : extra)
        r[k] = v;
    ualtest::append_record(report, r);
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    LONG n = InterlockedIncrement(&g_initCount);
    Emit("init", { { "count", std::to_string(n) } });
    if (g_crashInit)
    {
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
        *(volatile int*)nullptr = 0x0BADC0DE;
    }
}

extern "C" __declspec(dllexport) LONG ProbeInitCount()
{
    return g_initCount;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        g_self = hModule;
        DisableThreadLibraryCalls(hModule);
        auto name = Lower(FileName(ModulePath(hModule)));
        Emit("attach");

        if (name.find(L"chdir") != std::wstring::npos)
        {
            wchar_t win[MAX_PATH];
            GetWindowsDirectoryW(win, MAX_PATH);
            SetCurrentDirectoryW(win);
        }
        g_crashInit = name.find(L"crashinit") != std::wstring::npos;
        if (name.find(L"failinit") != std::wstring::npos)
            return FALSE;
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        Emit("detach");
    }
    return TRUE;
}
