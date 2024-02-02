#include <windows.h>
#include <mutex>
#include <functional>
#include <vector>
#include <map>
#include <string>
#include <future>
#include <subauth.h>
#include "../../external/ModuleList/ModuleList.hpp"

HMODULE hm = NULL;
HMODULE ual = NULL;
std::vector<std::wstring> extraDLLs = { 
    L"_nvngx.dll", L"dxilconv.dll" // for nvngx_dlss.dll loading
};

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
typedef HMODULE(WINAPI* tLoadLibraryExA)(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
typedef HMODULE(WINAPI* tLoadLibraryExW)(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags);
typedef HMODULE(WINAPI* tLoadLibraryA)(LPCSTR lpLibFileName);
typedef HMODULE(WINAPI* tLoadLibraryW)(LPCWSTR lpLibFileName);
typedef HANDLE(WINAPI* tFindFirstFileA)(LPCSTR lpFileName, LPWIN32_FIND_DATAA lpFindFileData);
typedef BOOL(WINAPI* tFindNextFileA)(HANDLE hFindFile, LPWIN32_FIND_DATAA lpFindFileData);
typedef HANDLE(WINAPI* tFindFirstFileW)(LPCWSTR lpFileName, LPWIN32_FIND_DATAW lpFindFileData);
typedef BOOL(WINAPI* tFindNextFileW)(HANDLE hFindFile, LPWIN32_FIND_DATAW lpFindFileData);
typedef HANDLE(WINAPI* tFindFirstFileExA)(LPCSTR lpFileName, FINDEX_INFO_LEVELS fInfoLevelId, WIN32_FIND_DATAA* lpFindFileData, FINDEX_SEARCH_OPS fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags);
typedef HANDLE(WINAPI* tFindFirstFileExW)(LPCWSTR lpFileName, FINDEX_INFO_LEVELS fInfoLevelId, WIN32_FIND_DATAW* lpFindFileData, FINDEX_SEARCH_OPS fSearchOp, LPVOID lpSearchFilter, DWORD dwAdditionalFlags);
tCreateFileA pCreateFileA;
tCreateFileW pCreateFileW;
tGetFileAttributesA pGetFileAttributesA;
tGetFileAttributesW pGetFileAttributesW;
tGetFileAttributesExA pGetFileAttributesExA;
tGetFileAttributesExW pGetFileAttributesExW;
tLoadLibraryExA pLoadLibraryExA;
tLoadLibraryExW pLoadLibraryExW;
tLoadLibraryA pLoadLibraryA;
tLoadLibraryW pLoadLibraryW;
tFindFirstFileA pFindFirstFileA;
tFindNextFileA pFindNextFileA;
tFindFirstFileW pFindFirstFileW;
tFindNextFileW pFindNextFileW;
tFindFirstFileExA pFindFirstFileExA;
tFindFirstFileExW pFindFirstFileExW;

class IATHook
{
public:
    template <class... Ts>
    static auto Replace(HMODULE target_module, std::string_view dll_name, Ts&& ... inputs)
    {
        std::map<std::string, std::future<void*>> originalPtrs;

        const DWORD_PTR instance = reinterpret_cast<DWORD_PTR>(target_module);
        const PIMAGE_NT_HEADERS ntHeader = reinterpret_cast<PIMAGE_NT_HEADERS>(instance + reinterpret_cast<PIMAGE_DOS_HEADER>(instance)->e_lfanew);
        PIMAGE_IMPORT_DESCRIPTOR pImports = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(instance + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
        DWORD dwProtect[2];

        for (; pImports->Name != 0; pImports++)
        {
            if (_stricmp(reinterpret_cast<const char*>(instance + pImports->Name), dll_name.data()) == 0)
            {
                if (pImports->OriginalFirstThunk != 0)
                {
                    const PIMAGE_THUNK_DATA pThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(instance + pImports->OriginalFirstThunk);

                    for (ptrdiff_t j = 0; pThunk[j].u1.AddressOfData != 0; j++)
                    {
                        auto pAddress = reinterpret_cast<void**>(instance + pImports->FirstThunk) + j;
                        if (!pAddress) continue;
                        VirtualProtect(pAddress, sizeof(pAddress), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
                        ([&]
                        {
                            auto name = std::string_view(std::get<0>(inputs));
                            auto newAddr = std::get<1>(inputs);
                            auto num = std::string("-1");
                            if (name.contains("@")) {
                                num = name.substr(name.find_last_of("@") + 1);
                                name = name.substr(0, name.find_last_of("@"));
                            }

                            if (pThunk[j].u1.Ordinal & IMAGE_ORDINAL_FLAG)
                            {
                                try
                                {
                                    if (newAddr && IMAGE_ORDINAL(pThunk[j].u1.Ordinal) == std::stoi(num.data()))
                                    {
                                        originalPtrs[std::get<0>(inputs)] = std::async(std::launch::deferred, [&]() -> void* { return *pAddress; });
                                        originalPtrs[std::get<0>(inputs)].wait();
                                        *pAddress = newAddr;
                                    }
                                }
                                catch (...) {}
                            }
                            else if ((newAddr && *pAddress && *pAddress == (void*)GetProcAddress(GetModuleHandleA(dll_name.data()), name.data())) ||
                            (strcmp(reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(instance + pThunk[j].u1.AddressOfData)->Name, name.data()) == 0))
                            {
                                originalPtrs[std::get<0>(inputs)] = std::async(std::launch::deferred, [&]() -> void* { return *pAddress; });
                                originalPtrs[std::get<0>(inputs)].wait();
                                *pAddress = newAddr;
                            }
                        } (), ...);
                        VirtualProtect(pAddress, sizeof(pAddress), dwProtect[0], &dwProtect[1]);
                    }
                }
                else
                {
                    auto pFunctions = reinterpret_cast<void**>(instance + pImports->FirstThunk);

                    for (ptrdiff_t j = 0; pFunctions[j] != nullptr; j++)
                    {
                        auto pAddress = reinterpret_cast<void**>(pFunctions[j]);
                        VirtualProtect(pAddress, sizeof(pAddress), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
                        ([&]
                        {
                            auto newAddr = std::get<1>(inputs);
                            if (newAddr && *pAddress && *pAddress == (void*)GetProcAddress(GetModuleHandleA(dll_name.data()), std::get<0>(inputs)))
                            {
                                originalPtrs[std::get<0>(inputs)] = std::async(std::launch::deferred, [&]() -> void* { return *pAddress; });
                                originalPtrs[std::get<0>(inputs)].wait();
                                *pAddress = newAddr;
                            }
                        } (), ...);
                        VirtualProtect(pAddress, sizeof(pAddress), dwProtect[0], &dwProtect[1]);
                    }
                }
            }
        }

        if (originalPtrs.empty())
        {
            PIMAGE_DELAYLOAD_DESCRIPTOR pDelayed = reinterpret_cast<PIMAGE_DELAYLOAD_DESCRIPTOR>(instance + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DELAY_IMPORT].VirtualAddress);
            if (pDelayed)
            {
                for (; pDelayed->DllNameRVA != 0; pDelayed++)
                {
                    if (_stricmp(reinterpret_cast<const char*>(instance + pDelayed->DllNameRVA), dll_name.data()) == 0)
                    {
                        if (pDelayed->ImportAddressTableRVA != 0)
                        {
                            const PIMAGE_THUNK_DATA pThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(instance + pDelayed->ImportNameTableRVA);
                            const PIMAGE_THUNK_DATA pFThunk = reinterpret_cast<PIMAGE_THUNK_DATA>(instance + pDelayed->ImportAddressTableRVA);

                            for (ptrdiff_t j = 0; pThunk[j].u1.AddressOfData != 0; j++)
                            {
                                auto pAddress = reinterpret_cast<void**>(pFThunk[j].u1.Function);
                                if (!pAddress) continue;
                                if (pThunk[j].u1.Ordinal & IMAGE_ORDINAL_FLAG)
                                    pAddress = *reinterpret_cast<void***>(pFThunk[j].u1.Function + 1); // mov     eax, offset *

                                VirtualProtect(pAddress, sizeof(pAddress), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
                                ([&]
                                {
                                    auto name = std::string_view(std::get<0>(inputs));
                                    auto newAddr = std::get<1>(inputs);
                                    auto num = std::string("-1");
                                    if (name.contains("@")) {
                                        num = name.substr(name.find_last_of("@") + 1);
                                        name = name.substr(0, name.find_last_of("@"));
                                    }

                                    if (pThunk[j].u1.Ordinal & IMAGE_ORDINAL_FLAG)
                                    {
                                        try
                                        {
                                            if (newAddr && IMAGE_ORDINAL(pThunk[j].u1.Ordinal) == std::stoi(num.data()))
                                            {
                                                originalPtrs[std::get<0>(inputs)] = std::async(std::launch::async,
                                                [](void** pAddress, void* value, PVOID instance) -> void*
                                                {
                                                    DWORD dwProtect[2];
                                                    VirtualProtect(pAddress, sizeof(pAddress), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
                                                    MEMORY_BASIC_INFORMATION mbi;
                                                    mbi.AllocationBase = instance;
                                                    do
                                                    {
                                                        VirtualQuery(*pAddress, &mbi, sizeof(MEMORY_BASIC_INFORMATION));
                                                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                                    } while (mbi.AllocationBase == instance);
                                                    auto r = *pAddress;
                                                    *pAddress = value;
                                                    VirtualProtect(pAddress, sizeof(pAddress), dwProtect[0], &dwProtect[1]);
                                                    return r;
                                                }, pAddress, newAddr, (PVOID)instance);
                                            }
                                        }
                                        catch (...) {}
                                    }
                                    else if ((newAddr && *pAddress && *pAddress == (void*)GetProcAddress(GetModuleHandleA(dll_name.data()), name.data())) ||
                                    (strcmp(reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(instance + pThunk[j].u1.AddressOfData)->Name, name.data()) == 0))
                                    {
                                        originalPtrs[std::get<0>(inputs)] = std::async(std::launch::async,
                                        [](void** pAddress, void* value, PVOID instance) -> void*
                                        {
                                            DWORD dwProtect[2];
                                            VirtualProtect(pAddress, sizeof(pAddress), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
                                            MEMORY_BASIC_INFORMATION mbi;
                                            mbi.AllocationBase = instance;
                                            do
                                            {
                                                VirtualQuery(*pAddress, &mbi, sizeof(MEMORY_BASIC_INFORMATION));
                                                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                                            } while (mbi.AllocationBase == instance);
                                            auto r = *pAddress;
                                            *pAddress = value;
                                            VirtualProtect(pAddress, sizeof(pAddress), dwProtect[0], &dwProtect[1]);
                                            return r;
                                        }, pAddress, newAddr, (PVOID)instance);
                                    }
                                } (), ...);
                                VirtualProtect(pAddress, sizeof(pAddress), dwProtect[0], &dwProtect[1]);
                            }
                        }
                    }
                }
            }
        }

        if (originalPtrs.empty()) // e.g. re5dx9.exe steam
        {
            static auto getSection = [](const PIMAGE_NT_HEADERS nt_headers, unsigned section) -> PIMAGE_SECTION_HEADER
            {
                return reinterpret_cast<PIMAGE_SECTION_HEADER>(
                    (UCHAR*)nt_headers->OptionalHeader.DataDirectory +
                    nt_headers->OptionalHeader.NumberOfRvaAndSizes * sizeof(IMAGE_DATA_DIRECTORY) +
                    section * sizeof(IMAGE_SECTION_HEADER));
            };

            for (auto i = 0; i < ntHeader->FileHeader.NumberOfSections; i++)
            {
                auto sec = getSection(ntHeader, i);
                auto pFunctions = reinterpret_cast<void**>(instance + max(sec->PointerToRawData, sec->VirtualAddress));

                for (ptrdiff_t j = 0; j < 300; j++)
                {
                    auto pAddress = reinterpret_cast<void**>(&pFunctions[j]);
                    VirtualProtect(pAddress, sizeof(pAddress), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
                    ([&]
                    {
                        auto name = std::string_view(std::get<0>(inputs));
                        auto newAddr = std::get<1>(inputs);
                        auto num = std::string("-1");
                        if (name.contains("@")) {
                            num = name.substr(name.find_last_of("@") + 1);
                            name = name.substr(0, name.find_last_of("@"));
                        }

                        if (newAddr && *pAddress && *pAddress == (void*)GetProcAddress(GetModuleHandleA(dll_name.data()), name.data()))
                        {
                            originalPtrs[std::get<0>(inputs)] = std::async(std::launch::deferred, [&]() -> void* { return *pAddress; });
                            originalPtrs[std::get<0>(inputs)].wait();
                            *pAddress = newAddr;
                        }
                    } (), ...);
                    VirtualProtect(pAddress, sizeof(pAddress), dwProtect[0], &dwProtect[1]);
                }

                if (!originalPtrs.empty())
                    return originalPtrs;
            }
        }

        return originalPtrs;
    }
};

void HookKernel32IATForOverride(HMODULE mod)
{
    IATHook::Replace(mod, "KERNEL32.DLL",
        std::forward_as_tuple("CreateFileA", pCreateFileA),
        std::forward_as_tuple("CreateFileW", pCreateFileW),
        std::forward_as_tuple("GetFileAttributesA", pGetFileAttributesA),
        std::forward_as_tuple("GetFileAttributesW", pGetFileAttributesW),
        std::forward_as_tuple("GetFileAttributesExA", pGetFileAttributesExA),
        std::forward_as_tuple("GetFileAttributesExW", pGetFileAttributesExW),
        std::forward_as_tuple("LoadLibraryExA", pLoadLibraryExA),
        std::forward_as_tuple("LoadLibraryExW", pLoadLibraryExW),
        std::forward_as_tuple("LoadLibraryA", pLoadLibraryA),
        std::forward_as_tuple("LoadLibraryW", pLoadLibraryW),
        std::forward_as_tuple("FindFirstFileA", pFindFirstFileA),
        std::forward_as_tuple("FindNextFileA", pFindNextFileA),
        std::forward_as_tuple("FindFirstFileW", pFindFirstFileW),
        std::forward_as_tuple("FindNextFileW", pFindNextFileW),
        std::forward_as_tuple("FindFirstFileExA", pFindFirstFileExA),
        std::forward_as_tuple("FindFirstFileExW", pFindFirstFileExW)
    );
}

void OverrideInDLLs(HMODULE mod)
{
    ModuleList dlls;
    dlls.Enumerate(ModuleList::SearchLocation::LocalOnly);
    for (auto& e : dlls.m_moduleList)
    {
        auto m = std::get<HMODULE>(e);
        if (m == mod && m != ual && m != hm && m != GetModuleHandle(NULL))
            HookKernel32IATForOverride(mod);
    }

    for (auto& e : extraDLLs)
    {
        if (mod == GetModuleHandle(e.c_str()))
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
            pCreateFileA = (tCreateFileA)GetProcAddress(m, "CustomCreateFileA");
            pCreateFileW = (tCreateFileW)GetProcAddress(m, "CustomCreateFileW");
            pGetFileAttributesA = (tGetFileAttributesA)GetProcAddress(m, "CustomGetFileAttributesA");
            pGetFileAttributesW = (tGetFileAttributesW)GetProcAddress(m, "CustomGetFileAttributesW");
            pGetFileAttributesExA = (tGetFileAttributesExA)GetProcAddress(m, "CustomGetFileAttributesExA");
            pGetFileAttributesExW = (tGetFileAttributesExW)GetProcAddress(m, "CustomGetFileAttributesExW");
            pLoadLibraryExA = (tLoadLibraryExA)GetProcAddress(m, "CustomLoadLibraryExA");
            pLoadLibraryExW = (tLoadLibraryExW)GetProcAddress(m, "CustomLoadLibraryExW");
            pLoadLibraryA = (tLoadLibraryA)GetProcAddress(m, "CustomLoadLibraryA");
            pLoadLibraryW = (tLoadLibraryW)GetProcAddress(m, "CustomLoadLibraryW");
            pFindFirstFileA = (tFindFirstFileA)GetProcAddress(m, "CustomFindFirstFileA");
            pFindNextFileA = (tFindNextFileA)GetProcAddress(m, "CustomFindNextFileA");
            pFindFirstFileW = (tFindFirstFileW)GetProcAddress(m, "CustomFindFirstFileW");
            pFindNextFileW = (tFindNextFileW)GetProcAddress(m, "CustomFindNextFileW");
            pFindFirstFileExA = (tFindFirstFileExA)GetProcAddress(m, "CustomFindFirstFileExA");
            pFindFirstFileExW = (tFindFirstFileExW)GetProcAddress(m, "CustomFindFirstFileExW");
            auto GetMemoryModule = (HMODULE(WINAPI*)())GetProcAddress(m, "GetMemoryModule");

            if (pCreateFileA && pCreateFileW)
            {
                ual = m;
                for (auto& e : dlls.m_moduleList)
                {
                    auto m = std::get<HMODULE>(e);
                    if (m != ual && m != hm && m != GetModuleHandle(NULL))
                        HookKernel32IATForOverride(m);
                    else if (m == ual && GetMemoryModule)
                    {
                        if (GetMemoryModule())
                        {
                            auto ptr = *reinterpret_cast<uintptr_t*>(GetMemoryModule());
                            if (*reinterpret_cast<uint16_t*>(ptr) == 0x4550) //IMAGE_NT_SIGNATURE
                            {
                                for (size_t i = 0; i <= 0xF0; i++)
                                {
                                    auto hModule = reinterpret_cast<PIMAGE_DOS_HEADER>(ptr - i);
                                    if (hModule->e_magic == IMAGE_DOS_SIGNATURE) {
                                        HookKernel32IATForOverride((HMODULE)hModule);
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }

                for (auto& e : extraDLLs)
                {
                    auto m = GetModuleHandle(e.c_str());
                    if (m)
                        HookKernel32IATForOverride(m);
                }

                DllCallbackHandler::RegisterCallback(OverrideInDLLs);

                break;
            }
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