#include "dllmain.h"
#if !X64
#include <d3d8to9\source\d3d8to9.hpp>
extern "C" Direct3D8 *WINAPI Direct3DCreate8(UINT SDKVersion);
#endif

HMODULE hm;
bool bLoadedPluginsYet;
char iniPath[MAX_PATH];

enum Kernel32ExportsNames
{
    eGetStartupInfoA,
    eGetStartupInfoW,
    eGetModuleHandleA,
    eGetModuleHandleW,
    eGetProcAddress,
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
        if (GetPrivateProfileInt("globalsets", "used3d8to9", TRUE, iniPath))
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
        MessageBox(0, "This library isn't supported. Try to rename it to d3d8.dll, d3d9.dll, d3d11.dll, winmmbase.dll, msacm32.dll, dinput8.dll, dsound.dll, vorbisFile.dll, xlive.dll or ddraw.dll.", "ASI Loader", MB_ICONERROR);
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
    HANDLE asiFile = FindFirstFile("*.asi", fd);
    if (asiFile != INVALID_HANDLE_VALUE)
    {
        do {
            if (!(fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                LoadLibrary(fd->cFileName);
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
    *strrchr(iniPath, '\\') = '\0';
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

        SetCurrentDirectory(oldDir);

        if (SetCurrentDirectory("scripts\\"))
            FindFiles(&fd);

        SetCurrentDirectory(oldDir);

        if (SetCurrentDirectory("plugins\\"))
            FindFiles(&fd);
    }

    SetCurrentDirectory(oldDir); // Reset the current directory
}

void LoadEverything()
{
    if (!bLoadedPluginsYet)
    {
        LoadOriginalLibrary();
#if !X64
        Direct3D8DisableMaximizedWindowedModeShim();
#endif
        LoadPlugins();
        bLoadedPluginsYet = true;
    }
}

void LoadPluginsAndRestoreIAT(Kernel32ExportsNames e)
{
    LoadEverything();

    auto ptr = (size_t*)Kernel32Data[e][IATPtr];
    DWORD dwProtect[2];
    VirtualProtect(ptr, sizeof(size_t), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
    *ptr = Kernel32Data[e][ProcAddress];
    VirtualProtect(ptr, sizeof(size_t), dwProtect[0], &dwProtect[1]);
}

void WINAPI CustomGetStartupInfoA(LPSTARTUPINFOA lpStartupInfo)
{
    LoadPluginsAndRestoreIAT(eGetStartupInfoA);
    return GetStartupInfoA(lpStartupInfo);
}

void WINAPI CustomGetStartupInfoW(LPSTARTUPINFOW lpStartupInfo)
{
    LoadPluginsAndRestoreIAT(eGetStartupInfoW);
    return GetStartupInfoW(lpStartupInfo);
}

HMODULE WINAPI CustomGetModuleHandleA(LPCSTR lpModuleName)
{
    LoadPluginsAndRestoreIAT(eGetModuleHandleA);
    return GetModuleHandleA(lpModuleName);
}

HMODULE WINAPI CustomGetModuleHandleW(LPCWSTR lpModuleName)
{
    LoadPluginsAndRestoreIAT(eGetModuleHandleW);
    return GetModuleHandleW(lpModuleName);
}

FARPROC WINAPI CustomGetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
    LoadPluginsAndRestoreIAT(eGetProcAddress);
    return GetProcAddress(hModule, lpProcName);
}

void HookKernel32IAT()
{
    auto hExecutableInstance = (size_t)GetModuleHandle(NULL);

    IMAGE_NT_HEADERS*			ntHeader = (IMAGE_NT_HEADERS*)(hExecutableInstance + ((IMAGE_DOS_HEADER*)hExecutableInstance)->e_lfanew);
    IMAGE_IMPORT_DESCRIPTOR*	pImports = (IMAGE_IMPORT_DESCRIPTOR*)(hExecutableInstance + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    size_t						nNumImports = ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size / sizeof(IMAGE_IMPORT_DESCRIPTOR) - 1;

    // Find kernel32.dll
    for (size_t i = 0; i < nNumImports; i++)
    {
        if (!_stricmp((const char*)(hExecutableInstance + pImports->Name), "KERNEL32.DLL"))
        {
            auto start = hExecutableInstance + pImports->FirstThunk;
            auto end = (pImports + 1) ? (hExecutableInstance + (pImports + 1)->FirstThunk) : start + 0x100;

            if (start && end)
            {
                Kernel32Data[eGetStartupInfoA] [ProcAddress] = (size_t)GetProcAddress(GetModuleHandle("KERNEL32.DLL"), "GetStartupInfoA");
                Kernel32Data[eGetStartupInfoW] [ProcAddress] = (size_t)GetProcAddress(GetModuleHandle("KERNEL32.DLL"), "GetStartupInfoW");
                Kernel32Data[eGetModuleHandleA][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle("KERNEL32.DLL"), "GetModuleHandleA");
                Kernel32Data[eGetModuleHandleW][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle("KERNEL32.DLL"), "GetModuleHandleW");
                Kernel32Data[eGetProcAddress]  [ProcAddress] = (size_t)GetProcAddress(GetModuleHandle("KERNEL32.DLL"), "GetProcAddress");

                for (auto i = start; i < end; i += 4)
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

                    VirtualProtect((size_t*)i, sizeof(size_t), dwProtect[0], &dwProtect[1]);
                }
            }
            return;
        }
        pImports++;
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
