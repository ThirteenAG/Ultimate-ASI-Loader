// Integration test infrastructure: build outputs, per-test sandbox game directories,
// process launching with dialog automation, and queries over the probe/host event log.
#pragma once

#include "../common/testlib.hpp"
#include "../common/report.hpp"
#include <filesystem>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace runner
{
    struct Arch
    {
        std::string name;   // "Win32" or "x64"
        fs::path bin;       // <repo>/bin/<arch>/<config>
        fs::path tests;     // <bin>/tests

        bool is64() const { return name == "x64"; }
        fs::path loader() const { return bin / L"dinput8.dll"; }
        fs::path genericHost() const { return tests / L"ual_host.exe"; }
        fs::path staticHost(const std::wstring& proxyBase) const { return tests / (L"ual_host_" + proxyBase + L".exe"); }
        fs::path probe() const { return tests / L"probe.asi"; }
        fs::path missingDepProbe() const { return tests / L"probe_missingdep.asi"; }
        fs::path fakeOriginal() const { return tests / L"ual_fake_original.dll"; }
        fs::path stubVorbis() const { return tests / L"stubs" / L"vorbis.dll"; }
        fs::path virtualFileServer() const { return bin.parent_path().parent_path() / L"x64" / bin.filename() / L"VirtualFileServer.exe"; }
        bool available() const;
        std::string missing() const;
    };

    const Arch& GetArch(const std::string& name);
    const Arch& OtherArch(const Arch& a);
    void InitArchs(const std::map<std::string, std::string>& options, const fs::path& runnerExe);

    // Registers "[Win32] name" and "[x64] name"
    struct ArchReg
    {
        ArchReg(const char* name, const char* tags, void (*fn)(const Arch&), const char* only, const char* file, int line);
    };
    void AddArchTest(const std::string& name, const std::string& tags, std::function<void(const Arch&)> fn, const std::string& only = "");

#define UT_ARCH_TEST_IMPL(id, only, name, tags)                                                                  \
    static void UT_CAT(ut_arch_fn_, id)(const ::runner::Arch&);                                                  \
    static ::runner::ArchReg UT_CAT(ut_arch_reg_, id)(name, tags, &UT_CAT(ut_arch_fn_, id), only, __FILE__, __LINE__); \
    static void UT_CAT(ut_arch_fn_, id)([[maybe_unused]] const ::runner::Arch& arch)
#define ARCH_TEST(name, tags) UT_ARCH_TEST_IMPL(__COUNTER__, "", name, tags)
#define WIN32_TEST(name, tags) UT_ARCH_TEST_IMPL(__COUNTER__, "Win32", name, tags)
#define X64_TEST(name, tags) UT_ARCH_TEST_IMPL(__COUNTER__, "x64", name, tags)

    struct DialogInfo
    {
        HWND hwnd = nullptr;
        std::wstring title;
        std::wstring className;
        bool isTaskDialog = false;       // also set for the loader's own window
        bool isModern = false;           // [GlobalSets] ModernUI=1 window
        std::vector<std::wstring> texts; // child window texts and UI Automation names
        double shownAt = 0;              // seconds since process start
        double closedAt = -1;            // -1 if still open at exit
        std::wstring AllText() const;
        bool Contains(std::wstring_view needle) const; // case-insensitive
    };

    struct DialogAction
    {
        enum Kind { Dismiss, ClickButton, Leave } kind = Dismiss;
        int buttonId = 0;
        static DialogAction Click(int id) { return { ClickButton, id }; }
        static DialogAction LeaveOpen() { return { Leave, 0 }; }
    };

    using DialogHandler = std::function<DialogAction(const DialogInfo&)>;

    struct RunResult
    {
        DWORD exitCode = 0;
        bool timedOut = false;
        double seconds = 0;
        std::vector<ualtest::Record> records;
        std::vector<DialogInfo> dialogs;
        bool userInput = false; // someone used keyboard or mouse during the run

        std::vector<const ualtest::Record*> Events(const std::string& src, const std::string& ev) const;
        const ualtest::Record* First(const std::string& src, const std::string& ev) const;
        const ualtest::Record* Last(const std::string& src, const std::string& ev) const;
        bool HostFinished() const { return First("host", "host_exit") != nullptr; }

        // Probe plugin queries, module names are case-insensitive
        int Inits(const std::string& mod = "") const;
        int Attaches(const std::string& mod = "") const;
        std::vector<std::string> InitOrder() const;
        const ualtest::Record* Init(const std::string& mod) const;

        const ualtest::Record* Action(const std::string& ev, const std::string& arg) const;
        std::string Dump(size_t maxLines = 60) const;
    };

    struct RunOptions
    {
        fs::path cwd;                                  // default: game directory
        DWORD timeoutMs = 30000;
        DialogHandler onDialog;                        // default: dismiss and record
        // Post Shift when a dialog appears so the loader's countdown stops and it can't auto-close mid-inspection
        bool freezeCountdown = true;
        std::map<std::wstring, std::wstring> env;
    };

    class Sandbox
    {
    public:
        explicit Sandbox(const Arch& arch, const std::wstring& gameDirName = L"game");
        ~Sandbox();
        Sandbox(const Sandbox&) = delete;
        Sandbox& operator=(const Sandbox&) = delete;

        const Arch& arch;
        fs::path root;   // per-test directory
        fs::path game;   // root/<gameDirName>: host exe, loader, plugins
        fs::path report; // root/report.log

        fs::path P(const fs::path& rel) const { return game / rel; }
        void Mkdir(const fs::path& rel) const;
        void Write(const fs::path& rel, const std::string& content) const;
        std::string Read(const fs::path& rel) const;
        bool Exists(const fs::path& rel) const;
        void Ini(const fs::path& rel, const wchar_t* section, const wchar_t* key, const std::wstring& value) const;
        void Copy(const fs::path& src, const fs::path& rel) const;

        void Loader(const std::wstring& asName = L"dinput8.dll", const fs::path& dir = {}) const;
        // Empty proxyBase deploys ual_host.exe, else ual_host_<proxyBase>.exe
        fs::path Host(const std::wstring& proxyBase = L"", const std::wstring& asName = L"game.exe");
        void Probe(const fs::path& rel, const Arch* fromArch = nullptr) const;
        void Zip(const fs::path& rel, const std::vector<std::pair<std::string, std::string>>& entries, bool store = false) const;

        RunResult Run(const std::vector<std::wstring>& args, RunOptions opts = {}) const;

        fs::path hostExe;
    };

    // Fails on any failed in-process check, turns ev=skip into a test SKIP
    void ExpectHostOk(const RunResult& r, const char* file, int line);
    void ExpectScenarioPassed(const RunResult& r, const std::string& scenario, const char* file, int line);
    // For dialog tests: failures during real user input are reported as skipped, since
    // the person may have operated the dialog.
    void ForgiveUserInterference(const RunResult& r);
#define REQUIRE_HOST_OK(r) ::runner::ExpectHostOk((r), __FILE__, __LINE__)
#define REQUIRE_SCENARIO(r, name) ::runner::ExpectScenarioPassed((r), (name), __FILE__, __LINE__)

    std::string ReadFileBytes(const fs::path& p);
    void WriteFileBytes(const fs::path& p, const std::string& data);
    std::string BuildZip(const std::vector<std::pair<std::string, std::string>>& entries, bool store = false);
    std::wstring Lower(std::wstring s);
    bool IEquals(std::wstring_view a, std::wstring_view b);
    std::wstring PathW(const fs::path& p);
}
