#include "dllmain.h"
#include "exception.hpp"
#include <initguid.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <map>
#include <FunctionHookMinHook.hpp>
#include <shellapi.h>
#include <Commctrl.h>
#pragma comment(lib, "delayimp")
#pragma comment(lib,"Comctl32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#define _WIDEN(x) L ## x
#define WIDEN(x) _WIDEN(x)
#define ws_UpdateUrl WIDEN(rsc_UpdateUrl)
LPCWSTR szFooter = L"<a href=\"" ws_UpdateUrl "\">" ws_UpdateUrl "</a>";
constexpr auto DEFAULT_BUTTON = 1000;

#if !X64
#include <d3d8to9\source\d3d8to9.hpp>
extern "C" Direct3D8* WINAPI Direct3DCreate8(UINT SDKVersion);
#endif

extern "C" IMAGE_DOS_HEADER __ImageBase;
typedef NTSTATUS(NTAPI* LdrAddRefDll_t)(ULONG, HMODULE);

namespace OverloadFromFolder
{
    void HookAPIForOverload();
}

bool WINAPI IsUltimateASILoader()
{
    return true;
}

void* ogMemModule = NULL;
void* WINAPI GetMemoryModule()
{
    return ogMemModule;
}

bool IsPackagedProcess()
{
    using LPFN_GPFN = LONG(WINAPI*)(HANDLE, UINT32*, PWSTR);
    bool bIsPackagedProcess = false;

    auto pKernel32 = GetModuleHandle(TEXT("kernel32"));
    if (pKernel32)
    {
        auto lpGetPackageFamilyName = (LPFN_GPFN)GetProcAddress(pKernel32, "GetPackageFamilyName");
        if (lpGetPackageFamilyName)
        {
            UINT32 size = 0;
            if (lpGetPackageFamilyName(GetCurrentProcess(), &size, NULL) == ERROR_INSUFFICIENT_BUFFER)
                bIsPackagedProcess = true;
        }
    }

    return bIsPackagedProcess;
}

static bool g_userInteracted = false;
static UINT_PTR g_timerID = 1;
static int g_remainingSeconds = 10;
static HWND g_mainDialogHwnd = NULL;
static POINT g_lastMousePos = { -1, -1 };
static HHOOK g_hMouseHook = NULL;

LRESULT CALLBACK LowLevelMouseProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION && wParam == WM_MOUSEMOVE)
    {
        MSLLHOOKSTRUCT* pMouseStruct = (MSLLHOOKSTRUCT*)lParam;
        if (pMouseStruct)
        {
            if (g_lastMousePos.x != -1 && (g_lastMousePos.x != pMouseStruct->pt.x || g_lastMousePos.y != pMouseStruct->pt.y))
            {
                if (!g_userInteracted)
                {
                    g_userInteracted = true;
                    KillTimer(g_mainDialogHwnd, g_timerID);
                    SendMessage(g_mainDialogHwnd, TDM_SET_ELEMENT_TEXT, TDE_FOOTER, (LPARAM)szFooter);
                    SendMessage(g_mainDialogHwnd, TDM_SET_PROGRESS_BAR_POS, 0, 0);
                    if (g_hMouseHook)
                    {
                        UnhookWindowsHookEx(g_hMouseHook);
                        g_hMouseHook = NULL;
                    }
                }
            }
            g_lastMousePos = pMouseStruct->pt;
        }
    }
    return CallNextHookEx(g_hMouseHook, nCode, wParam, lParam);
}

HRESULT CALLBACK TaskDialogCallbackProc(HWND hwnd, UINT uNotification, WPARAM wParam, LPARAM lParam, LONG_PTR dwRefData)
{
    constexpr int countdownSeconds = 10;

    switch (uNotification)
    {
    case TDN_CREATED:
    {
        g_mainDialogHwnd = hwnd;
        g_userInteracted = false;
        g_remainingSeconds = countdownSeconds;
        g_lastMousePos = { -1, -1 };

        // Initialize progress bar
        SendMessage(hwnd, TDM_SET_PROGRESS_BAR_RANGE, 0, MAKELPARAM(0, countdownSeconds));
        SendMessage(hwnd, TDM_SET_PROGRESS_BAR_POS, countdownSeconds, 0);

        // Set timer for countdown (1000ms = 1 second)
        g_timerID = SetTimer(g_mainDialogHwnd, g_timerID, 1000, NULL);

        // Set up a low-level mouse hook to detect any mouse movement.
        g_hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, LowLevelMouseProc, GetModuleHandle(NULL), 0);

        // Create a hook to capture other messages for this thread
        SetWindowsHookEx(WH_GETMESSAGE, [](int nCode, WPARAM wParam, LPARAM lParam) -> LRESULT
        {
            if (nCode >= 0 && wParam == PM_REMOVE)
            {
                MSG* msg = (MSG*)lParam;
                switch (msg->message)
                {
                case WM_KEYDOWN:
                case WM_LBUTTONDOWN:
                case WM_RBUTTONDOWN:
                case WM_MBUTTONDOWN:
                    if (!g_userInteracted)
                    {
                        g_userInteracted = true;
                        KillTimer(g_mainDialogHwnd, g_timerID);
                        SendMessage(g_mainDialogHwnd, TDM_SET_ELEMENT_TEXT, TDE_FOOTER, (LPARAM)szFooter);
                        SendMessage(g_mainDialogHwnd, TDM_SET_PROGRESS_BAR_POS, 0, 0);
                        if (g_hMouseHook)
                        {
                            UnhookWindowsHookEx(g_hMouseHook);
                            g_hMouseHook = NULL;
                        }
                    }
                    break;
                }
            }
            return CallNextHookEx(NULL, nCode, wParam, lParam);
        }, NULL, GetCurrentThreadId());
    }
    break;
    case TDN_DESTROYED:
        if (g_hMouseHook)
        {
            UnhookWindowsHookEx(g_hMouseHook);
            g_hMouseHook = NULL;
        }
        break;
    case TDN_TIMER:
        if (g_remainingSeconds > 0 && !g_userInteracted)
        {
            g_remainingSeconds--;
            SendMessage(hwnd, TDM_SET_PROGRESS_BAR_POS, g_remainingSeconds, 0);

            std::wstring progressText = L"Auto-closing in " + std::to_wstring(g_remainingSeconds) + L" seconds...";
            SendMessage(hwnd, TDM_SET_ELEMENT_TEXT, TDE_FOOTER, (LPARAM)progressText.c_str());

            if (g_remainingSeconds == 0)
            {
                KillTimer(hwnd, g_timerID);
                SendMessage(hwnd, TDM_CLICK_BUTTON, DEFAULT_BUTTON, 0);
            }
        }
        break;
    case TDN_BUTTON_CLICKED:
        if (!g_userInteracted)
        {
            g_userInteracted = true;
            KillTimer(hwnd, g_timerID);
            SendMessage(hwnd, TDM_SET_PROGRESS_BAR_POS, 0, 0);
        }
        break;
    case TDN_HYPERLINK_CLICKED:
        ShellExecuteW(hwnd, L"open", (LPCWSTR)lParam, NULL, NULL, SW_SHOW);
        if (!g_userInteracted)
        {
            g_userInteracted = true;
            KillTimer(hwnd, g_timerID);
            SendMessage(hwnd, TDM_SET_ELEMENT_TEXT, TDE_FOOTER, (LPARAM)szFooter);
            SendMessage(hwnd, TDM_SET_PROGRESS_BAR_POS, 0, 0);
        }
        break;
    }

    return S_OK;
}

HMODULE hm;
std::vector<std::wstring> iniPaths;
std::filesystem::path sFileLoaderPath;
std::vector<std::filesystem::path> sFileLoaderPaths;
std::filesystem::path gamePath;
std::wstring sLoadFromAPI;
thread_local std::unordered_map<HANDLE, std::string> mFindFileDirsA;
thread_local std::unordered_map<HANDLE, std::wstring> mFindFileDirsW;

bool iequals(std::wstring_view s1, std::wstring_view s2)
{
    if (s1.size() != s2.size()) return false;
    return std::equal(s1.begin(), s1.end(), s2.begin(), s2.end(),
        [](wchar_t a, wchar_t b) { return ::towlower(a) == ::towlower(b); });
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

std::wstring to_wstring(std::string_view cstr)
{
    if (cstr.empty()) return std::wstring();
    auto charsReturned = MultiByteToWideChar(CP_UTF8, 0, cstr.data(), static_cast<int>(cstr.size()), nullptr, 0);
    std::wstring wstr(charsReturned, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, cstr.data(), static_cast<int>(cstr.size()), wstr.data(), charsReturned);
    return wstr;
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

enum vccorlibExportsNames
{
    eGetCmdArguments,

    vccorlibExportsNamesCount
};

size_t Kernel32Data[Kernel32ExportsNamesCount][Kernel32ExportsDataCount];
size_t OLE32Data[OLE32ExportsNamesCount][Kernel32ExportsDataCount];
size_t vccorlibData[vccorlibExportsNamesCount][Kernel32ExportsDataCount];

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
    else if (iequals(szSelfName, L"xinput1_1.dll"))
    {
        szLocalPath += L"xinput1_1Hooked.dll";
        if (std::filesystem::exists(szLocalPath))
            xinput.LoadOriginalLibrary(LoadLib(szLocalPath));
        else
            xinput.LoadOriginalLibrary(LoadLib(szSystemPath));
    }
    else if (iequals(szSelfName, L"xinput1_2.dll"))
    {
        szLocalPath += L"xinput1_2Hooked.dll";
        if (std::filesystem::exists(szLocalPath))
            xinput.LoadOriginalLibrary(LoadLib(szLocalPath));
        else
            xinput.LoadOriginalLibrary(LoadLib(szSystemPath));
    }
    else if (iequals(szSelfName, L"xinput1_3.dll"))
    {
        szLocalPath += L"xinput1_3Hooked.dll";
        if (std::filesystem::exists(szLocalPath))
            xinput.LoadOriginalLibrary(LoadLib(szLocalPath));
        else
            xinput.LoadOriginalLibrary(LoadLib(szSystemPath));
    }
    else if (iequals(szSelfName, L"XInput1_4.dll"))
    {
        szLocalPath += L"XInput1_4Hooked.dll";
        if (std::filesystem::exists(szLocalPath))
            xinput.LoadOriginalLibrary(LoadLib(szLocalPath));
        else
            xinput.LoadOriginalLibrary(LoadLib(szSystemPath));
    }
    else if (iequals(szSelfName, L"XInput9_1_0.dll"))
    {
        szLocalPath += L"XInput9_1_0Hooked.dll";
        if (std::filesystem::exists(szLocalPath))
            xinput.LoadOriginalLibrary(LoadLib(szLocalPath));
        else
            xinput.LoadOriginalLibrary(LoadLib(szSystemPath));
    }
    else if (iequals(szSelfName, L"XInputUap.dll"))
    {
        szLocalPath += L"XInputUapHooked.dll";
        if (std::filesystem::exists(szLocalPath))
            xinput.LoadOriginalLibrary(LoadLib(szLocalPath));
        else
            xinput.LoadOriginalLibrary(LoadLib(szSystemPath));
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
                                TASKDIALOG_BUTTON aCustomButtons[] = { { DEFAULT_BUTTON, L"Continue" } };

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

                                if (!IsPackagedProcess())
                                    std::ignore = TaskDialogIndirect(&tdc, &nClickedBtn, NULL, &bCheckboxChecked);
                                else
                                    MessageBoxW(NULL, szHeader, szTitle, MB_OK | MB_ICONERROR);
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

void FindPlugins(WIN32_FIND_DATAW fd,
                 const wchar_t* szPath,
                 const wchar_t* szSelfPath,
                 const UINT nWantsToLoadRecursively)
{
    std::error_code ec;

    if (!std::filesystem::exists(szPath, ec))
        return;

    // Try to find ASI in directories inside szPath
    if (nWantsToLoadRecursively)
    {
        constexpr auto perms =
          std::filesystem::directory_options::skip_permission_denied |
          std::filesystem::directory_options::follow_directory_symlink;

        for (auto& i : std::filesystem::directory_iterator(szPath, perms, ec))
        {
            if (!i.is_directory(ec))
                continue;

            SetCurrentDirectoryW(szSelfPath);

            if (SetCurrentDirectoryW(i.path().wstring().c_str()))
                FindFiles(&fd);
        }
    }

    // Try to find ASI only inside szPath, not in directories
    SetCurrentDirectoryW(szSelfPath);

    if (SetCurrentDirectoryW(szPath))
        FindFiles(&fd);

    // After all, need to restore current directory to self path
    SetCurrentDirectoryW(szSelfPath);
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
    auto nWantsToLoadRecursively = GetPrivateProfileIntW(L"globalsets", L"loadrecursively", TRUE, iniPaths);

    if (nWantsToLoadPlugins)
    {
        WIN32_FIND_DATAW fd{};
        if (!nWantsToLoadFromScriptsOnly)
        {
            SetCurrentDirectoryW(szSelfPath.c_str());
            FindFiles(&fd);
        }

        FindPlugins(
          fd, L"scripts", szSelfPath.c_str(), nWantsToLoadRecursively);

        FindPlugins(
          fd, L"plugins", szSelfPath.c_str(), nWantsToLoadRecursively);

        if (!sFileLoaderPath.empty())
        {
            FindPlugins(fd,
                        sFileLoaderPath.c_str(),
                        szSelfPath.c_str(),
                        nWantsToLoadRecursively);
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

    if (sFileLoaderPaths.size() >= 1)
    {
        sFileLoaderPath = sFileLoaderPaths.front().make_preferred();

        if (sFileLoaderPaths.size() > 1)
        {
            constexpr auto dialogMutexName = L"Global\\UltimateASILoader-FolderSelectDialog-Mutex";
            auto hDialogMutex = CreateMutexW(NULL, TRUE, dialogMutexName);

            if (hDialogMutex && GetLastError() == ERROR_ALREADY_EXISTS)
            {
                CloseHandle(hDialogMutex);
                ExitProcess(0);
            }

            int buttonId = DEFAULT_BUTTON;
            std::vector<TASKDIALOG_BUTTON> aButtons;
            aButtons.reserve(sFileLoaderPaths.size() + 1);
            std::vector<std::wstring> buttonTexts;
            buttonTexts.reserve(sFileLoaderPaths.size() + 1);
            
            std::error_code ec;
            for (const auto& path : sFileLoaderPaths)
            {
                auto header = path.wstring();
                auto updateTxtPath = path / "update.txt";

                if (std::filesystem::exists(updateTxtPath, ec))
                {
                    std::wifstream file(updateTxtPath);
                    if (file.is_open())
                    {
                        std::wstring content;
                        wchar_t buffer[101] = { 0 };
                        file.getline(buffer, 100);
                        content = buffer;
                        if (!content.empty())
                            header = content;
                    }
                }

                std::wstring buttonText = std::wstring(L"&") + header + L"\n" + std::filesystem::absolute(path, ec).wstring();
                buttonTexts.push_back(buttonText);
                aButtons.push_back({ buttonId++, buttonTexts.back().c_str() });
            }
            
            // Add a "Cancel" option
            //buttonTexts.push_back(L"Cancel");
            //aButtons.push_back({ IDCANCEL, buttonTexts.back().c_str() });
            
            // Configure the TaskDialog
            TASKDIALOGCONFIG tdc = { sizeof(TASKDIALOGCONFIG) };
            int nClickedBtn = 0;
            BOOL bCheckboxChecked = FALSE;
            LPCWSTR szTitle = L"ASI Loader";
            LPCWSTR szHeader = L"Select Override (Update) Folder";
            LPCWSTR szContent = L"Multiple folders have been specified for file overloading.\nPlease select which folder you want to use:";
            
            tdc.hwndParent = NULL;
            tdc.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ENABLE_HYPERLINKS | TDF_SIZE_TO_CONTENT | TDF_CAN_BE_MINIMIZED;
            tdc.pButtons = aButtons.data();
            tdc.cButtons = static_cast<UINT>(aButtons.size());
            tdc.pszWindowTitle = szTitle;
            tdc.pszMainIcon = TD_WARNING_ICON;
            tdc.pszMainInstruction = szHeader;
            tdc.pszContent = szContent;
            tdc.pfCallback = TaskDialogCallbackProc;
            tdc.lpCallbackData = 0;
            tdc.nDefaultButton = DEFAULT_BUTTON;
            tdc.pszFooter = szFooter;
            tdc.pszFooterIcon = TD_INFORMATION_ICON;
            
            if (!IsPackagedProcess())
            {
                // Configure the TaskDialog with progress bar and timer
                tdc.dwFlags |= TDF_SHOW_PROGRESS_BAR | TDF_CALLBACK_TIMER;
                tdc.pfCallback = TaskDialogCallbackProc;

                if (SUCCEEDED(TaskDialogIndirect(&tdc, &nClickedBtn, NULL, &bCheckboxChecked)) && nClickedBtn != IDCANCEL) {
                    int selectedIndex = nClickedBtn - DEFAULT_BUTTON;
                    auto& path = sFileLoaderPaths[selectedIndex];
                    sFileLoaderPath = path.make_preferred();
                }
            }
            else
            {
                for (auto& path : sFileLoaderPaths)
                {
                    std::wstring message = L"Multiple folders have been specified for file overloading.\nUse this folder?\n\n";
                    message += path.wstring() + L"\n" + std::filesystem::absolute(path, ec).wstring();

                    auto result = MessageBoxW(NULL, message.c_str(), szTitle, MB_YESNO | MB_ICONQUESTION);
                    if (result == IDYES)
                    {
                        sFileLoaderPath = path.make_preferred();
                        break;
                    }
                }
            }

            if (hDialogMutex)
            {
                ReleaseMutex(hDialogMutex);
                CloseHandle(hDialogMutex);
            }
        }

        OverloadFromFolder::HookAPIForOverload();

        sFileLoaderPaths.clear();
    }

    LoadEverything();
}

HMODULE LoadLib(const std::wstring& lpLibFileName)
{
    return LoadLibraryW(lpLibFileName.c_str());
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
    return LoadLibraryExA(lpLibFileName, hFile, dwFlags);
}

HMODULE WINAPI CustomLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
    LoadOriginalLibrary();
    return LoadLibraryExW(lpLibFileName, hFile, dwFlags);
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
    return CreateFileA(lpFileName, dwAccess, dwSharing, saAttributes, dwCreation, dwAttributes, hTemplate);
}

HANDLE WINAPI CustomCreateFileW(LPCWSTR lpFileName, DWORD dwAccess, DWORD dwSharing, LPSECURITY_ATTRIBUTES saAttributes, DWORD dwCreation, DWORD dwAttributes, HANDLE hTemplate)
{
    static bool once = false;
    if (!once)
    {
        LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"CreateFileW");
        once = true;
    }
    return CreateFileW(lpFileName, dwAccess, dwSharing, saAttributes, dwCreation, dwAttributes, hTemplate);
}

DWORD WINAPI CustomGetFileAttributesA(LPCSTR lpFileName)
{
    static bool once = false;
    if (!once)
    {
        LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"GetFileAttributesA");
        once = true;
    }
    return GetFileAttributesA(lpFileName);
}

DWORD WINAPI CustomGetFileAttributesW(LPCWSTR lpFileName)
{
    static bool once = false;
    if (!once)
    {
        LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"GetFileAttributesW");
        once = true;
    }
    return GetFileAttributesW(lpFileName);
}

BOOL WINAPI CustomGetFileAttributesExA(LPCSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation)
{
    static bool once = false;
    if (!once)
    {
        LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"GetFileAttributesExA");
        once = true;
    }
    return GetFileAttributesExA(lpFileName, fInfoLevelId, lpFileInformation);
}

BOOL WINAPI CustomGetFileAttributesExW(LPCWSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation)
{
    static bool once = false;
    if (!once)
    {
        LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"GetFileAttributesExW");
        once = true;
    }
    return GetFileAttributesExW(lpFileName, fInfoLevelId, lpFileInformation);
}

HANDLE WINAPI CustomFindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData)
{
    static bool once = false;
    if (!once)
    {
        LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"FindFirstFileA");
        once = true;
    }
    return FindFirstFileA(lpFileName, lpFindFileData);
}

BOOL WINAPI CustomFindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData)
{
    static bool once = false;
    if (!once)
    {
        LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"FindNextFileA");
        once = true;
    }
    return FindNextFileA(hFindFile, lpFindFileData);
}

HANDLE WINAPI CustomFindFirstFileW(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData)
{
    static bool once = false;
    if (!once)
    {
        LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"FindFirstFileW");
        once = true;
    }
    return FindFirstFileW(lpFileName, lpFindFileData);
}

BOOL WINAPI CustomFindNextFileW(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData)
{
    static bool once = false;
    if (!once)
    {
        LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"FindNextFileW");
        once = true;
    }
    return FindNextFileW(hFindFile, lpFindFileData);
}

HANDLE WINAPI CustomFindFirstFileExA(LPCSTR lpFileName, FINDEX_INFO_LEVELS fInfoLevelId, WIN32_FIND_DATAA* lpFindFileData, FINDEX_SEARCH_OPS fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags)
{
    static bool once = false;
    if (!once)
    {
        LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"FindFirstFileExA");
        once = true;
    }
    return FindFirstFileExA(lpFileName, fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags);
}

HANDLE WINAPI CustomFindFirstFileExW(LPCWSTR lpFileName, FINDEX_INFO_LEVELS fInfoLevelId, WIN32_FIND_DATAW* lpFindFileData, FINDEX_SEARCH_OPS fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags)
{
    static bool once = false;
    if (!once)
    {
        LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"FindFirstFileExW");
        once = true;
    }
    return FindFirstFileExW(lpFileName, fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags);
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

LPWSTR WINAPI CustomGetCmdArguments(int* argc)
{
    auto fnGetCmdArguments = reinterpret_cast<decltype(&CustomGetCmdArguments)>(vccorlibData[eGetCmdArguments][ProcAddress]);
    LoadPluginsAndRestoreIAT((uintptr_t)_ReturnAddress(), L"GetCmdArguments");
    return fnGetCmdArguments(argc);
}

namespace OverloadFromFolder
{
    std::unique_ptr<FunctionHookMinHook> mhLoadLibraryExA = { nullptr };
    std::unique_ptr<FunctionHookMinHook> mhLoadLibraryExW = { nullptr };
    std::unique_ptr<FunctionHookMinHook> mhCreateFileA = { nullptr };
    std::unique_ptr<FunctionHookMinHook> mhCreateFileW = { nullptr };
    std::unique_ptr<FunctionHookMinHook> mhCreateFile2 = { nullptr };
    std::unique_ptr<FunctionHookMinHook> mhCreateFile3 = { nullptr };
    std::unique_ptr<FunctionHookMinHook> mhGetFileAttributesA = { nullptr };
    std::unique_ptr<FunctionHookMinHook> mhGetFileAttributesW = { nullptr };
    std::unique_ptr<FunctionHookMinHook> mhGetFileAttributesExA = { nullptr };
    std::unique_ptr<FunctionHookMinHook> mhGetFileAttributesExW = { nullptr };
    std::unique_ptr<FunctionHookMinHook> mhFindFirstFileA = { nullptr };
    std::unique_ptr<FunctionHookMinHook> mhFindNextFileA = { nullptr };
    std::unique_ptr<FunctionHookMinHook> mhFindFirstFileW = { nullptr };
    std::unique_ptr<FunctionHookMinHook> mhFindNextFileW = { nullptr };
    std::unique_ptr<FunctionHookMinHook> mhFindFirstFileExA = { nullptr };
    std::unique_ptr<FunctionHookMinHook> mhFindFirstFileExW = { nullptr };
    std::unique_ptr<FunctionHookMinHook> mhFindClose = { nullptr };

    std::filesystem::path WINAPI GetOverloadedFilePath(std::filesystem::path lpFilename)
    {
        try
        {
            std::error_code ec;

            static auto starts_with = [](const std::filesystem::path& path, const std::filesystem::path& base) -> bool
            {
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

            if (*relativePath.begin() == sFileLoaderPath)
                return {};

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

    bool WINAPI GetOverloadedFilePathA(const char* lpFilename, char* out, size_t out_size)
    {
        try
        {
            if (!sFileLoaderPath.empty())
            {
                auto path = GetOverloadedFilePath(lpFilename);
                if (!path.empty())
                {
                    if (out && out_size)
                    {
                        if (!std::filesystem::path(lpFilename).is_absolute())
                            path = lexicallyRelativeCaseIns(path, gamePath);
                        out[path.string().copy(out, out_size, 0)] = '\0';
                    }
                    return true;
                }
            }
        }
        catch (...) {}
        return false;
    }

    bool WINAPI GetOverloadedFilePathW(const wchar_t* lpFilename, wchar_t* out, size_t out_size)
    {
        try
        {
            if (!sFileLoaderPath.empty())
            {
                auto path = GetOverloadedFilePath(lpFilename);
                if (!path.empty())
                {
                    if (out && out_size)
                    {
                        if (!std::filesystem::path(lpFilename).is_absolute())
                            path = lexicallyRelativeCaseIns(path, gamePath);
                        out[path.wstring().copy(out, out_size, 0)] = L'\0';
                    }
                    return true;
                }
            }
        }
        catch (...) {}
        return false;
    }

    bool WINAPI GetOverloadPathA(char* out, size_t out_size)
    {
        try
        {
            if (!sFileLoaderPath.empty())
            {
                if (out && out_size)
                {
                    auto path = gamePath / sFileLoaderPath;
                    out[path.string().copy(out, out_size, 0)] = '\0';
                    return true;
                }
            }
        }
        catch (...) {}
        return false;
    }

    bool WINAPI GetOverloadPathW(wchar_t* out, size_t out_size)
    {
        try
        {
            if (!sFileLoaderPath.empty())
            {
                if (out && out_size)
                {
                    auto path = gamePath / sFileLoaderPath;
                    out[path.wstring().copy(out, out_size, 0)] = L'\0';
                    return true;
                }
            }
        }
        catch (...) {}
        return false;
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

    void FindFileCheckOverloadedPath(auto dir, auto lpFindFileData, auto filename)
    {
        if (!filename || filename[0] == 0)
            return;

        auto fullpath = std::filesystem::path(dir).remove_filename() / filename;
        auto name = fullpath.filename();
        if (name == "." || name == ".." || name == "*" || name == "?")
            return;

        auto newPath = GetFilePathForOverload(fullpath);

        std::error_code ec;
        if (!newPath.empty() && std::filesystem::is_regular_file(newPath, ec))
        {
            lpFindFileData->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
            auto fileSize = std::filesystem::file_size(newPath, ec);
            lpFindFileData->nFileSizeHigh = static_cast<DWORD>(fileSize >> 32);
            lpFindFileData->nFileSizeLow = static_cast<DWORD>(fileSize & 0xFFFFFFFF);
            //lpFindFileData->ftCreationTime;
            //lpFindFileData->ftLastAccessTime;
            //lpFindFileData->ftLastWriteTime;
        }
    }

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

        return GetFilePathForOverload(lpLibFileName);
    }

    HMODULE WINAPI shCustomLoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
    {
        auto raddr = _ReturnAddress();
        auto r = GetFilePathForOverload(lpLibFileName, isRecursive(raddr));
        return mhLoadLibraryExA->get_original<decltype(LoadLibraryExA)>()(value_orA(r, lpLibFileName), hFile, dwFlags);
    }

    HMODULE WINAPI shCustomLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
    {
        auto raddr = _ReturnAddress();
        auto r = GetFilePathForOverload(lpLibFileName, isRecursive(raddr));
        return mhLoadLibraryExW->get_original<decltype(LoadLibraryExW)>()(value_orW(r, lpLibFileName), hFile, dwFlags);
    }

    HANDLE WINAPI shCustomCreateFileA(LPCSTR lpFileName, DWORD dwAccess, DWORD dwSharing, LPSECURITY_ATTRIBUTES saAttributes, DWORD dwCreation, DWORD dwAttributes, HANDLE hTemplate)
    {
        auto raddr = _ReturnAddress();
        auto r = GetFilePathForOverload(lpFileName, isRecursive(raddr));
        return mhCreateFileA->get_original<decltype(CreateFileA)>()(value_orA(r, lpFileName), dwAccess, dwSharing, saAttributes, dwCreation, dwAttributes, hTemplate);
    }

    HANDLE WINAPI shCustomCreateFileW(LPCWSTR lpFileName, DWORD dwAccess, DWORD dwSharing, LPSECURITY_ATTRIBUTES saAttributes, DWORD dwCreation, DWORD dwAttributes, HANDLE hTemplate)
    {
        auto raddr = _ReturnAddress();
        auto r = GetFilePathForOverload(lpFileName, isRecursive(raddr));
        return mhCreateFileW->get_original<decltype(CreateFileW)>()(value_orW(r, lpFileName), dwAccess, dwSharing, saAttributes, dwCreation, dwAttributes, hTemplate);
    }

    HANDLE WINAPI shCustomCreateFile2(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, DWORD dwCreationDisposition, LPCREATEFILE2_EXTENDED_PARAMETERS pCreateExParams)
    {
        auto raddr = _ReturnAddress();
        auto r = GetFilePathForOverload(lpFileName, isRecursive(raddr));
        return mhCreateFile2->get_original<decltype(shCustomCreateFile2)>()(value_orW(r, lpFileName), dwDesiredAccess, dwShareMode, dwCreationDisposition, pCreateExParams);
    }

    HANDLE WINAPI shCustomCreateFile3(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, DWORD dwCreationDisposition, LPCREATEFILE3_EXTENDED_PARAMETERS pCreateExParams)
    {
        auto raddr = _ReturnAddress();
        auto r = GetFilePathForOverload(lpFileName, isRecursive(raddr));
        return mhCreateFile3->get_original<decltype(shCustomCreateFile3)>()(value_orW(r, lpFileName), dwDesiredAccess, dwShareMode, dwCreationDisposition, pCreateExParams);
    }

    DWORD WINAPI shCustomGetFileAttributesA(LPCSTR lpFileName)
    {
        auto raddr = _ReturnAddress();
        auto r = GetFilePathForOverload(lpFileName, isRecursive(raddr));
        return mhGetFileAttributesA->get_original<decltype(GetFileAttributesA)>()(value_orA(r, lpFileName));
    }

    DWORD WINAPI shCustomGetFileAttributesW(LPCWSTR lpFileName)
    {
        auto raddr = _ReturnAddress();
        auto r = GetFilePathForOverload(lpFileName, isRecursive(raddr));
        return mhGetFileAttributesW->get_original<decltype(GetFileAttributesW)>()(value_orW(r, lpFileName));
    }

    BOOL WINAPI shCustomGetFileAttributesExA(LPCSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation)
    {
        auto raddr = _ReturnAddress();
        auto r = GetFilePathForOverload(lpFileName, isRecursive(raddr));
        return mhGetFileAttributesExA->get_original<decltype(GetFileAttributesExA)>()(value_orA(r, lpFileName), fInfoLevelId, lpFileInformation);
    }

    BOOL WINAPI shCustomGetFileAttributesExW(LPCWSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation)
    {
        auto raddr = _ReturnAddress();
        auto r = GetFilePathForOverload(lpFileName, isRecursive(raddr));
        return mhGetFileAttributesExW->get_original<decltype(GetFileAttributesExW)>()(value_orW(r, lpFileName), fInfoLevelId, lpFileInformation);
    }

    HANDLE WINAPI shCustomFindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData)
    {
        auto raddr = _ReturnAddress();
        auto ret = mhFindFirstFileA->get_original<decltype(FindFirstFileA)>()(lpFileName, lpFindFileData);

        if (isRecursive(raddr))
            return ret;

        if (ret != INVALID_HANDLE_VALUE)
        {
            mFindFileDirsA[ret] = lpFileName;
            FindFileCheckOverloadedPath(lpFileName, lpFindFileData, lpFindFileData->cFileName);
        }

        return ret;
    }

    BOOL WINAPI shCustomFindNextFileA(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData)
    {
        auto raddr = _ReturnAddress();
        auto ret = mhFindNextFileA->get_original<decltype(FindNextFileA)>()(hFindFile, lpFindFileData);

        if (isRecursive(raddr))
            return ret;

        if (ret)
        {
            auto it = mFindFileDirsA.find(hFindFile);
            if (it != mFindFileDirsA.end())
                FindFileCheckOverloadedPath(it->second, lpFindFileData, lpFindFileData->cFileName);
        }

        return ret;
    }

    HANDLE WINAPI shCustomFindFirstFileW(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData)
    {
        auto raddr = _ReturnAddress();
        auto ret = mhFindFirstFileW->get_original<decltype(FindFirstFileW)>()(lpFileName, lpFindFileData);

        if (isRecursive(raddr))
            return ret;

        if (ret != INVALID_HANDLE_VALUE)
        {
            mFindFileDirsW[ret] = lpFileName;
            FindFileCheckOverloadedPath(lpFileName, lpFindFileData, lpFindFileData->cFileName);
        }

        return ret;
    }

    BOOL WINAPI shCustomFindNextFileW(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData)
    {
        auto raddr = _ReturnAddress();
        auto ret = mhFindNextFileW->get_original<decltype(FindNextFileW)>()(hFindFile, lpFindFileData);

        if (isRecursive(raddr))
            return ret;

        if (ret)
        {
            auto it = mFindFileDirsW.find(hFindFile);
            if (it != mFindFileDirsW.end())
                FindFileCheckOverloadedPath(it->second, lpFindFileData, lpFindFileData->cFileName);
        }

        return ret;
    }

    HANDLE WINAPI shCustomFindFirstFileExA(LPCSTR lpFileName, FINDEX_INFO_LEVELS fInfoLevelId, WIN32_FIND_DATAA* lpFindFileData, FINDEX_SEARCH_OPS fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags)
    {
        auto raddr = _ReturnAddress();
        auto ret = mhFindFirstFileExA->get_original<decltype(FindFirstFileExA)>()(lpFileName, fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags);

        if (isRecursive(raddr))
            return ret;

        if (ret != INVALID_HANDLE_VALUE)
        {
            if (fInfoLevelId != FindExInfoMaxInfoLevel)
            {
                mFindFileDirsA[ret] = lpFileName;
                FindFileCheckOverloadedPath(lpFileName, lpFindFileData, lpFindFileData->cFileName);
            }
        }

        return ret;
    }

    HANDLE WINAPI shCustomFindFirstFileExW(LPCWSTR lpFileName, FINDEX_INFO_LEVELS fInfoLevelId, WIN32_FIND_DATAW* lpFindFileData, FINDEX_SEARCH_OPS fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags)
    {
        auto raddr = _ReturnAddress();
        auto ret = mhFindFirstFileExW->get_original<decltype(FindFirstFileExW)>()(lpFileName, fInfoLevelId, lpFindFileData, fSearchOp, lpSearchFilter, dwAdditionalFlags);

        if (isRecursive(raddr))
            return ret;

        if (ret != INVALID_HANDLE_VALUE)
        {
            if (fInfoLevelId != FindExInfoMaxInfoLevel)
            {
                mFindFileDirsW[ret] = lpFileName;
                FindFileCheckOverloadedPath(lpFileName, lpFindFileData, lpFindFileData->cFileName);
            }
        }

        return ret;
    }

    BOOL WINAPI shCustomFindClose(HANDLE hFindFile)
    {
        auto raddr = _ReturnAddress();
        auto ret = mhFindClose->get_original<decltype(FindClose)>()(hFindFile);

        if (!isRecursive(raddr))
        {
            mFindFileDirsA.erase(hFindFile);
            mFindFileDirsW.erase(hFindFile);
        }

        return ret;
    }

    void HookAPIForOverload()
    {
        constexpr auto mutexName = L"Ultimate-ASI-Loader-OverloadFromFolder";

        auto hMutex = OpenMutexW(MUTEX_ALL_ACCESS, FALSE, mutexName);
        if (hMutex)
        {
            CloseHandle(hMutex);
            return;
        }

        hMutex = CreateMutexW(nullptr, TRUE, mutexName);
        if (!hMutex || GetLastError() == ERROR_ALREADY_EXISTS)
        {
            if (hMutex)
                CloseHandle(hMutex);
            return;
        }

        mhLoadLibraryExA = std::make_unique<FunctionHookMinHook>((uintptr_t)LoadLibraryExA, (uintptr_t)shCustomLoadLibraryExA);
        mhLoadLibraryExW = std::make_unique<FunctionHookMinHook>((uintptr_t)LoadLibraryExW, (uintptr_t)shCustomLoadLibraryExW);
        mhCreateFileA = std::make_unique<FunctionHookMinHook>((uintptr_t)CreateFileA, (uintptr_t)shCustomCreateFileA);
        mhCreateFileW = std::make_unique<FunctionHookMinHook>((uintptr_t)CreateFileW, (uintptr_t)shCustomCreateFileW);
        if (auto pKernel32 = GetModuleHandle(TEXT("kernel32.dll")))
        {
            if (auto pCreateFile2 = (uintptr_t)GetProcAddress(pKernel32, "CreateFile2"))
                mhCreateFile2 = std::make_unique<FunctionHookMinHook>(pCreateFile2, (uintptr_t)shCustomCreateFile2);
            if (auto pCreateFile3 = (uintptr_t)GetProcAddress(pKernel32, "CreateFile3"))
                mhCreateFile3 = std::make_unique<FunctionHookMinHook>(pCreateFile3, (uintptr_t)shCustomCreateFile3);
        }
        mhGetFileAttributesA = std::make_unique<FunctionHookMinHook>((uintptr_t)GetFileAttributesA, (uintptr_t)shCustomGetFileAttributesA);
        mhGetFileAttributesW = std::make_unique<FunctionHookMinHook>((uintptr_t)GetFileAttributesW, (uintptr_t)shCustomGetFileAttributesW);
        mhGetFileAttributesExA = std::make_unique<FunctionHookMinHook>((uintptr_t)GetFileAttributesExA, (uintptr_t)shCustomGetFileAttributesExA);
        mhGetFileAttributesExW = std::make_unique<FunctionHookMinHook>((uintptr_t)GetFileAttributesExW, (uintptr_t)shCustomGetFileAttributesExW);
        mhFindFirstFileA = std::make_unique<FunctionHookMinHook>((uintptr_t)FindFirstFileA, (uintptr_t)shCustomFindFirstFileA);
        mhFindNextFileA = std::make_unique<FunctionHookMinHook>((uintptr_t)FindNextFileA, (uintptr_t)shCustomFindNextFileA);
        mhFindFirstFileW = std::make_unique<FunctionHookMinHook>((uintptr_t)FindFirstFileW, (uintptr_t)shCustomFindFirstFileW);
        mhFindNextFileW = std::make_unique<FunctionHookMinHook>((uintptr_t)FindNextFileW, (uintptr_t)shCustomFindNextFileW);
        mhFindFirstFileExA = std::make_unique<FunctionHookMinHook>((uintptr_t)FindFirstFileExA, (uintptr_t)shCustomFindFirstFileExA);
        mhFindFirstFileExW = std::make_unique<FunctionHookMinHook>((uintptr_t)FindFirstFileExW, (uintptr_t)shCustomFindFirstFileExW);
        mhFindClose = std::make_unique<FunctionHookMinHook>((uintptr_t)FindClose, (uintptr_t)shCustomFindClose);

        mhLoadLibraryExA->create();
        mhLoadLibraryExW->create();
        mhCreateFileA->create();
        mhCreateFileW->create();
        if (mhCreateFile2)
            mhCreateFile2->create();
        if (mhCreateFile3)
            mhCreateFile3->create();
        mhGetFileAttributesA->create();
        mhGetFileAttributesW->create();
        mhGetFileAttributesExA->create();
        mhGetFileAttributesExW->create();
        mhFindFirstFileA->create();
        mhFindNextFileA->create();
        mhFindFirstFileW->create();
        mhFindNextFileW->create();
        mhFindFirstFileExA->create();
        mhFindFirstFileExW->create();
        mhFindClose->create();

        // increase the ref count in case this dll is unloaded before the game exit
        auto hNtdll = GetModuleHandleW(L"ntdll.dll");
        if (hNtdll)
        {
            auto pLdrAddRefDll = (LdrAddRefDll_t)GetProcAddress(hNtdll, "LdrAddRefDll");
            if (pLdrAddRefDll)
            {
                pLdrAddRefDll(0, (HMODULE)&__ImageBase);
            }
        }
    }
}

std::set<std::string> importedModulesList;
bool HookKernel32IAT(HMODULE mod, bool exe)
{
    auto hExecutableInstance = (size_t)mod;
    IMAGE_NT_HEADERS*           ntHeader = (IMAGE_NT_HEADERS*)(hExecutableInstance + ((IMAGE_DOS_HEADER*)hExecutableInstance)->e_lfanew);
    IMAGE_IMPORT_DESCRIPTOR*    pImports = (IMAGE_IMPORT_DESCRIPTOR*)(hExecutableInstance + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    size_t                      nNumImports = 0;
    if (ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress != 0)
    {
        IMAGE_IMPORT_DESCRIPTOR* importDesc = pImports;
        while (importDesc->Name != 0)
        {
            nNumImports++;
            importDesc++;
        }
    }

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
            if (auto pOLE32 = GetModuleHandle(TEXT("OLE32.DLL")))
                OLE32Data[eCoCreateInstance][ProcAddress] = (size_t)GetProcAddress(pOLE32, "CoCreateInstance");

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

    auto Patchvccorlib = [&](const char* dllname, size_t start, size_t end, size_t exe_end)
    {
        auto hHandle = GetModuleHandleA(dllname);
        if (!hHandle)
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
            vccorlibData[eGetCmdArguments][ProcAddress] = (size_t)GetProcAddress(hHandle, "?GetCmdArguments@Details@Platform@@YAPEAPEA_WPEAH@Z");

        for (auto i = start; i < end; i += sizeof(size_t))
        {
            DWORD dwProtect[2];
            VirtualProtect((size_t*)i, sizeof(size_t), PAGE_EXECUTE_READWRITE, &dwProtect[0]);

            auto ptr = *(size_t*)i;
            if (!ptr)
                continue;

            if (ptr == vccorlibData[eGetCmdArguments][ProcAddress])
            {
                if (exe) vccorlibData[eGetCmdArguments][IATPtr] = i;
                *(size_t*)i = (size_t)CustomGetCmdArguments;
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
            auto szName = (const char*)(hExecutableInstance + (pImports + i)->Name);
            auto dllname = std::string(szName);
            std::transform(dllname.begin(), dllname.end(), dllname.begin(), [](char c) { return ::tolower(c); });

            if (dllname == "kernel32.dll")
                PatchIAT(hExecutableInstance + (pImports + i)->FirstThunk, 0, hExecutableInstance_end);
            else if (dllname == "ole32.dll")
                PatchCoCreateInstance(hExecutableInstance + (pImports + i)->FirstThunk, 0, hExecutableInstance_end);
            else if (dllname.contains("vccorlib"))
                Patchvccorlib(szName, hExecutableInstance + (pImports + i)->FirstThunk, 0, hExecutableInstance_end);

            importedModulesList.insert(dllname);
        }
    }

    // Fixing ordinals
    auto szSelfName = GetSelfName();

    static auto PatchOrdinals = [&szSelfName](size_t hInstance)
    {
        IMAGE_NT_HEADERS*           ntHeader = (IMAGE_NT_HEADERS*)(hInstance + ((IMAGE_DOS_HEADER*)hInstance)->e_lfanew);
        IMAGE_IMPORT_DESCRIPTOR*    pImports = (IMAGE_IMPORT_DESCRIPTOR*)(hInstance + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
        size_t                      nNumImports = 0;
        if (ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress != 0)
        {
            IMAGE_IMPORT_DESCRIPTOR* importDesc = pImports;
            while (importDesc->Name != 0)
            {
                nNumImports++;
                importDesc++;
            }
        }

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
                            else if (iequals(szSelfName, L"xinput1_1.dll") || iequals(szSelfName, L"xinput1_2.dll"))
                            {
                                DWORD Protect;
                                VirtualProtect(&p[j], 4, PAGE_EXECUTE_READWRITE, &Protect);
                            
                                const enum exinput1_1
                                {
                                    DllMain = 1,
                                    XInputEnable = 2,
                                    XInputGetCapabilities = 3,
                                    XInputGetDSoundAudioDeviceGuids = 4,
                                    XInputGetState = 5,
                                    XInputSetState = 6,
                                };
                            
                                switch (IMAGE_ORDINAL(thunk->u1.Ordinal))
                                {
                                case exinput1_1::DllMain:
                                    p[j] = _DllMain;
                                    break;
                                case exinput1_1::XInputEnable:
                                    p[j] = _XInputEnable;
                                    break;
                                case exinput1_1::XInputGetCapabilities:
                                    p[j] = _XInputGetCapabilities;
                                    break;
                                case exinput1_1::XInputGetDSoundAudioDeviceGuids:
                                    p[j] = _XInputGetDSoundAudioDeviceGuids;
                                    break;
                                case exinput1_1::XInputGetState:
                                    p[j] = _XInputGetState;
                                    break;
                                case exinput1_1::XInputSetState:
                                    p[j] = _XInputSetState;
                                    break;
                                default:
                                    break;
                                }
                            }
                            else if (iequals(szSelfName, L"xinput1_3.dll"))
                            {
                                DWORD Protect;
                                VirtualProtect(&p[j], 4, PAGE_EXECUTE_READWRITE, &Protect);

                                const enum exinput1_3
                                {
                                    DllMain = 1,
                                    XInputGetState = 2,
                                    XInputSetState = 3,
                                    XInputGetCapabilities = 4,
                                    XInputEnable = 5,
                                    XInputGetDSoundAudioDeviceGuids = 6,
                                    XInputGetBatteryInformation = 7,
                                    XInputGetKeystroke = 8,
                                    XInputGetStateEx = 100,
                                    XInputWaitForGuideButton = 101,
                                    XInputCancelGuideButtonWait = 102,
                                    XInputPowerOffController = 103,
                                };

                                switch (IMAGE_ORDINAL(thunk->u1.Ordinal))
                                {
                                case exinput1_3::DllMain:
                                    p[j] = _DllMain;
                                    break;
                                case exinput1_3::XInputGetState:
                                    p[j] = _XInputGetState;
                                    break;
                                case exinput1_3::XInputSetState:
                                    p[j] = _XInputSetState;
                                    break;
                                case exinput1_3::XInputGetCapabilities:
                                    p[j] = _XInputGetCapabilities;
                                    break;
                                case exinput1_3::XInputEnable:
                                    p[j] = _XInputEnable;
                                    break;
                                case exinput1_3::XInputGetDSoundAudioDeviceGuids:
                                    p[j] = _XInputGetDSoundAudioDeviceGuids;
                                    break;
                                case exinput1_3::XInputGetBatteryInformation:
                                    p[j] = _XInputGetBatteryInformation;
                                    break;
                                case exinput1_3::XInputGetKeystroke:
                                    p[j] = _XInputGetKeystroke;
                                    break;
                                case exinput1_3::XInputGetStateEx:
                                    p[j] = _XInputGetStateEx;
                                    break;
                                case exinput1_3::XInputWaitForGuideButton:
                                    p[j] = _XInputWaitForGuideButton;
                                    break;
                                case exinput1_3::XInputCancelGuideButtonWait:
                                    p[j] = _XInputCancelGuideButtonWait;
                                    break;
                                case exinput1_3::XInputPowerOffController:
                                    p[j] = _XInputPowerOffController;
                                    break;
                                default:
                                    break;
                                }
                            }
                            else if (iequals(szSelfName, L"xinput1_4.dll"))
                            {
                                DWORD Protect;
                                VirtualProtect(&p[j], 4, PAGE_EXECUTE_READWRITE, &Protect);

                                const enum exinput1_4
                                {
                                    DllMain = 1,
                                    XInputGetState = 2,
                                    XInputSetState = 3,
                                    XInputGetCapabilities = 4,
                                    XInputEnable = 5,
                                    XInputGetBatteryInformation = 7,
                                    XInputGetKeystroke = 8,
                                    XInputGetAudioDeviceIds = 10,
                                    XInputGetStateEx = 100,
                                    XInputWaitForGuideButton = 101,
                                    XInputCancelGuideButtonWait = 102,
                                    XInputPowerOffController = 103,
                                    XInputGetBaseBusInformation = 104,
                                    XInputGetCapabilitiesEx = 108,
                                };

                                switch (IMAGE_ORDINAL(thunk->u1.Ordinal))
                                {
                                case exinput1_4::DllMain:
                                    p[j] = _DllMain;
                                    break;
                                case exinput1_4::XInputGetState:
                                    p[j] = _XInputGetState;
                                    break;
                                case exinput1_4::XInputSetState:
                                    p[j] = _XInputSetState;
                                    break;
                                case exinput1_4::XInputGetCapabilities:
                                    p[j] = _XInputGetCapabilities;
                                    break;
                                case exinput1_4::XInputEnable:
                                    p[j] = _XInputEnable;
                                    break;
                                case exinput1_4::XInputGetBatteryInformation:
                                    p[j] = _XInputGetBatteryInformation;
                                    break;
                                case exinput1_4::XInputGetKeystroke:
                                    p[j] = _XInputGetKeystroke;
                                    break;
                                case exinput1_4::XInputGetAudioDeviceIds:
                                    p[j] = _XInputGetAudioDeviceIds;
                                    break;
                                case exinput1_4::XInputGetStateEx:
                                    p[j] = _XInputGetStateEx;
                                    break;
                                case exinput1_4::XInputWaitForGuideButton:
                                    p[j] = _XInputWaitForGuideButton;
                                    break;
                                case exinput1_4::XInputCancelGuideButtonWait:
                                    p[j] = _XInputCancelGuideButtonWait;
                                    break;
                                case exinput1_4::XInputPowerOffController:
                                    p[j] = _XInputPowerOffController;
                                    break;
                                case exinput1_4::XInputGetBaseBusInformation:
                                    p[j] = _XInputGetBaseBusInformation;
                                    break;
                                case exinput1_4::XInputGetCapabilitiesEx:
                                    p[j] = _XInputGetCapabilitiesEx;
                                    break;
                                default:
                                    break;
                                }
                            }
                            else if (iequals(szSelfName, L"xinput9_1_0.dll"))
                            {
                                DWORD Protect;
                                VirtualProtect(&p[j], 4, PAGE_EXECUTE_READWRITE, &Protect);

                                const enum exinput9_1_0
                                {
                                    DllMain = 1,
                                    XInputGetCapabilities = 2,
                                    XInputGetDSoundAudioDeviceGuids = 3,
                                    XInputGetState = 4,
                                    XInputSetState = 5,
                                };

                                switch (IMAGE_ORDINAL(thunk->u1.Ordinal))
                                {
                                case exinput9_1_0::DllMain:
                                    p[j] = _DllMain;
                                    break;
                                case exinput9_1_0::XInputGetCapabilities:
                                    p[j] = _XInputGetCapabilities;
                                    break;
                                case exinput9_1_0::XInputGetDSoundAudioDeviceGuids:
                                    p[j] = _XInputGetDSoundAudioDeviceGuids;
                                    break;
                                case exinput9_1_0::XInputGetState:
                                    p[j] = _XInputGetState;
                                    break;
                                case exinput9_1_0::XInputSetState:
                                    p[j] = _XInputSetState;
                                    break;
                                default:
                                    break;
                                }
                            }
                            else if (iequals(szSelfName, L"xinputuap.dll"))
                            {
                                DWORD Protect;
                                VirtualProtect(&p[j], 4, PAGE_EXECUTE_READWRITE, &Protect);

                                const enum exinputuap
                                {
                                    DllMain = 1,
                                    XInputEnable = 2,
                                    XInputGetAudioDeviceIds = 3,
                                    XInputGetBatteryInformation = 4,
                                    XInputGetCapabilities = 5,
                                    XInputGetKeystroke = 6,
                                    XInputGetState = 7,
                                    XInputSetState = 8,
                                };

                                switch (IMAGE_ORDINAL(thunk->u1.Ordinal))
                                {
                                case exinputuap::DllMain:
                                    p[j] = _DllMain;
                                    break;
                                case exinputuap::XInputEnable:
                                    p[j] = _XInputEnable;
                                    break;
                                case exinputuap::XInputGetAudioDeviceIds:
                                    p[j] = _XInputGetAudioDeviceIds;
                                    break;
                                case exinputuap::XInputGetBatteryInformation:
                                    p[j] = _XInputGetBatteryInformation;
                                    break;
                                case exinputuap::XInputGetCapabilities:
                                    p[j] = _XInputGetCapabilities;
                                    break;
                                case exinputuap::XInputGetKeystroke:
                                    p[j] = _XInputGetKeystroke;
                                    break;
                                case exinputuap::XInputGetState:
                                    p[j] = _XInputGetState;
                                    break;
                                case exinputuap::XInputSetState:
                                    p[j] = _XInputSetState;
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
    auto sFileLoaderPathIniString = GetPrivateProfileStringW(TEXT("fileloader"), TEXT("overloadfromfolder"), TEXT("update"), iniPaths);

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

    auto ParseMultiplePaths = [](const std::wstring& pathsString, wchar_t delimiter = L'|') -> std::vector<std::filesystem::path>
    {
        std::vector<std::filesystem::path> paths;
        std::wstring::size_type start = 0;
        std::wstring::size_type end = 0;

        auto trim = [](const std::wstring& str) -> std::wstring
        {
            const auto first = str.find_first_not_of(L" \t\r\n");
            if (first == std::wstring::npos)
                return L"";

            const auto last = str.find_last_not_of(L" \t\r\n");
            return str.substr(first, last - first + 1);
        };

        auto removeQuotes = [](const std::wstring& str) -> std::wstring
        {
            if (str.size() >= 2 && str.front() == L'"' && str.back() == L'"')
                return str.substr(1, str.size() - 2);
            return str;
        };

        while (start < pathsString.length())
        {
            // Handle quoted paths: find the next quote if current position is a quote
            if (pathsString[start] == L'"')
            {
                // Find the closing quote
                end = pathsString.find(L'"', start + 1);
                if (end == std::wstring::npos) // No closing quote found
                    end = pathsString.length();
                else
                    end++; // Include the closing quote in the extraction

                // Find the next delimiter after the quoted string
                std::wstring::size_type nextDelim = pathsString.find(delimiter, end);
                if (nextDelim != std::wstring::npos)
                    end = nextDelim;
                else
                    end = pathsString.length();
            }
            else
            {
                // Standard behavior: find next delimiter
                end = pathsString.find(delimiter, start);
                if (end == std::wstring::npos)
                    end = pathsString.length();
            }

            // Extract and process the path
            std::wstring rawPath = pathsString.substr(start, end - start);
            std::wstring path = removeQuotes(trim(rawPath));
            if (!path.empty())
                paths.emplace_back(path);

            // Move to the next path
            start = (end == pathsString.length()) ? end : end + 1;
        }

        return paths;
    };

    auto FilterExistingPaths = [&FolderExists](std::vector<std::filesystem::path>& paths) -> bool
    {
        bool anyExists = false;

        paths.erase(
            std::remove_if(paths.begin(), paths.end(),
                [&](const std::filesystem::path& path) {
                    bool exists = FolderExists(path);
                    anyExists |= exists;  // Track if any path exists
                    return !exists;       // Remove if doesn't exist
                }
            ),
            paths.end()
        );

        return anyExists;
    };

    sFileLoaderPaths = ParseMultiplePaths(sFileLoaderPathIniString);

    if (!FilterExistingPaths(sFileLoaderPaths))
        sFileLoaderPath.clear();

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

        const auto it = std::find_if(std::begin(importedModulesList), std::end(importedModulesList), [&](const auto& str) { return str == "unityplayer.dll"; } );
        const auto bUnityPlayerImported = it != std::end(importedModulesList);

        HMODULE m = mainModule;
        if (nFindModule || importedModulesList.size() <= 2 || bUnityPlayerImported)
        {
            ModuleList dlls;
            dlls.Enumerate(ModuleList::SearchLocation::All);

            auto ual = std::find_if(dlls.m_moduleList.begin(), dlls.m_moduleList.end(), [](auto const& it)
            {
                return std::get<HMODULE>(it) == hm;
            });

            auto sim = std::find_if(dlls.m_moduleList.rbegin(), dlls.m_moduleList.rend(), [&ual](auto const& it)
            {
                auto str1 = std::get<std::wstring>(*ual);
                auto str2 = std::get<std::wstring>(it);
                auto bIsLocal = std::get<bool>(it);
                std::transform(str1.begin(), str1.end(), str1.begin(), [](wchar_t c) { return ::towlower(c); });
                std::transform(str2.begin(), str2.end(), str2.begin(), [](wchar_t c) { return ::towlower(c); });

                if (str2 == L"unityplayer" || str2 == L"clr" || str2 == L"coreclr")
                    return true;

                return bIsLocal && (str2 != str1) && (str2.find(str1) != std::wstring::npos);
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
            mhCreateFileA = {};
            mhCreateFileW = {};
            mhCreateFile2 = {};
            mhCreateFile3 = {};
            mhLoadLibraryExA = {};
            mhLoadLibraryExW = {};
            mhGetFileAttributesA = {};
            mhGetFileAttributesW = {};
            mhGetFileAttributesExA = {};
            mhGetFileAttributesExW = {};
            mhFindFirstFileA = {};
            mhFindNextFileA = {};
            mhFindFirstFileW = {};
            mhFindNextFileW = {};
            mhFindFirstFileExA = {};
            mhFindFirstFileExW = {};
            mhFindClose = {};
        }
    }
    return TRUE;
}
