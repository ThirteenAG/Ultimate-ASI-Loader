// ual_tests.exe [--list] [--filter text] [--tags a,b] [--exclude-tags a,b]
//               [--junit file] [--keep] [--bin dir] [--config Release]
//
// Each test deploys the built loader under a proxy name, a host exe and probe plugins into a
// throw-away game directory under %TEMP%\ual-tests, runs it and checks the event log.
// Only build outputs are used from the repository.
#include "framework.hpp"

int wmain(int argc, wchar_t** argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX); // inherited by the hosts
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    std::wstring self(32768, L'\0');
    self.resize(GetModuleFileNameW(nullptr, self.data(), (DWORD)self.size()));

    std::map<std::string, std::string> layout;
    for (int i = 1; i + 1 < argc; ++i)
    {
        std::wstring a = argv[i];
        if (a == L"--bin" || a == L"--config") layout[ut::narrow(a.substr(2))] = ut::narrow(argv[i + 1]);
    }
    runner::InitArchs(layout, self);

    ut::add_test("[setup] build outputs are present", "[setup]", [] {
        int found = 0;
        for (auto n : { "Win32", "x64" })
        {
            auto& a = runner::GetArch(n);
            INFO(std::string(n) + ": " + ut::narrow(a.bin.wstring()) + (a.available() ? " (ok)" : " (missing: " + a.missing() + ")"));
            found += a.available();
        }
        REQUIRE_MSG(found > 0, "no build outputs found - build the loader and the test projects first, or pass --bin <dir>");
    });
    std::rotate(ut::registry().begin(), ut::registry().end() - 1, ut::registry().end()); // run it first

    int rc = ut::run(argc, argv, "integration", { "bin", "config" });
    CoUninitialize();
    return rc;
}
