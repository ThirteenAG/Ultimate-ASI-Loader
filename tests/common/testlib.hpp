// Small test framework for the unit tests and the integration runner.
// TEST_CASE("name", "[tags]") or ut::add_test registers; CHECK/REQUIRE/CHECK_EQ/REQUIRE_EQ/
// CHECK_MSG/FAIL/SKIP/INFO assert. Writes JUnit XML (--junit) and $GITHUB_STEP_SUMMARY.
#pragma once

#include <windows.h>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#if defined(CHECK) || defined(REQUIRE) || defined(TEST_CASE)
#error "testlib.hpp: assertion macro names are already defined"
#endif

namespace ut
{
    // --- strings

    inline std::string narrow(std::wstring_view s)
    {
        if (s.empty()) return {};
        int n = WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
        std::string r(n, '\0');
        WideCharToMultiByte(CP_UTF8, 0, s.data(), (int)s.size(), r.data(), n, nullptr, nullptr);
        return r;
    }

    inline std::wstring widen(std::string_view s)
    {
        if (s.empty()) return {};
        int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
        std::wstring r(n, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), r.data(), n);
        return r;
    }

    template<class T>
    std::string to_str(const T& v)
    {
        using D = std::decay_t<T>;
        if constexpr (std::is_same_v<D, std::string> || std::is_same_v<D, std::string_view>)
            return "\"" + std::string(v) + "\"";
        else if constexpr (std::is_same_v<D, const char*> || std::is_same_v<D, char*>)
            return v ? "\"" + std::string(v) + "\"" : std::string("nullptr");
        else if constexpr (std::is_same_v<D, std::wstring> || std::is_same_v<D, std::wstring_view>)
            return "L\"" + narrow(v) + "\"";
        else if constexpr (std::is_same_v<D, const wchar_t*> || std::is_same_v<D, wchar_t*>)
            return v ? "L\"" + narrow(v) + "\"" : std::string("nullptr");
        else if constexpr (std::is_same_v<D, std::filesystem::path>)
            return "path(\"" + narrow(v.wstring()) + "\")";
        else if constexpr (std::is_same_v<D, bool>)
            return v ? "true" : "false";
        else if constexpr (std::is_same_v<D, char> || std::is_same_v<D, wchar_t>)
            return std::to_string((int)v);
        else if constexpr (std::is_enum_v<D>)
            return std::to_string((long long)v);
        else if constexpr (std::is_integral_v<D>)
        {
            std::ostringstream o;
            o << +v;
            if constexpr (sizeof(D) >= 4)
                if (v > 9 || (std::is_signed_v<D> && v < -9))
                    o << " (0x" << std::hex << (unsigned long long)(std::make_unsigned_t<D>)v << ")";
            return o.str();
        }
        else if constexpr (std::is_floating_point_v<D>)
            return std::to_string(v);
        else if constexpr (std::is_pointer_v<D> || std::is_null_pointer_v<D>)
        {
            char buf[32];
            snprintf(buf, sizeof(buf), "%p", (const void*)v);
            return buf;
        }
        else
            return "{?}";
    }

    // --- model

    enum class Status { Pass, Fail, Skip };

    struct TestInfo
    {
        std::string name;
        std::vector<std::string> tags;
        std::function<void()> body;
        std::string file;
        int line = 0;

        bool has_tag(std::string_view t) const
        {
            return std::any_of(tags.begin(), tags.end(), [&](const std::string& x) { return _stricmp(x.c_str(), std::string(t).c_str()) == 0; });
        }
    };

    struct Failure
    {
        std::string where;
        std::string message;
    };

    struct Result
    {
        const TestInfo* test = nullptr;
        Status status = Status::Pass;
        std::vector<Failure> failures;
        std::vector<std::string> notes;
        std::string skip_reason;
        double seconds = 0;
    };

    struct Options
    {
        std::vector<std::string> filters;
        std::vector<std::string> include_tags;
        std::vector<std::string> exclude_tags;
        std::string junit;
        bool list = false;
        bool fail_fast = false;
        bool keep = false;
        bool verbose = false;
        int repeat = 1;
        std::map<std::string, std::string> extra; // suite specific --key value
    };

    inline std::vector<TestInfo>& registry() { static std::vector<TestInfo> r; return r; }
    inline Options& options() { static Options o; return o; }
    inline Result*& current() { static Result* r = nullptr; return r; }

    struct RequireAbort {};
    struct SkipSignal { std::string reason; };

    inline std::vector<std::string> parse_tags(std::string_view s)
    {
        std::vector<std::string> out;
        size_t i = 0;
        while ((i = s.find('[', i)) != std::string_view::npos)
        {
            auto j = s.find(']', i);
            if (j == std::string_view::npos) break;
            if (j > i + 1) out.emplace_back(s.substr(i + 1, j - i - 1));
            i = j + 1;
        }
        return out;
    }

    inline void add_test(std::string name, std::string_view tags, std::function<void()> fn, const char* file = "", int line = 0)
    {
        registry().push_back({ std::move(name), parse_tags(tags), std::move(fn), file, line });
    }

    struct AutoReg
    {
        AutoReg(const char* name, const char* tags, void (*fn)(), const char* file, int line)
        {
            add_test(name, tags, fn, file, line);
        }
    };

    // --- asserts

    inline std::string where(const char* file, int line)
    {
        return std::filesystem::path(file).filename().string() + ":" + std::to_string(line);
    }

    inline void fail_impl(const char* file, int line, std::string msg, bool fatal)
    {
        if (auto r = current())
            r->failures.push_back({ where(file, line), std::move(msg) });
        if (fatal)
            throw RequireAbort{};
    }

    inline bool check_impl(bool ok, const char* expr, const char* file, int line, bool fatal, const std::string& msg = {})
    {
        if (!ok)
            fail_impl(file, line, std::string(fatal ? "REQUIRE(" : "CHECK(") + expr + ")" + (msg.empty() ? "" : "\n      " + msg), fatal);
        return ok;
    }

    // std::cmp_equal rejects bool and character types
    template<class T>
    constexpr bool is_cmp_int = std::is_integral_v<T> && !std::is_same_v<T, bool> && !std::is_same_v<T, char> && !std::is_same_v<T, wchar_t> &&
                                !std::is_same_v<T, char8_t> && !std::is_same_v<T, char16_t> && !std::is_same_v<T, char32_t>;

    template<class A, class B>
    bool check_cmp(const A& a, const B& b, bool wantEqual, const char* ea, const char* eb, const char* file, int line, bool fatal)
    {
        bool eq;
        if constexpr (is_cmp_int<A> && is_cmp_int<B>)
            eq = std::cmp_equal(a, b);
        else
            eq = (a == b);
        if (eq != wantEqual)
        {
            std::string m = std::string(fatal ? "REQUIRE" : "CHECK") + (wantEqual ? "_EQ(" : "_NE(") + ea + ", " + eb + ")\n      left : " + to_str(a) + "\n      right: " + to_str(b);
            fail_impl(file, line, m, fatal);
            return false;
        }
        return true;
    }

    inline void note(std::string s)
    {
        if (auto r = current()) r->notes.push_back(std::move(s));
    }
}

#define UT_CAT2(a, b) a##b
#define UT_CAT(a, b) UT_CAT2(a, b)
#define UT_TEST_CASE_IMPL(id, name, tags)                                                                                \
    static void UT_CAT(ut_test_fn_, id)();                                                                               \
    static ::ut::AutoReg UT_CAT(ut_test_reg_, id)(name, tags, &UT_CAT(ut_test_fn_, id), __FILE__, __LINE__);             \
    static void UT_CAT(ut_test_fn_, id)()
#define TEST_CASE(name, tags) UT_TEST_CASE_IMPL(__COUNTER__, name, tags)

#define CHECK(expr) ::ut::check_impl(static_cast<bool>(expr), #expr, __FILE__, __LINE__, false)
#define REQUIRE(expr) ::ut::check_impl(static_cast<bool>(expr), #expr, __FILE__, __LINE__, true)
#define CHECK_MSG(expr, msg) ::ut::check_impl(static_cast<bool>(expr), #expr, __FILE__, __LINE__, false, (msg))
#define REQUIRE_MSG(expr, msg) ::ut::check_impl(static_cast<bool>(expr), #expr, __FILE__, __LINE__, true, (msg))
#define CHECK_EQ(a, b) ::ut::check_cmp((a), (b), true, #a, #b, __FILE__, __LINE__, false)
#define REQUIRE_EQ(a, b) ::ut::check_cmp((a), (b), true, #a, #b, __FILE__, __LINE__, true)
#define CHECK_NE(a, b) ::ut::check_cmp((a), (b), false, #a, #b, __FILE__, __LINE__, false)
#define REQUIRE_NE(a, b) ::ut::check_cmp((a), (b), false, #a, #b, __FILE__, __LINE__, true)
#define FAIL(msg) ::ut::fail_impl(__FILE__, __LINE__, (msg), true)
#define FAIL_CHECK(msg) ::ut::fail_impl(__FILE__, __LINE__, (msg), false)
#define SKIP(msg) throw ::ut::SkipSignal{ (msg) }
#define INFO(msg) ::ut::note(msg)

namespace ut
{
    // --- runner

    namespace detail
    {
        inline bool wildcard_match(std::string_view pat, std::string_view s)
        {
            // case-insensitive '*' / '?' matching
            size_t p = 0, i = 0, star = std::string_view::npos, mark = 0;
            auto eq = [](char a, char b) { return tolower((unsigned char)a) == tolower((unsigned char)b); };
            while (i < s.size())
            {
                if (p < pat.size() && (pat[p] == '?' || eq(pat[p], s[i]))) { ++p; ++i; }
                else if (p < pat.size() && pat[p] == '*') { star = p++; mark = i; }
                else if (star != std::string_view::npos) { p = star + 1; i = ++mark; }
                else return false;
            }
            while (p < pat.size() && pat[p] == '*') ++p;
            return p == pat.size();
        }

        inline bool selected(const TestInfo& t, const Options& o)
        {
            if (!o.filters.empty())
            {
                bool any = false;
                for (auto& f : o.filters)
                {
                    bool hasWild = f.find_first_of("*?") != std::string::npos;
                    if (hasWild ? wildcard_match(f, t.name) : wildcard_match("*" + f + "*", t.name)) { any = true; break; }
                }
                if (!any) return false;
            }
            if (!o.include_tags.empty() && std::none_of(o.include_tags.begin(), o.include_tags.end(), [&](auto& x) { return t.has_tag(x); }))
                return false;
            if (std::any_of(o.exclude_tags.begin(), o.exclude_tags.end(), [&](auto& x) { return t.has_tag(x); }))
                return false;
            return true;
        }

        inline std::vector<std::string> split_csv(const std::string& s)
        {
            std::vector<std::string> out;
            std::stringstream ss(s);
            std::string item;
            while (std::getline(ss, item, ','))
            {
                item.erase(0, item.find_first_not_of(" []"));
                item.erase(item.find_last_not_of(" []") + 1);
                if (!item.empty()) out.push_back(item);
            }
            return out;
        }

        inline std::string xml_escape(std::string_view s)
        {
            std::string r;
            for (unsigned char c : s)
            {
                switch (c)
                {
                case '&': r += "&amp;"; break;
                case '<': r += "&lt;"; break;
                case '>': r += "&gt;"; break;
                case '"': r += "&quot;"; break;
                case '\'': r += "&apos;"; break;
                default:
                    if (c < 0x20 && c != '\n' && c != '\r' && c != '\t') { char b[8]; snprintf(b, sizeof(b), "&#%d;", c); r += b; }
                    else r += (char)c;
                }
            }
            return r;
        }

        inline std::string md_escape(std::string_view s)
        {
            std::string r;
            for (char c : s)
            {
                if (c == '|') r += "\\|";
                else if (c == '\n' || c == '\r') r += ' ';
                else r += c;
            }
            return r;
        }

        // Returns the SEH exception code instead of terminating. C++ exceptions propagate.
        inline DWORD seh_guard(void (*fn)(void*), void* ctx)
        {
            __try
            {
                fn(ctx);
                return 0;
            }
            __except (GetExceptionCode() == 0xE06D7363 ? EXCEPTION_CONTINUE_SEARCH : EXCEPTION_EXECUTE_HANDLER)
            {
                return GetExceptionCode();
            }
        }

        struct Console
        {
            bool color = false;
            Console()
            {
                HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
                DWORD mode = 0;
                if (h && GetConsoleMode(h, &mode))
                    color = SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
                SetConsoleOutputCP(CP_UTF8);
            }
            const char* c(const char* code) const { return color ? code : ""; }
        };
    }

    inline void print_usage(const char* suite)
    {
        printf(
            "%s - test runner\n"
            "  --list                  list tests (honours filters)\n"
            "  --filter <pattern>      run tests whose name contains <pattern> (or matches a * / ? wildcard); repeatable\n"
            "  --tags <a,b>            run only tests that have any of these tags\n"
            "  --exclude-tags <a,b>    skip tests that have any of these tags\n"
            "  --junit <file>          write JUnit XML report\n"
            "  --repeat <n>            run the selection n times\n"
            "  --fail-fast             stop at the first unexpected failure\n"
            "  --keep                  keep per-test sandboxes / temporary files\n"
            "  --verbose               print notes of passing tests as well\n",
            suite);
    }

    inline int run(int argc, wchar_t** argv, const char* suiteName, std::set<std::string> extraValueOptions = {})
    {
        auto& o = options();
        for (int i = 1; i < argc; ++i)
        {
            std::string a = narrow(argv[i]);
            auto next = [&]() -> std::string {
                if (i + 1 >= argc) { fprintf(stderr, "missing value for %s\n", a.c_str()); exit(2); }
                return narrow(argv[++i]);
            };
            if (a == "--help" || a == "-h" || a == "/?") { print_usage(suiteName); return 0; }
            else if (a == "--list") o.list = true;
            else if (a == "--filter" || a == "-f") o.filters.push_back(next());
            else if (a == "--tags" || a == "-t") for (auto& t : detail::split_csv(next())) o.include_tags.push_back(t);
            else if (a == "--exclude-tags") for (auto& t : detail::split_csv(next())) o.exclude_tags.push_back(t);
            else if (a == "--junit") o.junit = next();
            else if (a == "--repeat") o.repeat = (std::max)(1, atoi(next().c_str()));
            else if (a == "--fail-fast") o.fail_fast = true;
            else if (a == "--keep") o.keep = true;
            else if (a == "--verbose" || a == "-v") o.verbose = true;
            else if (a.rfind("--", 0) == 0 && extraValueOptions.count(a.substr(2))) o.extra[a.substr(2)] = next();
            else if (a.rfind("--", 0) == 0) { fprintf(stderr, "unknown option %s\n", a.c_str()); print_usage(suiteName); return 2; }
            else o.filters.push_back(a);
        }

        std::vector<const TestInfo*> selected;
        for (auto& t : registry())
            if (detail::selected(t, o)) selected.push_back(&t);

        if (o.list)
        {
            for (auto t : selected)
            {
                std::string tags;
                for (auto& x : t->tags) tags += "[" + x + "]";
                printf("%s  %s\n", t->name.c_str(), tags.c_str());
            }
            printf("%zu test(s)\n", selected.size());
            return 0;
        }

        if (selected.empty())
        {
            fprintf(stderr, "No tests matched the given filters.\n");
            return 2;
        }

        detail::Console con;
        const char* GREEN = con.c("\x1b[32m");
        const char* RED = con.c("\x1b[31m");
        const char* YELLOW = con.c("\x1b[33m");
        const char* CYAN = con.c("\x1b[36m");
        const char* RESET = con.c("\x1b[0m");

        std::vector<Result> results;
        results.reserve(selected.size() * o.repeat);
        auto t0 = std::chrono::steady_clock::now();
        bool stop = false;

        for (int rep = 0; rep < o.repeat && !stop; ++rep)
        {
            for (auto t : selected)
            {
                Result r;
                r.test = t;
                current() = &r;
                printf("%s[ RUN      ]%s %s\n", CYAN, RESET, t->name.c_str());
                fflush(stdout);

                auto start = std::chrono::steady_clock::now();
                std::string exceptionText;
                try
                {
                    struct Ctx { const TestInfo* t; } ctx{ t };
                    DWORD code = detail::seh_guard([](void* p) { static_cast<Ctx*>(p)->t->body(); }, &ctx);
                    if (code)
                    {
                        char b[96];
                        snprintf(b, sizeof(b), "structured exception 0x%08lX escaped the test body", code);
                        r.failures.push_back({ t->file.empty() ? "" : where(t->file.c_str(), t->line), b });
                    }
                }
                catch (const RequireAbort&) {}
                catch (const SkipSignal& s)
                {
                    // a SKIP after a failed CHECK must not hide the failure
                    if (r.failures.empty()) { r.status = Status::Skip; r.skip_reason = s.reason; }
                }
                catch (const std::exception& e) { r.failures.push_back({ "", std::string("unhandled exception: ") + e.what() }); }
                catch (...) { r.failures.push_back({ "", "unhandled non-standard exception" }); }
                r.seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
                current() = nullptr;

                if (r.status != Status::Skip)
                {
                    r.status = r.failures.empty() ? Status::Pass : Status::Fail;
                }

                auto ms = (int)(r.seconds * 1000);
                switch (r.status)
                {
                case Status::Pass: printf("%s[       OK ]%s %s (%d ms)\n", GREEN, RESET, t->name.c_str(), ms); break;
                case Status::Skip: printf("%s[  SKIPPED ]%s %s - %s\n", YELLOW, RESET, t->name.c_str(), r.skip_reason.c_str()); break;
                case Status::Fail: printf("%s[  FAILED  ]%s %s (%d ms)\n", RED, RESET, t->name.c_str(), ms); break;
                }
                if (r.status == Status::Fail)
                    for (auto& f : r.failures)
                        printf("    %s%s\n", f.where.empty() ? "" : (f.where + ": ").c_str(), f.message.c_str());
                if ((r.status == Status::Fail || o.verbose) && !r.notes.empty())
                    for (auto& n : r.notes)
                        printf("    note: %s\n", n.c_str());
                fflush(stdout);

                bool failed = r.status == Status::Fail;
                results.push_back(std::move(r));
                if (failed && o.fail_fast) { stop = true; break; }
            }
        }

        double total = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        size_t pass = 0, fail = 0, skip = 0;
        for (auto& r : results)
        {
            switch (r.status)
            {
            case Status::Pass: ++pass; break;
            case Status::Fail: ++fail; break;
            case Status::Skip: ++skip; break;
            }
        }

        printf("\n================================================================\n");
        printf("%s: %zu test(s) in %.1f s: %s%zu passed%s, %s%zu failed%s, %zu skipped\n",
            suiteName, results.size(), total, GREEN, pass, RESET, fail ? RED : "", fail, RESET, skip);
        if (fail)
        {
            printf("%sFailed tests:%s\n", RED, RESET);
            for (auto& r : results) if (r.status == Status::Fail) printf("  - %s\n", r.test->name.c_str());
        }

        auto classname = [&](const Result& r) {
            std::string c = suiteName;
            for (auto& tg : r.test->tags)
                if (tg != "Win32" && tg != "x64") { c += "." + tg; break; }
            return c;
        };
        auto failure_text = [](const Result& r) {
            std::string s;
            for (auto& f : r.failures) s += (f.where.empty() ? "" : f.where + ": ") + f.message + "\n";
            for (auto& n : r.notes) s += "note: " + n + "\n";
            return s;
        };

        if (!o.junit.empty())
        {
            std::error_code ec;
            auto jp = std::filesystem::path(widen(o.junit));
            if (jp.has_parent_path()) std::filesystem::create_directories(jp.parent_path(), ec);
            if (FILE* f = _wfopen(jp.c_str(), L"wb"))
            {
                size_t failures = fail, skipped = skip;
                fprintf(f, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
                fprintf(f, "<testsuites name=\"%s\" tests=\"%zu\" failures=\"%zu\" skipped=\"%zu\" time=\"%.3f\">\n", suiteName, results.size(), failures, skipped, total);
                fprintf(f, "  <testsuite name=\"%s\" tests=\"%zu\" failures=\"%zu\" skipped=\"%zu\" time=\"%.3f\">\n", suiteName, results.size(), failures, skipped, total);
                for (auto& r : results)
                {
                    fprintf(f, "    <testcase classname=\"%s\" name=\"%s\" time=\"%.3f\"", detail::xml_escape(classname(r)).c_str(), detail::xml_escape(r.test->name).c_str(), r.seconds);
                    auto text = failure_text(r);
                    auto first = r.failures.empty() ? std::string() : r.failures.front().message.substr(0, r.failures.front().message.find('\n'));
                    if (r.status == Status::Fail)
                        fprintf(f, ">\n      <failure message=\"%s\">%s</failure>\n    </testcase>\n", detail::xml_escape(first).c_str(), detail::xml_escape(text).c_str());
                    else if (r.status == Status::Skip)
                        fprintf(f, ">\n      <skipped message=\"%s\"/>\n    </testcase>\n", detail::xml_escape(r.skip_reason).c_str());
                    else
                        fprintf(f, "/>\n");
                }
                fprintf(f, "  </testsuite>\n</testsuites>\n");
                fclose(f);
            }
            else
                fprintf(stderr, "warning: cannot write %s\n", o.junit.c_str());
        }

        wchar_t summaryPath[MAX_PATH * 2];
        if (GetEnvironmentVariableW(L"GITHUB_STEP_SUMMARY", summaryPath, (DWORD)std::size(summaryPath)))
        {
            if (FILE* f = _wfopen(summaryPath, L"ab"))
            {
                fprintf(f, "### %s\n\n| Passed | Failed | Skipped | Time |\n|---|---|---|---|\n| %zu | %zu | %zu | %.1f s |\n\n",
                    suiteName, pass, fail, skip, total);
                if (fail)
                {
                    fprintf(f, "| Result | Test | Details |\n|---|---|---|\n");
                    for (auto& r : results)
                    {
                        if (r.status != Status::Fail) continue;
                        const char* st = "FAILED";
                        auto first = r.failures.empty() ? std::string() : r.failures.front().message;
                        fprintf(f, "| %s | %s | %s |\n", st, detail::md_escape(r.test->name).c_str(), detail::md_escape(first.substr(0, 300)).c_str());
                    }
                    fprintf(f, "\n");
                }
                fclose(f);
            }
        }

        return fail > 0 ? 1 : 0;
    }
}
