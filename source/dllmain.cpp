#include "dllmain.h"
#include "exception.hpp"
#include <initguid.h>

#if !X64
#include <d3d8to9\source\d3d8to9.hpp>
extern "C" Direct3D8 *WINAPI Direct3DCreate8(UINT SDKVersion);
#endif

HMODULE hm;
std::vector<std::wstring> iniPaths;

bool iequals(std::wstring_view s1, std::wstring_view s2)
{
    std::wstring str1(std::move(s1));
    std::wstring str2(std::move(s2));
    std::transform(str1.begin(), str1.end(), str1.begin(), [](wchar_t c) { return ::towlower(c); });
    std::transform(str2.begin(), str2.end(), str2.begin(), [](wchar_t c) { return ::towlower(c); });
    return (str1 == str2);
}

std::wstring to_wstring(std::string_view cstr)
{
    std::string str(std::move(cstr));
    auto charsReturned = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(charsReturned, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], charsReturned);
    return wstrTo;
}

std::wstring SHGetKnownFolderPath(REFKNOWNFOLDERID rfid, DWORD dwFlags, HANDLE hToken)
{
    std::wstring r;
    WCHAR* szSystemPath = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(rfid, dwFlags, hToken, &szSystemPath)))
    {
        r = szSystemPath;
    }
    CoTaskMemFree(szSystemPath);
    return r;
};

HMODULE LoadLibraryW(const std::wstring& lpLibFileName)
{
    return LoadLibraryW(lpLibFileName.c_str());
}

std::wstring GetCurrentDirectoryW()
{
    static constexpr auto INITIAL_BUFFER_SIZE = MAX_PATH;
    static constexpr auto MAX_ITERATIONS = 7;
    std::wstring ret;
    auto bufferSize = INITIAL_BUFFER_SIZE;
    for (size_t iterations = 0; iterations < MAX_ITERATIONS; ++iterations)
    {
        ret.resize(bufferSize);
        auto charsReturned = GetCurrentDirectoryW(bufferSize, &ret[0]);
        if (charsReturned < ret.length())
        {
            ret.resize(charsReturned);
            return ret;
        }
        else
        {
            bufferSize *= 2;
        }
    }
    return L"";
}

UINT GetPrivateProfileIntW(LPCWSTR lpAppName, LPCWSTR lpKeyName, INT nDefault, const std::vector<std::wstring>& fileNames)
{
    for (const auto& file : fileNames)
    {
        nDefault = GetPrivateProfileIntW(lpAppName, lpKeyName, nDefault, file.c_str());
    }
    return nDefault;
}

std::wstring GetSelfName()
{
    const std::wstring moduleFileName = GetModuleFileNameW(hm);
    return moduleFileName.substr(moduleFileName.find_last_of(L"/\\") + 1);
}

template<typename T, typename... Args>
void GetSections(T&& h, Args... args)
{
    const std::set< std::string_view, std::less<> > s = { args... };
    size_t dwLoadOffset = (size_t)GetModuleHandle(NULL);
    BYTE* pImageBase = reinterpret_cast<BYTE *>(dwLoadOffset);
    PIMAGE_DOS_HEADER   pDosHeader = reinterpret_cast<PIMAGE_DOS_HEADER>(dwLoadOffset);
    PIMAGE_NT_HEADERS   pNtHeader = reinterpret_cast<PIMAGE_NT_HEADERS>(pImageBase + pDosHeader->e_lfanew);
    PIMAGE_SECTION_HEADER pSection = IMAGE_FIRST_SECTION(pNtHeader);
    for (int iSection = 0; iSection < pNtHeader->FileHeader.NumberOfSections; ++iSection, ++pSection)
    {
        auto pszSectionName = reinterpret_cast<const char*>(pSection->Name);
        if (s.find(pszSectionName) != s.end())
        {
            DWORD dwPhysSize = (pSection->Misc.VirtualSize + 4095) & ~4095;
            std::forward<T>(h)(pSection, dwLoadOffset, dwPhysSize);
        }
    }
}

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
    eInterlockedCompareExchange,
    eSleep,

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
#define IDR_VORBISF    101
#define IDR_WNDMODE    103
#define IDR_WNDWINI    104
#define IDR_BINK00018X 105
#define IDR_BINK00019U 106
#define IDR_BINK00199W 107
#define IDR_BINK01994I 108
#endif

static LONG OriginalLibraryLoaded = 0;
void LoadOriginalLibrary()
{
    if (_InterlockedCompareExchange(&OriginalLibraryLoaded, 1, 0) != 0) return;

    auto szSelfName = GetSelfName();
    auto szSystemPath = SHGetKnownFolderPath(FOLDERID_System, 0, nullptr) + L'\\' + szSelfName;
    auto szLocalPath = GetModuleFileNameW(hm); szLocalPath = szLocalPath.substr(0, szLocalPath.find_last_of(L"/\\") + 1);

#if !X64
    if (iequals(szSelfName, L"vorbisFile.dll"))
    {
        szLocalPath += L"vorbisHooked.dll";
        if (std::filesystem::exists(szLocalPath))
        {
            vorbisfile.LoadOriginalLibrary(LoadLibraryW(szLocalPath), false);
        }
        else
        {
            HRSRC hResource = FindResource(hm, MAKEINTRESOURCE(IDR_VORBISF), RT_RCDATA);
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
                            vorbisfile.LoadOriginalLibrary(MemoryLoadLibrary((const void*)pLockedResource, dwResourceSize), true);

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
    }
    else if (iequals(szSelfName, L"dsound.dll"))
    {
        dsound.LoadOriginalLibrary(LoadLibraryW(szSystemPath));
    }
    else if (iequals(szSelfName, L"dinput8.dll"))
    {
        dinput8.LoadOriginalLibrary(LoadLibraryW(szSystemPath));
    }
    else if (iequals(szSelfName, L"ddraw.dll"))
    {
        ddraw.LoadOriginalLibrary(LoadLibraryW(szSystemPath));
    }
    else if (iequals(szSelfName, L"d3d8.dll"))
    {
        d3d8.LoadOriginalLibrary(LoadLibraryW(szSystemPath));
        if (GetPrivateProfileIntW(L"globalsets", L"used3d8to9", FALSE, iniPaths))
            d3d8.Direct3DCreate8 = (FARPROC)Direct3DCreate8;
    }
    else if (iequals(szSelfName, L"d3d9.dll"))
    {
        d3d9.LoadOriginalLibrary(LoadLibraryW(szSystemPath));
    }
    else if (iequals(szSelfName, L"d3d11.dll"))
    {
        d3d11.LoadOriginalLibrary(LoadLibraryW(szSystemPath));
    }
    else if (iequals(szSelfName, L"winmm.dll"))
    {
        winmm.LoadOriginalLibrary(LoadLibraryW(szSystemPath));
    }
    else if (iequals(szSelfName, L"msacm32.dll"))
    {
        msacm32.LoadOriginalLibrary(LoadLibraryW(szSystemPath));
    }
    else if (iequals(szSelfName, L"dinput.dll"))
    {
        dinput.LoadOriginalLibrary(LoadLibraryW(szSystemPath));
    }
    else if (iequals(szSelfName, L"msvfw32.dll"))
    {
        msvfw32.LoadOriginalLibrary(LoadLibraryW(szSystemPath));
    }
    else if (iequals(szSelfName, L"wininet.dll"))
    {
        wininet.LoadOriginalLibrary(LoadLibraryW(szSystemPath));
    }
    else if (iequals(szSelfName, L"version.dll"))
    {
        version.LoadOriginalLibrary(LoadLibraryW(szSystemPath));
    }
    else if (iequals(szSelfName, L"binkw32.dll"))
    {
        szLocalPath += L"binkw32Hooked.dll";
        if (std::filesystem::exists(szLocalPath))
        {
            binkw32.LoadOriginalLibrary(LoadLibraryW(szLocalPath), false);
        }
        else
        {
            SYSTEMTIME t = {};
            ModuleList dlls;
            dlls.Enumerate(ModuleList::SearchLocation::LocalOnly);
            for (auto& e : dlls.m_moduleList)
            {
                auto hInstance = (size_t)std::get<HMODULE>(e);
                IMAGE_NT_HEADERS* ntHeader = (IMAGE_NT_HEADERS*)(hInstance + ((IMAGE_DOS_HEADER*)hInstance)->e_lfanew);
                SYSTEMTIME stUTC, stLocal;
                LONGLONG ll;
                FILETIME ft;
                ll = Int32x32To64(ntHeader->FileHeader.TimeDateStamp, 10000000) + 116444736000000000;
                ft.dwLowDateTime = (DWORD)ll;
                ft.dwHighDateTime = ll >> 32;
                FileTimeToSystemTime(&ft, &stUTC);
                SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);
                if (t.wYear == 0 || t.wYear > stLocal.wYear)
                    t = stLocal;
            }

            HRSRC hResource = FindResource(hm, MAKEINTRESOURCE((t.wYear <= 2007) ? IDR_BINK00018X : (t.wYear <= 2010) ? IDR_BINK00019U : (t.wYear <= 2012) ? IDR_BINK00199W : IDR_BINK01994I), RT_RCDATA);
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
                            binkw32.LoadOriginalLibrary(MemoryLoadLibrary((const void*)pLockedResource, dwResourceSize), true);
                        }
                    }
                }
            }
        }
    }
    else if (iequals(szSelfName, L"xlive.dll"))
    {
        // Unprotect image - make .text and .rdata section writeable
        GetSections([](PIMAGE_SECTION_HEADER pSection, size_t dwLoadOffset, DWORD dwPhysSize)
        {
            DWORD oldProtect = 0;
            DWORD newProtect = (pSection->Characteristics & IMAGE_SCN_MEM_EXECUTE) ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
            if (!VirtualProtect(reinterpret_cast<VOID*>(dwLoadOffset + pSection->VirtualAddress), dwPhysSize, newProtect, &oldProtect))
            {
                ExitProcess(0);
            }
        }, ".text", ".rdata");
    }
    else
    {
        MessageBox(0, TEXT("This library isn't supported. Try to rename it to d3d8.dll, d3d9.dll, d3d11.dll, winmm.dll, wininet.dll, version.dll, \
            msacm32.dll, dinput.dll, dinput8.dll, dsound.dll, vorbisFile.dll, msvfw32.dll, xlive.dll or ddraw.dll."), TEXT("ASI Loader"), MB_ICONERROR);
        ExitProcess(0);
    }
#else
    if (iequals(szSelfName, L"dsound.dll"))
    {
        dsound.LoadOriginalLibrary(LoadLibraryW(szSystemPath));
    }
    else if (iequals(szSelfName, L"dinput8.dll"))
    {
        dinput8.LoadOriginalLibrary(LoadLibraryW(szSystemPath));
    }
    else if (iequals(szSelfName, L"wininet.dll"))
    {
        wininet.LoadOriginalLibrary(LoadLibraryW(szSystemPath));
    }
    else if (iequals(szSelfName, L"version.dll"))
    {
        version.LoadOriginalLibrary(LoadLibraryW(szSystemPath));
    }
    else
    {
        MessageBox(0, TEXT("This library isn't supported. Try to rename it to dsound.dll, dinput8.dll, wininet.dll or version.dll."), TEXT("ASI Loader"), MB_ICONERROR);
        ExitProcess(0);
    }
#endif
}

#if !X64
void Direct3D8DisableMaximizedWindowedModeShim()
{
    auto nDirect3D8DisableMaximizedWindowedModeShim = GetPrivateProfileIntW(L"globalsets", L"Direct3D8DisableMaximizedWindowedModeShim", FALSE, iniPaths);
    if (nDirect3D8DisableMaximizedWindowedModeShim)
    {
        HMODULE pd3d8 = NULL;
        if (d3d8.dll)
        {
            pd3d8 = d3d8.dll;
        }
        else
        {
            pd3d8 = LoadLibraryW(L"d3d8.dll");
            if (!pd3d8)
            {
                pd3d8 = LoadLibraryW(SHGetKnownFolderPath(FOLDERID_System, 0, nullptr) + L'\\' + L"d3d8.dll");
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

void FindFiles(WIN32_FIND_DATAW* fd)
{
    auto dir = GetCurrentDirectoryW();

    HANDLE asiFile = FindFirstFileW(L"*.asi", fd);
    if (asiFile != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (!(fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                auto pos = wcslen(fd->cFileName);

                if (fd->cFileName[pos - 4] == '.' &&
                        (fd->cFileName[pos - 3] == 'a' || fd->cFileName[pos - 3] == 'A') &&
                        (fd->cFileName[pos - 2] == 's' || fd->cFileName[pos - 2] == 'S') &&
                        (fd->cFileName[pos - 1] == 'i' || fd->cFileName[pos - 1] == 'I'))
                {
                    auto path = dir + L'\\' + fd->cFileName;

                    if (GetModuleHandle(path.c_str()) == NULL)
                    {
                        auto h = LoadLibraryW(path);
                        SetCurrentDirectoryW(dir.c_str()); //in case asi switched it

                        if (h == NULL)
                        {
                            auto e = GetLastError();
                            if (e != ERROR_DLL_INIT_FAILED) // in case dllmain returns false
                            {
                                std::wstring msg = L"Unable to load " + std::wstring(fd->cFileName) + L". Error: " + std::to_wstring(e);
                                MessageBoxW(0, msg.c_str(), L"ASI Loader", MB_ICONERROR);
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
            }
        }
        while (FindNextFileW(asiFile, fd));
        FindClose(asiFile);
    }
}

void LoadPlugins()
{
    auto oldDir = GetCurrentDirectoryW(); // store the current directory

    auto szSelfPath = GetModuleFileNameW(hm).substr(0, GetModuleFileNameW(hm).find_last_of(L"/\\") + 1);
    SetCurrentDirectoryW(szSelfPath.c_str());

#if !X64
    LoadLibraryW(L".\\modloader\\modupdater.asi");
    LoadLibraryW(L".\\modloader\\modloader.asi");

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

    auto nWantsToLoadPlugins = GetPrivateProfileIntW(L"globalsets", L"loadplugins", TRUE, iniPaths);
    auto nWantsToLoadFromScriptsOnly = GetPrivateProfileIntW(L"globalsets", L"loadfromscriptsonly", FALSE, iniPaths);

    if (nWantsToLoadPlugins)
    {
        WIN32_FIND_DATAW fd;
        if (!nWantsToLoadFromScriptsOnly)
            FindFiles(&fd);

        SetCurrentDirectoryW(szSelfPath.c_str());

        if (SetCurrentDirectoryW(L"scripts\\"))
            FindFiles(&fd);

        SetCurrentDirectoryW(szSelfPath.c_str());

        if (SetCurrentDirectoryW(L"plugins\\"))
            FindFiles(&fd);
    }

    SetCurrentDirectoryW(oldDir.c_str()); // Reset the current directory
}

static LONG LoadedPluginsYet = 0;
void LoadEverything()
{
    if (_InterlockedCompareExchange(&LoadedPluginsYet, 1, 0) != 0) return;

    LoadOriginalLibrary();
#if !X64
    Direct3D8DisableMaximizedWindowedModeShim();
#endif
    LoadPlugins();
}

static LONG RestoredOnce = 0;
void LoadPluginsAndRestoreIAT(uintptr_t retaddr)
{
    bool calledFromBind = false;

    //steam drm check
    GetSections([&](PIMAGE_SECTION_HEADER pSection, size_t dwLoadOffset, DWORD dwPhysSize)
    {
        auto dwStart = static_cast<uintptr_t>(dwLoadOffset + pSection->VirtualAddress);
        auto dwEnd = dwStart + dwPhysSize;
        if (retaddr >= dwStart && retaddr <= dwEnd)
            calledFromBind = true;
    }, ".bind");

    if (calledFromBind) return;

    if (_InterlockedCompareExchange(&RestoredOnce, 1, 0) != 0) return;

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
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress());
    return GetStartupInfoA(lpStartupInfo);
}

void WINAPI CustomGetStartupInfoW(LPSTARTUPINFOW lpStartupInfo)
{
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress());
    return GetStartupInfoW(lpStartupInfo);
}

HMODULE WINAPI CustomGetModuleHandleA(LPCSTR lpModuleName)
{
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress());
    return GetModuleHandleA(lpModuleName);
}

HMODULE WINAPI CustomGetModuleHandleW(LPCWSTR lpModuleName)
{
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress());
    return GetModuleHandleW(lpModuleName);
}

FARPROC WINAPI CustomGetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress());
    return GetProcAddress(hModule, lpProcName);
}

DWORD WINAPI CustomGetShortPathNameA(LPCSTR lpszLongPath, LPSTR lpszShortPath, DWORD cchBuffer)
{
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress());
    return GetShortPathNameA(lpszLongPath, lpszShortPath, cchBuffer);
}

BOOL WINAPI CustomFindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData)
{
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress());
    return FindNextFileA(hFindFile, lpFindFileData);
}

BOOL WINAPI CustomFindNextFileW(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData)
{
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress());
    return FindNextFileW(hFindFile, lpFindFileData);
}

HMODULE WINAPI CustomLoadLibraryA(LPCSTR lpLibFileName)
{
    LoadOriginalLibrary();

    return LoadLibraryA(lpLibFileName);
}

HMODULE WINAPI CustomLoadLibraryW(LPCWSTR lpLibFileName)
{
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

HANDLE WINAPI CustomCreateEventA(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCSTR lpName)
{
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress());
    return CreateEventA(lpEventAttributes, bManualReset, bInitialState, lpName);
}

HANDLE WINAPI CustomCreateEventW(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCWSTR lpName)
{
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress());
    return CreateEventW(lpEventAttributes, bManualReset, bInitialState, lpName);
}

void WINAPI CustomGetSystemInfo(LPSYSTEM_INFO lpSystemInfo)
{
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress());
    return GetSystemInfo(lpSystemInfo);
}

LONG WINAPI CustomInterlockedCompareExchange(LONG volatile* Destination, LONG ExChange, LONG Comperand)
{
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress());
    return _InterlockedCompareExchange(Destination, ExChange, Comperand);
}

void WINAPI CustomSleep(DWORD dwMilliseconds)
{
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress());
    return Sleep(dwMilliseconds);
}

DEFINE_GUID(CLSID_DirectInput, 0x25E609E0, 0xB259, 0x11CF, 0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00);
DEFINE_GUID(CLSID_DirectInput8, 0x25E609E4, 0xB259, 0x11CF, 0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00);
DEFINE_GUID(CLSID_WinInet, 0xC39EE728, 0xD419, 0x4BD4, 0xA3, 0xEF, 0xED, 0xA0, 0x59, 0xDB, 0xD9, 0x35);
HRESULT WINAPI CustomCoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID *ppv)
{
    HRESULT hr = REGDB_E_KEYMISSING;
    HMODULE hDll = NULL;

    if (rclsid == CLSID_DirectInput8)
        hDll = ::LoadLibrary(L"dinput8.dll");
    else if (rclsid == CLSID_DirectInput)
        hDll = ::LoadLibrary(L"dinput.dll");
    else if (rclsid == CLSID_WinInet)
        hDll = ::LoadLibrary(L"wininet.dll");

    if (hDll == NULL)
        return ::CoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);

    typedef HRESULT(__stdcall *pDllGetClassObject)(IN REFCLSID rclsid, IN REFIID riid, OUT LPVOID FAR* ppv);

    pDllGetClassObject GetClassObject = (pDllGetClassObject)::GetProcAddress(hDll, "DllGetClassObject");
    if (GetClassObject == NULL)
    {
        ::FreeLibrary(hDll);
        return hr;
    }

    IClassFactory *pIFactory;

    hr = GetClassObject(rclsid, IID_IClassFactory, (LPVOID *)&pIFactory);

    if (!SUCCEEDED(hr))
        return hr;

    hr = pIFactory->CreateInstance(pUnkOuter, riid, ppv);
    pIFactory->Release();

    return hr;
}

bool HookKernel32IAT(HMODULE mod, bool exe)
{
    auto hExecutableInstance = (size_t)mod;
    IMAGE_NT_HEADERS*           ntHeader = (IMAGE_NT_HEADERS*)(hExecutableInstance + ((IMAGE_DOS_HEADER*)hExecutableInstance)->e_lfanew);
    IMAGE_IMPORT_DESCRIPTOR*    pImports = (IMAGE_IMPORT_DESCRIPTOR*)(hExecutableInstance + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    size_t                      nNumImports = ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size / sizeof(IMAGE_IMPORT_DESCRIPTOR) - 1;

    if (exe)
    {
        Kernel32Data[eGetStartupInfoA][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "GetStartupInfoA");
        Kernel32Data[eGetStartupInfoW][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "GetStartupInfoW");
        Kernel32Data[eGetModuleHandleA][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "GetModuleHandleA");
        Kernel32Data[eGetModuleHandleW][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "GetModuleHandleW");
        Kernel32Data[eGetProcAddress][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "GetProcAddress");
        Kernel32Data[eGetShortPathNameA][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "GetShortPathNameA");
        Kernel32Data[eFindNextFileA][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "FindNextFileA");
        Kernel32Data[eFindNextFileW][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "FindNextFileW");
        Kernel32Data[eLoadLibraryA][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "LoadLibraryA");
        Kernel32Data[eLoadLibraryW][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "LoadLibraryW");
        Kernel32Data[eFreeLibrary][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "FreeLibrary");
        Kernel32Data[eCreateEventA][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "CreateEventA");
        Kernel32Data[eCreateEventW][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "CreateEventW");
        Kernel32Data[eGetSystemInfo][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "GetSystemInfo");
        Kernel32Data[eInterlockedCompareExchange][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "InterlockedCompareExchange");
        Kernel32Data[eSleep][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "Sleep");
    }

    uint32_t matchedImports = 0;

    auto PatchIAT = [&](size_t start, size_t end, size_t exe_end)
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
            if (!ptr)
                continue;

            if (ptr == Kernel32Data[eGetStartupInfoA][ProcAddress])
            {
                if (exe) Kernel32Data[eGetStartupInfoA][IATPtr] = i;
                *(size_t*)i = (size_t)CustomGetStartupInfoA;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eGetStartupInfoW][ProcAddress])
            {
                if (exe) Kernel32Data[eGetStartupInfoW][IATPtr] = i;
                *(size_t*)i = (size_t)CustomGetStartupInfoW;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eGetModuleHandleA][ProcAddress])
            {
                if (exe) Kernel32Data[eGetModuleHandleA][IATPtr] = i;
                *(size_t*)i = (size_t)CustomGetModuleHandleA;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eGetModuleHandleW][ProcAddress])
            {
                if (exe) Kernel32Data[eGetModuleHandleW][IATPtr] = i;
                *(size_t*)i = (size_t)CustomGetModuleHandleW;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eGetProcAddress][ProcAddress])
            {
                if (exe) Kernel32Data[eGetProcAddress][IATPtr] = i;
                *(size_t*)i = (size_t)CustomGetProcAddress;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eGetShortPathNameA][ProcAddress])
            {
                if (exe) Kernel32Data[eGetShortPathNameA][IATPtr] = i;
                *(size_t*)i = (size_t)CustomGetShortPathNameA;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eFindNextFileA][ProcAddress])
            {
                if (exe) Kernel32Data[eFindNextFileA][IATPtr] = i;
                *(size_t*)i = (size_t)CustomFindNextFileA;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eFindNextFileW][ProcAddress])
            {
                if (exe) Kernel32Data[eFindNextFileW][IATPtr] = i;
                *(size_t*)i = (size_t)CustomFindNextFileW;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eLoadLibraryA][ProcAddress])
            {
                if (exe) Kernel32Data[eLoadLibraryA][IATPtr] = i;
                *(size_t*)i = (size_t)CustomLoadLibraryA;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eLoadLibraryW][ProcAddress])
            {
                if (exe) Kernel32Data[eLoadLibraryW][IATPtr] = i;
                *(size_t*)i = (size_t)CustomLoadLibraryW;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eFreeLibrary][ProcAddress])
            {
                if (exe) Kernel32Data[eFreeLibrary][IATPtr] = i;
                *(size_t*)i = (size_t)CustomFreeLibrary;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eCreateEventA][ProcAddress])
            {
                if (exe) Kernel32Data[eCreateEventA][IATPtr] = i;
                *(size_t*)i = (size_t)CustomCreateEventA;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eCreateEventW][ProcAddress])
            {
                if (exe) Kernel32Data[eCreateEventW][IATPtr] = i;
                *(size_t*)i = (size_t)CustomCreateEventW;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eGetSystemInfo][ProcAddress])
            {
                if (exe) Kernel32Data[eGetSystemInfo][IATPtr] = i;
                *(size_t*)i = (size_t)CustomGetSystemInfo;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eInterlockedCompareExchange][ProcAddress])
            {
                if (exe) Kernel32Data[eInterlockedCompareExchange][IATPtr] = i;
                *(size_t*)i = (size_t)CustomInterlockedCompareExchange;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eSleep][ProcAddress])
            {
                if (exe) Kernel32Data[eSleep][IATPtr] = i;
                *(size_t*)i = (size_t)CustomSleep;
                matchedImports++;
            }

            VirtualProtect((size_t*)i, sizeof(size_t), dwProtect[0], &dwProtect[1]);
        }
    };

    auto PatchCoCreateInstance = [&](size_t start, size_t end, size_t exe_end)
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
            if (!ptr)
                continue;

            if (ptr == (size_t)GetProcAddress(GetModuleHandle(TEXT("OLE32.DLL")), "CoCreateInstance"))
            {
                *(size_t*)i = (size_t)CustomCoCreateInstance;
                VirtualProtect((size_t*)i, sizeof(size_t), dwProtect[0], &dwProtect[1]);
                break;
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

    static auto getSectionEnd = [](IMAGE_NT_HEADERS* ntHeader, size_t inst) -> auto
    {
        auto sec = getSection(ntHeader, ntHeader->FileHeader.NumberOfSections - 1);
        // .bind section may have vanished from the executable (test case: Yakuza 4)
        // so back to the first valid section if that happened
        while (sec->Misc.VirtualSize == 0) sec--;

        auto secSize = max(sec->SizeOfRawData, sec->Misc.VirtualSize);
        auto end = inst + max(sec->PointerToRawData, sec->VirtualAddress) + secSize;
        return end;
    };

    auto hExecutableInstance_end = getSectionEnd(ntHeader, hExecutableInstance);

    // Find kernel32.dll
    for (size_t i = 0; i < nNumImports; i++)
    {
        if ((size_t)(hExecutableInstance + (pImports + i)->Name) < hExecutableInstance_end)
        {
            if (!_stricmp((const char*)(hExecutableInstance + (pImports + i)->Name), "KERNEL32.DLL"))
                PatchIAT(hExecutableInstance + (pImports + i)->FirstThunk, 0, hExecutableInstance_end);
            else if (!_stricmp((const char*)(hExecutableInstance + (pImports + i)->Name), "OLE32.DLL"))
                PatchCoCreateInstance(hExecutableInstance + (pImports + i)->FirstThunk, 0, hExecutableInstance_end);
        }
    }

    // Fixing ordinals
    auto szSelfName = GetSelfName();

    static auto PatchOrdinals = [&szSelfName](size_t hInstance)
    {
        IMAGE_NT_HEADERS*           ntHeader = (IMAGE_NT_HEADERS*)(hInstance + ((IMAGE_DOS_HEADER*)hInstance)->e_lfanew);
        IMAGE_IMPORT_DESCRIPTOR*    pImports = (IMAGE_IMPORT_DESCRIPTOR*)(hInstance + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
        size_t                      nNumImports = ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size / sizeof(IMAGE_IMPORT_DESCRIPTOR) - 1;

        for (size_t i = 0; i < nNumImports; i++)
        {
            if ((size_t)(hInstance + (pImports + i)->Name) < getSectionEnd(ntHeader, (size_t)hInstance))
            {
                if (iequals(szSelfName, (to_wstring((const char*)(hInstance + (pImports + i)->Name)))))
                {
                    PIMAGE_THUNK_DATA thunk = (PIMAGE_THUNK_DATA)(hInstance + (pImports + i)->OriginalFirstThunk);
                    size_t j = 0;
                    while (thunk->u1.Function)
                    {
                        if (thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)
                        {
                            PIMAGE_IMPORT_BY_NAME import = (PIMAGE_IMPORT_BY_NAME)(hInstance + thunk->u1.AddressOfData);
                            void** p = (void**)(hInstance + (pImports + i)->FirstThunk);
                            if (iequals(szSelfName, L"dsound.dll"))
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
                            else if (iequals(szSelfName, L"dinput8.dll"))
                            {
                                DWORD Protect;
                                VirtualProtect(&p[j], 4, PAGE_EXECUTE_READWRITE, &Protect);

                                if ((IMAGE_ORDINAL(thunk->u1.Ordinal)) == 1)
                                    p[j] = _DirectInput8Create;
                            }
                            ++j;
                        }
                        ++thunk;
                    }
                    break;
                }
            }
        }
    };

    ModuleList dlls;
    dlls.Enumerate(ModuleList::SearchLocation::LocalOnly);
    for (auto& e : dlls.m_moduleList)
    {
        PatchOrdinals((size_t)std::get<HMODULE>(e));
    }
    return matchedImports > 0;
}

LONG WINAPI CustomUnhandledExceptionFilter(LPEXCEPTION_POINTERS ExceptionInfo)
{
    // step 1: write minidump
    wchar_t     modulename[MAX_PATH];
    wchar_t     filename[MAX_PATH];
    wchar_t     timestamp[128];
    __time64_t  time;
    struct tm   ltime;
    HANDLE      hFile;
    HWND        hWnd;

    wchar_t* modulenameptr;
    if (GetModuleFileNameW(GetModuleHandle(NULL), modulename, _countof(modulename)) != 0)
    {
        modulenameptr = wcsrchr(modulename, '\\');
        *modulenameptr = L'\0';
        modulenameptr += 1;
    }
    else
    {
        wcscpy(modulenameptr, L"err.err");
    }

    _time64(&time);
    _localtime64_s(&ltime, &time);
    wcsftime(timestamp, _countof(timestamp), L"%Y%m%d%H%M%S", &ltime);
    swprintf_s(filename, L"%s\\%s\\%s.%s.dmp", modulename, L"CrashDumps", modulenameptr, timestamp);

    hFile = CreateFileW(filename, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile != INVALID_HANDLE_VALUE)
    {
        MINIDUMP_EXCEPTION_INFORMATION ex;
        memset(&ex, 0, sizeof(ex));
        ex.ThreadId = GetCurrentThreadId();
        ex.ExceptionPointers = ExceptionInfo;
        ex.ClientPointers = TRUE;

        if (FAILED(MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile, MiniDumpWithDataSegs, &ex, NULL, NULL)))
        {
        }

        CloseHandle(hFile);
    }

    // step 2: write log
    // Logs exception into buffer and writes to file
    swprintf_s(filename, L"%s\\%s\\%s.%s.log", modulename, L"CrashDumps", modulenameptr, timestamp);
    hFile = CreateFileW(filename, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hFile != INVALID_HANDLE_VALUE)
    {
        auto Log = [ExceptionInfo, hFile](char* buffer, size_t size, bool reg, bool stack, bool trace)
        {
            if (LogException(buffer, size, (LPEXCEPTION_POINTERS)ExceptionInfo, reg, stack, trace))
            {
                DWORD NumberOfBytesWritten = 0;
                WriteFile(hFile, buffer, strlen(buffer), &NumberOfBytesWritten, NULL);
            }
        };

        // Try to make a very descriptive exception, for that we need to malloc a huge buffer...
        if (auto buffer = (char*)malloc(max_logsize_ever))
        {
            Log(buffer, max_logsize_ever, true, true, true);
            free(buffer);
        }
        else
        {
            // Use a static buffer, no need for any allocation
            static const auto size = max_logsize_basic + max_logsize_regs + max_logsize_stackdump;
            static char static_buf[size];
            static_assert(size <= max_static_buffer, "Static buffer is too big");

            Log(buffer = static_buf, sizeof(static_buf), true, true, false);
        }

        CloseHandle(hFile);
    }

    // step 3: exit the application
    ShowCursor(TRUE);
    hWnd = FindWindowW(0, L"");
    SetForegroundWindow(hWnd);

    return EXCEPTION_CONTINUE_SEARCH;
}

void Init()
{
    std::wstring modulePath = GetModuleFileNameW(hm);
    std::wstring moduleName = modulePath.substr(modulePath.find_last_of(L"/\\") + 1);
    moduleName.resize(moduleName.find_last_of(L'.'));
    modulePath.resize(modulePath.find_last_of(L"/\\") + 1);
    iniPaths.emplace_back(modulePath + moduleName + L".ini");
    iniPaths.emplace_back(modulePath + L"global.ini");
    iniPaths.emplace_back(modulePath + L"scripts\\global.ini");
    iniPaths.emplace_back(modulePath + L"plugins\\global.ini");

    auto nForceEPHook = GetPrivateProfileInt(TEXT("globalsets"), TEXT("forceentrypointhook"), FALSE, iniPaths);
    auto nDontLoadFromDllMain = GetPrivateProfileInt(TEXT("globalsets"), TEXT("dontloadfromdllmain"), TRUE, iniPaths);
    auto nFindModule = GetPrivateProfileInt(TEXT("globalsets"), TEXT("findmodule"), FALSE, iniPaths);
    auto nDisableCrashDumps = GetPrivateProfileInt(TEXT("globalsets"), TEXT("disablecrashdumps"), FALSE, iniPaths);

    if (!nDisableCrashDumps)
    {
        std::wstring m = GetModuleFileNameW(NULL);
        m = m.substr(0, m.find_last_of(L"/\\") + 1) + L"CrashDumps";

        auto FolderExists = [](LPCWSTR szPath) -> BOOL
        {
            DWORD dwAttrib = GetFileAttributes(szPath);
            return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
        };

        if (FolderExists(m.c_str()))
        {
            SetUnhandledExceptionFilter(CustomUnhandledExceptionFilter);
            // Now stub out CustomUnhandledExceptionFilter so NO ONE ELSE can set it!
#if !X64
            uint32_t ret = 0x900004C2; //ret4
#else
            uint32_t ret = 0x909090C3; //ret
#endif
            DWORD protect[2];
            VirtualProtect(&SetUnhandledExceptionFilter, sizeof(ret), PAGE_EXECUTE_READWRITE, &protect[0]);
            memcpy(&SetUnhandledExceptionFilter, &ret, sizeof(ret));
            VirtualProtect(&SetUnhandledExceptionFilter, sizeof(ret), protect[0], &protect[1]);
        }
    }

    if (nForceEPHook != FALSE || nDontLoadFromDllMain != FALSE)
    {
        HMODULE mainModule = GetModuleHandle(NULL);
        bool hookedSuccessfully = HookKernel32IAT(mainModule, true);
        if (!hookedSuccessfully)
        {
            LoadOriginalLibrary();
        }

        HMODULE m = mainModule;
        if (nFindModule)
        {
            ModuleList dlls;
            dlls.Enumerate(ModuleList::SearchLocation::LocalOnly);

            auto ual = std::find_if(dlls.m_moduleList.begin(), dlls.m_moduleList.end(), [](auto const& it)
            {
                return std::get<HMODULE>(it) == hm;
            });

            auto sim = std::find_if(dlls.m_moduleList.rbegin(), dlls.m_moduleList.rend(), [&ual](auto const& it)
            {
                auto str1 = std::get<std::wstring>(*ual);
                auto str2 = std::get<std::wstring>(it);
                std::transform(str1.begin(), str1.end(), str1.begin(), [](wchar_t c) { return ::towlower(c); });
                std::transform(str2.begin(), str2.end(), str2.begin(), [](wchar_t c) { return ::towlower(c); });

                return (str2 != str1) && (str2.find(str1) != std::wstring::npos);
            });

            if (ual != dlls.m_moduleList.begin())
            {
                if (sim != dlls.m_moduleList.rend())
                    m = std::get<HMODULE>(*sim);
                else
                    m = std::get<HMODULE>(*std::prev(ual, 1));
            }
        }

        if (m != mainModule)
        {
            HookKernel32IAT(m, false);
        }
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
