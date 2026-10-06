#include "cxxsnippets.hpp"
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <algorithm>
#include <cstring>

extern "C" __declspec(noinline) int case_add(int a, int b)
{
    volatile int x = a, y = b;
    return x + y;
}
struct Interface
{
    virtual int Add(int x)
    {
        return x + 42;
    }
    virtual int Other(int x)
    {
        return x - 3;
    }
};
static Interface object;
extern "C" void *case_object()
{
    return &object;
}
static unsigned char *midCode = nullptr;
extern "C" void *case_mid_site()
{
    return midCode + (sizeof(void *) == 8 ? 5 : 7);
}
// Hooking.Patterns scans through the last executable section of the module.
#pragma section(".cxxs", read, execute)
__declspec(allocate(".cxxs")) static volatile unsigned char patchBytes[] = {0x74, 0x10, 0x53, 0x53, 0x6a, 0x1b};
// A missing golden file is a test setup error, not an empty expectation.
static std::string Read(const std::string &path, bool required = true)
{
    std::ifstream f(path, std::ios::binary);
    if (!f)
    {
        if (!required)
            return {};
        std::fprintf(stderr, "FAIL: cannot read %s\n", path.c_str());
        std::exit(1);
    }
    std::ostringstream s;
    s << f.rdbuf();
    // Child() strips CR from the actual output; a checkout with CRLF conversion must compare equal too
    std::string text;
    for (char c : s.str())
        if (c != '\r')
            text += c;
    return text;
}
static void Require(bool b, const char *message)
{
    if (!b)
    {
        std::fprintf(stderr, "FAIL: %s\n", message);
        std::exit(1);
    }
}
static int Observe(const std::string &name)
{
    if (name == "midhook")
        return ((int (*)(int))midCode)(2);
    if (name == "vmthook")
    {
        auto p = (Interface *)case_object();
        return p->Add(2);
    }
    if (name == "pattern")
        return patchBytes[0];
    if (name == "dll")
    {
        MSG m{};
        PeekMessageA(&m, nullptr, 0, 0, PM_NOREMOVE);
        PeekMessageW(&m, nullptr, 0, 0, PM_NOREMOVE);
        return 1;
    }
    return case_add(2, 3);
}
int CaseChild(const std::string &name, const std::string &mode)
{
    midCode = (unsigned char *)VirtualAlloc(nullptr, 64, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Require(midCode != nullptr, "mid code allocation");
    std::memset(midCode, 0x90, 64);
    const unsigned char x64[] = {0x8b, 0xc1, 0x83, 0xc0, 0x2a};
    const unsigned char x86[] = {0x8b, 0x44, 0x24, 0x04, 0x83, 0xc0, 0x2a};
    std::memcpy(midCode, sizeof(void *) == 8 ? x64 : x86, sizeof(void *) == 8 ? sizeof(x64) : sizeof(x86));
    midCode[32] = 0xc3;
    DWORD old;
    VirtualProtect(midCode, 64, PAGE_EXECUTE_READ, &old);
    FlushInstructionCache(GetCurrentProcess(), midCode, 64);
    int before = Observe(name);
    HMODULE native = nullptr;
    std::unique_ptr<cxxsnippets::Module> module;
    void (*shutdown)() = nullptr;
    std::atomic<bool> stop = false;
    std::thread worker;
    if (name == "threadsafe")
        worker = std::thread([&] {
            while (!stop.load())
            {
                int result = case_add(1, 1);
                Require(result == 2 || result == 102, "concurrent inline hook result");
            }
        });
    if (mode == "runtime")
    {
        std::vector<cxxsnippets::Diagnostic> errors;
        module = cxxsnippets::CompileFile(std::string(CXXSNIPPETS_TEST_ROOT) + "/cases/" + name + ".cxx", {}, errors);
        for (auto &e : errors)
            std::fprintf(stderr, "%s\n", e.ToString().c_str());
        Require(module != nullptr, "compile use case");
        Require(module->RunInit(), "use case Init");
    }
    else
    {
        char exe[MAX_PATH];
        GetModuleFileNameA(nullptr, exe, MAX_PATH);
        std::string path = exe;
        path = path.substr(0, path.find_last_of("\\/") + 1) + "case-" + name + ".dll";
        native = LoadLibraryA(path.c_str());
        Require(native != nullptr, "load native DLL");
        auto init = (void (*)())GetProcAddress(native, "CaseInit");
        shutdown = (void (*)())GetProcAddress(native, "CaseShutdown");
        Require(init && shutdown, "native shim exports");
        init();
    }
    int during = Observe(name);
    stop = true;
    if (worker.joinable())
        worker.join();
    if (module)
    {
        module->RunShutdown();
        module.reset();
    }
    else
    {
        shutdown();
        FreeLibrary(native);
    }
    int after = Observe(name);
    std::printf("%s: %d %d %d\n", name.c_str(), before, during, after);
    VirtualFree(midCode, 0, MEM_RELEASE);
    return 0;
}
static std::string Child(const std::string &name, const std::string &mode)
{
    SECURITY_ATTRIBUTES security{sizeof(security), nullptr, TRUE};
    HANDLE read, write;
    Require(CreatePipe(&read, &write, &security, 0) != 0, "create child pipe");
    SetHandleInformation(read, HANDLE_FLAG_INHERIT, 0);
    char exe[MAX_PATH];
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string command = "\"" + std::string(exe) + "\" --case " + name + " " + mode;
    STARTUPINFOA start{};
    start.cb = sizeof(start);
    start.dwFlags = STARTF_USESTDHANDLES;
    start.hStdOutput = write;
    start.hStdError = write;
    start.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi{};
    Require(CreateProcessA(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &start,
                           &pi) != 0,
            "start child case");
    CloseHandle(write);
    std::string result;
    char buffer[4096];
    DWORD n;
    ULONGLONG deadline = GetTickCount64() + 30000;
    for (;;)
    {
        DWORD available = 0;
        if (!PeekNamedPipe(read, nullptr, 0, nullptr, &available, nullptr))
            break;
        if (available)
        {
            if (!ReadFile(read, buffer, std::min<DWORD>(available, sizeof(buffer)), &n, nullptr))
                break;
            result.append(buffer, n);
            continue;
        }
        if (WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0)
            break;
        if (GetTickCount64() > deadline)
        {
            TerminateProcess(pi.hProcess, 1);
            Require(false, "child case timeout");
        }
        Sleep(5);
    }
    Require(WaitForSingleObject(pi.hProcess, 30000) == WAIT_OBJECT_0, "child timeout");
    DWORD code;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(read);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    if (code)
    {
        std::fprintf(stderr, "%s %s failed (exit %lu):\n%s", name.c_str(), mode.c_str(), code, result.c_str());
        std::exit(1);
    }
    std::string normalized;
    for (char c : result)
        if (c != '\r')
            normalized += c;
    return normalized;
}
void UseCases()
{
    for (const char *name : {"minimal", "multiple", "threadsafe", "midhook", "vmthook", "dll", "pattern"})
    {
        auto expected = Read(std::string(CXXSNIPPETS_TEST_ROOT) + "/golden/" + name + ".txt");
        for (const char *mode : {"runtime", "native"})
        {
            // golden/<name>.<mode>.txt where modes differ on purpose: unloading a snippet reverts
            // its memory writes, a native DLL doesn't
            auto specific = Read(std::string(CXXSNIPPETS_TEST_ROOT) + "/golden/" + name + "." + mode + ".txt", false);
            const auto &want = specific.empty() ? expected : specific;
            auto actual = Child(name, mode);
            if (actual != want)
            {
                std::fprintf(stderr, "FAIL: %s %s\nexpected: %sactual: %s", name, mode, want.c_str(),
                             actual.c_str());
                std::exit(1);
            }
        }
        std::printf("PASS: %s (runtime and native)\n", name);
    }
}
