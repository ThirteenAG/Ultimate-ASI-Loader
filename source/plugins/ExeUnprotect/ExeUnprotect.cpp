// Makes the game's whole exe image writable and executable, for old plugins and trainers that
// patch it with plain pointer writes instead of calling VirtualProtect first.
// Folders load in the order game folder, scripts\, plugins\, and files within a folder in
// directory order (alphabetical on NTFS). To run first, put this in the game folder and the
// plugins that need it in scripts\ or plugins\.
#include <windows.h>
#include <mutex>
#include <stacktrace>

static void Init()
{
    auto base = reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr));
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    DWORD previous;
    VirtualProtect(base, nt->OptionalHeader.SizeOfImage, PAGE_EXECUTE_READWRITE, &previous);
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

// Ultimate ASI Loader calls this right after loading the plugin, outside the loader lock.
extern "C" __declspec(dllexport) void InitializeASI()
{
    InitOnce();
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID)
{
    // other ASI loaders only load the DLL; VirtualProtect is safe in DllMain
    if (reason == DLL_PROCESS_ATTACH && !LoadedByUltimateASILoader()) InitOnce();
    return TRUE;
}
