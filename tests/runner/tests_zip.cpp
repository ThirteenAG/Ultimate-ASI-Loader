// Zip packages (packages\*.zip, multi-part archives) served as virtual files
#include "framework.hpp"
#include "../common/pattern.hpp"
#include <algorithm>

using namespace runner;

namespace
{
    std::string Data(const RunResult& r, const std::string& arg, const std::string& ev = "read", const std::string& field = "data")
    {
        auto a = r.Action(ev, arg);
        if (!a) { FAIL_CHECK("no result for " + ev + ":" + arg + "\n      " + r.Dump()); return {}; }
        if (a->get("ok") != "1") { FAIL_CHECK(ev + ":" + arg + " failed with error " + a->get("err")); return {}; }
        return a->get(field);
    }

    std::vector<std::pair<std::string, std::string>> StandardEntries()
    {
        return {
            { "update/", "" },
            { "update/zipfile.txt", "FROMZIP" },
            { "update/sub/deep.txt", "DEEP" },
            { "update/big.bin", ualtest::Pattern(300000, 42) },
            { "update/update.txt", "Zipped Update" },
        };
    }

    void WriteSplit(const Sandbox& sb, const std::string& zip, const std::vector<std::wstring>& names)
    {
        size_t parts = names.size();
        size_t chunk = (zip.size() + parts - 1) / parts;
        for (size_t i = 0; i < parts; ++i)
        {
            size_t off = i * chunk;
            sb.Write(L"packages\\" + names[i], off < zip.size() ? zip.substr(off, chunk) : std::string());
        }
    }
}

ARCH_TEST("files in packages\\*.zip are served from the zip when the update folder does not exist", "[zip]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Zip(L"packages/test.zip", StandardEntries());
    sb.Write(L"zipfile.txt", "ORIGINAL");
    auto r = sb.Run({ L"read:zipfile.txt", L"readA:sub\\deep.txt", L"readstd:big.bin", L"attrex:zipfile.txt", L"attr:zipfile.txt", L"read2:zipfile.txt" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Data(r, "zipfile.txt"), std::string("FROMZIP"));
    CHECK_EQ(Data(r, "sub\\deep.txt"), std::string("DEEP"));
    CHECK_MSG(Data(r, "big.bin") == ualtest::Pattern(300000, 42), "deflated 300 KB file round-trips");
    CHECK_EQ(Data(r, "zipfile.txt", "attr", "size"), std::string("7"));
    for (auto rec : r.Events("host", "read"))
        CHECK_MSG(rec->get("ok") == "1", rec->get("api") + " " + rec->get("arg") + " failed: " + rec->get("err"));
}

ARCH_TEST("stored (uncompressed) zip entries are served", "[zip]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Zip(L"packages/stored.zip", { { "update/stored.txt", "STORED-DATA" } }, true);
    auto r = sb.Run({ L"read:stored.txt" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Data(r, "stored.txt"), std::string("STORED-DATA"));
}

ARCH_TEST("an empty file inside a zip package reads as an empty file", "[zip]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Zip(L"packages/test.zip", { { "update/empty.txt", "" }, { "update/zipfile.txt", "FROMZIP" } });
    auto r = sb.Run({ L"read:empty.txt", L"attrex:empty.txt" });
    REQUIRE_HOST_OK(r);
    auto a = r.Action("read", "empty.txt");
    REQUIRE(a);
    CHECK_MSG(a->get("ok") == "1", "reading an empty zipped file failed with error " + a->get("err"));
    CHECK_EQ(a->get("size"), std::string("0"));
}

ARCH_TEST("an update folder and a zip package for it are both used, files in the folder win", "[zip]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    auto entries = StandardEntries();
    entries.push_back({ "update/shared.txt", "ZIP-COPY" });
    sb.Zip(L"packages/test.zip", entries);
    sb.Write(L"update/real.txt", "REAL-UPDATE");
    sb.Write(L"update/shared.txt", "FOLDER-COPY");
    sb.Write(L"shared.txt", "ORIGINAL");
    auto r = sb.Run({ L"read:real.txt", L"read:zipfile.txt", L"read:sub\\deep.txt", L"read:shared.txt" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Data(r, "real.txt"), std::string("REAL-UPDATE"));
    CHECK_EQ(Data(r, "zipfile.txt"), std::string("FROMZIP"));
    CHECK_EQ(Data(r, "sub\\deep.txt"), std::string("DEEP"));
    CHECK_EQ(Data(r, "shared.txt"), std::string("FOLDER-COPY"));
}

ARCH_TEST("an empty update folder does not hide its zip package", "[zip]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Zip(L"packages/test.zip", StandardEntries());
    sb.Mkdir(L"update");
    auto r = sb.Run({ L"read:zipfile.txt" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Data(r, "zipfile.txt"), std::string("FROMZIP"));
}

// What plugins see when they read the update folder itself (GetOverloadPath)
ARCH_TEST("inside an update folder with a zip package, zip files appear next to the folder's own", "[zip]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    auto entries = StandardEntries();
    entries.push_back({ "update/shared.txt", "ZIP-COPY" });
    sb.Zip(L"packages/test.zip", entries);
    sb.Write(L"update/real.txt", "REAL-UPDATE");
    sb.Write(L"update/shared.txt", "FOLDER-COPY");
    auto r = sb.Run({ L"read:update\\zipfile.txt", L"read:update\\shared.txt", L"read:update\\sub\\deep.txt", L"find:update\\*.txt" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Data(r, "update\\zipfile.txt"), std::string("FROMZIP"));
    CHECK_EQ(Data(r, "update\\shared.txt"), std::string("FOLDER-COPY"));
    CHECK_EQ(Data(r, "update\\sub\\deep.txt"), std::string("DEEP"));
    auto find = r.Action("find", "update\\*.txt");
    REQUIRE(find != nullptr);
    std::vector<std::string> names;
    std::string listed = find->get("names") + ";";
    for (size_t start = 0, end; (end = listed.find(';', start)) != std::string::npos; start = end + 1)
        if (end > start) names.push_back(listed.substr(start, listed.find(':', start) - start));
    std::sort(names.begin(), names.end());
    std::string joined;
    for (const auto& n : names) joined += (joined.empty() ? "" : ",") + n;
    CHECK_EQ(joined, std::string("real.txt,shared.txt,update.txt,zipfile.txt"));
    CHECK_MSG(find->get("names").find("shared.txt:11") != std::string::npos, "shared.txt is listed once, with the folder copy's size: " + find->get("names"));
}

ARCH_TEST("when several packages provide the same file the alphabetically last one wins", "[zip]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Zip(L"packages/a.zip", { { "update/same.txt", "FROM-A" }, { "update/a_only.txt", "A" } });
    sb.Zip(L"packages/b.zip", { { "update/same.txt", "FROM-B" }, { "update/b_only.txt", "B" } });
    auto r = sb.Run({ L"read:same.txt", L"read:a_only.txt", L"read:b_only.txt" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Data(r, "same.txt"), std::string("FROM-B"));
    CHECK_EQ(Data(r, "a_only.txt"), std::string("A"));
    CHECK_EQ(Data(r, "b_only.txt"), std::string("B"));
}

ARCH_TEST("multi-part archives (name.zip.001, .002, ...) are joined", "[zip][multipart]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    WriteSplit(sb, BuildZip(StandardEntries()), { L"test.zip.001", L"test.zip.002", L"test.zip.003" });
    auto r = sb.Run({ L"read:zipfile.txt", L"read:big.bin" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Data(r, "zipfile.txt"), std::string("FROMZIP"));
    CHECK_MSG(Data(r, "big.bin") == ualtest::Pattern(300000, 42), "content spanning several parts");
}

ARCH_TEST("multi-part archives with more than 9 unpadded parts (.1 ... .11) are joined in numeric order", "[zip][multipart]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    std::vector<std::wstring> names;
    for (int i = 1; i <= 11; ++i) names.push_back(L"test.zip." + std::to_wstring(i));
    WriteSplit(sb, BuildZip(StandardEntries()), names);
    auto r = sb.Run({ L"read:zipfile.txt", L"read:big.bin" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Data(r, "zipfile.txt"), std::string("FROMZIP"));
}

ARCH_TEST("an archive whose name contains \".zip\" is not merged with another archive", "[zip]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Zip(L"packages/pack.zip", { { "update/p1.txt", "P1" } });
    sb.Zip(L"packages/pack.zip-extras.zip", { { "update/p2.txt", "P2" } });
    auto r = sb.Run({ L"read:p1.txt", L"read:p2.txt" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Data(r, "p1.txt"), std::string("P1"));
    CHECK_EQ(Data(r, "p2.txt"), std::string("P2"));
}

ARCH_TEST("the zip root folder is matched case-insensitively", "[zip]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Zip(L"packages/test.zip", { { "Update/zipfile.txt", "FROMZIP" } });
    auto r = sb.Run({ L"read:zipfile.txt" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Data(r, "zipfile.txt"), std::string("FROMZIP"));
}

ARCH_TEST("a zip package can provide a custom OverloadFromFolder folder", "[zip][ini]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Zip(L"packages/mod.zip", { { "modzip/zipfile.txt", "MODZIP" } });
    sb.Ini(L"global.ini", L"FileLoader", L"OverloadFromFolder", L"modzip");
    auto r = sb.Run({ L"read:zipfile.txt", L"ovpath" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Data(r, "zipfile.txt"), std::string("MODZIP"));
    CHECK_MSG(IEquals(ut::widen(r.Action("ovpath", "")->get("value")), sb.P(L"modzip").wstring()), "GetOverloadPath reports the zip folder name");
}

ARCH_TEST("a corrupt package is ignored and other packages still work", "[zip][errors]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Write(L"packages/a_corrupt.zip", "PK\x03\x04 this is not a zip file at all");
    sb.Write(L"packages/b_empty.zip", "");
    sb.Zip(L"packages/c_good.zip", { { "update/zipfile.txt", "FROMZIP" } });
    auto r = sb.Run({ L"read:zipfile.txt" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Data(r, "zipfile.txt"), std::string("FROMZIP"));
}

ARCH_TEST("plugins inside a zip package are not loaded", "[zip][loading]")
{
    auto probe = ReadFileBytes(arch.probe());
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Zip(L"packages/test.zip", { { "update/zipped.asi", probe }, { "update/zipfile.txt", "FROMZIP" } });
    auto r = sb.Run({ L"read:zipfile.txt" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Data(r, "zipfile.txt"), std::string("FROMZIP"));
    CHECK_EQ(r.Inits(), 0);
}

ARCH_TEST("a zip-only folder is offered in the selection dialog with its update.txt caption and a [ZIP] tag", "[zip][dialog][ui]")
{
    for (bool modern : { false, true }) // task dialog, ModernUI=1
    {
        INFO(modern ? "ModernUI=1" : "task dialog");
        Sandbox sb(arch);
        sb.Loader();
        sb.Host(L"dinput8");
        sb.Ini(L"global.ini", L"GlobalSets", L"ModernUI", modern ? L"1" : L"0");
        sb.Write(L"update/data.txt", "UPDATE");
        sb.Zip(L"packages/night.zip", { { "nightmare/update.txt", "Zipped Nightmare" }, { "nightmare/data.txt", "ZIP-NIGHTMARE" } });
        sb.Ini(L"global.ini", L"FileLoader", L"OverloadFromFolder", L"update | nightmare");
        RunOptions o;
        o.onDialog = [](const DialogInfo&) { return DialogAction::Click(1001); };
        auto r = sb.Run({ L"read:data.txt" }, o);
        REQUIRE_HOST_OK(r);
        REQUIRE_EQ(r.dialogs.size(), (size_t)1);
        CHECK_EQ(r.dialogs.front().isModern, modern);
        CHECK_MSG(r.dialogs.front().Contains(L"Zipped Nightmare [ZIP]"), ut::narrow(r.dialogs.front().AllText()));
        CHECK_EQ(Data(r, "data.txt"), std::string("ZIP-NIGHTMARE"));
        ForgiveUserInterference(r);
    }
}

ARCH_TEST("a plugin can override a zip-provided file with a higher priority virtual file", "[zip][vfs]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Zip(L"packages/test.zip", StandardEntries());
    auto r = sb.Run({ L"scenario:zip_vf_override" });
    REQUIRE_SCENARIO(r, "zip_vf_override");
}

ARCH_TEST("concurrent first reads of zip-backed files return correct data", "[zip][threads][stress]")
{
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    std::vector<std::pair<std::string, std::string>> entries;
    for (int i = 0; i < ualtest::kZipConcurrentFiles; ++i)
        entries.push_back({ "update/zc_" + std::to_string(i) + ".bin", ualtest::Pattern(ualtest::kZipConcurrentFileSize, 500 + i) });
    sb.Zip(L"packages/concurrent.zip", entries);
    RunOptions o;
    o.timeoutMs = 120000;
    auto r = sb.Run({ L"scenario:zip_concurrent" }, o);
    REQUIRE_SCENARIO(r, "zip_concurrent");
}

ARCH_TEST("zip packages work in a game directory with characters outside the ANSI code page", "[zip][unicode]")
{
    if (GetACP() == CP_UTF8) SKIP("the ANSI code page is UTF-8, every path is representable");
    Sandbox sb(arch, L"ゲーム Игра");
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Zip(L"packages/test.zip", StandardEntries());
    auto r = sb.Run({ L"read:zipfile.txt" });
    REQUIRE_HOST_OK(r);
    CHECK_EQ(Data(r, "zipfile.txt"), std::string("FROMZIP"));
}

// GTA IV FusionFix scans the update folder for "<name>.img" folders (scenario update_scan)
ARCH_TEST("update folder scanning works the same for a folder, a zip package and a split zip package", "[zip][overload][fusionfix]")
{
    const std::vector<std::pair<std::string, std::string>> tree = {
        { "pc/data/gtxd.img/a.wtd", "A-WTD" },
        { "pc/data/gtxd.img/sub/b.wtd", "B-WTD" },
        { "common/new.img/c.wdr", "C-WDR" },
        { "plain.txt", "PLAIN" },
        { "update.txt", "Update" },
    };
    for (const char* layout : { "folder", "zip", "split", "zip-case" })
    {
        INFO(std::string("layout: ") + layout);
        Sandbox sb(arch);
        sb.Loader();
        sb.Host(L"dinput8");
        sb.Write(L"pc/data/gtxd.img", "ORIGINAL-IMG");
        std::string l = layout;
        std::vector<std::pair<std::string, std::string>> entries;
        for (auto& [p, d] : tree) entries.emplace_back((l == "zip-case" ? "Update/" : "update/") + p, d);
        if (l == "folder")
            for (auto& [p, d] : entries) sb.Write(ut::widen(p), d);
        else if (l == "split")
            WriteSplit(sb, BuildZip(entries), { L"mod.zip.1", L"mod.zip.2", L"mod.zip.3" });
        else
            sb.Zip(L"packages/mod.zip", entries);
        auto r = sb.Run({ L"scenario:update_scan" });
        REQUIRE_SCENARIO(r, "update_scan");
        CHECK_MSG(!sb.Exists(L"update/common/new.img") || l == "folder", "nothing is extracted to disk");
    }
}
