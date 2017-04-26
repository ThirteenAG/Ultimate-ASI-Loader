#include <windows.h>

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		MessageBox(0, "ASI Loader works correctly.", "ASI Loader Test Plugin", MB_ICONWARNING);
	}
	return TRUE;
}