#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <direct.h>
using namespace std;

#define IDR_INI1 101
HMODULE dllModule;

DWORD WINAPI Thread(LPVOID param)
{
	char			moduleName[MAX_PATH];
	char			preparedPath[128];	// stores scripts\*exename*\settings.ini
	char*			tempPointer;

	GetModuleFileName(NULL, moduleName, MAX_PATH);
	tempPointer = strrchr(moduleName, '.');
	*tempPointer = '\0';

	tempPointer = strrchr(moduleName, '\\');
	strncpy_s(preparedPath, "scripts", 8);
	_mkdir(preparedPath);
	strcat_s(preparedPath, tempPointer);
	_mkdir(preparedPath);
	strcat_s(preparedPath, "\\settings.ini");

	ofstream settingsFile;
	settingsFile.open(preparedPath, ios::out | ios::in | ios::binary | ios::trunc);
	
	HRSRC hResource = FindResource(dllModule, MAKEINTRESOURCE(IDR_INI1), RT_RCDATA);
	if (hResource)
	{
		HGLOBAL hLoadedResource = LoadResource(dllModule, hResource);
		if (hLoadedResource)
		{
			LPVOID pLockedResource = LockResource(hLoadedResource);
			if (pLockedResource)
			{
				DWORD dwResourceSize = SizeofResource(dllModule, hResource);
				if (0 != dwResourceSize)
				{
					settingsFile.write((const char*)pLockedResource, dwResourceSize);
				}
			}
		}
	}
	settingsFile.close();

	MessageBox(0, "ASI Loader works correctly, UALx86.TestPlugin.asi will no longer be loaded. Check settings.ini inside scripts\\*your_exe_name* folder for more info.", "ASI Loader Test Plugin", MB_ICONWARNING);
	return 0;
}


BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		dllModule = hInst;
		CreateThread(0, 0, (LPTHREAD_START_ROUTINE)&Thread, NULL, 0, NULL);
	}
	return TRUE;
}