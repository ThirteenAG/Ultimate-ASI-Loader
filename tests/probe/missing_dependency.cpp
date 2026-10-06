// Builds ual_missing_dependency.dll (with UAL_MISSING_DEPENDENCY_DLL), never deployed, and
// probe_missingdep.asi, which imports it. Loading the plugin fails with ERROR_MOD_NOT_FOUND
// and the loader must show "Unable to load ..." instead of crashing.
#include <windows.h>

#ifdef UAL_MISSING_DEPENDENCY_DLL

extern "C" __declspec(dllexport) int WINAPI UalMissingDependencyFunction()
{
    return 42;
}

#else

extern "C" __declspec(dllimport) int WINAPI UalMissingDependencyFunction();

extern "C" __declspec(dllexport) void InitializeASI()
{
    UalMissingDependencyFunction();
}

#endif

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID)
{
    return TRUE;
}
