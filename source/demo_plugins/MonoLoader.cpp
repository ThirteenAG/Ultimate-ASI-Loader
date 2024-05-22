#include <windows.h>
#include <mutex>
#include <map>
#include <subauth.h>
#include "../../external/ModuleList/ModuleList.hpp"
#include "safetyhook.hpp"
#include <filesystem>
#include <functional>
#include <vector>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <ranges>
#include <string_view>
#include <set>

HMODULE hm = NULL;
HMODULE ual = NULL;

template <typename T, typename V>
bool iequals(const T& s1, const V& s2)
{
    T str1(s1); T str2(s2);
    std::transform(str1.begin(), str1.end(), str1.begin(), ::tolower);
    std::transform(str2.begin(), str2.end(), str2.begin(), ::tolower);
    return (str1 == str2);
}

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

using MonoObject = void;
using MonoMethod = void;
using MonoMethodDesc = void;
using MonoDomain = void;
using MonoAssembly = void;
using MonoImage = void;
using MonoImageOpenStatus = void;
using mono_bool = bool;

static std::map<std::string, FARPROC> monoExports = {
    { "mono_assembly_load_from_full", nullptr },
    { "mono_get_root_domain", nullptr },
    { "mono_domain_assembly_open", nullptr },
    { "mono_assembly_get_image", nullptr },
    { "mono_runtime_invoke", nullptr },
    { "mono_method_desc_new", nullptr },
    { "mono_method_desc_free", nullptr },
    { "mono_method_desc_search_in_image", nullptr },
};

SafetyHookInline sh_mono_assembly_load_from_full_hook{};
std::map<std::filesystem::path, std::set<std::filesystem::path>> pluginsToLoad;
MonoAssembly* mono_assembly_load_from_full_hook(MonoImage* image, const char* fname, MonoImageOpenStatus* status, mono_bool refonly)
{
    auto ret = sh_mono_assembly_load_from_full_hook.unsafe_call<MonoAssembly*>(image, fname, status, refonly);

    static std::once_flag flag;
    std::call_once(flag, []()
    {
        auto mono_get_root_domain = (MonoDomain*(*)())monoExports["mono_get_root_domain"];
        auto mono_domain_assembly_open = (MonoAssembly*(*)(MonoDomain* domain, const char* name))monoExports["mono_domain_assembly_open"];
        auto mono_assembly_get_image = (MonoImage*(*)(MonoAssembly* assembly))monoExports["mono_assembly_get_image"];
        auto mono_runtime_invoke = (MonoObject*(*)(MonoMethod* method, void* obj, void** params, MonoObject** exc))monoExports["mono_runtime_invoke"];
        auto mono_method_desc_new = (MonoMethodDesc*(*)(const char* name, mono_bool include_namespace))monoExports["mono_method_desc_new"];
        auto mono_method_desc_search_in_image = (MonoMethod*(*)(MonoMethodDesc* desc, MonoImage* image))monoExports["mono_method_desc_search_in_image"];
        auto mono_method_desc_free = (void(*)(MonoMethodDesc* desc))monoExports["mono_method_desc_free"];

        std::set<MonoMethod*> invokedMethods;

        auto insertDefaults = [&](auto dll)
        {
            pluginsToLoad[dll].emplace(L"UltimateASILoader:InitializeASI");
            pluginsToLoad[dll].emplace(L"UltimateASILoader.UltimateASILoader:InitializeASI");
        };

        auto split = [](std::wstring_view string, std::wstring_view delimiter) -> std::vector<std::wstring_view>
        {
            auto t = [](auto&& rng) { return std::wstring_view(&*rng.begin(), std::ranges::distance(rng)); };
            auto s = string | std::ranges::views::split(delimiter) | std::ranges::views::transform(t);
            return { s.begin(), s.end() };
        };

        auto parseFile = [&](std::filesystem::path file, std::filesystem::path dll = std::filesystem::path())
        {
            std::wifstream infile(file);
            std::wstring line;
            while (std::getline(infile, line)) {
                if (line.empty() || line.starts_with(L"#") || line.starts_with(L";"))
                    continue;

                auto subs = split(line, L"=");
                if (subs.size() == 2)
                    pluginsToLoad[subs.front()].emplace(subs.back());
                else if (line.contains(L":"))
                    pluginsToLoad[dll].emplace(line);
                else
                    insertDefaults(line);
            }
        };

        HMODULE h;
        WCHAR buffer[MAX_PATH];
        GetModuleFileNameW(GetModuleHandleW(NULL), buffer, ARRAYSIZE(buffer));
        std::filesystem::path exePath(buffer);
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&monoExports, &h);
        GetModuleFileNameW(h, buffer, ARRAYSIZE(buffer));
        std::filesystem::path modulePath(buffer);

        parseFile(exePath.parent_path() / L"assemblies.txt");
        parseFile(std::filesystem::path(modulePath).replace_extension(".ini"));
        parseFile(std::filesystem::path(modulePath).replace_extension(".txt"));

        std::error_code ec;
        for (auto& folder : { L"scripts", L"plugins" })
        {
            for (const auto& file : std::filesystem::recursive_directory_iterator(folder, std::filesystem::directory_options::skip_permission_denied, ec))
            {
                if (!std::filesystem::is_directory(file, ec) && file.is_regular_file(ec) && iequals(file.path().extension().wstring(), L".dll"))
                {
                    auto plugin_path = std::filesystem::absolute(file, ec);
                    insertDefaults(plugin_path);
                    parseFile(std::filesystem::path(plugin_path).replace_extension(".ini"), plugin_path);
                    parseFile(std::filesystem::path(plugin_path).replace_extension(".txt"), plugin_path);
                }
            }
        }

        for (auto& plugin : pluginsToLoad)
        {
            auto rootDomain = mono_get_root_domain();
            if (rootDomain)
            {
                auto monoAssembly = mono_domain_assembly_open(rootDomain, plugin.first.string().c_str());
                if (monoAssembly)
                {
                    auto image = mono_assembly_get_image(monoAssembly);
                    for (auto& method : plugin.second)
                    {
                        if (!method.empty())
                        {
                            for (auto inc_ns : { true, false })
                            {
                                auto description = mono_method_desc_new(method.string().c_str(), inc_ns);
                                if (description)
                                {
                                    auto method = mono_method_desc_search_in_image(description, image);
                                    mono_method_desc_free(description);
                                    if (method && !invokedMethods.contains(method))
                                    {
                                        void* exc = nullptr;
                                        mono_runtime_invoke(method, nullptr, nullptr, &exc);
                                        if (!exc)
                                        {
                                            invokedMethods.emplace(method);
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    });

    return ret;
}

void GetMonoDllCB(HMODULE mod)
{
    ModuleList dlls;
    dlls.Enumerate(ModuleList::SearchLocation::LocalOnly);
    for (auto& e : dlls.m_moduleList)
    {
        auto m = std::get<HMODULE>(e);
        if (m == mod && m != ual && m != hm && m != GetModuleHandle(NULL))
        {
            for (auto& [key, value] : monoExports)
            {
                auto v = GetProcAddress(m, key.c_str());
                if (v)
                    value = v;
                else
                    break;
            }
        }
    }

    if (std::all_of(monoExports.begin(), monoExports.end(), [](const auto& it) { return it.second != nullptr; }))
    {
        sh_mono_assembly_load_from_full_hook = safetyhook::create_inline(monoExports["mono_assembly_load_from_full"], mono_assembly_load_from_full_hook);
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
            GetMonoDllCB(std::get<HMODULE>(e));
        DllCallbackHandler::RegisterCallback(GetMonoDllCB);
    });
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID /*lpReserved*/)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        hm = hModule;
    }
    else if (reason == DLL_PROCESS_DETACH)
    {
        sh_mono_assembly_load_from_full_hook.reset();
    }
    return TRUE;
}