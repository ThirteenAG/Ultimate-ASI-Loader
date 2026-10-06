// Every named export of each system DLL must be exported by the loader too, or a game
// importing it fails to start with "entry point not found".
#include "framework.hpp"
#include "proxies.hpp"
#include <map>
#include <set>

using namespace runner;

namespace
{
    // Mapped as an image resource, so no code runs and the x64 runner can read x86 DLLs
    std::set<std::string> NamedExports(const fs::path& dll)
    {
        std::set<std::string> names;
        HMODULE h = LoadLibraryExW(dll.c_str(), nullptr, LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE);
        if (!h) return names;
        auto base = reinterpret_cast<BYTE*>(reinterpret_cast<ULONG_PTR>(h) & ~(ULONG_PTR)3);
        auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        auto nt32 = reinterpret_cast<IMAGE_NT_HEADERS32*>(base + dos->e_lfanew);
        IMAGE_DATA_DIRECTORY dir = nt32->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC
            ? reinterpret_cast<IMAGE_NT_HEADERS64*>(nt32)->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT]
            : nt32->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
        if (dir.VirtualAddress)
        {
            auto exp = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(base + dir.VirtualAddress);
            auto nameRvas = reinterpret_cast<DWORD*>(base + exp->AddressOfNames);
            for (DWORD i = 0; i < exp->NumberOfNames; ++i)
                names.insert(reinterpret_cast<const char*>(base + nameRvas[i]));
        }
        FreeLibrary(h);
        return names;
    }

    // Reviewed system exports the loader doesn't provide. Any other missing export fails, which
    // catches new Windows exports and accidental removals from x86.def/x64.def. Only list
    // functions games don't import, since such a game won't start with the loader.
    const std::map<std::wstring, std::map<std::string, const char*>> kKnownExportGaps = {
        { L"dinput8.dll", { { "GetdfDIJoystick", "helper behind the static c_dfDIJoystick in dinput8.lib; games never import it" } } },
        { L"d3d8.dll", { { "Direct3D8EnableMaximizedWindowedModeShim", "commented out on purpose in source/loader/x86.def (used by the AppCompat engine)" } } },
        { L"dxgi.dll", { { "UpdateHMDEmulationStatus", "private export" } } },
        { L"msacm32.dll", { { "XRegThunkEntry", "internal thunk" }, { "acmMessage32", "internal thunk" } } },
        { L"wininet.dll", { { "HttpIndicatePageLoadComplete", "newer Windows export that the loader does not forward yet" },
                            { "InternetSetSecureLegacyServersAppCompat", "newer Windows export that the loader does not forward yet" } } },
        { L"winhttp.dll", { { "WinHttpConnectionDeletePolicyEntriesByAppSid", "newer Windows export that the loader does not forward yet" },
                            { "WinHttpCreateProxyList", "newer Windows export that the loader does not forward yet" },
                            { "WinHttpCreateProxyManager", "newer Windows export that the loader does not forward yet" },
                            { "WinHttpCreateProxyResult", "newer Windows export that the loader does not forward yet" },
                            { "WinHttpCreateUiCompatibleProxyString", "newer Windows export that the loader does not forward yet" },
                            { "WinHttpProtocolCompleteUpgrade", "newer Windows export that the loader does not forward yet" },
                            { "WinHttpProtocolReceive", "newer Windows export that the loader does not forward yet" },
                            { "WinHttpProtocolSend", "newer Windows export that the loader does not forward yet" },
                            { "WinHttpRefreshProxySettings", "newer Windows export that the loader does not forward yet" },
                            { "WinHttpResolverGetProxyForUrl", "newer Windows export that the loader does not forward yet" } } },
    };

    fs::path SystemDll(const Arch& arch, const std::wstring& name)
    {
        wchar_t dir[MAX_PATH];
        if (arch.is64()) GetSystemDirectoryW(dir, MAX_PATH);
        else GetSystemWow64DirectoryW(dir, MAX_PATH);
        return fs::path(dir) / name;
    }

    struct ExportRegistrar
    {
        ExportRegistrar()
        {
            for (auto& p : kProxies)
            {
                std::string only = p.win32 && p.x64 ? "" : p.win32 ? "Win32" : "x64";
                AddArchTest("proxy " + ut::narrow(p.file) + ": exports every named function of the system DLL", "[exports][proxy]", [p](const Arch& arch) {
                    auto sys = SystemDll(arch, p.file);
                    if (!fs::exists(sys)) SKIP("no system " + ut::narrow(p.file) + " on this machine");
                    auto wanted = NamedExports(sys);
                    if (wanted.empty()) SKIP("cannot read the exports of " + ut::narrow(sys.wstring()));
                    auto have = NamedExports(arch.loader());
                    REQUIRE_MSG(!have.empty(), "cannot read the exports of the loader");
                    auto gaps = kKnownExportGaps.find(Lower(p.file));
                    std::string missing, accepted;
                    int count = 0;
                    for (auto& n : wanted)
                    {
                        if (have.count(n)) continue;
                        if (gaps != kKnownExportGaps.end() && gaps->second.count(n)) { accepted += n + " (" + gaps->second.at(n) + ") "; continue; }
                        missing += n + " ";
                        ++count;
                    }
                    if (!accepted.empty()) INFO("accepted gaps: " + accepted);
                    CHECK_MSG(count == 0, std::to_string(count) + " of " + std::to_string(wanted.size()) + " exports missing: " + missing);
                }, only);
            }
        }
    } g_exportRegistrar;
}

ARCH_TEST("the loader exports its own API under stable names", "[exports][api]")
{
    auto have = NamedExports(arch.loader());
    for (auto n : { "IsUltimateASILoader", "GetOverloadPathA", "GetOverloadPathW", "GetOverloadedFilePathA", "GetOverloadedFilePathW",
                    "AddVirtualFileForOverloadA", "AddVirtualFileForOverloadW", "RemoveVirtualFileFromOverloadA", "RemoveVirtualFileFromOverloadW",
                    "AddVirtualPathForOverloadA", "AddVirtualPathForOverloadW", "RemoveVirtualPathFromOverloadA", "RemoveVirtualPathFromOverloadW" })
        CHECK_MSG(have.count(n) == 1, std::string("missing export ") + n);
}
