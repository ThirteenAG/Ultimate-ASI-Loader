#include "dllmain.h"
#include "exception.hpp"
#include "miniz.h"
#include <initguid.h>
#include <filesystem>
#include <shared_mutex>
#include <variant>
#include <fstream>
#include <memory>
#include <map>
#include <set>
#include <unordered_set>
#include <FunctionHookMinHook.hpp>
#include <shellapi.h>
#include <SubAuth.h>
#include <Commctrl.h>
#pragma comment(lib, "delayimp")
#pragma comment(lib,"Comctl32.lib")
#pragma comment(linker,"\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#define _WIDEN(x) L ## x
#define WIDEN(x) _WIDEN(x)
#define ws_UpdateUrl WIDEN(rsc_UpdateUrl)
LPCWSTR szFooter = L"<a href=\"" ws_UpdateUrl "\">" ws_UpdateUrl "</a>";
constexpr auto DEFAULT_BUTTON = 1000;
#define IDI_CUSTOM_ICON 102

#if !X64
#include <d3d8to9\source\d3d8to9.hpp>
extern "C" Direct3D8* WINAPI Direct3DCreate8(UINT SDKVersion);
#endif

extern "C" IMAGE_DOS_HEADER __ImageBase;
typedef NTSTATUS(NTAPI* LdrAddRefDll_t)(ULONG, HMODULE);

namespace OverloadFromFolder
{
    struct MultiPartArchive
    {
        std::vector<std::filesystem::path> parts;
        std::vector<uint64_t> part_sizes;
        std::vector<uint64_t> part_offsets; // Cumulative offsets
        uint64_t total_size = 0;
        bool is_multi_part = false;
    };

    struct FileLoaderPathEntry
    {
        std::filesystem::path path;
        std::vector<std::filesystem::path> dependencies;
        int priority;
        bool isLessThanDependency = false; // true for '<', false for '>'
        bool isFromZip = false;
        std::vector<std::shared_ptr<MultiPartArchive>> archives;
    };

    std::filesystem::path gamePath;
    std::vector<FileLoaderPathEntry> sFileLoaderEntries;
    std::vector<std::filesystem::path> sActiveDirectories;
    std::unordered_map<std::filesystem::path, std::wstring> updateTxtContents;

    void HookAPIForOverload();
    void HookAPIForVirtualFiles();
    void LoadVirtualFilesFromZip();
    #if !X64
    bool InitializeServerConnection();
    #endif
    BOOL SetVirtualFilePointerEx(HANDLE hFile, LARGE_INTEGER liDistanceToMove, PLARGE_INTEGER lpNewFilePointer, DWORD dwMoveMethod);
    bool FilterExistingPathEntries(std::vector<FileLoaderPathEntry>& entries);
    std::vector<FileLoaderPathEntry> ParseMultiplePathsWithPriority(const std::wstring& pathsString);
    std::vector<std::filesystem::path> DetermineActiveDirectories(const std::vector<FileLoaderPathEntry>& entries, const std::filesystem::path& selectedPath, std::vector<bool>* isFromZipVector = nullptr);
    void LoadPackages();
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
static HICON hTaskbarIcon = NULL;

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

HICON GetApplicationIcon()
{
    HICON hIcon = nullptr;

    wchar_t szExePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, szExePath, MAX_PATH))
    {
        hIcon = ExtractIconW(GetModuleHandle(NULL), szExePath, 0);
        if (hIcon && hIcon != (HICON)1)
        {
            return hIcon;
        }

        if (hIcon == (HICON)1)
            hIcon = nullptr;
    }

    return hIcon;
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

            hTaskbarIcon = GetApplicationIcon();
            if (hTaskbarIcon)
                SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hTaskbarIcon);

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

                if (hTaskbarIcon)
                    DestroyIcon(hTaskbarIcon);
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
std::wstring sLoadFromAPI;

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

    if (!iequals(path.root_name().wstring(), base.root_name().wstring()) || path.is_absolute() != base.is_absolute() || (!path.has_root_directory() && base.has_root_directory()))
    {
        return std::filesystem::path();
    }
    std::filesystem::path::const_iterator a = path.begin(), b = base.begin();
    while (a != path.end() && b != base.end() && iequals(a->wstring(), b->wstring()))
    {
        ++a;
        ++b;
    }
    if (a == path.end() && b == base.end())
    {
        return std::filesystem::path(".");
    }
    int count = 0;
    for (const auto& element : input_iterator_range(b, base.end()))
    {
        if (element != "." && element != "" && element != "..")
        {
            ++count;
        }
        else if (element == "..")
        {
            --count;
        }
    }
    if (count < 0)
    {
        return std::filesystem::path();
    }
    std::filesystem::path result;
    for (int i = 0; i < count; ++i)
    {
        result /= "..";
    }
    for (const auto& element : input_iterator_range(a, path.end()))
    {
        result /= element;
    }
    return result;
}

bool FolderExists(auto szPath)
{
    try
    {
        auto path = std::filesystem::path(szPath);
        if (path.is_absolute())
            return std::filesystem::is_directory(path);
        else
            return std::filesystem::is_directory(std::filesystem::path(GetModuleFileNameW(NULL)).parent_path() / path);
    } catch (...)
    {
    }

    return false;
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
        GetPrivateProfileStringW(lpAppName, lpKeyName, ret.data(), ret.data(), DWORD(ret.size()), file.c_str());
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
    BYTE* pImageBase = reinterpret_cast<BYTE*>(dwLoadOffset);
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

struct FunctionData
{
    uintptr_t IATPtr;
    uintptr_t ProcAddress;

    void Restore() const
    {
        if (!IATPtr)
            return;
        auto ptr = (size_t*)IATPtr;
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery(ptr, &mbi, sizeof(mbi)) == 0 || mbi.State != MEM_COMMIT || !(mbi.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_WRITECOPY)))
            return;
        DWORD dwProtect[2];
        if (!VirtualProtect(ptr, sizeof(size_t), PAGE_EXECUTE_READWRITE, &dwProtect[0]))
            return;
        *ptr = ProcAddress;
        VirtualProtect(ptr, sizeof(size_t), dwProtect[0], &dwProtect[1]);
    }
};

struct ModuleIATData
{
    HMODULE module = NULL;
    std::unordered_map<std::string, FunctionData> kernel32Functions;
    std::unordered_map<std::string, FunctionData> ole32Functions;
    std::unordered_map<std::string, FunctionData> vccorlibFunctions;
};

std::vector<ModuleIATData> moduleIATs;

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
                                tdc.pszMainInstruction = szHeader;
                                tdc.pszContent = szContent;
                                tdc.pfCallback = TaskDialogCallbackProc;
                                tdc.lpCallbackData = 0;

                                if (!IsPackagedProcess())
                                {
                                    if (auto hCustomIcon = (HICON)LoadImage(hm, MAKEINTRESOURCE(IDI_CUSTOM_ICON), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED))
                                    {
                                        tdc.dwFlags |= TDF_USE_HICON_MAIN;
                                        tdc.hMainIcon = hCustomIcon;
                                    }
                                    else
                                    {
                                        tdc.pszMainIcon = TD_ERROR_ICON;
                                    }
                                    std::ignore = TaskDialogIndirect(&tdc, &nClickedBtn, NULL, &bCheckboxChecked);
                                }
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
        } while (FindNextFileW(asiFile, fd));
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
    auto sLoadExtraPlugins = GetPrivateProfileStringW(TEXT("globalsets"), TEXT("loadextraplugins"), TEXT("modloader\\modloader.asi"), iniPaths);

    if (nWantsToLoadPlugins)
    {
        auto sExtraPlugins = [](const std::wstring& pathsString) -> std::vector<std::wstring>
        {
            std::vector<std::wstring> entries;
            std::wstring::size_type start = 0;

            auto trim = [](std::wstring str)
            {
                auto first = str.find_first_not_of(L" \t\r\n");
                if (first == std::wstring::npos) return std::wstring();
                auto last = str.find_last_not_of(L" \t\r\n");
                return str.substr(first, last - first + 1);
            };

            auto removeQuotes = [](std::wstring str)
            {
                return (str.size() >= 2 && str.front() == L'"' && str.back() == L'"')
                    ? str.substr(1, str.size() - 2) : str;
            };

            while (start < pathsString.length())
            {
                auto end = pathsString.find(L'|', start);
                if (end == std::wstring::npos) end = pathsString.length();

                std::wstring cleanPath = removeQuotes(trim(pathsString.substr(start, end - start)));
                if (!cleanPath.empty() &&
                    std::find(entries.begin(), entries.end(), cleanPath) == entries.end())
                {
                    entries.push_back(cleanPath);
                }

                start = end + 1;
                while (start < pathsString.length() && ::iswspace(pathsString[start])) start++;
            }

            return entries;
        }(sLoadExtraPlugins);

        if (!sExtraPlugins.empty())
        {
            for (const auto& it : sExtraPlugins)
            {
                SetCurrentDirectoryW(szSelfPath.c_str());
                LoadLib(it);
                SetCurrentDirectoryW(oldDir.c_str());
            }
        }

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

        // Load plugins from active directories if they exist
        for (const auto& activeDir : OverloadFromFolder::sActiveDirectories)
        {
            FindPlugins(fd,
                activeDir.c_str(),
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
    if (!sLoadFromAPI.empty())
    {
        std::wstring LoadFromAPI = sLoadFromAPI;
        std::wstring LoadFromModule = L"";

        size_t dotPos = sLoadFromAPI.find(L'.');
        if (dotPos != std::wstring::npos)
        {
            LoadFromModule = sLoadFromAPI.substr(0, dotPos);
            LoadFromAPI = sLoadFromAPI.substr(dotPos + 1);
        }

        if (calledFrom != LoadFromAPI)
            return;

        HMODULE mod = NULL;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCTSTR)retaddr, &mod);

        if (mod != GetModuleHandle(LoadFromModule.empty() ? NULL : LoadFromModule.c_str()))
            return;
    }

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

    for (auto& modData : moduleIATs)
    {
        for (auto& [name, data] : modData.kernel32Functions)
            data.Restore();
        for (auto& [name, data] : modData.vccorlibFunctions)
            data.Restore();
    }

    {
        using namespace OverloadFromFolder;

        auto sFileLoaderPathIniString = GetPrivateProfileStringW(TEXT("fileloader"), TEXT("overloadfromfolder"), TEXT("update"), iniPaths);
        sFileLoaderEntries = ParseMultiplePathsWithPriority(sFileLoaderPathIniString);
        LoadPackages();
        if (!FilterExistingPathEntries(sFileLoaderEntries))
        {
            // No valid paths exist - clear everything
            sFileLoaderEntries.clear();
            sActiveDirectories.clear();
        }

        if (sFileLoaderEntries.size() > 1)
        {
            std::vector<std::filesystem::path> pathsForDialog;
            for (const auto& entry : sFileLoaderEntries)
            {
                pathsForDialog.push_back(entry.path);
            }

            std::wstring dialogMutexName = L"Global\\UltimateASILoader-FolderSelectDialog-Mutex" + std::to_wstring(GetCurrentProcessId());
            auto hDialogMutex = CreateMutexW(NULL, TRUE, dialogMutexName.c_str());

            if (hDialogMutex && GetLastError() == ERROR_ALREADY_EXISTS)
            {
                CloseHandle(hDialogMutex);
                ExitProcess(0);
            }

            int buttonId = DEFAULT_BUTTON;
            std::vector<TASKDIALOG_BUTTON> aButtons;
            aButtons.reserve(pathsForDialog.size());
            std::vector<std::wstring> buttonTexts;
            buttonTexts.reserve(pathsForDialog.size());

            std::error_code ec;
            for (const auto& path : pathsForDialog)
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
                else
                {
                    auto it = updateTxtContents.find(updateTxtPath);
                    if (it != updateTxtContents.end())
                    {
                        if (!it->second.empty())
                            header = it->second;
                    }
                }

                std::vector<bool> isZip;
                auto activeDirs = DetermineActiveDirectories(sFileLoaderEntries, path, &isZip);
                auto pathWithRelations = std::filesystem::absolute(path, ec).wstring();
                if (activeDirs.size() > 1)
                {
                    pathWithRelations += L" + ";
                    for (size_t i = 1; i < activeDirs.size(); ++i)
                    {
                        if (i > 1) pathWithRelations += L" + ";
                        pathWithRelations += activeDirs[i].filename().wstring();
                    }
                }

                {
                    for (size_t i = 0; i < activeDirs.size(); ++i)
                    {
                        if (isZip[i])
                        {
                            header += L" [ZIP]";
                            break;
                        }
                    }
                }

                std::wstring buttonText = std::wstring(L"&") + header + L"\n" + pathWithRelations;
                buttonTexts.push_back(buttonText);
                aButtons.push_back({ buttonId++, buttonTexts.back().c_str() });
            }

            updateTxtContents.clear();

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
            tdc.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ENABLE_HYPERLINKS | TDF_SIZE_TO_CONTENT | TDF_CAN_BE_MINIMIZED | TDF_SHOW_PROGRESS_BAR | TDF_CALLBACK_TIMER;
            tdc.pButtons = aButtons.data();
            tdc.cButtons = static_cast<UINT>(aButtons.size());
            tdc.pszWindowTitle = szTitle;
            tdc.pszMainInstruction = szHeader;
            tdc.pszContent = szContent;
            tdc.pfCallback = TaskDialogCallbackProc;
            tdc.lpCallbackData = 0;
            tdc.nDefaultButton = DEFAULT_BUTTON;
            tdc.pszFooter = szFooter;
            tdc.pszFooterIcon = TD_INFORMATION_ICON;

            if (!IsPackagedProcess())
            {
                if (auto hCustomIcon = (HICON)LoadImage(hm, MAKEINTRESOURCE(IDI_CUSTOM_ICON), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED))
                {
                    tdc.dwFlags |= TDF_USE_HICON_MAIN;
                    tdc.hMainIcon = hCustomIcon;
                }
                else
                {
                    tdc.pszMainIcon = TD_WARNING_ICON;
                }

                if (SUCCEEDED(TaskDialogIndirect(&tdc, &nClickedBtn, NULL, &bCheckboxChecked)) && nClickedBtn != IDCANCEL)
                {
                    int selectedIndex = nClickedBtn - DEFAULT_BUTTON;
                    auto& selectedPath = pathsForDialog[selectedIndex];

                    // Set active directories based on user selection
                    sActiveDirectories = DetermineActiveDirectories(sFileLoaderEntries, selectedPath);
                }
            }
            else
            {
                for (size_t i = 0; i < pathsForDialog.size(); ++i)
                {
                    const auto& path = pathsForDialog[i];
                    std::wstring message = L"Multiple folders have been specified for file overloading.\nUse this folder?\n\n";
                    message += path.wstring() + L"\n" + std::filesystem::absolute(path, ec).wstring();

                    auto result = MessageBoxW(NULL, message.c_str(), szTitle, MB_YESNO | MB_ICONQUESTION);
                    if (result == IDYES)
                    {
                        sActiveDirectories = DetermineActiveDirectories(sFileLoaderEntries, path);
                        break;
                    }
                }
            }

            if (hDialogMutex)
            {
                ReleaseMutex(hDialogMutex);
                CloseHandle(hDialogMutex);
            }

            LoadVirtualFilesFromZip();
            #if !X64
            InitializeServerConnection();
            #endif
            HookAPIForOverload();
            HookAPIForVirtualFiles();
        }
        else if (sFileLoaderEntries.size() == 1)
        {
            sActiveDirectories = DetermineActiveDirectories(sFileLoaderEntries, sFileLoaderEntries[0].path);

            LoadVirtualFilesFromZip();
            #if !X64
            InitializeServerConnection();
            #endif
            HookAPIForOverload();
            HookAPIForVirtualFiles();
        }
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

DEFINE_GUID(CLSID_DirectSound, 0x47d4d946, 0x62e8, 0x11cf, 0x93, 0xbc, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00);
DEFINE_GUID(CLSID_DirectSound8, 0x3901cc3f, 0x84b5, 0x4fa4, 0xba, 0x35, 0xaa, 0x81, 0x72, 0xb8, 0xa0, 0x9b);
DEFINE_GUID(CLSID_DirectInput, 0x25E609E0, 0xB259, 0x11CF, 0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00);
DEFINE_GUID(CLSID_DirectInput8, 0x25E609E4, 0xB259, 0x11CF, 0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00);
DEFINE_GUID(CLSID_WinInet, 0xC39EE728, 0xD419, 0x4BD4, 0xA3, 0xEF, 0xED, 0xA0, 0x59, 0xDB, 0xD9, 0x35);
HRESULT WINAPI CustomCoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID* ppv)
{
    HRESULT hr = REGDB_E_KEYMISSING;
    HMODULE hDll = NULL;

    if (rclsid == CLSID_DirectSound || rclsid == CLSID_DirectSound8)
        hDll = ::LoadLibrary(L"dsound.dll");
    else if (rclsid == CLSID_DirectInput8)
        hDll = ::LoadLibrary(L"dinput8.dll");
    else if (rclsid == CLSID_DirectInput)
        hDll = ::LoadLibrary(L"dinput.dll");
    else if (rclsid == CLSID_WinInet)
        hDll = ::LoadLibrary(L"wininet.dll");

    if (hDll == NULL || GetProcAddress(hDll, "IsUltimateASILoader") != NULL)
        return ::CoCreateInstance(rclsid, pUnkOuter, dwClsContext, riid, ppv);

    typedef HRESULT(__stdcall* pDllGetClassObject)(IN REFCLSID rclsid, IN REFIID riid, OUT LPVOID FAR* ppv);

    pDllGetClassObject GetClassObject = (pDllGetClassObject)::GetProcAddress(hDll, "DllGetClassObject");
    if (GetClassObject == NULL)
    {
        ::FreeLibrary(hDll);
        return hr;
    }

    IClassFactory* pIFactory;

    hr = GetClassObject(rclsid, IID_IClassFactory, (LPVOID*)&pIFactory);

    if (!SUCCEEDED(hr))
        return hr;

    hr = pIFactory->CreateInstance(pUnkOuter, riid, ppv);
    pIFactory->Release();

    return hr;
}

LPWSTR WINAPI CustomGetCmdArguments(int* argc)
{
    HMODULE mod = NULL;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCTSTR)_ReturnAddress(), &mod);
    uintptr_t original = 0;
    for (auto& m : moduleIATs)
    {
        if (m.module == mod)
        {
            auto it = m.vccorlibFunctions.find("?GetCmdArguments@Details@Platform@@YAPEAPEA_WPEAH@Z");
            if (it != m.vccorlibFunctions.end())
            {
                original = it->second.ProcAddress;
                break;
            }
        }
    }
    auto fnGetCmdArguments = reinterpret_cast<decltype(&CustomGetCmdArguments)>(original);
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

    std::unique_ptr<FunctionHookMinHook> mhReadFile = { nullptr };
    std::unique_ptr<FunctionHookMinHook> mhReadFileEx = { nullptr };
    std::unique_ptr<FunctionHookMinHook> mhGetFileSize = { nullptr };
    std::unique_ptr<FunctionHookMinHook> mhGetFileSizeEx = { nullptr };
    std::unique_ptr<FunctionHookMinHook> mhSetFilePointer = { nullptr };
    std::unique_ptr<FunctionHookMinHook> mhSetFilePointerEx = { nullptr };
    std::unique_ptr<FunctionHookMinHook> mhCloseHandle = { nullptr };
    std::unique_ptr<FunctionHookMinHook> mhGetFileInformationByHandle = { nullptr };
    std::unique_ptr<FunctionHookMinHook> mhGetFileInformationByHandleEx = { nullptr };
    std::unique_ptr<FunctionHookMinHook> mhGetFileType = { nullptr };

    thread_local std::unordered_map<HANDLE, std::string> mFindFileDirsA;
    thread_local std::unordered_map<HANDLE, std::wstring> mFindFileDirsW;

    extern "C" extern DWORD _tls_index;

    bool IsTlsInitialized()
    {
        __try
        {
            // Get TEB pointer - ThreadLocalStoragePointer is at offset 0x2C (32-bit) or 0x58 (64-bit)
            #if !X64
            void* tlsPtr = reinterpret_cast<void*>(__readfsdword(0x2C));
            #else
            void* tlsPtr = reinterpret_cast<void*>(__readgsqword(0x58));
            #endif

            if (!tlsPtr)
                return false;

            // Verify the TLS slot for this module is accessible
            auto tlsArray = static_cast<void**>(tlsPtr);
            volatile void* tlsSlot = tlsArray[_tls_index];
            (void)tlsSlot; // Suppress unused variable warning
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
    }

    HANDLE serverPipe = INVALID_HANDLE_VALUE;
    std::mutex serverMutex;

    struct ServerCommand
    {
        enum Type : uint32_t
        {
            ADD_FILE = 1,
            APPEND_FILE = 2,
            REMOVE_FILE = 3,
            GET_SIZE = 4,
            READ_FILE = 5
        };
        uint32_t command;
        uint64_t server_handle; // Used for most commands
        uint64_t data_size;     // For ADD/APPEND/READ
        int32_t priority;       // For ADD
        uint64_t offset;        // For READ
    };

    struct LocalData
    {
        std::vector<uint8_t> data;

        size_t GetSize() const
        {
            return data.size();
        }
    };

    struct ServerData
    {
        uint64_t server_handle;  // Handle from 64-bit server
        uint64_t size;

        size_t GetSize() const
        {
            return static_cast<size_t>(size);
        }
    };

    struct ZipData
    {
        std::shared_ptr<MultiPartArchive> archive;
        mz_uint fileIndex;
        uint64_t uncompressedSize;

        size_t GetSize() const
        {
            return static_cast<size_t>(uncompressedSize);
        }
    };

    struct VirtualFile
    {
        std::variant<LocalData, ServerData, ZipData> storage;
        int priority;
        FILETIME creationTime;
        FILETIME lastWriteTime;
        FILETIME lastAccessTime;
        std::atomic<uint64_t> position{ 0 };

        VirtualFile(const uint8_t* fileData, size_t fileSize, int filePriority)
            : priority(filePriority), position(0)
        {
            SYSTEMTIME st;
            GetSystemTime(&st);
            SystemTimeToFileTime(&st, &creationTime);
            lastWriteTime = creationTime;
            lastAccessTime = creationTime;

            #if !X64
            uint64_t handle = CreateFileOnServer(fileData, fileSize, filePriority);
            if (handle != 0)
            {
                storage = ServerData{ handle, static_cast<uint64_t>(fileSize) };
            }
            else
            {
                storage = LocalData{ std::vector<uint8_t>(fileData, fileData + fileSize) };
            }
            #else
            storage = LocalData{ std::vector<uint8_t>(fileData, fileData + fileSize) };
            #endif
        }

        VirtualFile(std::shared_ptr<MultiPartArchive> archive, uint32_t fileIndex, uint64_t uncompressedSize, int filePriority)
            : priority(filePriority), position(0)
        {
            SYSTEMTIME st;
            GetSystemTime(&st);
            SystemTimeToFileTime(&st, &creationTime);
            lastWriteTime = creationTime;
            lastAccessTime = creationTime;
            storage = ZipData{ archive, fileIndex, uncompressedSize };
        }

        size_t GetSize() const
        {
            return std::visit([](const auto& data) -> size_t
            {
                return data.GetSize();
            }, storage);
        }

    public:
        #if !X64
        static uint64_t CreateFileOnServer(const uint8_t* data, size_t size, int priority)
        {
            if (serverPipe == INVALID_HANDLE_VALUE)
            {
                return 0;
            }

            std::lock_guard<std::mutex> lock(serverMutex);

            ServerCommand request = {};
            request.command = ServerCommand::ADD_FILE;
            request.data_size = size;
            request.priority = priority;

            DWORD bytesWritten = 0;
            if (!WriteFile(serverPipe, &request, sizeof(request), &bytesWritten, NULL) || bytesWritten != sizeof(request))
            {
                CloseHandle(serverPipe);
                serverPipe = INVALID_HANDLE_VALUE;
                return 0;
            }

            if (size > 0 && (!WriteFile(serverPipe, data, (DWORD)size, &bytesWritten, NULL) || bytesWritten != size))
            {
                CloseHandle(serverPipe);
                serverPipe = INVALID_HANDLE_VALUE;
                return 0;
            }

            uint64_t new_handle = 0;
            DWORD bytesRead = 0;
            if (!ReadFile(serverPipe, &new_handle, sizeof(new_handle), &bytesRead, NULL) || bytesRead != sizeof(new_handle))
            {
                CloseHandle(serverPipe);
                serverPipe = INVALID_HANDLE_VALUE;
                return 0;
            }

            return new_handle;
        }

        static bool AppendFileOnServer(uint64_t server_handle, const uint8_t* data, size_t size)
        {
            if (serverPipe == INVALID_HANDLE_VALUE) return false;

            std::lock_guard<std::mutex> lock(serverMutex);

            ServerCommand request = {};
            request.command = ServerCommand::APPEND_FILE;
            request.server_handle = server_handle;
            request.data_size = size;

            DWORD bytesWritten = 0;
            if (!WriteFile(serverPipe, &request, sizeof(request), &bytesWritten, NULL) || bytesWritten != sizeof(request))
            {
                CloseHandle(serverPipe);
                serverPipe = INVALID_HANDLE_VALUE;
                return false;
            }

            if (size > 0 && (!WriteFile(serverPipe, data, (DWORD)size, &bytesWritten, NULL) || bytesWritten != size))
            {
                CloseHandle(serverPipe);
                serverPipe = INVALID_HANDLE_VALUE;
                return false;
            }

            return true;
        }

        static void RemoveFileOnServer(uint64_t server_handle)
        {
            if (serverPipe == INVALID_HANDLE_VALUE) return;

            std::lock_guard<std::mutex> lock(serverMutex);

            ServerCommand request = {};
            request.command = ServerCommand::REMOVE_FILE;
            request.server_handle = server_handle;

            DWORD bytesWritten = 0;
            WriteFile(serverPipe, &request, sizeof(request), &bytesWritten, NULL);
        }
        #endif
    };

    static std::atomic<size_t> virtualPathsCount{ 0 };
    static std::shared_mutex virtualPathMutex;
    static std::unordered_map<std::wstring, std::pair<std::filesystem::path, int>> virtualPathMappings;
    thread_local std::unordered_set<std::wstring> recursionGuard;

    static std::atomic<size_t> virtualFilesCount{ 0 };
    static std::shared_mutex virtualFilesMutex;
    static std::unordered_map<std::wstring, std::shared_ptr<VirtualFile>> virtualFilesByPath;
    static std::unordered_map<HANDLE, std::shared_ptr<VirtualFile>> virtualFilesByHandle;

    std::wstring NormalizePath(const std::filesystem::path& path)
    {
        static auto starts_with = [](const std::filesystem::path& path, const std::filesystem::path& base) -> bool
        {
            std::wstring str1(path.wstring()); std::wstring str2(base.wstring());
            std::transform(str1.begin(), str1.end(), str1.begin(), ::towlower);
            std::transform(str2.begin(), str2.end(), str2.begin(), ::towlower);
            return str1.starts_with(str2);
        };

        std::error_code ec;
        auto filePath = path;
        auto absolutePath = std::filesystem::absolute(filePath.make_preferred(), ec);
        auto relativePath = lexicallyRelativeCaseIns(absolutePath, gamePath);

        if (starts_with(relativePath, ".."))
        {
            std::filesystem::path rp;
            for (auto& p : relativePath)
            {
                if (p != "..")
                    rp = rp / p;
            }
            relativePath = rp;
        }

        auto relativePathStr = relativePath.wstring();
        std::transform(relativePathStr.begin(), relativePathStr.end(), relativePathStr.begin(), ::towlower);
        return relativePathStr;
    }

    inline bool HasVirtualPaths()
    {
        return virtualPathsCount.load(std::memory_order_acquire) > 0;
    }

    inline bool HasVirtualFiles()
    {
        return virtualFilesCount.load(std::memory_order_acquire) > 0;
    }

    inline bool IsVirtualHandle(HANDLE handle)
    {
        std::shared_lock lock(virtualFilesMutex);
        return virtualFilesByHandle.find(handle) != virtualFilesByHandle.end();
    }

    bool IsVirtualFile(const std::filesystem::path& path)
    {
        auto normalizedPath = NormalizePath(path);
        if (normalizedPath.empty())
            return false;

        std::shared_lock lock(virtualFilesMutex);
        return virtualFilesByPath.find(normalizedPath) != virtualFilesByPath.end();
    }

    VirtualFile* GetVirtualFileByHandle(HANDLE handle)
    {
        std::shared_lock lock(virtualFilesMutex);
        auto it = virtualFilesByHandle.find(handle);
        return (it != virtualFilesByHandle.end()) ? it->second.get() : nullptr;
    }

    VirtualFile* GetVirtualFileByPath(const std::filesystem::path& path)
    {
        std::shared_lock lock(virtualFilesMutex);
        auto it = virtualFilesByPath.find(NormalizePath(path));
        return (it != virtualFilesByPath.end()) ? it->second.get() : nullptr;
    }

    HANDLE CreateVirtualHandle(const std::filesystem::path& path, DWORD dwDesiredAccess, DWORD dwCreationDisposition)
    {
        if (dwDesiredAccess & GENERIC_WRITE)
        {
            SetLastError(ERROR_ACCESS_DENIED);
            return INVALID_HANDLE_VALUE;
        }

        if (dwCreationDisposition == CREATE_NEW || dwCreationDisposition == CREATE_ALWAYS)
        {
            SetLastError(ERROR_FILE_EXISTS);
            return INVALID_HANDLE_VALUE;
        }

        HANDLE dummyHandle = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (dummyHandle == NULL || dummyHandle == INVALID_HANDLE_VALUE)
            return INVALID_HANDLE_VALUE;

        std::unique_lock lock(virtualFilesMutex);

        // Find the virtual file by path
        auto pathIt = virtualFilesByPath.find(NormalizePath(path));
        if (pathIt != virtualFilesByPath.end())
        {
            virtualFilesByHandle[dummyHandle] = pathIt->second;
            pathIt->second->position = 0; // Reset position
            return dummyHandle;
        }
        else
        {
            // Path not found, clean up and return invalid handle
            CloseHandle(dummyHandle);
            return INVALID_HANDLE_VALUE;
        }
    }

    void CloseVirtualHandle(HANDLE handle)
    {
        std::unique_lock lock(virtualFilesMutex);
        virtualFilesByHandle.erase(handle);
        lock.unlock();
        CloseHandle(handle);
    }

    std::vector<FileLoaderPathEntry> ParseMultiplePathsWithPriority(const std::wstring& pathsString)
    {
        std::vector<FileLoaderPathEntry> entries;
        std::map<std::wstring, size_t> pathIndices;
        std::wstring::size_type start = 0;

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

        auto findLastOfBefore = [](const std::wstring& str, const std::wstring& chars, std::wstring::size_type pos) -> std::wstring::size_type
        {
            if (pos == 0) return std::wstring::npos;
            for (std::wstring::size_type i = pos - 1; i != std::wstring::npos; --i)
            {
                if (chars.find(str[i]) != std::wstring::npos)
                    return i;
            }
            return std::wstring::npos;
        };

        int currentPriority = 1000; // Start with high priority

        // First pass: Parse paths and create entries in order
        while (start < pathsString.length())
        {
            auto end = pathsString.find_first_of(L"|<>", start);
            if (end == std::wstring::npos) end = pathsString.length();

            std::wstring rawPath = pathsString.substr(start, end - start);
            std::wstring cleanPath = removeQuotes(trim(rawPath));

            if (!cleanPath.empty())
            {
                // Check for duplicates
                auto it = pathIndices.find(cleanPath);
                if (it == pathIndices.end())
                {
                    FileLoaderPathEntry entry;
                    entry.path = cleanPath;
                    entry.priority = currentPriority--;
                    entries.push_back(entry);
                    pathIndices[cleanPath] = entries.size() - 1; // Store index
                }

                // Handle '<' dependency during first pass
                if (end < pathsString.length())
                {
                    wchar_t separator = pathsString[end];
                    if (separator == L'<')
                    {
                        auto nextStart = end + 1;
                        auto nextEnd = pathsString.find_first_of(L"|<>", nextStart);
                        if (nextEnd == std::wstring::npos) nextEnd = pathsString.length();
                        std::wstring nextPath = removeQuotes(trim(pathsString.substr(nextStart, nextEnd - nextStart)));
                        if (!nextPath.empty())
                        {
                            // Ensure target entry exists or create it
                            size_t nextIndex = pathIndices.count(nextPath) ? pathIndices[nextPath] : entries.size();
                            if (nextIndex == entries.size())
                            {
                                FileLoaderPathEntry nextEntry;
                                nextEntry.path = nextPath;
                                nextEntry.priority = currentPriority--;
                                entries.push_back(nextEntry);
                                pathIndices[nextPath] = nextIndex;
                            }

                            // Add dependency if not already present
                            auto& deps = entries[pathIndices[cleanPath]].dependencies;
                            if (std::find(deps.begin(), deps.end(), nextPath) == deps.end())
                            {
                                deps.push_back(nextPath);
                                entries[pathIndices[cleanPath]].isLessThanDependency = true; // '<' syntax
                            }
                        }
                    }
                }
            }

            start = end == pathsString.length() ? end : end + 1;
            while (start < pathsString.length() && ::iswspace(pathsString[start]))
            {
                start++;
            }
        }

        // Second pass: Handle '>' dependencies
        for (std::wstring::size_type pos = 0; pos < pathsString.length(); ++pos)
        {
            if (pathsString[pos] == L'>')
            {
                auto beforeStart = findLastOfBefore(pathsString, L"|<>", pos);
                if (beforeStart == std::wstring::npos) beforeStart = 0;
                else beforeStart++;
                auto afterEnd = pathsString.find_first_of(L"|<>", pos + 1);
                if (afterEnd == std::wstring::npos) afterEnd = pathsString.length();

                std::wstring beforePath = removeQuotes(trim(pathsString.substr(beforeStart, pos - beforeStart)));
                std::wstring afterPath = removeQuotes(trim(pathsString.substr(pos + 1, afterEnd - pos - 1)));

                if (!beforePath.empty() && !afterPath.empty())
                {
                    // Ensure both entries exist
                    size_t beforeIndex = pathIndices.count(beforePath) ? pathIndices[beforePath] : entries.size();
                    size_t afterIndex = pathIndices.count(afterPath) ? pathIndices[afterPath] : entries.size();
                    if (beforeIndex == entries.size())
                    {
                        FileLoaderPathEntry entry;
                        entry.path = beforePath;
                        entry.priority = currentPriority--;
                        entries.push_back(entry);
                        pathIndices[beforePath] = beforeIndex;
                    }
                    if (afterIndex == entries.size())
                    {
                        FileLoaderPathEntry entry;
                        entry.path = afterPath;
                        entry.priority = currentPriority--;
                        entries.push_back(entry);
                        pathIndices[afterPath] = afterIndex;
                    }

                    // Add dependency if not already present
                    auto& deps = entries[pathIndices[beforePath]].dependencies;
                    if (std::find(deps.begin(), deps.end(), afterPath) == deps.end())
                    {
                        deps.push_back(afterPath);
                        entries[pathIndices[beforePath]].isLessThanDependency = false; // '>' syntax
                    }
                }
            }
        }

        // Adjust priorities based on dependencies
        // X < Y = Y has higher priority, X > Y = X has higher priority
        for (auto& entry : entries)
        {
            if (!entry.dependencies.empty())
            {
                for (const auto& dep : entry.dependencies)
                {
                    for (auto& depEntry : entries)
                    {
                        if (depEntry.path == dep)
                        {
                            if (entry.isLessThanDependency)
                            {
                                // '<' syntax: entry < dep, so dep should have higher priority
                                if (depEntry.priority <= entry.priority)
                                {
                                    depEntry.priority = entry.priority + 1;
                                }
                            }
                            else
                            {
                                // '>' syntax: entry > dep, so entry should have higher priority
                                if (entry.priority <= depEntry.priority)
                                {
                                    entry.priority = depEntry.priority + 1;
                                }
                            }
                            break;
                        }
                    }
                }
            }
        }

        return entries;
    }

    bool FilterExistingPathEntries(std::vector<FileLoaderPathEntry>& entries)
    {
        bool anyExists = false;
        entries.erase(std::remove_if(entries.begin(), entries.end(), [&](const FileLoaderPathEntry& entry)
        {
            if (entry.isFromZip)
            {
                anyExists = true;
                return false;
            }
            bool exists = FolderExists(entry.path);
            anyExists |= exists;
            return !exists;
        }), entries.end());
        return anyExists;
    };

    std::vector<std::filesystem::path> DetermineActiveDirectories(const std::vector<FileLoaderPathEntry>& entries, const std::filesystem::path& selectedPath, std::vector<bool>* isFromZipVector)
    {
        std::set<std::filesystem::path> pathsToKeep;
        std::set<std::filesystem::path> processed;

        // Lambda to recursively find all dependencies
        std::function<void(const std::filesystem::path&)> addPathAndDependencies = [&](const std::filesystem::path& currentPath)
        {
            if (processed.count(currentPath) > 0)
                return; // Already processed
            processed.insert(currentPath);
            pathsToKeep.insert(currentPath);

            // Find the entry for current path
            const FileLoaderPathEntry* currentEntry = nullptr;
            for (const auto& entry : entries)
            {
                if (entry.path == currentPath)
                {
                    currentEntry = &entry;
                    break;
                }
            }

            if (currentEntry)
            {
                // For '>' syntax: when selecting left side, also load right side (dependency)
                if (!currentEntry->isLessThanDependency && !currentEntry->dependencies.empty())
                {
                    for (const auto& dep : currentEntry->dependencies)
                    {
                        addPathAndDependencies(dep); // Recursive call
                    }
                }
            }

            // For '<' syntax: when selecting right side, also load left side (dependent)
            for (const auto& entry : entries)
            {
                if (entry.isLessThanDependency)
                {
                    for (const auto& dep : entry.dependencies)
                    {
                        if (dep == currentPath)
                        {
                            addPathAndDependencies(entry.path); // Recursive call
                            break;
                        }
                    }
                }
            }
        };

        // Start the recursive process with the selected path
        addPathAndDependencies(selectedPath);

        // Convert set to vector and sort by priority for proper overriding order
        std::vector<std::pair<std::filesystem::path, std::pair<bool, int>>> pathsWithPriority;
        for (const auto& path : pathsToKeep)
        {
            for (const auto& entry : entries)
            {
                if (entry.path == path)
                {
                    pathsWithPriority.emplace_back(path, std::make_pair(entry.isFromZip, entry.priority));
                    break;
                }
            }
        }

        // Sort by priority (higher priority numbers first for file overriding)
        std::sort(pathsWithPriority.begin(), pathsWithPriority.end(),
            [](const auto& a, const auto& b) { return a.second.second > b.second.second; });

        std::vector<std::filesystem::path> result;
        if (isFromZipVector)
        {
            isFromZipVector->clear(); // Clear to ensure correct size and content
            isFromZipVector->reserve(pathsWithPriority.size()); // Pre-allocate for efficiency
        }
        for (const auto& pair : pathsWithPriority)
        {
            result.push_back(pair.first);
            if (isFromZipVector)
            {
                isFromZipVector->push_back(pair.second.first); // Store isFromZip
            }
        }

        return result;
    }

    size_t multipart_zip_read_func(void* pOpaque, mz_uint64 file_ofs, void* pBuf, size_t n)
    {
        auto* archive = static_cast<MultiPartArchive*>(pOpaque);
        if (file_ofs >= archive->total_size) return 0;

        size_t bytes_read_total = 0;
        size_t bytes_to_read = n;
        if (file_ofs + n > archive->total_size)
            bytes_to_read = static_cast<size_t>(archive->total_size - file_ofs);

        size_t current_part_index = 0;
        for (size_t i = 0; i < archive->parts.size() - 1; ++i)
        {
            if (file_ofs < archive->part_offsets[i + 1])
            {
                current_part_index = i;
                break;
            }
        }
        if (file_ofs >= archive->part_offsets.back())
            current_part_index = archive->parts.size() - 1;

        uint64_t offset_in_part = file_ofs - archive->part_offsets[current_part_index];

        while (bytes_read_total < bytes_to_read && current_part_index < archive->parts.size())
        {
            std::ifstream part_stream(archive->parts[current_part_index], std::ios::binary);
            if (!part_stream) return 0;

            part_stream.seekg(offset_in_part, std::ios::beg);
            size_t remaining_in_part = static_cast<size_t>(archive->part_sizes[current_part_index] - offset_in_part);
            size_t can_read_now = min(bytes_to_read - bytes_read_total, remaining_in_part);

            part_stream.read(static_cast<char*>(pBuf) + bytes_read_total, can_read_now);
            size_t actual_read = size_t(part_stream.gcount());
            bytes_read_total += actual_read;

            if (actual_read < can_read_now) break;

            current_part_index++;
            offset_in_part = 0;
        }

        return bytes_read_total;
    }

    void LoadPackages()
    {
        std::error_code ec;
        auto packagesPath = gamePath / "packages";
        if (std::filesystem::exists(packagesPath, ec) && std::filesystem::is_directory(packagesPath, ec))
        {
            constexpr auto perms =
                std::filesystem::directory_options::skip_permission_denied |
                std::filesystem::directory_options::follow_directory_symlink;

            std::map<std::wstring, std::vector<std::filesystem::path>> groupedArchives;
            for (const auto& dir_entry : std::filesystem::directory_iterator(packagesPath, perms, ec))
            {
                if (!dir_entry.is_regular_file())
                    continue;

                std::wstring filename = dir_entry.path().filename().wstring();
                std::wstring lowercaseFilename = filename;
                std::transform(lowercaseFilename.begin(), lowercaseFilename.end(), lowercaseFilename.begin(), [](wchar_t c) { return ::towlower(c); });
                size_t pos = lowercaseFilename.find(L".zip");
                if (pos != std::wstring::npos)
                {
                    std::wstring extension = dir_entry.path().extension().wstring();
                    if (!extension.empty() && extension[0] == L'.')
                        extension = extension.substr(1);

                    if (lowercaseFilename.ends_with(L".zip") || (lowercaseFilename.ends_with(L".zip." + extension) && std::all_of(extension.cbegin(), extension.cend(), [](wchar_t c) { return ::iswdigit(c); })))
                    {
                        std::wstring baseName = filename.substr(0, pos);
                        groupedArchives[baseName].push_back(dir_entry.path());
                    }
                }
            }

            for (auto const& [baseName, parts] : groupedArchives)
            {
                auto sorted_parts = parts;
                std::sort(sorted_parts.begin(), sorted_parts.end());

                auto archive = std::make_shared<MultiPartArchive>();
                archive->is_multi_part = sorted_parts.size() > 1;

                uint64_t current_offset = 0;
                for (const auto& part_path : sorted_parts)
                {
                    std::error_code ec;
                    uint64_t part_size = std::filesystem::file_size(part_path, ec);
                    if (ec)
                        continue;
                    archive->parts.push_back(part_path);
                    archive->part_sizes.push_back(part_size);
                    archive->part_offsets.push_back(current_offset);
                    current_offset += part_size;
                }
                archive->total_size = current_offset;

                if (archive->parts.empty())
                    continue;

                mz_zip_archive zip_archive = {};
                bool reader_initialized = false;

                if (archive->is_multi_part)
                {
                    zip_archive.m_pRead = multipart_zip_read_func;
                    zip_archive.m_pIO_opaque = archive.get();
                    reader_initialized = mz_zip_reader_init(&zip_archive, archive->total_size, 0);
                }
                else
                {
                    reader_initialized = mz_zip_reader_init_file(&zip_archive, archive->parts[0].string().c_str(), 0);
                }

                if (reader_initialized)
                {
                    std::set<std::filesystem::path> rootDirsInZip;

                    for (uint32_t i = 0; i < mz_zip_reader_get_num_files(&zip_archive); i++)
                    {
                        mz_zip_archive_file_stat file_stat;
                        if (mz_zip_reader_file_stat(&zip_archive, i, &file_stat))
                        {
                            std::filesystem::path internalPath(file_stat.m_filename);
                            if (!mz_zip_reader_is_file_a_directory(&zip_archive, i) && internalPath.has_parent_path())
                            {
                                auto parent = internalPath.parent_path();
                                if (parent.begin() != parent.end())
                                {
                                    rootDirsInZip.insert(*parent.begin());
                                }

                                // Check for update.txt in the zip
                                if (internalPath.filename() == "update.txt")
                                {
                                    // Extract update.txt to memory
                                    size_t fileSize;
                                    void* fileData = mz_zip_reader_extract_to_heap(&zip_archive, i, &fileSize, 0);
                                    if (fileData)
                                    {
                                        try
                                        {
                                            std::string fileContent(static_cast<char*>(fileData), fileSize);
                                            std::wstring content = to_wstring(fileContent);

                                            // Extract first line (up to newline or 100 chars)
                                            size_t newlinePos = content.find(L'\n');
                                            if (newlinePos != std::wstring::npos)
                                                content = content.substr(0, newlinePos);
                                            if (content.size() > 100)
                                                content = content.substr(0, 100);

                                            if (!content.empty())
                                            {
                                                updateTxtContents[internalPath] = content;
                                            }
                                        } catch (const std::exception&)
                                        {
                                            // Skip if conversion fails
                                        }
                                        mz_free(fileData);
                                    }
                                }
                            }
                            else if (mz_zip_reader_is_file_a_directory(&zip_archive, i) && internalPath.begin() != internalPath.end())
                            {
                                rootDirsInZip.insert(*internalPath.begin());
                            }
                        }
                    }

                    auto loaderEntriesCopy = sFileLoaderEntries;
                    FilterExistingPathEntries(loaderEntriesCopy);

                    std::set<std::filesystem::path> remainingPaths;
                    for (const auto& entry : loaderEntriesCopy)
                        remainingPaths.insert(entry.path);

                    std::vector<FileLoaderPathEntry> excludedEntries;
                    for (const auto& entry : sFileLoaderEntries)
                    {
                        if (remainingPaths.find(entry.path) == remainingPaths.end())
                        {
                            excludedEntries.push_back(entry);
                        }
                    }

                    for (const auto& rootDir : rootDirsInZip)
                    {
                        bool bExcluded = std::find_if(excludedEntries.begin(), excludedEntries.end(), [&rootDir](const FileLoaderPathEntry& e)
                        {
                            return e.path == rootDir;
                        }) != excludedEntries.end();

                        bool found = false;
                        for (auto& entry : sFileLoaderEntries)
                        {
                            if (bExcluded && entry.path == rootDir)
                            {
                                entry.isFromZip = true;
                                entry.archives.push_back(archive);
                                found = true;
                                break;
                            }
                        }
                        if (!found)
                        {
                            if (bExcluded)
                            {
                                FileLoaderPathEntry newEntry;
                                newEntry.path = rootDir;
                                newEntry.priority = -1000;
                                newEntry.isFromZip = true;
                                newEntry.archives.push_back(archive);
                                sFileLoaderEntries.push_back(newEntry);
                                break;
                            }
                        }
                    }
                    mz_zip_reader_end(&zip_archive);
                }
            }
        }
    }

    std::filesystem::path WINAPI GetOverloadedFilePath(std::filesystem::path lpFilename)
    {
        try
        {
            if (HasVirtualPaths())
            {
                auto normalized = NormalizePath(lpFilename);

                if (IsTlsInitialized())
                {
                    if (recursionGuard.count(normalized))
                    {
                        return {};
                    }
                    recursionGuard.insert(normalized);
                }

                std::shared_lock lock(virtualPathMutex);
                auto it = virtualPathMappings.find(normalized);
                if (it != virtualPathMappings.end())
                {
                    std::filesystem::path vpath(it->second.first);
                    auto recursive = GetOverloadedFilePath(vpath);
                    if (IsTlsInitialized())
                        recursionGuard.erase(normalized);
                    if (!recursive.empty())
                        return recursive;
                    return vpath;
                }
                if (IsTlsInitialized())
                    recursionGuard.erase(normalized);
            }

            std::error_code ec;

            static auto starts_with = [](const std::filesystem::path& path, const std::filesystem::path& base) -> bool
            {
                std::wstring str1(path.wstring()); std::wstring str2(base.wstring());
                std::transform(str1.begin(), str1.end(), str1.begin(), ::towlower);
                std::transform(str2.begin(), str2.end(), str2.begin(), ::towlower);
                return str1.starts_with(str2);
            };

            auto filePath = lpFilename;
            auto absolutePath = std::filesystem::absolute(filePath.make_preferred(), ec);
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

            // Check if file is from any active directory (skip self-reference)
            for (const auto& activeDir : sActiveDirectories)
            {
                if (*relativePath.begin() == activeDir)
                    return {};
            }

            if (starts_with(std::filesystem::path(absolutePath).remove_filename(), gamePath) || starts_with(std::filesystem::path(absolutePath).remove_filename(), commonPath))
            {
                // Try each active directory in priority order (first = highest priority)
                for (const auto& activeDir : sActiveDirectories)
                {
                    auto newPath = gamePath / activeDir / relativePath;
                    if (std::filesystem::exists(newPath, ec) && !std::filesystem::is_directory(newPath, ec))
                        return newPath;
                }
            }
        } catch (...) {}

        return {};
    }

    bool WINAPI GetOverloadedFilePathA(const char* lpFilename, char* out, size_t out_size)
    {
        try
        {
            if (!sActiveDirectories.empty())
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
        } catch (...) {}
        return false;
    }

    bool WINAPI GetOverloadedFilePathW(const wchar_t* lpFilename, wchar_t* out, size_t out_size)
    {
        try
        {
            if (!sActiveDirectories.empty())
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
        } catch (...) {}
        return false;
    }

    bool WINAPI GetOverloadPathA(char* out, size_t out_size)
    {
        try
        {
            if (!sActiveDirectories.empty())
            {
                if (out && out_size)
                {
                    auto path = gamePath / sActiveDirectories[0]; // Use first active directory
                    out[path.string().copy(out, out_size, 0)] = '\0';
                    return true;
                }
            }
        } catch (...) {}
        return false;
    }

    bool WINAPI GetOverloadPathW(wchar_t* out, size_t out_size)
    {
        try
        {
            if (!sActiveDirectories.empty())
            {
                if (out && out_size)
                {
                    auto path = gamePath / sActiveDirectories[0]; // Use first active directory
                    out[path.wstring().copy(out, out_size, 0)] = L'\0';
                    return true;
                }
            }
        } catch (...) {}
        return false;
    }

    std::filesystem::path GetFilePathForOverload(auto path)
    {
        try
        {
            if (HasVirtualPaths())
            {
                std::shared_lock lock(virtualPathMutex);
                auto normalized = NormalizePath(path);
                auto it = virtualPathMappings.find(normalized);
                if (it != virtualPathMappings.end())
                {
                    std::filesystem::path vpath(it->second.first);
                    auto recursive = GetOverloadedFilePath(vpath);
                    if (!recursive.empty())
                        return recursive;
                    return vpath;
                }
            }

            if (!sActiveDirectories.empty())
                return GetOverloadedFilePath(path);
        } catch (...) {}

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
            SYSTEMTIME st;
            FILETIME time;
            GetSystemTime(&st);
            SystemTimeToFileTime(&st, &time);
            lpFindFileData->ftCreationTime = time;
            lpFindFileData->ftLastAccessTime = time;
            lpFindFileData->ftLastWriteTime = time;
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
        auto bRecursive = isRecursive(raddr);
        if (!bRecursive && HasVirtualFiles() && IsVirtualFile(lpFileName))
            return CreateVirtualHandle(lpFileName, dwAccess, dwCreation);
        auto r = GetFilePathForOverload(lpFileName, bRecursive);
        return mhCreateFileA->get_original<decltype(CreateFileA)>()(value_orA(r, lpFileName), dwAccess, dwSharing, saAttributes, dwCreation, dwAttributes, hTemplate);
    }

    HANDLE WINAPI shCustomCreateFileW(LPCWSTR lpFileName, DWORD dwAccess, DWORD dwSharing, LPSECURITY_ATTRIBUTES saAttributes, DWORD dwCreation, DWORD dwAttributes, HANDLE hTemplate)
    {
        auto raddr = _ReturnAddress();
        auto bRecursive = isRecursive(raddr);
        if (!bRecursive && HasVirtualFiles() && IsVirtualFile(lpFileName))
            return CreateVirtualHandle(lpFileName, dwAccess, dwCreation);
        auto r = GetFilePathForOverload(lpFileName, bRecursive);
        return mhCreateFileW->get_original<decltype(CreateFileW)>()(value_orW(r, lpFileName), dwAccess, dwSharing, saAttributes, dwCreation, dwAttributes, hTemplate);
    }

    HANDLE WINAPI shCustomCreateFile2(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, DWORD dwCreationDisposition, LPCREATEFILE2_EXTENDED_PARAMETERS pCreateExParams)
    {
        auto raddr = _ReturnAddress();
        auto bRecursive = isRecursive(raddr);
        if (!bRecursive && HasVirtualFiles() && IsVirtualFile(lpFileName))
            return CreateVirtualHandle(lpFileName, dwDesiredAccess, dwCreationDisposition);
        auto r = GetFilePathForOverload(lpFileName, bRecursive);
        return mhCreateFile2->get_original<decltype(shCustomCreateFile2)>()(value_orW(r, lpFileName), dwDesiredAccess, dwShareMode, dwCreationDisposition, pCreateExParams);
    }

    HANDLE WINAPI shCustomCreateFile3(LPCWSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, DWORD dwCreationDisposition, /*LPCREATEFILE3_EXTENDED_PARAMETERS*/void* pCreateExParams)
    {
        auto raddr = _ReturnAddress();
        auto bRecursive = isRecursive(raddr);
        if (!bRecursive && HasVirtualFiles() && IsVirtualFile(lpFileName))
            return CreateVirtualHandle(lpFileName, dwDesiredAccess, dwCreationDisposition);
        auto r = GetFilePathForOverload(lpFileName, bRecursive);
        return mhCreateFile3->get_original<decltype(shCustomCreateFile3)>()(value_orW(r, lpFileName), dwDesiredAccess, dwShareMode, dwCreationDisposition, pCreateExParams);
    }

    DWORD WINAPI shCustomGetFileAttributesA(LPCSTR lpFileName)
    {
        auto raddr = _ReturnAddress();
        auto bRecursive = isRecursive(raddr);
        if (!bRecursive && HasVirtualFiles() && IsVirtualFile(lpFileName))
        {
            if (GetVirtualFileByPath(lpFileName))
            {
                return FILE_ATTRIBUTE_NORMAL;
            }
            SetLastError(ERROR_FILE_NOT_FOUND);
            return INVALID_FILE_ATTRIBUTES;
        }
        auto r = GetFilePathForOverload(lpFileName, bRecursive);
        return mhGetFileAttributesA->get_original<decltype(GetFileAttributesA)>()(value_orA(r, lpFileName));
    }

    DWORD WINAPI shCustomGetFileAttributesW(LPCWSTR lpFileName)
    {
        auto raddr = _ReturnAddress();
        auto bRecursive = isRecursive(raddr);
        if (!bRecursive && HasVirtualFiles() && IsVirtualFile(lpFileName))
        {
            if (GetVirtualFileByPath(lpFileName))
            {
                return FILE_ATTRIBUTE_NORMAL;
            }
            SetLastError(ERROR_FILE_NOT_FOUND);
            return INVALID_FILE_ATTRIBUTES;
        }
        auto r = GetFilePathForOverload(lpFileName, bRecursive);
        return mhGetFileAttributesW->get_original<decltype(GetFileAttributesW)>()(value_orW(r, lpFileName));
    }

    BOOL WINAPI shCustomGetFileAttributesExA(LPCSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation)
    {
        auto raddr = _ReturnAddress();
        auto bRecursive = isRecursive(raddr);
        if (!bRecursive && HasVirtualFiles() && IsVirtualFile(lpFileName))
        {
            if (auto virtualFile = GetVirtualFileByPath(lpFileName))
            {
                if (fInfoLevelId == GetFileExInfoStandard && lpFileInformation)
                {
                    auto* fileAttributeData = static_cast<LPWIN32_FILE_ATTRIBUTE_DATA>(lpFileInformation);
                    uint64_t fileSize = virtualFile->GetSize();

                    fileAttributeData->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
                    fileAttributeData->ftCreationTime = virtualFile->creationTime;
                    fileAttributeData->ftLastAccessTime = virtualFile->lastAccessTime;
                    fileAttributeData->ftLastWriteTime = virtualFile->lastWriteTime;
                    fileAttributeData->nFileSizeLow = static_cast<DWORD>(fileSize & 0xFFFFFFFF);
                    fileAttributeData->nFileSizeHigh = static_cast<DWORD>(fileSize >> 32);
                    return TRUE;
                }
            }
            SetLastError(ERROR_FILE_NOT_FOUND);
            return FALSE;
        }
        auto r = GetFilePathForOverload(lpFileName, bRecursive);
        return mhGetFileAttributesExA->get_original<decltype(GetFileAttributesExA)>()(value_orA(r, lpFileName), fInfoLevelId, lpFileInformation);
    }

    BOOL WINAPI shCustomGetFileAttributesExW(LPCWSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation)
    {
        auto raddr = _ReturnAddress();
        auto bRecursive = isRecursive(raddr);
        if (!bRecursive && HasVirtualFiles() && IsVirtualFile(lpFileName))
        {
            if (auto virtualFile = GetVirtualFileByPath(lpFileName))
            {
                if (fInfoLevelId == GetFileExInfoStandard && lpFileInformation)
                {
                    auto* fileAttributeData = static_cast<LPWIN32_FILE_ATTRIBUTE_DATA>(lpFileInformation);
                    uint64_t fileSize = virtualFile->GetSize();

                    fileAttributeData->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
                    fileAttributeData->ftCreationTime = virtualFile->creationTime;
                    fileAttributeData->ftLastAccessTime = virtualFile->lastAccessTime;
                    fileAttributeData->ftLastWriteTime = virtualFile->lastWriteTime;
                    fileAttributeData->nFileSizeLow = static_cast<DWORD>(fileSize & 0xFFFFFFFF);
                    fileAttributeData->nFileSizeHigh = static_cast<DWORD>(fileSize >> 32);
                    return TRUE;
                }
            }
            SetLastError(ERROR_FILE_NOT_FOUND);
            return FALSE;
        }

        auto r = GetFilePathForOverload(lpFileName, bRecursive);
        return mhGetFileAttributesExW->get_original<decltype(GetFileAttributesExW)>()(value_orW(r, lpFileName), fInfoLevelId, lpFileInformation);
    }

    HANDLE WINAPI shCustomFindFirstFileA(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData)
    {
        auto raddr = _ReturnAddress();
        auto ret = mhFindFirstFileA->get_original<decltype(FindFirstFileA)>()(lpFileName, lpFindFileData);

        if (isRecursive(raddr))
            return ret;

        if (ret != INVALID_HANDLE_VALUE && IsTlsInitialized())
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

        if (ret && IsTlsInitialized())
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

        if (ret != INVALID_HANDLE_VALUE && IsTlsInitialized())
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

        if (ret && IsTlsInitialized())
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

        if (ret != INVALID_HANDLE_VALUE && IsTlsInitialized())
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

        if (ret != INVALID_HANDLE_VALUE && IsTlsInitialized())
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

        if (!isRecursive(raddr) && IsTlsInitialized())
        {
            mFindFileDirsA.erase(hFindFile);
            mFindFileDirsW.erase(hFindFile);
        }

        return ret;
    }

    BOOL ReadVirtualFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped, LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
    {
        if (!lpBuffer)
        {
            SetLastError(ERROR_INVALID_PARAMETER);
            return FALSE;
        }

        auto virtualFile = GetVirtualFileByHandle(hFile);
        if (!virtualFile)
        {
            SetLastError(ERROR_INVALID_HANDLE);
            return FALSE;
        }

        // Promote from ZipData to a readable format on first read
        if (std::holds_alternative<ZipData>(virtualFile->storage))
        {
            auto zipData = std::get<ZipData>(virtualFile->storage); // Make a copy
            mz_zip_archive zip_archive = {};
            bool success = false;

            if (zipData.archive->is_multi_part)
            {
                zip_archive.m_pRead = multipart_zip_read_func;
                zip_archive.m_pIO_opaque = zipData.archive.get();
                success = mz_zip_reader_init(&zip_archive, zipData.archive->total_size, 0);
            }
            else
            {
                success = mz_zip_reader_init_file(&zip_archive, zipData.archive->parts[0].string().c_str(), 0);
            }

            if (!success)
            {
                SetLastError(ERROR_OPEN_FAILED);
                return FALSE;
            }

            size_t uncompressed_size = 0;
            void* p = mz_zip_reader_extract_to_heap(&zip_archive, zipData.fileIndex, &uncompressed_size, 0);
            mz_zip_reader_end(&zip_archive);

            if (!p)
            {
                SetLastError(ERROR_READ_FAULT);
                return FALSE;
            }

            #if !X64
            // In 32-bit, try to offload to the 64-bit server process to save memory
            uint64_t handle = VirtualFile::CreateFileOnServer(static_cast<const uint8_t*>(p), uncompressed_size, virtualFile->priority);
            if (handle != 0)
            {
                virtualFile->storage = ServerData{ handle, uncompressed_size };
            }
            else
            {
                // Fallback to local memory if server fails
                virtualFile->storage = LocalData{ std::vector<uint8_t>(static_cast<uint8_t*>(p), static_cast<uint8_t*>(p) + uncompressed_size) };
            }
            #else
            // In 64-bit, always use local memory
            virtualFile->storage = LocalData{ std::vector<uint8_t>(static_cast<uint8_t*>(p), static_cast<uint8_t*>(p) + uncompressed_size) };
            #endif
            mz_free(p);
        }

        uint64_t readPosition;
        if (lpOverlapped)
        {
            LARGE_INTEGER liOffset;
            liOffset.LowPart = lpOverlapped->Offset;
            liOffset.HighPart = lpOverlapped->OffsetHigh;
            readPosition = liOffset.QuadPart;
            if (static_cast<int64_t>(readPosition) < 0)
            {
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }
            lpOverlapped->InternalHigh = 0;
        }
        else
        {
            readPosition = virtualFile->position;
            LARGE_INTEGER liDistanceToMove;
            LARGE_INTEGER liNewPosition;
            liDistanceToMove.QuadPart = 0; // Query current position
            if (SetVirtualFilePointerEx(hFile, liDistanceToMove, &liNewPosition, FILE_CURRENT))
            {
                readPosition = liNewPosition.QuadPart;
                virtualFile->position = readPosition;
            }
        }

        DWORD bytesToRead = 0;
        DWORD bytesRead = 0;

        // Handle different storage types
        return std::visit([&](const auto& storage) -> BOOL
        {
            using T = std::decay_t<decltype(storage)>;

            if constexpr (std::is_same_v<T, LocalData>)
            {
                const auto& data = storage.data;

                // Handle reads past EOF
                if (readPosition >= data.size())
                {
                    if (lpNumberOfBytesRead)
                        *lpNumberOfBytesRead = 0;
                    SetLastError(ERROR_HANDLE_EOF);
                    return TRUE;
                }

                uint64_t remainingBytes = data.size() - readPosition;
                bytesToRead = static_cast<DWORD>(min(static_cast<uint64_t>(nNumberOfBytesToRead), remainingBytes));

                if (bytesToRead > 0)
                {
                    memcpy(lpBuffer, data.data() + readPosition, bytesToRead);
                    bytesRead = bytesToRead;

                    if (!lpOverlapped)
                    {
                        virtualFile->position = readPosition + bytesRead;
                        LARGE_INTEGER liDistanceToMove;
                        LARGE_INTEGER liNewPosition;
                        liDistanceToMove.QuadPart = virtualFile->position; // Absolute position from beginning
                        if (!SetVirtualFilePointerEx(hFile, liDistanceToMove, &liNewPosition, FILE_BEGIN))
                        {
                            return FALSE;
                        }
                        // Validate the new position matches the intended position
                        if (liNewPosition.QuadPart != virtualFile->position)
                        {
                            SetLastError(ERROR_SEEK);
                            return FALSE;
                        }
                    }
                    else if (bytesRead > 0)
                    {
                        lpOverlapped->InternalHigh = bytesRead;
                    }
                }
            }
            else if constexpr (std::is_same_v<T, ServerData>)
            {
                #if !X64
                // 32-bit: request data from server
                if (serverPipe == INVALID_HANDLE_VALUE)
                {
                    SetLastError(ERROR_INVALID_HANDLE);
                    return FALSE;
                }

                std::lock_guard<std::mutex> lock(serverMutex);

                // Send read request to server
                ServerCommand request = {};
                request.command = ServerCommand::READ_FILE;
                request.server_handle = storage.server_handle;
                request.offset = readPosition;
                request.data_size = nNumberOfBytesToRead; // Use data_size for read length

                DWORD bytesWritten;
                if (!WriteFile(serverPipe, &request, sizeof(request), &bytesWritten, NULL) || bytesWritten != sizeof(request))
                {
                    CloseHandle(serverPipe);
                    serverPipe = INVALID_HANDLE_VALUE;
                    SetLastError(ERROR_WRITE_FAULT);
                    return FALSE;
                }

                // Read response size
                DWORD tempBytesToRead;
                if (!ReadFile(serverPipe, &tempBytesToRead, sizeof(tempBytesToRead), &bytesWritten, NULL) || bytesWritten != sizeof(tempBytesToRead))
                {
                    CloseHandle(serverPipe);
                    serverPipe = INVALID_HANDLE_VALUE;
                    SetLastError(ERROR_READ_FAULT);
                    return FALSE;
                }

                bytesToRead = tempBytesToRead;
                if (bytesToRead > 0)
                {
                    if (!ReadFile(serverPipe, lpBuffer, bytesToRead, &bytesRead, NULL))
                    {
                        CloseHandle(serverPipe);
                        serverPipe = INVALID_HANDLE_VALUE;
                        SetLastError(ERROR_READ_FAULT);
                        return FALSE;
                    }

                    if (bytesRead > 0)
                    {
                        if (!lpOverlapped)
                        {
                            virtualFile->position = readPosition + bytesRead;
                            LARGE_INTEGER liDistanceToMove;
                            LARGE_INTEGER liNewPosition;
                            liDistanceToMove.QuadPart = virtualFile->position;
                            if (!SetVirtualFilePointerEx(hFile, liDistanceToMove, &liNewPosition, FILE_BEGIN))
                            {
                                return FALSE;
                            }
                            if (liNewPosition.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
                            {
                                return FALSE;
                            }
                        }
                        else if (bytesRead > 0)
                        {
                            lpOverlapped->InternalHigh = bytesRead;
                        }
                    }
                    else
                    {
                        if (lpNumberOfBytesRead)
                            *lpNumberOfBytesRead = 0;
                        SetLastError(ERROR_HANDLE_EOF);
                        return TRUE;
                    }
                }
                else
                {
                    if (lpNumberOfBytesRead)
                        *lpNumberOfBytesRead = 0;
                    SetLastError(ERROR_HANDLE_EOF);
                    return TRUE;
                }
                #else
                SetLastError(ERROR_NOT_SUPPORTED);
                return FALSE;
                #endif
            }
            else if constexpr (std::is_same_v<T, ZipData>)
            {
                // This case should have been promoted already.
                // If we reach here, it's an internal logic error.
                SetLastError(ERROR_INTERNAL_ERROR);
                return FALSE;
            }

            if (lpNumberOfBytesRead)
            {
                *lpNumberOfBytesRead = bytesRead;
            }

            if (lpOverlapped)
            {
                if (lpOverlapped->hEvent)
                {
                    ResetEvent(lpOverlapped->hEvent);
                    SetEvent(lpOverlapped->hEvent);
                }
                if (lpCompletionRoutine)
                {
                    struct CompletionData
                    {
                        LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine;
                        DWORD dwErrorCode;
                        DWORD dwNumberOfBytesTransferred;
                        LPOVERLAPPED lpOverlapped;
                    };

                    auto CompletionRoutineWrapper = [](PVOID lpParameter) -> DWORD
                    {
                        auto* data = static_cast<CompletionData*>(lpParameter);
                        data->lpCompletionRoutine(data->dwErrorCode, data->dwNumberOfBytesTransferred, data->lpOverlapped);
                        delete data;
                        return ERROR_SUCCESS;
                    };

                    auto* completionData = new CompletionData
                    {
                        lpCompletionRoutine,
                        ERROR_SUCCESS,
                        bytesRead,
                        lpOverlapped
                    };

                    if (!QueueUserWorkItem(static_cast<LPTHREAD_START_ROUTINE>(CompletionRoutineWrapper), completionData, WT_EXECUTEDEFAULT))
                    {
                        DWORD error = GetLastError();
                        delete completionData;
                        SetLastError(error);
                        return FALSE;
                    }
                }
            }

            return TRUE;
        }, virtualFile->storage);
    }

    BOOL WINAPI shCustomReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped)
    {
        auto raddr = _ReturnAddress();
        if (!isRecursive(raddr) && IsVirtualHandle(hFile))
            return ReadVirtualFile(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped, nullptr);
        return mhReadFile->get_original<decltype(ReadFile)>()(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped);
    }

    BOOL WINAPI shCustomReadFileEx(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPOVERLAPPED lpOverlapped, LPOVERLAPPED_COMPLETION_ROUTINE lpCompletionRoutine)
    {
        auto raddr = _ReturnAddress();
        if (!isRecursive(raddr) && IsVirtualHandle(hFile))
            return ReadVirtualFile(hFile, lpBuffer, nNumberOfBytesToRead, nullptr, lpOverlapped, lpCompletionRoutine);
        return mhReadFileEx->get_original<decltype(ReadFileEx)>()(hFile, lpBuffer, nNumberOfBytesToRead, lpOverlapped, lpCompletionRoutine);
    }

    DWORD WINAPI shCustomGetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh)
    {
        auto raddr = _ReturnAddress();
        if (!isRecursive(raddr) && IsVirtualHandle(hFile))
        {
            if (auto virtualFile = GetVirtualFileByHandle(hFile))
            {
                DWORD64 fileSize = virtualFile->GetSize();
                if (lpFileSizeHigh)
                    *lpFileSizeHigh = static_cast<DWORD>(fileSize >> 32);
                return static_cast<DWORD>(fileSize & 0xFFFFFFFF);
            }
            return INVALID_FILE_SIZE;
        }
        return mhGetFileSize->get_original<decltype(GetFileSize)>()(hFile, lpFileSizeHigh);
    }

    BOOL WINAPI shCustomGetFileSizeEx(HANDLE hFile, PLARGE_INTEGER lpFileSize)
    {
        auto raddr = _ReturnAddress();
        if (!isRecursive(raddr) && IsVirtualHandle(hFile))
        {
            auto virtualFile = GetVirtualFileByHandle(hFile);
            if (virtualFile)
            {
                if (!lpFileSize)
                {
                    SetLastError(ERROR_INVALID_PARAMETER);
                    return FALSE;
                }
                lpFileSize->QuadPart = virtualFile->GetSize();
                return TRUE;
            }
            return FALSE;
        }
        return mhGetFileSizeEx->get_original<decltype(GetFileSizeEx)>()(hFile, lpFileSize);
    }

    BOOL SetVirtualFilePointerEx(HANDLE hFile, LARGE_INTEGER liDistanceToMove, PLARGE_INTEGER lpNewFilePointer, DWORD dwMoveMethod)
    {
        if (auto virtualFile = GetVirtualFileByHandle(hFile))
        {
            uint64_t fileSize = uint64_t(virtualFile->GetSize());
            uint64_t currentPosition = virtualFile->position;
            uint64_t newPosition;

            switch (dwMoveMethod)
            {
                case FILE_BEGIN:
                    if (liDistanceToMove.QuadPart < 0 || liDistanceToMove.QuadPart > int64_t(fileSize))
                    {
                        SetLastError(ERROR_NEGATIVE_SEEK);
                        return FALSE;
                    }
                    newPosition = liDistanceToMove.QuadPart;
                    break;
                case FILE_CURRENT:
                {
                    int64_t offset = liDistanceToMove.QuadPart;
                    newPosition = currentPosition + offset;
                    if (offset < 0 && static_cast<uint64_t>(-offset) > currentPosition)
                    {
                        SetLastError(ERROR_NEGATIVE_SEEK);
                        return FALSE;
                    }
                    if (newPosition > fileSize)
                    {
                        SetLastError(ERROR_SEEK_ON_DEVICE);
                        return FALSE;
                    }
                }
                break;
                case FILE_END:
                    if (liDistanceToMove.QuadPart > 0 || static_cast<uint64_t>(-liDistanceToMove.QuadPart) > fileSize)
                    {
                        SetLastError(ERROR_NEGATIVE_SEEK);
                        return FALSE;
                    }
                    newPosition = fileSize + liDistanceToMove.QuadPart;
                    break;
                default:
                    SetLastError(ERROR_INVALID_PARAMETER);
                    return FALSE;
            }
            virtualFile->position = newPosition;
            if (lpNewFilePointer)
            {
                lpNewFilePointer->QuadPart = newPosition;
            }
            return TRUE;
        }
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }

    BOOL WINAPI shCustomSetFilePointerEx(HANDLE hFile, LARGE_INTEGER liDistanceToMove, PLARGE_INTEGER lpNewFilePointer, DWORD dwMoveMethod)
    {
        auto raddr = _ReturnAddress();
        if (!isRecursive(raddr) && IsVirtualHandle(hFile))
            return SetVirtualFilePointerEx(hFile, liDistanceToMove, lpNewFilePointer, dwMoveMethod);
        return mhSetFilePointerEx->get_original<decltype(SetFilePointerEx)>()(hFile, liDistanceToMove, lpNewFilePointer, dwMoveMethod);
    }

    DWORD WINAPI shCustomSetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod)
    {
        auto raddr = _ReturnAddress();
        if (!isRecursive(raddr) && IsVirtualHandle(hFile))
        {
            LARGE_INTEGER liDistanceToMove;
            liDistanceToMove.LowPart = lDistanceToMove;
            liDistanceToMove.HighPart = lpDistanceToMoveHigh ? *lpDistanceToMoveHigh : 0;
            LARGE_INTEGER liNewPosition;
            if (!SetVirtualFilePointerEx(hFile, liDistanceToMove, &liNewPosition, dwMoveMethod))
            {
                return INVALID_SET_FILE_POINTER;
            }
            if (lpDistanceToMoveHigh)
            {
                *lpDistanceToMoveHigh = liNewPosition.HighPart;
            }
            return liNewPosition.LowPart;
        }
        return mhSetFilePointer->get_original<decltype(SetFilePointer)>()(hFile, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);
    }

    BOOL WINAPI shCustomCloseHandle(HANDLE hObject)
    {
        auto raddr = _ReturnAddress();
        if (!isRecursive(raddr) && IsVirtualHandle(hObject))
        {
            CloseVirtualHandle(hObject);
            return TRUE;
        }
        return mhCloseHandle->get_original<decltype(CloseHandle)>()(hObject);
    }

    BOOL WINAPI shCustomGetFileInformationByHandle(HANDLE hFile, LPBY_HANDLE_FILE_INFORMATION lpFileInformation)
    {
        auto raddr = _ReturnAddress();
        if (!isRecursive(raddr) && IsVirtualHandle(hFile))
        {
            if (!lpFileInformation)
            {
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }

            if (auto virtualFile = GetVirtualFileByHandle(hFile))
            {
                ZeroMemory(lpFileInformation, sizeof(BY_HANDLE_FILE_INFORMATION));
                lpFileInformation->dwFileAttributes = FILE_ATTRIBUTE_NORMAL;
                lpFileInformation->ftCreationTime = virtualFile->creationTime;
                lpFileInformation->ftLastAccessTime = virtualFile->lastAccessTime;
                lpFileInformation->ftLastWriteTime = virtualFile->lastWriteTime;
                uint64_t size = virtualFile->GetSize();
                lpFileInformation->nFileSizeHigh = static_cast<DWORD>((size >> 32) & 0xFFFFFFFF);
                lpFileInformation->nFileSizeLow = static_cast<DWORD>(size & 0xFFFFFFFF);
                lpFileInformation->nNumberOfLinks = 1;
                return TRUE;
            }

            SetLastError(ERROR_NOT_SUPPORTED);
            return FALSE;
        }
        return mhGetFileInformationByHandle->get_original<decltype(GetFileInformationByHandle)>()(hFile, lpFileInformation);
    }

    BOOL WINAPI shCustomGetFileInformationByHandleEx(HANDLE hFile, FILE_INFO_BY_HANDLE_CLASS FileInformationClass, LPVOID lpFileInformation, DWORD dwBufferSize)
    {
        auto raddr = _ReturnAddress();
        if (!isRecursive(raddr) && IsVirtualHandle(hFile))
        {
            if (FileInformationClass == FileBasicInfo && lpFileInformation && dwBufferSize >= sizeof(FILE_BASIC_INFO))
            {
                if (auto virtualFile = GetVirtualFileByHandle(hFile))
                {
                    auto fileInfo = static_cast<PFILE_BASIC_INFO>(lpFileInformation);
                    memset(fileInfo, 0, sizeof(FILE_BASIC_INFO));
                    fileInfo->CreationTime.QuadPart = reinterpret_cast<LONGLONG&>(virtualFile->creationTime);
                    fileInfo->LastAccessTime.QuadPart = reinterpret_cast<LONGLONG&>(virtualFile->lastAccessTime);
                    fileInfo->LastWriteTime.QuadPart = reinterpret_cast<LONGLONG&>(virtualFile->lastWriteTime);
                    fileInfo->FileAttributes = FILE_ATTRIBUTE_NORMAL;
                    return TRUE;
                }
            }
            else if (FileInformationClass == FileStandardInfo && lpFileInformation && dwBufferSize >= sizeof(FILE_STANDARD_INFO))
            {
                if (auto virtualFile = GetVirtualFileByHandle(hFile))
                {
                    auto fileInfo = static_cast<PFILE_STANDARD_INFO>(lpFileInformation);
                    memset(fileInfo, 0, sizeof(FILE_STANDARD_INFO));
                    uint64_t size = virtualFile->GetSize();
                    fileInfo->AllocationSize.QuadPart = static_cast<LONGLONG>(size);
                    fileInfo->EndOfFile.QuadPart = static_cast<LONGLONG>(size);
                    fileInfo->NumberOfLinks = 1;
                    fileInfo->DeletePending = FALSE;
                    fileInfo->Directory = FALSE;
                    return TRUE;
                }
            }

            SetLastError(ERROR_NOT_SUPPORTED);
            return FALSE;
        }
        return mhGetFileInformationByHandleEx->get_original<decltype(GetFileInformationByHandleEx)>()(hFile, FileInformationClass, lpFileInformation, dwBufferSize);
    }

    DWORD WINAPI shCustomGetFileType(HANDLE hFile)
    {
        auto raddr = _ReturnAddress();
        if (!isRecursive(raddr) && IsVirtualHandle(hFile))
        {
            return FILE_TYPE_DISK;
        }
        return mhGetFileType->get_original<decltype(GetFileType)>()(hFile);
    }

    void HookAPIForOverload()
    {
        static bool apiOverloadHooksActive = false;
        if (apiOverloadHooksActive)
            return;

        if (sActiveDirectories.empty() && !HasVirtualPaths())
            return;

        std::wstring mutexName = L"Ultimate-ASI-Loader-OverloadFromFolder" + std::to_wstring(GetCurrentProcessId());

        auto hMutex = OpenMutexW(MUTEX_ALL_ACCESS, FALSE, mutexName.c_str());
        if (hMutex)
        {
            CloseHandle(hMutex);
            return;
        }

        hMutex = CreateMutexW(nullptr, TRUE, mutexName.c_str());
        if (!hMutex || GetLastError() == ERROR_ALREADY_EXISTS)
        {
            if (hMutex)
                CloseHandle(hMutex);
            return;
        }

        std::vector<std::tuple<std::wstring, std::unique_ptr<FunctionHookMinHook>&, std::string, uintptr_t>> hookFunctions =
        {
            { L"KernelBase.dll", mhLoadLibraryExA,       "LoadLibraryExA",       (uintptr_t)shCustomLoadLibraryExA       },
            { L"KernelBase.dll", mhLoadLibraryExW,       "LoadLibraryExW",       (uintptr_t)shCustomLoadLibraryExW       },
            { L"KernelBase.dll", mhCreateFileA,          "CreateFileA",          (uintptr_t)shCustomCreateFileA          },
            { L"KernelBase.dll", mhCreateFileW,          "CreateFileW",          (uintptr_t)shCustomCreateFileW          },
            { L"KernelBase.dll", mhCreateFile2,          "CreateFile2",          (uintptr_t)shCustomCreateFile2          },
            { L"KernelBase.dll", mhCreateFile3,          "CreateFile3",          (uintptr_t)shCustomCreateFile3          },
            { L"KernelBase.dll", mhGetFileAttributesA,   "GetFileAttributesA",   (uintptr_t)shCustomGetFileAttributesA   },
            { L"KernelBase.dll", mhGetFileAttributesW,   "GetFileAttributesW",   (uintptr_t)shCustomGetFileAttributesW   },
            { L"KernelBase.dll", mhGetFileAttributesExA, "GetFileAttributesExA", (uintptr_t)shCustomGetFileAttributesExA },
            { L"KernelBase.dll", mhGetFileAttributesExW, "GetFileAttributesExW", (uintptr_t)shCustomGetFileAttributesExW },
            { L"KernelBase.dll", mhFindFirstFileA,       "FindFirstFileA",       (uintptr_t)shCustomFindFirstFileA       },
            { L"KernelBase.dll", mhFindNextFileA,        "FindNextFileA",        (uintptr_t)shCustomFindNextFileA        },
            { L"KernelBase.dll", mhFindFirstFileW,       "FindFirstFileW",       (uintptr_t)shCustomFindFirstFileW       },
            { L"KernelBase.dll", mhFindNextFileW,        "FindNextFileW",        (uintptr_t)shCustomFindNextFileW        },
            { L"KernelBase.dll", mhFindFirstFileExA,     "FindFirstFileExA",     (uintptr_t)shCustomFindFirstFileExA     },
            { L"KernelBase.dll", mhFindFirstFileExW,     "FindFirstFileExW",     (uintptr_t)shCustomFindFirstFileExW     },
            { L"KernelBase.dll", mhFindClose,            "FindClose",            (uintptr_t)shCustomFindClose            },
        };

        for (auto& [dllName, hookVar, funcName, customHook] : hookFunctions)
        {
            if (auto pDll = GetModuleHandleW(dllName.c_str()))
            {
                if (!hookVar)
                {
                    if (auto pFunc = (uintptr_t)GetProcAddress(pDll, funcName.c_str()))
                    {
                        hookVar = std::make_unique<FunctionHookMinHook>(pFunc, customHook);
                        if (hookVar)
                            hookVar->create();
                    }
                }
            }
        }

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

        apiOverloadHooksActive = true;
    }

    void HookAPIForVirtualFiles()
    {
        static bool virtualFileHooksActive = false;
        bool shouldHaveHooks = HasVirtualFiles();

        if (!virtualFileHooksActive && shouldHaveHooks)
        {
            std::wstring mutexName = L"Ultimate-ASI-Loader-VirtualFiles" + std::to_wstring(GetCurrentProcessId());

            auto hVirtualFilesMutex = OpenMutexW(MUTEX_ALL_ACCESS, FALSE, mutexName.c_str());
            if (hVirtualFilesMutex)
            {
                CloseHandle(hVirtualFilesMutex);
                hVirtualFilesMutex = nullptr;
                return;
            }

            hVirtualFilesMutex = CreateMutexW(nullptr, TRUE, mutexName.c_str());
            if (!hVirtualFilesMutex || GetLastError() == ERROR_ALREADY_EXISTS)
            {
                if (hVirtualFilesMutex) CloseHandle(hVirtualFilesMutex);
                hVirtualFilesMutex = nullptr;
                return;
            }

            std::vector<std::tuple<std::wstring, std::unique_ptr<FunctionHookMinHook>&, std::string, uintptr_t>> hookFunctions =
            {
                { L"KernelBase.dll", mhCreateFileA,                  "CreateFileA",                  (uintptr_t)shCustomCreateFileA,                  },
                { L"KernelBase.dll", mhCreateFileW,                  "CreateFileW",                  (uintptr_t)shCustomCreateFileW,                  },
                { L"KernelBase.dll", mhCreateFile2,                  "CreateFile2",                  (uintptr_t)shCustomCreateFile2,                  },
                { L"KernelBase.dll", mhCreateFile3,                  "CreateFile3",                  (uintptr_t)shCustomCreateFile3,                  },
                { L"KernelBase.dll", mhGetFileAttributesA,           "GetFileAttributesA",           (uintptr_t)shCustomGetFileAttributesA,           },
                { L"KernelBase.dll", mhGetFileAttributesW,           "GetFileAttributesW",           (uintptr_t)shCustomGetFileAttributesW,           },
                { L"KernelBase.dll", mhGetFileAttributesExA,         "GetFileAttributesExA",         (uintptr_t)shCustomGetFileAttributesExA,         },
                { L"KernelBase.dll", mhGetFileAttributesExW,         "GetFileAttributesExW",         (uintptr_t)shCustomGetFileAttributesExW,         },
                { L"KernelBase.dll", mhReadFile,                     "ReadFile",                     (uintptr_t)shCustomReadFile,                     },
                { L"KernelBase.dll", mhReadFileEx,                   "ReadFileEx",                   (uintptr_t)shCustomReadFileEx,                   },
                { L"KernelBase.dll", mhGetFileSize,                  "GetFileSize",                  (uintptr_t)shCustomGetFileSize,                  },
                { L"KernelBase.dll", mhGetFileSizeEx,                "GetFileSizeEx",                (uintptr_t)shCustomGetFileSizeEx,                },
                { L"KernelBase.dll", mhSetFilePointer,               "SetFilePointer",               (uintptr_t)shCustomSetFilePointer,               },
                { L"KernelBase.dll", mhSetFilePointerEx,             "SetFilePointerEx",             (uintptr_t)shCustomSetFilePointerEx,             },
                { L"KernelBase.dll", mhCloseHandle,                  "CloseHandle",                  (uintptr_t)shCustomCloseHandle,                  },
                { L"KernelBase.dll", mhGetFileInformationByHandle,   "GetFileInformationByHandle",   (uintptr_t)shCustomGetFileInformationByHandle,   },
                { L"KernelBase.dll", mhGetFileInformationByHandleEx, "GetFileInformationByHandleEx", (uintptr_t)shCustomGetFileInformationByHandleEx, },
                { L"KernelBase.dll", mhGetFileType,                  "GetFileType",                  (uintptr_t)shCustomGetFileType,                  },
            };

            if (shouldHaveHooks && !virtualFileHooksActive)
            {
                for (auto& [dllName, hookVar, funcName, customHook] : hookFunctions)
                {
                    if (auto pDll = GetModuleHandleW(dllName.c_str()))
                    {
                        if (!hookVar)
                        {
                            if (auto pFunc = (uintptr_t)GetProcAddress(pDll, funcName.c_str()))
                            {
                                hookVar = std::make_unique<FunctionHookMinHook>(pFunc, customHook);
                                if (hookVar)
                                    hookVar->create();
                            }
                        }
                    }
                }

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

                virtualFileHooksActive = true;
            }
        }
    }

    #if !X64
    bool InitializeServerConnection()
    {
        static std::once_flag flag;
        bool result = false;

        std::call_once(flag, [&]()
        {
            std::lock_guard<std::mutex> lock(serverMutex);
            std::wstring mutexName = L"Global\\Ultimate-ASI-Loader-VirtualFileClientMutex" + std::to_wstring(GetCurrentProcessId());
            HANDLE hMutex = CreateMutexW(NULL, TRUE, mutexName.c_str());
            if (GetLastError() == ERROR_ALREADY_EXISTS)
            {
                CloseHandle(hMutex);
                return;
            }

            auto CreateProcessInJob = [](HANDLE hJob, LPCTSTR lpApplicationName, LPTSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes,
                LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment,
                LPCTSTR lpCurrentDirectory, LPSTARTUPINFO lpStartupInfo, LPPROCESS_INFORMATION ppi) -> BOOL
            {
                BOOL fRc = CreateProcess(lpApplicationName, lpCommandLine, lpProcessAttributes,
                    lpThreadAttributes, bInheritHandles, dwCreationFlags | CREATE_SUSPENDED, lpEnvironment, lpCurrentDirectory,
                    lpStartupInfo, ppi);
                if (fRc)
                {
                    fRc = AssignProcessToJobObject(hJob, ppi->hProcess);
                    if (fRc && !(dwCreationFlags & CREATE_SUSPENDED))
                    {
                        fRc = ResumeThread(ppi->hThread) != (DWORD)-1;
                    }
                    if (!fRc)
                    {
                        TerminateProcess(ppi->hProcess, 0);
                        CloseHandle(ppi->hProcess);
                        CloseHandle(ppi->hThread);
                        ppi->hProcess = ppi->hThread = nullptr;
                    }
                }
                return fRc;
            };

            std::error_code ec;
            auto processPath = std::filesystem::path(GetModulePath(hm)).replace_extension(L".exe");
            auto workingDir = std::filesystem::path(processPath).remove_filename();
            if (std::filesystem::exists(processPath, ec))
            {
                HANDLE hJob = CreateJobObject(nullptr, nullptr);
                JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = { };
                info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
                SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &info, sizeof(info));

                STARTUPINFOW si = { sizeof(si) };
                PROCESS_INFORMATION pi;
                if (CreateProcessInJob(hJob, processPath.c_str(), NULL, nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                    nullptr, workingDir.c_str(), &si, &pi))
                {
                    CloseHandle(pi.hProcess);
                    CloseHandle(pi.hThread);

                    // Retry loop for pipe creation
                    constexpr DWORD max_attempts = 10;
                    DWORD retry_delay_ms = 100;
                    for (DWORD attempt = 0; attempt < max_attempts; ++attempt)
                    {
                        serverPipe = CreateFileW(L"\\\\.\\pipe\\Ultimate-ASI-Loader-VirtualFileServer", GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
                        if (serverPipe != INVALID_HANDLE_VALUE)
                        {
                            DWORD pid = GetCurrentProcessId();
                            DWORD bytes;
                            if (WriteFile(serverPipe, &pid, sizeof(pid), &bytes, NULL) && bytes == sizeof(pid))
                            {
                                result = true;
                                break;
                            }
                            CloseHandle(serverPipe);
                            serverPipe = INVALID_HANDLE_VALUE;
                        }
                        Sleep(retry_delay_ms);
                        retry_delay_ms += 50;
                    }
                }
                else
                {
                    CloseHandle(hJob);
                }
            }
            CloseHandle(hMutex);
        });

        return result;
    }
    #endif

    void LoadVirtualFilesFromZip()
    {
        for (const auto& activeDir : sActiveDirectories)
        {
            for (const auto& entry : sFileLoaderEntries)
            {
                if (entry.path == activeDir && entry.isFromZip)
                {
                    for (const auto& archive : entry.archives)
                    {
                        mz_zip_archive zip_archive = {};
                        bool reader_initialized = false;

                        if (archive->is_multi_part)
                        {
                            zip_archive.m_pRead = multipart_zip_read_func;
                            zip_archive.m_pIO_opaque = archive.get();
                            reader_initialized = mz_zip_reader_init(&zip_archive, archive->total_size, 0);
                        }
                        else
                        {
                            reader_initialized = mz_zip_reader_init_file(&zip_archive, archive->parts[0].string().c_str(), 0);
                        }

                        if (reader_initialized)
                        {
                            for (uint32_t i = 0; i < mz_zip_reader_get_num_files(&zip_archive); i++)
                            {
                                mz_zip_archive_file_stat file_stat;
                                if (mz_zip_reader_file_stat(&zip_archive, i, &file_stat) && !mz_zip_reader_is_file_a_directory(&zip_archive, i))
                                {
                                    std::filesystem::path internalPath(file_stat.m_filename);
                                    if (!internalPath.has_root_path() && internalPath.begin() != internalPath.end() && *internalPath.begin() == activeDir)
                                    {
                                        auto virtualPath = lexicallyRelativeCaseIns(internalPath, activeDir);
                                        auto normalizedPath = NormalizePath(virtualPath);
                                        std::unique_lock lock(virtualFilesMutex);
                                        auto it = virtualFilesByPath.find(normalizedPath);
                                        int newPri = entry.priority - 1000;
                                        if (it == virtualFilesByPath.end() || it->second->priority < newPri)
                                        {
                                            auto virtualFile = std::make_shared<VirtualFile>(archive, i, file_stat.m_uncomp_size, newPri);
                                            virtualFilesByPath[normalizedPath] = virtualFile;
                                            if (it == virtualFilesByPath.end())
                                            {
                                                virtualFilesCount.fetch_add(1, std::memory_order_release);
                                            }
                                        }
                                    }
                                }
                            }
                            mz_zip_reader_end(&zip_archive);
                        }
                    }
                }
            }
        }
    }

    bool WINAPI AddVirtualPathForOverload(auto originalPath, auto virtualPath, int priority)
    {
        if (!originalPath || !virtualPath)
            return false;

        auto key = NormalizePath(originalPath);
        if (key.empty())
            return false;

        // Lock both mutexes to check and modify containers
        std::scoped_lock lock(virtualPathMutex, virtualFilesMutex);

        // Check virtual files and remove if lower priority
        auto fileIt = virtualFilesByPath.find(key);
        if (fileIt != virtualFilesByPath.end() && fileIt->second->priority < priority)
        {
            auto virtualFile = fileIt->second;
            // Handle server cleanup if needed
            std::visit([&](const auto& storage)
            {
                using T = std::decay_t<decltype(storage)>;
                if constexpr (std::is_same_v<T, ServerData>)
                {
                    #if !X64
                    VirtualFile::RemoveFileOnServer(storage.server_handle);
                    #endif
                }
            }, virtualFile->storage);

            // Remove associated handles
            for (auto handleIt = virtualFilesByHandle.begin(); handleIt != virtualFilesByHandle.end();)
            {
                if (handleIt->second == virtualFile)
                {
                    handleIt = virtualFilesByHandle.erase(handleIt);
                }
                else
                {
                    ++handleIt;
                }
            }

            virtualFilesByPath.erase(fileIt);
            virtualFilesCount.fetch_sub(1, std::memory_order_release);
        }

        // Check existing virtual path
        auto pathIt = virtualPathMappings.find(key);
        bool isNew = (pathIt == virtualPathMappings.end());

        if (!isNew && priority <= pathIt->second.second)
            return false; // Existing path has higher or equal priority

        auto vpath = std::filesystem::path(virtualPath);
        if (vpath.is_relative())
            vpath = gamePath / vpath;

        virtualPathMappings[key] = { vpath, priority };

        if (isNew)
        {
            virtualPathsCount.fetch_add(1, std::memory_order_release);
        }

        if (sActiveDirectories.empty() && virtualPathsCount.load(std::memory_order_acquire) == 1)
        {
            HookAPIForOverload();
        }

        return true;
    }

    void WINAPI RemoveVirtualPathFromOverload(auto originalPath)
    {
        if (!originalPath)
            return;

        auto key = NormalizePath(originalPath);
        if (key.empty())
            return;

        std::unique_lock lock(virtualPathMutex);
        if (virtualPathMappings.erase(key))
        {
            virtualPathsCount.fetch_sub(1, std::memory_order_release);
        }
    }

    bool WINAPI AddVirtualPathForOverloadA(const char* originalPath, const char* virtualPath, int priority)
    {
        return AddVirtualPathForOverload(originalPath, virtualPath, priority);
    }

    void WINAPI RemoveVirtualPathFromOverloadA(const char* originalPath)
    {
        return RemoveVirtualPathFromOverload(originalPath);
    }

    bool WINAPI AddVirtualPathForOverloadW(const wchar_t* originalPath, const wchar_t* virtualPath, int priority)
    {
        return AddVirtualPathForOverload(originalPath, virtualPath, priority);
    }

    void WINAPI RemoveVirtualPathFromOverloadW(const wchar_t* originalPath)
    {
        return RemoveVirtualPathFromOverload(originalPath);
    }

    bool WINAPI AddVirtualFileForOverload(auto virtualPath, const uint8_t* data, size_t size, int priority)
    {
        if (!virtualPath || !data || size == 0)
            return false;

        try
        {
            auto filePath = NormalizePath(virtualPath);
            if (filePath.empty())
                return false;

            // Lock both mutexes to check and modify containers
            std::scoped_lock lock(virtualPathMutex, virtualFilesMutex);

            // Check virtual paths and remove if lower priority
            auto pathIt = virtualPathMappings.find(filePath);
            if (pathIt != virtualPathMappings.end() && pathIt->second.second < priority)
            {
                virtualPathMappings.erase(pathIt);
                virtualPathsCount.fetch_sub(1, std::memory_order_release);
            }

            // Check existing virtual file
            auto fileIt = virtualFilesByPath.find(filePath);
            if (fileIt != virtualFilesByPath.end())
            {
                auto& existingFile = fileIt->second;
                if (existingFile->priority > priority)
                    return false; // Existing has higher priority, skip

                // For append operation, we need to handle different storage types
                std::visit([&](auto& storage)
                {
                    using T = std::decay_t<decltype(storage)>;

                    if constexpr (std::is_same_v<T, LocalData>)
                    {
                        // Append to local data
                        size_t oldSize = storage.data.size();
                        storage.data.resize(oldSize + size);
                        std::memcpy(storage.data.data() + oldSize, data, size);
                    }
                    else if constexpr (std::is_same_v<T, ServerData>)
                    {
                        #if !X64
                        // Send append command to server
                        if (VirtualFile::AppendFileOnServer(storage.server_handle, data, size))
                        {
                            // Update local size tracking
                            storage.size += size;
                        }
                        #endif
                    }
                    else if constexpr (std::is_same_v<T, ZipData>)
                    {
                        // Appending to a file in a zip is not supported.
                    }
                }, existingFile->storage);

                // Update priority if new is higher
                if (priority > existingFile->priority)
                    existingFile->priority = priority;

                return true;
            }
            else
            {
                auto virtualFile = std::make_shared<VirtualFile>(data, size, priority);
                virtualFilesByPath[filePath] = virtualFile;
                virtualFilesCount.fetch_add(1, std::memory_order_release);
                HookAPIForVirtualFiles();
                return true;
            }
        } catch (...)
        {
            return false;
        }
    }

    void WINAPI RemoveVirtualFileFromOverload(auto virtualPath)
    {
        if (!virtualPath)
            return;

        try
        {
            auto filePath = NormalizePath(virtualPath);
            std::unique_lock lock(virtualFilesMutex);

            auto pathIt = virtualFilesByPath.find(filePath);
            if (pathIt != virtualFilesByPath.end())
            {
                auto virtualFile = pathIt->second;

                // Handle server cleanup if needed
                std::visit([&](const auto& storage)
                {
                    using T = std::decay_t<decltype(storage)>;

                    if constexpr (std::is_same_v<T, ServerData>)
                    {
                        #if !X64
                        VirtualFile::RemoveFileOnServer(storage.server_handle);
                        #endif
                    }
                }, virtualFile->storage);

                virtualFilesByPath.erase(pathIt);

                // Remove all handles that reference this virtual file
                for (auto handleIt = virtualFilesByHandle.begin(); handleIt != virtualFilesByHandle.end();)
                {
                    if (handleIt->second == virtualFile)
                    {
                        handleIt = virtualFilesByHandle.erase(handleIt);
                    }
                    else
                    {
                        ++handleIt;
                    }
                }

                virtualFilesCount.fetch_sub(1, std::memory_order_release);
            }
        } catch (...)
        {
        }
    }

    bool WINAPI AddVirtualFileForOverloadA(const char* virtualPath, const uint8_t* data, size_t size, int priority)
    {
        return AddVirtualFileForOverload(virtualPath, data, size, priority);
    }

    void WINAPI RemoveVirtualFileFromOverloadA(const char* virtualPath)
    {
        return RemoveVirtualFileFromOverload(virtualPath);
    }

    bool WINAPI AddVirtualFileForOverloadW(const wchar_t* virtualPath, const uint8_t* data, size_t size, int priority)
    {
        return AddVirtualFileForOverload(virtualPath, data, size, priority);
    }

    void WINAPI RemoveVirtualFileFromOverloadW(const wchar_t* virtualPath)
    {
        return RemoveVirtualFileFromOverload(virtualPath);
    }
}

const std::unordered_map<std::string, void*> kernel32Customs = {
    { "GetStartupInfoA",            (void*)CustomGetStartupInfoA            },
    { "GetStartupInfoW",            (void*)CustomGetStartupInfoW            },
    { "GetModuleHandleA",           (void*)CustomGetModuleHandleA           },
    { "GetModuleHandleW",           (void*)CustomGetModuleHandleW           },
    { "GetProcAddress",             (void*)CustomGetProcAddress             },
    { "GetShortPathNameA",          (void*)CustomGetShortPathNameA          },
    { "FindFirstFileA",             (void*)CustomFindFirstFileA             },
    { "FindNextFileA",              (void*)CustomFindNextFileA              },
    { "FindFirstFileW",             (void*)CustomFindFirstFileW             },
    { "FindNextFileW",              (void*)CustomFindNextFileW              },
    { "FindFirstFileExA",           (void*)CustomFindFirstFileExA           },
    { "FindFirstFileExW",           (void*)CustomFindFirstFileExW           },
    { "LoadLibraryExA",             (void*)CustomLoadLibraryExA             },
    { "LoadLibraryExW",             (void*)CustomLoadLibraryExW             },
    { "LoadLibraryA",               (void*)CustomLoadLibraryA               },
    { "LoadLibraryW",               (void*)CustomLoadLibraryW               },
    { "FreeLibrary",                (void*)CustomFreeLibrary                },
    { "CreateEventA",               (void*)CustomCreateEventA               },
    { "CreateEventW",               (void*)CustomCreateEventW               },
    { "GetSystemInfo",              (void*)CustomGetSystemInfo              },
    { "InterlockedCompareExchange", (void*)CustomInterlockedCompareExchange },
    { "Sleep",                      (void*)CustomSleep                      },
    { "GetSystemTimeAsFileTime",    (void*)CustomGetSystemTimeAsFileTime    },
    { "GetCurrentProcessId",        (void*)CustomGetCurrentProcessId        },
    { "GetCommandLineA",            (void*)CustomGetCommandLineA            },
    { "GetCommandLineW",            (void*)CustomGetCommandLineW            },
    { "AcquireSRWLockExclusive",    (void*)CustomAcquireSRWLockExclusive    },
    { "CreateFileA",                (void*)CustomCreateFileA                },
    { "CreateFileW",                (void*)CustomCreateFileW                },
    { "GetFileAttributesA",         (void*)CustomGetFileAttributesA         },
    { "GetFileAttributesW",         (void*)CustomGetFileAttributesW         },
    { "GetFileAttributesExA",       (void*)CustomGetFileAttributesExA       },
    { "GetFileAttributesExW",       (void*)CustomGetFileAttributesExW       },
};

const std::unordered_map<std::string, void*> ole32Customs = {
    { "CoCreateInstance", (void*)CustomCoCreateInstance },
};

const std::unordered_map<std::string, void*> vccorlibCustoms = {
    { "?GetCmdArguments@Details@Platform@@YAPEAPEA_WPEAH@Z", (void*)CustomGetCmdArguments },
};

bool PatchKernel32IAT(HMODULE mod, ModuleIATData& data)
{
    auto hExecutableInstance = (size_t)mod;
    IMAGE_NT_HEADERS* ntHeader = (IMAGE_NT_HEADERS*)(hExecutableInstance + ((IMAGE_DOS_HEADER*)hExecutableInstance)->e_lfanew);
    IMAGE_IMPORT_DESCRIPTOR* pImports = (IMAGE_IMPORT_DESCRIPTOR*)(hExecutableInstance + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    size_t nNumImports = 0;
    if (ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress != 0)
    {
        IMAGE_IMPORT_DESCRIPTOR* importDesc = pImports;
        while (importDesc->Name != 0)
        {
            nNumImports++;
            importDesc++;
        }
    }
    uint32_t matchedImports = 0;
    auto getSectionEnd = [](IMAGE_NT_HEADERS* ntHeader, size_t inst) -> size_t
    {
        auto sec = reinterpret_cast<PIMAGE_SECTION_HEADER>((UCHAR*)ntHeader->OptionalHeader.DataDirectory + ntHeader->OptionalHeader.NumberOfRvaAndSizes * sizeof(IMAGE_DATA_DIRECTORY) + (ntHeader->FileHeader.NumberOfSections - 1) * sizeof(IMAGE_SECTION_HEADER));
        while (sec->Misc.VirtualSize == 0) sec--;
        auto secSize = max(sec->SizeOfRawData, sec->Misc.VirtualSize);
        auto end = inst + max(sec->PointerToRawData, sec->VirtualAddress) + secSize;
        return end;
    };
    auto hExecutableInstance_end = getSectionEnd(ntHeader, hExecutableInstance);
    for (size_t i = 0; i < nNumImports; i++)
    {
        if ((size_t)(hExecutableInstance + (pImports + i)->Name) < hExecutableInstance_end)
        {
            auto szName = (const char*)(hExecutableInstance + (pImports + i)->Name);
            auto dllname = std::string(szName);
            std::transform(dllname.begin(), dllname.end(), dllname.begin(), [](char c) { return ::tolower(c); });
            if (dllname == "kernel32.dll")
            {
                // Try to use OriginalFirstThunk if available (unbound executables)
                if ((pImports + i)->OriginalFirstThunk != 0)
                {
                    PIMAGE_THUNK_DATA thunk = (PIMAGE_THUNK_DATA)(hExecutableInstance + (pImports + i)->OriginalFirstThunk);
                    size_t* iat = (size_t*)(hExecutableInstance + (pImports + i)->FirstThunk);
                    size_t j = 0;
                    while (thunk && thunk->u1.Function && (size_t)&iat[j] < hExecutableInstance_end)
                    {
                        if ((thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) == 0)
                        {
                            auto importName = (PIMAGE_IMPORT_BY_NAME)(hExecutableInstance + thunk->u1.AddressOfData);
                            if ((size_t)importName < hExecutableInstance_end)
                            {
                                std::string funcName = importName->Name;
                                auto it = kernel32Customs.find(funcName);
                                if (it != kernel32Customs.end())
                                {
                                    DWORD dwProtect[2];
                                    VirtualProtect(&iat[j], sizeof(size_t), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
                                    size_t original = iat[j];
                                    iat[j] = (size_t)it->second;
                                    data.kernel32Functions[funcName] = { (size_t)&iat[j], original };
                                    VirtualProtect(&iat[j], sizeof(size_t), dwProtect[0], &dwProtect[1]);
                                    matchedImports++;
                                }
                            }
                        }
                        j++;
                        thunk++;
                    }
                }
                else if ((pImports + i)->FirstThunk != 0) // Fallback to FirstThunk for bound executables (where OriginalFirstThunk is null)
                {
                    auto pFunctions = reinterpret_cast<void**>(hExecutableInstance + (pImports + i)->FirstThunk);
                    for (ptrdiff_t j = 0; pFunctions[j] != nullptr && (size_t)&pFunctions[j] < hExecutableInstance_end; j++)
                    {
                        auto pAddress = &pFunctions[j];
                        for (const auto& [funcName, replacement] : kernel32Customs)
                        {
                            if (*pAddress && *pAddress == (void*)GetProcAddress(GetModuleHandleA("kernel32.dll"), funcName.c_str()))
                            {
                                DWORD dwProtect[2];
                                VirtualProtect(pAddress, sizeof(void*), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
                                void* original = *pAddress;
                                *pAddress = replacement;
                                data.kernel32Functions[funcName] = { (uintptr_t)pAddress, (uintptr_t)original };
                                VirtualProtect(pAddress, sizeof(void*), dwProtect[0], &dwProtect[1]);
                                matchedImports++;
                                break;
                            }
                        }
                    }
                }
                else // Third fallback: brute-force section scan (only if previous fallbacks fail)
                {
                    static auto getSection = [](const PIMAGE_NT_HEADERS nt_headers, unsigned section) -> PIMAGE_SECTION_HEADER
                    {
                        return reinterpret_cast<PIMAGE_SECTION_HEADER>(
                            (UCHAR*)nt_headers->OptionalHeader.DataDirectory +
                            nt_headers->OptionalHeader.NumberOfRvaAndSizes * sizeof(IMAGE_DATA_DIRECTORY) +
                            section * sizeof(IMAGE_SECTION_HEADER));
                    };

                    auto hDll = GetModuleHandleA("kernel32.dll");
                    if (!hDll) continue;

                    for (auto secIdx = 0; secIdx < ntHeader->FileHeader.NumberOfSections; secIdx++)
                    {
                        auto sec = getSection(ntHeader, secIdx);
                        if (!(sec->Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
                        auto pFunctions = reinterpret_cast<void**>(hExecutableInstance + max(sec->PointerToRawData, sec->VirtualAddress));
                        size_t sectionSize = max(sec->SizeOfRawData, sec->Misc.VirtualSize);
                        size_t maxScan = min(300, sectionSize / sizeof(void*)); // Limit to 300 or section size

                        for (ptrdiff_t j = 0; j < maxScan; j++)
                        {
                            auto pAddress = &pFunctions[j];
                            if ((size_t)pAddress >= hExecutableInstance + sectionSize) break; // Bounds check
                            // Reverse lookup for names (similar to second fallback)
                            for (const auto& [funcName, replacement] : kernel32Customs)
                            {
                                if (*pAddress && *pAddress == GetProcAddress(hDll, funcName.c_str()))
                                {
                                    DWORD dwProtect[2];
                                    VirtualProtect(pAddress, sizeof(void*), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
                                    void* original = *pAddress;
                                    *pAddress = replacement;
                                    data.kernel32Functions[funcName] = { (uintptr_t)pAddress, (uintptr_t)original };
                                    VirtualProtect(pAddress, sizeof(void*), dwProtect[0], &dwProtect[1]);
                                    matchedImports++;
                                    break; // Patched, move to next function
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return matchedImports > 0;
}

void PatchCoCreateInstance(HMODULE mod, ModuleIATData& data)
{
    if (iequals(GetSelfName(), L"dsound.dll") || iequals(GetSelfName(), L"dinput8.dll") || iequals(GetSelfName(), L"dinput.dll") || iequals(GetSelfName(), L"wininet.dll"))
        return;
    auto hExecutableInstance = (size_t)mod;
    IMAGE_NT_HEADERS* ntHeader = (IMAGE_NT_HEADERS*)(hExecutableInstance + ((IMAGE_DOS_HEADER*)hExecutableInstance)->e_lfanew);
    IMAGE_IMPORT_DESCRIPTOR* pImports = (IMAGE_IMPORT_DESCRIPTOR*)(hExecutableInstance + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    size_t nNumImports = 0;
    if (ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress != 0)
    {
        IMAGE_IMPORT_DESCRIPTOR* importDesc = pImports;
        while (importDesc->Name != 0)
        {
            nNumImports++;
            importDesc++;
        }
    }
    auto getSectionEnd = [](IMAGE_NT_HEADERS* ntHeader, size_t inst) -> size_t
    {
        auto sec = reinterpret_cast<PIMAGE_SECTION_HEADER>((UCHAR*)ntHeader->OptionalHeader.DataDirectory + ntHeader->OptionalHeader.NumberOfRvaAndSizes * sizeof(IMAGE_DATA_DIRECTORY) + (ntHeader->FileHeader.NumberOfSections - 1) * sizeof(IMAGE_SECTION_HEADER));
        while (sec->Misc.VirtualSize == 0) sec--;
        auto secSize = max(sec->SizeOfRawData, sec->Misc.VirtualSize);
        auto end = inst + max(sec->PointerToRawData, sec->VirtualAddress) + secSize;
        return end;
    };
    auto hExecutableInstance_end = getSectionEnd(ntHeader, hExecutableInstance);
    for (size_t i = 0; i < nNumImports; i++)
    {
        if ((size_t)(hExecutableInstance + (pImports + i)->Name) < hExecutableInstance_end)
        {
            auto szName = (const char*)(hExecutableInstance + (pImports + i)->Name);
            auto dllname = std::string(szName);
            std::transform(dllname.begin(), dllname.end(), dllname.begin(), [](char c) { return ::tolower(c); });
            if (dllname == "ole32.dll")
            {
                // Try to use OriginalFirstThunk if available (unbound executables)
                if ((pImports + i)->OriginalFirstThunk != 0)
                {
                    PIMAGE_THUNK_DATA thunk = (PIMAGE_THUNK_DATA)(hExecutableInstance + (pImports + i)->OriginalFirstThunk);
                    size_t* iat = (size_t*)(hExecutableInstance + (pImports + i)->FirstThunk);
                    size_t j = 0;
                    while (thunk && thunk->u1.Function && (size_t)&iat[j] < hExecutableInstance_end)
                    {
                        if ((thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) == 0)
                        {
                            auto importName = (PIMAGE_IMPORT_BY_NAME)(hExecutableInstance + thunk->u1.AddressOfData);
                            if ((size_t)importName < hExecutableInstance_end)
                            {
                                std::string funcName = importName->Name;
                                auto it = ole32Customs.find(funcName);
                                if (it != ole32Customs.end())
                                {
                                    DWORD dwProtect[2];
                                    VirtualProtect(&iat[j], sizeof(size_t), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
                                    size_t original = iat[j];
                                    iat[j] = (size_t)it->second;
                                    data.ole32Functions[funcName] = { (size_t)&iat[j], original };
                                    VirtualProtect(&iat[j], sizeof(size_t), dwProtect[0], &dwProtect[1]);
                                }
                            }
                        }
                        j++;
                        thunk++;
                    }
                }
                else if ((pImports + i)->FirstThunk != 0) // Fallback to FirstThunk for bound executables (where OriginalFirstThunk is null)
                {
                    auto pFunctions = reinterpret_cast<void**>(hExecutableInstance + (pImports + i)->FirstThunk);
                    for (ptrdiff_t j = 0; pFunctions[j] != nullptr && (size_t)&pFunctions[j] < hExecutableInstance_end; j++)
                    {
                        auto pAddress = &pFunctions[j];
                        for (const auto& [funcName, replacement] : ole32Customs)
                        {
                            if (*pAddress && *pAddress == (void*)GetProcAddress(GetModuleHandleA("ole32.dll"), funcName.c_str()))
                            {
                                DWORD dwProtect[2];
                                VirtualProtect(pAddress, sizeof(void*), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
                                void* original = *pAddress;
                                *pAddress = replacement;
                                data.ole32Functions[funcName] = { (uintptr_t)pAddress, (uintptr_t)original };
                                VirtualProtect(pAddress, sizeof(void*), dwProtect[0], &dwProtect[1]);
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
}

void Patchvccorlib(HMODULE mod, ModuleIATData& data)
{
    auto hExecutableInstance = (size_t)mod;
    IMAGE_NT_HEADERS* ntHeader = (IMAGE_NT_HEADERS*)(hExecutableInstance + ((IMAGE_DOS_HEADER*)hExecutableInstance)->e_lfanew);
    IMAGE_IMPORT_DESCRIPTOR* pImports = (IMAGE_IMPORT_DESCRIPTOR*)(hExecutableInstance + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    size_t nNumImports = 0;
    if (ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress != 0)
    {
        IMAGE_IMPORT_DESCRIPTOR* importDesc = pImports;
        while (importDesc->Name != 0)
        {
            nNumImports++;
            importDesc++;
        }
    }
    auto getSectionEnd = [](IMAGE_NT_HEADERS* ntHeader, size_t inst) -> size_t
    {
        auto sec = reinterpret_cast<PIMAGE_SECTION_HEADER>((UCHAR*)ntHeader->OptionalHeader.DataDirectory + ntHeader->OptionalHeader.NumberOfRvaAndSizes * sizeof(IMAGE_DATA_DIRECTORY) + (ntHeader->FileHeader.NumberOfSections - 1) * sizeof(IMAGE_SECTION_HEADER));
        while (sec->Misc.VirtualSize == 0) sec--;
        auto secSize = max(sec->SizeOfRawData, sec->Misc.VirtualSize);
        auto end = inst + max(sec->PointerToRawData, sec->VirtualAddress) + secSize;
        return end;
    };
    auto hExecutableInstance_end = getSectionEnd(ntHeader, hExecutableInstance);
    for (size_t i = 0; i < nNumImports; i++)
    {
        if ((size_t)(hExecutableInstance + (pImports + i)->Name) < hExecutableInstance_end)
        {
            auto szName = (const char*)(hExecutableInstance + (pImports + i)->Name);
            auto dllname = std::string(szName);
            std::transform(dllname.begin(), dllname.end(), dllname.begin(), [](char c) { return ::tolower(c); });
            if (dllname.find("vccorlib") != std::string::npos)
            {
                auto hHandle = GetModuleHandleA(szName);
                if (!hHandle) return;
                // Try to use OriginalFirstThunk if available (unbound executables)
                if ((pImports + i)->OriginalFirstThunk != 0)
                {
                    PIMAGE_THUNK_DATA thunk = (PIMAGE_THUNK_DATA)(hExecutableInstance + (pImports + i)->OriginalFirstThunk);
                    size_t* iat = (size_t*)(hExecutableInstance + (pImports + i)->FirstThunk);
                    size_t j = 0;
                    while (thunk->u1.Function && (size_t)&iat[j] < hExecutableInstance_end)
                    {
                        if ((thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG) == 0)
                        {
                            auto importName = (PIMAGE_IMPORT_BY_NAME)(hExecutableInstance + thunk->u1.AddressOfData);
                            if ((size_t)importName < hExecutableInstance_end)
                            {
                                std::string funcName = importName->Name;
                                auto it = vccorlibCustoms.find(funcName);
                                if (it != vccorlibCustoms.end())
                                {
                                    DWORD dwProtect[2];
                                    VirtualProtect(&iat[j], sizeof(size_t), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
                                    size_t original = iat[j];
                                    iat[j] = (size_t)it->second;
                                    data.vccorlibFunctions[funcName] = { (size_t)&iat[j], original };
                                    VirtualProtect(&iat[j], sizeof(size_t), dwProtect[0], &dwProtect[1]);
                                }
                            }
                        }
                        j++;
                        thunk++;
                    }
                }
                // Fallback to FirstThunk for bound executables (where OriginalFirstThunk is null)
                else if ((pImports + i)->FirstThunk != 0)
                {
                    auto pFunctions = reinterpret_cast<void**>(hExecutableInstance + (pImports + i)->FirstThunk);
                    for (ptrdiff_t j = 0; pFunctions[j] != nullptr && (size_t)&pFunctions[j] < hExecutableInstance_end; j++)
                    {
                        auto pAddress = &pFunctions[j];
                        for (const auto& [funcName, replacement] : vccorlibCustoms)
                        {
                            if (*pAddress && *pAddress == (void*)GetProcAddress(hHandle, funcName.c_str()))
                            {
                                DWORD dwProtect[2];
                                VirtualProtect(pAddress, sizeof(void*), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
                                void* original = *pAddress;
                                *pAddress = replacement;
                                data.vccorlibFunctions[funcName] = { (uintptr_t)pAddress, (uintptr_t)original };
                                VirtualProtect(pAddress, sizeof(void*), dwProtect[0], &dwProtect[1]);
                                break; // Found and patched, move to next function
                            }
                        }
                    }
                }
            }
        }
    }
}

void PatchOrdinals(HMODULE mod)
{
    auto szSelfName = GetSelfName();
    auto hInstance = (size_t)mod;
    IMAGE_NT_HEADERS* ntHeader = (IMAGE_NT_HEADERS*)(hInstance + ((IMAGE_DOS_HEADER*)hInstance)->e_lfanew);
    IMAGE_IMPORT_DESCRIPTOR* pImports = (IMAGE_IMPORT_DESCRIPTOR*)(hInstance + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    size_t nNumImports = 0;
    if (ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress != 0)
    {
        IMAGE_IMPORT_DESCRIPTOR* importDesc = pImports;
        while (importDesc->Name != 0)
        {
            nNumImports++;
            importDesc++;
        }
    }

    // Lambda to handle ordinal patching (keeps enums and switches intact)
    auto patchOrdinal = [&](void** p, WORD ordinal)
    {
        DWORD Protect;
        VirtualProtect(p, 4, PAGE_EXECUTE_READWRITE, &Protect);

        if (iequals(szSelfName, L"dsound.dll"))
        {
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
            switch (ordinal)
            {
                case edsound::DirectSoundCaptureCreate: *p = _DirectSoundCaptureCreate; break;
                case edsound::DirectSoundCaptureCreate8: *p = _DirectSoundCaptureCreate8; break;
                case edsound::DirectSoundCaptureEnumerateA: *p = _DirectSoundCaptureEnumerateA; break;
                case edsound::DirectSoundCaptureEnumerateW: *p = _DirectSoundCaptureEnumerateW; break;
                case edsound::DirectSoundCreate: *p = _DirectSoundCreate; break;
                case edsound::DirectSoundCreate8: *p = _DirectSoundCreate8; break;
                case edsound::DirectSoundEnumerateA: *p = _DirectSoundEnumerateA; break;
                case edsound::DirectSoundEnumerateW: *p = _DirectSoundEnumerateW; break;
                case edsound::DirectSoundFullDuplexCreate: *p = _DirectSoundFullDuplexCreate; break;
                case edsound::GetDeviceID: *p = _GetDeviceID; break;
                default: break;
            }
        }
        else if (iequals(szSelfName, L"dinput8.dll"))
        {
            if (ordinal == 1) *p = _DirectInput8Create;
        }
        else if (iequals(szSelfName, L"winhttp.dll"))
        {
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
            switch (ordinal)
            {
                case ewinhttp::Private1: *p = _Private1; break;
                case ewinhttp::SvchostPushServiceGlobals: *p = _SvchostPushServiceGlobals; break;
                case ewinhttp::WinHttpAddRequestHeaders: *p = _WinHttpAddRequestHeaders; break;
                case ewinhttp::WinHttpAddRequestHeadersEx: *p = _WinHttpAddRequestHeadersEx; break;
                case ewinhttp::WinHttpAutoProxySvcMain: *p = _WinHttpAutoProxySvcMain; break;
                case ewinhttp::WinHttpCheckPlatform: *p = _WinHttpCheckPlatform; break;
                case ewinhttp::WinHttpCloseHandle: *p = _WinHttpCloseHandle; break;
                case ewinhttp::WinHttpConnect: *p = _WinHttpConnect; break;
                case ewinhttp::WinHttpConnectionDeletePolicyEntries: *p = _WinHttpConnectionDeletePolicyEntries; break;
                case ewinhttp::WinHttpConnectionDeleteProxyInfo: *p = _WinHttpConnectionDeleteProxyInfo; break;
                case ewinhttp::WinHttpConnectionFreeNameList: *p = _WinHttpConnectionFreeNameList; break;
                case ewinhttp::WinHttpConnectionFreeProxyInfo: *p = _WinHttpConnectionFreeProxyInfo; break;
                case ewinhttp::WinHttpConnectionFreeProxyList: *p = _WinHttpConnectionFreeProxyList; break;
                case ewinhttp::WinHttpConnectionGetNameList: *p = _WinHttpConnectionGetNameList; break;
                case ewinhttp::WinHttpConnectionGetProxyInfo: *p = _WinHttpConnectionGetProxyInfo; break;
                case ewinhttp::WinHttpConnectionGetProxyList: *p = _WinHttpConnectionGetProxyList; break;
                case ewinhttp::WinHttpConnectionOnlyConvert: *p = _WinHttpConnectionOnlyConvert; break;
                case ewinhttp::WinHttpConnectionOnlyReceive: *p = _WinHttpConnectionOnlyReceive; break;
                case ewinhttp::WinHttpConnectionOnlySend: *p = _WinHttpConnectionOnlySend; break;
                case ewinhttp::WinHttpConnectionSetPolicyEntries: *p = _WinHttpConnectionSetPolicyEntries; break;
                case ewinhttp::WinHttpConnectionSetProxyInfo: *p = _WinHttpConnectionSetProxyInfo; break;
                case ewinhttp::WinHttpConnectionUpdateIfIndexTable: *p = _WinHttpConnectionUpdateIfIndexTable; break;
                case ewinhttp::WinHttpCrackUrl: *p = _WinHttpCrackUrl; break;
                case ewinhttp::WinHttpCreateProxyResolver: *p = _WinHttpCreateProxyResolver; break;
                case ewinhttp::WinHttpCreateUrl: *p = _WinHttpCreateUrl; break;
                case ewinhttp::WinHttpDetectAutoProxyConfigUrl: *p = _WinHttpDetectAutoProxyConfigUrl; break;
                case ewinhttp::WinHttpFreeProxyResult: *p = _WinHttpFreeProxyResult; break;
                case ewinhttp::WinHttpFreeProxyResultEx: *p = _WinHttpFreeProxyResultEx; break;
                case ewinhttp::WinHttpFreeProxySettings: *p = _WinHttpFreeProxySettings; break;
                case ewinhttp::WinHttpFreeProxySettingsEx: *p = _WinHttpFreeProxySettingsEx; break;
                case ewinhttp::WinHttpFreeQueryConnectionGroupResult: *p = _WinHttpFreeQueryConnectionGroupResult; break;
                case ewinhttp::WinHttpGetDefaultProxyConfiguration: *p = _WinHttpGetDefaultProxyConfiguration; break;
                case ewinhttp::WinHttpGetIEProxyConfigForCurrentUser: *p = _WinHttpGetIEProxyConfigForCurrentUser; break;
                case ewinhttp::WinHttpGetProxyForUrl: *p = _WinHttpGetProxyForUrl; break;
                case ewinhttp::WinHttpGetProxyForUrlEx: *p = _WinHttpGetProxyForUrlEx; break;
                case ewinhttp::WinHttpGetProxyForUrlEx2: *p = _WinHttpGetProxyForUrlEx2; break;
                case ewinhttp::WinHttpGetProxyForUrlHvsi: *p = _WinHttpGetProxyForUrlHvsi; break;
                case ewinhttp::WinHttpGetProxyResult: *p = _WinHttpGetProxyResult; break;
                case ewinhttp::WinHttpGetProxyResultEx: *p = _WinHttpGetProxyResultEx; break;
                case ewinhttp::WinHttpGetProxySettingsEx: *p = _WinHttpGetProxySettingsEx; break;
                case ewinhttp::WinHttpGetProxySettingsResultEx: *p = _WinHttpGetProxySettingsResultEx; break;
                case ewinhttp::WinHttpGetProxySettingsVersion: *p = _WinHttpGetProxySettingsVersion; break;
                case ewinhttp::WinHttpGetTunnelSocket: *p = _WinHttpGetTunnelSocket; break;
                case ewinhttp::WinHttpOpen: *p = _WinHttpOpen; break;
                case ewinhttp::WinHttpOpenRequest: *p = _WinHttpOpenRequest; break;
                case ewinhttp::WinHttpPacJsWorkerMain: *p = _WinHttpPacJsWorkerMain; break;
                case ewinhttp::WinHttpProbeConnectivity: *p = _WinHttpProbeConnectivity; break;
                case ewinhttp::WinHttpQueryAuthSchemes: *p = _WinHttpQueryAuthSchemes; break;
                case ewinhttp::WinHttpQueryConnectionGroup: *p = _WinHttpQueryConnectionGroup; break;
                case ewinhttp::WinHttpQueryDataAvailable: *p = _WinHttpQueryDataAvailable; break;
                case ewinhttp::WinHttpQueryHeaders: *p = _WinHttpQueryHeaders; break;
                case ewinhttp::WinHttpQueryHeadersEx: *p = _WinHttpQueryHeadersEx; break;
                case ewinhttp::WinHttpQueryOption: *p = _WinHttpQueryOption; break;
                case ewinhttp::WinHttpReadData: *p = _WinHttpReadData; break;
                case ewinhttp::WinHttpReadDataEx: *p = _WinHttpReadDataEx; break;
                case ewinhttp::WinHttpReadProxySettings: *p = _WinHttpReadProxySettings; break;
                case ewinhttp::WinHttpReadProxySettingsHvsi: *p = _WinHttpReadProxySettingsHvsi; break;
                case ewinhttp::WinHttpReceiveResponse: *p = _WinHttpReceiveResponse; break;
                case ewinhttp::WinHttpRegisterProxyChangeNotification: *p = _WinHttpRegisterProxyChangeNotification; break;
                case ewinhttp::WinHttpResetAutoProxy: *p = _WinHttpResetAutoProxy; break;
                case ewinhttp::WinHttpSaveProxyCredentials: *p = _WinHttpSaveProxyCredentials; break;
                case ewinhttp::WinHttpSendRequest: *p = _WinHttpSendRequest; break;
                case ewinhttp::WinHttpSetCredentials: *p = _WinHttpSetCredentials; break;
                case ewinhttp::WinHttpSetDefaultProxyConfiguration: *p = _WinHttpSetDefaultProxyConfiguration; break;
                case ewinhttp::WinHttpSetOption: *p = _WinHttpSetOption; break;
                case ewinhttp::WinHttpSetProxySettingsPerUser: *p = _WinHttpSetProxySettingsPerUser; break;
                case ewinhttp::WinHttpSetSecureLegacyServersAppCompat: *p = _WinHttpSetSecureLegacyServersAppCompat; break;
                case ewinhttp::WinHttpSetStatusCallback: *p = _WinHttpSetStatusCallback; break;
                case ewinhttp::WinHttpSetTimeouts: *p = _WinHttpSetTimeouts; break;
                case ewinhttp::WinHttpTimeFromSystemTime: *p = _WinHttpTimeFromSystemTime; break;
                case ewinhttp::WinHttpTimeToSystemTime: *p = _WinHttpTimeToSystemTime; break;
                case ewinhttp::WinHttpUnregisterProxyChangeNotification: *p = _WinHttpUnregisterProxyChangeNotification; break;
                case ewinhttp::WinHttpWebSocketClose: *p = _WinHttpWebSocketClose; break;
                case ewinhttp::WinHttpWebSocketCompleteUpgrade: *p = _WinHttpWebSocketCompleteUpgrade; break;
                case ewinhttp::WinHttpWebSocketQueryCloseStatus: *p = _WinHttpWebSocketQueryCloseStatus; break;
                case ewinhttp::WinHttpWebSocketReceive: *p = _WinHttpWebSocketReceive; break;
                case ewinhttp::WinHttpWebSocketSend: *p = _WinHttpWebSocketSend; break;
                case ewinhttp::WinHttpWebSocketShutdown: *p = _WinHttpWebSocketShutdown; break;
                case ewinhttp::WinHttpWriteData: *p = _WinHttpWriteData; break;
                case ewinhttp::WinHttpWriteProxySettings: *p = _WinHttpWriteProxySettings; break;
                default: break;
            }
        }
        else if (iequals(szSelfName, L"xinput1_1.dll") || iequals(szSelfName, L"xinput1_2.dll"))
        {
            const enum exinput1_1
            {
                DllMain = 1,
                XInputEnable = 2,
                XInputGetCapabilities = 3,
                XInputGetDSoundAudioDeviceGuids = 4,
                XInputGetState = 5,
                XInputSetState = 6,
            };
            switch (ordinal)
            {
                case exinput1_1::DllMain: *p = _DllMain; break;
                case exinput1_1::XInputEnable: *p = _XInputEnable; break;
                case exinput1_1::XInputGetCapabilities: *p = _XInputGetCapabilities; break;
                case exinput1_1::XInputGetDSoundAudioDeviceGuids: *p = _XInputGetDSoundAudioDeviceGuids; break;
                case exinput1_1::XInputGetState: *p = _XInputGetState; break;
                case exinput1_1::XInputSetState: *p = _XInputSetState; break;
                default: break;
            }
        }
        else if (iequals(szSelfName, L"xinput1_3.dll"))
        {
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
            switch (ordinal)
            {
                case exinput1_3::DllMain: *p = _DllMain; break;
                case exinput1_3::XInputGetState: *p = _XInputGetState; break;
                case exinput1_3::XInputSetState: *p = _XInputSetState; break;
                case exinput1_3::XInputGetCapabilities: *p = _XInputGetCapabilities; break;
                case exinput1_3::XInputEnable: *p = _XInputEnable; break;
                case exinput1_3::XInputGetDSoundAudioDeviceGuids: *p = _XInputGetDSoundAudioDeviceGuids; break;
                case exinput1_3::XInputGetBatteryInformation: *p = _XInputGetBatteryInformation; break;
                case exinput1_3::XInputGetKeystroke: *p = _XInputGetKeystroke; break;
                case exinput1_3::XInputGetStateEx: *p = _XInputGetStateEx; break;
                case exinput1_3::XInputWaitForGuideButton: *p = _XInputWaitForGuideButton; break;
                case exinput1_3::XInputCancelGuideButtonWait: *p = _XInputCancelGuideButtonWait; break;
                case exinput1_3::XInputPowerOffController: *p = _XInputPowerOffController; break;
                default: break;
            }
        }
        else if (iequals(szSelfName, L"xinput1_4.dll"))
        {
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
            switch (ordinal)
            {
                case exinput1_4::DllMain: *p = _DllMain; break;
                case exinput1_4::XInputGetState: *p = _XInputGetState; break;
                case exinput1_4::XInputSetState: *p = _XInputSetState; break;
                case exinput1_4::XInputGetCapabilities: *p = _XInputGetCapabilities; break;
                case exinput1_4::XInputEnable: *p = _XInputEnable; break;
                case exinput1_4::XInputGetBatteryInformation: *p = _XInputGetBatteryInformation; break;
                case exinput1_4::XInputGetKeystroke: *p = _XInputGetKeystroke; break;
                case exinput1_4::XInputGetAudioDeviceIds: *p = _XInputGetAudioDeviceIds; break;
                case exinput1_4::XInputGetStateEx: *p = _XInputGetStateEx; break;
                case exinput1_4::XInputWaitForGuideButton: *p = _XInputWaitForGuideButton; break;
                case exinput1_4::XInputCancelGuideButtonWait: *p = _XInputCancelGuideButtonWait; break;
                case exinput1_4::XInputPowerOffController: *p = _XInputPowerOffController; break;
                case exinput1_4::XInputGetBaseBusInformation: *p = _XInputGetBaseBusInformation; break;
                case exinput1_4::XInputGetCapabilitiesEx: *p = _XInputGetCapabilitiesEx; break;
                default: break;
            }
        }
        else if (iequals(szSelfName, L"xinput9_1_0.dll"))
        {
            const enum exinput9_1_0
            {
                DllMain = 1,
                XInputGetCapabilities = 2,
                XInputGetDSoundAudioDeviceGuids = 3,
                XInputGetState = 4,
                XInputSetState = 5,
            };
            switch (ordinal)
            {
                case exinput9_1_0::DllMain: *p = _DllMain; break;
                case exinput9_1_0::XInputGetCapabilities: *p = _XInputGetCapabilities; break;
                case exinput9_1_0::XInputGetDSoundAudioDeviceGuids: *p = _XInputGetDSoundAudioDeviceGuids; break;
                case exinput9_1_0::XInputGetState: *p = _XInputGetState; break;
                case exinput9_1_0::XInputSetState: *p = _XInputSetState; break;
                default: break;
            }
        }
        else if (iequals(szSelfName, L"xinputuap.dll"))
        {
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
            switch (ordinal)
            {
                case exinputuap::DllMain: *p = _DllMain; break;
                case exinputuap::XInputEnable: *p = _XInputEnable; break;
                case exinputuap::XInputGetAudioDeviceIds: *p = _XInputGetAudioDeviceIds; break;
                case exinputuap::XInputGetBatteryInformation: *p = _XInputGetBatteryInformation; break;
                case exinputuap::XInputGetCapabilities: *p = _XInputGetCapabilities; break;
                case exinputuap::XInputGetKeystroke: *p = _XInputGetKeystroke; break;
                case exinputuap::XInputGetState: *p = _XInputGetState; break;
                case exinputuap::XInputSetState: *p = _XInputSetState; break;
                default: break;
            }
        }

        VirtualProtect(p, 4, Protect, &Protect);
    };

    for (size_t i = 0; i < nNumImports; i++)
    {
        auto getSectionEnd = [](IMAGE_NT_HEADERS* ntHeader, size_t inst) -> size_t
        {
            auto sec = reinterpret_cast<PIMAGE_SECTION_HEADER>((UCHAR*)ntHeader->OptionalHeader.DataDirectory + ntHeader->OptionalHeader.NumberOfRvaAndSizes * sizeof(IMAGE_DATA_DIRECTORY) + (ntHeader->FileHeader.NumberOfSections - 1) * sizeof(IMAGE_SECTION_HEADER));
            while (sec->Misc.VirtualSize == 0) sec--;
            auto secSize = max(sec->SizeOfRawData, sec->Misc.VirtualSize);
            auto end = inst + max(sec->PointerToRawData, sec->VirtualAddress) + secSize;
            return end;
        };
        if ((size_t)(hInstance + (pImports + i)->Name) < getSectionEnd(ntHeader, hInstance))
        {
            if (iequals(szSelfName, (to_wstring((const char*)(hInstance + (pImports + i)->Name)))))
            {
                // Try to use OriginalFirstThunk if available (unbound executables)
                if ((pImports + i)->OriginalFirstThunk != 0)
                {
                    PIMAGE_THUNK_DATA thunk = (PIMAGE_THUNK_DATA)(hInstance + (pImports + i)->OriginalFirstThunk);
                    size_t j = 0;
                    while (thunk->u1.Function)
                    {
                        if (thunk->u1.Ordinal & IMAGE_ORDINAL_FLAG)
                        {
                            void** p = (void**)(hInstance + (pImports + i)->FirstThunk);
                            WORD ordinal = IMAGE_ORDINAL(thunk->u1.Ordinal);
                            patchOrdinal(&p[j], ordinal);
                        }
                        j++;
                        thunk++;
                    }
                }
                else if ((pImports + i)->FirstThunk != 0) // Fallback to FirstThunk for bound executables (where OriginalFirstThunk is null)
                {
                    auto pFunctions = reinterpret_cast<void**>(hInstance + (pImports + i)->FirstThunk);
                    auto hDll = GetModuleHandleW(szSelfName.c_str());
                    if (!hDll) continue;

                    // Define expected ordinals for reverse lookup (based on switch cases)
                    std::vector<WORD> expectedOrdinals;
                    if (iequals(szSelfName, L"dsound.dll"))
                        expectedOrdinals = { 1, 6, 7, 8, 9, 10, 11, 12 };
                    else if (iequals(szSelfName, L"dinput8.dll"))
                        expectedOrdinals = { 1 };
                    else if (iequals(szSelfName, L"winhttp.dll"))
                        expectedOrdinals = { 1, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82 };
                    else if (iequals(szSelfName, L"xinput1_1.dll") || iequals(szSelfName, L"xinput1_2.dll"))
                        expectedOrdinals = { 1, 2, 3, 4, 5, 6 };
                    else if (iequals(szSelfName, L"xinput1_3.dll"))
                        expectedOrdinals = { 1, 2, 3, 4, 5, 6, 7, 8, 100, 101, 102, 103 };
                    else if (iequals(szSelfName, L"xinput1_4.dll"))
                        expectedOrdinals = { 1, 2, 3, 4, 5, 7, 8, 10, 100, 101, 102, 103, 104, 108 };
                    else if (iequals(szSelfName, L"xinput9_1_0.dll"))
                        expectedOrdinals = { 1, 2, 3, 4, 5 };
                    else if (iequals(szSelfName, L"xinputuap.dll"))
                        expectedOrdinals = { 1, 2, 3, 4, 5, 6, 7, 8 };

                    for (ptrdiff_t j = 0; pFunctions[j] != nullptr && (size_t)&pFunctions[j] < getSectionEnd(ntHeader, hInstance); j++)
                    {
                        auto pAddress = &pFunctions[j];
                        // Reverse lookup: check if the address matches any expected ordinal
                        for (auto ord : expectedOrdinals)
                        {
                            if (*pAddress == GetProcAddress(hDll, MAKEINTRESOURCEA(ord)))
                            {
                                patchOrdinal(pAddress, ord);
                                break; // Patched, move to next IAT entry
                            }
                        }
                    }
                }
                break;
            }
        }
    }
}

HANDLE g_hHookIATMutex = nullptr;
bool HookIAT()
{
    std::wstring mutexName = L"Ultimate-ASI-Loader-HookIAT" + std::to_wstring(GetCurrentProcessId());

    auto hMutex = OpenMutexW(MUTEX_ALL_ACCESS, FALSE, mutexName.c_str());
    if (hMutex)
    {
        CloseHandle(hMutex);
        return false;
    }

    hMutex = CreateMutexW(nullptr, TRUE, mutexName.c_str());
    if (!hMutex || GetLastError() == ERROR_ALREADY_EXISTS)
    {
        if (hMutex)
            CloseHandle(hMutex);
        return false;
    }

    g_hHookIATMutex = hMutex;

    ModuleList modules;
    modules.Enumerate(ModuleList::SearchLocation::All);
    for (auto& e : modules.m_moduleList)
    {
        auto mod = std::get<HMODULE>(e);
        if (mod != hm)
        {
            auto name = std::get<std::wstring>(e);
            auto bIsLocal = std::get<bool>(e);
            std::transform(name.begin(), name.end(), name.begin(), [](wchar_t c) { return ::towlower(c); });

            if (bIsLocal || name == L"unityplayer" || name == L"clr" || name == L"coreclr")
            {
                // Skip patching mss32 for backwards compatibility
                if (sLoadFromAPI.empty() && name == L"mss32")
                    continue;

                ModuleIATData data;
                data.module = mod;
                PatchKernel32IAT(mod, data);
                Patchvccorlib(mod, data);
                PatchCoCreateInstance(mod, data);
                PatchOrdinals(mod);
                moduleIATs.push_back(data);
            }
        }
    }

    return !moduleIATs.empty();
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
                WriteFile(hFile, buffer, DWORD(strlen(buffer)), &NumberOfBytesWritten, NULL);
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
    OverloadFromFolder::gamePath = std::filesystem::path(GetExeModulePath());
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
    auto nDisableCrashDumps = GetPrivateProfileIntW(TEXT("globalsets"), TEXT("disablecrashdumps"), FALSE, iniPaths);

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
        } catch (...) {}

        bool hookedSuccessfully = HookIAT();

        for (const auto& m : moduleIATs)
        {
            if (m.module == GetModuleHandle(NULL))
            {
                if (m.kernel32Functions.empty() && m.vccorlibFunctions.empty())
                {
                    hookedSuccessfully = false;
                }
                break;
            }
        }

        if (!hookedSuccessfully)
        {
            LoadOriginalLibrary();
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
        for (auto& modData : moduleIATs)
        {
            for (auto& [name, data] : modData.ole32Functions)
                data.Restore();
        }

        if (g_hHookIATMutex)
        {
            CloseHandle(g_hHookIATMutex);
            g_hHookIATMutex = nullptr;
        }
    }
    return TRUE;
}
