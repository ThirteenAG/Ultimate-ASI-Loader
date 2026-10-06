// White-box tests of source/loader, compiled in without main.cpp. DllMain never runs and
// no hooks are installed; tests/runner covers the hooked behaviour.
#include "core/loader.hpp"
#include "core/paths.hpp"
#include "core/pe.hpp"
#include "core/strings.hpp"
#include "proxy/proxy.hpp"
#include "startup/imports.hpp"
#include "vfs/internal.hpp"
#include "vfs/packages.hpp"
#include "vfs/spec.hpp"
#include "../common/testlib.hpp"

#include <miniz.h>
#include <algorithm>
#include <fstream>
#include <map>
#include <thread>

extern "C"
{
    bool WINAPI GetOverloadPathW(wchar_t* out, size_t outSize);
    bool WINAPI GetOverloadPathA(char* out, size_t outSize);
    bool WINAPI GetOverloadedFilePathW(const wchar_t* file, wchar_t* out, size_t outSize);
}

namespace fs = std::filesystem;
using namespace ual;
using namespace ual::vfs;

int wmain(int argc, wchar_t** argv)
{
    return ut::run(argc, argv, "unit");
}

namespace
{
    struct TempDir
    {
        fs::path p;
        TempDir()
        {
            static int n = 0;
            p = fs::temp_directory_path() / L"ual-unit" / (std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(++n));
            std::error_code ec;
            fs::remove_all(p, ec);
            fs::create_directories(p);
        }
        ~TempDir()
        {
            std::error_code ec;
            if (!ut::options().keep) fs::remove_all(p, ec);
        }
        fs::path operator/(const fs::path& rel) const { return p / rel; }
    };

    void WriteBytes(const fs::path& p, const std::string& s)
    {
        fs::create_directories(p.parent_path());
        std::ofstream f(p, std::ios::binary);
        f.write(s.data(), (std::streamsize)s.size());
    }

    // level 0 = stored
    std::string BuildZip(const std::vector<std::pair<std::string, std::string>>& entries, int level = MZ_BEST_COMPRESSION)
    {
        mz_zip_archive z{};
        mz_zip_writer_init_heap(&z, 0, 0);
        for (auto& [name, data] : entries)
            mz_zip_writer_add_mem(&z, name.c_str(), data.data(), data.size(), level);
        void* buf = nullptr;
        size_t size = 0;
        mz_zip_writer_finalize_heap_archive(&z, &buf, &size);
        std::string out(static_cast<char*>(buf), size);
        mz_free(buf);
        mz_zip_writer_end(&z);
        return out;
    }

    std::wstring Key(const fs::path& p)
    {
        std::wstring k;
        if (!MakeKey(p.c_str(), k)) return L"<none>";
        return k;
    }

    std::string ReadAll(FileData& d)
    {
        std::string s((size_t)d.Size(), '\0');
        DWORD n = 0;
        if (!d.Read(0, s.data(), (DWORD)s.size(), n)) return "<error>";
        s.resize(n);
        return s;
    }

    const Archive::Entry* FindEntry(const Archive& a, const wchar_t* path)
    {
        for (auto& e : a.Entries())
            if (IEquals(e.path, path)) return &e;
        return nullptr;
    }

    std::vector<std::wstring> Names(const OverloadSpec& s, const std::vector<size_t>& idx)
    {
        std::vector<std::wstring> r;
        for (size_t i : idx) r.push_back(s.folders[i]);
        return r;
    }

    size_t Rank(const OverloadSpec& s, const wchar_t* folder) // 0 = highest priority
    {
        for (size_t r = 0; r < s.priorityOrder.size(); ++r)
            if (s.folders[s.priorityOrder[r]] == folder) return r;
        FAIL("folder " + ut::narrow(folder) + " not found");
        return SIZE_MAX;
    }
}

// --- strings

TEST_CASE("IEquals / IStartsWith / IEndsWith ignore case, including non-ASCII", "[strings]")
{
    CHECK(IEquals(L"dinput8.dll", L"DINPUT8.DLL"));
    CHECK(!IEquals(L"d3d9.dll", L"d3d9.dl"));
    CHECK(!IEquals(L"d3d9.dll", L"d3d8.dll"));
    CHECK(IEquals(L"", L""));
    CHECK(IEquals(L"\u041A\u043E\u0448", L"\u043A\u041E\u0428"));
    CHECK(IStartsWith(L"Update\\x", L"update"));
    CHECK(!IStartsWith(L"upd", L"update"));
    CHECK(IEndsWith(L"name.ZIP", L".zip"));
    CHECK_EQ(ToLower(L"A\u00C4\u0416"), std::wstring(L"a\u00E4\u0436"));
}

TEST_CASE("UTF-8 / ANSI conversions and text decoding", "[strings]")
{
    CHECK_EQ(Utf8ToWide(""), std::wstring());
    CHECK_EQ(Utf8ToWide("\xD0\x9A\xD0\xBE\xD1\x88\xD0\xBC\xD0\xB0\xD1\x80 \xE2\x80\x94 x"), std::wstring(L"\u041A\u043E\u0448\u043C\u0430\u0440 \u2014 x"));
    CHECK_EQ(WideToUtf8(L"\u2014"), std::string("\xE2\x80\x94"));
    CHECK_EQ(DecodeText("\xEF\xBB\xBFplain"), std::wstring(L"plain"));
    CHECK_EQ(DecodeText("caf\xC3\xA9"), std::wstring(L"caf\u00E9"));
    CHECK_EQ(DecodeText(std::string("\xFF\xFEh\0i\0", 6)), std::wstring(L"hi"));
    CHECK_EQ(DecodeText(std::string("\xFE\xFF\0h\0i", 6)), std::wstring(L"hi"));
    CHECK_EQ(DecodeText("caf\xE9"), AnsiToWide("caf\xE9")); // invalid UTF-8 falls back to the ANSI code page
}

TEST_CASE("Trim, Unquote and SplitList", "[strings]")
{
    CHECK_EQ(Trim(L" \t a b \r\n"), std::wstring(L"a b"));
    CHECK_EQ(Unquote(L"\"my mods\""), std::wstring(L"my mods"));
    CHECK_EQ(Unquote(L"\"unbalanced"), std::wstring(L"\"unbalanced"));
    CHECK(SplitList(L" a | b|  \"c d\" | | A ") == (std::vector<std::wstring>{ L"a", L"b", L"c d" }));
    CHECK(SplitList(L"").empty());
}

TEST_CASE("LexicallyRelativeCaseIns computes relative paths ignoring case", "[strings][paths]")
{
    CHECK_EQ(LexicallyRelativeCaseIns(L"C:\\Game\\Data\\x.txt", L"c:\\game").wstring(), std::wstring(L"Data\\x.txt"));
    CHECK_EQ(LexicallyRelativeCaseIns(L"C:\\Game", L"C:\\GAME").wstring(), std::wstring(L"."));
    CHECK_EQ(LexicallyRelativeCaseIns(L"C:\\Other\\x.txt", L"C:\\Game").wstring(), std::wstring(L"..\\Other\\x.txt"));
    CHECK_EQ(LexicallyRelativeCaseIns(L"C:\\x.txt", L"C:\\Game\\Bin").wstring(), std::wstring(L"..\\..\\x.txt"));
    CHECK_EQ(LexicallyRelativeCaseIns(L"D:\\Game\\x.txt", L"C:\\Game").wstring(), std::wstring());
    CHECK_EQ(LexicallyRelativeCaseIns(L"Game\\x.txt", L"C:\\Game").wstring(), std::wstring());
}

// --- ini files

TEST_CASE("IniInt over several files: the last file that has the key wins", "[ini]")
{
    TempDir t;
    auto a = (t / L"a.ini").wstring(), b = (t / L"b.ini").wstring(), c = (t / L"c.ini").wstring(), missing = (t / L"missing.ini").wstring();
    WritePrivateProfileStringW(L"GlobalSets", L"LoadPlugins", L"0", a.c_str());
    WritePrivateProfileStringW(L"GlobalSets", L"Other", L"7", b.c_str());
    WritePrivateProfileStringW(L"GlobalSets", L"LoadPlugins", L"5", c.c_str());
    CHECK_EQ(IniInt({ a, b, missing }, L"GlobalSets", L"LoadPlugins", 1), 0);
    CHECK_EQ(IniInt({ a, b, c }, L"GlobalSets", L"LoadPlugins", 1), 5);
    CHECK_EQ(IniInt({ missing }, L"globalsets", L"loadplugins", 1), 1);
    CHECK_EQ(IniInt({}, L"GlobalSets", L"LoadPlugins", 3), 3);
}

TEST_CASE("IniString over several files: the last file that has the key wins, empty values count", "[ini]")
{
    TempDir t;
    auto a = (t / L"a.ini").wstring(), b = (t / L"b.ini").wstring(), c = (t / L"c.ini").wstring();
    WritePrivateProfileStringW(L"FileLoader", L"OverloadFromFolder", L"update | mods", a.c_str());
    WritePrivateProfileStringW(L"FileLoader", L"Other", L"x", b.c_str());
    WritePrivateProfileStringW(L"FileLoader", L"OverloadFromFolder", L"", c.c_str());
    CHECK_EQ(IniString({ a, b }, L"FileLoader", L"OverloadFromFolder", L"update"), std::wstring(L"update | mods"));
    CHECK_EQ(IniString({ b }, L"FileLoader", L"OverloadFromFolder", L"update"), std::wstring(L"update"));
    CHECK_EQ(IniString({ a, c }, L"FileLoader", L"OverloadFromFolder", L"update"), std::wstring());
}

TEST_CASE("IniString returns values longer than MAX_PATH unchanged", "[ini]")
{
    TempDir t;
    auto a = (t / L"a.ini").wstring();
    std::wstring value;
    for (int i = 0; i < 400; ++i) value += L"folder_" + std::to_wstring(i) + L" | ";
    value += L"last";
    REQUIRE(value.size() > 2048);
    WritePrivateProfileStringW(L"GlobalSets", L"LoadExtraPlugins", value.c_str(), a.c_str());
    auto got = IniString({ a }, L"GlobalSets", L"LoadExtraPlugins", L"");
    CHECK_EQ(got.size(), value.size());
    CHECK(got == value);
}

// --- OverloadFromFolder

TEST_CASE("OverloadSpec: separators, trimming, quotes and duplicates", "[overload][parse]")
{
    CHECK(OverloadSpec::Parse(L"").folders.empty());
    CHECK(OverloadSpec::Parse(L"  |  | ").folders.empty());

    auto one = OverloadSpec::Parse(L"update");
    REQUIRE_EQ(one.folders.size(), (size_t)1);
    CHECK_EQ(one.folders[0], std::wstring(L"update"));
    CHECK(one.overrides.empty());

    auto three = OverloadSpec::Parse(L"  update | nightmare\t|mods  ");
    CHECK(three.folders == (std::vector<std::wstring>{ L"update", L"nightmare", L"mods" }));
    CHECK(three.priorityOrder == (std::vector<size_t>{ 0, 1, 2 })); // earlier = higher

    auto quoted = OverloadSpec::Parse(L"\"my mods\" | \"C:\\Program Files\\x\"");
    CHECK(quoted.folders == (std::vector<std::wstring>{ L"my mods", L"C:\\Program Files\\x" }));

    CHECK_EQ(OverloadSpec::Parse(L"a | b | A | b").folders.size(), (size_t)2);
}

TEST_CASE("OverloadSpec: 'a < b' and 'a > b' relations", "[overload][parse][priority]")
{
    auto lt = OverloadSpec::Parse(L"base < patch");
    CHECK(Rank(lt, L"patch") < Rank(lt, L"base"));
    auto gt = OverloadSpec::Parse(L"addon | main > addon");
    CHECK(Rank(gt, L"main") < Rank(gt, L"addon"));
    auto chainLt = OverloadSpec::Parse(L"a < b < c");
    CHECK(Rank(chainLt, L"c") < Rank(chainLt, L"b"));
    CHECK(Rank(chainLt, L"b") < Rank(chainLt, L"a"));
    auto chainGt = OverloadSpec::Parse(L"a > b > c");
    CHECK(Rank(chainGt, L"a") < Rank(chainGt, L"b"));
    CHECK(Rank(chainGt, L"b") < Rank(chainGt, L"c"));
}

TEST_CASE("a folder used with both '<' and '>' keeps both relations", "[overload][parse][priority]")
{
    auto s = OverloadSpec::Parse(L"a < b | a > c");
    CHECK(Rank(s, L"b") < Rank(s, L"a"));
    CHECK(Rank(s, L"a") < Rank(s, L"c"));
    CHECK(Names(s, s.Activate(1)) == (std::vector<std::wstring>{ L"b", L"a", L"c" }));
}

TEST_CASE("priorities satisfy every relation of a dependency chain written in any order", "[overload][parse][priority]")
{
    auto s = OverloadSpec::Parse(L"p < q | t < s | s < r | r < p");
    CHECK(Rank(s, L"q") < Rank(s, L"p"));
    CHECK(Rank(s, L"p") < Rank(s, L"r"));
    CHECK(Rank(s, L"r") < Rank(s, L"s"));
    CHECK(Rank(s, L"s") < Rank(s, L"t"));
}

TEST_CASE("OverloadSpec::Activate follows relations and orders by priority", "[overload][priority]")
{
    auto single = OverloadSpec::Parse(L"update | nightmare");
    CHECK(Names(single, single.Activate(1)) == std::vector<std::wstring>{ L"nightmare" });

    auto lt = OverloadSpec::Parse(L"base < patch");
    CHECK(Names(lt, lt.Activate(1)) == (std::vector<std::wstring>{ L"patch", L"base" }));
    CHECK(Names(lt, lt.Activate(0)) == std::vector<std::wstring>{ L"base" });

    auto gt = OverloadSpec::Parse(L"main > addon");
    CHECK(Names(gt, gt.Activate(0)) == (std::vector<std::wstring>{ L"main", L"addon" }));
    CHECK(Names(gt, gt.Activate(1)) == std::vector<std::wstring>{ L"addon" });

    auto chain = OverloadSpec::Parse(L"a > b > c");
    CHECK(Names(chain, chain.Activate(0)) == (std::vector<std::wstring>{ L"a", L"b", L"c" }));

    auto cyc = OverloadSpec::Parse(L"x < y | y < x"); // the relation closing the cycle is dropped
    CHECK_EQ(cyc.priorityOrder.size(), (size_t)2);
    CHECK_EQ(cyc.Activate(1).size(), (size_t)2);
    CHECK_EQ(cyc.Activate(0).size(), (size_t)1);
}

// --- keys

TEST_CASE("MakeKey: lower case paths relative to the game folder", "[vfs][keys]")
{
    TempDir t;
    auto game = t / L"Game";
    fs::create_directories(game);
    InitKeys(game.wstring() + L"\\");
    CHECK_EQ(Key(game / L"Data/File.TXT"), std::wstring(L"data\\file.txt"));
    CHECK_EQ(Key(game / L"a\\..\\B.bin"), std::wstring(L"b.bin"));
    CHECK_EQ(Key(game / L"dir\\"), std::wstring(L"dir"));
    CHECK_EQ(Key(game), std::wstring());
    CHECK_EQ(Key(L"\\\\?\\" + (game / L"X.bin").wstring()), std::wstring(L"x.bin"));

    wchar_t old[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, old);
    SetCurrentDirectoryW(game.c_str());
    CHECK_EQ(Key(L"Sub/X.bin"), std::wstring(L"sub\\x.bin"));
    SetCurrentDirectoryW(old);

    // same drive: the part after the common ancestor
    CHECK_EQ(Key(t / L"Data/x.txt"), std::wstring(L"data\\x.txt"));
    // ancestors of the game folder are not the game folder (their listings must not show the update folder)
    CHECK_EQ(Key(game.parent_path()), std::wstring(L"<none>"));
    CHECK_EQ(Key(game.root_path()), std::wstring(L"<none>"));
    CHECK_EQ(Key(game / L".."), std::wstring(L"<none>"));
    // nothing for other drives and devices
    std::wstring other = game.root_name().wstring() == L"Q:" ? L"R:\\x.txt" : L"Q:\\x.txt";
    CHECK_EQ(Key(other), std::wstring(L"<none>"));
    CHECK_EQ(Key(L"\\\\.\\pipe\\x"), std::wstring(L"<none>"));
    CHECK_EQ(Key(L""), std::wstring(L"<none>"));

    CHECK_EQ(KeyToPath(L"data\\x.txt"), game.wstring() + L"\\data\\x.txt");
    CHECK(ParentKey(L"a\\b\\c") == L"a\\b");
    CHECK(ParentKey(L"a") == L"");
    CHECK(NameOfKey(L"a\\b\\c") == L"c");
}

TEST_CASE("MatchesMask follows FindFirstFile wildcard rules", "[vfs][keys]")
{
    CHECK(MatchesMask(L"a.txt", L"*"));
    CHECK(MatchesMask(L"a.txt", L"*.*"));
    CHECK(MatchesMask(L"noext", L"*.*"));
    CHECK(MatchesMask(L"A.TXT", L"*.txt"));
    CHECK(!MatchesMask(L"a.txt2", L"*.txt"));
    CHECK(MatchesMask(L"abc", L"a?c"));
    CHECK(!MatchesMask(L"abbc", L"a?c"));
    CHECK(MatchesMask(L"file.img", L"file.img"));
    CHECK(MatchesMask(L"x.tar.gz", L"*.gz"));
    CHECK(!MatchesMask(L"a.txt", L"b*"));
}

// --- zip archives and packages

TEST_CASE("PackagePart recognises single and numbered archives", "[zip][packages]")
{
    std::wstring base;
    uint64_t part = 99;
    CHECK(PackagePart(L"Mods.ZIP", base, part));
    CHECK_EQ(base, std::wstring(L"Mods"));
    CHECK_EQ(part, (uint64_t)0);
    CHECK(PackagePart(L"mods.zip.011", base, part));
    CHECK_EQ(part, (uint64_t)11);
    CHECK(!PackagePart(L"mods.zip.x1", base, part));
    CHECK(!PackagePart(L"mods.rar", base, part));
    CHECK(!PackagePart(L".zip", base, part));
}

namespace
{
    // Writes a zip split into parts with the given names (archive order) and scans the folder.
    std::vector<Package> SplitAndScan(const TempDir& t, const std::vector<std::wstring>& names, const std::string& content)
    {
        auto zip = BuildZip({ { "update/a.txt", content } });
        size_t chunk = (zip.size() + names.size() - 1) / names.size();
        for (size_t i = 0; i < names.size(); ++i)
            WriteBytes(t / L"packages" / names[i], zip.substr((std::min)(zip.size(), i * chunk), chunk));
        return ScanPackages((t / L"packages").wstring());
    }

    std::string ExtractText(Archive& a, const wchar_t* path)
    {
        auto e = FindEntry(a, path);
        if (!e) return "<missing>";
        std::vector<uint8_t> data;
        if (!a.Extract(*e, data)) return "<error>";
        return std::string(data.begin(), data.end());
    }
}

TEST_CASE("ScanPackages joins name.zip.001, .002, ... into one archive", "[zip][packages][multipart]")
{
    TempDir t;
    std::string content(5000, 'a');
    auto p = SplitAndScan(t, { L"test.zip.001", L"test.zip.002", L"test.zip.003" }, content);
    REQUIRE_EQ(p.size(), (size_t)1);
    CHECK(p[0].roots == std::vector<std::wstring>{ L"update" });
    CHECK_EQ(ExtractText(*p[0].archive, L"update\\a.txt"), content);
}

TEST_CASE("ScanPackages orders zero-padded parts (.001 ... .011)", "[zip][packages][multipart]")
{
    TempDir t;
    std::vector<std::wstring> names;
    for (int i = 1; i <= 11; ++i)
    {
        wchar_t n[32];
        swprintf_s(n, L"test.zip.%03d", i);
        names.push_back(n);
    }
    std::string content(20000, 'z');
    auto p = SplitAndScan(t, names, content);
    REQUIRE_EQ(p.size(), (size_t)1);
    CHECK_EQ(ExtractText(*p[0].archive, L"update\\a.txt"), content);
}

TEST_CASE("unpadded part numbers (.1 ... .11) are joined in numeric order", "[zip][packages][multipart]")
{
    TempDir t;
    std::vector<std::wstring> names;
    for (int i = 1; i <= 11; ++i) names.push_back(L"test.zip." + std::to_wstring(i));
    std::string content(20000, 'q');
    auto p = SplitAndScan(t, names, content);
    REQUIRE_MSG(p.size() == 1, "the multi-part archive was not recognised");
    CHECK_EQ(ExtractText(*p[0].archive, L"update\\a.txt"), content);
}

TEST_CASE("archives whose names merely contain \".zip\" are separate archives", "[zip][packages]")
{
    TempDir t;
    WriteBytes(t / L"packages/pack.zip", BuildZip({ { "update/p1.txt", "P1" } }));
    WriteBytes(t / L"packages/pack.zip-extras.zip", BuildZip({ { "update/p2.txt", "P2" } }));
    auto p = ScanPackages((t / L"packages").wstring());
    REQUIRE_EQ(p.size(), (size_t)2);
    CHECK_EQ(ExtractText(*p[0].archive, L"update\\p1.txt"), std::string("P1"));
    CHECK_EQ(ExtractText(*p[1].archive, L"update\\p2.txt"), std::string("P2"));
}

TEST_CASE("zip root folders keep their case (matched case-insensitively by the setup)", "[zip][packages]")
{
    TempDir t;
    WriteBytes(t / L"packages/test.zip", BuildZip({ { "Update/a.txt", "A" }, { "top.txt", "T" }, { "other/sub/b.txt", "B" } }));
    auto p = ScanPackages((t / L"packages").wstring());
    REQUIRE_EQ(p.size(), (size_t)1);
    CHECK(p[0].roots == (std::vector<std::wstring>{ L"Update", L"other" }));
}

TEST_CASE("archives in folders outside the ANSI code page open", "[zip][unicode]")
{
    TempDir t;
    auto dir = t / L"\u4E2D\u6587 \u0416";
    WriteBytes(dir / L"packages/\u00FC.zip", BuildZip({ { "update/a.txt", "A" } }));
    auto p = ScanPackages((dir / L"packages").wstring());
    REQUIRE_EQ(p.size(), (size_t)1);
    CHECK_EQ(ExtractText(*p[0].archive, L"update\\a.txt"), std::string("A"));
}

TEST_CASE("OpenZipEntry: stored entries are read in place, deflated ones extracted", "[zip][vfs]")
{
    TempDir t;
    std::string stored = "0123456789", deflated(100000, 'x');
    for (size_t i = 0; i < deflated.size(); i += 7) deflated[i] = char('a' + i % 26);
    WriteBytes(t / L"s.zip", BuildZip({ { "s.bin", stored } }, 0));
    WriteBytes(t / L"d.zip", BuildZip({ { "d.bin", deflated } }));
    auto s = Archive::Open({ (t / L"s.zip").wstring() });
    auto d = Archive::Open({ (t / L"d.zip").wstring() });
    REQUIRE(s && d);
    auto se = FindEntry(*s, L"s.bin");
    auto de = FindEntry(*d, L"d.bin");
    REQUIRE(se && de);
    CHECK_EQ(se->method, (uint16_t)0);
    CHECK_NE(de->method, (uint16_t)0);

    auto sd = OpenZipEntry(s, *se);
    REQUIRE(sd != nullptr);
    CHECK_EQ(sd->Size(), (uint64_t)10);
    char buf[8];
    DWORD n = 0;
    CHECK(sd->Read(7, buf, 8, n));
    CHECK_EQ(std::string(buf, n), std::string("789"));
    CHECK(sd->Read(10, buf, 8, n));
    CHECK_EQ(n, (DWORD)0);

    auto dd = OpenZipEntry(d, *de);
    REQUIRE(dd != nullptr);
    CHECK(ReadAll(*dd) == deflated);
    CHECK_MSG(OpenZipEntry(d, *de) == dd, "a deflated entry is extracted once while in use");
}

TEST_CASE("concurrent first reads of a deflated zip entry return correct data", "[zip][threads]")
{
    TempDir t;
    std::string content(300000, '\0');
    for (size_t i = 0; i < content.size(); ++i) content[i] = char(i * 31 % 251);
    WriteBytes(t / L"c.zip", BuildZip({ { "c.bin", content } }));
    auto a = Archive::Open({ (t / L"c.zip").wstring() });
    REQUIRE(a != nullptr);
    auto e = FindEntry(*a, L"c.bin");
    REQUIRE(e != nullptr);
    std::atomic<int> bad{ 0 };
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i)
        threads.emplace_back([&] {
            auto d = OpenZipEntry(a, *e);
            if (!d || ReadAll(*d) != content) ++bad;
        });
    for (auto& th : threads) th.join();
    CHECK_EQ(bad.load(), 0);
}

// --- layers (update folders, zip packages)
// Layers are activated once per process, so one test case covers them all.

TEST_CASE("layers: priorities, nested folders, zip mounts, listings, watcher and API paths", "[vfs][layers]")
{
    TempDir t;
    auto game = t / L"Game";
    WriteBytes(game / L"game.txt", "game");
    WriteBytes(game / L"high/both.txt", "H");
    WriteBytes(game / L"low/both.txt", "L");
    WriteBytes(game / L"low/low_only.txt", "L");
    WriteBytes(game / L"low/Sub/deep.txt", "D");
    WriteBytes(game / L"mods/update/m.txt", "M");
    WriteBytes(game / L"mods/update/mods/update/m.txt", "NESTED");
    WriteBytes(game / L"packages/z.zip", BuildZip({ { "zipped/Z.txt", "Z" }, { "zipped/Dir/inner.img", "IMG" }, { "zipped/both.txt", "ZB" } }));
    InitKeys(game.wstring() + L"\\");

    auto packages = ScanPackages((game / L"packages").wstring());
    REQUIRE_EQ(packages.size(), (size_t)1);
    ActivateLayers({
        LayerSpec{ L"high", (game / L"high").wstring(), 4, {} },
        LayerSpec{ L"mods\\update", (game / L"mods\\update").wstring(), 3, {} },
        LayerSpec{ L"low", (game / L"low").wstring(), 2, {} },
        LayerSpec{ L"zipped", (game / L"zipped").wstring(), 1, { packages[0].archive } },
    });
    REQUIRE(HasLayers());
    CHECK(HasZipLayers());

    auto resolve = [&](const fs::path& p) { return ResolveInLayers(Key(p)); };

    // priority
    auto both = resolve(game / L"both.txt");
    CHECK_EQ((int)both.kind, (int)Resolved::Physical);
    CHECK(fs::path(both.path) == game / L"high/both.txt");
    CHECK(fs::path(resolve(game / L"LOW_ONLY.txt").path) == game / L"low/low_only.txt");
    CHECK_EQ((int)resolve(game / L"game.txt").kind, (int)Resolved::None);
    CHECK_EQ((int)resolve(game / L"missing.txt").kind, (int)Resolved::None);
    CHECK_EQ((int)resolve(game / L"sub").kind, (int)Resolved::Directory);

    // files inside a folder layer are not overloaded again (nested layer root)
    CHECK(fs::path(resolve(game / L"m.txt").path) == game / L"mods/update/m.txt");
    CHECK_EQ((int)resolve(game / L"mods/update/m.txt").kind, (int)Resolved::None);
    CHECK_EQ((int)resolve(game / L"high/both.txt").kind, (int)Resolved::None);

    // zip layer: overloads game files and is mounted at <game>\zipped
    auto z = resolve(game / L"z.txt");
    REQUIRE_EQ((int)z.kind, (int)Resolved::Virtual);
    CHECK_EQ(ReadAll(*z.data), std::string("Z"));
    auto inner = resolve(game / L"zipped/dir/INNER.img");
    REQUIRE_EQ((int)inner.kind, (int)Resolved::Virtual);
    CHECK_EQ(ReadAll(*inner.data), std::string("IMG"));
    CHECK_EQ((int)resolve(game / L"zipped").kind, (int)Resolved::Directory);
    CHECK_EQ((int)resolve(game / L"zipped/dir").kind, (int)Resolved::Directory);
    CHECK_EQ((int)resolve(game / L"zipped/nothing").kind, (int)Resolved::None);

    // listings
    auto list = [&](const fs::path& dir) {
        std::vector<ListedItem> items;
        LayerListing(Key(dir), items);
        std::vector<std::wstring> names;
        for (auto& i : items) names.push_back(i.name + (i.directory ? L"\\" : L""));
        std::sort(names.begin(), names.end());
        return names;
    };
    auto root = list(game);
    for (const wchar_t* n : { L"both.txt", L"low_only.txt", L"Sub\\", L"m.txt", L"mods\\", L"Z.txt", L"Dir\\", L"zipped\\" })
        CHECK_MSG(std::find(root.begin(), root.end(), n) != root.end(), "game folder listing lacks " + ut::narrow(n));
    CHECK(list(game / L"zipped") == (std::vector<std::wstring>{ L"Dir\\", L"Z.txt", L"both.txt" }));
    CHECK(list(game / L"zipped/dir") == std::vector<std::wstring>{ L"inner.img" });

    // watcher picks up files added while the game runs
    WriteBytes(game / L"low/new.txt", "N");
    bool seen = false;
    for (int i = 0; i < 100 && !seen; ++i)
    {
        seen = resolve(game / L"new.txt").kind == Resolved::Physical;
        if (!seen) Sleep(20);
    }
    CHECK_MSG(seen, "a file added to an update folder was not picked up");
    fs::remove(game / L"low/new.txt");
    bool gone = false;
    for (int i = 0; i < 100 && !gone; ++i)
    {
        gone = resolve(game / L"new.txt").kind == Resolved::None;
        if (!gone) Sleep(20);
    }
    CHECK_MSG(gone, "a file removed from an update folder is still overloaded");

    // API
    wchar_t buf[MAX_PATH * 2];
    REQUIRE(GetOverloadPathW(buf, std::size(buf)));
    CHECK(fs::path(buf) == game / L"high");
    char abuf[MAX_PATH * 2];
    REQUIRE(GetOverloadPathA(abuf, std::size(abuf)));
    CHECK(fs::path(abuf) == game / L"high");
    REQUIRE(GetOverloadedFilePathW((game / L"both.txt").c_str(), buf, std::size(buf)));
    CHECK(fs::path(buf) == game / L"high/both.txt");
    CHECK(!GetOverloadedFilePathW((game / L"game.txt").c_str(), buf, std::size(buf)));
    CHECK(!GetOverloadPathW(nullptr, 10));
    CHECK_MSG(GetOverloadedFilePathW((game / L"both.txt").c_str(), nullptr, 0), "a null buffer only asks whether the file is overloaded");

    // relative input gives a path relative to the current directory
    wchar_t old[MAX_PATH];
    GetCurrentDirectoryW(MAX_PATH, old);
    SetCurrentDirectoryW(game.c_str());
    REQUIRE(GetOverloadedFilePathW(L"both.txt", buf, std::size(buf)));
    CHECK_EQ(std::wstring(buf), std::wstring(L"high\\both.txt"));
    SetCurrentDirectoryW(old);

    // never writes past outSize
    std::fill(std::begin(buf), std::end(buf), L'#');
    CHECK(!GetOverloadPathW(buf, 4));
    CHECK_MSG(buf[4] == L'#', "GetOverloadPathW wrote to out[outSize]");
    std::fill(std::begin(buf), std::end(buf), L'#');
    CHECK(!GetOverloadedFilePathW((game / L"both.txt").c_str(), buf, 4));
    CHECK_MSG(buf[4] == L'#', "GetOverloadedFilePathW wrote to out[outSize]");
    std::fill(std::begin(abuf), std::end(abuf), '#');
    CHECK(!GetOverloadPathA(abuf, 4));
    CHECK_MSG(abuf[4] == '#', "GetOverloadPathA wrote to out[outSize]");

    CHECK(PhysicalLayerRoots() == (std::vector<std::wstring>{ (game / L"high").wstring(), (game / L"mods\\update").wstring(), (game / L"low").wstring() }));
}

// --- proxy

TEST_CASE("proxy: supported names and ordinal tables", "[proxy]")
{
    for (const wchar_t* n : { L"dinput8.dll", L"DSOUND.dll", L"version.dll", L"winmm.dll", L"xinput1_3.dll", L"d3d12.dll", L"dxgi.dll", L"winhttp.dll" })
        CHECK_MSG(proxy::IsSupportedName(n), ut::narrow(n));
#ifndef _WIN64
    for (const wchar_t* n : { L"vorbisFile.dll", L"d3d8.dll", L"ddraw.dll", L"binkw32.dll", L"xlive.dll", L"msacm32.dll" })
        CHECK_MSG(proxy::IsSupportedName(n), ut::narrow(n));
#endif
    CHECK(!proxy::IsSupportedName(L"kernel32.dll"));
    CHECK(!proxy::IsSupportedName(L"dinput8.asi"));

    // dsound ordinals 1-12 map to the loader's export of the same name
    auto dsound = proxy::OrdinalEntries(L"dsound.dll");
    for (WORD o = 1; o <= 12; ++o)
        CHECK_MSG(std::any_of(dsound.begin(), dsound.end(), [&](auto& e) { return e.first == o && e.second; }), "dsound ordinal " + std::to_string(o));
}

TEST_CASE("proxy: ordinals of named exports follow the system DLL of this Windows version", "[proxy][ordinals]")
{
    for (const wchar_t* dll : { L"winhttp.dll", L"dsound.dll", L"dinput8.dll", L"version.dll", L"winmm.dll" })
    {
        HMODULE sys = LoadLibraryExW((SystemDirectory() + dll).c_str(), nullptr, LOAD_LIBRARY_AS_IMAGE_RESOURCE | LOAD_LIBRARY_AS_DATAFILE);
        REQUIRE(sys);
        auto base = (const BYTE*)((uintptr_t)sys & ~(uintptr_t)3);
        auto nt = (const IMAGE_NT_HEADERS*)(base + ((const IMAGE_DOS_HEADER*)base)->e_lfanew);
        auto exp = (const IMAGE_EXPORT_DIRECTORY*)(base + nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);
        auto names = (const DWORD*)(base + exp->AddressOfNames);
        auto ords = (const WORD*)(base + exp->AddressOfNameOrdinals);
        auto entries = proxy::OrdinalEntries(dll);
        int mapped = 0;
        for (DWORD i = 0; i < exp->NumberOfNames; ++i)
        {
            const char* name = (const char*)(base + names[i]);
            const void* expected = proxy::ExportFor(dll, name);
            if (!expected) continue;
            WORD ordinal = (WORD)(exp->Base + ords[i]);
            auto it = std::find_if(entries.begin(), entries.end(), [&](auto& e) { return e.first == ordinal; });
            CHECK_MSG(it != entries.end() && it->second == expected, ut::narrow(dll) + " " + name);
            ++mapped;
        }
        FreeLibrary(sys);
        CHECK_MSG(mapped > 0, ut::narrow(dll));
    }
}

// --- import table patching (synthetic PE images)

namespace
{
    // In-memory PE image with one import descriptor per entry of `imports`
    class FakeImage
    {
    public:
        struct Import
        {
            std::string dll;
            std::vector<std::string> names;  // "#n" for ordinals, empty for a bound import without names
            std::vector<void*> iat;          // initial IAT, one per name or bound slot
        };

        explicit FakeImage(const std::vector<Import>& imports)
        {
            base = static_cast<BYTE*>(VirtualAlloc(nullptr, kSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
            dos->e_magic = IMAGE_DOS_SIGNATURE;
            dos->e_lfanew = 0x80;
            auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + 0x80);
            nt->Signature = IMAGE_NT_SIGNATURE;
            nt->FileHeader.NumberOfSections = 1;
            nt->FileHeader.SizeOfOptionalHeader = sizeof(IMAGE_OPTIONAL_HEADER);
            nt->OptionalHeader.Magic = IMAGE_NT_OPTIONAL_HDR_MAGIC;
            nt->OptionalHeader.NumberOfRvaAndSizes = IMAGE_NUMBEROF_DIRECTORY_ENTRIES;
            nt->OptionalHeader.SizeOfImage = kSize;
            auto sec = IMAGE_FIRST_SECTION(nt);
            memcpy(sec->Name, ".idata", 6);
            sec->VirtualAddress = 0x1000;
            sec->PointerToRawData = 0x1000;
            sec->Misc.VirtualSize = kSize - 0x1000;
            sec->SizeOfRawData = kSize - 0x1000;

            DWORD descRva = 0x1000;
            DWORD cursor = 0x2000;
            auto alloc = [&](DWORD size) { DWORD r = cursor; cursor += (size + 15) & ~15u; return r; };
            nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT] = { descRva, DWORD((imports.size() + 1) * sizeof(IMAGE_IMPORT_DESCRIPTOR)) };
            for (size_t i = 0; i < imports.size(); ++i)
            {
                auto& imp = imports[i];
                auto d = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + descRva) + i;
                d->Name = alloc((DWORD)imp.dll.size() + 1);
                memcpy(base + d->Name, imp.dll.c_str(), imp.dll.size() + 1);
                size_t count = imp.iat.size();
                d->FirstThunk = alloc(DWORD((count + 1) * sizeof(void*)));
                auto iat = reinterpret_cast<void**>(base + d->FirstThunk);
                for (size_t j = 0; j < count; ++j) iat[j] = imp.iat[j];
                iatOf[imp.dll] = iat;
                if (!imp.names.empty())
                {
                    d->OriginalFirstThunk = alloc(DWORD((count + 1) * sizeof(IMAGE_THUNK_DATA)));
                    auto thunks = reinterpret_cast<IMAGE_THUNK_DATA*>(base + d->OriginalFirstThunk);
                    for (size_t j = 0; j < count; ++j)
                    {
                        auto& n = imp.names[j];
                        if (n[0] == '#')
                            thunks[j].u1.Ordinal = IMAGE_ORDINAL_FLAG | (ULONG_PTR)atoi(n.c_str() + 1);
                        else
                        {
                            DWORD rva = alloc(DWORD(sizeof(WORD) + n.size() + 1));
                            memcpy(base + rva + sizeof(WORD), n.c_str(), n.size() + 1);
                            thunks[j].u1.AddressOfData = rva;
                        }
                    }
                }
            }
        }
        ~FakeImage() { VirtualFree(base, 0, MEM_RELEASE); }
        HMODULE module() const { return reinterpret_cast<HMODULE>(base); }
        void** iat(const std::string& dll) { return iatOf.at(dll); }

    private:
        static constexpr DWORD kSize = 0x10000;
        BYTE* base = nullptr;
        std::map<std::string, void**> iatOf;
    };

    void* K32(const char* name) { return (void*)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), name); }

    const void* OrdinalEntry(const wchar_t* dll, WORD ordinal)
    {
        for (const auto& [o, e] : proxy::OrdinalEntries(dll))
            if (o == ordinal) return e;
        return nullptr;
    }
}

TEST_CASE("PatchKernel32 redirects named kernel32 imports and RestoreSlots undoes it", "[iat]")
{
    void* sleep = K32("Sleep");
    void* sysinfo = K32("GetSystemInfo");
    void* exitp = K32("ExitProcess");
    FakeImage img({ { "KERNEL32.dll", { "Sleep", "GetSystemInfo", "ExitProcess" }, { sleep, sysinfo, exitp } } });
    startup::ModuleImports rec;
    startup::PatchKernel32(pe::Image(img.module()), rec);
    auto iat = img.iat("KERNEL32.dll");
    CHECK(iat[0] == startup::Kernel32Wrapper("Sleep"));
    CHECK(iat[1] == startup::Kernel32Wrapper("GetSystemInfo"));
    CHECK_MSG(iat[2] == exitp, "functions without a wrapper are untouched");
    CHECK_EQ(rec.kernel32.size(), (size_t)2);
    startup::RestoreSlots(rec.kernel32);
    CHECK(iat[0] == sleep);
    CHECK(iat[1] == sysinfo);
}

TEST_CASE("PatchKernel32 recognises API-set imports and bound imports by address", "[iat]")
{
    void* sleep = K32("Sleep");
    void* cev = K32("CreateEventW");
    FakeImage img({
        { "api-ms-win-core-synch-l1-2-0.dll", { "Sleep" }, { sleep } },
        { "bound.dll", {}, { cev, (void*)&wmain } },
    });
    startup::ModuleImports rec;
    startup::PatchKernel32(pe::Image(img.module()), rec);
    CHECK(img.iat("api-ms-win-core-synch-l1-2-0.dll")[0] == startup::Kernel32Wrapper("Sleep"));
    CHECK(img.iat("bound.dll")[0] == startup::Kernel32Wrapper("CreateEventW"));
    CHECK(img.iat("bound.dll")[1] == (void*)&wmain);
}

TEST_CASE("PatchKernel32 ignores same-named imports of unrelated DLLs", "[iat]")
{
    FakeImage img({ { "mylib.dll", { "Sleep" }, { (void*)&wmain } } });
    startup::ModuleImports rec;
    startup::PatchKernel32(pe::Image(img.module()), rec);
    CHECK(rec.kernel32.empty());
    CHECK(img.iat("mylib.dll")[0] == (void*)&wmain);
}

TEST_CASE("PatchOrdinals redirects ordinal imports of the proxied DLL", "[iat][ordinals]")
{
    HMODULE dsound = LoadLibraryW(L"dsound.dll");
    REQUIRE(dsound);
    FakeImage img({ { "dsound.dll", { "#1", "#2", "#11" }, { (void*)1, (void*)2, (void*)3 } } });
    startup::PatchOrdinals(pe::Image(img.module()), dsound, L"dsound.dll");
    auto iat = img.iat("dsound.dll");
    CHECK(iat[0] == OrdinalEntry(L"dsound.dll", 1));
    CHECK(iat[1] == OrdinalEntry(L"dsound.dll", 2));
    CHECK(iat[2] == OrdinalEntry(L"dsound.dll", 11));
    CHECK(iat[0] != nullptr);
}

TEST_CASE("PatchOrdinals handles every dsound ordinal in bound imports", "[iat][ordinals]")
{
    HMODULE dsound = LoadLibraryW(L"dsound.dll");
    REQUIRE(dsound);
    auto byOrd = [&](int o) { return (void*)GetProcAddress(dsound, MAKEINTRESOURCEA(o)); };
    // Bound import: no OriginalFirstThunk, the IAT holds resolved addresses
    FakeImage img({ { "dsound.dll", {}, { byOrd(1), byOrd(2), byOrd(3), byOrd(11) } } });
    startup::PatchOrdinals(pe::Image(img.module()), dsound, L"DSOUND.DLL");
    auto iat = img.iat("dsound.dll");
    CHECK(iat[0] == OrdinalEntry(L"dsound.dll", 1));
    CHECK_MSG(iat[1] == OrdinalEntry(L"dsound.dll", 2), "ordinal 2 (DirectSoundEnumerateA) is not redirected");
    CHECK_MSG(iat[2] == OrdinalEntry(L"dsound.dll", 3), "ordinal 3 (DirectSoundEnumerateW) is not redirected");
    CHECK(iat[3] == OrdinalEntry(L"dsound.dll", 11));
}
