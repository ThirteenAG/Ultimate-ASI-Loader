#include "startup.hpp"
#include "imports.hpp"
#include "../core/hooks.hpp"
#include "../core/loader.hpp"
#include "../core/log.hpp"
#include "../core/paths.hpp"
#include "../core/pe.hpp"
#include "../core/strings.hpp"
#include "../plugins/plugins.hpp"
#include "../proxy/proxy.hpp"
#include "../vfs/vfs.hpp"
#ifndef _WIN64
#include "../../compat/d3d8/maximized_shim.hpp"
#endif
#include <initguid.h>
#include <algorithm>
#include <intrin.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace ual::startup
{
    namespace
    {
        std::vector<ModuleImports> g_modules;
        HANDLE g_hookMutex = nullptr;
        bool g_exePatched = false;          // if set, only calls from the exe count
        volatile LONG g_started = 0;
        volatile LONG g_everythingLoaded = 0;

        HMODULE ModuleOf(const void* address)
        {
            HMODULE m = nullptr;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)address, &m);
            return m;
        }

        // Computed before any hook is installed, because the checks run inside hooked functions.
        struct Range
        {
            uintptr_t begin = 0, end = 0;
            bool Contains(const void* p) const { return (uintptr_t)p >= begin && (uintptr_t)p < end; }
        };
        Range g_exe;
        Range g_entry; // entry point section, if it isn't the first code section
        Range g_bind;  // Steam's .bind section

        // First executable section. Old exes running with DEP off may mark none executable (Max Payne has only
        // read/write data sections), so fall back to the first section with code, then the first section.
        const IMAGE_SECTION_HEADER* FirstCodeSection(const pe::Image& exe)
        {
            auto sec = IMAGE_FIRST_SECTION(exe.nt);
            WORD n = exe.nt->FileHeader.NumberOfSections;
            const IMAGE_SECTION_HEADER* found = nullptr;
            // Sections with no data on disk hold no code: .textbss of incrementally linked exes and UPX0 of
            // packed ones are executable but empty, and come before the real code section.
            auto empty = [](const IMAGE_SECTION_HEADER& s) { return s.SizeOfRawData == 0 || (s.Characteristics & IMAGE_SCN_CNT_UNINITIALIZED_DATA); };
            for (DWORD flag : { (DWORD)IMAGE_SCN_MEM_EXECUTE, (DWORD)IMAGE_SCN_CNT_CODE })
            {
                for (WORD i = 0; i < n; ++i)
                    if ((sec[i].Characteristics & flag) && !empty(sec[i]) && (!found || sec[i].VirtualAddress < found->VirtualAddress)) found = &sec[i];
                if (found) return found;
            }
            for (WORD i = 0; i < n; ++i)
                if (!found || sec[i].VirtualAddress < found->VirtualAddress) found = &sec[i];
            return found;
        }

        // Code that runs before the game's own (Steam DRM, Origin / Denuvo / SecuROM entry code, packers) lives in
        // its own section, and its calls never start loading. The game's code is in the first code section.
        void InitRanges()
        {
            pe::Image exe(GetModuleHandleW(nullptr));
            if (!exe.Valid()) return;
            g_exe = { exe.base, exe.base + exe.nt->OptionalHeader.SizeOfImage };
            pe::ForEachSection(GetModuleHandleW(nullptr), { ".bind" }, [](IMAGE_SECTION_HEADER*, uintptr_t start, size_t size) { g_bind = { start, start + size }; });
            DWORD entry = exe.nt->OptionalHeader.AddressOfEntryPoint;
            auto sec = IMAGE_FIRST_SECTION(exe.nt);
            const IMAGE_SECTION_HEADER* firstCode = FirstCodeSection(exe);
            const IMAGE_SECTION_HEADER* entrySection = nullptr;
            for (WORD i = 0; i < exe.nt->FileHeader.NumberOfSections; ++i)
            {
                DWORD size = (std::max)(sec[i].Misc.VirtualSize, sec[i].SizeOfRawData);
                if (entry >= sec[i].VirtualAddress && entry < sec[i].VirtualAddress + size) entrySection = &sec[i];
            }
            // A protection stub keeps its own import directory in its section (it resolves the game's imports
            // itself). An exe whose entry point merely sits in a later code section is not a stub, and its calls count.
            const auto& imports = exe.nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            auto inSection = [&](const IMAGE_SECTION_HEADER& s, DWORD rva) {
                return rva >= s.VirtualAddress && rva < s.VirtualAddress + (std::max)(s.Misc.VirtualSize, s.SizeOfRawData);
            };
            if (entrySection && entrySection != firstCode && (!imports.VirtualAddress || inSection(*entrySection, imports.VirtualAddress)))
            {
                g_entry = { exe.base + entrySection->VirtualAddress,
                            exe.base + entrySection->VirtualAddress + (std::max)(entrySection->Misc.VirtualSize, entrySection->SizeOfRawData) };
                UAL_LOG("entry point in section %.8s, the first code section is %.8s: calls from %.8s are ignored", (const char*)entrySection->Name,
                        firstCode ? (const char*)firstCode->Name : "-", (const char*)entrySection->Name);
            }
            if (g_bind.end) UAL_LOG("Steam DRM section .bind: calls from it are ignored");
        }

        // The exe, DLLs in its folder, and engine hosts of managed / Unity games.
        bool IsGameModule(const void* address)
        {
            if (g_exe.Contains(address)) return true;
            HMODULE m = ModuleOf(address);
            if (!m) return false;
            wchar_t path[MAX_PATH * 2];
            DWORD n = GetModuleFileNameW(m, path, (DWORD)std::size(path));
            if (!n || n >= std::size(path)) return false;
            std::wstring_view p(path, n);
            if (IStartsWith(p, Self().exeDir)) return true;
            auto name = p.substr(p.find_last_of(L'\\') + 1);
            return IEquals(name, L"UnityPlayer.dll") || IEquals(name, L"clr.dll") || IEquals(name, L"coreclr.dll");
        }

        // Reason this call doesn't start loading, or null if it does.
        const char* Rejection(const void* returnAddress, const char* api)
        {
            if (LoaderLockHeldByThisThread()) return "loader lock held (DllMain / TLS callback)";
            if (g_entry.Contains(returnAddress)) return "from the entry point section";
            if (g_bind.Contains(returnAddress)) return "from .bind";

            // LoadFromAPI=[module.]function restricts loading to that call, from that module
            const auto& fromApi = GetSettings().loadFromAPI;
            if (!fromApi.empty())
            {
                std::wstring module, function = fromApi;
                if (auto dot = fromApi.find(L'.'); dot != std::wstring::npos)
                {
                    module = fromApi.substr(0, dot);
                    function = fromApi.substr(dot + 1);
                }
                if (function != AnsiToWide(api)) return "not the LoadFromAPI function";
                if (ModuleOf(returnAddress) != GetModuleHandleW(module.empty() ? nullptr : module.c_str())) return "not from the LoadFromAPI module";
            }
            return nullptr;
        }

        // rejected calls are logged only up to 200
        bool Decide(const void* returnAddress, const char* api, const char* rejection)
        {
            static volatile LONG logged = 0;
            if (log::Enabled() && (!rejection || InterlockedIncrement(&logged) <= 200))
                UAL_LOG("%s from %s: %s", api, log::Describe(returnAddress).c_str(), rejection ? rejection : "starts loading the plugins");
            return !rejection;
        }

        void DisableInlineHooks();

        void Start()
        {
            for (auto& m : g_modules)
            {
                RestoreSlots(m.kernel32);
                RestoreSlots(m.vccorlib);
            }
            DisableInlineHooks();
            LoadEverything();
        }

        void OnEntryCall(const void* returnAddress, const char* api)
        {
            proxy::LoadOriginalLibrary(); // as early as possible, regardless of caller
            if (g_started) return;
            if (!Decide(returnAddress, api, g_exePatched && !g_exe.Contains(returnAddress) ? "not from the executable" : Rejection(returnAddress, api))) return;
            if (InterlockedCompareExchange(&g_started, 1, 0) != 0) return;
            Start();
        }

        // inline hooks
        // Used when imports can't be patched (a protection stub resolved them, or the exe imports nothing usable,
        // like .NET and UWP launchers). File functions are left out because file overloading hooks them later.

        bool g_inlineExecutableOnly = false; // protection stub case, only the exe's calls count

        void OnInlineCall(const void* returnAddress, const char* api)
        {
            // what we call here may be hooked too, so those calls pass straight through
            static thread_local bool inside = false;
            if (inside) return;
            inside = true;
            proxy::LoadOriginalLibrary();
            if (!g_started)
            {
                const char* rejection = (g_inlineExecutableOnly ? g_exe.Contains(returnAddress) : IsGameModule(returnAddress))
                                            ? Rejection(returnAddress, api)
                                            : (g_inlineExecutableOnly ? "not from the executable" : "not from a module of the game");
                if (Decide(returnAddress, api, rejection) && InterlockedCompareExchange(&g_started, 1, 0) == 0) Start();
            }
            inside = false;
        }

#define UAL_INLINE_HOOKS(X)                                                                        \
    X(void, GetStartupInfoA, (LPSTARTUPINFOA a), (a))                                              \
    X(void, GetStartupInfoW, (LPSTARTUPINFOW a), (a))                                              \
    X(HMODULE, GetModuleHandleA, (LPCSTR a), (a))                                                  \
    X(HMODULE, GetModuleHandleW, (LPCWSTR a), (a))                                                 \
    X(FARPROC, GetProcAddress, (HMODULE a, LPCSTR b), (a, b))                                      \
    X(DWORD, GetShortPathNameA, (LPCSTR a, LPSTR b, DWORD c), (a, b, c))                           \
    X(HANDLE, CreateEventA, (LPSECURITY_ATTRIBUTES a, BOOL b, BOOL c, LPCSTR d), (a, b, c, d))    \
    X(HANDLE, CreateEventW, (LPSECURITY_ATTRIBUTES a, BOOL b, BOOL c, LPCWSTR d), (a, b, c, d))   \
    X(void, GetSystemInfo, (LPSYSTEM_INFO a), (a))                                                 \
    X(void, Sleep, (DWORD a), (a))                                                                 \
    X(void, GetSystemTimeAsFileTime, (LPFILETIME a), (a))                                          \
    X(DWORD, GetCurrentProcessId, (), ())                                                          \
    X(LPSTR, GetCommandLineA, (), ())                                                              \
    X(LPWSTR, GetCommandLineW, (), ())

#define UAL_INLINE_DETOUR(ret, name, params, args)               \
    ret(WINAPI* oInline##name) params = nullptr;                 \
    ret WINAPI Inline##name params                               \
    {                                                            \
        OnInlineCall(_ReturnAddress(), #name);                   \
        return oInline##name args;                               \
    }
        UAL_INLINE_HOOKS(UAL_INLINE_DETOUR)
#undef UAL_INLINE_DETOUR

        const HookSpec kInlineHooks[] = {
#define UAL_INLINE_SPEC(ret, name, params, args) { L"kernel32.dll", #name, (void*)Inline##name, (void**)&oInline##name },
            UAL_INLINE_HOOKS(UAL_INLINE_SPEC)
#undef UAL_INLINE_SPEC
        };

        bool g_inlineInstalled = false;

        void DisableInlineHooks()
        {
            if (g_inlineInstalled) DisableHooks(kInlineHooks);
        }

        bool InstallInlineHooks(bool executableOnly)
        {
            g_inlineExecutableOnly = executableOnly;
            int hooked = InstallHooks(kInlineHooks);
            g_inlineInstalled = hooked > 0;
            UAL_LOG("kernel32 hooked inline: %d of %d functions; calls from %s count", hooked, (int)std::size(kInlineHooks),
                    executableOnly ? "the executable" : "the executable, DLLs in its folder and engine hosts");
            return g_inlineInstalled;
        }

#define UAL_ENTRY(api) OnEntryCall(_ReturnAddress(), #api)

        // kernel32 wrappers

        void WINAPI EntryGetStartupInfoA(LPSTARTUPINFOA si) { UAL_ENTRY(GetStartupInfoA); GetStartupInfoA(si); }
        void WINAPI EntryGetStartupInfoW(LPSTARTUPINFOW si) { UAL_ENTRY(GetStartupInfoW); GetStartupInfoW(si); }
        HMODULE WINAPI EntryGetModuleHandleA(LPCSTR n) { UAL_ENTRY(GetModuleHandleA); return GetModuleHandleA(n); }
        HMODULE WINAPI EntryGetModuleHandleW(LPCWSTR n) { UAL_ENTRY(GetModuleHandleW); return GetModuleHandleW(n); }
        FARPROC WINAPI EntryGetProcAddress(HMODULE m, LPCSTR n) { UAL_ENTRY(GetProcAddress); return GetProcAddress(m, n); }
        DWORD WINAPI EntryGetShortPathNameA(LPCSTR l, LPSTR s, DWORD c) { UAL_ENTRY(GetShortPathNameA); return GetShortPathNameA(l, s, c); }
        HANDLE WINAPI EntryCreateEventA(LPSECURITY_ATTRIBUTES a, BOOL m, BOOL i, LPCSTR n) { UAL_ENTRY(CreateEventA); return CreateEventA(a, m, i, n); }
        HANDLE WINAPI EntryCreateEventW(LPSECURITY_ATTRIBUTES a, BOOL m, BOOL i, LPCWSTR n) { UAL_ENTRY(CreateEventW); return CreateEventW(a, m, i, n); }
        void WINAPI EntryGetSystemInfo(LPSYSTEM_INFO si) { UAL_ENTRY(GetSystemInfo); GetSystemInfo(si); }
        LONG WINAPI EntryInterlockedCompareExchange(LONG volatile* d, LONG e, LONG c) { UAL_ENTRY(InterlockedCompareExchange); return _InterlockedCompareExchange(d, e, c); }
        void WINAPI EntrySleep(DWORD ms) { UAL_ENTRY(Sleep); Sleep(ms); }
        void WINAPI EntryGetSystemTimeAsFileTime(LPFILETIME ft) { UAL_ENTRY(GetSystemTimeAsFileTime); GetSystemTimeAsFileTime(ft); }
        DWORD WINAPI EntryGetCurrentProcessId() { UAL_ENTRY(GetCurrentProcessId); return GetCurrentProcessId(); }
        LPSTR WINAPI EntryGetCommandLineA() { UAL_ENTRY(GetCommandLineA); return GetCommandLineA(); }
        LPWSTR WINAPI EntryGetCommandLineW() { UAL_ENTRY(GetCommandLineW); return GetCommandLineW(); }
        void WINAPI EntryAcquireSRWLockExclusive(PSRWLOCK l) { UAL_ENTRY(AcquireSRWLockExclusive); AcquireSRWLockExclusive(l); }
        HANDLE WINAPI EntryCreateFileA(LPCSTR n, DWORD a, DWORD s, LPSECURITY_ATTRIBUTES sa, DWORD c, DWORD f, HANDLE t) { UAL_ENTRY(CreateFileA); return CreateFileA(n, a, s, sa, c, f, t); }
        HANDLE WINAPI EntryCreateFileW(LPCWSTR n, DWORD a, DWORD s, LPSECURITY_ATTRIBUTES sa, DWORD c, DWORD f, HANDLE t) { UAL_ENTRY(CreateFileW); return CreateFileW(n, a, s, sa, c, f, t); }
        DWORD WINAPI EntryGetFileAttributesA(LPCSTR n) { UAL_ENTRY(GetFileAttributesA); return GetFileAttributesA(n); }
        DWORD WINAPI EntryGetFileAttributesW(LPCWSTR n) { UAL_ENTRY(GetFileAttributesW); return GetFileAttributesW(n); }
        BOOL WINAPI EntryGetFileAttributesExA(LPCSTR n, GET_FILEEX_INFO_LEVELS l, LPVOID i) { UAL_ENTRY(GetFileAttributesExA); return GetFileAttributesExA(n, l, i); }
        BOOL WINAPI EntryGetFileAttributesExW(LPCWSTR n, GET_FILEEX_INFO_LEVELS l, LPVOID i) { UAL_ENTRY(GetFileAttributesExW); return GetFileAttributesExW(n, l, i); }
        HANDLE WINAPI EntryFindFirstFileA(LPCSTR n, LPWIN32_FIND_DATAA d) { UAL_ENTRY(FindFirstFileA); return FindFirstFileA(n, d); }
        BOOL WINAPI EntryFindNextFileA(HANDLE h, LPWIN32_FIND_DATAA d) { UAL_ENTRY(FindNextFileA); return FindNextFileA(h, d); }
        HANDLE WINAPI EntryFindFirstFileW(LPCWSTR n, LPWIN32_FIND_DATAW d) { UAL_ENTRY(FindFirstFileW); return FindFirstFileW(n, d); }
        BOOL WINAPI EntryFindNextFileW(HANDLE h, LPWIN32_FIND_DATAW d) { UAL_ENTRY(FindNextFileW); return FindNextFileW(h, d); }
        HANDLE WINAPI EntryFindFirstFileExA(LPCSTR n, FINDEX_INFO_LEVELS l, LPVOID d, FINDEX_SEARCH_OPS o, LPVOID f, DWORD x)
        {
            UAL_ENTRY(FindFirstFileExA);
            return FindFirstFileExA(n, l, d, o, f, x);
        }
        HANDLE WINAPI EntryFindFirstFileExW(LPCWSTR n, FINDEX_INFO_LEVELS l, LPVOID d, FINDEX_SEARCH_OPS o, LPVOID f, DWORD x)
        {
            UAL_ENTRY(FindFirstFileExW);
            return FindFirstFileExW(n, l, d, o, f, x);
        }

        // the DLL being loaded may import the proxied original
        HMODULE WINAPI EntryLoadLibraryA(LPCSTR n) { proxy::LoadOriginalLibrary(); return LoadLibraryA(n); }
        HMODULE WINAPI EntryLoadLibraryW(LPCWSTR n) { proxy::LoadOriginalLibrary(); return LoadLibraryW(n); }
        HMODULE WINAPI EntryLoadLibraryExA(LPCSTR n, HANDLE f, DWORD x) { proxy::LoadOriginalLibrary(); return LoadLibraryExA(n, f, x); }
        HMODULE WINAPI EntryLoadLibraryExW(LPCWSTR n, HANDLE f, DWORD x) { proxy::LoadOriginalLibrary(); return LoadLibraryExW(n, f, x); }

        // the game must not unload us, our hooks and plugins stay in use
        BOOL WINAPI EntryFreeLibrary(HMODULE m) { return m == Self().module ? TRUE : FreeLibrary(m); }

#undef UAL_ENTRY

        const std::pair<const char*, void*> kKernel32Wrappers[] = {
            { "GetStartupInfoA", (void*)EntryGetStartupInfoA },
            { "GetStartupInfoW", (void*)EntryGetStartupInfoW },
            { "GetModuleHandleA", (void*)EntryGetModuleHandleA },
            { "GetModuleHandleW", (void*)EntryGetModuleHandleW },
            { "GetProcAddress", (void*)EntryGetProcAddress },
            { "GetShortPathNameA", (void*)EntryGetShortPathNameA },
            { "FindFirstFileA", (void*)EntryFindFirstFileA },
            { "FindNextFileA", (void*)EntryFindNextFileA },
            { "FindFirstFileW", (void*)EntryFindFirstFileW },
            { "FindNextFileW", (void*)EntryFindNextFileW },
            { "FindFirstFileExA", (void*)EntryFindFirstFileExA },
            { "FindFirstFileExW", (void*)EntryFindFirstFileExW },
            { "LoadLibraryExA", (void*)EntryLoadLibraryExA },
            { "LoadLibraryExW", (void*)EntryLoadLibraryExW },
            { "LoadLibraryA", (void*)EntryLoadLibraryA },
            { "LoadLibraryW", (void*)EntryLoadLibraryW },
            { "FreeLibrary", (void*)EntryFreeLibrary },
            { "CreateEventA", (void*)EntryCreateEventA },
            { "CreateEventW", (void*)EntryCreateEventW },
            { "GetSystemInfo", (void*)EntryGetSystemInfo },
            { "InterlockedCompareExchange", (void*)EntryInterlockedCompareExchange },
            { "Sleep", (void*)EntrySleep },
            { "GetSystemTimeAsFileTime", (void*)EntryGetSystemTimeAsFileTime },
            { "GetCurrentProcessId", (void*)EntryGetCurrentProcessId },
            { "GetCommandLineA", (void*)EntryGetCommandLineA },
            { "GetCommandLineW", (void*)EntryGetCommandLineW },
            { "AcquireSRWLockExclusive", (void*)EntryAcquireSRWLockExclusive },
            { "CreateFileA", (void*)EntryCreateFileA },
            { "CreateFileW", (void*)EntryCreateFileW },
            { "GetFileAttributesA", (void*)EntryGetFileAttributesA },
            { "GetFileAttributesW", (void*)EntryGetFileAttributesW },
            { "GetFileAttributesExA", (void*)EntryGetFileAttributesExA },
            { "GetFileAttributesExW", (void*)EntryGetFileAttributesExW },
        };

        // COM

        DEFINE_GUID(CLSID_UAL_DirectSound, 0x47d4d946, 0x62e8, 0x11cf, 0x93, 0xbc, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00);
        DEFINE_GUID(CLSID_UAL_DirectSound8, 0x3901cc3f, 0x84b5, 0x4fa4, 0xba, 0x35, 0xaa, 0x81, 0x72, 0xb8, 0xa0, 0x9b);
        DEFINE_GUID(CLSID_UAL_DirectInput, 0x25E609E0, 0xB259, 0x11CF, 0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00);
        DEFINE_GUID(CLSID_UAL_DirectInput8, 0x25E609E4, 0xB259, 0x11CF, 0xBF, 0xC7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00);
        DEFINE_GUID(CLSID_UAL_WinInet, 0xC39EE728, 0xD419, 0x4BD4, 0xA3, 0xEF, 0xED, 0xA0, 0x59, 0xDB, 0xD9, 0x35);

        // Create these objects from the DLL found by the normal search order (e.g. a dsound.dll wrapper in the
        // game folder), unless that DLL is a copy of the loader.
        HRESULT WINAPI EntryCoCreateInstance(REFCLSID clsid, LPUNKNOWN outer, DWORD context, REFIID iid, LPVOID* out)
        {
            const wchar_t* dll = nullptr;
            if (IsEqualCLSID(clsid, CLSID_UAL_DirectSound) || IsEqualCLSID(clsid, CLSID_UAL_DirectSound8)) dll = L"dsound.dll";
            else if (IsEqualCLSID(clsid, CLSID_UAL_DirectInput8)) dll = L"dinput8.dll";
            else if (IsEqualCLSID(clsid, CLSID_UAL_DirectInput)) dll = L"dinput.dll";
            else if (IsEqualCLSID(clsid, CLSID_UAL_WinInet)) dll = L"wininet.dll";
            HMODULE m = dll ? LoadLibraryW(dll) : nullptr;
            if (!m || GetProcAddress(m, "IsUltimateASILoader")) return CoCreateInstance(clsid, outer, context, iid, out);

            using GetClassObjectFn = HRESULT(__stdcall*)(REFCLSID, REFIID, LPVOID*);
            auto getClassObject = (GetClassObjectFn)GetProcAddress(m, "DllGetClassObject");
            if (!getClassObject)
            {
                FreeLibrary(m);
                return REGDB_E_KEYMISSING;
            }
            IClassFactory* factory = nullptr;
            HRESULT hr = getClassObject(clsid, IID_IClassFactory, (LPVOID*)&factory);
            if (FAILED(hr) || !factory)
            {
                FreeLibrary(m);
                return FAILED(hr) ? hr : E_FAIL;
            }
            hr = factory->CreateInstance(outer, iid, out);
            factory->Release();
            return hr;
        }

        // vccorlib (UWP / C++/CX games)

        constexpr char kGetCmdArguments[] = "?GetCmdArguments@Details@Platform@@YAPEAPEA_WPEAH@Z";

        LPWSTR WINAPI EntryGetCmdArguments(int* argc)
        {
            using Fn = LPWSTR(WINAPI*)(int*);
            Fn original = nullptr;
            HMODULE caller = ModuleOf(_ReturnAddress());
            for (auto& m : g_modules)
                if (auto it = m.vccorlib.find(kGetCmdArguments); it != m.vccorlib.end() && (m.module == caller || !original))
                    original = (Fn)it->second.original;
            OnEntryCall(_ReturnAddress(), "GetCmdArguments");
            return original ? original(argc) : nullptr;
        }

    }

    // import patching

    void* Kernel32Wrapper(const char* name)
    {
        for (const auto& [n, f] : kKernel32Wrappers)
            if (!strcmp(n, name)) return f;
        return nullptr;
    }

    void RestoreSlots(PatchedSlots& slots)
    {
        for (auto& [name, s] : slots)
        {
            MEMORY_BASIC_INFORMATION mbi;
            if (VirtualQuery(s.slot, &mbi, sizeof(mbi)) && mbi.State == MEM_COMMIT) pe::WritePointer(s.slot, s.original);
        }
    }

    namespace
    {
        void Patch(PatchedSlots& record, const std::string& name, void** slot, void* replacement)
        {
            void* original = pe::WritePointer(slot, replacement);
            if (*slot != replacement) return; // the page could not be made writable (ACG, a protector's guard): not patched
            record[name] = { slot, original };
        }

        bool IsKernel32Like(std::string_view dll)
        {
            return dll == "kernel32.dll" || dll.starts_with("api-ms-win-") || dll.starts_with("ext-ms-win-");
        }
    }

    void PatchKernel32(const pe::Image& img, ModuleImports& rec)
    {
        HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
        if (!k32) return;
        // kernel32 functions may be imported under any API set name, so also match by address
        std::unordered_map<void*, const char*> byAddress;
        for (const auto& [name, f] : kKernel32Wrappers)
            if (void* a = (void*)GetProcAddress(k32, name)) byAddress[a] = name;

        pe::ForEachImport(img, [&](const pe::ImportSlot& s) {
            if (s.name)
            {
                void* wrapper = Kernel32Wrapper(s.name);
                if (wrapper && (IsKernel32Like(s.dll) || byAddress.count(*s.slot))) Patch(rec.kernel32, s.name, s.slot, wrapper);
            }
            else if (!s.ordinal) // bound import without names
            {
                if (auto it = byAddress.find(*s.slot); it != byAddress.end()) Patch(rec.kernel32, it->second, s.slot, Kernel32Wrapper(it->second));
            }
            return true;
        });

        // Packed exes may list kernel32 without thunks. Find their IAT by scanning the start of executable sections.
        if (!rec.kernel32.empty()) return;
        auto& dir = img.nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        bool thunkless = false;
        if (dir.VirtualAddress)
            for (auto d = img.At<IMAGE_IMPORT_DESCRIPTOR>(dir.VirtualAddress); img.Contains(d, sizeof(*d)) && d->Name; ++d)
            {
                auto name = img.At<const char>(d->Name);
                if (!img.Contains(name) || d->OriginalFirstThunk || d->FirstThunk) continue;
                std::string lower(name);
                for (auto& c : lower) c = (char)tolower((unsigned char)c);
                thunkless |= IsKernel32Like(lower);
            }
        if (!thunkless) return;
        auto sec = IMAGE_FIRST_SECTION(img.nt);
        for (WORD i = 0; i < img.nt->FileHeader.NumberOfSections; ++i, ++sec)
        {
            if (!(sec->Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
            auto p = img.At<void*>(sec->VirtualAddress);
            size_t count = (std::min<size_t>)(300, (std::max)(sec->SizeOfRawData, sec->Misc.VirtualSize) / sizeof(void*));
            for (size_t j = 0; j < count && img.Contains(&p[j], sizeof(void*)); ++j)
                if (auto it = byAddress.find(p[j]); it != byAddress.end()) Patch(rec.kernel32, it->second, &p[j], Kernel32Wrapper(it->second));
        }
    }

    void PatchCom(const pe::Image& img, ModuleImports& rec)
    {
        // we are the DLL that creates these objects
        for (const wchar_t* n : { L"dsound.dll", L"dinput8.dll", L"dinput.dll", L"wininet.dll" })
            if (IEquals(Self().name, n)) return;
        HMODULE ole32 = GetModuleHandleW(L"ole32.dll");
        void* coCreate = ole32 ? (void*)GetProcAddress(ole32, "CoCreateInstance") : nullptr;
        pe::ForEachImport(img, [&](const pe::ImportSlot& s) {
            if (s.dll != "ole32.dll") return true;
            if ((s.name && !strcmp(s.name, "CoCreateInstance")) || (!s.name && !s.ordinal && coCreate && *s.slot == coCreate))
                Patch(rec.ole32, "CoCreateInstance", s.slot, (void*)EntryCoCreateInstance);
            return true;
        });
    }

    void PatchVccorlib(const pe::Image& img, ModuleImports& rec)
    {
        pe::ForEachImport(img, [&](const pe::ImportSlot& s) {
            if (s.dll.find("vccorlib") == std::string_view::npos) return true;
            bool match = s.name && !strcmp(s.name, kGetCmdArguments);
            if (!s.name && !s.ordinal)
            {
                std::string dll(s.dll);
                HMODULE m = GetModuleHandleA(dll.c_str());
                match = m && *s.slot == (void*)GetProcAddress(m, kGetCmdArguments);
            }
            if (match) Patch(rec.vccorlib, kGetCmdArguments, s.slot, (void*)EntryGetCmdArguments);
            return true;
        });
    }

    // Our export ordinals differ from the system DLL's, so ordinal imports of the proxied DLL are redirected.
    void PatchOrdinals(const pe::Image& img, HMODULE self, std::wstring_view selfName)
    {
        std::string selfAnsi = WideToAnsi(ToLower(std::wstring(selfName)));
        std::vector<std::pair<WORD, const void*>> entries; // built lazily
        bool built = false;
        auto entryFor = [&](WORD ordinal) -> const void* {
            for (const auto& [o, e] : entries)
                if (o == ordinal) return e;
            return nullptr;
        };
        pe::ForEachImport(img, [&](const pe::ImportSlot& s) {
            if (s.dll != selfAnsi || s.name) return true;
            if (!built)
            {
                entries = proxy::OrdinalEntries(selfName);
                built = true;
            }
            if (s.ordinal)
            {
                if (auto e = entryFor(s.ordinal)) pe::WritePointer(s.slot, (void*)e);
            }
            else // bound, find the ordinal the slot was bound to
            {
                for (const auto& [o, e] : entries)
                    if (*s.slot == (void*)GetProcAddress(self, MAKEINTRESOURCEA(o)))
                    {
                        pe::WritePointer(s.slot, (void*)e);
                        break;
                    }
            }
            return true;
        });
    }

    namespace
    {
        // Several loader copies with different proxy names can be loaded. Only one hooks.
        bool ClaimHooking()
        {
            std::wstring mutexName = L"Ultimate-ASI-Loader-HookIAT" + std::to_wstring(GetCurrentProcessId());
            if (HANDLE existing = OpenMutexW(SYNCHRONIZE, FALSE, mutexName.c_str()))
            {
                CloseHandle(existing);
                return false;
            }
            HANDLE mutex = CreateMutexW(nullptr, TRUE, mutexName.c_str());
            if (!mutex || GetLastError() == ERROR_ALREADY_EXISTS)
            {
                if (mutex) CloseHandle(mutex);
                return false;
            }
            g_hookMutex = mutex;
            return true;
        }
    }

    bool HookEntryPoints()
    {
        if (!ClaimHooking()) return false;
        InitRanges();

        HMODULE exe = GetModuleHandleW(nullptr);
        for (HMODULE mod : LoadedModules())
        {
            if (mod == Self().module) continue;
            auto path = ModulePath(mod);
            auto file = FileNameOf(path);
            auto stem = file.substr(0, file.find_last_of(L'.'));
            auto name = ToLower(stem);
            bool local = IStartsWith(path, Self().exeDir);
            if (mod != exe && !local && name != L"unityplayer" && name != L"clr" && name != L"coreclr") continue;
            pe::Image img(mod);
            if (!img.Valid()) continue;
            ModuleImports rec;
            rec.module = mod;
            PatchKernel32(img, rec);
            PatchVccorlib(img, rec);
            PatchCom(img, rec);
            PatchOrdinals(img, Self().module, Self().name);
            // only wrappers that can start loading count; LoadLibrary*/FreeLibrary wrappers never do
            auto triggers = [](const PatchedSlots& slots) {
                for (const auto& [name, s] : slots)
                    if (name.rfind("LoadLibrary", 0) != 0 && name != "FreeLibrary") return true;
                return false;
            };
            if (mod == exe) g_exePatched = triggers(rec.kernel32) || !rec.vccorlib.empty();
            if (!rec.kernel32.empty() || !rec.vccorlib.empty() || !rec.ole32.empty())
                UAL_LOG("imports patched in %ls: %d kernel32, %d vccorlib, %d ole32", stem.c_str(), (int)rec.kernel32.size(),
                        (int)rec.vccorlib.size(), (int)rec.ole32.size());
            g_modules.push_back(std::move(rec));
        }
        if (g_exePatched)
        {
            // The patched slots of a packed exe belong to its stub, which rebuilds the real import table after
            // unpacking, so the unpacked code never reaches the wrappers. Inline hooks catch its first call.
            if (g_entry.end) InstallInlineHooks(true);
            return true;
        }
        UAL_LOG("the executable imports none of the entry point functions");
        // nothing to patch in the exe (.NET / UWP launchers), so calls from any game module count
        return InstallInlineHooks(false);
    }

    bool LoadedByProtectionStub()
    {
        pe::Image exe(GetModuleHandleW(nullptr));
        if (!exe.Valid()) return false;
        DWORD entry = exe.nt->OptionalHeader.AddressOfEntryPoint;
        const auto& dir = exe.nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (!entry || !dir.VirtualAddress) return false;
        auto contains = [](const IMAGE_SECTION_HEADER& s, DWORD rva) { return rva >= s.VirtualAddress && rva < s.VirtualAddress + (std::max)(s.Misc.VirtualSize, s.SizeOfRawData); };
        auto sec = IMAGE_FIRST_SECTION(exe.nt);
        const IMAGE_SECTION_HEADER* firstCode = FirstCodeSection(exe);
        const IMAGE_SECTION_HEADER* entrySection = nullptr;
        for (WORD i = 0; i < exe.nt->FileHeader.NumberOfSections; ++i)
            if (contains(sec[i], entry)) entrySection = &sec[i];
        if (!entrySection || entrySection == firstCode || !contains(*entrySection, dir.VirtualAddress)) return false;
        bool importsLoader = false;
        std::string self = WideToAnsi(ToLower(Self().name));
        pe::ForEachImport(exe, [&](const pe::ImportSlot& s) {
            importsLoader |= s.dll == self;
            return !importsLoader;
        });
        return !importsLoader;
    }

    bool HookEntryPointsInline()
    {
        if (!ClaimHooking()) return false;
        UAL_LOG("loaded by a protection stub that resolves the game's imports itself");
        InitRanges();
        if (!InstallInlineHooks(true))
        {
            // Nothing can tell us when game code starts. We are inside DllMain, where loading plugins (dialogs,
            // plugin DllMains, waits) deadlocks under the loader lock, so load from a thread: it starts as soon
            // as the lock is released, which is as early as possible.
            UAL_LOG("no entry point hook possible, loading from a thread");
            HANDLE t = CreateThread(nullptr, 0, [](void*) -> DWORD { LoadEverything(); return 0; }, nullptr, 0, nullptr);
            if (t) CloseHandle(t);
            else LoadEverything();
        }
        return true;
    }

    void RestoreImports()
    {
        DisableInlineHooks();
        for (auto& m : g_modules)
        {
            RestoreSlots(m.kernel32);
            RestoreSlots(m.ole32);
            RestoreSlots(m.vccorlib);
        }
        if (g_hookMutex)
        {
            CloseHandle(g_hookMutex);
            g_hookMutex = nullptr;
        }
    }

    void LoadEverything()
    {
        if (InterlockedCompareExchange(&g_everythingLoaded, 1, 0) != 0) return;
        g_started = 1;
        // Hooks installed from here on (VFS, window mode, plugin callbacks) live for the whole process.
        // Pin so a game that loads and frees us (GTA 2 probes ddraw.dll) can't unmap them.
        HMODULE pinned;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN, (LPCWSTR)&LoadEverything, &pinned);
        UAL_LOG("loading%s", LoaderLockHeldByThisThread() ? " (loader lock held)" : "");
        proxy::LoadOriginalLibrary();
        vfs::Setup();
#ifndef _WIN64
        if (GetSettings().d3d8DisableMaximizedWindowedModeShim)
            compat::d3d8::DisableMaximizedWindowedModeShim(IEquals(Self().name, L"d3d8.dll") ? proxy::OriginalModule() : nullptr);
#endif
        plugins::LoadAll(vfs::PluginFolders());
        UAL_LOG("plugins loaded");
    }
}
