// GTA: SA ASI Loader 1.2
// Written by Silent
// Based on ASI Loader by Stanislav "listener" Golovin
// Initialization part made by NTAuthority

#include <windows.h>
#include <iostream>
#include "vorbisFile.h"

BYTE 					originalCode[5];
BYTE* 					originalEP = 0;
extern HINSTANCE		hExecutableInstance;

//=============================================================================
// vorbisfile connector
extern "C" {
struct ov_callbacks {
  size_t (*read_func)  (void* ptr, size_t size, size_t nmemb, void* datasource);
  int    (*seek_func)  (void* datasource, long long offset, int whence);
  int    (*close_func) (void* datasource);
  long   (*tell_func)  (void* datasource);
};

void* __ov_open_callbacks;
__declspec(dllexport, naked) int ov_open_callbacks(void* datasource, void* vf, char* initial, long ibytes, ov_callbacks callbacks)
{ _asm jmp __ov_open_callbacks  }

void* __ov_clear;
__declspec(dllexport, naked) int ov_clear(void* vf)
{ _asm jmp __ov_clear }

void* __ov_time_total;
__declspec(dllexport, naked) double ov_time_total (void* vf, int i)
{ _asm jmp __ov_time_total }

void* __ov_time_tell;
__declspec(dllexport, naked) double ov_time_tell(void* vf)
{ _asm jmp __ov_time_tell }

void* __ov_read;
__declspec(dllexport, naked) long ov_read(void* vf, char* buffer, int length, int bigendianp, int word, int sgned, int* bitstream)
{ _asm jmp __ov_read }

void* __ov_info;
__declspec(dllexport, naked) void* ov_info(void* vf, int link)
{ _asm jmp __ov_info }

void* __ov_time_seek;
__declspec(dllexport, naked) int ov_time_seek(void* vf, double pos)
{ _asm jmp __ov_time_seek }

void* __ov_time_seek_page;
__declspec(dllexport, naked) int ov_time_seek_page(void* vf, double pos)
{ _asm jmp __ov_time_seek_page }

}

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

void LoadPlugins()
{
    HMODULE vorbisHooked = LoadLibrary ("vorbishooked");
    if ( vorbisHooked )
    {
        __ov_open_callbacks = GetProcAddress (vorbisHooked, "ov_open_callbacks");
        __ov_clear = GetProcAddress (vorbisHooked, "ov_clear");
        __ov_time_total = GetProcAddress (vorbisHooked, "ov_time_total");
        __ov_time_tell = GetProcAddress (vorbisHooked, "ov_time_tell");
        __ov_read = GetProcAddress (vorbisHooked, "ov_read");
        __ov_info = GetProcAddress (vorbisHooked, "ov_info");
        __ov_time_seek = GetProcAddress (vorbisHooked, "ov_time_seek");
        __ov_time_seek_page = GetProcAddress (vorbisHooked, "ov_time_seek_page");

        // Regular ASI Loader
        WIN32_FIND_DATA fd;
        char			moduleName[MAX_PATH];
        char			preparedPath[128];	// stores scripts\*exename*\settings.ini
        char*			tempPointer;
        int 			nWantsToLoadPlugins;
        int				nThatExeWantsPlugins;

        GetModuleFileName(NULL, moduleName, MAX_PATH);
        tempPointer = strrchr(moduleName, '.');
        *tempPointer  = '\0';

        tempPointer = strrchr(moduleName, '\\');
        strncpy(preparedPath, "scripts", 8);
        strcat(preparedPath, tempPointer);
        strcat(preparedPath, "\\settings.ini");

        // Before we load any ASI files, let's see if user wants to do it at all
        nWantsToLoadPlugins = GetPrivateProfileInt("globalsets", "loadplugins", TRUE, "scripts\\global.ini");
        // Or perhaps this EXE wants to override global settings?
        nThatExeWantsPlugins = GetPrivateProfileInt("exclusivesets", "loadplugins", -1, preparedPath);

        if ( nThatExeWantsPlugins )	// Will not process only if this EXE wishes not to load anything but its exclusive plugins
        {
            if ( nWantsToLoadPlugins || nThatExeWantsPlugins == TRUE )
            {
                // Load excludes
                ExcludedEntriesList	excludes;

                ExcludedEntriesListInit(&excludes);
                if ( FILE* iniFile = fopen(preparedPath, "rt") )
                {
                    char	line[256];
                    bool	bItsExcludesList = false;

                    while ( fgets(line, 256, iniFile) )
                    {
                        char*	newline = strchr(line, '\n');

                        if ( newline )
                            *newline = '\0';

                        if ( bItsExcludesList )
                        {
                            if ( line[0] && line[0] != ';' )
                                ExcludedEntriesListPush(&excludes, line);
                        }
                        else
                        {
                            if ( !_stricmp(line, "[excludes]") )
                                bItsExcludesList = true;
                        }
                    }

                    fclose(iniFile);
                }
                FindFiles(&fd, &excludes);
                if ( SetCurrentDirectory("scripts\\") )
                {
                    FindFiles(&fd, &excludes);
                    if ( SetCurrentDirectory(tempPointer + 1) )
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
            if ( SetCurrentDirectory(preparedPath) )
            {
                FindFiles(&fd, NULL);
                SetCurrentDirectory("..\\..\\");
            }
        }
    }

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

void VorbisFile()
{
    hExecutableInstance = GetModuleHandle(NULL); // passing NULL should be safe even with the loader lock being held (according to ReactOS ldr.c)

    if (hExecutableInstance)
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
}