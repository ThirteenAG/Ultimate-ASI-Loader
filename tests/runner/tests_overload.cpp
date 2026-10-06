// OverloadFromFolder: update folder redirection through every hooked file API, folder
// selection (ini, dialog, dependency syntax) and the related exports.
#include "framework.hpp"
#include <tlhelp32.h>

using namespace runner;

namespace
{
    const std::string kUpdated = "UPDATED-CONTENT"; // 15 bytes
    const std::string kOriginal = "ORIGINAL";       // 8 bytes

    void DeployOverloadTree(Sandbox& sb)
    {
        sb.Loader();
        sb.Host(L"dinput8");
        sb.Write(L"data.txt", kOriginal);
        sb.Write(L"only_root.txt", "ROOT");
        sb.Write(L"sub/data.txt", "ORIGINAL-SUB");
        sb.Write(L"update/data.txt", kUpdated);
        sb.Write(L"update/sub/data.txt", "UPDATE-SUB");
        sb.Write(L"update/only_update.txt", "NEWFILE");
        sb.Write(L"update/update/data.txt", "NESTED");
        sb.Write(L"update/ov.txt", "OV");
        sb.Mkdir(L"update/onlydir");
        WriteFileBytes(sb.root / L"outside.txt", "OUTSIDE-ORIGINAL");
        sb.Write(L"update/outside.txt", "OUTSIDE-UPDATED");
    }

    std::string Data(const RunResult& r, const std::string& ev, const std::string& arg, const std::string& field = "data")
    {
        auto a = r.Action(ev, arg);
        if (!a) FAIL_CHECK("no result for " + ev + ":" + arg + "\n      " + r.Dump());
        if (a && a->get("ok") != "1") FAIL_CHECK(ev + ":" + arg + " failed with error " + a->get("err"));
        return a ? a->get(field) : std::string();
    }

    std::string Err(const RunResult& r, const std::string& ev, const std::string& arg)
    {
        auto a = r.Action(ev, arg);
        return a ? (a->get("ok") == "1" ? "ok" : a->get("err")) : "missing";
    }
}

ARCH_TEST("files in the update folder replace game files for every file API", "[overload]")
{
    Sandbox sb(arch);
    DeployOverloadTree(sb);
    auto abs = sb.P(L"data.txt").wstring();
    auto r = sb.Run({ L"read:data.txt", L"readA:data.txt", L"read2:data.txt", L"readstd:data.txt", L"read:" + abs,
                      L"attrex:data.txt", L"attrexA:data.txt", L"attr:data.txt", L"attrA:data.txt",
                      L"find:data.txt", L"findA:data.txt", L"findex:data.txt", L"findexA:data.txt" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Data(r, "read", "data.txt"), kUpdated);
    for (auto& rec : r.Events("host", "read"))
        CHECK_MSG(rec->get("data") == kUpdated, rec->get("api") + " " + rec->get("arg") + " -> " + rec->get("data"));
    for (auto& rec : r.Events("host", "attr"))
    {
        CHECK_MSG(rec->get("ok") == "1", rec->get("api") + " failed");
        if (rec->has("size")) CHECK_MSG(rec->get("size") == "15", rec->get("api") + " reports the size of the update copy, got " + rec->get("size"));
    }
    for (auto& rec : r.Events("host", "find"))
        CHECK_MSG(rec->get("names") == "data.txt:15", rec->get("api") + " reports the size of the update copy, got " + rec->get("names"));
}

ARCH_TEST("directory enumeration reports update-folder sizes for overloaded files", "[overload]")
{
    Sandbox sb(arch);
    DeployOverloadTree(sb);
    auto r = sb.Run({ L"find:*.txt", L"findA:*.txt" });
    REQUIRE_HOST_OK(r);
    for (auto& rec : r.Events("host", "find"))
    {
        auto names = rec->get("names");
        CHECK_MSG(names.find("data.txt:15") != std::string::npos, rec->get("api") + ": " + names);
        CHECK_MSG(names.find("only_root.txt:4") != std::string::npos, rec->get("api") + ": " + names);
    }
}

ARCH_TEST("game files without a replacement are read unchanged", "[overload]")
{
    Sandbox sb(arch);
    DeployOverloadTree(sb);
    auto r = sb.Run({ L"read:only_root.txt", L"attrex:only_root.txt" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Data(r, "read", "only_root.txt"), std::string("ROOT"));
    CHECK_EQ(Data(r, "attr", "only_root.txt", "size"), std::string("4"));
}

ARCH_TEST("path spellings are normalized: sub folders, case, slashes, . and ..", "[overload]")
{
    Sandbox sb(arch);
    DeployOverloadTree(sb);
    auto r = sb.Run({ L"read:sub\\data.txt", L"read:SUB/DATA.TXT", L"read:.\\data.txt", L"read:sub\\..\\data.txt", L"read:" + sb.P(L"sub/data.txt").wstring() });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Data(r, "read", "sub\\data.txt"), std::string("UPDATE-SUB"));
    CHECK_EQ(Data(r, "read", "SUB/DATA.TXT"), std::string("UPDATE-SUB"));
    CHECK_EQ(Data(r, "read", ".\\data.txt"), kUpdated);
    CHECK_EQ(Data(r, "read", "sub\\..\\data.txt"), kUpdated);
    CHECK_EQ(Data(r, "read", ut::narrow(sb.P(L"sub/data.txt").wstring())), std::string("UPDATE-SUB"));
}

ARCH_TEST("files above the game directory are looked up relative to the update folder", "[overload]")
{
    // Documented in the readme for games whose exe is in a sub folder (Binaries\Win64)
    Sandbox sb(arch);
    DeployOverloadTree(sb);
    auto r = sb.Run({ L"read:..\\outside.txt" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Data(r, "read", "..\\outside.txt"), std::string("OUTSIDE-UPDATED"));
}

ARCH_TEST("files that only exist in the update folder can be opened", "[overload]")
{
    Sandbox sb(arch);
    DeployOverloadTree(sb);
    auto r = sb.Run({ L"read:only_update.txt", L"attr:only_update.txt" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Data(r, "read", "only_update.txt"), std::string("NEWFILE"));
    CHECK_EQ(Err(r, "attr", "only_update.txt"), std::string("ok"));
}

ARCH_TEST("FindFirstFile finds files that only exist in the update folder", "[overload]")
{
    Sandbox sb(arch);
    DeployOverloadTree(sb);
    auto r = sb.Run({ L"read:only_update.txt", L"find:only_update.txt", L"findA:only_update.txt" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Data(r, "read", "only_update.txt"), std::string("NEWFILE"));
    CHECK_MSG(Err(r, "find", "only_update.txt") == "ok", "FindFirstFileW: " + Err(r, "find", "only_update.txt"));
}

ARCH_TEST("paths that already point into the update folder are not redirected again", "[overload]")
{
    Sandbox sb(arch);
    DeployOverloadTree(sb);
    auto r = sb.Run({ L"read:update\\data.txt" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Data(r, "read", "update\\data.txt"), kUpdated);
}

ARCH_TEST("folders that exist only in the update folder appear in the game folder", "[overload]")
{
    Sandbox sb(arch);
    DeployOverloadTree(sb);
    sb.Write(L"update/onlydir/deep/inner.txt", "INNER");
    auto r = sb.Run({ L"attr:onlydir", L"attr:onlydir\\deep", L"read:onlydir\\deep\\inner.txt" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Err(r, "attr", "onlydir"), std::string("ok"));
    CHECK_EQ(Err(r, "attr", "onlydir\\deep"), std::string("ok"));
    CHECK_EQ(Data(r, "read", "onlydir\\deep\\inner.txt"), std::string("INNER"));
    CHECK_MSG(!sb.Exists(L"onlydir"), "looking at a folder does not create it");
}

ARCH_TEST("a new file in a folder that exists only in the update folder is created in the game folder", "[overload]")
{
    // The game sees the folder through GetFileAttributes/FindFirstFile and doesn't create it before saving
    Sandbox sb(arch);
    DeployOverloadTree(sb);
    sb.Write(L"update/onlydir/deep/inner.txt", "INNER");
    auto r = sb.Run({ L"write:onlydir\\deep\\saved.txt|SAVED", L"read:onlydir\\deep\\inner.txt" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(sb.Read(L"onlydir/deep/saved.txt"), std::string("SAVED"));
    CHECK(!sb.Exists(L"update/onlydir/deep/saved.txt"));
    CHECK_EQ(Data(r, "read", "onlydir\\deep\\inner.txt"), std::string("INNER"));
}

ARCH_TEST("a file in a folder that exists nowhere still fails with ERROR_PATH_NOT_FOUND", "[overload]")
{
    Sandbox sb(arch);
    DeployOverloadTree(sb);
    auto r = sb.Run({ L"write:nowhere\\saved.txt|SAVED" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Err(r, "write", "nowhere\\saved.txt"), std::to_string(ERROR_PATH_NOT_FOUND));
    CHECK(!sb.Exists(L"nowhere"));
}

ARCH_TEST("LoadLibrary prefers a DLL from the update folder", "[overload]")
{
    Sandbox sb(arch);
    DeployOverloadTree(sb);
    sb.Probe(L"helper.dll");
    sb.Probe(L"update/helper.dll");
    sb.Probe(L"helper2.dll");
    sb.Probe(L"update/helper2.dll");
    auto r = sb.Run({ L"loadlib:helper.dll", L"loadlibA:helper2.dll" });
    REQUIRE_HOST_OK(r);
    CHECK_MSG(IEquals(ut::widen(Data(r, "loadlib", "helper.dll", "module")), sb.P(L"update\\helper.dll").wstring()), Data(r, "loadlib", "helper.dll", "module"));
    CHECK_MSG(IEquals(ut::widen(Data(r, "loadlib", "helper2.dll", "module")), sb.P(L"update\\helper2.dll").wstring()), Data(r, "loadlib", "helper2.dll", "module"));
}

ARCH_TEST("writes to an overloaded file modify the update copy; new files are created in place", "[overload]")
{
    Sandbox sb(arch);
    DeployOverloadTree(sb);
    auto r = sb.Run({ L"write:data.txt|WRITTEN", L"write:brand_new.txt|NEW" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(sb.Read(L"update/data.txt"), std::string("WRITTEN"));
    CHECK_EQ(sb.Read(L"data.txt"), kOriginal);
    CHECK(sb.Exists(L"brand_new.txt"));
    CHECK(!sb.Exists(L"update/brand_new.txt"));
}

ARCH_TEST("GetOverloadPath and GetOverloadedFilePath report the active folder and overloaded paths", "[overload][api]")
{
    Sandbox sb(arch);
    DeployOverloadTree(sb);
    auto abs = sb.P(L"data.txt").wstring();
    auto r = sb.Run({ L"ovpath", L"ovfile:data.txt", L"ovfile:" + abs, L"ovfile:only_root.txt" });
    REQUIRE_HOST_OK(r);
    auto ov = r.Action("ovpath", "");
    REQUIRE(ov);
    CHECK_EQ(ov->get("ok"), std::string("1"));
    CHECK_MSG(IEquals(ut::widen(ov->get("value")), sb.P(L"update").wstring()), ov->get("value"));
    CHECK_MSG(IEquals(ut::widen(ov->get("valueA")), sb.P(L"update").wstring()), ov->get("valueA"));
    auto rel = r.Action("ovfile", "data.txt");
    CHECK_EQ(rel->get("value"), std::string("update\\data.txt"));
    CHECK_EQ(rel->get("valueA"), std::string("update\\data.txt"));
    auto a = r.Action("ovfile", ut::narrow(abs));
    CHECK_MSG(IEquals(ut::widen(a->get("value")), sb.P(L"update\\data.txt").wstring()), a->get("value"));
    CHECK_EQ(r.Action("ovfile", "only_root.txt")->get("ok"), std::string("0"));
}

ARCH_TEST("GetOverloadPath/GetOverloadedFilePath never write past out_size", "[overload][api]")
{
    Sandbox sb(arch);
    DeployOverloadTree(sb);
    auto r = sb.Run({ L"scenario:ovr_export_overflow" });
    REQUIRE_SCENARIO(r, "ovr_export_overflow");
}

ARCH_TEST("GetOverloadedFilePath returns a path usable from the caller's current directory", "[overload][api][cwd]")
{
    Sandbox sb(arch);
    DeployOverloadTree(sb);
    auto r = sb.Run({ L"scenario:ovr_relative_cwd" });
    REQUIRE_SCENARIO(r, "ovr_relative_cwd");
}

ARCH_TEST("without an update folder nothing is redirected and GetOverloadPath fails", "[overload][api]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Write(L"data.txt", kOriginal);
    auto r = sb.Run({ L"read:data.txt", L"ovpath", L"ovfile:data.txt" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Data(r, "read", "data.txt"), kOriginal);
    CHECK_EQ(r.Action("ovpath", "")->get("ok"), std::string("0"));
    CHECK_EQ(r.Action("ovfile", "data.txt")->get("ok"), std::string("0"));
}

ARCH_TEST("OverloadFromFolder selects a custom folder", "[overload][ini]")
{
    Sandbox sb(arch);
    DeployOverloadTree(sb);
    sb.Write(L"mods/data.txt", "MODS");
    sb.Ini(L"global.ini", L"FileLoader", L"OverloadFromFolder", L"mods");
    auto r = sb.Run({ L"read:data.txt", L"read:sub\\data.txt", L"ovpath" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Data(r, "read", "data.txt"), std::string("MODS"));
    CHECK_EQ(Data(r, "read", "sub\\data.txt"), std::string("ORIGINAL-SUB"));
    CHECK_MSG(IEquals(ut::widen(r.Action("ovpath", "")->get("value")), sb.P(L"mods").wstring()), r.Action("ovpath", "")->get("value"));
}

ARCH_TEST("OverloadFromFolder accepts an absolute folder outside the game directory", "[overload][ini]")
{
    Sandbox sb(arch);
    DeployOverloadTree(sb);
    WriteFileBytes(sb.root / L"external mods" / L"data.txt", "EXTERNAL");
    sb.Ini(L"global.ini", L"FileLoader", L"OverloadFromFolder", (sb.root / L"external mods").wstring());
    auto r = sb.Run({ L"read:data.txt" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Data(r, "read", "data.txt"), std::string("EXTERNAL"));
}

ARCH_TEST("an empty OverloadFromFolder value disables the update folder", "[overload][ini]")
{
    Sandbox sb(arch);
    DeployOverloadTree(sb);
    sb.Ini(L"global.ini", L"FileLoader", L"OverloadFromFolder", L"");
    auto r = sb.Run({ L"read:data.txt", L"ovpath" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Data(r, "read", "data.txt"), kOriginal);
    CHECK_EQ(r.Action("ovpath", "")->get("ok"), std::string("0"));
}

ARCH_TEST("when only one of several listed folders exists it is used without asking", "[overload][ini]")
{
    Sandbox sb(arch);
    DeployOverloadTree(sb);
    sb.Write(L"mods/data.txt", "MODS");
    sb.Ini(L"global.ini", L"FileLoader", L"OverloadFromFolder", L"missing1 | \"mods\" | missing2");
    auto r = sb.Run({ L"read:data.txt" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Data(r, "read", "data.txt"), std::string("MODS"));
    CHECK(r.dialogs.empty());
}

// --- folder selection dialog

namespace
{
    // task dialog, then the loader's own window
    constexpr bool kDialogStyles[] = { false, true };

    void UseDialogStyle(Sandbox& sb, bool modern)
    {
        sb.Ini(L"global.ini", L"GlobalSets", L"ModernUI", modern ? L"1" : L"0");
    }

    std::string StyleName(bool modern)
    {
        return modern ? "ModernUI=1" : "task dialog";
    }

    // Records the dialog and clicks <id>
    DialogHandler Click(int id, std::vector<DialogInfo>* seen = nullptr)
    {
        return [id, seen](const DialogInfo& d) {
            if (seen) seen->push_back(d);
            return DialogAction::Click(id);
        };
    }
}

ARCH_TEST("several existing folders show a selection dialog; the chosen folder is used", "[overload][dialog][ui]")
{
    for (bool modern : kDialogStyles)
        for (int choice : { 1000, 1001 })
        {
            INFO(StyleName(modern));
            Sandbox sb(arch);
            DeployOverloadTree(sb);
            UseDialogStyle(sb, modern);
            sb.Write(L"nightmare/data.txt", "NIGHTMARE");
            sb.Write(L"nightmare/update.txt", "Resident Evil 5 - Nightmare\r\nsecond line is ignored");
            sb.Probe(L"update/u.asi");
            sb.Probe(L"nightmare/n.asi");
            sb.Ini(L"global.ini", L"FileLoader", L"OverloadFromFolder", L"update | nightmare");
            RunOptions o;
            o.onDialog = Click(choice);
            auto r = sb.Run({ L"read:data.txt", L"ovpath" }, o);
            REQUIRE_HOST_OK(r);
            REQUIRE_EQ(r.dialogs.size(), (size_t)1);
            auto& d = r.dialogs.front();
            CHECK_EQ(d.isModern, modern);
            CHECK(d.title == L"ASI Loader");
            CHECK_MSG(d.Contains(L"Select Override (Update) Folder"), ut::narrow(d.AllText()));
            CHECK_MSG(d.Contains(L"Resident Evil 5 - Nightmare"), "update.txt provides the button caption");
            CHECK_MSG(!d.Contains(L"second line is ignored"), "only the first line of update.txt is used");
            INFO("dialog text: " + ut::narrow(d.AllText()));
            bool night = choice == 1001;
            CHECK_EQ(Data(r, "read", "data.txt"), night ? std::string("NIGHTMARE") : kUpdated);
            CHECK_MSG(IEquals(ut::widen(r.Action("ovpath", "")->get("value")), sb.P(night ? L"nightmare" : L"update").wstring()), r.Action("ovpath", "")->get("value"));
            CHECK_EQ(r.Inits("n.asi"), night ? 1 : 0);
            CHECK_EQ(r.Inits("u.asi"), night ? 0 : 1);
            ForgiveUserInterference(r);
        }
}

ARCH_TEST("the folder selection dialog closes by itself and selects the first folder", "[overload][dialog][ui][slow]")
{
    // Any mouse or keyboard input stops the countdown by design. Don't touch the machine during [ui] tests.
    for (bool modern : kDialogStyles)
    {
        INFO(StyleName(modern));
        Sandbox sb(arch);
        DeployOverloadTree(sb);
        UseDialogStyle(sb, modern);
        sb.Write(L"nightmare/data.txt", "NIGHTMARE");
        sb.Ini(L"global.ini", L"FileLoader", L"OverloadFromFolder", L"update | nightmare");
        RunOptions o;
        o.onDialog = [](const DialogInfo&) { return DialogAction::LeaveOpen(); };
        o.freezeCountdown = false;
        o.timeoutMs = 60000;
        auto r = sb.Run({ L"read:data.txt" }, o);
        if (r.userInput) SKIP("keyboard/mouse input during the test stops the dialog's countdown (by design)");
        REQUIRE_HOST_OK(r);
        REQUIRE_EQ(r.dialogs.size(), (size_t)1);
        CHECK_EQ(r.dialogs.front().isModern, modern);
        CHECK_EQ(Data(r, "read", "data.txt"), kUpdated);
    }
}

ARCH_TEST("the folder selection dialog auto-closes after the announced 10 seconds", "[overload][dialog][ui][slow]")
{
    // TDN_TIMER fires about every 200 ms, but the callback counted each one as a second
    for (bool modern : kDialogStyles)
    {
        INFO(StyleName(modern));
        Sandbox sb(arch);
        DeployOverloadTree(sb);
        UseDialogStyle(sb, modern);
        sb.Write(L"nightmare/data.txt", "NIGHTMARE");
        sb.Ini(L"global.ini", L"FileLoader", L"OverloadFromFolder", L"update | nightmare");
        RunOptions o;
        o.onDialog = [](const DialogInfo&) { return DialogAction::LeaveOpen(); };
        o.freezeCountdown = false;
        o.timeoutMs = 60000;
        auto r = sb.Run({ L"read:data.txt" }, o);
        if (r.userInput) SKIP("keyboard/mouse input during the test stops the dialog's countdown (by design)");
        REQUIRE_HOST_OK(r);
        REQUIRE_EQ(r.dialogs.size(), (size_t)1);
        auto& d = r.dialogs.front();
        REQUIRE_MSG(d.closedAt > 0, "the dialog did not close by itself");
        double shownFor = d.closedAt - d.shownAt;
        CHECK_MSG(shownFor >= 9.0 && shownFor <= 13.0, "the dialog was shown for " + std::to_string(shownFor) + " s");
    }
}

ARCH_TEST("'base < patch': selecting patch also activates base with lower priority", "[overload][dialog][ui][priority]")
{
    struct Case { int click; std::string both, baseOnly, patchOnly; };
    for (bool modern : kDialogStyles)
        for (auto c : { Case{ 1001, "PATCH", "BASE-ONLY", "PATCH-ONLY" }, Case{ 1000, "BASE", "BASE-ONLY", "" } })
        {
            INFO(StyleName(modern));
            Sandbox sb(arch);
            sb.Loader();
            sb.Host(L"dinput8");
            UseDialogStyle(sb, modern);
            sb.Write(L"both.txt", "ORIG");
            sb.Write(L"base_only.txt", "ORIG");
            sb.Write(L"patch_only.txt", "ORIG");
            sb.Write(L"base/both.txt", "BASE");
            sb.Write(L"base/base_only.txt", "BASE-ONLY");
            sb.Write(L"patch/both.txt", "PATCH");
            sb.Write(L"patch/patch_only.txt", "PATCH-ONLY");
            sb.Ini(L"global.ini", L"FileLoader", L"OverloadFromFolder", L"base < patch");
            RunOptions o;
            std::vector<DialogInfo> seen;
            o.onDialog = Click(c.click, &seen);
            auto r = sb.Run({ L"read:both.txt", L"read:base_only.txt", L"read:patch_only.txt" }, o);
            REQUIRE_HOST_OK(r);
            INFO("clicked " + std::to_string(c.click));
            REQUIRE_EQ(seen.size(), (size_t)1);
            if (modern) // UIA doesn't expose a task dialog command link's second line
                CHECK_MSG(seen[0].Contains(L"+ base"), "the patch choice shows that it includes base: " + ut::narrow(seen[0].AllText()));
            CHECK_EQ(Data(r, "read", "both.txt"), c.both);
            CHECK_EQ(Data(r, "read", "base_only.txt"), c.baseOnly);
            CHECK_EQ(Data(r, "read", "patch_only.txt"), c.patchOnly.empty() ? std::string("ORIG") : c.patchOnly);
            ForgiveUserInterference(r);
        }
}

ARCH_TEST("'main > addon': selecting main also activates addon with lower priority", "[overload][dialog][ui][priority]")
{
    struct Case { int click; std::string both, addonOnly; };
    for (bool modern : kDialogStyles)
        for (auto c : { Case{ 1000, "MAIN", "ADDON-ONLY" }, Case{ 1001, "ADDON", "ADDON-ONLY" } })
        {
            INFO(StyleName(modern));
            Sandbox sb(arch);
            sb.Loader();
            sb.Host(L"dinput8");
            UseDialogStyle(sb, modern);
            sb.Write(L"both.txt", "ORIG");
            sb.Write(L"addon_only.txt", "ORIG");
            sb.Write(L"main/both.txt", "MAIN");
            sb.Write(L"addon/both.txt", "ADDON");
            sb.Write(L"addon/addon_only.txt", "ADDON-ONLY");
            sb.Ini(L"global.ini", L"FileLoader", L"OverloadFromFolder", L"main > addon");
            RunOptions o;
            o.onDialog = Click(c.click);
            auto r = sb.Run({ L"read:both.txt", L"read:addon_only.txt" }, o);
            REQUIRE_HOST_OK(r);
            INFO("clicked " + std::to_string(c.click));
            CHECK_EQ(Data(r, "read", "both.txt"), c.both);
            CHECK_EQ(Data(r, "read", "addon_only.txt"), c.addonOnly);
            ForgiveUserInterference(r);
        }
}

ARCH_TEST("a UTF-8 update.txt caption is shown correctly in the selection dialog", "[overload][dialog][ui][unicode]")
{
    std::wstring caption = L"Кошмар — Nightmare édition";
    REQUIRE_MSG(caption[0] == L'\x041A', "the tests must be compiled as UTF-8 (/utf-8), or this literal is already garbled");
    for (bool modern : kDialogStyles)
    {
        INFO(StyleName(modern));
        Sandbox sb(arch);
        DeployOverloadTree(sb);
        UseDialogStyle(sb, modern);
        sb.Write(L"nightmare/update.txt", ut::narrow(caption));
        sb.Write(L"nightmare/data.txt", "NIGHTMARE");
        sb.Ini(L"global.ini", L"FileLoader", L"OverloadFromFolder", L"update | nightmare");
        auto r = sb.Run({ L"read:data.txt" });
        REQUIRE_HOST_OK(r);
        REQUIRE_EQ(r.dialogs.size(), (size_t)1);
        CHECK_EQ(r.dialogs.front().isModern, modern);
        CHECK_MSG(r.dialogs.front().Contains(caption), "dialog text: " + ut::narrow(r.dialogs.front().AllText()));
    }
}

ARCH_TEST("under the loader lock (DontLoadFromDllMain=0) the first folder is used without asking", "[overload][dialog][ui]")
{
    // No window is safe under the loader lock
    for (bool modern : kDialogStyles)
    {
        INFO(StyleName(modern));
        Sandbox sb(arch);
        DeployOverloadTree(sb);
        UseDialogStyle(sb, modern);
        sb.Ini(L"global.ini", L"GlobalSets", L"DontLoadFromDllMain", L"0");
        sb.Write(L"nightmare/data.txt", "NIGHTMARE");
        sb.Ini(L"global.ini", L"FileLoader", L"OverloadFromFolder", L"update | nightmare");
        auto r = sb.Run({ L"read:data.txt" });
        REQUIRE_HOST_OK(r);
        CHECK_EQ(r.dialogs.size(), (size_t)0);
        CHECK_EQ(Data(r, "read", "data.txt"), kUpdated);
    }
}

ARCH_TEST("the ModernUI dialog can be used with the keyboard", "[overload][dialog][ui][modern]")
{
    // Down, Down (wraps to the first), Down, Enter selects the second folder
    Sandbox sb(arch);
    DeployOverloadTree(sb);
    UseDialogStyle(sb, true);
    sb.Write(L"nightmare/data.txt", "NIGHTMARE");
    sb.Ini(L"global.ini", L"FileLoader", L"OverloadFromFolder", L"update | nightmare");
    RunOptions o;
    o.onDialog = [](const DialogInfo& d) {
        for (WPARAM key : { VK_DOWN, VK_DOWN, VK_DOWN, VK_RETURN })
        {
            PostMessageW(d.hwnd, WM_KEYDOWN, key, 0);
            PostMessageW(d.hwnd, WM_KEYUP, key, 0xC0000001);
        }
        return DialogAction::LeaveOpen();
    };
    auto r = sb.Run({ L"read:data.txt" }, o);
    REQUIRE_HOST_OK(r);
    REQUIRE_EQ(r.dialogs.size(), (size_t)1);
    CHECK(r.dialogs.front().isModern);
    CHECK_EQ(Data(r, "read", "data.txt"), std::string("NIGHTMARE"));
    ForgiveUserInterference(r);
}

// --- virtual files and paths

namespace
{
    struct VfsCase
    {
        const char* scenario;
        const char* title;
        const char* tags;
    };

    struct VfsRegistrar
    {
        VfsRegistrar()
        {
            const VfsCase cases[] = {
                { "vf_basic", "a virtual file is visible to every file API", "[vfs]" },
                { "vf_append", "adding to an existing virtual file appends; lower priorities are rejected", "[vfs]" },
                { "vf_access", "virtual files are read-only and cannot be re-created", "[vfs]" },
                { "vf_seek", "SetFilePointer(Ex) on virtual files behaves like on real files", "[vfs]" },
                { "vf_overlapped", "overlapped reads honour the offset and signal the event", "[vfs]" },
                { "vf_remove", "removed virtual files disappear", "[vfs]" },
                { "vf_shadow_real", "a virtual file shadows a real file until it is removed", "[vfs]" },
                { "vf_invalid_args", "invalid arguments are rejected", "[vfs]" },
                { "vf_large", "an 8 MiB virtual file reads back correctly, including random seeks", "[vfs]" },
                { "vf_threads", "several threads use their own virtual files concurrently", "[vfs][threads]" },
                { "vpath_basic", "a virtual path redirects a file to another file", "[vpath]" },
                { "vpath_priority", "virtual path priorities", "[vpath]" },
                { "vpath_vs_vfile", "virtual paths and virtual files replace each other by priority", "[vpath][vfs]" },
                { "vpath_chain", "virtual path chains resolve and cycles terminate", "[vpath]" },
                { "vpath_absolute_target", "virtual paths can point outside the game directory", "[vpath]" },
                { "vf_two_handles", "every handle of a virtual file has its own file position", "[vfs]" },
                { "vf_access_write_rights", "every write access right is denied on virtual files", "[vfs]" },
                { "vf_seek_negative_low", "SetFilePointer without a high part sign-extends negative distances", "[vfs]" },
                { "vf_seek_past_eof", "seeking past the end of a virtual file is allowed", "[vfs]" },
                { "vf_overlapped_eof", "overlapped reads past the end fail with ERROR_HANDLE_EOF", "[vfs]" },
                { "vf_readfileex", "ReadFileEx completion routines run as APCs on the calling thread", "[vfs]" },
                { "vf_mapping_protection", "CreateFileMapping on a virtual file honours the requested protection", "[vfs]" },
                { "vf_findfirst", "FindFirstFile finds virtual files", "[vfs]" },
                { "vf_storage_race", "removing or appending to a virtual file while other threads read it is safe", "[vfs][threads][stress]" },
                { "vpath_deadlock", "concurrent virtual path lookups and updates do not deadlock", "[vpath][threads][stress]" },
            };
            for (auto& c : cases)
                AddArchTest(c.title, c.tags, [c](const Arch& arch) {
                    Sandbox sb(arch);
                    sb.Loader();
                    sb.Host(L"dinput8");
                    RunOptions o;
                    o.timeoutMs = 60000;
                    auto r = sb.Run({ L"scenario:" + ut::widen(c.scenario) }, o);
                    REQUIRE_SCENARIO(r, c.scenario);
                });
        }
    } g_vfsRegistrar;
}

ARCH_TEST("virtual files work together with an active update folder and a run-time loaded loader", "[vfs][overload]")
{
    Sandbox sb(arch);
    DeployOverloadTree(sb);
    auto r = sb.Run({ L"scenario:vf_basic", L"read:data.txt" });
    REQUIRE_SCENARIO(r, "vf_basic");
    CHECK_EQ(Data(r, "read", "data.txt"), kUpdated);

    Sandbox sb2(arch);
    sb2.Loader(L"d3d9.dll");
    sb2.Host();
    auto r2 = sb2.Run({ L"load:d3d9.dll", L"trigger:Sleep", L"scenario:vf_basic", L"scenario:vpath_basic" });
    REQUIRE_SCENARIO(r2, "vf_basic");
    REQUIRE_SCENARIO(r2, "vpath_basic");
}

ARCH_TEST("the original test.bat flow: overload, virtual file append, zip and virtual path", "[overload][vfs][vpath][zip]")
{
    // The five directories of the old tests/test.bat
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Write(L"input.bin", "wrong.txt");
    sb.Write(L"update/input.bin", "update_test_passed.txt");
    sb.Write(L"input4.bin", "wrong4.txt");
    sb.Write(L"storage/input4.bin", "virtual_path_test_passed.txt");
    auto r = sb.Run({ L"read:input.bin", L"vadd:input2.bin|virtual_file_|1000", L"vadd:input2.bin|test_passed.txt|1000", L"read:input2.bin",
                      L"vpath:input4.bin|storage/input4.bin|1001", L"read:input4.bin", L"vpathrm:input4.bin", L"read:input4.bin" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Data(r, "read", "input.bin"), std::string("update_test_passed.txt"));
    CHECK_EQ(Data(r, "read", "input2.bin"), std::string("virtual_file_test_passed.txt"));
    auto reads = r.Events("host", "read");
    std::vector<std::string> input4;
    for (auto rec : reads) if (rec->get("arg") == "input4.bin") input4.push_back(rec->get("data"));
    REQUIRE_EQ(input4.size(), (size_t)2);
    CHECK_EQ(input4[0], std::string("virtual_path_test_passed.txt"));
    CHECK_EQ(input4[1], std::string("wrong4.txt"));
}

// --- performance

namespace
{
    // Median of 5 system-wide thread snapshots, which MH_EnableHook takes to freeze threads
    double ThreadSnapshotSeconds()
    {
        std::vector<double> v;
        for (int i = 0; i < 5; ++i)
        {
            LARGE_INTEGER f, a, b;
            QueryPerformanceFrequency(&f);
            QueryPerformanceCounter(&a);
            HANDLE h = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
            THREADENTRY32 te{ sizeof(te) };
            if (Thread32First(h, &te))
                while (Thread32Next(h, &te)) {}
            CloseHandle(h);
            QueryPerformanceCounter(&b);
            v.push_back(double(b.QuadPart - a.QuadPart) / f.QuadPart);
        }
        std::sort(v.begin(), v.end());
        return v[2];
    }

    double FastestRun(Sandbox& sb, int runs = 3)
    {
        double best = 1e9;
        for (int i = 0; i < runs; ++i)
        {
            auto r = sb.Run({});
            REQUIRE_HOST_OK(r);
            best = (std::min)(best, r.seconds);
        }
        return best;
    }
}

ARCH_TEST("an update folder does not add seconds to game start-up", "[overload][perf]")
{
    // Each overload hook got its own MH_EnableHook, each taking a system-wide thread snapshot.
    // MH_QueueEnableHook + MH_ApplyQueued freezes once, costing a few snapshots.
    double snapshot = ThreadSnapshotSeconds();
    Sandbox plain(arch);
    plain.Loader();
    plain.Host(L"dinput8");
    double base = FastestRun(plain);

    Sandbox withUpdate(arch);
    withUpdate.Loader();
    withUpdate.Host(L"dinput8");
    withUpdate.Mkdir(L"update");
    double slow = FastestRun(withUpdate);

    double overhead = slow - base;
    double allowed = 0.25 + 4 * snapshot;
    char msg[256];
    sprintf_s(msg, "start-up %.0f ms without / %.0f ms with an empty update folder: overhead %.0f ms, allowed %.0f ms (one thread snapshot = %.1f ms)",
        base * 1000, slow * 1000, overhead * 1000, allowed * 1000, snapshot * 1000);
    INFO(msg);
    CHECK_MSG(overhead <= allowed, msg);
}
