// The smallest ASI plugin. If the message appears, the loader works.
//
// An ASI plugin is a DLL renamed to .asi. Ultimate ASI Loader loads it with LoadLibrary, then
// calls its exported InitializeASI. Do your work there, not in DllMain. DllMain runs under the
// loader lock, where showing a window, waiting on a thread, loading a DLL or using COM can
// deadlock or crash the game. InitializeASI runs outside the lock, when the game's own code
// starts (DontLoadFromDllMain=1).
//
// Other ASI loaders only call LoadLibrary, so DllMain starts the plugin when they load it.
#include <windows.h>
#include <mutex>
#include <stacktrace>

static void Init()
{
    MessageBoxW(nullptr, L"ASI Loader works correctly.", L"ASI Loader Test Plugin", MB_ICONINFORMATION);
}

static void InitOnce()
{
    static std::once_flag once;
    std::call_once(once, Init);
}

// Ultimate ASI Loader is on the call stack while it loads the plugin.
static bool LoadedByUltimateASILoader()
{
    for (const auto& frame : std::stacktrace::current())
    {
        HMODULE m = nullptr;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)frame.native_handle(), &m) &&
            GetProcAddress(m, "IsUltimateASILoader"))
            return true;
    }
    return false;
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    InitOnce();
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID)
{
    // Another loader won't call InitializeASI, so show the message here. The game waits for it,
    // which is handy for attaching a debugger. Under the loader lock a window can hang the game
    // if something inspects it, such as an accessibility tool.
    if (reason == DLL_PROCESS_ATTACH && !LoadedByUltimateASILoader()) InitOnce();
    return TRUE;
}
