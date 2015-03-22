#include "includes\structs.h"

void ExcludedEntriesListInit(ExcludedEntriesList* list)
{
	list->first = NULL;
	list->last = NULL;
}

void ExcludedEntriesListPush(ExcludedEntriesList* list, const char* entryName)
{
	ExcludedEntry* 	newEntry = (ExcludedEntry*)malloc(sizeof(ExcludedEntry));
	int 			length = strlen(entryName) + 1;
	if ( !list->first )
		list->first = newEntry;
	else
		list->last->next = newEntry;

	newEntry->prev = list->last;
	newEntry->next = NULL;
	list->last = newEntry;

	newEntry->entry = (char*)malloc(length);
	strncpy(newEntry->entry, entryName, length);
}

bool ExcludedEntriesListHasEntry(ExcludedEntriesList* list, const char* entryName)
{
	ExcludedEntry*	it = list->first;
	while ( it )
	{
		if ( !_stricmp(it->entry, entryName) )
		{
			// It has an entry, we can pop it now
			if ( it->next )
				it->next->prev = it->prev;
			if ( it->prev )
				it->prev->next = it->next;

			if ( list->first == it )
				list->first = it->next;

			free(it->entry);
			free(it);
			return true;
		}
		it = it->next;
	}

	return false;
}

void ExcludedEntriesListFree(ExcludedEntriesList* list)
{
	ExcludedEntry*	it = list->first;
	while ( it )
	{
		ExcludedEntry* nextEntry = it->next;
		free(it->entry);
		free(it);
		it = nextEntry;
	}
}

void FindFiles(WIN32_FIND_DATA* fd, ExcludedEntriesList* list)
{
	HANDLE asiFile = FindFirstFile ("*.asi", fd);
	if (asiFile != INVALID_HANDLE_VALUE)
	{

		do {
			if (!(fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {

				unsigned int pos = 5;
				while (fd->cFileName[pos])
					++pos;
				if (fd->cFileName[pos-4] == '.' &&
					(fd->cFileName[pos-3] == 'a' || fd->cFileName[pos-3] == 'A') &&
					(fd->cFileName[pos-2] == 's' || fd->cFileName[pos-2] == 'S') &&
					(fd->cFileName[pos-1] == 'i' || fd->cFileName[pos-1] == 'I'))
				{
					if ( !list || !ExcludedEntriesListHasEntry(list, fd->cFileName) )
						LoadLibrary (fd->cFileName);
				}
			}

		} while (FindNextFile (asiFile, fd));
		FindClose (asiFile);
	}
}

void LoadOriginalLibrary()
{
	if (_stricmp(DllName + 1, "vorbisFile.dll") == NULL)
	{
		HMEMORYMODULE vorbisHooked = MemoryLoadLibrary(vorbisHookedhex);
		__ov_open_callbacks = MemoryGetProcAddress(vorbisHooked, "ov_open_callbacks");
		__ov_clear = MemoryGetProcAddress(vorbisHooked, "ov_clear");
		__ov_time_total = MemoryGetProcAddress(vorbisHooked, "ov_time_total");
		__ov_time_tell = MemoryGetProcAddress(vorbisHooked, "ov_time_tell");
		__ov_read = MemoryGetProcAddress(vorbisHooked, "ov_read");
		__ov_info = MemoryGetProcAddress(vorbisHooked, "ov_info");
		__ov_time_seek = MemoryGetProcAddress(vorbisHooked, "ov_time_seek");
		__ov_time_seek_page = MemoryGetProcAddress(vorbisHooked, "ov_time_seek_page");
	}
	else
	{
		if (_stricmp(DllName + 1, "dsound.dll") == NULL) {
			dsound.dll = LoadLibrary(szSystemPath);
			dsound.DirectSoundCaptureCreate = GetProcAddress(dsound.dll, "DirectSoundCaptureCreate");
			dsound.DirectSoundCaptureCreate8 = GetProcAddress(dsound.dll, "DirectSoundCaptureCreate8");
			dsound.DirectSoundCaptureEnumerateA = GetProcAddress(dsound.dll, "DirectSoundCaptureEnumerateA");
			dsound.DirectSoundCaptureEnumerateW = GetProcAddress(dsound.dll, "DirectSoundCaptureEnumerateW");
			dsound.DirectSoundCreate = GetProcAddress(dsound.dll, "DirectSoundCreate");
			dsound.DirectSoundCreate8 = GetProcAddress(dsound.dll, "DirectSoundCreate8");
			dsound.DirectSoundEnumerateA = GetProcAddress(dsound.dll, "DirectSoundEnumerateA");
			dsound.DirectSoundEnumerateW = GetProcAddress(dsound.dll, "DirectSoundEnumerateW");
			dsound.DirectSoundFullDuplexCreate = GetProcAddress(dsound.dll, "DirectSoundFullDuplexCreate");
			dsound.DllCanUnloadNow_dsound = GetProcAddress(dsound.dll, "DllCanUnloadNow_dsound");
			dsound.DllGetClassObject_dsound = GetProcAddress(dsound.dll, "DllGetClassObject_dsound");
			dsound.GetDeviceID = GetProcAddress(dsound.dll, "GetDeviceID");
		}
		else
		{
			if (_stricmp(DllName + 1, "dinput8.dll") == NULL) {
				dinput8.dll = LoadLibrary(szSystemPath);
				dinput8.DirectInput8Create = GetProcAddress(dinput8.dll, "DirectInput8Create");
				dinput8.DllCanUnloadNow = GetProcAddress(dinput8.dll, "DllCanUnloadNow");
				dinput8.DllGetClassObject = GetProcAddress(dinput8.dll, "DllGetClassObject");
				dinput8.DllRegisterServer = GetProcAddress(dinput8.dll, "DllRegisterServer");
				dinput8.DllUnregisterServer = GetProcAddress(dinput8.dll, "DllUnregisterServer");
			}
			else
			{
				if (_stricmp(DllName + 1, "ddraw.dll") == NULL) {
					ddraw.dll = LoadLibrary(szSystemPath);
					ddraw.AcquireDDThreadLock = GetProcAddress(ddraw.dll, "AcquireDDThreadLock");
					ddraw.CheckFullscreen = GetProcAddress(ddraw.dll, "CheckFullscreen");
					ddraw.CompleteCreateSysmemSurface = GetProcAddress(ddraw.dll, "CompleteCreateSysmemSurface");
					ddraw.D3DParseUnknownCommand = GetProcAddress(ddraw.dll, "D3DParseUnknownCommand");
					ddraw.DDGetAttachedSurfaceLcl = GetProcAddress(ddraw.dll, "DDGetAttachedSurfaceLcl");
					ddraw.DDInternalLock = GetProcAddress(ddraw.dll, "DDInternalLock");
					ddraw.DDInternalUnlock = GetProcAddress(ddraw.dll, "DDInternalUnlock");
					ddraw.DSoundHelp = GetProcAddress(ddraw.dll, "DSoundHelp");
					ddraw.DirectDrawCreate = GetProcAddress(ddraw.dll, "DirectDrawCreate");
					ddraw.DirectDrawCreateClipper = GetProcAddress(ddraw.dll, "DirectDrawCreateClipper");
					ddraw.DirectDrawCreateEx = GetProcAddress(ddraw.dll, "DirectDrawCreateEx");
					ddraw.DirectDrawEnumerateA = GetProcAddress(ddraw.dll, "DirectDrawEnumerateA");
					ddraw.DirectDrawEnumerateExA = GetProcAddress(ddraw.dll, "DirectDrawEnumerateExA");
					ddraw.DirectDrawEnumerateExW = GetProcAddress(ddraw.dll, "DirectDrawEnumerateExW");
					ddraw.DirectDrawEnumerateW = GetProcAddress(ddraw.dll, "DirectDrawEnumerateW");
					ddraw.DllCanUnloadNow = GetProcAddress(ddraw.dll, "DllCanUnloadNow");
					ddraw.DllGetClassObject = GetProcAddress(ddraw.dll, "DllGetClassObject");
					ddraw.GetDDSurfaceLocal = GetProcAddress(ddraw.dll, "GetDDSurfaceLocal");
					ddraw.GetOLEThunkData = GetProcAddress(ddraw.dll, "GetOLEThunkData");
					ddraw.GetSurfaceFromDC = GetProcAddress(ddraw.dll, "GetSurfaceFromDC");
					ddraw.RegisterSpecialCase = GetProcAddress(ddraw.dll, "RegisterSpecialCase");
					ddraw.ReleaseDDThreadLock = GetProcAddress(ddraw.dll, "ReleaseDDThreadLock");
				}
				else
				{
					if (_stricmp(DllName + 1, "d3d8.dll") == NULL) {
						d3d8.dll = LoadLibrary(szSystemPath);
						d3d8.DebugSetMute_d3d8 = GetProcAddress(d3d8.dll, "DebugSetMute_d3d8");
						d3d8.Direct3DCreate8 = GetProcAddress(d3d8.dll, "Direct3DCreate8");
						d3d8.ValidatePixelShader = GetProcAddress(d3d8.dll, "ValidatePixelShader");
						d3d8.ValidateVertexShader = GetProcAddress(d3d8.dll, "ValidateVertexShader");
					}
					else {
						if (_stricmp(DllName + 1, "d3d9.dll") == NULL) {
							d3d9.dll = LoadLibrary(szSystemPath);
							d3d9.D3DPERF_BeginEvent = GetProcAddress(d3d9.dll, "D3DPERF_BeginEvent");
							d3d9.D3DPERF_EndEvent = GetProcAddress(d3d9.dll, "D3DPERF_EndEvent");
							d3d9.D3DPERF_GetStatus = GetProcAddress(d3d9.dll, "D3DPERF_GetStatus");
							d3d9.D3DPERF_QueryRepeatFrame = GetProcAddress(d3d9.dll, "D3DPERF_QueryRepeatFrame");
							d3d9.D3DPERF_SetMarker = GetProcAddress(d3d9.dll, "D3DPERF_SetMarker");
							d3d9.D3DPERF_SetOptions = GetProcAddress(d3d9.dll, "D3DPERF_SetOptions");
							d3d9.D3DPERF_SetRegion = GetProcAddress(d3d9.dll, "D3DPERF_SetRegion");
							d3d9.DebugSetLevel = GetProcAddress(d3d9.dll, "DebugSetLevel");
							d3d9.DebugSetMute = GetProcAddress(d3d9.dll, "DebugSetMute");
							//d3d9.Direct3D9EnableMaximizedWindowedModeShim = GetProcAddress(d3d9.dll, "Direct3D9EnableMaximizedWindowedModeShim");
							d3d9.Direct3DCreate9 = GetProcAddress(d3d9.dll, "Direct3DCreate9");
							d3d9.Direct3DCreate9Ex = GetProcAddress(d3d9.dll, "Direct3DCreate9Ex");
							d3d9.Direct3DShaderValidatorCreate9 = GetProcAddress(d3d9.dll, "Direct3DShaderValidatorCreate9");
							d3d9.PSGPError = GetProcAddress(d3d9.dll, "PSGPError");
							d3d9.PSGPSampleTexture = GetProcAddress(d3d9.dll, "PSGPSampleTexture");
						}
						else
						{
							if (_stricmp(DllName + 1, "d3d11.dll") == NULL) {
								d3d11.dll = LoadLibrary(szSystemPath);
								d3d11.D3D11CoreCreateDevice = GetProcAddress(d3d11.dll, "D3D11CoreCreateDevice");
								d3d11.D3D11CoreCreateLayeredDevice = GetProcAddress(d3d11.dll, "D3D11CoreCreateLayeredDevice");
								d3d11.D3D11CoreGetLayeredDeviceSize = GetProcAddress(d3d11.dll, "D3D11CoreGetLayeredDeviceSize");
								d3d11.D3D11CoreRegisterLayers = GetProcAddress(d3d11.dll, "D3D11CoreRegisterLayers");
								d3d11.D3D11CreateDevice = GetProcAddress(d3d11.dll, "D3D11CreateDevice");
								d3d11.D3D11CreateDeviceAndSwapChain = GetProcAddress(d3d11.dll, "D3D11CreateDeviceAndSwapChain");
								d3d11.D3DKMTCloseAdapter = GetProcAddress(d3d11.dll, "D3DKMTCloseAdapter");
								d3d11.D3DKMTCreateAllocation = GetProcAddress(d3d11.dll, "D3DKMTCreateAllocation");
								d3d11.D3DKMTCreateContext = GetProcAddress(d3d11.dll, "D3DKMTCreateContext");
								d3d11.D3DKMTCreateDevice = GetProcAddress(d3d11.dll, "D3DKMTCreateDevice");
								d3d11.D3DKMTCreateSynchronizationObject = GetProcAddress(d3d11.dll, "D3DKMTCreateSynchronizationObject");
								d3d11.D3DKMTDestroyAllocation = GetProcAddress(d3d11.dll, "D3DKMTDestroyAllocation");
								d3d11.D3DKMTDestroyContext = GetProcAddress(d3d11.dll, "D3DKMTDestroyContext");
								d3d11.D3DKMTDestroyDevice = GetProcAddress(d3d11.dll, "D3DKMTDestroyDevice");
								d3d11.D3DKMTDestroySynchronizationObject = GetProcAddress(d3d11.dll, "D3DKMTDestroySynchronizationObject");
								d3d11.D3DKMTEscape = GetProcAddress(d3d11.dll, "D3DKMTEscape");
								d3d11.D3DKMTGetContextSchedulingPriority = GetProcAddress(d3d11.dll, "D3DKMTGetContextSchedulingPriority");
								d3d11.D3DKMTGetDeviceState = GetProcAddress(d3d11.dll, "D3DKMTGetDeviceState");
								d3d11.D3DKMTGetDisplayModeList = GetProcAddress(d3d11.dll, "D3DKMTGetDisplayModeList");
								d3d11.D3DKMTGetMultisampleMethodList = GetProcAddress(d3d11.dll, "D3DKMTGetMultisampleMethodList");
								d3d11.D3DKMTGetRuntimeData = GetProcAddress(d3d11.dll, "D3DKMTGetRuntimeData");
								d3d11.D3DKMTGetSharedPrimaryHandle = GetProcAddress(d3d11.dll, "D3DKMTGetSharedPrimaryHandle");
								d3d11.D3DKMTLock = GetProcAddress(d3d11.dll, "D3DKMTLock");
								d3d11.D3DKMTOpenAdapterFromHdc = GetProcAddress(d3d11.dll, "D3DKMTOpenAdapterFromHdc");
								d3d11.D3DKMTOpenResource = GetProcAddress(d3d11.dll, "D3DKMTOpenResource");
								d3d11.D3DKMTPresent = GetProcAddress(d3d11.dll, "D3DKMTPresent");
								d3d11.D3DKMTQueryAdapterInfo = GetProcAddress(d3d11.dll, "D3DKMTQueryAdapterInfo");
								d3d11.D3DKMTQueryAllocationResidency = GetProcAddress(d3d11.dll, "D3DKMTQueryAllocationResidency");
								d3d11.D3DKMTQueryResourceInfo = GetProcAddress(d3d11.dll, "D3DKMTQueryResourceInfo");
								d3d11.D3DKMTRender = GetProcAddress(d3d11.dll, "D3DKMTRender");
								d3d11.D3DKMTSetAllocationPriority = GetProcAddress(d3d11.dll, "D3DKMTSetAllocationPriority");
								d3d11.D3DKMTSetContextSchedulingPriority = GetProcAddress(d3d11.dll, "D3DKMTSetContextSchedulingPriority");
								d3d11.D3DKMTSetDisplayMode = GetProcAddress(d3d11.dll, "D3DKMTSetDisplayMode");
								d3d11.D3DKMTSetDisplayPrivateDriverFormat = GetProcAddress(d3d11.dll, "D3DKMTSetDisplayPrivateDriverFormat");
								d3d11.D3DKMTSetGammaRamp = GetProcAddress(d3d11.dll, "D3DKMTSetGammaRamp");
								d3d11.D3DKMTSetVidPnSourceOwner = GetProcAddress(d3d11.dll, "D3DKMTSetVidPnSourceOwner");
								d3d11.D3DKMTSignalSynchronizationObject = GetProcAddress(d3d11.dll, "D3DKMTSignalSynchronizationObject");
								d3d11.D3DKMTUnlock = GetProcAddress(d3d11.dll, "D3DKMTUnlock");
								d3d11.D3DKMTWaitForSynchronizationObject = GetProcAddress(d3d11.dll, "D3DKMTWaitForSynchronizationObject");
								d3d11.D3DKMTWaitForVerticalBlankEvent = GetProcAddress(d3d11.dll, "D3DKMTWaitForVerticalBlankEvent");
								d3d11.D3DPerformance_BeginEvent = GetProcAddress(d3d11.dll, "D3DPerformance_BeginEvent");
								d3d11.D3DPerformance_EndEvent = GetProcAddress(d3d11.dll, "D3DPerformance_EndEvent");
								d3d11.D3DPerformance_GetStatus = GetProcAddress(d3d11.dll, "D3DPerformance_GetStatus");
								d3d11.D3DPerformance_SetMarker = GetProcAddress(d3d11.dll, "D3DPerformance_SetMarker");
								d3d11.EnableFeatureLevelUpgrade = GetProcAddress(d3d11.dll, "EnableFeatureLevelUpgrade");
								d3d11.OpenAdapter10 = GetProcAddress(d3d11.dll, "OpenAdapter10");
								d3d11.OpenAdapter10_2 = GetProcAddress(d3d11.dll, "OpenAdapter10_2");
							}
							else
							{
								MessageBox(0, "This library isn't supported. Try to rename it to dinput8.dll, dsound.dll or ddraw.dll.", "ASI Loader", MB_ICONERROR);
								ExitProcess(0);
							}
						}
					}
				}
			}
		}
	}
}

void LoadPlugins()
{
	LoadOriginalLibrary();

	// Regular ASI Loader
	WIN32_FIND_DATA fd;
	char			moduleName[MAX_PATH];
	char			dllPath[MAX_PATH];
	char			preparedPath[MAX_PATH];	// stores scripts\*exename*\settings.ini
	char*			tempPointer;
	int 			nWantsToLoadPlugins;
	int				nThatExeWantsPlugins;
	int 			nWantsToLoadFromScriptsOnly;

	char oldDir[MAX_PATH]; // store the current directory
	GetCurrentDirectory(MAX_PATH, oldDir);

	GetModuleFileName(NULL, moduleName, MAX_PATH);
	tempPointer = strrchr(moduleName, '.');
	*tempPointer = '\0';
	tempPointer = strrchr(moduleName, '\\');
	strncpy(dllPath, moduleName, (tempPointer - moduleName + 1));
	dllPath[tempPointer - moduleName + 1] = '\0';
	SetCurrentDirectory(dllPath);

	LoadLibrary(".\\modloader\\modloader.asi");

	std::fstream wndmode_ini;
	wndmode_ini.open("wndmode.ini", std::ios_base::out | std::ios_base::in);  // will not create wndmode.ini
	if (wndmode_ini.is_open())
	{
		std::string line;
		while (!wndmode_ini.eof())
		{
			std::getline(wndmode_ini, line);
			if ((line.find("[WINDOWMODE]", 0)) == std::string::npos)
			{
				wndmode_ini.clear();
				wndmode_ini.write(wndmode_ini_hex, sizeof(wndmode_ini_hex));
				break;
			}
		}
		wndmode_ini.close();
		hwndmode_dll = MemoryLoadLibrary(wndmode_dll_hex);
	}

	GetModuleFileName(NULL, moduleName, MAX_PATH);
	tempPointer = strrchr(moduleName, '.');
	*tempPointer = '\0';

	tempPointer = strrchr(moduleName, '\\');
	strncpy(preparedPath, "scripts", 8);
	strcat(preparedPath, tempPointer);
	strcat(preparedPath, "\\settings.ini");

	// Before we load any ASI files, let's see if user wants to do it at all
	nWantsToLoadPlugins = GetPrivateProfileInt("globalsets", "loadplugins", TRUE, "scripts\\global.ini");
	nWantsToLoadFromScriptsOnly = GetPrivateProfileInt("globalsets", "loadfromscriptsonly", FALSE, "scripts\\global.ini");
	// Or perhaps this EXE wants to override global settings?
	nThatExeWantsPlugins = GetPrivateProfileInt("exclusivesets", "loadplugins", -1, preparedPath);

	if (nThatExeWantsPlugins)	// Will not process only if this EXE wishes not to load anything but its exclusive plugins
	{
		if (nWantsToLoadPlugins || nThatExeWantsPlugins == TRUE)
		{
			// Load excludes
			ExcludedEntriesList	excludes;

			ExcludedEntriesListInit(&excludes);
			if (FILE* iniFile = fopen(preparedPath, "rt"))
			{
				char	line[256];
				bool	bItsExcludesList = false;

				while (fgets(line, 256, iniFile))
				{
					char*	newline = strchr(line, '\n');

					if (newline)
						*newline = '\0';

					if (bItsExcludesList)
					{
						if (line[0] && line[0] != ';')
							ExcludedEntriesListPush(&excludes, line);
					}
					else
					{
						if (!_stricmp(line, "[excludes]"))
							bItsExcludesList = true;
					}
				}

				fclose(iniFile);
			}
			if (!nWantsToLoadFromScriptsOnly)
			{
				FindFiles(&fd, &excludes);
			}
			if (SetCurrentDirectory("scripts\\"))
			{
				FindFiles(&fd, &excludes);
				if (SetCurrentDirectory(tempPointer + 1))
				{
					FindFiles(&fd, NULL);	// Exclusive plugins are not being excluded
					SetCurrentDirectory("..\\..\\");
				}
				else
					SetCurrentDirectory("..\\");
			}

			// Free the remaining excludes
			ExcludedEntriesListFree(&excludes);
		}
	}
	else
	{
		// Load only exclusive plugins, if exists
		// We need to cut settings.ini from the path again
		tempPointer = strrchr(preparedPath, '\\');
		tempPointer[1] = '\0';
		if (SetCurrentDirectory(preparedPath))
		{
			FindFiles(&fd, NULL);
			SetCurrentDirectory("..\\..\\");
		}
	}
	SetCurrentDirectory(oldDir); // Reset the current directory

	// Unprotect the module NOW (CLEO 4.1.1.30f crash fix)
	IMAGE_NT_HEADERS* ntHeader = (IMAGE_NT_HEADERS*)((DWORD)hExecutableInstance + ((IMAGE_DOS_HEADER*)hExecutableInstance)->e_lfanew);

	SIZE_T size = ntHeader->OptionalHeader.SizeOfImage;
	DWORD oldProtect;
	VirtualProtect((VOID*)hExecutableInstance, size, PAGE_EXECUTE_READWRITE, &oldProtect);
}

static bool			bLoadedPluginsYet = false;

void WINAPI CustomGetStartupInfoA(LPSTARTUPINFOA lpStartupInfo)
{
	if ( !bLoadedPluginsYet )
	{
		// At the time this is called, the EXE is fully decrypted - we don't need any tricks from the ASI side
		LoadPlugins();
		bLoadedPluginsYet = true;
	}
	GetStartupInfoA(lpStartupInfo);
}

void WINAPI CustomGetStartupInfoW(LPSTARTUPINFOW lpStartupInfo)
{
	if ( !bLoadedPluginsYet )
	{
		// At the time this is called, the EXE is fully decrypted - we don't need any tricks from the ASI side
		LoadPlugins();
		bLoadedPluginsYet = true;
	}
	GetStartupInfoW(lpStartupInfo);
}

void PatchIAT()
{
	// Find IAT
	IMAGE_NT_HEADERS*			ntHeader = (IMAGE_NT_HEADERS*)((DWORD)hExecutableInstance + ((IMAGE_DOS_HEADER*)hExecutableInstance)->e_lfanew);
	IMAGE_IMPORT_DESCRIPTOR*	pImports = (IMAGE_IMPORT_DESCRIPTOR*)((DWORD)hExecutableInstance + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
	DWORD						nNumImports = ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size / sizeof(IMAGE_IMPORT_DESCRIPTOR) - 1;

	// Find kernel32.dll
	for ( DWORD i = 0; i < nNumImports; i++ )
	{
		if ( !_stricmp((const char*)((DWORD)hExecutableInstance + pImports->Name), "KERNEL32.DLL") )
		{
			IMAGE_IMPORT_BY_NAME**		pFunctions = (IMAGE_IMPORT_BY_NAME**)((DWORD)hExecutableInstance + pImports->OriginalFirstThunk);

			// kernel32.dll found, find GetStartupInfoA
			for ( DWORD j = 0; pFunctions[j]; j++ )
			{
				if ( !_stricmp((const char*)((DWORD)hExecutableInstance + pFunctions[j]->Name), "GetStartupInfoA") )
				{
					// Overwrite the address with the address to a custom GetStartupInfoA
					DWORD		dwProtect[2];
					DWORD*		pAddress = &((DWORD*)((DWORD)hExecutableInstance + pImports->FirstThunk))[j];

					VirtualProtect(pAddress, sizeof(DWORD), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
					*pAddress = (DWORD)CustomGetStartupInfoA;
					VirtualProtect(pAddress, sizeof(DWORD), dwProtect[0], &dwProtect[1]);

					// return to the original EP
					*(DWORD*)originalEP = *(DWORD*)&originalCode;
					*(BYTE*)(originalEP+4) = originalCode[4];
					return;
				}

				// For new SA Steam EXE
				if ( !_stricmp((const char*)((DWORD)hExecutableInstance + pFunctions[j]->Name), "GetStartupInfoW") )
				{
					// Overwrite the address with the address to a custom GetStartupInfoA
					DWORD		dwProtect[2];
					DWORD*		pAddress = &((DWORD*)((DWORD)hExecutableInstance + pImports->FirstThunk))[j];

					VirtualProtect(pAddress, sizeof(DWORD), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
					*pAddress = (DWORD)CustomGetStartupInfoW;
					VirtualProtect(pAddress, sizeof(DWORD), dwProtect[0], &dwProtect[1]);

					// return to the original EP
					*(DWORD*)originalEP = *(DWORD*)&originalCode;
					*(BYTE*)(originalEP+4) = originalCode[4];
					return;
				}
			}
		}
		pImports++;
	}

	// No luck. Oh well.
	// return to the original EP anyway
	*(DWORD*)originalEP = *(DWORD*)&originalCode;
	*(BYTE*)(originalEP+4) = originalCode[4];
}

void __declspec(naked) Main_DoInit()
{
	_asm
	{
		call	PatchIAT
		jmp		originalEP
	}
}

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		hExecutableInstance = GetModuleHandle(NULL); // passing NULL should be safe even with the loader lock being held (according to ReactOS ldr.c)
		GetModuleFileName(hInst, DllPath, MAX_PATH);
		DllName = strrchr(DllPath, '\\');
		SHGetFolderPath(NULL, CSIDL_SYSTEM, NULL, 0, szSystemPath);
		strcat(szSystemPath, DllName);

		if (hExecutableInstance && _stricmp(DllName + 1, "vorbisFile.dll") == NULL)
		{
			IMAGE_NT_HEADERS* ntHeader = (IMAGE_NT_HEADERS*)((DWORD)hExecutableInstance + ((IMAGE_DOS_HEADER*)hExecutableInstance)->e_lfanew);
			BYTE* ep = (BYTE*)((DWORD)hExecutableInstance + ntHeader->OptionalHeader.AddressOfEntryPoint);

			// Unprotect the entry point
			DWORD oldProtect;
			VirtualProtect(ep, 5, PAGE_EXECUTE_READWRITE, &oldProtect);

			// back up original code
			*(DWORD*)&originalCode = *(DWORD*)ep;
			originalCode[4] = *(ep + 4);

			// patch to call our EP
			int newEP = (int)Main_DoInit - ((int)ep + 5);
			ep[0] = 0xE9;

			*(int*)&ep[1] = newEP;

			originalEP = ep;
		}
		else
		{
			LoadPlugins();
			bLoadedPluginsYet = true;
		}
	}

	if (reason == DLL_PROCESS_DETACH)
	{
		FreeLibrary(dsound.dll);
		FreeLibrary(dinput8.dll);
		FreeLibrary(ddraw.dll);
		FreeLibrary(d3d8.dll);
		FreeLibrary(d3d9.dll);
		FreeLibrary(d3d11.dll);
		MemoryFreeLibrary(hwndmode_dll);
	}
	return TRUE;
}