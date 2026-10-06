// .cxx snippets: loading, compile errors and crashes reported with their line, CxxHotReload=1
#include "framework.hpp"
#include <thread>

using namespace runner;

namespace
{
    // Snippet code that appends a src=snippet record. The Windows functions it declares
    // are resolved from the DLLs loaded in the game.
    const std::string kReport = R"(
extern "C" {
void* __stdcall CreateFileA(const char* name, unsigned long access, unsigned long share, void* security, unsigned long disposition, unsigned long flags, void* templ);
int __stdcall WriteFile(void* file, const void* data, unsigned long size, unsigned long* written, void* overlapped);
int __stdcall CloseHandle(void* object);
unsigned long __stdcall GetEnvironmentVariableA(const char* name, char* value, unsigned long size);
}
void Report(const char* fields)
{
    char path[600];
    if (!GetEnvironmentVariableA("UAL_TEST_REPORT", path, 600))
        return;
    void* file = CreateFileA(path, 4, 7, nullptr, 4, 0, nullptr); // FILE_APPEND_DATA, share all, OPEN_ALWAYS
    unsigned long length = 0;
    while (fields[length])
        ++length;
    unsigned long written = 0;
    WriteFile(file, "src=snippet\t", 12, &written, nullptr);
    WriteFile(file, fields, length, &written, nullptr);
    WriteFile(file, "\n", 1, &written, nullptr);
    CloseHandle(file);
}
)";

    std::string Snippet(const std::string& body)
    {
        return kReport + body;
    }

    // 1-based line of `marker` in a snippet
    int LineOf(const std::string& text, const std::string& marker)
    {
        size_t at = text.find(marker);
        return at == std::string::npos ? 0 : 1 + (int)std::count(text.begin(), text.begin() + at, '\n');
    }

    std::vector<std::string> Events(const RunResult& r, const std::string& ev)
    {
        std::vector<std::string> out;
        for (auto rec : r.Events("snippet", ev)) out.push_back(rec->get("name"));
        return out;
    }
}

ARCH_TEST("a .cxx snippet next to the plugins is compiled and its Init runs", "[cxx]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Probe(L"scripts/probe.asi");
    sb.Write(L"scripts/hello.cxx", Snippet(R"(
int value = 41;
void Init() { if (++value == 42) Report("ev=init\tname=hello"); }
)"));
    sb.Write(L"scripts/sub/nested.cxx", Snippet(R"(void Init() { Report("ev=init\tname=nested"); })"));
    auto r = sb.Run({ L"trigger:Sleep" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(r.Inits("probe.asi"), 1);
    auto inits = Events(r, "init");
    CHECK_MSG(std::find(inits.begin(), inits.end(), "hello") != inits.end(), "scripts\\hello.cxx: global initialization, then Init");
    CHECK_MSG(std::find(inits.begin(), inits.end(), "nested") != inits.end(), "scripts\\sub\\nested.cxx (LoadRecursively)");
    CHECK_EQ(r.dialogs.size(), (size_t)0);
}

ARCH_TEST("a .cxx compile error is reported with its file and line; other snippets still load", "[cxx][errors][ui]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Write(L"scripts/a_bad.cxx", "int f()\n{\n    return undefined_name;\n}\n");
    sb.Write(L"scripts/b_good.cxx", Snippet(R"(void Init() { Report("ev=init\tname=good"); })"));
    auto r = sb.Run({ L"trigger:Sleep" });
    REQUIRE_HOST_OK(r);
    REQUIRE_EQ(r.dialogs.size(), (size_t)1);
    auto& d = r.dialogs.front();
    CHECK_MSG(d.Contains(L"Unable to load a_bad.cxx"), ut::narrow(d.AllText()));
    CHECK_MSG(d.Contains(L"a_bad.cxx(3,"), "the error names file, line and column: " + ut::narrow(d.AllText()));
    CHECK_MSG(d.Contains(L"undefined_name"), ut::narrow(d.AllText()));
    CHECK_EQ(Events(r, "init"), std::vector<std::string>{ "good" });
}

ARCH_TEST("a crash in a snippet's Init is reported with its line, the snippet is unloaded and its writes undone", "[cxx][errors][ui]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    std::string text = R"(#include <injector/injector.hpp>
extern "C" int g_ualSnippetTarget[4];
void Init()
{
    injector::WriteMemory<int>(&g_ualSnippetTarget[0], 100, true);
    int* p = nullptr;
    *p = 1; // crash
}
)";
    sb.Write(L"scripts/crash.cxx", text);
    auto r = sb.Run({ L"trigger:Sleep", L"target" });
    REQUIRE_HOST_OK(r);
    REQUIRE_EQ(r.dialogs.size(), (size_t)1);
    auto& d = r.dialogs.front();
    CHECK_MSG(d.Contains(L"crash.cxx crashed in Init"), ut::narrow(d.AllText()));
    CHECK_MSG(d.Contains(L"crash.cxx line " + std::to_wstring(LineOf(text, "*p = 1"))), "the line that crashed: " + ut::narrow(d.AllText()));
    CHECK_MSG(d.Contains(L"0xC0000005"), ut::narrow(d.AllText()));
    CHECK_EQ(r.Action("target", "")->get("value"), std::string("1,2,3,4"));
}

ARCH_TEST("a crash in snippet code on another thread is logged and named in the crash report", "[cxx][crashdumps]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Mkdir(L"CrashDumps");
    std::string text = R"(#include <windows.h>
DWORD __stdcall Worker(void*)
{
    Sleep(100);
    int* p = nullptr;
    return *p; // crash
}
void Init() { CreateThread(nullptr, 0, Worker, nullptr, 0, nullptr); }
)";
    sb.Write(L"scripts/later.cxx", text);
    RunOptions o;
    o.timeoutMs = 20000;
    auto r = sb.Run({ L"trigger:Sleep", L"wait:5000" }, o);
    CHECK_MSG(r.exitCode != 0, "the crash ends the game");
    int line = LineOf(text, "return *p");
    std::string expect = "scripts\\later.cxx line " + std::to_string(line) + ", in Worker";
    REQUIRE(sb.Exists(L"dinput8.log"));
    auto log = sb.Read(L"dinput8.log");
    CHECK_MSG(log.find("exception 0xC0000005 in snippet code: " + expect) != std::string::npos, log);
    std::string report;
    std::error_code ec;
    for (auto& e : fs::directory_iterator(sb.P(L"CrashDumps"), ec))
        if (IEquals(e.path().extension().wstring(), L".log")) report = ReadFileBytes(e.path());
    REQUIRE_MSG(!report.empty(), "a crash report was written");
    CHECK_MSG(report.find("Snippet:   " + expect) != std::string::npos, report.substr(0, 2000));
}

ARCH_TEST("CxxHotReload=1: an edited snippet replaces the running one; a broken edit keeps it", "[cxx][reload]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Ini(L"global.ini", L"GlobalSets", L"CxxHotReload", L"1");
    auto version = [](int v, int index, int value) {
        return Snippet(R"(#include <injector/injector.hpp>
extern "C" int g_ualSnippetTarget[4];
void Init()
{
    injector::WriteMemory<int>(&g_ualSnippetTarget[)" + std::to_string(index) + "], " + std::to_string(value) + R"(, true);
    Report("ev=init\tname=v)" + std::to_string(v) + R"(");
}
void Shutdown() { Report("ev=shutdown\tname=v)" + std::to_string(v) + R"("); }
)");
    };
    sb.Write(L"scripts/live.cxx", version(1, 1, 21));
    // while the game runs: a broken edit, then a working one
    std::thread editor([&] {
        Sleep(2000);
        sb.Write(L"scripts/live.cxx", "void Init() { this does not compile }\n");
        Sleep(2000);
        sb.Write(L"scripts/live.cxx", version(3, 2, 33));
    });
    RunOptions o;
    o.timeoutMs = 30000;
    auto r = sb.Run({ L"trigger:Sleep", L"wait:1500", L"target", L"wait:2000", L"target", L"wait:2500", L"target" }, o);
    editor.join();
    REQUIRE_HOST_OK(r);
    auto targets = r.Events("host", "target");
    REQUIRE_EQ(targets.size(), (size_t)3);
    CHECK_EQ(targets[0]->get("value"), std::string("1,21,3,4"));  // version 1
    CHECK_EQ(targets[1]->get("value"), std::string("1,21,3,4"));  // broken edit keeps version 1
    CHECK_EQ(targets[2]->get("value"), std::string("1,2,33,4"));  // version 1 undone, version 3 applied
    CHECK_EQ(Events(r, "init"), (std::vector<std::string>{ "v1", "v3" }));
    CHECK_EQ(Events(r, "shutdown"), std::vector<std::string>{ "v1" });
    bool reloadError = false;
    for (auto& d : r.dialogs) reloadError |= d.Contains(L"Unable to reload live.cxx") && d.Contains(L"keeps running");
    CHECK_MSG(reloadError, "the broken edit is reported");
}
