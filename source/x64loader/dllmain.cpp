#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <Shlobj.h>
#include <dsound.h>

HINSTANCE				hExecutableInstance, hDLLInstance;
TCHAR DllPath[MAX_PATH], *DllName, szSystemPath[MAX_PATH];

struct dsound_dll
{
	HMODULE dll;
	FARPROC DirectSoundCaptureCreate;
	FARPROC DirectSoundCaptureCreate8;
	FARPROC DirectSoundCaptureEnumerateA;
	FARPROC DirectSoundCaptureEnumerateW;
	FARPROC DirectSoundCreate;
	FARPROC DirectSoundCreate8;
	FARPROC DirectSoundEnumerateA;
	FARPROC DirectSoundEnumerateW;
	FARPROC DirectSoundFullDuplexCreate;
	FARPROC DllCanUnloadNow_dsound;
	FARPROC DllGetClassObject_dsound;
	FARPROC GetDeviceID;
} dsound;

typedef HRESULT(*fn_DirectSoundCaptureCreate)(LPGUID lpGUID, LPDIRECTSOUNDCAPTURE *lplpDSC, LPUNKNOWN pUnkOuter);
void _DirectSoundCaptureCreate() { (fn_DirectSoundCaptureCreate)dsound.DirectSoundCaptureCreate(); }

typedef HRESULT(*fn_DirectSoundCaptureCreate8)(LPCGUID lpcGUID, LPDIRECTSOUNDCAPTURE8 * lplpDSC, LPUNKNOWN pUnkOuter);
void _DirectSoundCaptureCreate8() { (fn_DirectSoundCaptureCreate8)dsound.DirectSoundCaptureCreate8(); }

typedef HRESULT(*fn_DirectSoundCaptureEnumerateA)(LPDSENUMCALLBACKA lpDSEnumCallback, LPVOID lpContext);
void _DirectSoundCaptureEnumerateA() { (fn_DirectSoundCaptureEnumerateA)dsound.DirectSoundCaptureEnumerateA(); }

typedef HRESULT(*fn_DirectSoundCaptureEnumerateW)(LPDSENUMCALLBACKW lpDSEnumCallback, LPVOID lpContext);
void _DirectSoundCaptureEnumerateW() { (fn_DirectSoundCaptureEnumerateW)dsound.DirectSoundCaptureEnumerateW(); }

typedef HRESULT(*fn_DirectSoundCreate)(LPCGUID lpcGUID, LPDIRECTSOUND* ppDS, IUnknown* pUnkOuter);
void _DirectSoundCreate() { (fn_DirectSoundCreate)dsound.DirectSoundCreate(); }

typedef HRESULT(*fn_DirectSoundCreate8)(LPCGUID lpcGUID, LPDIRECTSOUND8* ppDS, IUnknown* pUnkOuter);
void _DirectSoundCreate8() { (fn_DirectSoundCreate8)dsound.DirectSoundCreate8(); }

typedef HRESULT(*fn_DirectSoundEnumerateA)(LPDSENUMCALLBACKA lpDSEnumCallback, LPVOID lpContext);
void _DirectSoundEnumerateA() { (fn_DirectSoundEnumerateA)dsound.DirectSoundEnumerateA(); }

typedef HRESULT(*fn_DirectSoundEnumerateW)(LPDSENUMCALLBACKW lpDSEnumCallback, LPVOID lpContext);
void _DirectSoundEnumerateW() { (fn_DirectSoundEnumerateW)dsound.DirectSoundEnumerateW(); }

typedef HRESULT(*fn_DirectSoundFullDuplexCreate)(const GUID* capture_dev, const GUID* render_dev, const DSCBUFFERDESC* cbufdesc, const DSBUFFERDESC* bufdesc, HWND  hwnd, DWORD level, IDirectSoundFullDuplex**  dsfd, IDirectSoundCaptureBuffer8** dscb8, IDirectSoundBuffer8** dsb8, IUnknown* outer_unk);
void _DirectSoundFullDuplexCreate() { (fn_DirectSoundFullDuplexCreate)dsound.DirectSoundFullDuplexCreate(); }

typedef HRESULT(*fn_DllCanUnloadNow_dsound)();
void _DllCanUnloadNow_dsound() { (fn_DllCanUnloadNow_dsound)dsound.DllCanUnloadNow_dsound(); }

typedef HRESULT(*fn_DllGetClassObject_dsound)(REFCLSID rclsid, REFIID   riid, LPVOID   *ppv);
void _DllGetClassObject_dsound() { (fn_DllGetClassObject_dsound)dsound.DllGetClassObject_dsound(); }

typedef HRESULT(*fn_GetDeviceID)(LPCGUID pGuidSrc, LPGUID pGuidDest);
void _GetDeviceID() { (fn_GetDeviceID)dsound.GetDeviceID(); }

struct ExcludedEntry {
	char*			entry;
	ExcludedEntry*	prev;
	ExcludedEntry*	next;
};

struct ExcludedEntriesList {
	ExcludedEntry*	first;
	ExcludedEntry*	last;
};

void ExcludedEntriesListInit(ExcludedEntriesList* list)
{
	list->first = NULL;
	list->last = NULL;
}

void ExcludedEntriesListPush(ExcludedEntriesList* list, const char* entryName)
{
	ExcludedEntry* 	newEntry = (ExcludedEntry*)malloc(sizeof(ExcludedEntry));
	size_t 			length = strlen(entryName) + 1;
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
	char			preparedPath[128];	// stores scripts\*exename*\settings.ini
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

			if (SetCurrentDirectory("plugins\\"))
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