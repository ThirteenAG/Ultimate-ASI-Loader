// Demo plugins from source/plugins, built to bin\<arch>\<config>\scripts
#include "framework.hpp"

using namespace runner;

namespace
{
    // Copies the built plugin and its .ini, if any, to scripts in the sandbox
    void DeployPlugin(Sandbox& sb, const std::wstring& name)
    {
        std::wstring file = name == L"MessageBox" && sb.arch.is64() ? L"MessageBox_x64.asi" : name + L".asi";
        auto dir = sb.arch.bin / L"scripts";
        if (!fs::exists(dir / file)) SKIP("demo plugin not built: " + ut::narrow((dir / file).wstring()));
        sb.Copy(dir / file, fs::path(L"scripts") / file);
        if (fs::exists(dir / (name + L".ini"))) sb.Copy(dir / (name + L".ini"), fs::path(L"scripts") / (name + L".ini"));
    }

    std::string ReadData(const RunResult& r, const std::string& path)
    {
        auto a = r.Action("read", path);
        if (!a || a->get("ok") != "1") return "<read failed>";
        return a->get("data");
    }
}

ARCH_TEST("demo plugin MessageBox shows its message from InitializeASI", "[plugins][ui]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    DeployPlugin(sb, L"MessageBox");
    auto r = sb.Run({ L"snapshot" });
    REQUIRE_HOST_OK(r);
    REQUIRE_EQ(r.dialogs.size(), (size_t)1);
    CHECK_MSG(r.dialogs.front().Contains(L"ASI Loader works correctly."), ut::narrow(r.dialogs.front().AllText()));
    ForgiveUserInterference(r);
}

ARCH_TEST("demo plugin VirtualFiles serves the files of its ini", "[plugins][vfs]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    DeployPlugin(sb, L"VirtualFiles");
    sb.Write(L"original.txt", "ORIGINAL");
    sb.Write(L"replacement.txt", "REPLACED");
    sb.Write(L"scripts/VirtualFiles.ini", "[Paths]\noriginal.txt = replacement.txt\n[Text]\nsettings\\virtual.cfg = \"FROM INI\"\n");
    auto r = sb.Run({ L"read:original.txt", L"read:settings\\virtual.cfg" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(ReadData(r, "original.txt"), std::string("REPLACED"));
    CHECK_EQ(ReadData(r, "settings\\virtual.cfg"), std::string("FROM INI"));
    CHECK_EQ(sb.Read(L"original.txt"), std::string("ORIGINAL")); // nothing changed on disk
}

ARCH_TEST("demo plugin PluginTemplate hooks the window title and logs the loader", "[plugins]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    DeployPlugin(sb, L"PluginTemplate");
    sb.Write(L"scripts/PluginTemplate.ini", "[MAIN]\nTitleSuffix=_PT\nLog=1\n");
    sb.Mkdir(L"update");
    auto r = sb.Run({ L"scenario:window_title|_PT" });
    REQUIRE_HOST_OK(r);
    REQUIRE_SCENARIO(r, "window_title");
    auto log = sb.Read(L"scripts/PluginTemplate.log");
    CHECK_MSG(log.find("Ultimate ASI Loader: ") != std::string::npos, log);
    CHECK_MSG(log.find("update folder: ") != std::string::npos && log.find("\\update") != std::string::npos, log);
    CHECK_MSG(log.find("window title hooks: installed") != std::string::npos, log);
    CHECK_MSG(log.find("started by InitializeASI (Ultimate ASI Loader)") != std::string::npos, log);
}

// Other ASI loaders only call LoadLibrary, so the plugins start from DllMain
// (LoadedByUltimateASILoader in UltimateASILoader.hpp). Here the host loads them itself.
ARCH_TEST("demo plugins start from DllMain when another ASI loader loads them", "[plugins][ui]")
{
    Sandbox sb(arch);
    sb.Host();
    DeployPlugin(sb, L"PluginTemplate");
    DeployPlugin(sb, L"MessageBox");
    sb.Write(L"scripts/PluginTemplate.ini", "[MAIN]\nTitleSuffix=_PT\nLog=1\n");
    std::wstring messageBox = arch.is64() ? L"MessageBox_x64.asi" : L"MessageBox.asi";
    auto r = sb.Run({ L"loadlib:scripts\\PluginTemplate.asi", L"scenario:window_title|_PT", L"loadlib:scripts\\" + messageBox, L"wait:3000" });
    REQUIRE_HOST_OK(r);
    REQUIRE_SCENARIO(r, "window_title");
    auto log = sb.Read(L"scripts/PluginTemplate.log");
    CHECK_MSG(log.find("started by DllMain (another ASI loader)") != std::string::npos, log);
    CHECK_MSG(log.find("not loaded by Ultimate ASI Loader") != std::string::npos, log);
    REQUIRE_EQ(r.dialogs.size(), (size_t)1); // MessageBox from DllMain blocks the game
    CHECK(r.dialogs.front().Contains(L"ASI Loader works correctly."));
    ForgiveUserInterference(r);
}

ARCH_TEST("demo plugin FrameLimiter limits Direct3D 9 and DXGI presents", "[plugins][slow]")
{
    for (const wchar_t* api : { L"d3d9", L"dxgi" })
    {
        INFO(ut::narrow(api));
        Sandbox sb(arch);
        sb.Loader();
        sb.Host(L"dinput8");
        DeployPlugin(sb, L"FrameLimiter");
        sb.Write(L"scripts/FrameLimiter.ini", "[MAIN]\nFPSLimit=40\nApi=all\n");
        auto r = sb.Run({ std::wstring(L"scenario:present_rate|") + api + L"|40|30" });
        REQUIRE_HOST_OK(r);
        REQUIRE_SCENARIO(r, "present_rate");
    }
}
