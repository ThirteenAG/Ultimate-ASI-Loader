#include <windows.h>

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		// Unprotect the module NOW
		auto hExecutableInstance = (size_t)GetModuleHandle(NULL);
		IMAGE_NT_HEADERS* ntHeader = (IMAGE_NT_HEADERS*)(hExecutableInstance + ((IMAGE_DOS_HEADER*)hExecutableInstance)->e_lfanew);
		SIZE_T size = ntHeader->OptionalHeader.SizeOfImage;
		DWORD oldProtect;
		VirtualProtect((VOID*)hExecutableInstance, size, PAGE_EXECUTE_READWRITE, &oldProtect);
	}
	return TRUE;
}