#include <windows.h>
#include <mutex>
#include <functional>
#include <vector>
#include <subauth.h>
#include "../../external/ModuleList/ModuleList.hpp"

HMODULE hm = NULL;
HMODULE ual = NULL;

class DllCallbackHandler
{
public:
    static inline void RegisterCallback(std::function<void(HMODULE)>&& fn)
    {
        RegisterDllNotification();
        GetCallbackList().emplace_back(std::forward<std::function<void(HMODULE)>>(fn));
    }
private:
    static inline void callOnLoad(HMODULE mod)
    {
        if (!GetCallbackList().empty())
        {
            for (auto& f : GetCallbackList())
            {
                f(mod);
            }
        }
    }

private:
    static inline std::vector<std::function<void(HMODULE)>>& GetCallbackList()
    {
        return onLoad;
    }

    typedef NTSTATUS(NTAPI* _LdrRegisterDllNotification) (ULONG, PVOID, PVOID, PVOID);
    typedef NTSTATUS(NTAPI* _LdrUnregisterDllNotification) (PVOID);

    typedef struct _LDR_DLL_LOADED_NOTIFICATION_DATA
    {
        ULONG Flags;                    //Reserved.
        PUNICODE_STRING FullDllName;    //The full path name of the DLL module.
        PUNICODE_STRING BaseDllName;    //The base file name of the DLL module.
        PVOID DllBase;                  //A pointer to the base address for the DLL in memory.
        ULONG SizeOfImage;              //The size of the DLL image, in bytes.
    } LDR_DLL_LOADED_NOTIFICATION_DATA, LDR_DLL_UNLOADED_NOTIFICATION_DATA, * PLDR_DLL_LOADED_NOTIFICATION_DATA, * PLDR_DLL_UNLOADED_NOTIFICATION_DATA;

    typedef union _LDR_DLL_NOTIFICATION_DATA
    {
        LDR_DLL_LOADED_NOTIFICATION_DATA Loaded;
        LDR_DLL_UNLOADED_NOTIFICATION_DATA Unloaded;
    } LDR_DLL_NOTIFICATION_DATA, * PLDR_DLL_NOTIFICATION_DATA;

    typedef NTSTATUS(NTAPI* PLDR_MANIFEST_PROBER_ROUTINE) (IN HMODULE DllBase, IN PCWSTR FullDllPath, OUT PHANDLE ActivationContext);
    typedef NTSTATUS(NTAPI* PLDR_ACTX_LANGUAGE_ROURINE) (IN HANDLE Unk, IN USHORT LangID, OUT PHANDLE ActivationContext);
    typedef void(NTAPI* PLDR_RELEASE_ACT_ROUTINE) (IN HANDLE ActivationContext);
    typedef VOID(NTAPI* fnLdrSetDllManifestProber) (IN PLDR_MANIFEST_PROBER_ROUTINE ManifestProberRoutine,
        IN PLDR_ACTX_LANGUAGE_ROURINE CreateActCtxLanguageRoutine, IN PLDR_RELEASE_ACT_ROUTINE ReleaseActCtxRoutine);

private:
    static inline void CALLBACK LdrDllNotification(ULONG NotificationReason, PLDR_DLL_NOTIFICATION_DATA NotificationData, PVOID Context)
    {
        static constexpr auto LDR_DLL_NOTIFICATION_REASON_LOADED = 1;
        static constexpr auto LDR_DLL_NOTIFICATION_REASON_UNLOADED = 2;
        if (NotificationReason == LDR_DLL_NOTIFICATION_REASON_LOADED)
        {
            callOnLoad((HMODULE)NotificationData->Loaded.DllBase);
        }
    }

    static inline NTSTATUS NTAPI ProbeCallback(IN HMODULE DllBase, IN PCWSTR FullDllPath, OUT PHANDLE ActivationContext)
    {
        std::wstring str(FullDllPath);
        callOnLoad(DllBase);

        HANDLE actx = NULL;
        ACTCTXW act = { 0 };

        act.cbSize = sizeof(act);
        act.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID | ACTCTX_FLAG_HMODULE_VALID;
        act.lpSource = FullDllPath;
        act.hModule = DllBase;
        act.lpResourceName = ISOLATIONAWARE_MANIFEST_RESOURCE_ID;
        *ActivationContext = 0;
        actx = CreateActCtxW(&act);
        if (actx == INVALID_HANDLE_VALUE)
            return 0xC000008B; //STATUS_RESOURCE_NAME_NOT_FOUND;
        *ActivationContext = actx;
        return STATUS_SUCCESS;
    }

    static inline void RegisterDllNotification()
    {
        LdrRegisterDllNotification = (_LdrRegisterDllNotification)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "LdrRegisterDllNotification");
        if (LdrRegisterDllNotification)
        {
            if (!cookie)
                LdrRegisterDllNotification(0, LdrDllNotification, 0, &cookie);
        }
        else
        {
            LdrSetDllManifestProber = (fnLdrSetDllManifestProber)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "LdrSetDllManifestProber");
            if (LdrSetDllManifestProber)
            {
                LdrSetDllManifestProber(&ProbeCallback, NULL, &ReleaseActCtx);
            }
        }
    }

    static inline void UnRegisterDllNotification()
    {
        LdrUnregisterDllNotification = (_LdrUnregisterDllNotification)GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "LdrUnregisterDllNotification");
        if (LdrUnregisterDllNotification && cookie)
            LdrUnregisterDllNotification(cookie);
    }

private:
    static inline _LdrRegisterDllNotification   LdrRegisterDllNotification;
    static inline _LdrUnregisterDllNotification LdrUnregisterDllNotification;
    static inline void* cookie;
    static inline fnLdrSetDllManifestProber     LdrSetDllManifestProber;
    static inline std::vector<std::function<void(HMODULE)>> onLoad;
};

typedef HANDLE(WINAPI* tCreateFileA)(LPCSTR lpFilename, DWORD dwAccess, DWORD dwSharing, LPSECURITY_ATTRIBUTES saAttributes, DWORD dwCreation, DWORD dwAttributes, HANDLE hTemplate);
typedef HANDLE(WINAPI* tCreateFileW)(LPCWSTR lpFilename, DWORD dwAccess, DWORD dwSharing, LPSECURITY_ATTRIBUTES saAttributes, DWORD dwCreation, DWORD dwAttributes, HANDLE hTemplate);
typedef DWORD(WINAPI* tGetFileAttributesA)(LPCSTR lpFileName);
typedef DWORD(WINAPI* tGetFileAttributesW)(LPCWSTR lpFileName);
typedef BOOL(WINAPI* tGetFileAttributesExA)(LPCSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation);
typedef BOOL(WINAPI* tGetFileAttributesExW)(LPCWSTR lpFileName, GET_FILEEX_INFO_LEVELS fInfoLevelId, LPVOID lpFileInformation);
tCreateFileA ptrCreateFileA;
tCreateFileW ptrCreateFileW;
tGetFileAttributesA ptrGetFileAttributesA;
tGetFileAttributesW ptrGetFileAttributesW;
tGetFileAttributesExA ptrGetFileAttributesExA;
tGetFileAttributesExW ptrGetFileAttributesExW;

PIMAGE_SECTION_HEADER getSection(const PIMAGE_NT_HEADERS nt_headers, unsigned section)
{
    return reinterpret_cast<PIMAGE_SECTION_HEADER>(
        (UCHAR*)nt_headers->OptionalHeader.DataDirectory +
        nt_headers->OptionalHeader.NumberOfRvaAndSizes * sizeof(IMAGE_DATA_DIRECTORY) +
        section * sizeof(IMAGE_SECTION_HEADER));
}

auto getSectionEnd(IMAGE_NT_HEADERS* ntHeader, size_t inst)
{
    auto sec = getSection(ntHeader, ntHeader->FileHeader.NumberOfSections - 1);
    // .bind section may have vanished from the executable (test case: Yakuza 4)
    // so back to the first valid section if that happened
    while (sec->Misc.VirtualSize == 0) sec--;

    auto secSize = max(sec->SizeOfRawData, sec->Misc.VirtualSize);
    auto end = inst + max(sec->PointerToRawData, sec->VirtualAddress) + secSize;
    return end;
}

bool HookKernel32IATForOverride(HMODULE mod)
{
    auto hExecutableInstance = (size_t)mod;
    IMAGE_NT_HEADERS* ntHeader = (IMAGE_NT_HEADERS*)(hExecutableInstance + ((IMAGE_DOS_HEADER*)hExecutableInstance)->e_lfanew);
    IMAGE_IMPORT_DESCRIPTOR* pImports = (IMAGE_IMPORT_DESCRIPTOR*)(hExecutableInstance + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    size_t nNumImports = ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size / sizeof(IMAGE_IMPORT_DESCRIPTOR) - 1;

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

            if (ptr == (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "CreateFileA"))
            {
                *(size_t*)i = (size_t)ptrCreateFileA;
                matchedImports++;
            }
            else if (ptr == (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "CreateFileW"))
            {
                *(size_t*)i = (size_t)ptrCreateFileW;
                matchedImports++;
            }
            else if (ptrGetFileAttributesA && ptr == (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "GetFileAttributesA"))
            {
                *(size_t*)i = (size_t)ptrGetFileAttributesA;
                matchedImports++;
            }
            else if (ptrGetFileAttributesW && ptr == (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "GetFileAttributesW"))
            {
                *(size_t*)i = (size_t)ptrGetFileAttributesW;
                matchedImports++;
            }
            else if (ptrGetFileAttributesExA && ptr == (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "GetFileAttributesExA"))
            {
                *(size_t*)i = (size_t)ptrGetFileAttributesExA;
                matchedImports++;
            }
            else if (ptrGetFileAttributesExW && ptr == (size_t)GetProcAddress(GetModuleHandle(TEXT("KERNEL32.DLL")), "GetFileAttributesExW"))
            {
                *(size_t*)i = (size_t)ptrGetFileAttributesExW;
                matchedImports++;
            }

            VirtualProtect((size_t*)i, sizeof(size_t), dwProtect[0], &dwProtect[1]);
        }
    };

    auto hExecutableInstance_end = getSectionEnd(ntHeader, hExecutableInstance);

    // Find kernel32.dll
    for (size_t i = 0; i < nNumImports; i++)
    {
        if ((size_t)(hExecutableInstance + (pImports + i)->Name) < hExecutableInstance_end)
        {
            if (!_stricmp((const char*)(hExecutableInstance + (pImports + i)->Name), "KERNEL32.DLL"))
                PatchIAT(hExecutableInstance + (pImports + i)->FirstThunk, 0, hExecutableInstance_end);
        }
    }

    return matchedImports > 0;
}

void OverrideCreateFileInDLLs(HMODULE mod)
{
    ModuleList dlls;
    dlls.Enumerate(ModuleList::SearchLocation::LocalOnly);
    for (auto& e : dlls.m_moduleList)
    {
        auto m = std::get<HMODULE>(e);
        if (m == mod && m != ual && m != hm && m != GetModuleHandle(NULL))
            HookKernel32IATForOverride(mod);
    }
}

extern "C" __declspec(dllexport) void InitializeASI()
{
    static std::once_flag flag;
    std::call_once(flag, []()
    {
        ModuleList dlls;
        dlls.Enumerate(ModuleList::SearchLocation::LocalOnly);
        for (auto& e : dlls.m_moduleList)
        {
            auto m = std::get<HMODULE>(e);
            auto pCreateFileA = (tCreateFileA)GetProcAddress(m, "CustomCreateFileA");
            auto pCreateFileW = (tCreateFileW)GetProcAddress(m, "CustomCreateFileW");
            auto pGetFileAttributesA = (tGetFileAttributesA)GetProcAddress(m, "CustomGetFileAttributesA");
            auto pGetFileAttributesW = (tGetFileAttributesW)GetProcAddress(m, "CustomGetFileAttributesW");
            auto pGetFileAttributesExA = (tGetFileAttributesExA)GetProcAddress(m, "CustomGetFileAttributesExA");
            auto pGetFileAttributesExW = (tGetFileAttributesExW)GetProcAddress(m, "CustomGetFileAttributesExW");
            if (pCreateFileA && pCreateFileW)
            {
                ual = m;
                ptrCreateFileA = pCreateFileA;
                ptrCreateFileW = pCreateFileW;
                if (pGetFileAttributesA)
                    ptrGetFileAttributesA = pGetFileAttributesA;
                if (pGetFileAttributesW)
                    ptrGetFileAttributesW = pGetFileAttributesW;
                if (pGetFileAttributesExA)
                    ptrGetFileAttributesExA = pGetFileAttributesExA;
                if (pGetFileAttributesExW)
                    ptrGetFileAttributesExW = pGetFileAttributesExW;
                break;
            }
        }

        if (ptrCreateFileA && ptrCreateFileW)
        {
            for (auto& e : dlls.m_moduleList)
            {
                auto m = std::get<HMODULE>(e);
                if (m != ual && m != hm && m != GetModuleHandle(NULL))
                    HookKernel32IATForOverride(m);
            }
            DllCallbackHandler::RegisterCallback(OverrideCreateFileInDLLs);
        }
    });
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID /*lpReserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        hm = hModule;
    }
    return TRUE;
}