// Plugin loading: proxy names, search locations, INI options, load triggers, failures
// and process-level features.
#include "framework.hpp"
#include "proxies.hpp"
#include "../common/sentinels.hpp"
#include <miniz.h>
#include <chrono>

using namespace runner;

namespace
{
    std::wstring Base(const std::wstring& file)
    {
        return Lower(file.substr(0, file.rfind(L'.')));
    }

    void ExpectNoDialogs(const RunResult& r)
    {
        CHECK_MSG(r.dialogs.empty(), "unexpected dialog: " + (r.dialogs.empty() ? std::string() : ut::narrow(r.dialogs.front().AllText())));
    }

    // Files a real game ships next to the proxy
    void DeployProxyDependencies(const Sandbox& sb, const std::wstring& proxy)
    {
        if (IEquals(proxy, L"vorbisFile.dll"))
            sb.Copy(sb.arch.stubVorbis(), L"vorbis.dll"); // the game's vorbis.dll, used by the built-in vorbisfile
    }

    void DeployPluginTree(Sandbox& sb)
    {
        sb.Probe(L"root.asi");
        sb.Probe(L"scripts/s.asi");
        sb.Probe(L"scripts/sub/s_sub.asi");
        sb.Probe(L"scripts/sub/deeper/s_deep.asi");
        sb.Probe(L"plugins/p.asi");
        sb.Probe(L"plugins/sub/p_sub.asi");
        sb.Probe(L"update/u.asi");
    }

    // --- proxy matrix

    struct ProxyRegistrar
    {
        ProxyRegistrar()
        {
            for (auto& p : kProxies)
            {
                std::string name = ut::narrow(p.file);
                std::string only = p.win32 && p.x64 ? "" : p.win32 ? "Win32" : "x64";

                if (p.staticHost)
                    AddArchTest("proxy " + name + ": plugins load before the entry point (static import)", "[loading][proxy][static]", [p](const Arch& arch) {
                        Sandbox sb(arch);
                        sb.Loader(p.file);
                        DeployProxyDependencies(sb, p.file);
                        sb.Host(Base(p.file));
                        sb.Probe(L"scripts/probe.asi");
                        auto r = sb.Run({ L"snapshot" });
                        REQUIRE_HOST_OK(r);
                        CHECK_EQ(r.First("host", "host_main")->get("ual"), std::string("1"));
                        REQUIRE_MSG(r.Inits("probe.asi") == 1, r.Dump());
                        auto init = r.Init("probe.asi");
                        CHECK_MSG(init->get("hostmain") == "0", "InitializeASI should run before the executable's main()");
                        CHECK_EQ(init->get("ualcount"), std::string("1"));
                        CHECK_MSG(init->get("loaderlock") == "0", "with the default DontLoadFromDllMain=1 plugins must not run under the loader lock");
                        CHECK_EQ(r.Attaches("probe.asi"), 1);
                        ExpectNoDialogs(r);
                    }, only);

                AddArchTest("proxy " + name + ": plugins load when the DLL is loaded at run time", "[loading][proxy][dynamic]", [p](const Arch& arch) {
                    Sandbox sb(arch);
                    sb.Loader(p.file);
                    DeployProxyDependencies(sb, p.file);
                    sb.Host();
                    sb.Probe(L"scripts/probe.asi");
                    auto r = sb.Run({ L"snapshot", L"load:" + std::wstring(p.file), L"trigger:Sleep", L"snapshot" });
                    REQUIRE_HOST_OK(r);
                    auto load = r.Action("load", ut::narrow(p.file));
                    REQUIRE(load);
                    REQUIRE_MSG(load->get("ok") == "1", "LoadLibrary failed: " + load->get("err"));
                    CHECK_MSG(IEquals(fs::path(ut::widen(load->get("module"))).wstring(), sb.P(p.file).wstring()), "the local loader copy must be loaded, got " + load->get("module"));
                    auto snaps = r.Events("host", "snapshot");
                    REQUIRE_EQ(snaps.size(), (size_t)2);
                    CHECK_EQ(snaps[0]->get("probes"), std::string());
                    CHECK_EQ(snaps[1]->get("probes"), std::string("probe.asi:1"));
                    CHECK_EQ(r.Inits("probe.asi"), 1);
                    ExpectNoDialogs(r);
                }, only);

                if (p.forward)
                    AddArchTest("proxy " + name + ": exports forward to the original system DLL", "[loading][proxy][forwarding]", [p](const Arch& arch) {
                        Sandbox sb(arch);
                        sb.Loader(p.file);
                        DeployProxyDependencies(sb, p.file);
                        sb.Host();
                        auto r = sb.Run({ L"load:" + std::wstring(p.file), L"trigger:Sleep", L"scenario:forward|" + std::wstring(p.file) });
                        REQUIRE_SCENARIO(r, "forward");
                    }, only);
            }
        }
    } g_proxyRegistrar;
}

// --- search locations

ARCH_TEST("plugins are loaded from the root, scripts, plugins and update folders and one level of sub folders", "[loading][search]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    DeployPluginTree(sb);
    auto r = sb.Run({});
    REQUIRE_HOST_OK(r);
    for (auto m : { "root.asi", "s.asi", "s_sub.asi", "p.asi", "p_sub.asi", "u.asi" })
        CHECK_MSG(r.Inits(m) == 1, std::string(m) + " should be initialized exactly once");
    CHECK_MSG(r.Attaches("s_deep.asi") == 0, "only one level of sub folders is searched");
    ExpectNoDialogs(r);
}

ARCH_TEST("plugins are loaded in a fixed order: root, scripts sub folders, scripts, plugins sub folders, plugins, update", "[loading][search]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    DeployPluginTree(sb);
    auto r = sb.Run({});
    REQUIRE_HOST_OK(r);
    std::vector<std::string> expected = { "root.asi", "s_sub.asi", "s.asi", "p_sub.asi", "p.asi", "u.asi" };
    auto order = r.InitOrder();
    std::string got;
    for (auto& o : order) got += o + " ";
    CHECK_MSG(order == expected, "init order: " + got);
}

ARCH_TEST("LoadRecursively=0 skips sub folders of scripts and plugins", "[loading][search][ini]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    DeployPluginTree(sb);
    sb.Ini(L"global.ini", L"GlobalSets", L"LoadRecursively", L"0");
    auto r = sb.Run({});
    REQUIRE_HOST_OK(r);
    CHECK_EQ(r.Inits("s.asi"), 1);
    CHECK_EQ(r.Inits("p.asi"), 1);
    CHECK_EQ(r.Attaches("s_sub.asi"), 0);
    CHECK_EQ(r.Attaches("p_sub.asi"), 0);
}

ARCH_TEST("LoadFromScriptsOnly=1 skips plugins in the root folder", "[loading][search][ini]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    DeployPluginTree(sb);
    sb.Ini(L"scripts/global.ini", L"GlobalSets", L"LoadFromScriptsOnly", L"1");
    auto r = sb.Run({});
    REQUIRE_HOST_OK(r);
    CHECK_EQ(r.Attaches("root.asi"), 0);
    CHECK_EQ(r.Inits("s.asi"), 1);
    CHECK_EQ(r.Inits("p.asi"), 1);
}

ARCH_TEST("LoadExtraPlugins: a plugin listed there and found in scripts initializes once", "[loading][search][ini]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Probe(L"scripts/probe.asi");
    sb.Ini(L"global.ini", L"GlobalSets", L"LoadExtraPlugins", L"scripts\\probe.asi");
    auto r = sb.Run({});
    REQUIRE_HOST_OK(r);
    CHECK_EQ(r.Attaches("probe.asi"), 1);
    CHECK_EQ(r.Inits("probe.asi"), 1);
}

ARCH_TEST("LoadPlugins=0 disables plugin loading", "[loading][ini]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    DeployPluginTree(sb);
    sb.Ini(L"global.ini", L"GlobalSets", L"LoadPlugins", L"0");
    auto r = sb.Run({});
    REQUIRE_HOST_OK(r);
    CHECK_EQ(r.Attaches(), 0);
}

ARCH_TEST("only files with the .asi extension (any case) are loaded", "[loading][search]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Probe(L"UPPER.ASI");
    sb.Probe(L"Mixed.AsI");
    sb.Probe(L"backup.asi.bak");
    sb.Probe(L"longext.asix");
    sb.Probe(L"library.dll");
    sb.Probe(L"scripts/noext");
    sb.Mkdir(L"folder.asi");
    sb.Probe(L"folder.asi/inner.asi");
    auto r = sb.Run({});
    REQUIRE_HOST_OK(r);
    CHECK_EQ(r.Inits("UPPER.ASI"), 1);
    CHECK_EQ(r.Inits("Mixed.AsI"), 1);
    CHECK_EQ(r.Attaches("backup.asi.bak"), 0);
    CHECK_EQ(r.Attaches("longext.asix"), 0);
    CHECK_EQ(r.Attaches("library.dll"), 0);
    CHECK_EQ(r.Attaches("noext"), 0);
    CHECK_EQ(r.Attaches("inner.asi"), 0);
    ExpectNoDialogs(r);
}

ARCH_TEST("each plugin gets DllMain before InitializeASI and InitializeASI exactly once", "[loading]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Probe(L"a.asi");
    sb.Probe(L"scripts/b.asi");
    sb.Probe(L"plugins/c.asi");
    // More trigger calls must not re-run the loader
    auto r = sb.Run({ L"trigger:Sleep", L"trigger:GetSystemInfo", L"trigger:CreateEventW", L"trigger:GetSystemTimeAsFileTime" });
    REQUIRE_HOST_OK(r);
    for (auto m : { "a.asi", "b.asi", "c.asi" })
    {
        CHECK_EQ(r.Attaches(m), 1);
        CHECK_EQ(r.Inits(m), 1);
        size_t attachIdx = SIZE_MAX, initIdx = SIZE_MAX;
        for (size_t i = 0; i < r.records.size(); ++i)
        {
            if (r.records[i].get("src") != "probe" || _stricmp(r.records[i].get("mod").c_str(), m) != 0) continue;
            if (r.records[i].get("ev") == "attach" && attachIdx == SIZE_MAX) attachIdx = i;
            if (r.records[i].get("ev") == "init" && initIdx == SIZE_MAX) initIdx = i;
        }
        CHECK_MSG(attachIdx < initIdx, std::string(m) + ": DllMain must run before InitializeASI");
    }
}

// --- ini files

namespace
{
    struct IniCase
    {
        const wchar_t* loaderName;
        std::vector<std::pair<const wchar_t*, const wchar_t*>> files; // ini path, LoadPlugins value
        bool expectLoaded;
        const char* title;
    };

    struct IniRegistrar
    {
        IniRegistrar()
        {
            std::vector<IniCase> cases = {
                { L"dinput8.dll", { { L"dinput8.ini", L"0" } }, false, "<loader name>.ini is read" },
                { L"dinput8.dll", { { L"global.ini", L"0" } }, false, "global.ini next to the loader is read" },
                { L"dinput8.dll", { { L"scripts\\global.ini", L"0" } }, false, "scripts\\global.ini is read" },
                { L"dinput8.dll", { { L"plugins\\global.ini", L"0" } }, false, "plugins\\global.ini is read" },
                { L"dinput8.dll", { { L"update\\global.ini", L"0" } }, false, "update\\global.ini is read" },
                { L"version.dll", { { L"version.ini", L"0" } }, false, "the ini is named after the proxy name in use (version.ini)" },
                { L"version.dll", { { L"dinput8.ini", L"0" } }, true, "an ini named after another proxy is ignored" },
                { L"dinput8.dll", { { L"dinput8.ini", L"0" }, { L"scripts\\global.ini", L"1" } }, true, "scripts\\global.ini overrides <loader name>.ini" },
                { L"dinput8.dll", { { L"global.ini", L"1" }, { L"plugins\\global.ini", L"0" } }, false, "plugins\\global.ini overrides global.ini" },
                { L"dinput8.dll", { { L"dinput8.ini", L"1" }, { L"update\\global.ini", L"0" } }, false, "update\\global.ini has the highest precedence" },
            };
            for (auto& c : cases)
                AddArchTest(std::string("ini precedence: ") + c.title, "[loading][ini]", [c](const Arch& arch) {
                    Sandbox sb(arch);
                    sb.Loader(c.loaderName);
                    sb.Host(Base(c.loaderName));
                    sb.Probe(L"scripts/probe.asi");
                    for (auto& [file, value] : c.files)
                        sb.Ini(file, L"GlobalSets", L"LoadPlugins", value);
                    auto r = sb.Run({});
                    REQUIRE_HOST_OK(r);
                    CHECK_EQ(r.Inits("probe.asi"), c.expectLoaded ? 1 : 0);
                });
        }
    } g_iniRegistrar;
}

// --- failures

ARCH_TEST("a plugin whose DllMain fails and a plugin for the other architecture are skipped silently", "[loading][errors]")
{
    if (!OtherArch(arch).available()) SKIP("binaries for the other architecture are not built");
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Probe(L"a_failinit.asi");
    sb.Probe(L"b_good.asi");
    sb.Probe(L"c_wrongarch.asi", &OtherArch(arch));
    sb.Probe(L"scripts/d_good.asi");
    auto r = sb.Run({});
    REQUIRE_HOST_OK(r);
    CHECK_EQ(r.Attaches("a_failinit.asi"), 1);
    CHECK_EQ(r.Inits("a_failinit.asi"), 0);
    CHECK_EQ(r.Attaches("c_wrongarch.asi"), 0);
    CHECK_EQ(r.Inits("b_good.asi"), 1);
    CHECK_EQ(r.Inits("d_good.asi"), 1);
    ExpectNoDialogs(r);
}

ARCH_TEST("a plugin with a missing dependency shows an error dialog and loading continues", "[loading][errors][ui]")
{
    for (bool modern : { false, true }) // task dialog, ModernUI=1
    {
        INFO(modern ? "ModernUI=1" : "task dialog");
        Sandbox sb(arch);
        sb.Loader();
        sb.Host(L"dinput8");
        sb.Ini(L"global.ini", L"GlobalSets", L"ModernUI", modern ? L"1" : L"0");
        sb.Copy(arch.missingDepProbe(), L"scripts/a_missingdep.asi");
        sb.Probe(L"scripts/z_good.asi");
        auto r = sb.Run({});
        REQUIRE_HOST_OK(r);
        REQUIRE_EQ(r.dialogs.size(), (size_t)1);
        auto& d = r.dialogs.front();
        CHECK_EQ(d.isModern, modern);
        CHECK(d.title == L"ASI Loader");
        CHECK_MSG(d.Contains(L"Unable to load a_missingdep.asi"), ut::narrow(d.AllText()));
        CHECK_MSG(d.Contains(L"126"), "the dialog shows the Win32 error code");
        CHECK_MSG(d.Contains(L"Dependencies"), "ERROR_MOD_NOT_FOUND explains how to find the missing dependency");
        CHECK_EQ(r.Inits("z_good.asi"), 1);
    }
}

ARCH_TEST("plugin errors under the loader lock (DontLoadFromDllMain=0) are shown once it is released", "[loading][errors][ui][modern]")
{
    // Shown from a separate thread, no window is safe under the loader lock
    for (bool modern : { false, true })
    {
        INFO(modern ? "ModernUI=1" : "task dialog");
        Sandbox sb(arch);
        sb.Loader();
        sb.Host(L"dinput8");
        sb.Ini(L"global.ini", L"GlobalSets", L"ModernUI", modern ? L"1" : L"0");
        sb.Ini(L"global.ini", L"GlobalSets", L"DontLoadFromDllMain", L"0");
        sb.Copy(arch.missingDepProbe(), L"scripts/a_missingdep.asi");
        sb.Probe(L"scripts/z_good.asi");
        auto r = sb.Run({ L"wait:3000" });
        REQUIRE_HOST_OK(r);
        REQUIRE_EQ(r.dialogs.size(), (size_t)1);
        CHECK_EQ(r.dialogs.front().isModern, modern);
        CHECK_MSG(r.dialogs.front().Contains(L"Unable to load a_missingdep.asi"), ut::narrow(r.dialogs.front().AllText()));
        CHECK_EQ(r.Inits("z_good.asi"), 1);
    }
}

ARCH_TEST("a plugin that changes the current directory does not break loading of the next plugins", "[loading][cwd]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Probe(L"scripts/a_chdir.asi");
    sb.Probe(L"scripts/b.asi");
    sb.Probe(L"plugins/c.asi");
    auto r = sb.Run({ L"snapshot" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(r.Inits("b.asi"), 1);
    CHECK_EQ(r.Inits("c.asi"), 1);
    CHECK_MSG(IEquals(ut::widen(r.First("host", "host_main")->get("cwd")), sb.game.wstring()), "the game's current directory must be restored");
}

ARCH_TEST("plugins load when the game is started from another working directory, which is preserved", "[loading][cwd]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Probe(L"root.asi");
    sb.Probe(L"scripts/s.asi");
    sb.Probe(L"plugins/p.asi");
    RunOptions o;
    o.cwd = sb.root;
    auto r = sb.Run({}, o);
    REQUIRE_HOST_OK(r);
    CHECK_EQ(r.Inits("root.asi"), 1);
    CHECK_EQ(r.Inits("s.asi"), 1);
    CHECK_EQ(r.Inits("p.asi"), 1);
    CHECK_MSG(IEquals(ut::widen(r.First("host", "host_main")->get("cwd")), sb.root.wstring()), "current directory: " + r.First("host", "host_main")->get("cwd"));
}

ARCH_TEST("LoadFromScriptsOnly=1 still loads the scripts folder when the working directory differs", "[loading][cwd]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Probe(L"scripts/s.asi");
    sb.Probe(L"plugins/p.asi");
    sb.Ini(L"global.ini", L"GlobalSets", L"LoadFromScriptsOnly", L"1");
    RunOptions o;
    o.cwd = sb.root;
    auto r = sb.Run({}, o);
    REQUIRE_HOST_OK(r);
    CHECK_EQ(r.Inits("p.asi"), 1);
    CHECK_MSG(r.Inits("s.asi") == 1, "scripts\\s.asi was not loaded: FindPlugins(L\"scripts\") resolves the relative path against the game's working directory");
}

ARCH_TEST("plugins load from a game directory with spaces and non-ANSI characters", "[loading][unicode]")
{
    Sandbox sb(arch, L"Gämé 測試 ゲーム dir");
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Probe(L"über.asi");
    sb.Probe(L"scripts/插件.asi");
    sb.Probe(L"plugins/p.asi");
    auto r = sb.Run({});
    REQUIRE_HOST_OK(r);
    CHECK_EQ(r.Inits(ut::narrow(L"über.asi")), 1);
    CHECK_EQ(r.Inits(ut::narrow(L"插件.asi")), 1);
    CHECK_EQ(r.Inits("p.asi"), 1);
}

// --- LoadExtraPlugins

ARCH_TEST("LoadExtraPlugins loads every listed DLL relative to the loader, ignoring duplicates and missing files", "[loading][ini][extra]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Probe(L"extra/a.dll");
    sb.Probe(L"extra dir/b.dll");
    sb.Ini(L"global.ini", L"GlobalSets", L"LoadExtraPlugins", L"extra\\a.dll | \"extra dir\\b.dll\" | missing\\nope.dll |extra\\a.dll");
    auto r = sb.Run({});
    REQUIRE_HOST_OK(r);
    CHECK_EQ(r.Attaches("a.dll"), 1);
    CHECK_EQ(r.Attaches("b.dll"), 1);
    ExpectNoDialogs(r);
}

ARCH_TEST("modloader\\modloader.asi is loaded by default", "[loading][extra]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Probe(L"modloader/modloader.asi");
    auto r = sb.Run({});
    REQUIRE_HOST_OK(r);
    CHECK_EQ(r.Attaches("modloader.asi"), 1);
}

ARCH_TEST("plugins listed in LoadExtraPlugins get InitializeASI like every other plugin", "[loading][extra]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Probe(L"extra/a.asi");
    sb.Ini(L"global.ini", L"GlobalSets", L"LoadExtraPlugins", L"extra\\a.asi");
    auto r = sb.Run({});
    REQUIRE_HOST_OK(r);
    REQUIRE_EQ(r.Attaches("a.asi"), 1);
    CHECK_MSG(r.Inits("a.asi") == 1, "InitializeASI was never called for a plugin loaded through LoadExtraPlugins");
}

ARCH_TEST("INI string values longer than MAX_PATH are not truncated", "[loading][ini][extra]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    std::wstring list;
    std::vector<std::wstring> names;
    for (int i = 0; i < 12; ++i)
    {
        auto n = L"extra_plugin_with_a_rather_long_name_" + std::to_wstring(i) + L".dll";
        names.push_back(n);
        sb.Probe(L"extra\\" + n);
        list += (i ? L" | " : L"") + std::wstring(L"extra\\") + n;
    }
    INFO("LoadExtraPlugins value length: " + std::to_string(list.size()));
    sb.Ini(L"global.ini", L"GlobalSets", L"LoadExtraPlugins", list);
    auto r = sb.Run({});
    REQUIRE_HOST_OK(r);
    std::string missing;
    for (auto& n : names)
        if (r.Attaches(ut::narrow(n)) != 1) missing += ut::narrow(n) + " ";
    CHECK_MSG(missing.empty(), "not loaded: " + missing);
}

// --- load triggers

ARCH_TEST("DontLoadFromDllMain=0 initializes plugins from the loader's DllMain", "[loading][ini][trigger]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Probe(L"scripts/probe.asi");
    sb.Ini(L"global.ini", L"GlobalSets", L"DontLoadFromDllMain", L"0");
    auto r = sb.Run({});
    REQUIRE_HOST_OK(r);
    REQUIRE_EQ(r.Inits("probe.asi"), 1);
    CHECK_EQ(r.Init("probe.asi")->get("loaderlock"), std::string("1"));
    CHECK_EQ(r.Init("probe.asi")->get("hostmain"), std::string("0"));
}

ARCH_TEST("LoadFromAPI delays plugin loading until the executable calls that API", "[loading][ini][trigger]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Probe(L"scripts/probe.asi");
    sb.Ini(L"global.ini", L"GlobalSets", L"LoadFromAPI", L"Sleep");
    auto r = sb.Run({ L"snapshot", L"trigger:GetSystemInfo", L"trigger:CreateEventW", L"snapshot", L"trigger:Sleep", L"snapshot" });
    REQUIRE_HOST_OK(r);
    auto snaps = r.Events("host", "snapshot");
    REQUIRE_EQ(snaps.size(), (size_t)3);
    CHECK_EQ(snaps[0]->get("probes"), std::string());
    CHECK_EQ(snaps[1]->get("probes"), std::string());
    CHECK_EQ(snaps[2]->get("probes"), std::string("probe.asi:1"));
}

ARCH_TEST("LoadFromAPI naming an API the game never calls means plugins are never loaded", "[loading][ini][trigger]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Probe(L"scripts/probe.asi");
    sb.Ini(L"global.ini", L"GlobalSets", L"LoadFromAPI", L"NoSuchApiIsEverCalled");
    auto r = sb.Run({ L"trigger:Sleep", L"trigger:GetSystemInfo", L"snapshot" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(r.Inits(), 0);
}

ARCH_TEST("games with a GTA V / RDR 2 executable name still load plugins", "[loading][trigger]")
{
    for (auto exe : { L"GTA5.exe", L"RDR2.exe" })
    {
        Sandbox sb(arch);
        sb.Loader();
        sb.Host(L"dinput8", exe);
        sb.Probe(L"scripts/probe.asi");
        auto r = sb.Run({ L"trigger:GetSystemTimeAsFileTime", L"snapshot" });
        REQUIRE_HOST_OK(r);
        CHECK_MSG(r.Inits("probe.asi") == 1, ut::narrow(exe));
    }
}

ARCH_TEST("two loader copies in one process initialize plugins only once", "[loading][multi]")
{
    Sandbox sb(arch);
    sb.Loader(L"dinput8.dll");
    sb.Loader(L"version.dll");
    sb.Host();
    sb.Probe(L"scripts/probe.asi");
    auto r = sb.Run({ L"load:dinput8.dll", L"load:version.dll", L"trigger:Sleep", L"snapshot" });
    REQUIRE_HOST_OK(r);
    // a system DLL may load "VERSION.dll" first
    CHECK_EQ(Lower(ut::widen(r.Last("host", "snapshot")->get("uals"))), std::wstring(L"dinput8.dll;version.dll"));
    CHECK_EQ(r.Inits("probe.asi"), 1);
    ExpectNoDialogs(r);
}

// games that free the loader again (GTA 2 probing ddraw.dll, Manhunt) unmapped its
// window mode, VFS and crash report hooks, and the next hooked call crashed without a report.
ARCH_TEST("the loader stays mapped when the game frees it after loading the plugins", "[loading]")
{
    Sandbox sb(arch);
    sb.Loader(L"dinput8.dll");
    sb.Host();
    sb.Probe(L"scripts/probe.asi");
    auto r = sb.Run({ L"load:dinput8.dll", L"trigger:Sleep", L"free:dinput8.dll", L"trigger:Sleep", L"snapshot" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(r.Inits("probe.asi"), 1);
    auto f = r.Last("host", "free");
    REQUIRE(f != nullptr);
    CHECK_MSG(f->get("mapped") == "1", "the loader was unmapped by FreeLibrary after it loaded the plugins");
}

ARCH_TEST("an unsupported DLL name shows an error and terminates the process", "[loading][errors][ui]")
{
    Sandbox sb(arch);
    sb.Loader(L"unsupported.dll");
    sb.Host();
    sb.Probe(L"scripts/probe.asi");
    auto r = sb.Run({ L"load:unsupported.dll", L"trigger:Sleep", L"snapshot" });
    REQUIRE_EQ(r.dialogs.size(), (size_t)1);
    CHECK(r.dialogs.front().Contains(L"This library isn't supported."));
    CHECK_MSG(!r.HostFinished(), "the process must be terminated after the message");
    CHECK_EQ(r.exitCode, (DWORD)0);
    CHECK_EQ(r.Inits(), 0);
}

// --- original library chaining

ARCH_TEST("a local <name>Hooked.dll is used as the original library instead of System32", "[loading][chaining]")
{
    for (auto name : { L"dinput8.dll", L"version.dll", L"winmm.dll" })
    {
        Sandbox sb(arch);
        std::wstring base = std::wstring(name).substr(0, wcslen(name) - 4);
        sb.Loader(name);
        sb.Copy(arch.fakeOriginal(), base + L"Hooked.dll");
        sb.Host();
        auto r = sb.Run({ L"load:" + std::wstring(name), L"trigger:Sleep", L"scenario:hooked_sentinel|" + std::wstring(name) });
        REQUIRE_SCENARIO(r, "hooked_sentinel");
    }
}

WIN32_TEST("vorbisFile.dll uses its built-in vorbisfile without a local original", "[loading][chaining][x86]")
{
    for (bool withVorbisDll : { true, false }) // game's vorbis.dll if present, else built-in libvorbis
    {
        INFO(withVorbisDll ? "with vorbis.dll" : "without vorbis.dll");
        Sandbox sb(arch);
        sb.Loader(L"vorbisFile.dll");
        if (withVorbisDll) DeployProxyDependencies(sb, L"vorbisFile.dll");
        sb.Host();
        sb.Probe(L"scripts/probe.asi");
        auto r = sb.Run({ L"load:vorbisFile.dll", L"trigger:Sleep", L"scenario:vorbisfile_builtin" });
        REQUIRE_SCENARIO(r, "vorbisfile_builtin");
        CHECK_EQ(r.Inits("probe.asi"), 1);
    }
}

WIN32_TEST("vorbisFile.dll does not crash the game when vorbis.dll is missing", "[loading][chaining][x86][errors]")
{
    // The old embedded vorbisfile.dll needed vorbis.dll, and without it the loader passed NULL
    // to MemoryGetProcAddress. The built-in vorbisfile falls back to its own libvorbis.
    Sandbox sb(arch);
    sb.Loader(L"vorbisFile.dll");
    sb.Host();
    sb.Probe(L"scripts/probe.asi");
    auto r = sb.Run({ L"load:vorbisFile.dll", L"trigger:Sleep", L"scenario:vorbisfile_builtin" });
    CHECK_MSG(r.exitCode != EXCEPTION_ACCESS_VIOLATION, "the game crashed with an access violation");
    REQUIRE_SCENARIO(r, "vorbisfile_builtin");
    CHECK_EQ(r.Inits("probe.asi"), 1);
}

WIN32_TEST("binkw32.dll without a local binkw32Hooked.dll still loads the plugins", "[loading][chaining][x86]")
{
    Sandbox sb(arch);
    sb.Loader(L"binkw32.dll");
    sb.Host();
    sb.Probe(L"scripts/probe.asi");
    auto r = sb.Run({ L"load:binkw32.dll", L"trigger:Sleep" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(r.Inits("probe.asi"), 1);
}

WIN32_TEST("vorbisFile.dll forwards to a local vorbisHooked.dll", "[loading][chaining][x86]")
{
    Sandbox sb(arch);
    sb.Loader(L"vorbisFile.dll");
    sb.Copy(arch.fakeOriginal(), L"vorbisHooked.dll");
    sb.Host();
    auto r = sb.Run({ L"load:vorbisFile.dll", L"trigger:Sleep", L"scenario:hooked_sentinel|vorbisFile.dll" });
    REQUIRE_SCENARIO(r, "hooked_sentinel");
}

WIN32_TEST("vorbisFile.dll honours vorbisFileHooked.dll as documented (<dllname>Hooked.dll)", "[loading][chaining][x86]")
{
    Sandbox sb(arch);
    sb.Loader(L"vorbisFile.dll");
    DeployProxyDependencies(sb, L"vorbisFile.dll");
    sb.Copy(arch.fakeOriginal(), L"vorbisFileHooked.dll");
    sb.Host();
    auto r = sb.Run({ L"load:vorbisFile.dll", L"trigger:Sleep", L"scenario:hooked_sentinel|vorbisFile.dll" });
    REQUIRE_SCENARIO(r, "hooked_sentinel");
}

// --- x86 extras

WIN32_TEST("an empty wndmode.ini is filled with the default configuration", "[x86][wndmode]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Write(L"wndmode.ini", "");
    sb.Probe(L"scripts/probe.asi");
    auto r = sb.Run({});
    REQUIRE_HOST_OK(r);
    auto ini = sb.Read(L"wndmode.ini");
    CHECK_MSG(ini.find("[WINDOWMODE]") != std::string::npos, "wndmode.ini content: " + ini.substr(0, 100));
    CHECK_EQ(r.Inits("probe.asi"), 1);
}

WIN32_TEST("an existing wndmode.ini is left untouched and no wndmode.ini is created otherwise", "[x86][wndmode]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Write(L"wndmode.ini", "[WINDOWMODE]\r\nUseWindowMode=0\r\n");
    auto r = sb.Run({});
    REQUIRE_HOST_OK(r);
    CHECK_EQ(sb.Read(L"wndmode.ini"), std::string("[WINDOWMODE]\r\nUseWindowMode=0\r\n"));

    Sandbox sb2(arch);
    sb2.Loader();
    sb2.Host(L"dinput8");
    REQUIRE_HOST_OK(sb2.Run({}));
    CHECK(!sb2.Exists(L"wndmode.ini"));
}

WIN32_TEST("UseD3D8to9=1 replaces Direct3DCreate8 with the built-in d3d8to9", "[x86][d3d8]")
{
    Sandbox sb(arch);
    sb.Loader(L"d3d8.dll");
    sb.Host();
    sb.Ini(L"global.ini", L"GlobalSets", L"UseD3D8to9", L"1");
    auto r = sb.Run({ L"load:d3d8.dll", L"trigger:Sleep", L"scenario:d3d8to9|on" });
    REQUIRE_SCENARIO(r, "d3d8to9");
}

WIN32_TEST("without UseD3D8to9 Direct3DCreate8 comes from the system d3d8.dll", "[x86][d3d8]")
{
    Sandbox sb(arch);
    sb.Loader(L"d3d8.dll");
    sb.Host();
    auto r = sb.Run({ L"load:d3d8.dll", L"trigger:Sleep", L"scenario:d3d8to9|off" });
    REQUIRE_SCENARIO(r, "d3d8to9");
}

WIN32_TEST("xlive.dll makes the executable's code section writable", "[x86][xlive]")
{
    Sandbox sb(arch);
    sb.Loader(L"xlive.dll");
    sb.Host();
    sb.Probe(L"scripts/probe.asi");
    auto r = sb.Run({ L"load:xlive.dll", L"trigger:Sleep", L"scenario:exe_text_writable|1" });
    REQUIRE_SCENARIO(r, "exe_text_writable");
    CHECK_EQ(r.Inits("probe.asi"), 1);
}

ARCH_TEST("other proxy names leave the executable's code section read-only", "[loading]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    auto r = sb.Run({ L"scenario:exe_text_writable|0" });
    REQUIRE_SCENARIO(r, "exe_text_writable");
}

// --- process integration

ARCH_TEST("the loader exports its API", "[api]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    auto r = sb.Run({ L"scenario:exports" });
    REQUIRE_SCENARIO(r, "exports");
}

ARCH_TEST("kernel32 imports of the executable are restored after plugins are loaded", "[loading][iat]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Probe(L"scripts/probe.asi");
    auto r = sb.Run({ L"scenario:iat_restored" });
    REQUIRE_SCENARIO(r, "iat_restored");

    Sandbox sb2(arch);
    sb2.Loader(L"d3d11.dll");
    sb2.Host();
    auto r2 = sb2.Run({ L"load:d3d11.dll", L"trigger:Sleep", L"scenario:iat_restored" });
    REQUIRE_SCENARIO(r2, "iat_restored");
}

ARCH_TEST("CoCreateInstance is redirected unless the loader itself is a COM provider, and still works", "[loading][com]")
{
    for (auto [name, hook] : { std::pair{ L"version.dll", true }, std::pair{ L"d3d9.dll", true }, std::pair{ L"dinput8.dll", false }, std::pair{ L"dsound.dll", false } })
    {
        Sandbox sb(arch);
        sb.Loader(name);
        sb.Host();
        auto r = sb.Run({ L"load:" + std::wstring(name), L"trigger:Sleep", std::wstring(L"scenario:cocreate|hook=") + (hook ? L"1" : L"0") });
        INFO(ut::narrow(name));
        REQUIRE_SCENARIO(r, "cocreate");
    }
}

// --- crash dumps

namespace
{
    std::vector<fs::path> DumpFiles(const Sandbox& sb, const wchar_t* ext)
    {
        std::vector<fs::path> v;
        std::error_code ec;
        for (auto& e : fs::directory_iterator(sb.P(L"CrashDumps"), ec))
            if (IEquals(e.path().extension().wstring(), ext)) v.push_back(e.path());
        return v;
    }

    // name -> contents of every file in a zip archive
    std::map<std::string, std::string> ZipContents(const fs::path& zip)
    {
        std::map<std::string, std::string> out;
        std::string bytes = ReadFileBytes(zip);
        mz_zip_archive z{};
        if (!mz_zip_reader_init_mem(&z, bytes.data(), bytes.size(), 0)) return out;
        for (mz_uint i = 0; i < mz_zip_reader_get_num_files(&z); ++i)
        {
            mz_zip_archive_file_stat st;
            if (!mz_zip_reader_file_stat(&z, i, &st)) continue;
            size_t size = 0;
            void* data = mz_zip_reader_extract_to_heap(&z, i, &size, 0);
            out[st.m_filename] = data ? std::string((const char*)data, size) : std::string();
            mz_free(data);
        }
        mz_zip_reader_end(&z);
        return out;
    }

    struct CrashReport
    {
        fs::path log, zip, dmp;
        std::string text; // the .log
    };

    // the single report of a sandbox
    CrashReport OnlyReport(const Sandbox& sb, bool zipped)
    {
        CrashReport r;
        auto logs = DumpFiles(sb, L".log");
        REQUIRE_EQ(logs.size(), (size_t)1);
        r.log = logs[0];
        r.text = ReadFileBytes(r.log);
        auto zips = DumpFiles(sb, L".zip");
        auto dmps = DumpFiles(sb, L".dmp");
        if (zipped)
        {
            REQUIRE_EQ(zips.size(), (size_t)1);
            CHECK_EQ(dmps.size(), (size_t)0);
            r.zip = zips[0];
        }
        else
        {
            REQUIRE_EQ(dmps.size(), (size_t)1);
            CHECK_EQ(zips.size(), (size_t)0);
            r.dmp = dmps[0];
        }
        return r;
    }

    bool Contains(const std::string& text, const std::string& what)
    {
        return text.find(what) != std::string::npos;
    }
}

ARCH_TEST("a CrashDumps folder enables crash reports: a readable log and a zip with the minidump", "[crashdumps]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Mkdir(L"CrashDumps");
    sb.Ini(L"global.ini", L"GlobalSets", L"DisableCrashDumps", L"0");
    auto r = sb.Run({ L"crash" });
    CHECK_EQ(r.exitCode, (DWORD)EXCEPTION_ACCESS_VIOLATION);
    auto rep = OnlyReport(sb, true);
    CHECK_MSG(rep.log.filename().wstring().starts_with(L"game.exe."), "report name: " + ut::narrow(rep.log.filename().wstring()));

    // minidump, log and the loader's ini files
    auto zip = ZipContents(rep.zip);
    std::string stem = ut::narrow(rep.log.stem().wstring());
    REQUIRE(zip.count(stem + ".dmp"));
    REQUIRE(zip.count(stem + ".log"));
    CHECK(zip[stem + ".dmp"].size() > 1024);
    CHECK(zip[stem + ".dmp"].starts_with("MDMP"));
    CHECK(zip[stem + ".log"] == rep.text);
    CHECK_MSG(zip.count("config/global.ini"), "the loader ini file is in the archive");

    // the log
    const std::string& t = rep.text;
    for (const char* section : { "Summary", "System", "Exception", "Call stack", "Possible return addresses", "Registers", "Code around the crash address",
                                 "Stack memory", "Other threads", "Plugins", "Loaded modules", "End of report." })
        CHECK_MSG(Contains(t, section), std::string("the report has the section ") + section);
    CHECK(Contains(t, "EXCEPTION_ACCESS_VIOLATION (0xC0000005): writing address"));
    CHECK(Contains(t, "(null pointer)"));
    CHECK_MSG(Contains(t, "Location:  game.exe+0x"), "the crash location is in the game executable");
    std::string lower = t;
    for (auto& c : lower) c = (char)tolower((unsigned char)c);
    CHECK_MSG(Contains(lower, "loader  8.") || Contains(lower, "  loader  "), "the loader is marked in the module list");
    CHECK(Contains(lower, "\\dinput8.dll"));
    CHECK_MSG(Contains(t, "(wmain+0x"), "the crash frame is symbolized from the PDB of the game");
}

ARCH_TEST("crash reports: a crash inside a plugin names the plugin", "[crashdumps]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Mkdir(L"CrashDumps");
    sb.Probe(L"scripts/bad_crashinit.asi");
    sb.Probe(L"scripts/a_good.asi"); // alphabetically before the crashing one
    auto r = sb.Run({ L"snapshot" });
    CHECK_EQ(r.exitCode, (DWORD)EXCEPTION_ACCESS_VIOLATION);
    auto rep = OnlyReport(sb, true);
    CHECK_MSG(Contains(rep.text, "Location:  bad_crashinit.asi+0x"), rep.text.substr(0, 800));
    CHECK(Contains(rep.text, "<plugin>"));
    CHECK(Contains(rep.text, "Cause:     the crash happened inside the plugin scripts\\bad_crashinit.asi"));
    // both plugins listed in [Plugins]
    auto plugins = rep.text.substr(rep.text.find("\r\nPlugins\r\n"));
    plugins = plugins.substr(0, plugins.find("\r\nLoaded modules\r\n"));
    CHECK(Contains(plugins, "bad_crashinit.asi"));
    CHECK(Contains(plugins, "a_good.asi"));
}

ARCH_TEST("crash reports: CrashDumpZip=0 keeps a plain minidump", "[crashdumps][ini]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Mkdir(L"CrashDumps");
    sb.Ini(L"global.ini", L"GlobalSets", L"CrashDumpZip", L"0");
    auto r = sb.Run({ L"crash" });
    CHECK_EQ(r.exitCode, (DWORD)EXCEPTION_ACCESS_VIOLATION);
    auto rep = OnlyReport(sb, false);
    CHECK(fs::file_size(rep.dmp) > 1024);
    CHECK(ReadFileBytes(rep.dmp).starts_with("MDMP"));
}

ARCH_TEST("crash reports: an unhandled C++ exception shows its type and message", "[crashdumps]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Mkdir(L"CrashDumps");
    auto r = sb.Run({ L"throw:the answer is 42" });
    CHECK_EQ(r.exitCode, (DWORD)0xE06D7363);
    auto rep = OnlyReport(sb, true);
    CHECK_MSG(Contains(rep.text, "C++ exception (0xE06D7363): std::runtime_error: \"the answer is 42\""), rep.text.substr(0, 800));
    CHECK_MSG(Contains(rep.text, "Caller:    game.exe+0x"), "the summary points at the code that threw, not at RaiseException");
}

ARCH_TEST("crash reports: a stack overflow is reported (from the reporter thread)", "[crashdumps]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Mkdir(L"CrashDumps");
    auto r = sb.Run({ L"stackoverflow" });
    CHECK_EQ(r.exitCode, (DWORD)EXCEPTION_STACK_OVERFLOW);
    auto rep = OnlyReport(sb, true);
    CHECK(Contains(rep.text, "EXCEPTION_STACK_OVERFLOW"));
    CHECK(Contains(rep.text, "endless recursion"));
}

ARCH_TEST("crash reports: a crash on another thread is reported", "[crashdumps]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Mkdir(L"CrashDumps");
    auto r = sb.Run({ L"crashthread" });
    CHECK_EQ(r.exitCode, (DWORD)EXCEPTION_ACCESS_VIOLATION);
    auto rep = OnlyReport(sb, true);
    CHECK(Contains(rep.text, "EXCEPTION_ACCESS_VIOLATION"));
    CHECK(Contains(rep.text, "Other threads"));
}

ARCH_TEST("crash reports: only the newest CrashDumpMaxReports reports are kept", "[crashdumps][ini]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Mkdir(L"CrashDumps");
    sb.Ini(L"global.ini", L"GlobalSets", L"CrashDumpMaxReports", L"2");
    for (int i = 1; i <= 3; ++i) // three older reports
    {
        std::wstring base = L"CrashDumps\\game.exe.2020010" + std::to_wstring(i) + L"_000000";
        sb.Write(base + L".log", "old report");
        sb.Write(base + L".zip", "old archive");
        std::error_code ec;
        auto when = fs::file_time_type::clock::now() - std::chrono::hours(24 * (10 - i));
        fs::last_write_time(sb.P(base + L".log"), when, ec);
        fs::last_write_time(sb.P(base + L".zip"), when, ec);
    }
    auto r = sb.Run({ L"crash" });
    CHECK_EQ(r.exitCode, (DWORD)EXCEPTION_ACCESS_VIOLATION);
    CHECK_EQ(DumpFiles(sb, L".log").size(), (size_t)2);
    CHECK_EQ(DumpFiles(sb, L".zip").size(), (size_t)2);
    CHECK(sb.Exists(L"CrashDumps\\game.exe.20200103_000000.log")); // the newest old report
    CHECK(!sb.Exists(L"CrashDumps\\game.exe.20200101_000000.log"));
}

ARCH_TEST("crash reports: a minidump left unzipped is archived on the next start", "[crashdumps]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Mkdir(L"CrashDumps");
    sb.Write(L"CrashDumps\\game.exe.20200101_000000.dmp", std::string(4096, 'D'));
    sb.Write(L"CrashDumps\\game.exe.20200101_000000.log", "report");
    sb.Write(L"CrashDumps\\game.exe.20200101_000000.zip.tmp", "partial");
    sb.Run({ L"wait:3000" });
    CHECK(sb.Exists(L"CrashDumps\\game.exe.20200101_000000.zip"));
    CHECK(!sb.Exists(L"CrashDumps\\game.exe.20200101_000000.dmp"));
    CHECK(!sb.Exists(L"CrashDumps\\game.exe.20200101_000000.zip.tmp"));
    auto zip = ZipContents(sb.P(L"CrashDumps\\game.exe.20200101_000000.zip"));
    CHECK_EQ(zip["game.exe.20200101_000000.dmp"].size(), (size_t)4096);
}

ARCH_TEST("the game cannot replace the crash dump handler once it is installed", "[crashdumps]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Mkdir(L"CrashDumps");
    auto r = sb.Run({ L"setfilter", L"crash" });
    CHECK_EQ(r.Events("host", "host_filter_called").size(), (size_t)0);
    CHECK_EQ(DumpFiles(sb, L".zip").size(), (size_t)1);
}

ARCH_TEST("DisableCrashDumps=1 or a missing CrashDumps folder disables crash dumps", "[crashdumps][ini]")
{
    {
        Sandbox sb(arch);
        sb.Loader();
        sb.Host(L"dinput8");
        sb.Mkdir(L"CrashDumps");
        sb.Ini(L"global.ini", L"GlobalSets", L"DisableCrashDumps", L"1");
        auto r = sb.Run({ L"setfilter", L"crash" });
        CHECK_EQ(DumpFiles(sb, L".dmp").size(), (size_t)0);
        CHECK_EQ(DumpFiles(sb, L".zip").size(), (size_t)0);
        CHECK_EQ(DumpFiles(sb, L".log").size(), (size_t)0);
        CHECK_EQ(r.Events("host", "host_filter_called").size(), (size_t)1);
    }
    {
        Sandbox sb(arch);
        sb.Loader();
        sb.Host(L"dinput8");
        auto r = sb.Run({ L"crash" });
        CHECK_EQ(r.exitCode, (DWORD)EXCEPTION_ACCESS_VIOLATION);
        CHECK(!sb.Exists(L"CrashDumps"));
    }
}

// --- protection stubs

// Origin/EA wrapped executables (Need for Speed The Run): the import directory belongs to the
// stub, which unpacks the game and LoadLibrary's its DLLs, so the game never calls through a
// patchable import. kernel32 is hooked inline until game code calls it. See ual_host_stub.exe.
ARCH_TEST("a loader loaded by a protection stub loads plugins on the game's first call, not under the loader lock", "[loading][dynamic][drm]")
{
    Sandbox sb(arch);
    sb.Loader(L"dinput8.dll");
    sb.Host(L"stub");
    sb.Probe(L"scripts/probe.asi");
    sb.Write(L"data.txt", "ORIGINAL");
    sb.Write(L"update/data.txt", "UPDATED");
    auto r = sb.Run({ L"load:dinput8.dll", L"trigger:Sleep", L"snapshot", L"read:data.txt" });
    REQUIRE_HOST_OK(r);
    REQUIRE_EQ(r.Inits("probe.asi"), 1);
    CHECK_EQ(r.First("probe", "init")->get("loaderlock"), std::string("0"));
    auto read = r.Action("read", "data.txt");
    REQUIRE(read);
    CHECK_MSG(read->get("data") == "UPDATED", "the file overloading hooks work after the entry point hooks: " + read->get("data"));
}

ARCH_TEST("calls from the protection stub's own code do not start loading", "[loading][dynamic][drm]")
{
    // LoadFromAPI=Sleep, and the stub section calls Sleep first
    Sandbox sb(arch);
    sb.Loader(L"dinput8.dll");
    sb.Host(L"stub");
    sb.Probe(L"scripts/probe.asi");
    sb.Write(L"dinput8.ini", "[GlobalSets]\nLoadFromAPI=Sleep\n");
    auto r = sb.Run({ L"load:dinput8.dll", L"stubcall", L"snapshot", L"trigger:Sleep", L"snapshot" });
    REQUIRE_HOST_OK(r);
    auto stub = r.Action("stubcall", "");
    REQUIRE(stub);
    REQUIRE_EQ(stub->get("available"), std::string("1"));
    auto snaps = r.Events("host", "snapshot");
    REQUIRE_EQ(snaps.size(), (size_t)2);
    CHECK_MSG(snaps[0]->get("probes").empty(), "Sleep from the stub section loaded the plugins: " + snaps[0]->get("probes"));
    CHECK_EQ(snaps[1]->get("probes"), std::string("probe.asi:1"));
    CHECK_EQ(r.First("probe", "init")->get("loaderlock"), std::string("0"));
}

ARCH_TEST("DontLoadFromDllMain=0 loads plugins at once under a protection stub too", "[loading][dynamic][drm][ini]")
{
    Sandbox sb(arch);
    sb.Loader(L"dinput8.dll");
    sb.Host(L"stub");
    sb.Probe(L"scripts/probe.asi");
    sb.Write(L"dinput8.ini", "[GlobalSets]\nDontLoadFromDllMain=0\n");
    auto r = sb.Run({ L"load:dinput8.dll", L"snapshot" });
    REQUIRE_HOST_OK(r);
    REQUIRE_EQ(r.Inits("probe.asi"), 1);
    CHECK_EQ(r.First("probe", "init")->get("loaderlock"), std::string("1"));
}

ARCH_TEST("a loader loaded at run time by an ordinary executable waits for the game's code", "[loading][dynamic][drm]")
{
    Sandbox sb(arch);
    sb.Loader(L"dinput8.dll");
    sb.Host();
    sb.Probe(L"scripts/probe.asi");
    auto r = sb.Run({ L"load:dinput8.dll", L"trigger:Sleep", L"snapshot" });
    REQUIRE_HOST_OK(r);
    REQUIRE_EQ(r.Inits("probe.asi"), 1);
    CHECK_EQ(r.First("probe", "init")->get("loaderlock"), std::string("0"));
}

// Denuvo/SecuROM/Steam style: the exe imports the loader but its entry point is in a separate
// section that runs first. Calls from there, here via the patched Sleep import, must not start loading.
ARCH_TEST("calls from the entry point section of an executable that imports the loader do not start loading", "[loading][drm]")
{
    Sandbox sb(arch);
    sb.Loader(L"dinput8.dll");
    sb.Host(L"stub_dinput8");
    sb.Probe(L"scripts/probe.asi");
    sb.Write(L"dinput8.ini", "[GlobalSets]\nLoadFromAPI=Sleep\n");
    auto r = sb.Run({ L"stubcall", L"snapshot", L"trigger:Sleep", L"snapshot" });
    REQUIRE_HOST_OK(r);
    REQUIRE_EQ(r.Action("stubcall", "")->get("available"), std::string("1"));
    auto snaps = r.Events("host", "snapshot");
    REQUIRE_EQ(snaps.size(), (size_t)2);
    CHECK_MSG(snaps[0]->get("probes").empty(), "Sleep from the entry point section loaded the plugins: " + snaps[0]->get("probes"));
    CHECK_EQ(snaps[1]->get("probes"), std::string("probe.asi:1"));
    CHECK_EQ(r.First("probe", "init")->get("loaderlock"), std::string("0"));
}

// with no executable section, .text was taken for a protection stub and the game's
// calls never started loading (Max Payne: no plugins at the startup dialog).
WIN32_TEST("an executable without executable sections loads the plugins on its first call", "[loading][drm][x86]")
{
    DWORD policy = GetSystemDEPPolicy(); // 0 AlwaysOff, 1 AlwaysOn, 2 OptIn, 3 OptOut
    if (policy == 1 || policy == 3) SKIP("DEP is on for every process (policy " + std::to_string(policy) + "): such an executable cannot run");
    Sandbox sb(arch);
    sb.Loader(L"dinput8.dll");
    sb.Host(L"noexec_dinput8");
    sb.Probe(L"scripts/probe.asi");
    auto r = sb.Run({ L"trigger:Sleep", L"snapshot" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(r.Last("host", "snapshot")->get("probes"), std::string("probe.asi:1"));
}

// .NET/UWP launchers keep the game code in DLLs (GTA SA UWP, Cuphead UWP). The loader hooks
// kernel32 and waits for a call from a game module, here Sleep through a raw pointer.
ARCH_TEST("an executable without usable imports: the first call from a game module loads the plugins", "[loading][dynamic]")
{
    Sandbox sb(arch);
    sb.Loader(L"dinput8.dll");
    sb.Host(L"launcher");
    sb.Copy(arch.tests / L"ual_launcher_game.dll", L"ual_launcher_game.dll");
    sb.Probe(L"scripts/probe.asi");
    auto r = sb.Run({});
    CHECK_EQ(r.exitCode, (DWORD)0);
    auto done = r.First("host", "done");
    REQUIRE_MSG(done, "the launcher did not finish");
    CHECK_MSG(done->get("detail") == "loaded inits=1", "after the Sleep call: " + done->get("detail"));
    REQUIRE_EQ(r.Inits("probe.asi"), 1);
    CHECK_EQ(r.First("probe", "init")->get("loaderlock"), std::string("0"));
}

ARCH_TEST("DebugLog=1 writes <loader>.log with what started plugin loading", "[loading][ini]")
{
    Sandbox sb(arch);
    sb.Loader(L"dinput8.dll");
    sb.Host(L"dinput8");
    sb.Probe(L"scripts/probe.asi");
    sb.Write(L"dinput8.ini", "[GlobalSets]\nDebugLog=1\n");
    auto r = sb.Run({ L"trigger:Sleep" });
    REQUIRE_HOST_OK(r);
    REQUIRE(sb.Exists(L"dinput8.log"));
    std::string log = sb.Read(L"dinput8.log");
    INFO(log);
    CHECK(log.find("imports patched in") != std::string::npos);
    CHECK(log.find("starts loading the plugins") != std::string::npos);
    CHECK(log.find("probe.asi: loaded") != std::string::npos);
    CHECK(log.find("plugins loaded") != std::string::npos);
}

ARCH_TEST("without DebugLog no log file is written", "[loading][ini]")
{
    Sandbox sb(arch);
    sb.Loader(L"dinput8.dll");
    sb.Host(L"dinput8");
    auto r = sb.Run({ L"trigger:Sleep" });
    REQUIRE_HOST_OK(r);
    CHECK(!sb.Exists(L"dinput8.log"));
}
