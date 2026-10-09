#include "proxy.hpp"
#include "bink.hpp"
#include "../core/loader.hpp"
#include "../core/paths.hpp"
#include "../core/pe.hpp"
#include "../core/strings.hpp"
#include "../core/log.hpp"
#include "../../compat/xidi/xidi.hpp"
#ifndef _WIN64
#include "../../compat/vorbisfile/vorbisfile.hpp"
#include <d3d8to9/source/d3d8to9.hpp>
extern "C" Direct3D8* WINAPI Direct3DCreate8(UINT SDKVersion);
#endif
#include <cstring>
#include <string>

namespace ual::proxy
{
    enum ProxyFlags
    {
        UAL_SHARED = 1,    // also provides DllCanUnloadNow, DllGetClassObject, ...
        UAL_APPCOMPAT = 2, // provides SetAppCompatStringPointer
        UAL_CUSTOM = 4,    // has its own loading code (vorbisfile)
    };

    // One FARPROC per original export, named <proxy>_<export>.

    struct Slots
    {
#define UAL_PROXY(id, flags)
#define UAL_SLOT(id, field, name) FARPROC id##_##field;
#define UAL_SLOT_ORD(id, field, ordinal) FARPROC id##_##field;
#include "exports.inl"
#undef UAL_PROXY
#undef UAL_SLOT
#undef UAL_SLOT_ORD
    };
    Slots g_slots{};

    namespace
    {
        struct SlotInfo
        {
            const char* proxy;
            FARPROC* slot;
            const char* name; // null for lookup by ordinal
            WORD ordinal;
        };

        const SlotInfo kSlots[] = {
#define UAL_PROXY(id, flags)
#define UAL_SLOT(id, field, name) { #id, &g_slots.id##_##field, name, 0 },
#define UAL_SLOT_ORD(id, field, ordinal) { #id, &g_slots.id##_##field, nullptr, ordinal },
#include "exports.inl"
#undef UAL_PROXY
#undef UAL_SLOT
#undef UAL_SLOT_ORD
        };

        struct ProxyInfo
        {
            const char* id;
            int flags;
        };

        const ProxyInfo kProxies[] = {
#define UAL_PROXY(id, flags) { #id, flags },
#define UAL_SLOT(id, field, name)
#define UAL_SLOT_ORD(id, field, ordinal)
#include "exports.inl"
#undef UAL_PROXY
#undef UAL_SLOT
#undef UAL_SLOT_ORD
        };

        // loader file name -> proxy
        struct ProxyName
        {
            const wchar_t* file;
            const char* proxy;
        };

        const ProxyName kNames[] = {
            { L"dsound.dll", "dsound" },         { L"dinput8.dll", "dinput8" },     { L"wininet.dll", "wininet" },
            { L"version.dll", "version" },       { L"d3d9.dll", "d3d9" },           { L"d3d10.dll", "d3d10" },
            { L"d3d11.dll", "d3d11" },           { L"d3d12.dll", "d3d12" },         { L"dxgi.dll", "dxgi" },
            { L"winmm.dll", "winmm" },           { L"winhttp.dll", "winhttp" },     { L"xinput1_1.dll", "xinput" },
            { L"xinput1_2.dll", "xinput" },      { L"xinput1_3.dll", "xinput" },    { L"xinput1_4.dll", "xinput" },
            { L"xinput9_1_0.dll", "xinput" },    { L"xinputuap.dll", "xinput" },
#ifndef _WIN64
            { L"vorbisFile.dll", "vorbisfile" }, { L"ddraw.dll", "ddraw" },         { L"d3d8.dll", "d3d8" },
            { L"msacm32.dll", "msacm32" },       { L"dinput.dll", "dinput" },       { L"msvfw32.dll", "msvfw32" },
#endif
        };

        HMODULE g_original = nullptr;
        volatile LONG g_loading = 0;
        SRWLOCK g_loadLock = SRWLOCK_INIT;

        const ProxyInfo* FindProxy(const char* id)
        {
            for (const auto& p : kProxies)
                if (!strcmp(p.id, id)) return &p;
            return nullptr;
        }

        void Fill(const char* proxy, HMODULE m)
        {
            for (const auto& s : kSlots)
                if (!strcmp(s.proxy, proxy)) *s.slot = s.name ? GetProcAddress(m, s.name) : GetProcAddress(m, MAKEINTRESOURCEA(s.ordinal));
        }

        void ReportMissingOriginal(const std::wstring& path, DWORD error)
        {
            std::wstring msg = L"Unable to load the original " + Self().name + L":\n" + path + L"\n\nError " + std::to_wstring(error) +
                               L". Functions of this DLL will not work.";
            MessageBoxW(nullptr, msg.c_str(), L"ASI Loader", MB_ICONERROR | MB_OK);
        }

        // Xidi.32.dll / Xidi.64.dll next to the loader replaces what Xidi's forwarder of this name would
        void FillFromXidi(const char* proxy)
        {
            HMODULE xidi = compat::xidi::Load(proxy, Self().dir);
            if (!xidi) return;
            int n = 0;
            for (const auto& s : kSlots)
                if (s.name && (!strcmp(s.proxy, proxy) || !strcmp(s.proxy, "shared")))
                    if (FARPROC f = compat::xidi::Export(xidi, proxy, s.name))
                    {
                        *s.slot = f;
                        ++n;
                    }
            UAL_LOG("Xidi: %d %s exports", n, proxy);
        }

#ifndef _WIN64
        // vorbisFileHooked.dll or vorbisHooked.dll next to the loader (both names are in use),
        // otherwise the built-in vorbisfile from compat/vorbisfile
        void LoadVorbisFile()
        {
            const auto& self = Self();
            for (const wchar_t* name : { L"vorbisFileHooked.dll", L"vorbisHooked.dll" })
            {
                auto path = self.dir + name;
                if (!FileExists(path)) continue;
                if (HMODULE m = LoadLibraryW(path.c_str()))
                {
                    g_original = m;
                    Fill("vorbisfile", m);
                    return;
                }
            }
            for (const auto& s : kSlots)
                if (!strcmp(s.proxy, "vorbisfile")) *s.slot = compat::vorbisfile::BuiltinExport(s.name);

            // CLEO 4.1.1.30f crashes unless the exe is writable, it relied on a side effect
            // of the old in-memory vorbisfile.dll.
            pe::Image exe(GetModuleHandleW(nullptr));
            DWORD old;
            if (exe.Valid()) VirtualProtect((void*)exe.base, exe.nt->OptionalHeader.SizeOfImage, PAGE_EXECUTE_READWRITE, &old);
        }

        // xlive.dll is served by xliveless. GfWL-protected games need their code writable.
        void LoadXLive()
        {
            pe::ForEachSection(GetModuleHandleW(nullptr), { ".text", ".rdata" }, [](IMAGE_SECTION_HEADER* sec, uintptr_t start, size_t size) {
                DWORD old = 0;
                DWORD prot = (sec->Characteristics & IMAGE_SCN_MEM_EXECUTE) ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE;
                if (!VirtualProtect((void*)start, size, prot, &old)) ExitProcess(0);
            });
        }
#endif

        void Load()
        {
            const auto& self = Self();
            if (bink::Load(self.name)) return;
#ifndef _WIN64
            if (IEquals(self.name, L"vorbisFile.dll")) return LoadVorbisFile();
            if (IEquals(self.name, L"xlive.dll")) return LoadXLive();
#endif
            const ProxyName* entry = nullptr;
            for (const auto& n : kNames)
                if (IEquals(self.name, n.file)) entry = &n;
            if (!entry)
            {
                MessageBoxW(nullptr, L"This library isn't supported.", L"ASI Loader", MB_ICONERROR);
                ExitProcess(0);
            }

            bool hooked = false;
            auto path = self.dir + self.stem + L"Hooked.dll";
            HMODULE m = nullptr;
            if (FileExists(path))
            {
                hooked = true;
                m = LoadLibraryW(path.c_str());
            }
            else
            {
                path = SystemDirectory() + self.name;
                m = LoadLibraryW(path.c_str());
                // xinput1_1..1_3 come with the DirectX redistributable and are often missing.
                // The system XInput is compatible.
                if (!m && !strcmp(entry->proxy, "xinput"))
                    for (const wchar_t* alt : { L"xinput1_4.dll", L"xinput9_1_0.dll" })
                        if ((m = LoadLibraryW((SystemDirectory() + alt).c_str())) != nullptr) break;
            }
            if (!m)
            {
                ReportMissingOriginal(path, GetLastError());
                return;
            }
            g_original = m;
            Fill(entry->proxy, m);
            if (auto p = FindProxy(entry->proxy))
            {
                if (p->flags & UAL_SHARED) LoadSharedExports(m);
                if (p->flags & UAL_APPCOMPAT) Fill("appcompat", m);
            }
            if (!hooked) FillFromXidi(entry->proxy);
#ifndef _WIN64
            if (!strcmp(entry->proxy, "d3d8") && !hooked && GetSettings().useD3D8to9) g_slots.d3d8_Direct3DCreate8 = (FARPROC)Direct3DCreate8;
#endif
        }
    }

    void LoadOriginalLibrary()
    {
        static volatile DWORD loadingThread = 0;
        if (g_loading == 2 || loadingThread == GetCurrentThreadId()) return; // done, or re-entered while loading
        AcquireSRWLockExclusive(&g_loadLock); // other threads wait until the original is usable
        if (g_loading == 0)
        {
            g_loading = 1;
            loadingThread = GetCurrentThreadId();
            Load();
            loadingThread = 0;
            g_loading = 2;
        }
        ReleaseSRWLockExclusive(&g_loadLock);
    }

    bool IsSupportedName(const std::wstring& fileName)
    {
        for (const auto& n : kNames)
            if (IEquals(fileName, n.file)) return true;
#ifndef _WIN64
        if (IEquals(fileName, L"xlive.dll") || IEquals(fileName, L"binkw32.dll") || IEquals(fileName, L"bink2w32.dll")) return true;
#else
        if (IEquals(fileName, L"bink2w64.dll") || IEquals(fileName, L"binkw64.dll")) return true;
#endif
        return false;
    }

    HMODULE OriginalModule()
    {
        return g_original;
    }

    void LoadSharedExports(HMODULE dll)
    {
        Fill("shared", dll);
    }
}

// Each export is a jump through its slot, so any signature or calling convention works
// and the stack is untouched. x86: jmp [slot]. x64: mov rax, &slot; jmp [rax]
// (rax is volatile on entry in the x64 ABI).

#pragma pack(push, 1)
struct ProxyThunk
{
#ifdef _WIN64
    unsigned char mov[2];
    FARPROC* slot;
    unsigned char jmp[2];
#else
    unsigned char jmp[2];
    FARPROC* slot;
#endif
};
#pragma pack(pop)

#ifdef _WIN64
#define UAL_THUNK_CODE(slot) { { 0x48, 0xB8 }, slot, { 0xFF, 0x20 } }
#else
#define UAL_THUNK_CODE(slot) { { 0xFF, 0x25 }, slot }
#endif

#pragma section(".ualprx", read, execute)

extern "C"
{
#define UAL_THUNK(symbol, id, field) __declspec(allocate(".ualprx")) extern const ProxyThunk symbol = UAL_THUNK_CODE(&ual::proxy::g_slots.id##_##field);
#include "thunks.inl"
#undef UAL_THUNK
}

namespace ual::proxy
{
    namespace
    {
        struct NamedThunk
        {
            const char* proxy;
            const char* name;
            const void* entry;
        };

        const NamedThunk kThunks[] = {
#define UAL_THUNK(symbol, id, field) { #id, #field, &symbol },
#include "thunks.inl"
#undef UAL_THUNK
        };

        struct StaticOrdinal
        {
            const wchar_t* dll; // lower case
            WORD ordinal;
            const void* entry;
        };

        const StaticOrdinal kOrdinals[] = {
#define UAL_ORDINAL(dll, ordinal, thunk) { dll, ordinal, &thunk },
#include "ordinals.inl"
#undef UAL_ORDINAL
        };

        // calls f(ordinal, name) for each named export
        template<class F>
        void ForEachNamedExport(const BYTE* base, F&& f)
        {
            auto dos = (const IMAGE_DOS_HEADER*)base;
            if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
            auto nt = (const IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
            if (nt->Signature != IMAGE_NT_SIGNATURE) return;
            const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
            if (!dir.VirtualAddress || !dir.Size) return;
            auto exp = (const IMAGE_EXPORT_DIRECTORY*)(base + dir.VirtualAddress);
            auto names = (const DWORD*)(base + exp->AddressOfNames);
            auto ordinals = (const WORD*)(base + exp->AddressOfNameOrdinals);
            for (DWORD i = 0; i < exp->NumberOfNames; ++i) f((WORD)(exp->Base + ordinals[i]), (const char*)(base + names[i]));
        }
    }

    const void* ExportFor(std::wstring_view dllName, std::string_view name)
    {
        const ProxyName* entry = nullptr;
        for (const auto& n : kNames)
            if (IEquals(dllName, n.file)) entry = &n;
        if (!entry) return nullptr;
        auto p = FindProxy(entry->proxy);
        for (const auto& t : kThunks)
        {
            if (name != t.name) continue;
            if (!strcmp(t.proxy, entry->proxy) || (p && (p->flags & UAL_SHARED) && !strcmp(t.proxy, "shared")) ||
                (p && (p->flags & UAL_APPCOMPAT) && !strcmp(t.proxy, "appcompat")))
                return t.entry;
        }
        return nullptr;
    }

    std::vector<std::pair<WORD, const void*>> OrdinalEntries(std::wstring_view dllName)
    {
        std::vector<std::pair<WORD, const void*>> result;
        auto set = [&](WORD ordinal, const void* e) {
            for (auto& r : result)
                if (r.first == ordinal)
                {
                    r.second = e;
                    return;
                }
            result.emplace_back(ordinal, e);
        };
        for (const auto& o : kOrdinals)
            if (IEquals(dllName, o.dll)) set(o.ordinal, o.entry);

        // mapped as an image resource, so no DllMain runs and no imports are resolved
        auto path = SystemDirectory() + std::wstring(dllName);
        if (HMODULE image = LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE))
        {
            ForEachNamedExport((const BYTE*)((uintptr_t)image & ~(uintptr_t)3), [&](WORD ordinal, const char* name) {
                if (auto e = ExportFor(dllName, name)) set(ordinal, e);
            });
            FreeLibrary(image);
        }
        return result;
    }
}
