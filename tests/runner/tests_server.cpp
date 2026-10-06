// Protocol tests against VirtualFileServer.exe, the x64 helper that stores virtual files
// for x86 games, plus a client-side check.
#include "framework.hpp"

using namespace runner;

namespace
{
    // Must match source/loader/vfs/server.hpp and source/VirtualFileServer/VirtualFileServer.cpp
#pragma pack(push, 4)
    struct ServerCommand
    {
        uint32_t command;
        uint64_t handle;
        uint64_t size;
        int32_t reserved;
        uint64_t offset;
    };
#pragma pack(pop)
    static_assert(sizeof(ServerCommand) == 32);
    enum : uint32_t { ADD_FILE = 1, APPEND_FILE = 2, REMOVE_FILE = 3, READ_FILE = 5 };

    // Serves only the pid on its command line, over \\.\pipe\Ultimate-ASI-Loader-VirtualFileServer-<pid>
    std::wstring PipeName()
    {
        return L"\\\\.\\pipe\\Ultimate-ASI-Loader-VirtualFileServer-" + std::to_wstring(GetCurrentProcessId());
    }

    struct Server
    {
        PROCESS_INFORMATION pi{};
        HANDLE job = nullptr;
        HANDLE pipe = INVALID_HANDLE_VALUE;

        ~Server()
        {
            if (pipe != INVALID_HANDLE_VALUE) CloseHandle(pipe);
            if (job) { TerminateJobObject(job, 0); CloseHandle(job); }
            if (pi.hProcess) CloseHandle(pi.hProcess);
            if (pi.hThread) CloseHandle(pi.hThread);
        }

        void Start(const fs::path& exe, const std::wstring& args)
        {
            job = CreateJobObjectW(nullptr, nullptr);
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION li{};
            li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(job, JobObjectExtendedLimitInformation, &li, sizeof(li));
            STARTUPINFOW si{ sizeof(si) };
            std::wstring cmd = L"\"" + exe.wstring() + L"\" " + args;
            if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, exe.parent_path().c_str(), &si, &pi))
                FAIL("cannot start " + ut::narrow(exe.wstring()));
            AssignProcessToJobObject(job, pi.hProcess);
            ResumeThread(pi.hThread);
        }

        void StartForThisProcess(const fs::path& exe) { Start(exe, std::to_wstring(GetCurrentProcessId())); }

        bool Connect()
        {
            for (int i = 0; i < 100; ++i)
            {
                pipe = CreateFileW(PipeName().c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
                if (pipe != INVALID_HANDLE_VALUE) return true;
                if (WaitForSingleObject(pi.hProcess, 50) == WAIT_OBJECT_0) return false;
            }
            return false;
        }

        bool Exited(DWORD ms, DWORD* code = nullptr)
        {
            if (WaitForSingleObject(pi.hProcess, ms) != WAIT_OBJECT_0) return false;
            if (code) GetExitCodeProcess(pi.hProcess, code);
            return true;
        }

        void Send(const ServerCommand& c, const std::string& payload = {})
        {
            DWORD n = 0;
            REQUIRE(WriteFile(pipe, &c, sizeof(c), &n, nullptr) && n == sizeof(c));
            if (!payload.empty()) REQUIRE(WriteFile(pipe, payload.data(), (DWORD)payload.size(), &n, nullptr) && n == payload.size());
        }

        bool ReadExact(void* p, DWORD size)
        {
            auto b = (char*)p;
            while (size)
            {
                DWORD n = 0;
                if (!ReadFile(pipe, b, size, &n, nullptr) || !n) return false;
                b += n;
                size -= n;
            }
            return true;
        }

        uint64_t Add(const std::string& data)
        {
            Send({ ADD_FILE, 0, data.size(), 0, 0 }, data);
            uint64_t h = 0;
            REQUIRE(ReadExact(&h, sizeof(h)));
            return h;
        }

        std::string Read(uint64_t h, uint64_t off, uint64_t len)
        {
            Send({ READ_FILE, h, len, 0, off });
            DWORD size = 0;
            REQUIRE(ReadExact(&size, sizeof(size)));
            std::string s(size, '\0');
            if (size) REQUIRE(ReadExact(s.data(), size));
            return s;
        }
    };

    const fs::path& ServerExe()
    {
        static fs::path p = GetArch("x64").virtualFileServer();
        if (!fs::exists(p)) SKIP("VirtualFileServer.exe is not built: " + ut::narrow(p.wstring()));
        return p;
    }
}

TEST_CASE("VirtualFileServer refuses to start without a valid client process", "[server]")
{
    auto& exe = ServerExe();
    for (const wchar_t* args : { L"", L"0", L"notanumber" })
    {
        Server s;
        s.Start(exe, args);
        DWORD code = 0;
        REQUIRE_MSG(s.Exited(10000, &code), "the server keeps running without a client: '" + ut::narrow(args) + "'");
        CHECK_EQ(code, (DWORD)1);
    }
}

TEST_CASE("VirtualFileServer stores, appends, reads and removes files", "[server]")
{
    auto& exe = ServerExe();
    Server s;
    s.StartForThisProcess(exe);
    REQUIRE_MSG(s.Connect(), "cannot connect to the server pipe");

    uint64_t h = s.Add("hello");
    CHECK(h >= 1);
    CHECK_EQ(s.Read(h, 0, 100), std::string("hello"));
    CHECK_EQ(s.Read(h, 1, 3), std::string("ell"));
    CHECK_EQ(s.Read(h, 5, 10), std::string());
    CHECK_EQ(s.Read(h, 1000, 10), std::string());

    s.Send({ APPEND_FILE, h, 6, 0, 0 }, " world");
    CHECK_EQ(s.Read(h, 0, 100), std::string("hello world"));

    uint64_t h2 = s.Add("second");
    CHECK_NE(h2, h);
    CHECK_EQ(s.Read(h2, 0, 100), std::string("second"));

    s.Send({ REMOVE_FILE, h, 0, 0, 0 });
    CHECK_EQ(s.Read(h, 0, 100), std::string());
    CHECK_EQ(s.Read(9999, 0, 100), std::string());
    CHECK_EQ(s.Read(h2, 0, 100), std::string("second"));

    std::string big(16 << 20, 'z'); // larger than the pipe buffers
    big[12345] = 'y';
    uint64_t h3 = s.Add(big);
    CHECK(s.Read(h3, 0, big.size()) == big);
    CHECK_EQ(s.Read(h3, 12344, 3), std::string("zyz"));

    // server exits when its client disconnects
    CloseHandle(s.pipe);
    s.pipe = INVALID_HANDLE_VALUE;
    CHECK_MSG(s.Exited(10000), "the server keeps running after the client disconnected");
}

TEST_CASE("VirtualFileServer serves only its own client and stops on an invalid command", "[server]")
{
    auto& exe = ServerExe();
    Server s;
    s.StartForThisProcess(exe);
    REQUIRE(s.Connect());
    // One pipe instance per client, so a second connection is refused
    HANDLE second = CreateFileW(PipeName().c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    CHECK(second == INVALID_HANDLE_VALUE);
    if (second != INVALID_HANDLE_VALUE) CloseHandle(second);

    CHECK_EQ(s.Read(s.Add("x"), 0, 1), std::string("x"));
    s.Send({ 77, 0, 0, 0, 0 }); // stream can't resync after an unknown command
    CHECK_MSG(s.Exited(10000), "the server keeps running after an invalid command");
}

WIN32_TEST("the 32-bit loader starts and connects to the virtual file server", "[server][vfs]")
{
    auto& exe = ServerExe();
    Sandbox sb(arch);
    sb.Loader();
    sb.Host(L"dinput8");
    sb.Copy(exe, L"dinput8.exe"); // named like the loader, as in the release package
    sb.Write(L"update/data.txt", "UPDATED");
    auto r = sb.Run({ L"scenario:vfs_server|dinput8.exe" });
    REQUIRE_SCENARIO(r, "vfs_server");
}
