#include "dllmain.h"
#if !X64
#include <d3d8to9\source\d3d8to9.hpp>
extern "C" Direct3D8 *WINAPI Direct3DCreate8(UINT SDKVersion);
#endif

HMODULE hm;
bool bLoadedPluginsYet, bOriginalLibraryLoaded;
char iniPath[MAX_PATH];

enum Kernel32ExportsNames
{
    eGetStartupInfoA,
    eGetStartupInfoW,
    eGetModuleHandleA,
    eGetModuleHandleW,
    eGetProcAddress,
    eGetShortPathNameA,
    eFindNextFileA,
    eFindNextFileW,
    eLoadLibraryA,
    eLoadLibraryW,
    eFreeLibrary,
    eCreateEventA,
    eCreateEventW,
    eGetSystemInfo,

    Kernel32ExportsNamesCount
};

enum Kernel32ExportsData
{
    IATPtr,
    ProcAddress,

    Kernel32ExportsDataCount
};

size_t Kernel32Data[Kernel32ExportsNamesCount][Kernel32ExportsDataCount];

#if !X64
#define IDR_VBHKD   101
#define IDR_WNDMODE 103
#define IDR_WNDWINI 104
#endif

void LoadOriginalLibrary()
{
    bOriginalLibraryLoaded = true;

    char SelfPath[MAX_PATH];
    char szSystemPath[MAX_PATH];
    GetModuleFileName(hm, SelfPath, MAX_PATH);
    auto SelfName = strrchr(SelfPath, '\\');
    SHGetFolderPath(NULL, CSIDL_SYSTEM, NULL, 0, szSystemPath);
    strcat_s(szSystemPath, SelfName);

#if !X64
    if (_stricmp(SelfName + 1, "vorbisFile.dll") == NULL)
    {
        HRSRC hResource = FindResource(hm, MAKEINTRESOURCE(IDR_VBHKD), RT_RCDATA);
        if (hResource)
        {
            HGLOBAL hLoadedResource = LoadResource(hm, hResource);
            if (hLoadedResource)
            {
                LPVOID pLockedResource = LockResource(hLoadedResource);
                if (pLockedResource)
                {
                    size_t dwResourceSize = SizeofResource(hm, hResource);
                    if (0 != dwResourceSize)
                    {
                        vorbisfile.LoadOriginalLibrary(MemoryLoadLibrary((const void*)pLockedResource, dwResourceSize));

                        // Unprotect the module NOW (CLEO 4.1.1.30f crash fix)
                        auto hExecutableInstance = (size_t)GetModuleHandle(NULL);
                        IMAGE_NT_HEADERS* ntHeader = (IMAGE_NT_HEADERS*)(hExecutableInstance + ((IMAGE_DOS_HEADER*)hExecutableInstance)->e_lfanew);
                        SIZE_T size = ntHeader->OptionalHeader.SizeOfImage;
                        DWORD oldProtect;
                        VirtualProtect((VOID*)hExecutableInstance, size, PAGE_EXECUTE_READWRITE, &oldProtect);
                    }
                }
            }
        }
    }
    else if (_stricmp(SelfName + 1, "dsound.dll") == NULL) {
        dsound.LoadOriginalLibrary(LoadLibrary(szSystemPath));
    }
    else if (_stricmp(SelfName + 1, "dinput8.dll") == NULL) {
        dinput8.LoadOriginalLibrary(LoadLibrary(szSystemPath));
    }
    else if (_stricmp(SelfName + 1, "ddraw.dll") == NULL) {
        ddraw.LoadOriginalLibrary(LoadLibrary(szSystemPath));
    }
    else if (_stricmp(SelfName + 1, "d3d8.dll") == NULL) {
        d3d8.LoadOriginalLibrary(LoadLibrary(szSystemPath));
        if (GetPrivateProfileInt("globalsets", "used3d8to9", FALSE, iniPath))
            d3d8.Direct3DCreate8 = (FARPROC)Direct3DCreate8;
    }
    else if (_stricmp(SelfName + 1, "d3d9.dll") == NULL) {
        d3d9.LoadOriginalLibrary(LoadLibrary(szSystemPath));
    }
    else if (_stricmp(SelfName + 1, "d3d11.dll") == NULL) {
        d3d11.LoadOriginalLibrary(LoadLibrary(szSystemPath));
    }
    else if (_stricmp(SelfName + 1, "winmmbase.dll") == NULL) {
        winmmbase.LoadOriginalLibrary(LoadLibrary(szSystemPath));
    }
    else if (_stricmp(SelfName + 1, "msacm32.dll") == NULL) {
        msacm32.LoadOriginalLibrary(LoadLibrary(szSystemPath));
    }
    else if (_stricmp(SelfName + 1, "dinput.dll") == NULL) {
        dinput.LoadOriginalLibrary(LoadLibrary(szSystemPath));
    }
    else if (_stricmp(SelfName + 1, "msvfw32.dll") == NULL) {
        msvfw32.LoadOriginalLibrary(LoadLibrary(szSystemPath));
    }
    else if (_stricmp(SelfName + 1, "xlive.dll") == NULL) {
        // Unprotect image - make .text and .rdata section writeable
        // get load address of the exe
        size_t dwLoadOffset = (size_t)GetModuleHandle(NULL);
        BYTE * pImageBase = reinterpret_cast<BYTE *>(dwLoadOffset);
        PIMAGE_DOS_HEADER   pDosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(dwLoadOffset);
        PIMAGE_NT_HEADERS   pNtHeader = reinterpret_cast<PIMAGE_NT_HEADERS>(pImageBase + pDosHeader->e_lfanew);
        PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNtHeader);

        for (int iSection = 0; iSection < pNtHeader->FileHeader.NumberOfSections; ++iSection, ++pSection) {
            char * pszSectionName = reinterpret_cast<char *>(pSection->Name);
            if (!strcmp(pszSectionName, ".text") || !strcmp(pszSectionName, ".rdata")) {
                DWORD dwPhysSize = (pSection->Misc.VirtualSize + 4095) & ~4095;
                DWORD	oldProtect;
                DWORD   newProtect = (pSection->Characteristics & IMAGE_SCN_MEM_EXECUTE) ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
                if (!VirtualProtect(reinterpret_cast <VOID *>(dwLoadOffset + pSection->VirtualAddress), dwPhysSize, newProtect, &oldProtect)) {
                    ExitProcess(0);
                }
            }
        }
    }
    else
    {
        MessageBox(0, "This library isn't supported. Try to rename it to d3d8.dll, d3d9.dll, d3d11.dll, winmmbase.dll, msacm32.dll, dinput.dll, dinput8.dll, dsound.dll, vorbisFile.dll, msvfw32.dll, xlive.dll or ddraw.dll.", "ASI Loader", MB_ICONERROR);
        ExitProcess(0);
    }
#else
        if (_stricmp(SelfName + 1, "dsound.dll") == NULL) {
            dsound.LoadOriginalLibrary(LoadLibrary(szSystemPath));
        }
        else if (_stricmp(SelfName + 1, "dinput8.dll") == NULL) {
            dinput8.LoadOriginalLibrary(LoadLibrary(szSystemPath));
        }
        else
        {
            MessageBox(0, "This library isn't supported. Try to rename it to dsound.dll or dinput8.dll.", "ASI Loader", MB_ICONERROR);
            ExitProcess(0);
        }
#endif
}

#if !X64
void Direct3D8DisableMaximizedWindowedModeShim()
{
    auto nDirect3D8DisableMaximizedWindowedModeShim = GetPrivateProfileInt("globalsets", "Direct3D8DisableMaximizedWindowedModeShim", FALSE, iniPath);
    if (nDirect3D8DisableMaximizedWindowedModeShim)
    {
        HMODULE pd3d8 = NULL;
        if (d3d8.dll)
        {
            pd3d8 = d3d8.dll;
        }
        else
        {
            pd3d8 = LoadLibrary("d3d8.dll");
            if (!pd3d8)
            {
                TCHAR szSystemPath[MAX_PATH];
                SHGetFolderPath(NULL, CSIDL_SYSTEM, NULL, 0, szSystemPath);
                strcat_s(szSystemPath, "\\d3d8.dll");
                pd3d8 = LoadLibrary(szSystemPath);
            }
        }

        if (pd3d8)
        {
            auto addr = (uintptr_t)GetProcAddress(pd3d8, "Direct3D8EnableMaximizedWindowedModeShim");
            if (addr)
            {
                DWORD Protect;
                VirtualProtect((LPVOID)(addr + 6), 4, PAGE_EXECUTE_READWRITE, &Protect);
                *(uint32_t*)(addr + 6) = 0;
                *(uint32_t*)(*(uint32_t*)(addr + 2)) = 0;
                VirtualProtect((LPVOID)(addr + 6), 4, Protect, &Protect);
            }
        }
    }
}
#endif

void FindFiles(WIN32_FIND_DATA* fd)
{
    char dir[MAX_PATH] = { 0 };
    GetCurrentDirectory(MAX_PATH, dir);

    HANDLE asiFile = FindFirstFile("*.asi", fd);
    if (asiFile != INVALID_HANDLE_VALUE)
    {
        do {
            if (!(fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                auto pos = strlen(fd->cFileName);

                if (fd->cFileName[pos - 4] == '.' &&
                   (fd->cFileName[pos - 3] == 'a' || fd->cFileName[pos - 3] == 'A') &&
                   (fd->cFileName[pos - 2] == 's' || fd->cFileName[pos - 2] == 'S') &&
                   (fd->cFileName[pos - 1] == 'i' || fd->cFileName[pos - 1] == 'I'))
                {
                    char path[MAX_PATH] = { 0 };
                    strcat(path, dir);
                    strcat(path, "\\");
                    strcat(path, fd->cFileName);

                    auto h = LoadLibrary(path);
                    SetCurrentDirectory(dir); //in case asi switched it

                    if (h == NULL)
                    {
                        auto e = GetLastError();
                        if (e != ERROR_DLL_INIT_FAILED) // in case dllmain returns false
                        {
                            char msg[200] = { 0 }; char err[15];
                            strcat(msg, "Unable to load ");
                            strcat(msg, fd->cFileName);
                            strcat(msg, ". Error: ");
                            sprintf(err, "%d", e);
                            strcat(msg, err);
                            MessageBox(0, msg, "ASI Loader", MB_ICONERROR);
                        }
                    }
                    else
                    {
                        auto procedure = (void(*)())GetProcAddress(h, "InitializeASI");

                        if (procedure != NULL)
                        {
                            procedure();
                        }
                    }
                }
            }
        } while (FindNextFile(asiFile, fd));
        FindClose(asiFile);
    }
}

void LoadPlugins()
{
    char oldDir[MAX_PATH]; // store the current directory
    GetCurrentDirectory(MAX_PATH, oldDir);

    char selfPath[MAX_PATH];
    GetModuleFileName(hm, selfPath, MAX_PATH);
    *strrchr(selfPath, '\\') = '\0';
    SetCurrentDirectory(selfPath);

#if !X64
    LoadLibrary(".\\modloader\\modupdater.asi");
    LoadLibrary(".\\modloader\\modloader.asi");

    std::fstream wndmode_ini;
    wndmode_ini.open("wndmode.ini", std::ios_base::out | std::ios_base::in | std::ios_base::binary);
    if (wndmode_ini.is_open())
    {
        wndmode_ini.seekg(0, wndmode_ini.end);
        bool bIsEmpty = !wndmode_ini.tellg();
        wndmode_ini.seekg(wndmode_ini.tellg(), wndmode_ini.beg);

        if (bIsEmpty)
        {
            HRSRC hResource = FindResource(hm, MAKEINTRESOURCE(IDR_WNDWINI), RT_RCDATA);
            if (hResource)
            {
                HGLOBAL hLoadedResource = LoadResource(hm, hResource);
                if (hLoadedResource)
                {
                    LPVOID pLockedResource = LockResource(hLoadedResource);
                    if (pLockedResource)
                    {
                        DWORD dwResourceSize = SizeofResource(hm, hResource);
                        if (0 != dwResourceSize)
                        {
                            wndmode_ini.write((char*)pLockedResource, dwResourceSize);
                        }
                    }
                }
            }
        }

        wndmode_ini.close();

        HRSRC hResource = FindResource(hm, MAKEINTRESOURCE(IDR_WNDMODE), RT_RCDATA);
        if (hResource)
        {
            HGLOBAL hLoadedResource = LoadResource(hm, hResource);
            if (hLoadedResource)
            {
                LPVOID pLockedResource = LockResource(hLoadedResource);
                if (pLockedResource)
                {
                    DWORD dwResourceSize = SizeofResource(hm, hResource);
                    if (0 != dwResourceSize)
                    {
                        MemoryLoadLibrary((const void*)pLockedResource, dwResourceSize);
                    }
                }
            }
        }
    }
#endif

    auto nWantsToLoadPlugins = GetPrivateProfileInt("globalsets", "loadplugins", TRUE, iniPath);
    auto nWantsToLoadFromScriptsOnly = GetPrivateProfileInt("globalsets", "loadfromscriptsonly", FALSE, iniPath);

    if (nWantsToLoadPlugins)
    {
        WIN32_FIND_DATA fd;
        if (!nWantsToLoadFromScriptsOnly)
            FindFiles(&fd);

        SetCurrentDirectory(selfPath);

        if (SetCurrentDirectory("scripts\\"))
            FindFiles(&fd);

        SetCurrentDirectory(selfPath);

        if (SetCurrentDirectory("plugins\\"))
            FindFiles(&fd);
    }

    SetCurrentDirectory(oldDir); // Reset the current directory
}

void LoadEverything()
{
    if (!bLoadedPluginsYet)
    {
        if (!bOriginalLibraryLoaded)
            LoadOriginalLibrary();
#if !X64
        Direct3D8DisableMaximizedWindowedModeShim();
#endif
        LoadPlugins();
        bLoadedPluginsYet = true;
    }
}

void LoadPluginsAndRestoreIAT()
{
    LoadEverything();

    for (size_t i = 0; i < Kernel32ExportsNamesCount; i++)
    {
        if (Kernel32Data[i][IATPtr] && Kernel32Data[i][ProcAddress])
        {
            auto ptr = (size_t*)Kernel32Data[i][IATPtr];
            DWORD dwProtect[2];
            VirtualProtect(ptr, sizeof(size_t), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
            *ptr = Kernel32Data[i][ProcAddress];
            VirtualProtect(ptr, sizeof(size_t), dwProtect[0], &dwProtect[1]);
        }
    }
}

void WINAPI CustomGetStartupInfoA(LPSTARTUPINFOA lpStartupInfo)
{
    LoadPluginsAndRestoreIAT();
    return GetStartupInfoA(lpStartupInfo);
}

void WINAPI CustomGetStartupInfoW(LPSTARTUPINFOW lpStartupInfo)
{
    LoadPluginsAndRestoreIAT();
    return GetStartupInfoW(lpStartupInfo);
}

HMODULE WINAPI CustomGetModuleHandleA(LPCSTR lpModuleName)
{
    LoadPluginsAndRestoreIAT();
    return GetModuleHandleA(lpModuleName);
}

HMODULE WINAPI CustomGetModuleHandleW(LPCWSTR lpModuleName)
{
    LoadPluginsAndRestoreIAT();
    return GetModuleHandleW(lpModuleName);
}

FARPROC WINAPI CustomGetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
    LoadPluginsAndRestoreIAT();
    return GetProcAddress(hModule, lpProcName);
}

DWORD WINAPI CustomGetShortPathNameA(LPCSTR lpszLongPath, LPSTR lpszShortPath, DWORD cchBuffer)
{
    LoadPluginsAndRestoreIAT();
    return GetShortPathNameA(lpszLongPath, lpszShortPath, cchBuffer);
}

BOOL WINAPI CustomFindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData)
{
    LoadPluginsAndRestoreIAT();
    return FindNextFileA(hFindFile, lpFindFileData);
}

BOOL WINAPI CustomFindNextFileW(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData)
{
    LoadPluginsAndRestoreIAT();
    return FindNextFileW(hFindFile, lpFindFileData);
}

HMODULE WINAPI CustomLoadLibraryA(LPCSTR lpLibFileName)
{
    if (!bOriginalLibraryLoaded)
        LoadOriginalLibrary();

    return LoadLibraryA(lpLibFileName);
}

HMODULE WINAPI CustomLoadLibraryW(LPCWSTR lpLibFileName)
{
    if (!bOriginalLibraryLoaded)
        LoadOriginalLibrary();

    return LoadLibraryW(lpLibFileName);
}

BOOL WINAPI CustomFreeLibrary(HMODULE hLibModule)
{
    if (hLibModule != hm)
        return FreeLibrary(hLibModule);
    else
        return !NULL;
}

HANDLE WINAPI CustomCreateEventA(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCTSTR lpName)
{
    LoadPluginsAndRestoreIAT();
    return CreateEventA(lpEventAttributes, bManualReset, bInitialState, lpName);
}

HANDLE WINAPI CustomCreateEventW(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCWSTR lpName)
{
    LoadPluginsAndRestoreIAT();
    return CreateEventW(lpEventAttributes, bManualReset, bInitialState, lpName);
}

void WINAPI CustomGetSystemInfo(LPSYSTEM_INFO lpSystemInfo)
{
    LoadPluginsAndRestoreIAT();
    return GetSystemInfo(lpSystemInfo);
}

void HookKernel32IAT()
{
    auto hExecutableInstance = (size_t)GetModuleHandle(NULL);
    IMAGE_NT_HEADERS*           ntHeader = (IMAGE_NT_HEADERS*)(hExecutableInstance + ((IMAGE_DOS_HEADER*)hExecutableInstance)->e_lfanew);
    IMAGE_IMPORT_DESCRIPTOR*    pImports = (IMAGE_IMPORT_DESCRIPTOR*)(hExecutableInstance + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    size_t                      nNumImports = ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size / sizeof(IMAGE_IMPORT_DESCRIPTOR) - 1;

    Kernel32Data[eGetStartupInfoA]  [ProcAddress] = (size_t)GetProcAddress(GetModuleHandle("KERNEL32.DLL"), "GetStartupInfoA");
    Kernel32Data[eGetStartupInfoW]  [ProcAddress] = (size_t)GetProcAddress(GetModuleHandle("KERNEL32.DLL"), "GetStartupInfoW");
    Kernel32Data[eGetModuleHandleA] [ProcAddress] = (size_t)GetProcAddress(GetModuleHandle("KERNEL32.DLL"), "GetModuleHandleA");
    Kernel32Data[eGetModuleHandleW] [ProcAddress] = (size_t)GetProcAddress(GetModuleHandle("KERNEL32.DLL"), "GetModuleHandleW");
    Kernel32Data[eGetProcAddress]   [ProcAddress] = (size_t)GetProcAddress(GetModuleHandle("KERNEL32.DLL"), "GetProcAddress");
    Kernel32Data[eGetShortPathNameA][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle("KERNEL32.DLL"), "GetShortPathNameA");
    Kernel32Data[eFindNextFileA]    [ProcAddress] = (size_t)GetProcAddress(GetModuleHandle("KERNEL32.DLL"), "FindNextFileA");
    Kernel32Data[eFindNextFileW]    [ProcAddress] = (size_t)GetProcAddress(GetModuleHandle("KERNEL32.DLL"), "FindNextFileW");
    Kernel32Data[eLoadLibraryA]     [ProcAddress] = (size_t)GetProcAddress(GetModuleHandle("KERNEL32.DLL"), "LoadLibraryA");
    Kernel32Data[eLoadLibraryW]     [ProcAddress] = (size_t)GetProcAddress(GetModuleHandle("KERNEL32.DLL"), "LoadLibraryW");
    Kernel32Data[eFreeLibrary]      [ProcAddress] = (size_t)GetProcAddress(GetModuleHandle("KERNEL32.DLL"), "FreeLibrary");
    Kernel32Data[eCreateEventA]     [ProcAddress] = (size_t)GetProcAddress(GetModuleHandle("KERNEL32.DLL"), "CreateEventA");
    Kernel32Data[eCreateEventW]     [ProcAddress] = (size_t)GetProcAddress(GetModuleHandle("KERNEL32.DLL"), "CreateEventW");
    Kernel32Data[eGetSystemInfo]    [ProcAddress] = (size_t)GetProcAddress(GetModuleHandle("KERNEL32.DLL"), "GetSystemInfo");

    auto PatchIAT = [&nNumImports, &hExecutableInstance, &pImports](size_t start, size_t end, size_t exe_end)
    {
        for (size_t i = 0; i < nNumImports; i++)
        {
            if (hExecutableInstance + (pImports + i)->FirstThunk > start && !(end && hExecutableInstance + (pImports + i)->FirstThunk > end))
                end = hExecutableInstance + (pImports + i)->FirstThunk;
        }

        if (!end) { end = start + 0x100; }
        if (end > exe_end) //for very broken exes
        { 
            start = hExecutableInstance;
            end = exe_end; 
        }

        for (auto i = start; i < end; i += sizeof(size_t))
        {
            DWORD dwProtect[2];
            VirtualProtect((size_t*)i, sizeof(size_t), PAGE_EXECUTE_READWRITE, &dwProtect[0]);

            auto ptr = *(size_t*)i;

            if (ptr == Kernel32Data[eGetStartupInfoA][ProcAddress])
            {
                Kernel32Data[eGetStartupInfoA][IATPtr] = i;
                *(size_t*)i = (size_t)CustomGetStartupInfoA;
            }
            else if (ptr == Kernel32Data[eGetStartupInfoW][ProcAddress])
            {
                Kernel32Data[eGetStartupInfoW][IATPtr] = i;
                *(size_t*)i = (size_t)CustomGetStartupInfoW;
            }
            else if (ptr == Kernel32Data[eGetModuleHandleA][ProcAddress])
            {
                Kernel32Data[eGetModuleHandleA][IATPtr] = i;
                *(size_t*)i = (size_t)CustomGetModuleHandleA;
            }
            else if (ptr == Kernel32Data[eGetModuleHandleW][ProcAddress])
            {
                Kernel32Data[eGetModuleHandleW][IATPtr] = i;
                *(size_t*)i = (size_t)CustomGetModuleHandleW;
            }
            else if (ptr == Kernel32Data[eGetProcAddress][ProcAddress])
            {
                Kernel32Data[eGetProcAddress][IATPtr] = i;
                *(size_t*)i = (size_t)CustomGetProcAddress;
            }
            else if (ptr == Kernel32Data[eGetShortPathNameA][ProcAddress])
            {
                Kernel32Data[eGetShortPathNameA][IATPtr] = i;
                *(size_t*)i = (size_t)CustomGetShortPathNameA;
            }
            else if (ptr == Kernel32Data[eFindNextFileA][ProcAddress])
            {
                Kernel32Data[eFindNextFileA][IATPtr] = i;
                *(size_t*)i = (size_t)CustomFindNextFileA;
            }
            else if (ptr == Kernel32Data[eFindNextFileW][ProcAddress])
            {
                Kernel32Data[eFindNextFileW][IATPtr] = i;
                *(size_t*)i = (size_t)CustomFindNextFileW;
            }
            else if (ptr == Kernel32Data[eLoadLibraryA][ProcAddress])
            {
                Kernel32Data[eLoadLibraryA][IATPtr] = i;
                *(size_t*)i = (size_t)CustomLoadLibraryA;
            }
            else if (ptr == Kernel32Data[eLoadLibraryW][ProcAddress])
            {
                Kernel32Data[eLoadLibraryW][IATPtr] = i;
                *(size_t*)i = (size_t)CustomLoadLibraryW;
            }
            else if (ptr == Kernel32Data[eFreeLibrary][ProcAddress])
            {
                Kernel32Data[eFreeLibrary][IATPtr] = i;
                *(size_t*)i = (size_t)CustomFreeLibrary;
            }
            else if (ptr == Kernel32Data[eCreateEventA][ProcAddress])
            {
                Kernel32Data[eCreateEventA][IATPtr] = i;
                *(size_t*)i = (size_t)CustomCreateEventA;
            }
            else if (ptr == Kernel32Data[eCreateEventW][ProcAddress])
            {
                Kernel32Data[eCreateEventW][IATPtr] = i;
                *(size_t*)i = (size_t)CustomCreateEventW;
            }
            else if (ptr == Kernel32Data[eGetSystemInfo][ProcAddress])
            {
                Kernel32Data[eGetSystemInfo][IATPtr] = i;
                *(size_t*)i = (size_t)CustomGetSystemInfo;
            }

            VirtualProtect((size_t*)i, sizeof(size_t), dwProtect[0], &dwProtect[1]);
        }
    };

    static auto getSection = [](const PIMAGE_NT_HEADERS nt_headers, unsigned section) -> PIMAGE_SECTION_HEADER
    {
        return reinterpret_cast<PIMAGE_SECTION_HEADER>(
            (UCHAR*)nt_headers->OptionalHeader.DataDirectory +
            nt_headers->OptionalHeader.NumberOfRvaAndSizes * sizeof(IMAGE_DATA_DIRECTORY) +
            section * sizeof(IMAGE_SECTION_HEADER));
    };
 
    auto sec = getSection(ntHeader, ntHeader->FileHeader.NumberOfSections - 1);
    auto secSize = max(sec->SizeOfRawData, sec->Misc.VirtualSize);
    auto hExecutableInstance_end = hExecutableInstance + max(sec->PointerToRawData, sec->VirtualAddress) + secSize;

    char SelfPath[MAX_PATH];
    GetModuleFileName(hm, SelfPath, MAX_PATH);
    auto SelfName = strrchr(SelfPath, '\\') + 1;
    
    // Find kernel32.dll
    for (size_t i = 0; i < nNumImports; i++)
    {
        if ((size_t)(hExecutableInstance + (pImports + i)->Name) < hExecutableInstance_end)
        {
            if (!_stricmp((const char*)(hExecutableInstance + (pImports + i)->Name), "KERNEL32.DLL"))
                PatchIAT(hExecutableInstance + (pImports + i)->FirstThunk, 0, hExecutableInstance_end);

            //Checking for ordinals
            if (!_stricmp((const char*)(hExecutableInstance + (pImports + i)->Name), SelfName))
            {
                PIMAGE_THUNK_DATA thunk = (PIMAGE_THUNK_DATA)(hExecutableInstance + (pImports + i)->OriginalFirstThunk);
                size_t j = 0;
                while (thunk->u1.Function)
                {
                    if (thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)
                    {
                        PIMAGE_IMPORT_BY_NAME import = (PIMAGE_IMPORT_BY_NAME)(hExecutableInstance + thunk->u1.AddressOfData);
                        void** p = (void**)(hExecutableInstance + (pImports + i)->FirstThunk);
                        if (!_stricmp(SelfName, "DSOUND.DLL"))
                        {
                            DWORD Protect;
                            VirtualProtect(&p[j], 4, PAGE_EXECUTE_READWRITE, &Protect);

                            const enum edsound
                            {
                                DirectSoundCaptureCreate = 6,
                                DirectSoundCaptureCreate8 = 12,
                                DirectSoundCaptureEnumerateA = 7,
                                DirectSoundCaptureEnumerateW = 8,
                                DirectSoundCreate = 1,
                                DirectSoundCreate8 = 11,
                                DirectSoundEnumerateA = 2,
                                DirectSoundEnumerateW = 3,
                                DirectSoundFullDuplexCreate = 10,
                                GetDeviceID = 9
                            };

                            switch (IMAGE_ORDINAL(thunk->u1.Ordinal))
                            {
                            case edsound::DirectSoundCaptureCreate:
                                p[j] = _DirectSoundCaptureCreate;
                                break;
                            case edsound::DirectSoundCaptureCreate8:
                                p[j] = _DirectSoundCaptureCreate8;
                                break;
                            case edsound::DirectSoundCaptureEnumerateA:
                                p[j] = _DirectSoundCaptureEnumerateA;
                                break;
                            case edsound::DirectSoundCaptureEnumerateW:
                                p[j] = _DirectSoundCaptureEnumerateW;
                                break;
                            case edsound::DirectSoundCreate:
                                p[j] = _DirectSoundCreate;
                                break;
                            case edsound::DirectSoundCreate8:
                                p[j] = _DirectSoundCreate8;
                                break;
                            case edsound::DirectSoundEnumerateA:
                                p[j] = _DirectSoundEnumerateA;
                                break;
                            case edsound::DirectSoundEnumerateW:
                                p[j] = _DirectSoundEnumerateW;
                                break;
                            case edsound::DirectSoundFullDuplexCreate:
                                p[j] = _DirectSoundFullDuplexCreate;
                                break;
                            case edsound::GetDeviceID:
                                p[j] = _GetDeviceID;
                                break;
                            default:
                                break;
                            }
                        }
                        else if (!_stricmp(SelfName, "DINPUT8.DLL"))
                        {
                            DWORD Protect;
                            VirtualProtect(&p[j], 4, PAGE_EXECUTE_READWRITE, &Protect);

                            if ((IMAGE_ORDINAL(thunk->u1.Ordinal)) == 1)
                                p[j] = _DirectInput8Create;
                        }
                    }
                    ++thunk;
                }
            }
        }
    }
}

void Init()
{
    GetModuleFileName(hm, iniPath, MAX_PATH);
    *strrchr(iniPath, '\\') = '\0';
    strcat_s(iniPath, "\\scripts\\global.ini");

    auto nForceEPHook = GetPrivateProfileInt("globalsets", "forceentrypointhook", TRUE, iniPath);

    if (GetModuleHandle(NULL) && nForceEPHook != FALSE)
    {
        HookKernel32IAT();
    }
    else
    {
        LoadEverything();
    }
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID /*lpReserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        hm = hModule;
        Init();
    }
    return TRUE;
}
