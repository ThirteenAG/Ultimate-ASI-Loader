#include "dllmain.h"
#include "exception.hpp"
#include <shellapi.h>
#include <Commctrl.h>
#pragma comment(lib,"Comctl32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#include <initguid.h>
#include <filesystem>
#include <safetyhook.hpp>

#if !X64
#include <d3d8to9\source\d3d8to9.hpp>
extern "C" Direct3D8* WINAPI Direct3DCreate8(UINT SDKVersion);
#endif

bool WINAPI IsUltimateASILoader()
{
    return true;
}

void* ogMemModule = NULL;
void* WINAPI GetMemoryModule()
{
    return ogMemModule;
}

HMODULE hm;
std::vector<std::wstring> iniPaths;
std::filesystem::path sFileLoaderPath;
std::filesystem::path gamePath;
std::wstring sLoadFromAPI;
std::vector<std::pair<std::filesystem::path, LARGE_INTEGER>> updateFilenames;
thread_local std::string sCurrentFindFileDirA;
thread_local std::wstring sCurrentFindFileDirW;

HRESULT CALLBACK TaskDialogCallbackProc(HWND hwnd, UINT uNotification, WPARAM wParam, LPARAM lParam, LONG_PTR dwRefData)
{
    switch (uNotification)
    {
    case TDN_HYPERLINK_CLICKED:
        ShellExecuteW(hwnd, L"open", (LPCWSTR)lParam, NULL, NULL, SW_SHOW);
        break;
    }

    return S_OK;
}

bool iequals(std::wstring_view s1, std::wstring_view s2)
{
    std::wstring str1(std::move(s1));
    std::wstring str2(std::move(s2));
    std::transform(str1.begin(), str1.end(), str1.begin(), [](wchar_t c) { return ::towlower(c); });
    std::transform(str2.begin(), str2.end(), str2.begin(), [](wchar_t c) { return ::towlower(c); });
    return (str1 == str2);
}

std::filesystem::path lexicallyRelativeCaseIns(const std::filesystem::path& path, const std::filesystem::path& base)
{
    class input_iterator_range
    {
    public:
        input_iterator_range(const std::filesystem::path::const_iterator& first, const std::filesystem::path::const_iterator& last)
            : _first(first)
            , _last(last)
        {}
        std::filesystem::path::const_iterator begin() const { return _first; }
        std::filesystem::path::const_iterator end() const { return _last; }
    private:
        std::filesystem::path::const_iterator _first;
        std::filesystem::path::const_iterator _last;
    };

    if (!iequals(path.root_name().wstring(), base.root_name().wstring()) || path.is_absolute() != base.is_absolute() || (!path.has_root_directory() && base.has_root_directory())) {
        return std::filesystem::path();
    }
    std::filesystem::path::const_iterator a = path.begin(), b = base.begin();
    while (a != path.end() && b != base.end() && iequals(a->wstring(), b->wstring())) {
        ++a;
        ++b;
    }
    if (a == path.end() && b == base.end()) {
        return std::filesystem::path(".");
    }
    int count = 0;
    for (const auto& element : input_iterator_range(b, base.end())) {
        if (element != "." && element != "" && element != "..") {
            ++count;
        }
        else if (element == "..") {
            --count;
        }
    }
    if (count < 0) {
        return std::filesystem::path();
    }
    std::filesystem::path result;
    for (int i = 0; i < count; ++i) {
        result /= "..";
    }
    for (const auto& element : input_iterator_range(a, path.end())) {
        result /= element;
    }
    return result;
}

void FillUpdateFilenames()
{
    if (updateFilenames.empty())
    {
        auto path = gamePath / sFileLoaderPath;
        std::error_code ec;
        constexpr auto perms = std::filesystem::directory_options::skip_permission_denied | std::filesystem::directory_options::follow_directory_symlink;
        for (const auto& it : std::filesystem::recursive_directory_iterator(path, perms, ec))
        {
            if (!it.is_directory(ec))
            {
                LARGE_INTEGER Integer;
                Integer.QuadPart = std::filesystem::file_size(it, ec);
                updateFilenames.emplace_back(lexicallyRelativeCaseIns(it.path(), path), Integer);
            }
        }
    }
}

LARGE_INTEGER FindFileCheckOverloadedPath(CHAR* filename)
{
    FillUpdateFilenames();

    if (!updateFilenames.empty())
    {
        auto it = std::find_if(updateFilenames.begin(), updateFilenames.end(), [&](const auto& val)
        {
            return (sCurrentFindFileDirA.starts_with(val.first.parent_path().string()) && val.first.string().ends_with(filename));
        });

        if (it != updateFilenames.end())
            return it->second;
    }

    return {};
}

LARGE_INTEGER FindFileCheckOverloadedPath(WCHAR* filename)
{
    FillUpdateFilenames();

    if (!updateFilenames.empty())
    {
        auto it = std::find_if(updateFilenames.begin(), updateFilenames.end(), [&](const auto& val)
        {
            return (sCurrentFindFileDirW.starts_with(val.first.parent_path().wstring()) && val.first.wstring().ends_with(filename));
        });

        if (it != updateFilenames.end())
            return it->second;
    }

    return {};
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

std::wstring GetCurrentDirectoryW()
{
    static constexpr auto INITIAL_BUFFER_SIZE = MAX_PATH;
    static constexpr auto MAX_ITERATIONS = 7;
    std::wstring ret;
    auto bufferSize = INITIAL_BUFFER_SIZE;
    for (size_t iterations = 0; iterations < MAX_ITERATIONS; ++iterations)
    {
        ret.resize(bufferSize);
        auto charsReturned = GetCurrentDirectoryW(bufferSize, ret.data());
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

std::wstring GetModulePath(HMODULE hModule)
{
    static constexpr auto INITIAL_BUFFER_SIZE = MAX_PATH;
    static constexpr auto MAX_ITERATIONS = 7;
    std::wstring ret;
    auto bufferSize = INITIAL_BUFFER_SIZE;
    for (size_t iterations = 0; iterations < MAX_ITERATIONS; ++iterations)
    {
        ret.resize(bufferSize);
        size_t charsReturned = 0;
        charsReturned = GetModuleFileNameW(hModule, ret.data(), bufferSize);
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
    return std::wstring();
}

std::wstring GetExeModulePath()
{
    std::wstring r = GetModulePath(NULL);
    r = r.substr(0, r.find_last_of(L"/\\") + 1);
    return r;
}

UINT GetPrivateProfileIntW(LPCWSTR lpAppName, LPCWSTR lpKeyName, INT nDefault, const std::vector<std::wstring>& fileNames)
{
    for (const auto& file : fileNames)
    {
        nDefault = GetPrivateProfileIntW(lpAppName, lpKeyName, nDefault, file.c_str());
    }
    return nDefault;
}

std::wstring GetPrivateProfileStringW(LPCWSTR lpAppName, LPCWSTR lpKeyName, LPCWSTR szDefault, const std::vector<std::wstring>& fileNames)
{
    std::wstring ret(szDefault);
    ret.resize(MAX_PATH);
    for (const auto& file : fileNames)
    {
        GetPrivateProfileStringW(lpAppName, lpKeyName, ret.data(), ret.data(), ret.size(), file.c_str());
    }
    return ret.data();
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
    eFindFirstFileA,
    eFindNextFileA,
    eFindFirstFileW,
    eFindNextFileW,
    eFindFirstFileExA,
    eFindFirstFileExW,
    eLoadLibraryExA,
    eLoadLibraryExW,
    eLoadLibraryA,
    eLoadLibraryW,
    eFreeLibrary,
    eCreateEventA,
    eCreateEventW,
    eGetSystemInfo,
    eInterlockedCompareExchange,
    eSleep,
    eGetSystemTimeAsFileTime,
    eGetCurrentProcessId,
    eGetCommandLineA,
    eGetCommandLineW,
    eAcquireSRWLockExclusive,
    eCreateFileA,
    eCreateFileW,
    eGetFileAttributesA,
    eGetFileAttributesW,
    eGetFileAttributesExA,
    eGetFileAttributesExW,

    Kernel32ExportsNamesCount
};

enum Kernel32ExportsData
{
    IATPtr,
    ProcAddress,

    Kernel32ExportsDataCount
};

enum OLE32ExportsNames
{
    eCoCreateInstance,

    OLE32ExportsNamesCount
};

size_t Kernel32Data[Kernel32ExportsNamesCount][Kernel32ExportsDataCount];
size_t OLE32Data[OLE32ExportsNamesCount][Kernel32ExportsDataCount];

namespace OverloadFromFolder
{
    SafetyHookInline shLoadLibraryExA = {};
    SafetyHookInline shLoadLibraryExW = {};
    SafetyHookInline shCreateFileA = {};
    SafetyHookInline shCreateFileW = {};
    SafetyHookInline shGetFileAttributesA = {};
    SafetyHookInline shGetFileAttributesW = {};
    SafetyHookInline shGetFileAttributesExA = {};
    SafetyHookInline shGetFileAttributesExW = {};
    SafetyHookInline shFindFirstFileA = {};
    SafetyHookInline shFindNextFileA = {};
    SafetyHookInline shFindFirstFileW = {};
    SafetyHookInline shFindNextFileW = {};
    SafetyHookInline shFindFirstFileExA = {};
    SafetyHookInline shFindFirstFileExW = {};

    void HookAPIForOverload();
}

#if !X64
#define IDR_VORBISF    101
#define IDR_WNDMODE    103
#define IDR_WNDWINI    104
#define IDR_BINK       105
#endif

HMODULE LoadLib(const std::wstring& lpLibFileName);
static LONG OriginalLibraryLoaded = 0;
void LoadOriginalLibrary()
{
    if (_InterlockedCompareExchange(&OriginalLibraryLoaded, 1, 0) != 0) return;

    auto szSelfName = GetSelfName();
    auto szSystemPath = SHGetKnownFolderPath(FOLDERID_System, 0, nullptr) + L'\\' + szSelfName;
    auto szLocalPath = GetModuleFileNameW(hm); szLocalPath = szLocalPath.substr(0, szLocalPath.find_last_of(L"/\\") + 1);

    if (iequals(szSelfName, L"dsound.dll"))
    {
        szLocalPath += L"dsoundHooked.dll";
        if (std::filesystem::exists(szLocalPath))
            dsound.LoadOriginalLibrary(LoadLib(szLocalPath));
        else
            dsound.LoadOriginalLibrary(LoadLib(szSystemPath));
    }
    else if (iequals(szSelfName, L"dinput8.dll"))
    {
        szLocalPath += L"dinput8Hooked.dll";
        if (std::filesystem::exists(szLocalPath))
            dinput8.LoadOriginalLibrary(LoadLib(szLocalPath));
        else
            dinput8.LoadOriginalLibrary(LoadLib(szSystemPath));
    }
    else if (iequals(szSelfName, L"wininet.dll"))
    {
        szLocalPath += L"wininetHooked.dll";
        if (std::filesystem::exists(szLocalPath))
            wininet.LoadOriginalLibrary(LoadLib(szLocalPath));
        else
            wininet.LoadOriginalLibrary(LoadLib(szSystemPath));
    }
    else if (iequals(szSelfName, L"version.dll"))
    {
        szLocalPath += L"versionHooked.dll";
        if (std::filesystem::exists(szLocalPath))
            version.LoadOriginalLibrary(LoadLib(szLocalPath));
        else
            version.LoadOriginalLibrary(LoadLib(szSystemPath));
    }
    else if (iequals(szSelfName, L"d3d9.dll"))
    {
        szLocalPath += L"d3d9Hooked.dll";
        if (std::filesystem::exists(szLocalPath))
            d3d9.LoadOriginalLibrary(LoadLib(szLocalPath));
        else
            d3d9.LoadOriginalLibrary(LoadLib(szSystemPath));
    }
    else if (iequals(szSelfName, L"d3d10.dll"))
    {
        szLocalPath += L"d3d10Hooked.dll";
        if (std::filesystem::exists(szLocalPath))
            d3d10.LoadOriginalLibrary(LoadLib(szLocalPath));
        else
            d3d10.LoadOriginalLibrary(LoadLib(szSystemPath));
    }
    else if (iequals(szSelfName, L"d3d11.dll"))
    {
        szLocalPath += L"d3d11Hooked.dll";
        if (std::filesystem::exists(szLocalPath))
            d3d11.LoadOriginalLibrary(LoadLib(szLocalPath));
        else
            d3d11.LoadOriginalLibrary(LoadLib(szSystemPath));
    }
    else if (iequals(szSelfName, L"d3d12.dll"))
    {
        szLocalPath += L"d3d12Hooked.dll";
        if (std::filesystem::exists(szLocalPath))
            d3d12.LoadOriginalLibrary(LoadLib(szLocalPath));
        else
            d3d12.LoadOriginalLibrary(LoadLib(szSystemPath));
    } 
    else if (iequals(szSelfName, L"winmm.dll"))
    {
        szLocalPath += L"winmmHooked.dll";
        if (std::filesystem::exists(szLocalPath))
            winmm.LoadOriginalLibrary(LoadLib(szLocalPath));
        else
            winmm.LoadOriginalLibrary(LoadLib(szSystemPath));
    }
    else if (iequals(szSelfName, L"winhttp.dll"))
    {
        szLocalPath += L"winhttpHooked.dll";
        if (std::filesystem::exists(szLocalPath))
            winhttp.LoadOriginalLibrary(LoadLib(szLocalPath));
        else
            winhttp.LoadOriginalLibrary(LoadLib(szSystemPath));
    }
    else
#if !X64
    if (iequals(szSelfName, L"vorbisFile.dll"))
    {
        szLocalPath += L"vorbisHooked.dll";
        if (std::filesystem::exists(szLocalPath))
        {
            vorbisfile.LoadOriginalLibrary(LoadLib(szLocalPath), false);
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
                            vorbisfile.LoadOriginalLibrary(ogMemModule = MemoryLoadLibrary((const void*)pLockedResource, dwResourceSize), true);

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
    else if (iequals(szSelfName, L"ddraw.dll"))
    {
        szLocalPath += L"ddrawHooked.dll";
        if (std::filesystem::exists(szLocalPath))
            ddraw.LoadOriginalLibrary(LoadLib(szLocalPath));
        else
            ddraw.LoadOriginalLibrary(LoadLib(szSystemPath));
    }
    else if (iequals(szSelfName, L"d3d8.dll"))
    {
        szLocalPath += L"d3d8Hooked.dll";
        if (std::filesystem::exists(szLocalPath))
            d3d8.LoadOriginalLibrary(LoadLib(szLocalPath));
        else
        {
            d3d8.LoadOriginalLibrary(LoadLib(szSystemPath));
            if (GetPrivateProfileIntW(L"globalsets", L"used3d8to9", FALSE, iniPaths))
                d3d8.Direct3DCreate8 = (FARPROC)Direct3DCreate8;
        }
    }
    else if (iequals(szSelfName, L"msacm32.dll"))
    {
        szLocalPath += L"msacm32Hooked.dll";
        if (std::filesystem::exists(szLocalPath))
            msacm32.LoadOriginalLibrary(LoadLib(szLocalPath));
        else
            msacm32.LoadOriginalLibrary(LoadLib(szSystemPath));
    }
    else if (iequals(szSelfName, L"dinput.dll"))
    {
        szLocalPath += L"dinputHooked.dll";
        if (std::filesystem::exists(szLocalPath))
            dinput.LoadOriginalLibrary(LoadLib(szLocalPath));
        else
            dinput.LoadOriginalLibrary(LoadLib(szSystemPath));
    }
    else if (iequals(szSelfName, L"msvfw32.dll"))
    {
        szLocalPath += L"msvfw32Hooked.dll";
        if (std::filesystem::exists(szLocalPath))
            msvfw32.LoadOriginalLibrary(LoadLib(szLocalPath));
        else
            msvfw32.LoadOriginalLibrary(LoadLib(szSystemPath));
    }
    else if (iequals(szSelfName, L"binkw32.dll"))
    {
        szLocalPath += L"binkw32Hooked.dll";
        if (std::filesystem::exists(szLocalPath))
        {
            bink2w32.LoadOriginalLibrary(LoadLib(szLocalPath), false);
        }
        else
        {
            HRSRC hResource = FindResource(hm, MAKEINTRESOURCE(IDR_BINK), RT_RCDATA);
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
                            bink2w32.LoadOriginalLibrary(ogMemModule = MemoryLoadLibrary((const void*)pLockedResource, dwResourceSize), true);
                        }
                    }
                }
            }
        }
    }
    else if (iequals(szSelfName, L"bink2w32.dll"))
    {
        szLocalPath += L"bink2w32Hooked.dll";
        if (std::filesystem::exists(szLocalPath))
        {
            bink2w32.LoadOriginalLibrary(LoadLib(szLocalPath), false);
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
#else
    if (iequals(szSelfName, L"bink2w64.dll"))
    {
        szLocalPath += L"bink2w64Hooked.dll";
        if (std::filesystem::exists(szLocalPath))
        {
            bink2w64.LoadOriginalLibrary(LoadLib(szLocalPath));
        }
    } 
    else if (iequals(szSelfName, L"binkw64.dll"))
    {
        szLocalPath += L"binkw64Hooked.dll";
        if (std::filesystem::exists(szLocalPath))
        {
            bink2w64.LoadOriginalLibrary(LoadLib(szLocalPath));
        }
    }
    else
#endif
    {
        MessageBox(0, TEXT("This library isn't supported."), TEXT("ASI Loader"), MB_ICONERROR);
        ExitProcess(0);
    }

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
                pd3d8 = LoadLib(SHGetKnownFolderPath(FOLDERID_System, 0, nullptr) + L'\\' + L"d3d8.dll");
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
                        auto h = LoadLib(path);
                        SetCurrentDirectoryW(dir.c_str()); //in case asi switched it

                        if (h == NULL)
                        {
                            auto e = GetLastError();
                            if (e != ERROR_DLL_INIT_FAILED && e != ERROR_BAD_EXE_FORMAT) // in case dllmain returns false or IMAGE_MACHINE is not compatible
                            {
                                TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
                                int nClickedBtn;
                                BOOL bCheckboxChecked;
                                LPCWSTR szTitle = L"ASI Loader", szHeader = L"", szContent = L"";
                                TASKDIALOG_BUTTON aCustomButtons[] = { { 1000, L"Continue" } };

                                std::wstring msg = L"Unable to load " + std::wstring(fd->cFileName) + L". Error: " + std::to_wstring(e);
                                szHeader = msg.c_str();

                                if (e == ERROR_MOD_NOT_FOUND)
                                {
                                    szContent = L"This ASI file requires a dependency that is missing from your system. To identify the missing dependency, download and run the free, open-source app, " \
                                        L"<a href=\"https://github.com/lucasg/Dependencies/releases/latest\">Dependencies</a>.\n\n" \
                                        L"<a href=\"https://github.com/lucasg/Dependencies\">https://github.com/lucasg/Dependencies</a>";
                                }

                                tdc.hwndParent = NULL;
                                tdc.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ENABLE_HYPERLINKS | TDF_SIZE_TO_CONTENT | TDF_CAN_BE_MINIMIZED;
                                tdc.pButtons = aCustomButtons;
                                tdc.cButtons = _countof(aCustomButtons);
                                tdc.pszWindowTitle = szTitle;
                                tdc.pszMainIcon = TD_ERROR_ICON;
                                tdc.pszMainInstruction = szHeader;
                                tdc.pszContent = szContent;
                                tdc.pfCallback = TaskDialogCallbackProc;
                                tdc.lpCallbackData = 0;

                                std::ignore = TaskDialogIndirect(&tdc, &nClickedBtn, NULL, &bCheckboxChecked);
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
                        ogMemModule = MemoryLoadLibrary((const void*)pLockedResource, dwResourceSize);
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

        SetCurrentDirectoryW(szSelfPath.c_str());

        if (!sFileLoaderPath.empty())
        {
            if (SetCurrentDirectoryW(sFileLoaderPath.wstring().c_str()))
                FindFiles(&fd);
        }
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
void LoadPluginsAndRestoreIAT(uintptr_t retaddr, std::wstring_view calledFrom = L"")
{
    if (!sLoadFromAPI.empty() && calledFrom != sLoadFromAPI)
        return;

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

    if (!sFileLoaderPath.empty())
    {
        OverloadFromFolder::HookAPIForOverload();
    }

    LoadEverything();
}

std::filesystem::path WINAPI GetOverloadedFilePath(std::filesystem::path lpFilename)
{
    try
    {
        std::error_code ec;

        static auto starts_with = [](const std::filesystem::path& path, const std::filesystem::path& base) -> bool {
            std::wstring str1(path.wstring()); std::wstring str2(base.wstring());
            std::transform(str1.begin(), str1.end(), str1.begin(), ::tolower);
            std::transform(str2.begin(), str2.end(), str2.begin(), ::tolower);
            return str1.starts_with(str2);
        };

        if (gamePath.empty())
            gamePath = std::filesystem::path(GetExeModulePath());

        auto filePath = lpFilename;
        auto absolutePath = std::filesystem::absolute(filePath, ec);
        auto relativePath = lexicallyRelativeCaseIns(absolutePath, gamePath);
        auto commonPath = gamePath;

        if (starts_with(relativePath, ".."))
        {
            auto common = std::mismatch(absolutePath.begin(), absolutePath.end(), gamePath.begin());
            for (auto& iter = common.second; iter != gamePath.end(); ++iter)
                commonPath = commonPath.parent_path();

            std::filesystem::path rp;
            for (auto& p : relativePath)
            {
                if (p != "..")
                    rp = rp / p;
            }
            relativePath = rp;
        }

        if (starts_with(std::filesystem::path(absolutePath).remove_filename(), gamePath) || starts_with(std::filesystem::path(absolutePath).remove_filename(), commonPath))
        {
            auto newPath = gamePath / sFileLoaderPath / relativePath;
            if (std::filesystem::exists(newPath, ec) && !std::filesystem::is_directory(newPath, ec))
                return newPath;
            }
        }
    catch (...) {}

    return {};
}

std::filesystem::path GetFilePathForOverload(auto path)
{
    try
    {
        if (!sFileLoaderPath.empty())
            return GetOverloadedFilePath(path);
    }
    catch (...) {}

    return {};
}

#define value_orA(path1, path2) (path1.empty() ? path2 : path1.string().c_str())
#define value_orW(path1, path2) (path1.empty() ? path2 : path1.wstring().c_str())

HMODULE LoadLib(const std::wstring& lpLibFileName)
{
    auto r = GetFilePathForOverload(lpLibFileName);
    return LoadLibraryW(value_orW(r, lpLibFileName.c_str()));
}

void WINAPI CustomGetStartupInfoA(LPSTARTUPINFOA lpStartupInfo)
{
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"GetStartupInfoA");
    return GetStartupInfoA(lpStartupInfo);
}

void WINAPI CustomGetStartupInfoW(LPSTARTUPINFOW lpStartupInfo)
{
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"GetStartupInfoW");
    return GetStartupInfoW(lpStartupInfo);
}

HMODULE WINAPI CustomGetModuleHandleA(LPCSTR lpModuleName)
{
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"GetModuleHandleA");
    return GetModuleHandleA(lpModuleName);
}

HMODULE WINAPI CustomGetModuleHandleW(LPCWSTR lpModuleName)
{
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"GetModuleHandleW");
    return GetModuleHandleW(lpModuleName);
}

FARPROC WINAPI CustomGetProcAddress(HMODULE hModule, LPCSTR lpProcName)
{
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"GetProcAddress");
    return GetProcAddress(hModule, lpProcName);
}

DWORD WINAPI CustomGetShortPathNameA(LPCSTR lpszLongPath, LPSTR lpszShortPath, DWORD cchBuffer)
{
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"GetShortPathNameA");
    return GetShortPathNameA(lpszLongPath, lpszShortPath, cchBuffer);
}

HMODULE WINAPI CustomLoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
    LoadOriginalLibrary();

    auto r = GetFilePathForOverload(lpLibFileName);
    return LoadLibraryExA(value_orA(r, lpLibFileName), hFile, dwFlags);
}

HMODULE WINAPI CustomLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
    LoadOriginalLibrary();

    auto r = GetFilePathForOverload(lpLibFileName);
    return LoadLibraryExW(value_orW(r, lpLibFileName), hFile, dwFlags);
}

HMODULE WINAPI CustomLoadLibraryA(LPCSTR lpLibFileName)
{
    LoadOriginalLibrary();

    auto r = GetFilePathForOverload(lpLibFileName);
    return LoadLibraryA(value_orA(r, lpLibFileName));
}

HMODULE WINAPI CustomLoadLibraryW(LPCWSTR lpLibFileName)
{
    LoadOriginalLibrary();

    auto r = GetFilePathForOverload(lpLibFileName);
    return LoadLibraryW(value_orW(r, lpLibFileName));
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
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"CreateEventA");
    return CreateEventA(lpEventAttributes, bManualReset, bInitialState, lpName);
}

HANDLE WINAPI CustomCreateEventW(LPSECURITY_ATTRIBUTES lpEventAttributes, BOOL bManualReset, BOOL bInitialState, LPCWSTR lpName)
{
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"CreateEventW");
    return CreateEventW(lpEventAttributes, bManualReset, bInitialState, lpName);
}

void WINAPI CustomGetSystemInfo(LPSYSTEM_INFO lpSystemInfo)
{
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"GetSystemInfo");
    return GetSystemInfo(lpSystemInfo);
}

LONG WINAPI CustomInterlockedCompareExchange(LONG volatile* Destination, LONG ExChange, LONG Comperand)
{
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"InterlockedCompareExchange");
    return _InterlockedCompareExchange(Destination, ExChange, Comperand);
}

void WINAPI CustomSleep(DWORD dwMilliseconds)
{
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"Sleep");
    return Sleep(dwMilliseconds);
}

void WINAPI CustomGetSystemTimeAsFileTime(LPFILETIME lpSystemTimeAsFileTime)
{
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"GetSystemTimeAsFileTime");
    return GetSystemTimeAsFileTime(lpSystemTimeAsFileTime);
}

DWORD WINAPI CustomGetCurrentProcessId()
{
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"GetCurrentProcessId");
    return GetCurrentProcessId();
}

LPSTR WINAPI CustomGetCommandLineA()
{
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"GetCommandLineA");
    return GetCommandLineA();
}

LPWSTR WINAPI CustomGetCommandLineW()
{
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"GetCommandLineW");
    return GetCommandLineW();
}

void WINAPI CustomAcquireSRWLockExclusive(PSRWLOCK SRWLock)
{
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"AcquireSRWLockExclusive");
    return AcquireSRWLockExclusive(SRWLock);
}

HANDLE WINAPI CustomCreateFileA(LPCSTR lpFileName, DWORD dwAccess, DWORD dwSharing, LPSECURITY_ATTRIBUTES saAttributes, DWORD dwCreation, DWORD dwAttributes, HANDLE hTemplate)
{
    static bool once = false;
    if (!once)
    {
        LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"CreateFileA");
        once = true;
    }

    auto r = GetFilePathForOverload(lpFileName);
    return CreateFileA(value_orA(r, lpFileName), dwAccess, dwSharing, saAttributes, dwCreation, dwAttributes, hTemplate);
}

HANDLE WINAPI CustomCreateFileW(LPCWSTR lpFileName, DWORD dwAccess, DWORD dwSharing, LPSECURITY_ATTRIBUTES saAttributes, DWORD dwCreation, DWORD dwAttributes, HANDLE hTemplate)
{
    static bool once = false;
    if (!once)
    {
        LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"CreateFileW");
        once = true;
    }

    auto r = GetFilePathForOverload(lpFileName);
    return CreateFileW(value_orW(r, lpFileName), dwAccess, dwSharing, saAttributes, dwCreation, dwAttributes, hTemplate);
}

DWORD WINAPI CustomGetFileAttributesA(LPCSTR lpFileName)
{
    static bool once = false;
    if (!once)
    {
        LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"GetFileAttributesA");
        once = true;
    }

    auto r = GetFilePathForOverload(lpFileName);
    return GetFileAttributesA(value_orA(r, lpFileName));
}

DWORD WINAPI CustomGetFileAttributesW(LPCWSTR lpFileName)
{
    static bool once = false;
    if (!once)
    {
        LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"GetFileAttributesW");
        once = true;
    }

    auto r = GetFilePathForOverload(lpFileName);
    return GetFileAttributesW(value_orW(r, lpFileName));
}

BOOL WINAPI CustomGetFileAttributesExA(LPCSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation)
{
    static bool once = false;
    if (!once)
    {
        LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"GetFileAttributesExA");
        once = true;
    }

    auto r = GetFilePathForOverload(lpFileName);
    return GetFileAttributesExA(value_orA(r, lpFileName), fInfoLevelId, lpFileInformation);
}

BOOL WINAPI CustomGetFileAttributesExW(LPCWSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation)
{
    static bool once = false;
    if (!once)
    {
        LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"GetFileAttributesExW");
        once = true;
    }

    auto r = GetFilePathForOverload(lpFileName);
    return GetFileAttributesExW(value_orW(r, lpFileName), fInfoLevelId, lpFileInformation);
}

HANDLE WINAPI CustomFindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData)
{
    static bool once = false;
    if (!once)
    {
        LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"FindFirstFileA");
        once = true;
    }

    auto ret = FindFirstFileA(lpFileName, lpFindFileData);

    sCurrentFindFileDirA = lpFileName;

    auto i = FindFileCheckOverloadedPath(lpFindFileData->cFileName);
    if (i.QuadPart)
    {
        lpFindFileData->nFileSizeHigh = i.HighPart;
        lpFindFileData->nFileSizeLow = i.LowPart;
    }

    return ret;
}

BOOL WINAPI CustomFindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData)
{
    static bool once = false;
    if (!once)
    {
        LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"FindNextFileA");
        once = true;
    }

    auto ret = FindNextFileA(hFindFile, lpFindFileData);

    auto i = FindFileCheckOverloadedPath(lpFindFileData->cFileName);
    if (i.QuadPart)
    {
        lpFindFileData->nFileSizeHigh = i.HighPart;
        lpFindFileData->nFileSizeLow = i.LowPart;
    }

    return ret;
}

HANDLE WINAPI CustomFindFirstFileW(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData)
{
    static bool once = false;
    if (!once)
    {
        LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"FindFirstFileW");
        once = true;
    }

    auto ret = FindFirstFileW(lpFileName, lpFindFileData);

    sCurrentFindFileDirW = lpFileName;

    auto i = FindFileCheckOverloadedPath(lpFindFileData->cFileName);
    if (i.QuadPart)
    {
        lpFindFileData->nFileSizeHigh = i.HighPart;
        lpFindFileData->nFileSizeLow = i.LowPart;
    }

    return ret;
}

BOOL WINAPI CustomFindNextFileW(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData)
{
    static bool once = false;
    if (!once)
    {
        LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"FindNextFileW");
        once = true;
    }

    auto ret = FindNextFileW(hFindFile, lpFindFileData);

    auto i = FindFileCheckOverloadedPath(lpFindFileData->cFileName);
    if (i.QuadPart)
    {
        lpFindFileData->nFileSizeHigh = i.HighPart;
        lpFindFileData->nFileSizeLow = i.LowPart;
    }

    return ret;
}

HANDLE WINAPI CustomFindFirstFileExA(LPCSTR lpFileName, FINDEX_INFO_LEVELS fInfoLevelId, WIN32_FIND_DATAA* lpFindFileData, FINDEX_SEARCH_OPS fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags)
{
    static bool once = false;
    if (!once)
    {
        LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"FindFirstFileExA");
        once = true;
    }

    auto ret = FindFirstFileExA(lpFileName, fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags);

    if (fInfoLevelId != FindExInfoMaxInfoLevel)
    {
        sCurrentFindFileDirA = lpFileName;

        auto i = FindFileCheckOverloadedPath(lpFindFileData->cFileName);
        if (i.QuadPart)
        {
            lpFindFileData->nFileSizeHigh = i.HighPart;
            lpFindFileData->nFileSizeLow = i.LowPart;
        }
    }

    return ret;
}

HANDLE WINAPI CustomFindFirstFileExW(LPCWSTR lpFileName, FINDEX_INFO_LEVELS fInfoLevelId, WIN32_FIND_DATAW* lpFindFileData, FINDEX_SEARCH_OPS fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags)
{
    static bool once = false;
    if (!once)
    {
        LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"FindFirstFileExW");
        once = true;
    }

    auto ret = FindFirstFileExW(lpFileName, fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags);

    if (fInfoLevelId != FindExInfoMaxInfoLevel)
    {
        sCurrentFindFileDirW = lpFileName;

        auto i = FindFileCheckOverloadedPath(lpFindFileData->cFileName);
        if (i.QuadPart)
        {
            lpFindFileData->nFileSizeHigh = i.HighPart;
            lpFindFileData->nFileSizeLow = i.LowPart;
        }
    }

    return ret;
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

    if (hDll == NULL || GetProcAddress(hDll, "IsUltimateASILoader") != NULL)
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

namespace OverloadFromFolder
{
    template<typename Callable>
    using ReturnType = typename decltype(std::function{ std::declval<Callable>() })::result_type;

    bool isRecursive(auto addr)
    {
        HMODULE hModule = NULL;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCTSTR)addr, &hModule);
        if (hModule == hm)
            return true;
        return false;
    }

    std::filesystem::path GetFilePathForOverload(auto& lpLibFileName, bool bRecursive)
    {
        if (bRecursive)
            return {};

        return ::GetFilePathForOverload(lpLibFileName);
    }

    HMODULE WINAPI shCustomLoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
    {
        auto raddr = _ReturnAddress();
        auto r = GetFilePathForOverload(lpLibFileName, isRecursive(raddr));
        return shLoadLibraryExA.unsafe_stdcall<ReturnType<decltype(LoadLibraryExA)>>(value_orA(r, lpLibFileName), hFile, dwFlags);
    }

    HMODULE WINAPI shCustomLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
    {
        auto raddr = _ReturnAddress();
        auto r = GetFilePathForOverload(lpLibFileName, isRecursive(raddr));
        return shLoadLibraryExW.unsafe_stdcall<ReturnType<decltype(LoadLibraryExW)>>(value_orW(r, lpLibFileName), hFile, dwFlags);
    }

    HANDLE WINAPI shCustomCreateFileA(LPCSTR lpFileName, DWORD dwAccess, DWORD dwSharing, LPSECURITY_ATTRIBUTES saAttributes, DWORD dwCreation, DWORD dwAttributes, HANDLE hTemplate)
    {
        auto raddr = _ReturnAddress();
        auto r = GetFilePathForOverload(lpFileName, isRecursive(raddr));
        return shCreateFileA.unsafe_stdcall<ReturnType<decltype(CreateFileA)>>(value_orA(r, lpFileName), dwAccess, dwSharing, saAttributes, dwCreation, dwAttributes, hTemplate);
    }

    HANDLE WINAPI shCustomCreateFileW(LPCWSTR lpFileName, DWORD dwAccess, DWORD dwSharing, LPSECURITY_ATTRIBUTES saAttributes, DWORD dwCreation, DWORD dwAttributes, HANDLE hTemplate)
    {
        auto raddr = _ReturnAddress();
        auto r = GetFilePathForOverload(lpFileName, isRecursive(raddr));
        return shCreateFileW.unsafe_stdcall<ReturnType<decltype(CreateFileW)>>(value_orW(r, lpFileName), dwAccess, dwSharing, saAttributes, dwCreation, dwAttributes, hTemplate);
    }

    DWORD WINAPI shCustomGetFileAttributesA(LPCSTR lpFileName)
    {
        auto raddr = _ReturnAddress();
        auto r = GetFilePathForOverload(lpFileName, isRecursive(raddr));
        return shGetFileAttributesA.unsafe_stdcall<ReturnType<decltype(GetFileAttributesA)>>(value_orA(r, lpFileName));
    }

    DWORD WINAPI shCustomGetFileAttributesW(LPCWSTR lpFileName)
    {
        auto raddr = _ReturnAddress();
        auto r = GetFilePathForOverload(lpFileName, isRecursive(raddr));
        return shGetFileAttributesW.unsafe_stdcall<ReturnType<decltype(GetFileAttributesW)>>(value_orW(r, lpFileName));
    }

    BOOL WINAPI shCustomGetFileAttributesExA(LPCSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation)
    {
        auto raddr = _ReturnAddress();
        auto r = GetFilePathForOverload(lpFileName, isRecursive(raddr));
        return shGetFileAttributesExA.unsafe_stdcall<ReturnType<decltype(GetFileAttributesExA)>>(value_orA(r, lpFileName), fInfoLevelId, lpFileInformation);
    }

    BOOL WINAPI shCustomGetFileAttributesExW(LPCWSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation)
    {
        auto raddr = _ReturnAddress();
        auto r = GetFilePathForOverload(lpFileName, isRecursive(raddr));
        return shGetFileAttributesExW.unsafe_stdcall<ReturnType<decltype(GetFileAttributesExW)>>(value_orW(r, lpFileName), fInfoLevelId, lpFileInformation);
    }

    typedef HANDLE(WINAPI* tFindFirstFileA)(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData);
    HANDLE WINAPI shCustomFindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData)
    {
        auto raddr = _ReturnAddress();
        auto ret = shFindFirstFileA.unsafe_stdcall<ReturnType<decltype(FindFirstFileA)>>(lpFileName, lpFindFileData);

        if (isRecursive(raddr))
            return ret;

        if (!sFileLoaderPath.empty())
        {
            sCurrentFindFileDirA = lpFileName;

            auto i = FindFileCheckOverloadedPath(lpFindFileData->cFileName);
            if (i.QuadPart)
            {
                lpFindFileData->nFileSizeHigh = i.HighPart;
                lpFindFileData->nFileSizeLow = i.LowPart;
            }
        }

        return ret;
    }

    typedef BOOL(WINAPI* tFindNextFileA)(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData);
    BOOL WINAPI shCustomFindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData)
    {
        auto raddr = _ReturnAddress();
        auto ret = shFindNextFileA.unsafe_stdcall<ReturnType<decltype(FindNextFileA)>>(hFindFile, lpFindFileData);

        if (isRecursive(raddr))
            return ret;

        if (!sFileLoaderPath.empty())
        {
            auto i = FindFileCheckOverloadedPath(lpFindFileData->cFileName);
            if (i.QuadPart)
            {
                lpFindFileData->nFileSizeHigh = i.HighPart;
                lpFindFileData->nFileSizeLow = i.LowPart;
            }
        }

        return ret;
    }

    typedef HANDLE(WINAPI* tFindFirstFileW)(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData);
    HANDLE WINAPI shCustomFindFirstFileW(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData)
    {
        auto raddr = _ReturnAddress();
        auto ret = shFindFirstFileW.unsafe_stdcall<ReturnType<decltype(FindFirstFileW)>>(lpFileName, lpFindFileData);

        if (isRecursive(raddr))
            return ret;

        if (!sFileLoaderPath.empty())
        {
            sCurrentFindFileDirW = lpFileName;

            auto i = FindFileCheckOverloadedPath(lpFindFileData->cFileName);
            if (i.QuadPart)
            {
                lpFindFileData->nFileSizeHigh = i.HighPart;
                lpFindFileData->nFileSizeLow = i.LowPart;
            }
        }

        return ret;
    }

    typedef BOOL(WINAPI* tFindNextFileW)(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData);
    BOOL WINAPI shCustomFindNextFileW(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData)
    {
        auto raddr = _ReturnAddress();
        auto ret = shFindNextFileW.unsafe_stdcall<ReturnType<decltype(FindNextFileW)>>(hFindFile, lpFindFileData);

        if (isRecursive(raddr))
            return ret;

        if (!sFileLoaderPath.empty())
        {
            auto i = FindFileCheckOverloadedPath(lpFindFileData->cFileName);
            if (i.QuadPart)
            {
                lpFindFileData->nFileSizeHigh = i.HighPart;
                lpFindFileData->nFileSizeLow = i.LowPart;
            }
        }

        return ret;
    }

    HANDLE WINAPI shCustomFindFirstFileExA(LPCSTR lpFileName, FINDEX_INFO_LEVELS fInfoLevelId, WIN32_FIND_DATAA* lpFindFileData, FINDEX_SEARCH_OPS fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags)
    {
        auto raddr = _ReturnAddress();
        auto ret = shFindFirstFileExA.unsafe_stdcall<ReturnType<decltype(FindFirstFileExA)>>(lpFileName, fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags);

        if (isRecursive(raddr))
            return ret;

        if (fInfoLevelId != FindExInfoMaxInfoLevel && !sFileLoaderPath.empty())
        {
            sCurrentFindFileDirA = lpFileName;

            auto i = FindFileCheckOverloadedPath(lpFindFileData->cFileName);
            if (i.QuadPart)
            {
                lpFindFileData->nFileSizeHigh = i.HighPart;
                lpFindFileData->nFileSizeLow = i.LowPart;
            }
        }

        return ret;
    }

    HANDLE WINAPI shCustomFindFirstFileExW(LPCWSTR lpFileName, FINDEX_INFO_LEVELS fInfoLevelId, WIN32_FIND_DATAW* lpFindFileData, FINDEX_SEARCH_OPS fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags)
    {
        auto raddr = _ReturnAddress();
        auto ret = shFindFirstFileExW.unsafe_stdcall<ReturnType<decltype(FindFirstFileExW)>>(lpFileName, fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags);

        if (isRecursive(raddr))
            return ret;

        if (fInfoLevelId != FindExInfoMaxInfoLevel && !sFileLoaderPath.empty())
        {
            sCurrentFindFileDirW = lpFileName;

            auto i = FindFileCheckOverloadedPath(lpFindFileData->cFileName);
            if (i.QuadPart)
            {
                lpFindFileData->nFileSizeHigh = i.HighPart;
                lpFindFileData->nFileSizeLow = i.LowPart;
            }
        }

        return ret;
    }

    void HookAPIForOverload()
    {
        shLoadLibraryExA = safetyhook::create_inline(LoadLibraryExA, shCustomLoadLibraryExA);
        shLoadLibraryExW = safetyhook::create_inline(LoadLibraryExW, shCustomLoadLibraryExW);
        shCreateFileA = safetyhook::create_inline(CreateFileA, shCustomCreateFileA);
        shCreateFileW = safetyhook::create_inline(CreateFileW, shCustomCreateFileW);
        shGetFileAttributesA = safetyhook::create_inline(GetFileAttributesA, shCustomGetFileAttributesA);
        shGetFileAttributesW = safetyhook::create_inline(GetFileAttributesW, shCustomGetFileAttributesW);
        shGetFileAttributesExA = safetyhook::create_inline(GetFileAttributesExA, shCustomGetFileAttributesExA);
        shGetFileAttributesExW = safetyhook::create_inline(GetFileAttributesExW, shCustomGetFileAttributesExW);
        shFindFirstFileA = safetyhook::create_inline(FindFirstFileA, shCustomFindFirstFileA);
        shFindNextFileA = safetyhook::create_inline(FindNextFileA, shCustomFindNextFileA);
        shFindFirstFileW = safetyhook::create_inline(FindFirstFileW, shCustomFindFirstFileW);
        shFindNextFileW = safetyhook::create_inline(FindNextFileW, shCustomFindNextFileW);
        shFindFirstFileExA = safetyhook::create_inline(FindFirstFileExA, shCustomFindFirstFileExA);
        shFindFirstFileExW = safetyhook::create_inline(FindFirstFileExW, shCustomFindFirstFileExW);
    }
}

std::vector<std::string> importedModulesList;
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
        Kernel32Data[eFindFirstFileA][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "FindFirstFileA");
        Kernel32Data[eFindNextFileA][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "FindNextFileA");
        Kernel32Data[eFindFirstFileW][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "FindFirstFileW");
        Kernel32Data[eFindNextFileW][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "FindNextFileW");
        Kernel32Data[eFindFirstFileExA][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "FindFirstFileExA");
        Kernel32Data[eFindFirstFileExW][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "FindFirstFileExW");
        Kernel32Data[eLoadLibraryExA][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "LoadLibraryExA");
        Kernel32Data[eLoadLibraryExW][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "LoadLibraryExW");
        Kernel32Data[eLoadLibraryA][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "LoadLibraryA");
        Kernel32Data[eLoadLibraryW][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "LoadLibraryW");
        Kernel32Data[eFreeLibrary][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "FreeLibrary");
        Kernel32Data[eCreateEventA][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "CreateEventA");
        Kernel32Data[eCreateEventW][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "CreateEventW");
        Kernel32Data[eGetSystemInfo][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "GetSystemInfo");
        Kernel32Data[eInterlockedCompareExchange][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "InterlockedCompareExchange");
        Kernel32Data[eSleep][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "Sleep");
        Kernel32Data[eGetSystemTimeAsFileTime][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "GetSystemTimeAsFileTime");
        Kernel32Data[eGetCurrentProcessId][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "GetCurrentProcessId");
        Kernel32Data[eGetCommandLineA][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "GetCommandLineA");
        Kernel32Data[eGetCommandLineW][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "GetCommandLineW");
        Kernel32Data[eAcquireSRWLockExclusive][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "AcquireSRWLockExclusive");
        Kernel32Data[eCreateFileA][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "CreateFileA");
        Kernel32Data[eCreateFileW][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "CreateFileW");
        Kernel32Data[eGetFileAttributesA][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "GetFileAttributesA");
        Kernel32Data[eGetFileAttributesW][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "GetFileAttributesW");
        Kernel32Data[eGetFileAttributesExA][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "GetFileAttributesExA");
        Kernel32Data[eGetFileAttributesExW][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "GetFileAttributesExW");
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
            else if (ptr == Kernel32Data[eFindFirstFileA][ProcAddress])
            {
                if (exe) Kernel32Data[eFindFirstFileA][IATPtr] = i;
                *(size_t*)i = (size_t)CustomFindFirstFileA;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eFindNextFileA][ProcAddress])
            {
                if (exe) Kernel32Data[eFindNextFileA][IATPtr] = i;
                *(size_t*)i = (size_t)CustomFindNextFileA;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eFindFirstFileW][ProcAddress])
            {
                if (exe) Kernel32Data[eFindFirstFileW][IATPtr] = i;
                *(size_t*)i = (size_t)CustomFindFirstFileW;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eFindNextFileW][ProcAddress])
            {
                if (exe) Kernel32Data[eFindNextFileW][IATPtr] = i;
                *(size_t*)i = (size_t)CustomFindNextFileW;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eFindFirstFileExA][ProcAddress])
            {
                if (exe) Kernel32Data[eFindFirstFileExA][IATPtr] = i;
                *(size_t*)i = (size_t)CustomFindFirstFileExA;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eFindFirstFileExW][ProcAddress])
            {
                if (exe) Kernel32Data[eFindFirstFileExW][IATPtr] = i;
                *(size_t*)i = (size_t)CustomFindFirstFileExW;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eLoadLibraryExA][ProcAddress])
            {
                if (exe) Kernel32Data[eLoadLibraryExA][IATPtr] = i;
                *(size_t*)i = (size_t)CustomLoadLibraryExA;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eLoadLibraryExW][ProcAddress])
            {
                if (exe) Kernel32Data[eLoadLibraryExW][IATPtr] = i;
                *(size_t*)i = (size_t)CustomLoadLibraryExW;
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
            else if (ptr == Kernel32Data[eGetSystemTimeAsFileTime][ProcAddress])
            {
                if (exe) Kernel32Data[eGetSystemTimeAsFileTime][IATPtr] = i;
                *(size_t*)i = (size_t)CustomGetSystemTimeAsFileTime;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eGetCurrentProcessId][ProcAddress])
            {
                if (exe) Kernel32Data[eGetCurrentProcessId][IATPtr] = i;
                *(size_t*)i = (size_t)CustomGetCurrentProcessId;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eGetCommandLineA][ProcAddress])
            {
                if (exe) Kernel32Data[eGetCommandLineA][IATPtr] = i;
                *(size_t*)i = (size_t)CustomGetCommandLineA;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eGetCommandLineW][ProcAddress])
            {
                if (exe) Kernel32Data[eGetCommandLineW][IATPtr] = i;
                *(size_t*)i = (size_t)CustomGetCommandLineW;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eAcquireSRWLockExclusive][ProcAddress])
            {
                if (exe) Kernel32Data[eAcquireSRWLockExclusive][IATPtr] = i;
                *(size_t*)i = (size_t)CustomAcquireSRWLockExclusive;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eCreateFileA][ProcAddress])
            {
                if (exe) Kernel32Data[eCreateFileA][IATPtr] = i;
                *(size_t*)i = (size_t)CustomCreateFileA;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eCreateFileW][ProcAddress])
            {
                if (exe) Kernel32Data[eCreateFileW][IATPtr] = i;
                *(size_t*)i = (size_t)CustomCreateFileW;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eGetFileAttributesA][ProcAddress])
            {
                if (exe) Kernel32Data[eGetFileAttributesA][IATPtr] = i;
                *(size_t*)i = (size_t)CustomGetFileAttributesA;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eGetFileAttributesW][ProcAddress])
            {
                if (exe) Kernel32Data[eGetFileAttributesW][IATPtr] = i;
                *(size_t*)i = (size_t)CustomGetFileAttributesW;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eGetFileAttributesExA][ProcAddress])
            {
                if (exe) Kernel32Data[eGetFileAttributesExA][IATPtr] = i;
                *(size_t*)i = (size_t)CustomGetFileAttributesExA;
                matchedImports++;
            }
            else if (ptr == Kernel32Data[eGetFileAttributesExW][ProcAddress])
            {
                if (exe) Kernel32Data[eGetFileAttributesExW][IATPtr] = i;
                *(size_t*)i = (size_t)CustomGetFileAttributesExW;
                matchedImports++;
            }

            VirtualProtect((size_t*)i, sizeof(size_t), dwProtect[0], &dwProtect[1]);
        }
    };

    auto PatchCoCreateInstance = [&](size_t start, size_t end, size_t exe_end)
    {
        if (iequals(GetSelfName(), L"dinput8.dll") || iequals(GetSelfName(), L"dinput.dll") || iequals(GetSelfName(), L"wininet.dll"))
            return;

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

        if (exe)
            OLE32Data[eCoCreateInstance][ProcAddress] = (size_t)GetProcAddress(GetModuleHandle(TEXT("OLE32.DLL")), "CoCreateInstance");

        for (auto i = start; i < end; i += sizeof(size_t))
        {
            DWORD dwProtect[2];
            VirtualProtect((size_t*)i, sizeof(size_t), PAGE_EXECUTE_READWRITE, &dwProtect[0]);

            auto ptr = *(size_t*)i;
            if (!ptr)
                continue;

            if (ptr == OLE32Data[eCoCreateInstance][ProcAddress])
            {
                if (exe) OLE32Data[eCoCreateInstance][IATPtr] = i;
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
            importedModulesList.emplace_back((const char*)(hExecutableInstance + (pImports + i)->Name));
        }
    }

    // Fixing ordinals
    auto szSelfName = GetSelfName();

    static auto PatchOrdinals = [&szSelfName](size_t hInstance)
    {
        IMAGE_NT_HEADERS*           ntHeader = (IMAGE_NT_HEADERS*)(hInstance + ((IMAGE_DOS_HEADER*)hInstance)->e_lfanew);
        IMAGE_IMPORT_DESCRIPTOR*    pImports = (IMAGE_IMPORT_DESCRIPTOR*)(hInstance + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
        size_t                      nNumImports = ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size / sizeof(IMAGE_IMPORT_DESCRIPTOR) - 1;

        if (nNumImports == (size_t)-1)
            return;

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
                            else if (iequals(szSelfName, L"winhttp.dll"))
                            {
                                DWORD Protect;
                                VirtualProtect(&p[j], 4, PAGE_EXECUTE_READWRITE, &Protect);

                                const enum ewinhttp
                                {
                                    Private1 = 4,
                                    SvchostPushServiceGlobals = 5,
                                    WinHttpAddRequestHeaders = 6,
                                    WinHttpAddRequestHeadersEx = 7,
                                    WinHttpAutoProxySvcMain = 8,
                                    WinHttpCheckPlatform = 9,
                                    WinHttpCloseHandle = 10,
                                    WinHttpConnect = 11,
                                    WinHttpConnectionDeletePolicyEntries = 12,
                                    WinHttpConnectionDeleteProxyInfo = 13,
                                    WinHttpConnectionFreeNameList = 14,
                                    WinHttpConnectionFreeProxyInfo = 15,
                                    WinHttpConnectionFreeProxyList = 16,
                                    WinHttpConnectionGetNameList = 17,
                                    WinHttpConnectionGetProxyInfo = 18,
                                    WinHttpConnectionGetProxyList = 19,
                                    WinHttpConnectionOnlyConvert = 20,
                                    WinHttpConnectionOnlyReceive = 21,
                                    WinHttpConnectionOnlySend = 22,
                                    WinHttpConnectionSetPolicyEntries = 23,
                                    WinHttpConnectionSetProxyInfo = 24,
                                    WinHttpConnectionUpdateIfIndexTable = 25,
                                    WinHttpCrackUrl = 26,
                                    WinHttpCreateProxyResolver = 27,
                                    WinHttpCreateUrl = 28,
                                    WinHttpDetectAutoProxyConfigUrl = 29,
                                    WinHttpFreeProxyResult = 30,
                                    WinHttpFreeProxyResultEx = 31,
                                    WinHttpFreeProxySettings = 32,
                                    WinHttpFreeProxySettingsEx = 33,
                                    WinHttpFreeQueryConnectionGroupResult = 34,
                                    WinHttpGetDefaultProxyConfiguration = 35,
                                    WinHttpGetIEProxyConfigForCurrentUser = 36,
                                    WinHttpGetProxyForUrl = 37,
                                    WinHttpGetProxyForUrlEx = 38,
                                    WinHttpGetProxyForUrlEx2 = 39,
                                    WinHttpGetProxyForUrlHvsi = 40,
                                    WinHttpGetProxyResult = 41,
                                    WinHttpGetProxyResultEx = 42,
                                    WinHttpGetProxySettingsEx = 43,
                                    WinHttpGetProxySettingsResultEx = 44,
                                    WinHttpGetProxySettingsVersion = 45,
                                    WinHttpGetTunnelSocket = 46,
                                    WinHttpOpen = 47,
                                    WinHttpOpenRequest = 48,
                                    WinHttpPacJsWorkerMain = 49,
                                    WinHttpProbeConnectivity = 50,
                                    WinHttpQueryAuthSchemes = 51,
                                    WinHttpQueryConnectionGroup = 52,
                                    WinHttpQueryDataAvailable = 53,
                                    WinHttpQueryHeaders = 54,
                                    WinHttpQueryHeadersEx = 55,
                                    WinHttpQueryOption = 56,
                                    WinHttpReadData = 57,
                                    WinHttpReadDataEx = 58,
                                    WinHttpReadProxySettings = 59,
                                    WinHttpReadProxySettingsHvsi = 60,
                                    WinHttpReceiveResponse = 61,
                                    WinHttpRegisterProxyChangeNotification = 62,
                                    WinHttpResetAutoProxy = 63,
                                    WinHttpSaveProxyCredentials = 64,
                                    WinHttpSendRequest = 65,
                                    WinHttpSetCredentials = 66,
                                    WinHttpSetDefaultProxyConfiguration = 67,
                                    WinHttpSetOption = 68,
                                    WinHttpSetProxySettingsPerUser = 69,
                                    WinHttpSetSecureLegacyServersAppCompat = 1,
                                    WinHttpSetStatusCallback = 70,
                                    WinHttpSetTimeouts = 71,
                                    WinHttpTimeFromSystemTime = 72,
                                    WinHttpTimeToSystemTime = 73,
                                    WinHttpUnregisterProxyChangeNotification = 74,
                                    WinHttpWebSocketClose = 75,
                                    WinHttpWebSocketCompleteUpgrade = 76,
                                    WinHttpWebSocketQueryCloseStatus = 77,
                                    WinHttpWebSocketReceive = 78,
                                    WinHttpWebSocketSend = 79,
                                    WinHttpWebSocketShutdown = 80,
                                    WinHttpWriteData = 81,
                                    WinHttpWriteProxySettings = 82
                                };

                                switch (IMAGE_ORDINAL(thunk->u1.Ordinal))
                                {
                                case ewinhttp::Private1:
                                    p[j] = _Private1;
                                        break;
                                case ewinhttp::SvchostPushServiceGlobals:
                                    p[j] = _SvchostPushServiceGlobals;
                                        break;
                                case ewinhttp::WinHttpAddRequestHeaders:
                                    p[j] = _WinHttpAddRequestHeaders;
                                        break;
                                case ewinhttp::WinHttpAddRequestHeadersEx:
                                    p[j] = _WinHttpAddRequestHeadersEx;
                                        break;
                                case ewinhttp::WinHttpAutoProxySvcMain:
                                    p[j] = _WinHttpAutoProxySvcMain;
                                        break;
                                case ewinhttp::WinHttpCheckPlatform:
                                    p[j] = _WinHttpCheckPlatform;
                                        break;
                                case ewinhttp::WinHttpCloseHandle:
                                    p[j] = _WinHttpCloseHandle;
                                        break;
                                case ewinhttp::WinHttpConnect:
                                    p[j] = _WinHttpConnect;
                                        break;
                                case ewinhttp::WinHttpConnectionDeletePolicyEntries:
                                    p[j] = _WinHttpConnectionDeletePolicyEntries;
                                        break;
                                case ewinhttp::WinHttpConnectionDeleteProxyInfo:
                                    p[j] = _WinHttpConnectionDeleteProxyInfo;
                                        break;
                                case ewinhttp::WinHttpConnectionFreeNameList:
                                    p[j] = _WinHttpConnectionFreeNameList;
                                        break;
                                case ewinhttp::WinHttpConnectionFreeProxyInfo:
                                    p[j] = _WinHttpConnectionFreeProxyInfo;
                                        break;
                                case ewinhttp::WinHttpConnectionFreeProxyList:
                                    p[j] = _WinHttpConnectionFreeProxyList;
                                        break;
                                case ewinhttp::WinHttpConnectionGetNameList:
                                    p[j] = _WinHttpConnectionGetNameList;
                                        break;
                                case ewinhttp::WinHttpConnectionGetProxyInfo:
                                    p[j] = _WinHttpConnectionGetProxyInfo;
                                        break;
                                case ewinhttp::WinHttpConnectionGetProxyList:
                                    p[j] = _WinHttpConnectionGetProxyList;
                                        break;
                                case ewinhttp::WinHttpConnectionOnlyConvert:
                                    p[j] = _WinHttpConnectionOnlyConvert;
                                        break;
                                case ewinhttp::WinHttpConnectionOnlyReceive:
                                    p[j] = _WinHttpConnectionOnlyReceive;
                                        break;
                                case ewinhttp::WinHttpConnectionOnlySend:
                                    p[j] = _WinHttpConnectionOnlySend;
                                        break;
                                case ewinhttp::WinHttpConnectionSetPolicyEntries:
                                    p[j] = _WinHttpConnectionSetPolicyEntries;
                                        break;
                                case ewinhttp::WinHttpConnectionSetProxyInfo:
                                    p[j] = _WinHttpConnectionSetProxyInfo;
                                        break;
                                case ewinhttp::WinHttpConnectionUpdateIfIndexTable:
                                    p[j] = _WinHttpConnectionUpdateIfIndexTable;
                                        break;
                                case ewinhttp::WinHttpCrackUrl:
                                    p[j] = _WinHttpCrackUrl;
                                        break;
                                case ewinhttp::WinHttpCreateProxyResolver:
                                    p[j] = _WinHttpCreateProxyResolver;
                                        break;
                                case ewinhttp::WinHttpCreateUrl:
                                    p[j] = _WinHttpCreateUrl;
                                        break;
                                case ewinhttp::WinHttpDetectAutoProxyConfigUrl:
                                    p[j] = _WinHttpDetectAutoProxyConfigUrl;
                                        break;
                                case ewinhttp::WinHttpFreeProxyResult:
                                    p[j] = _WinHttpFreeProxyResult;
                                        break;
                                case ewinhttp::WinHttpFreeProxyResultEx:
                                    p[j] = _WinHttpFreeProxyResultEx;
                                        break;
                                case ewinhttp::WinHttpFreeProxySettings:
                                    p[j] = _WinHttpFreeProxySettings;
                                        break;
                                case ewinhttp::WinHttpFreeProxySettingsEx:
                                    p[j] = _WinHttpFreeProxySettingsEx;
                                        break;
                                case ewinhttp::WinHttpFreeQueryConnectionGroupResult:
                                    p[j] = _WinHttpFreeQueryConnectionGroupResult;
                                        break;
                                case ewinhttp::WinHttpGetDefaultProxyConfiguration:
                                    p[j] = _WinHttpGetDefaultProxyConfiguration;
                                        break;
                                case ewinhttp::WinHttpGetIEProxyConfigForCurrentUser:
                                    p[j] = _WinHttpGetIEProxyConfigForCurrentUser;
                                        break;
                                case ewinhttp::WinHttpGetProxyForUrl:
                                    p[j] = _WinHttpGetProxyForUrl;
                                        break;
                                case ewinhttp::WinHttpGetProxyForUrlEx:
                                    p[j] = _WinHttpGetProxyForUrlEx;
                                        break;
                                case ewinhttp::WinHttpGetProxyForUrlEx2:
                                    p[j] = _WinHttpGetProxyForUrlEx2;
                                        break;
                                case ewinhttp::WinHttpGetProxyForUrlHvsi:
                                    p[j] = _WinHttpGetProxyForUrlHvsi;
                                        break;
                                case ewinhttp::WinHttpGetProxyResult:
                                    p[j] = _WinHttpGetProxyResult;
                                        break;
                                case ewinhttp::WinHttpGetProxyResultEx:
                                    p[j] = _WinHttpGetProxyResultEx;
                                        break;
                                case ewinhttp::WinHttpGetProxySettingsEx:
                                    p[j] = _WinHttpGetProxySettingsEx;
                                        break;
                                case ewinhttp::WinHttpGetProxySettingsResultEx:
                                    p[j] = _WinHttpGetProxySettingsResultEx;
                                        break;
                                case ewinhttp::WinHttpGetProxySettingsVersion:
                                    p[j] = _WinHttpGetProxySettingsVersion;
                                        break;
                                case ewinhttp::WinHttpGetTunnelSocket:
                                    p[j] = _WinHttpGetTunnelSocket;
                                        break;
                                case ewinhttp::WinHttpOpen:
                                    p[j] = _WinHttpOpen;
                                        break;
                                case ewinhttp::WinHttpOpenRequest:
                                    p[j] = _WinHttpOpenRequest;
                                        break;
                                case ewinhttp::WinHttpPacJsWorkerMain:
                                    p[j] = _WinHttpPacJsWorkerMain;
                                        break;
                                case ewinhttp::WinHttpProbeConnectivity:
                                    p[j] = _WinHttpProbeConnectivity;
                                        break;
                                case ewinhttp::WinHttpQueryAuthSchemes:
                                    p[j] = _WinHttpQueryAuthSchemes;
                                        break;
                                case ewinhttp::WinHttpQueryConnectionGroup:
                                    p[j] = _WinHttpQueryConnectionGroup;
                                        break;
                                case ewinhttp::WinHttpQueryDataAvailable:
                                    p[j] = _WinHttpQueryDataAvailable;
                                        break;
                                case ewinhttp::WinHttpQueryHeaders:
                                    p[j] = _WinHttpQueryHeaders;
                                        break;
                                case ewinhttp::WinHttpQueryHeadersEx:
                                    p[j] = _WinHttpQueryHeadersEx;
                                        break;
                                case ewinhttp::WinHttpQueryOption:
                                    p[j] = _WinHttpQueryOption;
                                        break;
                                case ewinhttp::WinHttpReadData:
                                    p[j] = _WinHttpReadData;
                                        break;
                                case ewinhttp::WinHttpReadDataEx:
                                    p[j] = _WinHttpReadDataEx;
                                        break;
                                case ewinhttp::WinHttpReadProxySettings:
                                    p[j] = _WinHttpReadProxySettings;
                                        break;
                                case ewinhttp::WinHttpReadProxySettingsHvsi:
                                    p[j] = _WinHttpReadProxySettingsHvsi;
                                        break;
                                case ewinhttp::WinHttpReceiveResponse:
                                    p[j] = _WinHttpReceiveResponse;
                                        break;
                                case ewinhttp::WinHttpRegisterProxyChangeNotification:
                                    p[j] = _WinHttpRegisterProxyChangeNotification;
                                        break;
                                case ewinhttp::WinHttpResetAutoProxy:
                                    p[j] = _WinHttpResetAutoProxy;
                                        break;
                                case ewinhttp::WinHttpSaveProxyCredentials:
                                    p[j] = _WinHttpSaveProxyCredentials;
                                        break;
                                case ewinhttp::WinHttpSendRequest:
                                    p[j] = _WinHttpSendRequest;
                                        break;
                                case ewinhttp::WinHttpSetCredentials:
                                    p[j] = _WinHttpSetCredentials;
                                        break;
                                case ewinhttp::WinHttpSetDefaultProxyConfiguration:
                                    p[j] = _WinHttpSetDefaultProxyConfiguration;
                                        break;
                                case ewinhttp::WinHttpSetOption:
                                    p[j] = _WinHttpSetOption;
                                        break;
                                case ewinhttp::WinHttpSetProxySettingsPerUser:
                                    p[j] = _WinHttpSetProxySettingsPerUser;
                                        break;
                                case ewinhttp::WinHttpSetSecureLegacyServersAppCompat:
                                    p[j] = _WinHttpSetSecureLegacyServersAppCompat;
                                        break;
                                case ewinhttp::WinHttpSetStatusCallback:
                                    p[j] = _WinHttpSetStatusCallback;
                                        break;
                                case ewinhttp::WinHttpSetTimeouts:
                                    p[j] = _WinHttpSetTimeouts;
                                        break;
                                case ewinhttp::WinHttpTimeFromSystemTime:
                                    p[j] = _WinHttpTimeFromSystemTime;
                                        break;
                                case ewinhttp::WinHttpTimeToSystemTime:
                                    p[j] = _WinHttpTimeToSystemTime;
                                        break;
                                case ewinhttp::WinHttpUnregisterProxyChangeNotification:
                                    p[j] = _WinHttpUnregisterProxyChangeNotification;
                                        break;
                                case ewinhttp::WinHttpWebSocketClose:
                                    p[j] = _WinHttpWebSocketClose;
                                        break;
                                case ewinhttp::WinHttpWebSocketCompleteUpgrade:
                                    p[j] = _WinHttpWebSocketCompleteUpgrade;
                                        break;
                                case ewinhttp::WinHttpWebSocketQueryCloseStatus:
                                    p[j] = _WinHttpWebSocketQueryCloseStatus;
                                        break;
                                case ewinhttp::WinHttpWebSocketReceive:
                                    p[j] = _WinHttpWebSocketReceive;
                                        break;
                                case ewinhttp::WinHttpWebSocketSend:
                                    p[j] = _WinHttpWebSocketSend;
                                        break;
                                case ewinhttp::WinHttpWebSocketShutdown:
                                    p[j] = _WinHttpWebSocketShutdown;
                                        break;
                                case ewinhttp::WinHttpWriteData:
                                    p[j] = _WinHttpWriteData;
                                        break;
                                case ewinhttp::WinHttpWriteProxySettings:
                                    p[j] = _WinHttpWriteProxySettings;
                                        break;
                                default:
                                    break;
                                }
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

    wchar_t* modulenameptr = nullptr;
    if (GetModuleFileNameW(GetModuleHandle(NULL), modulename, _countof(modulename)) != 0)
    {
        modulenameptr = wcsrchr(modulename, '\\');
        *modulenameptr = L'\0';
        modulenameptr += 1;
    }
    else
    {
        wcscpy_s(modulename, L"err.err");
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
    iniPaths.emplace_back(modulePath + L"update\\global.ini");

    auto nForceEPHook = GetPrivateProfileIntW(TEXT("globalsets"), TEXT("forceentrypointhook"), FALSE, iniPaths);
    auto nDontLoadFromDllMain = GetPrivateProfileIntW(TEXT("globalsets"), TEXT("dontloadfromdllmain"), TRUE, iniPaths);
    sLoadFromAPI = GetPrivateProfileStringW(TEXT("globalsets"), TEXT("loadfromapi"), L"", iniPaths);
    auto nFindModule = GetPrivateProfileIntW(TEXT("globalsets"), TEXT("findmodule"), FALSE, iniPaths);
    auto nDisableCrashDumps = GetPrivateProfileIntW(TEXT("globalsets"), TEXT("disablecrashdumps"), FALSE, iniPaths);
    sFileLoaderPath = GetPrivateProfileStringW(TEXT("fileloader"), TEXT("overloadfromfolder"), TEXT("update"), iniPaths);

    auto FolderExists = [](auto szPath) -> bool
    {
        try
        {
            auto path = std::filesystem::path(szPath);
            if (path.is_absolute())
                return std::filesystem::is_directory(path);
            else
                return std::filesystem::is_directory(std::filesystem::path(GetModuleFileNameW(NULL)).parent_path() / path);
        }
        catch (...) {}
        
        return false;
    };

    if (!nDisableCrashDumps)
    {
        if (FolderExists(L"CrashDumps"))
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

    if (!FolderExists(sFileLoaderPath))
        sFileLoaderPath.clear();
    else
        sFileLoaderPath = sFileLoaderPath.make_preferred();

    if (nForceEPHook != FALSE || nDontLoadFromDllMain != FALSE)
    {
        try
        {
            auto exeName = std::filesystem::path(GetModulePath(NULL)).stem().wstring();

            if (sLoadFromAPI.empty()) // compatibility with GTAV/RDR2 plugins
            {
                if (iequals(exeName, L"GTA5") || iequals(exeName, L"RDR2") || iequals(exeName, L"game_win64_master"))
                    sLoadFromAPI = L"GetSystemTimeAsFileTime";
            }
        }
        catch (...) {}

        HMODULE mainModule = GetModuleHandle(NULL);
        bool hookedSuccessfully = HookKernel32IAT(mainModule, true);
        if (!hookedSuccessfully)
        {
            LoadOriginalLibrary();
        }

        const auto it = std::find_if(std::begin(importedModulesList), std::end(importedModulesList), [&](const auto& str) { return iequals(L"unityplayer.dll", to_wstring(str)); } );
        const auto bUnityPlayerImported = it != std::end(importedModulesList);

        HMODULE m = mainModule;
        if (nFindModule || importedModulesList.size() <= 2 || bUnityPlayerImported)
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

                if (str2 == L"unityplayer")
                    return true;

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
    else if (reason == DLL_PROCESS_DETACH)
    {
        for (size_t i = 0; i < OLE32ExportsNamesCount; i++)
        {
            if (OLE32Data[i][IATPtr] && OLE32Data[i][ProcAddress])
            {
                auto ptr = (size_t*)OLE32Data[i][IATPtr];
                DWORD dwProtect[2];
                VirtualProtect(ptr, sizeof(size_t), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
                *ptr = OLE32Data[i][ProcAddress];
                VirtualProtect(ptr, sizeof(size_t), dwProtect[0], &dwProtect[1]);
            }
        }

        {
            using namespace OverloadFromFolder;
            shCreateFileA = {};
            shCreateFileW = {};
            shLoadLibraryExA = {};
            shLoadLibraryExW = {};
            shGetFileAttributesA = {};
            shGetFileAttributesW = {};
            shGetFileAttributesExA = {};
            shGetFileAttributesExW = {};
            shFindFirstFileA = {};
            shFindNextFileA = {};
            shFindFirstFileW = {};
            shFindNextFileW = {};
            shFindFirstFileExA = {};
            shFindFirstFileExW = {};
        }
    }
    return TRUE;
}
