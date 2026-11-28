// Early plugins: an UAL_EARLY RCDATA resource makes the loader LoadLibrary the plugin from its own DllMain,
// before the game's entry point. InitializeASI still comes from the normal pass, once.
// The probe records hostentry (the exe's entry point has run) with every event.
#include "framework.hpp"

using namespace runner;

namespace
{
    void Early(const Sandbox& sb, const fs::path& rel, const wchar_t* file = L"probe_early.asi", const Arch* fromArch = nullptr)
    {
        auto src = (fromArch ? *fromArch : sb.arch).tests / file;
        if (!fs::exists(src)) SKIP("not built: " + ut::narrow(src.wstring()));
        sb.Copy(src, rel);
    }

    void ExpectNoDialogs(const RunResult& r)
    {
        CHECK_MSG(r.dialogs.empty(), "unexpected dialog: " + (r.dialogs.empty() ? std::string() : ut::narrow(r.dialogs.front().AllText())));
    }

    std::vector<std::string> Order(const RunResult& r, const char* ev)
    {
        std::vector<std::string> v;
        for (auto e : r.Events("probe", ev)) v.push_back(ut::narrow(Lower(ut::widen(e->get("mod")))));
        return v;
    }

    std::string Join(const std::vector<std::string>& v)
    {
        std::string s;
        for (auto& x : v) s += x + " ";
        return s;
    }

    const ualtest::Record* FirstOf(const RunResult& r, const char* ev, const std::string& mod)
    {
        for (auto e : r.Events("probe", ev))
            if (_stricmp(e->get("mod").c_str(), mod.c_str()) == 0) return e;
        return nullptr;
    }

    size_t IndexOf(const RunResult& r, const char* ev, const std::string& mod)
    {
        auto e = FirstOf(r, ev, mod);
        for (size_t i = 0; e && i < r.records.size(); ++i)
            if (&r.records[i] == e) return i;
        return SIZE_MAX;
    }

    // An early attach: before the exe's entry point
    void ExpectEarlyAttach(const RunResult& r, const std::string& mod)
    {
        INFO(mod);
        auto a = FirstOf(r, "attach", mod);
        REQUIRE_MSG(a, mod + " was not loaded\n" + r.Dump());
        CHECK_MSG(a->get("hostentry") == "0", mod + " should load before the executable's entry point");
    }

    // Offset of the first log line containing every part, npos if none
    size_t LogLine(const std::string& log, std::initializer_list<const char*> parts)
    {
        size_t start = 0;
        while (start < log.size())
        {
            size_t end = log.find('\n', start);
            if (end == std::string::npos) end = log.size();
            std::string_view line(log.data() + start, end - start);
            bool all = true;
            for (auto p : parts) all &= line.find(p) != std::string_view::npos;
            if (all) return start;
            start = end + 1;
        }
        return std::string::npos;
    }
}

ARCH_TEST("early plugin: DllMain before the entry point and every other plugin, InitializeASI once after the game starts", "[loading][early]")
{
    for (const wchar_t* proxy : { L"dinput8", L"version" })
    {
        INFO(ut::narrow(proxy));
        Sandbox sb(arch);
        sb.Loader(std::wstring(proxy) + L".dll");
        sb.Host(proxy);
        sb.Write(L"global.ini", "[GlobalSets]\nDebugLog=1\n");
        Early(sb, L"scripts/early.asi");
        sb.Probe(L"a_normal.asi");
        sb.Probe(L"scripts/z_normal.asi");
        auto r = sb.Run({ L"trigger:Sleep", L"trigger:GetSystemInfo" });
        REQUIRE_HOST_OK(r);
        ExpectEarlyAttach(r, "early.asi");
        CHECK_EQ(FirstOf(r, "attach", "early.asi")->get("hostmain"), std::string("0"));
        CHECK_EQ(r.Attaches("early.asi"), 1);
        REQUIRE_EQ(r.Inits("early.asi"), 1);
        auto init = r.Init("early.asi");
        CHECK_MSG(init->get("hostentry") == "1", "InitializeASI of an early plugin comes with the normal pass, after the entry point");
        CHECK_EQ(init->get("loaderlock"), std::string("0"));
        CHECK_EQ(init->get("count"), std::string("1"));
        for (auto m : { "a_normal.asi", "z_normal.asi" })
        {
            INFO(m);
            CHECK_MSG(IndexOf(r, "attach", "early.asi") < IndexOf(r, "attach", m), "the early plugin must load before the normal ones");
            CHECK_EQ(FirstOf(r, "attach", m)->get("hostentry"), std::string("1"));
            CHECK_EQ(r.Inits(m), 1);
        }
        CHECK_EQ(Join(Order(r, "init")), std::string("a_normal.asi early.asi z_normal.asi "));
        ExpectNoDialogs(r);

        std::string log = sb.Read(std::wstring(proxy) + L".log");
        INFO(log);
        CHECK(LogLine(log, { "early plugin ", "early.asi: loaded" }) != std::string::npos);
        CHECK(LogLine(log, { "early.asi: loaded early, calling InitializeASI" }) != std::string::npos);
        CHECK_MSG(LogLine(log, { "imports patched in early" }) == std::string::npos, "the imports of an early plugin must not be patched");
    }
}

ARCH_TEST("early plugin without InitializeASI loads once", "[loading][early]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    Early(sb, L"scripts/early.asi", L"probe_early_noinit.asi");
    sb.Probe(L"scripts/normal.asi");
    auto r = sb.Run({ L"trigger:Sleep", L"snapshot" });
    REQUIRE_HOST_OK(r);
    ExpectEarlyAttach(r, "early.asi");
    CHECK_EQ(r.Attaches("early.asi"), 1);
    CHECK_EQ(r.Inits("early.asi"), 0);
    CHECK_EQ(r.Inits("normal.asi"), 1);
    CHECK_EQ(r.Attaches("normal.asi"), 1);
    ExpectNoDialogs(r);
}

ARCH_TEST("early plugins in the plugin tree: unmarked plugins keep their load time and order, update folders are not scanned early", "[loading][early][search]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Probe(L"root.asi");
    sb.Probe(L"scripts/s.asi");
    sb.Probe(L"scripts/sub/s_sub.asi");
    Early(sb, L"scripts/sub/e_sub.asi");
    sb.Probe(L"scripts/sub/deeper/s_deep.asi");
    Early(sb, L"scripts/sub/deeper/e_deep.asi");
    sb.Probe(L"plugins/p.asi");
    Early(sb, L"plugins/e_p.asi");
    sb.Probe(L"plugins/sub/p_sub.asi");
    sb.Probe(L"update/u.asi");
    Early(sb, L"update/e_u.asi");
    auto r = sb.Run({});
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Join(Order(r, "attach")), std::string("e_sub.asi e_p.asi root.asi s_sub.asi s.asi p_sub.asi p.asi e_u.asi u.asi "));
    CHECK_EQ(Join(Order(r, "init")), std::string("root.asi e_sub.asi s_sub.asi s.asi p_sub.asi e_p.asi p.asi e_u.asi u.asi "));
    ExpectEarlyAttach(r, "e_sub.asi");
    ExpectEarlyAttach(r, "e_p.asi");
    for (auto m : { "root.asi", "s_sub.asi", "s.asi", "p_sub.asi", "p.asi", "u.asi", "e_u.asi" })
    {
        INFO(m);
        auto a = FirstOf(r, "attach", m);
        REQUIRE(a);
        CHECK_MSG(a->get("hostentry") == "1", "loaded by the normal pass, after the entry point");
        CHECK_EQ(r.Attaches(m), 1);
        CHECK_EQ(r.Inits(m), 1);
    }
    CHECK_EQ(r.Attaches("s_deep.asi"), 0);
    CHECK_EQ(r.Attaches("e_deep.asi"), 0);
    ExpectNoDialogs(r);
}

ARCH_TEST("DontLoadFromDllMain=0: early plugins come first, InitializeASI once", "[loading][early][ini]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Write(L"global.ini", "[GlobalSets]\nDontLoadFromDllMain=0\n");
    sb.Probe(L"a_normal.asi");
    Early(sb, L"scripts/early.asi");
    auto r = sb.Run({});
    REQUIRE_HOST_OK(r);
    ExpectEarlyAttach(r, "early.asi");
    auto normal = FirstOf(r, "attach", "a_normal.asi");
    REQUIRE(normal);
    CHECK_EQ(normal->get("hostentry"), std::string("0"));
    CHECK(IndexOf(r, "attach", "early.asi") < IndexOf(r, "attach", "a_normal.asi"));
    CHECK_EQ(Join(Order(r, "init")), std::string("a_normal.asi early.asi "));
    CHECK_EQ(r.Attaches("early.asi"), 1);
    CHECK_EQ(r.Init("early.asi")->get("hostentry"), std::string("0"));
    ExpectNoDialogs(r);
}

ARCH_TEST("the early pass honours LoadRecursively, LoadFromScriptsOnly and LoadPlugins", "[loading][early][ini]")
{
    struct Case
    {
        const char* ini;
        std::vector<std::pair<const char*, int>> attaches;
    };
    std::vector<Case> cases = {
        { "LoadRecursively=0", { { "e_root.asi", 1 }, { "e_s.asi", 1 }, { "e_sub.asi", 0 } } },
        { "LoadFromScriptsOnly=1", { { "e_root.asi", 0 }, { "e_s.asi", 1 }, { "e_sub.asi", 1 } } },
        { "LoadPlugins=0", { { "e_root.asi", 0 }, { "e_s.asi", 0 }, { "e_sub.asi", 0 } } },
    };
    for (auto& c : cases)
    {
        INFO(c.ini);
        Sandbox sb(arch);
        sb.Loader();
        sb.Host(L"dinput8");
        sb.Write(L"global.ini", std::string("[GlobalSets]\n") + c.ini + "\n");
        Early(sb, L"e_root.asi");
        Early(sb, L"scripts/e_s.asi");
        Early(sb, L"scripts/sub/e_sub.asi");
        auto r = sb.Run({ L"trigger:Sleep" });
        REQUIRE_HOST_OK(r);
        for (auto& [m, n] : c.attaches)
        {
            INFO(m);
            CHECK_EQ(r.Attaches(m), n);
            CHECK_EQ(r.Inits(m), n);
            if (n) ExpectEarlyAttach(r, m);
        }
    }
}

ARCH_TEST("several early plugins load in the order of the normal pass, LoadExtraPlugins first", "[loading][early][extra]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Write(L"global.ini", "[GlobalSets]\nLoadExtraPlugins=extra\\e_x.asi\n");
    Early(sb, L"extra/e_x.asi");
    Early(sb, L"e_root.asi");
    Early(sb, L"scripts/e_s.asi");
    Early(sb, L"scripts/sub/e_sub.asi");
    Early(sb, L"plugins/e_p.asi");
    sb.Probe(L"scripts/n.asi");
    auto r = sb.Run({});
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Join(Order(r, "attach")), std::string("e_x.asi e_root.asi e_sub.asi e_s.asi e_p.asi n.asi "));
    CHECK_EQ(Join(Order(r, "init")), std::string("e_x.asi e_root.asi e_sub.asi e_s.asi n.asi e_p.asi "));
    for (auto m : { "e_x.asi", "e_root.asi", "e_sub.asi", "e_s.asi", "e_p.asi" })
    {
        ExpectEarlyAttach(r, m);
        CHECK_EQ(r.Attaches(m), 1);
        CHECK_EQ(r.Inits(m), 1);
    }
    ExpectNoDialogs(r);
}

ARCH_TEST("an early plugin whose DllMain fails is logged, tried again by the normal pass, and the others load", "[loading][early][errors]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Write(L"global.ini", "[GlobalSets]\nDebugLog=1\n");
    Early(sb, L"scripts/a_early_failinit.asi");
    Early(sb, L"scripts/b_early.asi");
    sb.Probe(L"scripts/c.asi");
    auto r = sb.Run({});
    REQUIRE_HOST_OK(r);
    auto attaches = r.Events("probe", "attach");
    std::vector<const ualtest::Record*> failed;
    for (auto a : attaches)
        if (a->get("mod") == "a_early_failinit.asi") failed.push_back(a);
    REQUIRE_EQ(failed.size(), (size_t)2);
    CHECK_EQ(failed[0]->get("hostentry"), std::string("0"));
    CHECK_EQ(failed[1]->get("hostentry"), std::string("1"));
    CHECK_EQ(r.Inits("a_early_failinit.asi"), 0);
    ExpectEarlyAttach(r, "b_early.asi");
    CHECK_EQ(r.Inits("b_early.asi"), 1);
    CHECK_EQ(r.Inits("c.asi"), 1);
    ExpectNoDialogs(r);
    std::string log = sb.Read(L"dinput8.log");
    INFO(log);
    CHECK(LogLine(log, { "early plugin ", "a_early_failinit.asi: error 1114" }) != std::string::npos);
}

ARCH_TEST("an early plugin with a missing dependency: logged in DllMain, the dialog comes from the normal pass", "[loading][early][errors][ui]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Write(L"global.ini", "[GlobalSets]\nDebugLog=1\n");
    Early(sb, L"scripts/a_missingdep.asi", L"probe_missingdep_early.asi");
    Early(sb, L"scripts/b_early.asi");
    sb.Probe(L"scripts/z_good.asi");
    auto r = sb.Run({});
    REQUIRE_HOST_OK(r);
    REQUIRE_EQ(r.dialogs.size(), (size_t)1);
    CHECK_MSG(r.dialogs.front().Contains(L"Unable to load a_missingdep.asi"), ut::narrow(r.dialogs.front().AllText()));
    ExpectEarlyAttach(r, "b_early.asi");
    CHECK_EQ(r.Inits("b_early.asi"), 1);
    CHECK_EQ(r.Inits("z_good.asi"), 1);
    std::string log = sb.Read(L"dinput8.log");
    INFO(log);
    size_t early = LogLine(log, { "early plugin ", "a_missingdep.asi: error 126" });
    size_t loading = LogLine(log, { "] loading" });
    size_t normal = LogLine(log, { "] plugin ", "a_missingdep.asi: error 126" });
    CHECK(early != std::string::npos);
    CHECK(loading != std::string::npos);
    CHECK(normal != std::string::npos);
    CHECK_MSG(early < loading && loading < normal, "early failure, then the normal pass that reports it");
    ForgiveUserInterference(r);
}

ARCH_TEST("the early pass skips a non-PE .asi and fails silently on an early plugin of the other architecture", "[loading][early][errors]")
{
    if (!OtherArch(arch).available()) SKIP("binaries for the other architecture are not built");
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Write(L"global.ini", "[GlobalSets]\nDebugLog=1\n");
    sb.Write(L"scripts/garbage.asi", "MZ this is not a plugin");
    Early(sb, L"scripts/m_wrongarch.asi", L"probe_early.asi", &OtherArch(arch));
    Early(sb, L"scripts/z_early.asi");
    auto r = sb.Run({});
    REQUIRE_HOST_OK(r);
    ExpectEarlyAttach(r, "z_early.asi");
    CHECK_EQ(r.Inits("z_early.asi"), 1);
    CHECK_EQ(r.Attaches("m_wrongarch.asi"), 0);
    ExpectNoDialogs(r);
    std::string log = sb.Read(L"dinput8.log");
    INFO(log);
    CHECK(LogLine(log, { "early plugin ", "m_wrongarch.asi: error 193" }) != std::string::npos);
    CHECK(LogLine(log, { "early plugin ", "garbage.asi" }) == std::string::npos);
}

ARCH_TEST("a plugin an early plugin imports loads early with it and gets InitializeASI once", "[loading][early]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    Early(sb, L"scripts/early_importer.asi", L"probe_early_importer.asi");
    sb.Probe(L"scripts/probe.asi"); // unmarked, imported as probe.asi
    auto r = sb.Run({});
    REQUIRE_HOST_OK(r);
    ExpectEarlyAttach(r, "probe.asi");
    ExpectEarlyAttach(r, "early_importer.asi");
    CHECK(IndexOf(r, "attach", "probe.asi") < IndexOf(r, "attach", "early_importer.asi"));
    CHECK_EQ(FirstOf(r, "attach", "early_importer.asi")->get("depinit"), std::string("0"));
    for (auto m : { "probe.asi", "early_importer.asi" })
    {
        INFO(m);
        CHECK_EQ(r.Attaches(m), 1);
        REQUIRE_EQ(r.Inits(m), 1);
        CHECK_EQ(r.Init(m)->get("hostentry"), std::string("1"));
    }
    ExpectNoDialogs(r);
}

// LoadFromAPI=Sleep keeps the host's other kernel32 calls from starting the normal pass
ARCH_TEST("a loader loaded at run time (protection stub or ordinary exe) loads early plugins at once, the others on the game's call", "[loading][early][dynamic][drm]")
{
    for (const wchar_t* host : { L"stub", L"" })
    {
        INFO(*host ? "protection stub" : "ordinary exe");
        Sandbox sb(arch);
        sb.Loader(L"dinput8.dll");
        sb.Host(host);
        sb.Write(L"dinput8.ini", "[GlobalSets]\nLoadFromAPI=Sleep\n");
        Early(sb, L"scripts/early.asi");
        sb.Probe(L"scripts/probe.asi");
        auto r = sb.Run({ L"snapshot", L"load:dinput8.dll", L"snapshot", L"trigger:Sleep", L"snapshot" });
        REQUIRE_HOST_OK(r);
        auto snaps = r.Events("host", "snapshot");
        REQUIRE_EQ(snaps.size(), (size_t)3);
        CHECK_EQ(snaps[0]->get("probes"), std::string());
        CHECK_MSG(snaps[1]->get("probes") == "early.asi:0", "after LoadLibrary: " + snaps[1]->get("probes"));
        auto last = snaps[2]->get("probes");
        CHECK_MSG(last.find("early.asi:1") != std::string::npos && last.find("probe.asi:1") != std::string::npos, last);
        CHECK(IndexOf(r, "attach", "early.asi") < IndexOf(r, "attach", "probe.asi"));
        CHECK_EQ(r.Attaches("early.asi"), 1);
        REQUIRE_EQ(r.Inits("early.asi"), 1);
        CHECK_EQ(r.Init("early.asi")->get("loaderlock"), std::string("0"));
        ExpectNoDialogs(r);
    }
}

ARCH_TEST("an executable whose entry point section runs first still gets early plugins before it", "[loading][early][drm]")
{
    Sandbox sb(arch);
    sb.Loader(L"dinput8.dll");
    sb.Host(L"stub_dinput8");
    Early(sb, L"scripts/early.asi");
    sb.Probe(L"scripts/probe.asi");
    auto r = sb.Run({ L"stubcall", L"trigger:Sleep", L"snapshot" });
    REQUIRE_HOST_OK(r);
    ExpectEarlyAttach(r, "early.asi");
    CHECK_EQ(r.Inits("early.asi"), 1);
    CHECK_EQ(r.Inits("probe.asi"), 1);
    CHECK_EQ(r.First("probe", "init")->get("loaderlock"), std::string("0"));
}

ARCH_TEST("two loader copies load an early plugin once", "[loading][early][multi]")
{
    Sandbox sb(arch);
    sb.Loader(L"dinput8.dll");
    sb.Loader(L"version.dll");
    sb.Host();
    Early(sb, L"scripts/early.asi");
    auto r = sb.Run({ L"load:dinput8.dll", L"load:version.dll", L"trigger:Sleep", L"snapshot" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(r.Attaches("early.asi"), 1);
    CHECK_EQ(r.Inits("early.asi"), 1);
    ExpectNoDialogs(r);
}

ARCH_TEST("an early plugin keeps the loader mapped when the game frees it before plugins load", "[loading][early]")
{
    Sandbox sb(arch);
    sb.Loader(L"dinput8.dll");
    sb.Host();
    Early(sb, L"scripts/early.asi");
    sb.Probe(L"scripts/probe.asi");
    auto r = sb.Run({ L"load:dinput8.dll", L"free:dinput8.dll", L"trigger:Sleep", L"snapshot" });
    REQUIRE_HOST_OK(r);
    auto f = r.Last("host", "free");
    REQUIRE(f != nullptr);
    CHECK_MSG(f->get("mapped") == "1", "the loader was unmapped while an early plugin may still use it");
    CHECK_EQ(r.Inits("early.asi"), 1);
    CHECK_EQ(r.Inits("probe.asi"), 1);
}
