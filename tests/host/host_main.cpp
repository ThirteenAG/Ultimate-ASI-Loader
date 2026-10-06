// Test host entry point. See host.hpp.
//
// Actions, one per argument, run in order:
//   load:<dll>                   LoadLibraryW
//   trigger:<api>                call a kernel32 API from the exe (plugin load trigger)
//   snapshot                     record loaded probe plugins and loaders
//   chdir:<dir>                  SetCurrentDirectoryW
//   wait:<ms>                    wait without calling any hooked API
//   read:<path>   readA:<path>   read2:<path>   readstd:<path>
//                                read via CreateFileW / CreateFileA / CreateFile2 / std::ifstream
//   attr:<path>   attrA:<path>   attrex:<path>   attrexA:<path>
//                                GetFileAttributes[Ex][A|W]
//   find:<pattern> findA:<pattern> findex:<pattern> findexA:<pattern>
//                                FindFirstFile[Ex]/FindNextFile enumeration
//   loadlib:<path> loadlibA:<path>  LoadLibraryW/A, records the loaded module's path
//   free:<dll>                   FreeLibrary until unmapped or refused, records whether still mapped
//   write:<path>|<content>       CreateFileW(CREATE_ALWAYS) + WriteFile
//   ovpath                       GetOverloadPathW/A
//   ovfile:<path>                GetOverloadedFilePathW/A
//   vadd:<path>|<content>|<prio> vaddA:...   AddVirtualFileForOverloadW/A
//   vrm:<path>  vrmA:<path>      RemoveVirtualFileFromOverloadW/A
//   vpath:<orig>|<target>|<prio> vpathA:...  AddVirtualPathForOverloadW/A
//   vpathrm:<orig> vpathrmA:<orig>           RemoveVirtualPathFromOverloadW/A
//   setfilter                    SetUnhandledExceptionFilter(host filter)
//   crash                        access violation, WER UI suppressed
//   throw:<message>              unhandled std::runtime_error(message)
//   stackoverflow                unbounded recursion
//   crashthread                  access violation on a new thread
//   scenario:<name>[|args]       run an in-process scenario from scenarios_*.cpp
//   stubcall                     Sleep(0) from the stub section (ual_host_stub.exe only)
//   target                       report g_ualSnippetTarget, which .cxx snippets patch
//   exit:<code>                  stop and return <code>
#include "host.hpp"
#include <psapi.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <stdexcept>

extern "C" __declspec(dllexport) volatile LONG g_ualHostMainEntered = 0;
// .cxx snippets patch this, the "target" action reports it
extern "C" __declspec(dllexport) volatile int g_ualSnippetTarget[4] = { 1, 2, 3, 4 };

namespace host
{
    std::map<std::string, ScenarioFn>& Scenarios()
    {
        static std::map<std::string, ScenarioFn> m;
        return m;
    }

    // --- helpers

    void (*g_stubCall)() = nullptr;

    std::wstring ExeDir()
    {
        std::wstring s(32768, L'\0');
        s.resize(GetModuleFileNameW(nullptr, s.data(), (DWORD)s.size()));
        return s.substr(0, s.find_last_of(L"\\/"));
    }

    std::wstring SystemDir()
    {
        wchar_t buf[MAX_PATH];
        GetSystemDirectoryW(buf, MAX_PATH);
        return buf;
    }

    std::string Acp(const std::wstring& s)
    {
        if (s.empty()) return {};
        int n = WideCharToMultiByte(CP_ACP, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
        std::string r(n, '\0');
        WideCharToMultiByte(CP_ACP, 0, s.data(), (int)s.size(), r.data(), n, nullptr, nullptr);
        return r;
    }

    std::vector<std::wstring> Split(const std::wstring& s, wchar_t sep)
    {
        std::vector<std::wstring> out;
        size_t start = 0;
        while (true)
        {
            auto p = s.find(sep, start);
            out.push_back(s.substr(start, p == std::wstring::npos ? std::wstring::npos : p - start));
            if (p == std::wstring::npos) break;
            start = p + 1;
        }
        return out;
    }

    bool ReadAll(HANDLE h, std::string& out, DWORD chunk)
    {
        std::vector<char> buf(chunk);
        for (;;)
        {
            DWORD n = 0;
            if (!::ReadFile(h, buf.data(), chunk, &n, nullptr))
                return false;
            if (n == 0)
                return true;
            out.append(buf.data(), n);
        }
    }

    std::string ReadFileW(const std::wstring& path, DWORD* err)
    {
        std::string data;
        HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
        {
            if (err) *err = GetLastError();
            return {};
        }
        ReadAll(h, data);
        CloseHandle(h);
        if (err) *err = 0;
        return data;
    }

    bool WriteFileW(const std::wstring& path, const std::string& data)
    {
        HANDLE h = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE)
            return false;
        DWORD n = 0;
        BOOL ok = ::WriteFile(h, data.data(), (DWORD)data.size(), &n, nullptr);
        CloseHandle(h);
        return ok && n == data.size();
    }

    HMODULE FindModuleByName(const wchar_t* fileName)
    {
        HMODULE mods[1024];
        DWORD needed = 0;
        if (!K32EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &needed))
            return nullptr;
        for (DWORD i = 0; i < needed / sizeof(HMODULE) && i < 1024; ++i)
        {
            wchar_t p[MAX_PATH * 4];
            DWORD n = GetModuleFileNameW(mods[i], p, (DWORD)std::size(p));
            std::wstring s(p, n);
            auto name = s.substr(s.find_last_of(L"\\/") + 1);
            if (_wcsicmp(name.c_str(), fileName) == 0)
                return mods[i];
        }
        return nullptr;
    }

    bool AddressInModule(const void* addr, HMODULE mod)
    {
        HMODULE owner = nullptr;
        if (!addr || !mod) return false;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)addr, &owner);
        return owner == mod;
    }

    // --- UalApi

    bool UalApi::Resolve()
    {
        HMODULE mods[1024];
        DWORD needed = 0;
        module = nullptr;
        if (K32EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &needed))
        {
            for (DWORD i = 0; i < needed / sizeof(HMODULE) && i < 1024; ++i)
                if (GetProcAddress(mods[i], "IsUltimateASILoader")) { module = mods[i]; break; }
        }
        if (!module) return false;
        auto get = [&](auto& fn, const char* name) { fn = reinterpret_cast<std::remove_reference_t<decltype(fn)>>(GetProcAddress(module, name)); };
        get(IsUltimateASILoader, "IsUltimateASILoader");
        get(GetOverloadPathA, "GetOverloadPathA");
        get(GetOverloadPathW, "GetOverloadPathW");
        get(GetOverloadedFilePathA, "GetOverloadedFilePathA");
        get(GetOverloadedFilePathW, "GetOverloadedFilePathW");
        get(AddVirtualFileForOverloadA, "AddVirtualFileForOverloadA");
        get(AddVirtualFileForOverloadW, "AddVirtualFileForOverloadW");
        get(RemoveVirtualFileFromOverloadA, "RemoveVirtualFileFromOverloadA");
        get(RemoveVirtualFileFromOverloadW, "RemoveVirtualFileFromOverloadW");
        get(AddVirtualPathForOverloadA, "AddVirtualPathForOverloadA");
        get(AddVirtualPathForOverloadW, "AddVirtualPathForOverloadW");
        get(RemoveVirtualPathFromOverloadA, "RemoveVirtualPathFromOverloadA");
        get(RemoveVirtualPathFromOverloadW, "RemoveVirtualPathFromOverloadW");
        return true;
    }

    bool UalApi::Complete() const
    {
        return module && IsUltimateASILoader && GetOverloadPathA && GetOverloadPathW && GetOverloadedFilePathA && GetOverloadedFilePathW &&
            AddVirtualFileForOverloadA && AddVirtualFileForOverloadW && RemoveVirtualFileFromOverloadA && RemoveVirtualFileFromOverloadW &&
            AddVirtualPathForOverloadA && AddVirtualPathForOverloadW && RemoveVirtualPathFromOverloadA && RemoveVirtualPathFromOverloadW;
    }

    // --- Host

    void Host::Emit(ualtest::Record r)
    {
        r["src"] = "host";
        if (!currentScenario.empty() && !r.has("scenario"))
            r["scenario"] = currentScenario;
        ualtest::append_record(report, r);
    }

    bool Host::Check(bool ok, const std::string& name, const std::string& detail)
    {
        ++totalChecks;
        if (!ok) ++failedChecks;
        ualtest::Record r;
        r["ev"] = "check";
        r["name"] = name;
        r["ok"] = ok ? "1" : "0";
        if (!detail.empty()) r["detail"] = detail;
        Emit(r);
        return ok;
    }

    bool Host::RequireUal()
    {
        if (!ual.module) ual.Resolve();
        return Check(ual.Complete(), "loader module with all API exports is present");
    }
}

using namespace host;

// --- actions

static ualtest::Record Base(const char* ev, const std::wstring& arg)
{
    ualtest::Record r;
    r["ev"] = ev;
    r["arg"] = ualtest::utf8(arg);
    return r;
}

static void SetResult(ualtest::Record& r, bool ok, DWORD err)
{
    r["ok"] = ok ? "1" : "0";
    r["err"] = std::to_string(ok ? 0 : err);
}

static void ActionRead(Host& h, const std::wstring& verb, const std::wstring& path)
{
    auto r = Base("read", path);
    r["api"] = ualtest::utf8(verb);
    std::string data;
    bool ok = false;
    DWORD err = 0;
    if (verb == L"readstd")
    {
        std::ifstream f(std::filesystem::path(path), std::ios::binary);
        ok = f.is_open();
        if (ok)
        {
            std::stringstream ss;
            ss << f.rdbuf();
            data = ss.str();
        }
        else
            err = ERROR_FILE_NOT_FOUND;
    }
    else
    {
        HANDLE fh = INVALID_HANDLE_VALUE;
        if (verb == L"readA")
            fh = CreateFileA(Acp(path).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        else if (verb == L"read2")
            fh = CreateFile2(path.c_str(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr);
        else
            fh = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        err = GetLastError();
        if (fh != INVALID_HANDLE_VALUE)
        {
            ok = ReadAll(fh, data);
            err = GetLastError();
            LARGE_INTEGER size{};
            if (GetFileSizeEx(fh, &size)) r["filesize"] = std::to_string(size.QuadPart);
            CloseHandle(fh);
        }
    }
    SetResult(r, ok, err);
    if (ok)
    {
        r["data"] = data;
        r["size"] = std::to_string(data.size());
    }
    h.Emit(r);
}

static void ActionAttr(Host& h, const std::wstring& verb, const std::wstring& path)
{
    auto r = Base("attr", path);
    r["api"] = ualtest::utf8(verb);
    if (verb == L"attr" || verb == L"attrA")
    {
        DWORD a = verb == L"attrA" ? GetFileAttributesA(Acp(path).c_str()) : GetFileAttributesW(path.c_str());
        DWORD err = GetLastError();
        SetResult(r, a != INVALID_FILE_ATTRIBUTES, err);
        r["attrs"] = std::to_string(a);
        if (a != INVALID_FILE_ATTRIBUTES) r["dir"] = (a & FILE_ATTRIBUTE_DIRECTORY) ? "1" : "0";
    }
    else
    {
        WIN32_FILE_ATTRIBUTE_DATA d{};
        BOOL ok = verb == L"attrexA" ? GetFileAttributesExA(Acp(path).c_str(), GetFileExInfoStandard, &d) : GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &d);
        DWORD err = GetLastError();
        SetResult(r, ok != FALSE, err);
        if (ok)
        {
            r["attrs"] = std::to_string(d.dwFileAttributes);
            r["size"] = std::to_string(((uint64_t)d.nFileSizeHigh << 32) | d.nFileSizeLow);
        }
    }
    h.Emit(r);
}

static void ActionFind(Host& h, const std::wstring& verb, const std::wstring& pattern)
{
    auto r = Base("find", pattern);
    r["api"] = ualtest::utf8(verb);
    std::string names;
    int count = 0;
    bool ansi = verb == L"findA" || verb == L"findexA";
    bool ex = verb == L"findex" || verb == L"findexA";
    auto add = [&](const std::string& name, DWORD hi, DWORD lo) {
        if (!names.empty()) names += ";";
        names += name + ":" + std::to_string(((uint64_t)hi << 32) | lo);
        ++count;
    };
    if (ansi)
    {
        WIN32_FIND_DATAA fd{};
        HANDLE fh = ex ? FindFirstFileExA(Acp(pattern).c_str(), FindExInfoStandard, &fd, FindExSearchNameMatch, nullptr, 0) : FindFirstFileA(Acp(pattern).c_str(), &fd);
        DWORD err = GetLastError();
        SetResult(r, fh != INVALID_HANDLE_VALUE, err);
        if (fh != INVALID_HANDLE_VALUE)
        {
            do
            {
                std::wstring w(MultiByteToWideChar(CP_ACP, 0, fd.cFileName, -1, nullptr, 0), L'\0');
                MultiByteToWideChar(CP_ACP, 0, fd.cFileName, -1, w.data(), (int)w.size());
                w.resize(wcslen(w.c_str()));
                add(ualtest::utf8(w), fd.nFileSizeHigh, fd.nFileSizeLow);
            } while (FindNextFileA(fh, &fd));
            FindClose(fh);
        }
    }
    else
    {
        WIN32_FIND_DATAW fd{};
        HANDLE fh = ex ? FindFirstFileExW(pattern.c_str(), FindExInfoStandard, &fd, FindExSearchNameMatch, nullptr, 0) : FindFirstFileW(pattern.c_str(), &fd);
        DWORD err = GetLastError();
        SetResult(r, fh != INVALID_HANDLE_VALUE, err);
        if (fh != INVALID_HANDLE_VALUE)
        {
            do add(ualtest::utf8(fd.cFileName), fd.nFileSizeHigh, fd.nFileSizeLow);
            while (FindNextFileW(fh, &fd));
            FindClose(fh);
        }
    }
    r["names"] = names;
    r["count"] = std::to_string(count);
    h.Emit(r);
}

static void ActionLoadLib(Host& h, const std::wstring& verb, const std::wstring& path)
{
    auto r = Base(verb == L"load" ? "load" : "loadlib", path);
    HMODULE m = verb == L"loadlibA" ? LoadLibraryA(Acp(path).c_str()) : LoadLibraryW(path.c_str());
    DWORD err = GetLastError();
    SetResult(r, m != nullptr, err);
    if (m)
    {
        wchar_t p[MAX_PATH * 4];
        DWORD n = GetModuleFileNameW(m, p, (DWORD)std::size(p));
        r["module"] = ualtest::utf8(std::wstring(p, n));
    }
    h.Emit(r);
}

static void ActionTrigger(Host& h, const std::wstring& api)
{
    auto r = Base("trigger", api);
    bool known = true;
    if (api == L"Sleep") Sleep(0);
    else if (api == L"GetSystemTimeAsFileTime") { FILETIME ft; GetSystemTimeAsFileTime(&ft); }
    else if (api == L"GetSystemInfo") { SYSTEM_INFO si; GetSystemInfo(&si); }
    else if (api == L"GetCurrentProcessId") GetCurrentProcessId();
    else if (api == L"CreateEventW") CloseHandle(CreateEventW(nullptr, FALSE, FALSE, nullptr));
    else if (api == L"CreateEventA") CloseHandle(CreateEventA(nullptr, FALSE, FALSE, nullptr));
    else if (api == L"GetStartupInfoW") { STARTUPINFOW si{ sizeof(si) }; GetStartupInfoW(&si); }
    else if (api == L"GetStartupInfoA") { STARTUPINFOA si{ sizeof(si) }; GetStartupInfoA(&si); }
    else if (api == L"GetCommandLineW") GetCommandLineW();
    else if (api == L"GetCommandLineA") GetCommandLineA();
    else if (api == L"GetModuleHandleW") GetModuleHandleW(nullptr);
    else if (api == L"GetModuleHandleA") GetModuleHandleA(nullptr);
    else if (api == L"GetProcAddress") GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "Sleep");
    else known = false;
    r["known"] = known ? "1" : "0";
    h.Emit(r);
}

static void ActionSnapshot(Host& h)
{
    auto r = Base("snapshot", L"");
    HMODULE mods[1024];
    DWORD needed = 0;
    std::string probes, uals;
    if (K32EnumProcessModules(GetCurrentProcess(), mods, sizeof(mods), &needed))
    {
        for (DWORD i = 0; i < needed / sizeof(HMODULE) && i < 1024; ++i)
        {
            wchar_t p[MAX_PATH * 4];
            DWORD n = GetModuleFileNameW(mods[i], p, (DWORD)std::size(p));
            std::wstring s(p, n);
            auto name = ualtest::utf8(s.substr(s.find_last_of(L"\\/") + 1));
            if (auto count = reinterpret_cast<LONG(*)()>(GetProcAddress(mods[i], "ProbeInitCount")))
                probes += (probes.empty() ? "" : ";") + name + ":" + std::to_string(count());
            if (GetProcAddress(mods[i], "IsUltimateASILoader"))
                uals += (uals.empty() ? "" : ";") + name;
        }
    }
    wchar_t cwd[MAX_PATH * 4];
    GetCurrentDirectoryW((DWORD)std::size(cwd), cwd);
    r["probes"] = probes;
    r["uals"] = uals;
    r["cwd"] = ualtest::utf8(cwd);
    h.Emit(r);
}

static void ActionVirtual(Host& h, const std::wstring& verb, const std::wstring& arg)
{
    if (!h.ual.module) h.ual.Resolve();
    auto parts = Split(arg, L'|');
    auto r = Base(ualtest::utf8(verb).c_str(), arg);
    if (!h.ual.Complete())
    {
        r["ok"] = "0";
        r["error"] = "loader not found";
        h.Emit(r);
        return;
    }
    bool result = true;
    if (verb == L"vadd" || verb == L"vaddA")
    {
        std::string content = parts.size() > 1 ? ualtest::utf8(parts[1]) : "";
        int prio = parts.size() > 2 ? _wtoi(parts[2].c_str()) : 1000;
        result = verb == L"vadd" ? h.ual.AddVirtualFileForOverloadW(parts[0].c_str(), (const uint8_t*)content.data(), content.size(), prio)
                                 : h.ual.AddVirtualFileForOverloadA(Acp(parts[0]).c_str(), (const uint8_t*)content.data(), content.size(), prio);
    }
    else if (verb == L"vrm") h.ual.RemoveVirtualFileFromOverloadW(parts[0].c_str());
    else if (verb == L"vrmA") h.ual.RemoveVirtualFileFromOverloadA(Acp(parts[0]).c_str());
    else if (verb == L"vpath" || verb == L"vpathA")
    {
        std::wstring target = parts.size() > 1 ? parts[1] : L"";
        int prio = parts.size() > 2 ? _wtoi(parts[2].c_str()) : 1000;
        result = verb == L"vpath" ? h.ual.AddVirtualPathForOverloadW(parts[0].c_str(), target.c_str(), prio)
                                  : h.ual.AddVirtualPathForOverloadA(Acp(parts[0]).c_str(), Acp(target).c_str(), prio);
    }
    else if (verb == L"vpathrm") h.ual.RemoveVirtualPathFromOverloadW(parts[0].c_str());
    else if (verb == L"vpathrmA") h.ual.RemoveVirtualPathFromOverloadA(Acp(parts[0]).c_str());
    r["ok"] = result ? "1" : "0";
    h.Emit(r);
}

static void ActionOverloadQuery(Host& h, const std::wstring& verb, const std::wstring& arg)
{
    if (!h.ual.module) h.ual.Resolve();
    auto r = Base(ualtest::utf8(verb).c_str(), arg);
    if (!h.ual.Complete())
    {
        r["ok"] = "0";
        r["error"] = "loader not found";
        h.Emit(r);
        return;
    }
    std::wstring w(4096, L'\0');
    std::string a(4096, '\0');
    bool okW, okA;
    if (verb == L"ovpath")
    {
        okW = h.ual.GetOverloadPathW(w.data(), w.size());
        okA = h.ual.GetOverloadPathA(a.data(), a.size());
    }
    else
    {
        okW = h.ual.GetOverloadedFilePathW(arg.c_str(), w.data(), w.size());
        okA = h.ual.GetOverloadedFilePathA(Acp(arg).c_str(), a.data(), a.size());
    }
    r["ok"] = okW ? "1" : "0";
    r["okA"] = okA ? "1" : "0";
    r["value"] = okW ? ualtest::utf8(w.c_str()) : "";
    if (okA)
    {
        std::wstring aw(MultiByteToWideChar(CP_ACP, 0, a.c_str(), -1, nullptr, 0), L'\0');
        MultiByteToWideChar(CP_ACP, 0, a.c_str(), -1, aw.data(), (int)aw.size());
        r["valueA"] = ualtest::utf8(aw.c_str());
    }
    h.Emit(r);
}

static Host* g_host = nullptr;

static LONG WINAPI HostUnhandledFilter(EXCEPTION_POINTERS*)
{
    if (g_host)
    {
        ualtest::Record r;
        r["ev"] = "host_filter_called";
        g_host->Emit(r);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static void Crash()
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    volatile int* p = nullptr;
    *p = 0x0BADC0DE;
}

#pragma optimize("", off)
#pragma warning(push)
#pragma warning(disable : 4717) // the recursion is the point
static int Recurse(volatile int depth)
{
    volatile char pad[512];
    pad[0] = (char)depth;
    return Recurse(depth + 1) + pad[0];
}
#pragma warning(pop)
#pragma optimize("", on)

static DWORD WINAPI CrashThread(void*)
{
    Crash();
    return 0;
}

int wmain(int argc, wchar_t** argv)
{
    InterlockedExchange(&g_ualHostMainEntered, 1);

    Host h;
    g_host = &h;
    h.report = ualtest::report_path_from_env();

    {
        ualtest::Record r;
        r["ev"] = "host_main";
        r["static_proxy"] = ual_host_static_proxy();
        r["anchor"] = ual_host_static_anchor() ? "1" : "0";
        h.ual.Resolve();
        r["ual"] = h.ual.module ? "1" : "0";
        wchar_t cwd[MAX_PATH * 4];
        GetCurrentDirectoryW((DWORD)std::size(cwd), cwd);
        r["cwd"] = ualtest::utf8(cwd);
        r["arch"] = sizeof(void*) == 8 ? "x64" : "Win32";
        h.Emit(r);
    }

    int exitCode = 0;
    for (int i = 1; i < argc; ++i)
    {
        std::wstring a = argv[i];
        auto colon = a.find(L':');
        std::wstring verb = a.substr(0, colon);
        std::wstring arg = colon == std::wstring::npos ? L"" : a.substr(colon + 1);

        if (verb == L"load") ActionLoadLib(h, verb, arg);
        else if (verb == L"trigger") ActionTrigger(h, arg);
        else if (verb == L"target")
        {
            auto r = Base("target", L"");
            r["value"] = std::to_string(g_ualSnippetTarget[0]) + "," + std::to_string(g_ualSnippetTarget[1]) + "," + std::to_string(g_ualSnippetTarget[2]) + "," +
                         std::to_string(g_ualSnippetTarget[3]);
            h.Emit(r);
        }
        else if (verb == L"stubcall")
        {
            bool available = g_stubCall != nullptr;
            if (available) g_stubCall(); // before anything else of this action calls kernel32
            auto r = Base("stubcall", L"");
            r["available"] = available ? "1" : "0";
            h.Emit(r);
        }
        else if (verb == L"snapshot") ActionSnapshot(h);
        else if (verb == L"chdir")
        {
            auto r = Base("chdir", arg);
            SetResult(r, SetCurrentDirectoryW(arg.c_str()) != FALSE, GetLastError());
            h.Emit(r);
        }
        else if (verb == L"wait") WaitForSingleObject(GetCurrentProcess(), (DWORD)_wtoi(arg.c_str()));
        else if (verb == L"read" || verb == L"readA" || verb == L"read2" || verb == L"readstd") ActionRead(h, verb, arg);
        else if (verb == L"attr" || verb == L"attrA" || verb == L"attrex" || verb == L"attrexA") ActionAttr(h, verb, arg);
        else if (verb == L"find" || verb == L"findA" || verb == L"findex" || verb == L"findexA") ActionFind(h, verb, arg);
        else if (verb == L"loadlib" || verb == L"loadlibA") ActionLoadLib(h, verb, arg);
        else if (verb == L"free")
        {
            // like a game that probes a DLL and frees it again (GTA 2 and ddraw.dll)
            auto r = Base("free", arg);
            int calls = 0;
            for (HMODULE m; (m = GetModuleHandleW(arg.c_str())) && calls < 64; ++calls)
                if (!FreeLibrary(m)) break;
            r["calls"] = std::to_string(calls);
            r["mapped"] = GetModuleHandleW(arg.c_str()) ? "1" : "0";
            h.Emit(r);
        }
        else if (verb == L"write")
        {
            auto parts = Split(arg, L'|');
            auto r = Base("write", parts[0]);
            bool ok = WriteFileW(parts[0], parts.size() > 1 ? ualtest::utf8(parts[1]) : "");
            SetResult(r, ok, GetLastError()); // after the call: argument evaluation order is unspecified
            h.Emit(r);
        }
        else if (verb == L"ovpath" || verb == L"ovfile") ActionOverloadQuery(h, verb, arg);
        else if (verb == L"vadd" || verb == L"vaddA" || verb == L"vrm" || verb == L"vrmA" || verb == L"vpath" || verb == L"vpathA" || verb == L"vpathrm" || verb == L"vpathrmA")
            ActionVirtual(h, verb, arg);
        else if (verb == L"setfilter")
        {
            SetUnhandledExceptionFilter(HostUnhandledFilter);
            h.Emit(Base("setfilter", L""));
        }
        else if (verb == L"crash")
        {
            h.Emit(Base("crash", L""));
            Crash();
        }
        else if (verb == L"throw")
        {
            h.Emit(Base("throw", arg));
            SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
            throw std::runtime_error(ualtest::utf8(arg));
        }
        else if (verb == L"stackoverflow")
        {
            h.Emit(Base("stackoverflow", L""));
            SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
            Recurse(0);
        }
        else if (verb == L"crashthread")
        {
            h.Emit(Base("crashthread", L""));
            HANDLE t = CreateThread(nullptr, 0, CrashThread, nullptr, 0, nullptr);
            WaitForSingleObject(t, INFINITE);
        }
        else if (verb == L"scenario")
        {
            auto bar = arg.find(L'|');
            std::string name = ualtest::utf8(arg.substr(0, bar));
            std::wstring sargs = bar == std::wstring::npos ? L"" : arg.substr(bar + 1);
            auto it = Scenarios().find(name);
            h.currentScenario = name;
            int before = h.failedChecks, beforeTotal = h.totalChecks;
            if (it == Scenarios().end())
                h.Check(false, "scenario exists", "unknown scenario " + name);
            else
                it->second(h, sargs);
            ualtest::Record r;
            r["ev"] = "scenario_done";
            r["failed"] = std::to_string(h.failedChecks - before);
            r["checks"] = std::to_string(h.totalChecks - beforeTotal);
            h.Emit(r);
            h.currentScenario.clear();
        }
        else if (verb == L"exit")
        {
            exitCode = _wtoi(arg.c_str());
            break;
        }
        else
        {
            auto r = Base("unknown_action", a);
            h.Emit(r);
            exitCode = 3;
        }
    }

    ualtest::Record r;
    r["ev"] = "host_exit";
    r["code"] = std::to_string(exitCode);
    h.Emit(r);
    return exitCode;
}
