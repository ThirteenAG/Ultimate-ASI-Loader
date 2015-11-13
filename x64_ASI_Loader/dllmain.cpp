#include "structs.h"

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
		dsound.DllCanUnloadNow_dsound = GetProcAddress(dsound.dll, "DllCanUnloadNow");
		dsound.DllGetClassObject_dsound = GetProcAddress(dsound.dll, "DllGetClassObject");
		dsound.GetDeviceID = GetProcAddress(dsound.dll, "GetDeviceID");
	}
	else
	{
		MessageBox(0, "This library isn't supported. dsound.dll is the only one supported.", "x64 ASI Loader", MB_ICONERROR);
		ExitProcess(0);
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
	int				nUseD3D8to9;

	char oldDir[MAX_PATH]; // store the current directory
	GetCurrentDirectory(MAX_PATH, oldDir);

	GetModuleFileName(NULL, moduleName, MAX_PATH);
	tempPointer = strrchr(moduleName, '.');
	*tempPointer = '\0';
	tempPointer = strrchr(moduleName, '\\');
	strncpy(dllPath, moduleName, (tempPointer - moduleName + 1));
	dllPath[tempPointer - moduleName + 1] = '\0';
	SetCurrentDirectory(dllPath);

	/*LoadLibrary(".\\modloader\\modloader.asi");

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
		hwndmode = MemoryLoadLibrary(wndmode_dll_hex);
	}*/

	GetModuleFileName(NULL, moduleName, MAX_PATH);
	tempPointer = strrchr(moduleName, '.');
	*tempPointer = '\0';

	tempPointer = strrchr(moduleName, '\\');
	strncpy(preparedPath, "scripts", 8);
	strcat(preparedPath, tempPointer);
	strcat(preparedPath, "\\settings.ini");

	/*nUseD3D8to9 = GetPrivateProfileInt("globalsets", "used3d8to9", FALSE, "scripts\\global.ini");
	if (nUseD3D8to9 && _stricmp(DllName + 1, "d3d8.dll") == NULL)
	{
		d3d8to9 = MemoryLoadLibrary(d3d8to9hex);
		d3d8.DebugSetMute_d3d8 = MemoryGetProcAddress(d3d8to9, "DebugSetMute_d3d8");
		d3d8.Direct3DCreate8 = MemoryGetProcAddress(d3d8to9, "Direct3DCreate8");
		d3d8.ValidatePixelShader = MemoryGetProcAddress(d3d8to9, "ValidatePixelShader");
		d3d8.ValidateVertexShader = MemoryGetProcAddress(d3d8to9, "ValidateVertexShader");
	}*/

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

		LoadPlugins();
	}

	if (reason == DLL_PROCESS_DETACH)
	{
		FreeLibrary(dsound.dll);
	}
	return TRUE;
}