#include <windows.h>
void Init();
void Shutdown();
extern "C" __declspec(dllexport) void CaseInit()
{
    Init();
}
extern "C" __declspec(dllexport) void CaseShutdown()
{
    Shutdown();
}
BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID)
{
    return TRUE;
}
