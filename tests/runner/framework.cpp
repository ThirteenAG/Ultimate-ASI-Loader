#include "framework.hpp"
#include <commctrl.h>
#include <UIAutomation.h>
#include <atomic>
#include <fstream>
#include <set>
#include <sstream>
#include "miniz.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

namespace runner
{
    // --- utilities

    std::wstring Lower(std::wstring s)
    {
        CharLowerBuffW(s.data(), (DWORD)s.size());
        return s;
    }

    bool IEquals(std::wstring_view a, std::wstring_view b)
    {
        return CompareStringOrdinal(a.data(), (int)a.size(), b.data(), (int)b.size(), TRUE) == CSTR_EQUAL;
    }

    std::wstring PathW(const fs::path& p) { return p.wstring(); }

    std::string ReadFileBytes(const fs::path& p)
    {
        std::ifstream f(p, std::ios::binary);
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    void WriteFileBytes(const fs::path& p, const std::string& data)
    {
        std::error_code ec;
        if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
        std::ofstream f(p, std::ios::binary | std::ios::trunc);
        f.write(data.data(), (std::streamsize)data.size());
        if (!f) FAIL("cannot write " + ut::narrow(p.wstring()));
    }

    std::string BuildZip(const std::vector<std::pair<std::string, std::string>>& entries, bool store)
    {
        mz_zip_archive z{};
        if (!mz_zip_writer_init_heap(&z, 0, 0)) FAIL("mz_zip_writer_init_heap failed");
        for (auto& [name, data] : entries)
        {
            mz_uint level = name.ends_with('/') || store ? MZ_NO_COMPRESSION : MZ_BEST_COMPRESSION;
            if (!mz_zip_writer_add_mem(&z, name.c_str(), data.data(), data.size(), level))
                FAIL("mz_zip_writer_add_mem failed for " + name);
        }
        void* buf = nullptr;
        size_t size = 0;
        if (!mz_zip_writer_finalize_heap_archive(&z, &buf, &size)) FAIL("mz_zip_writer_finalize_heap_archive failed");
        std::string out(static_cast<char*>(buf), size);
        mz_free(buf);
        mz_zip_writer_end(&z);
        return out;
    }

    // --- archs

    static std::map<std::string, Arch>& Archs()
    {
        static std::map<std::string, Arch> a;
        return a;
    }

    void InitArchs(const std::map<std::string, std::string>& options, const fs::path& runnerExe)
    {
        // <repo>/bin/<arch>/<config>/tests/ual_tests.exe
        fs::path binRoot = runnerExe.parent_path().parent_path().parent_path().parent_path();
        std::string config = runnerExe.parent_path().parent_path().filename().string();
        if (auto it = options.find("bin"); it != options.end()) binRoot = ut::widen(it->second);
        if (auto it = options.find("config"); it != options.end()) config = it->second;
        for (auto n : { "Win32", "x64" })
        {
            Arch a;
            a.name = n;
            a.bin = binRoot / n / config;
            a.tests = a.bin / L"tests";
            Archs()[n] = a;
        }
    }

    const Arch& GetArch(const std::string& name) { return Archs().at(name); }
    const Arch& OtherArch(const Arch& a) { return GetArch(a.is64() ? "Win32" : "x64"); }

    bool Arch::available() const { return missing().empty(); }

    std::string Arch::missing() const
    {
        std::string m;
        for (auto& p : { loader(), genericHost(), probe() })
            if (!fs::exists(p)) m += ut::narrow(p.wstring()) + " ";
        return m;
    }

    void AddArchTest(const std::string& name, const std::string& tags, std::function<void(const Arch&)> fn, const std::string& only)
    {
        for (std::string archName : { "Win32", "x64" })
        {
            if (!only.empty() && only != archName) continue;
            ut::add_test("[" + archName + "] " + name, tags + "[" + archName + "]", [archName, fn] {
                auto& a = GetArch(archName);
                if (!a.available()) SKIP(archName + " test binaries are not built: missing " + a.missing());
                fn(a);
            });
        }
    }

    ArchReg::ArchReg(const char* name, const char* tags, void (*fn)(const Arch&), const char* only, const char*, int)
    {
        AddArchTest(name, tags, fn, only);
    }

    // --- dialogs

    std::wstring DialogInfo::AllText() const
    {
        std::wstring s = title;
        for (auto& t : texts) s += L"\n" + t;
        return s;
    }

    bool DialogInfo::Contains(std::wstring_view needle) const
    {
        return Lower(AllText()).find(Lower(std::wstring(needle))) != std::wstring::npos;
    }

    static std::vector<std::wstring> UiaNames(HWND hwnd)
    {
        std::vector<std::wstring> out;
        IUIAutomation* uia = nullptr;
        if (FAILED(CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER, __uuidof(IUIAutomation), (void**)&uia)) || !uia)
            return out;
        IUIAutomationElement* root = nullptr;
        if (SUCCEEDED(uia->ElementFromHandle(hwnd, &root)) && root)
        {
            IUIAutomationCondition* cond = nullptr;
            uia->CreateTrueCondition(&cond);
            IUIAutomationElementArray* arr = nullptr;
            if (cond && SUCCEEDED(root->FindAll(TreeScope_Descendants, cond, &arr)) && arr)
            {
                int n = 0;
                arr->get_Length(&n);
                for (int i = 0; i < n; ++i)
                {
                    IUIAutomationElement* e = nullptr;
                    if (SUCCEEDED(arr->GetElement(i, &e)) && e)
                    {
                        BSTR name = nullptr;
                        if (SUCCEEDED(e->get_CurrentName(&name)) && name)
                        {
                            if (SysStringLen(name)) out.emplace_back(name, SysStringLen(name));
                            SysFreeString(name);
                        }
                        e->Release();
                    }
                }
                arr->Release();
            }
            if (cond) cond->Release();
            root->Release();
        }
        uia->Release();
        return out;
    }

    static DialogInfo Inspect(HWND hwnd)
    {
        DialogInfo d;
        d.hwnd = hwnd;
        wchar_t buf[1024];
        GetWindowTextW(hwnd, buf, 1024);
        d.title = buf;
        GetClassNameW(hwnd, buf, 1024);
        d.className = buf;
        EnumChildWindows(hwnd, [](HWND c, LPARAM lp) -> BOOL {
            auto info = reinterpret_cast<DialogInfo*>(lp);
            wchar_t b[2048];
            GetClassNameW(c, b, 2048);
            if (wcscmp(b, L"DirectUIHWND") == 0) info->isTaskDialog = true;
            if (GetWindowTextW(c, b, 2048) > 0) info->texts.push_back(b);
            return TRUE;
        }, reinterpret_cast<LPARAM>(&d));
        // The loader's ModernUI=1 window takes TDM_CLICK_BUTTON like a task dialog and keeps its texts in hidden children
        d.isModern = d.className == L"UltimateASILoaderDialog";
        if (d.isModern) d.isTaskDialog = true;
        else if (d.isTaskDialog)
        {
            // A task dialog fills its UIA tree gradually and the first read can miss the main
            // instruction, so read until two reads agree
            std::vector<std::wstring> names, previous;
            for (int i = 0; i < 20; ++i)
            {
                names = UiaNames(hwnd);
                if (!names.empty() && names == previous) break;
                previous = names;
                Sleep(100);
            }
            for (auto& n : names) d.texts.push_back(n);
        }
        return d;
    }

    static void Dismiss(const DialogInfo& d, int buttonId = 0)
    {
        if (d.isTaskDialog)
        {
            DWORD_PTR res;
            SendMessageTimeoutW(d.hwnd, TDM_CLICK_BUTTON, buttonId ? buttonId : 1000, 0, SMTO_ABORTIFHUNG, 2000, &res);
            return;
        }
        // MessageBox: press the requested or first button
        struct Ctx { int id; HWND btn; } ctx{ buttonId, nullptr };
        EnumChildWindows(d.hwnd, [](HWND c, LPARAM lp) -> BOOL {
            auto ctx = reinterpret_cast<Ctx*>(lp);
            wchar_t cls[64];
            GetClassNameW(c, cls, 64);
            if (_wcsicmp(cls, L"Button") == 0 && (ctx->id == 0 || GetDlgCtrlID(c) == ctx->id)) { ctx->btn = c; return FALSE; }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&ctx));
        if (ctx.btn)
            PostMessageW(d.hwnd, WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(ctx.btn), BN_CLICKED), (LPARAM)ctx.btn);
        else
            PostMessageW(d.hwnd, WM_CLOSE, 0, 0);
    }

    // --- results

    static bool ModMatch(const ualtest::Record& r, const std::string& mod)
    {
        return mod.empty() || _stricmp(r.get("mod").c_str(), mod.c_str()) == 0;
    }

    std::vector<const ualtest::Record*> RunResult::Events(const std::string& src, const std::string& ev) const
    {
        std::vector<const ualtest::Record*> out;
        for (auto& r : records)
            if ((src.empty() || r.get("src") == src) && r.get("ev") == ev) out.push_back(&r);
        return out;
    }

    const ualtest::Record* RunResult::First(const std::string& src, const std::string& ev) const
    {
        auto v = Events(src, ev);
        return v.empty() ? nullptr : v.front();
    }

    const ualtest::Record* RunResult::Last(const std::string& src, const std::string& ev) const
    {
        auto v = Events(src, ev);
        return v.empty() ? nullptr : v.back();
    }

    int RunResult::Inits(const std::string& mod) const
    {
        int n = 0;
        for (auto r : Events("probe", "init")) n += ModMatch(*r, mod);
        return n;
    }

    int RunResult::Attaches(const std::string& mod) const
    {
        int n = 0;
        for (auto r : Events("probe", "attach")) n += ModMatch(*r, mod);
        return n;
    }

    std::vector<std::string> RunResult::InitOrder() const
    {
        std::vector<std::string> v;
        for (auto r : Events("probe", "init")) v.push_back(r->get("mod"));
        return v;
    }

    const ualtest::Record* RunResult::Init(const std::string& mod) const
    {
        for (auto r : Events("probe", "init"))
            if (ModMatch(*r, mod)) return r;
        return nullptr;
    }

    const ualtest::Record* RunResult::Action(const std::string& ev, const std::string& arg) const
    {
        for (auto& r : records)
            if (r.get("src") == "host" && r.get("ev") == ev && r.get("arg") == arg) return &r;
        return nullptr;
    }

    std::string RunResult::Dump(size_t maxLines) const
    {
        std::string s = "exit code " + std::to_string(exitCode) + (timedOut ? " (TIMED OUT)" : "") + ", " + std::to_string(records.size()) + " record(s)";
        size_t start = records.size() > maxLines ? records.size() - maxLines : 0;
        for (size_t i = start; i < records.size(); ++i)
        {
            auto line = ualtest::serialize(records[i]);
            if (line.size() > 400) line = line.substr(0, 400) + "...";
            s += "\n        | " + line;
        }
        for (auto& d : dialogs)
            s += "\n        dialog: " + ut::narrow(d.title) + " / " + ut::narrow(d.AllText().substr(0, 300));
        return s;
    }

    void ExpectHostOk(const RunResult& r, const char* file, int line)
    {
        if (r.timedOut) ut::fail_impl(file, line, "the host process timed out\n      " + r.Dump(), true);
        if (!r.HostFinished()) ut::fail_impl(file, line, "the host process did not finish normally\n      " + r.Dump(), true);
        if (r.exitCode != 0) ut::fail_impl(file, line, "the host process exited with " + std::to_string(r.exitCode) + "\n      " + r.Dump(), true);
    }

    void ForgiveUserInterference(const RunResult& r)
    {
        auto c = ut::current();
        if (c && !c->failures.empty() && r.userInput)
        {
            std::string first = c->failures.front().message;
            c->failures.clear();
            throw ut::SkipSignal{ "inconclusive: keyboard/mouse input reached the session while the dialog was shown (" + first.substr(0, first.find('\n')) + ")" };
        }
    }

    void ExpectScenarioPassed(const RunResult& r, const std::string& scenario, const char* file, int line)
    {
        for (auto rec : r.Events("host", "skip"))
            if (rec->get("scenario") == scenario) throw ut::SkipSignal{ rec->get("reason") };

        const ualtest::Record* done = nullptr;
        for (auto rec : r.Events("host", "scenario_done"))
            if (rec->get("scenario") == scenario) done = rec;

        int failed = 0, checks = 0;
        for (auto rec : r.Events("host", "check"))
        {
            if (rec->get("scenario") != scenario) continue;
            ++checks;
            if (rec->get("ok") != "1")
            {
                ++failed;
                ut::fail_impl(file, line, "[" + scenario + "] " + rec->get("name") + (rec->get("detail").empty() ? "" : "\n      " + rec->get("detail")), false);
            }
        }
        if (!done && r.exitCode == 0xDEAD10CC)
            ut::fail_impl(file, line, "scenario '" + scenario + "': the host watchdog detected a deadlock and terminated the process", true);
        if (!done)
            ut::fail_impl(file, line, "scenario '" + scenario + "' did not complete (crash, hang or early exit)\n      " + r.Dump(), true);
        if (checks == 0)
            ut::fail_impl(file, line, "scenario '" + scenario + "' performed no checks", true);
        if (!failed && (r.timedOut || r.exitCode != 0))
            ut::fail_impl(file, line, "the host process did not exit cleanly\n      " + r.Dump(), true);
    }

    // --- sandbox

    static std::wstring RunId()
    {
        static std::wstring id = [] {
            SYSTEMTIME st;
            GetLocalTime(&st);
            wchar_t b[64];
            swprintf_s(b, L"%04d%02d%02d-%02d%02d%02d-%lu", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, GetCurrentProcessId());
            return std::wstring(b);
        }();
        return id;
    }

    static std::atomic<int> g_sandboxCounter{ 0 };

    Sandbox::Sandbox(const Arch& a, const std::wstring& gameDirName) : arch(a)
    {
        std::wstring name;
        if (auto r = ut::current())
            for (char c : r->test->name)
                if (isalnum((unsigned char)c) || c == '-' || c == '_') name += (wchar_t)c;
                else if (!name.empty() && name.back() != L'-') name += L'-';
        if (name.size() > 48) name.resize(48);
        wchar_t idx[16];
        swprintf_s(idx, L"%04d-", ++g_sandboxCounter);
        root = fs::temp_directory_path() / L"ual-tests" / RunId() / (idx + name);
        game = root / gameDirName;
        report = root / L"report.log";
        std::error_code ec;
        fs::remove_all(root, ec);
        fs::create_directories(game, ec);
        if (ec) FAIL("cannot create sandbox " + ut::narrow(game.wstring()) + ": " + ec.message());
    }

    Sandbox::~Sandbox()
    {
        auto r = ut::current();
        bool failed = r && !r->failures.empty();
        if (ut::options().keep || failed)
        {
            if (r) r->notes.push_back("sandbox kept at " + ut::narrow(root.wstring()));
            return;
        }
        std::error_code ec;
        for (int i = 0; i < 20; ++i)
        {
            fs::remove_all(root, ec);
            if (!ec) break;
            Sleep(100);
        }
    }

    void Sandbox::Mkdir(const fs::path& rel) const
    {
        std::error_code ec;
        fs::create_directories(game / rel, ec);
    }

    void Sandbox::Write(const fs::path& rel, const std::string& content) const { WriteFileBytes(game / rel, content); }
    std::string Sandbox::Read(const fs::path& rel) const { return ReadFileBytes(game / rel); }
    bool Sandbox::Exists(const fs::path& rel) const { return fs::exists(game / rel); }

    void Sandbox::Ini(const fs::path& rel, const wchar_t* section, const wchar_t* key, const std::wstring& value) const
    {
        auto p = game / rel;
        std::error_code ec;
        if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
        if (!WritePrivateProfileStringW(section, key, value.c_str(), p.c_str()))
            FAIL("cannot write ini " + ut::narrow(p.wstring()));
    }

    void Sandbox::Copy(const fs::path& src, const fs::path& rel) const
    {
        auto dst = game / rel;
        std::error_code ec;
        if (dst.has_parent_path()) fs::create_directories(dst.parent_path(), ec);
        if (!fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec))
            FAIL("cannot copy " + ut::narrow(src.wstring()) + " -> " + ut::narrow(dst.wstring()) + ": " + ec.message());
    }

    void Sandbox::Loader(const std::wstring& asName, const fs::path& dir) const
    {
        Copy(arch.loader(), dir / asName);
    }

    fs::path Sandbox::Host(const std::wstring& proxyBase, const std::wstring& asName)
    {
        auto src = proxyBase.empty() ? arch.genericHost() : arch.staticHost(proxyBase);
        if (!fs::exists(src)) SKIP("host binary not built: " + ut::narrow(src.wstring()));
        Copy(src, asName);
        hostExe = game / asName;
        return hostExe;
    }

    void Sandbox::Probe(const fs::path& rel, const Arch* fromArch) const
    {
        Copy((fromArch ? *fromArch : arch).probe(), rel);
    }

    void Sandbox::Zip(const fs::path& rel, const std::vector<std::pair<std::string, std::string>>& entries, bool store) const
    {
        WriteFileBytes(game / rel, BuildZip(entries, store));
    }

    static std::wstring QuoteArg(const std::wstring& a)
    {
        if (!a.empty() && a.find_first_of(L" \t\"") == std::wstring::npos) return a;
        std::wstring r = L"\"";
        size_t bs = 0;
        for (wchar_t c : a)
        {
            if (c == L'\\') { ++bs; continue; }
            if (c == L'"') r.append(bs * 2 + 1, L'\\');
            else r.append(bs, L'\\');
            bs = 0;
            r += c;
        }
        r.append(bs * 2, L'\\');
        r += L'"';
        return r;
    }

    RunResult Sandbox::Run(const std::vector<std::wstring>& args, RunOptions opts) const
    {
        if (hostExe.empty()) FAIL("Sandbox::Run called before Sandbox::Host");
        std::error_code ec;
        fs::remove(report, ec);

        std::wstring cmd = QuoteArg(hostExe.wstring());
        for (auto& a : args) cmd += L" " + QuoteArg(a);

        // Inherited environment plus report path and extras
        std::map<std::wstring, std::wstring, decltype([](const std::wstring& a, const std::wstring& b) { return _wcsicmp(a.c_str(), b.c_str()) < 0; })> env;
        if (auto block = GetEnvironmentStringsW())
        {
            for (auto p = block; *p; p += wcslen(p) + 1)
            {
                std::wstring kv = p;
                auto eq = kv.find(L'=', 1);
                if (eq != std::wstring::npos) env[kv.substr(0, eq)] = kv.substr(eq + 1);
            }
            FreeEnvironmentStringsW(block);
        }
        env[ualtest::kReportEnv] = report.wstring();
        for (auto& [k, v] : opts.env) env[k] = v;
        std::wstring envBlock;
        for (auto& [k, v] : env) envBlock += k + L"=" + v + L'\0';
        envBlock += L'\0';

        auto cwd = opts.cwd.empty() ? game : opts.cwd;

        HANDLE job = CreateJobObjectW(nullptr, nullptr);
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION li{};
        li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &li, sizeof(li));

        STARTUPINFOW si{ sizeof(si) };
        PROCESS_INFORMATION pi{};
        auto start = GetTickCount64();
        DWORD startTick32 = GetTickCount();
        if (!CreateProcessW(hostExe.c_str(), cmd.data(), nullptr, nullptr, FALSE, CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT | CREATE_NO_WINDOW,
                envBlock.data(), cwd.c_str(), &si, &pi))
        {
            DWORD e = GetLastError();
            CloseHandle(job);
            FAIL("CreateProcess failed for " + ut::narrow(hostExe.wstring()) + ": error " + std::to_string(e));
        }
        AssignProcessToJobObject(job, pi.hProcess);
        ResumeThread(pi.hThread);

        RunResult res;
        std::map<HWND, ULONGLONG> handled;
        for (;;)
        {
            DWORD w = WaitForSingleObject(pi.hProcess, 50);
            if (w == WAIT_OBJECT_0) break;
            if (GetTickCount64() - start > opts.timeoutMs)
            {
                res.timedOut = true;
                TerminateJobObject(job, 0xDEAD);
                WaitForSingleObject(pi.hProcess, 5000);
                break;
            }

            struct Ctx { DWORD pid; std::vector<HWND> wnds; } ctx{ pi.dwProcessId, {} };
            EnumWindows([](HWND h, LPARAM lp) -> BOOL {
                auto c = reinterpret_cast<Ctx*>(lp);
                DWORD pid = 0;
                GetWindowThreadProcessId(h, &pid);
                wchar_t cls[64];
                if (pid == c->pid && IsWindowVisible(h) && GetClassNameW(h, cls, 64) && (wcscmp(cls, L"#32770") == 0 || wcscmp(cls, L"UltimateASILoaderDialog") == 0))
                    c->wnds.push_back(h);
                return TRUE;
            }, reinterpret_cast<LPARAM>(&ctx));

            for (auto& d : res.dialogs) // record when dialogs close
                if (d.closedAt < 0 && !IsWindow(d.hwnd))
                    d.closedAt = (GetTickCount64() - start) / 1000.0;

            for (HWND h : ctx.wnds)
            {
                auto it = handled.find(h);
                auto now = GetTickCount64();
                if (it != handled.end())
                {
                    if (it->second != 0 && now - it->second > 3000) // still open, dismiss again
                    {
                        Dismiss(Inspect(h));
                        it->second = now;
                    }
                    continue;
                }
                double shown = (now - start) / 1000.0;
                if (opts.freezeCountdown)
                {
                    PostMessageW(h, WM_KEYDOWN, VK_SHIFT, 0);
                    PostMessageW(h, WM_KEYUP, VK_SHIFT, 0xC0000001);
                }
                Sleep(150); // let the dialog finish layout
                auto info = Inspect(h);
                info.shownAt = shown;
                res.dialogs.push_back(info);
                DialogAction act = opts.onDialog ? opts.onDialog(info) : DialogAction{};
                if (act.kind == DialogAction::Leave)
                    handled[h] = 0;
                else
                {
                    Dismiss(info, act.kind == DialogAction::ClickButton ? act.buttonId : 0);
                    handled[h] = now;
                }
            }
        }

        for (auto& d : res.dialogs)
            if (d.closedAt < 0 && !IsWindow(d.hwnd))
                d.closedAt = (GetTickCount64() - start) / 1000.0;
        LASTINPUTINFO lii{ sizeof(lii) };
        if (GetLastInputInfo(&lii))
            res.userInput = (LONG)(lii.dwTime - startTick32) >= 0;
        GetExitCodeProcess(pi.hProcess, &res.exitCode);
        res.seconds = (GetTickCount64() - start) / 1000.0;
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        CloseHandle(job); // kills leftovers such as the virtual file server

        res.records = ualtest::parse_records(ReadFileBytes(report));
        return res;
    }
}
