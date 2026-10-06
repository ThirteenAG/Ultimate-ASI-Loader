#include "server.hpp"
#include "internal.hpp"
#include "../core/loader.hpp"
#include "../core/paths.hpp"
#include <mutex>
#include <string>

namespace ual::vfs::server
{
#ifndef _WIN64
    namespace
    {
        HANDLE g_pipe = INVALID_HANDLE_VALUE;
        std::mutex g_lock; // one request at a time on the pipe
        std::once_flag g_started;

        void Disconnect()
        {
            CloseHandle(g_pipe);
            g_pipe = INVALID_HANDLE_VALUE;
        }

        bool WriteAll(const void* data, size_t size)
        {
            auto p = (const uint8_t*)data;
            while (size)
            {
                DWORD chunk = (DWORD)(std::min<size_t>)(size, 1u << 24), written = 0;
                if (!WriteFile(g_pipe, p, chunk, &written, nullptr) || !written) return false;
                p += written;
                size -= written;
            }
            return true;
        }

        bool ReadAll(void* data, size_t size)
        {
            auto p = (uint8_t*)data;
            while (size)
            {
                DWORD got = 0;
                if (!ReadFile(g_pipe, p, (DWORD)size, &got, nullptr) || !got) return false;
                p += got;
                size -= got;
            }
            return true;
        }
    }

    bool Available()
    {
        return g_pipe != INVALID_HANDLE_VALUE;
    }

    void Start()
    {
        std::call_once(g_started, [] {
            const auto& self = Self();
            std::wstring exe = self.dir + self.stem + L".exe";
            if (!FileExists(exe)) return;

            // kill-on-close job that is never closed, so the server dies with the game
            HANDLE job = CreateJobObjectW(nullptr, nullptr);
            if (!job) return;
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));

            std::wstring pid = std::to_wstring(GetCurrentProcessId());
            std::wstring cmd = L"\"" + exe + L"\" " + pid;
            STARTUPINFOW si{ sizeof(si) };
            PROCESS_INFORMATION pi{};
            if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, self.dir.c_str(), &si, &pi))
            {
                CloseHandle(job);
                return;
            }
            if (!AssignProcessToJobObject(job, pi.hProcess) || ResumeThread(pi.hThread) == (DWORD)-1)
            {
                TerminateProcess(pi.hProcess, 0);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
                CloseHandle(job);
                return;
            }
            CloseHandle(pi.hThread);

            std::wstring pipeName = L"\\\\.\\pipe\\Ultimate-ASI-Loader-VirtualFileServer-" + pid;
            for (int attempt = 0; attempt < 40 && g_pipe == INVALID_HANDLE_VALUE; ++attempt)
            {
                if (WaitForSingleObject(pi.hProcess, 0) == WAIT_OBJECT_0) break; // the server exited
                if (WaitNamedPipeW(pipeName.c_str(), 50))
                {
                    g_pipe = CreateFileW(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
                    // the name is predictable: only talk to the server we started
                    ULONG serverPid = 0;
                    if (g_pipe != INVALID_HANDLE_VALUE && (!GetNamedPipeServerProcessId(g_pipe, &serverPid) || serverPid != pi.dwProcessId))
                    {
                        CloseHandle(g_pipe);
                        g_pipe = INVALID_HANDLE_VALUE;
                        break;
                    }
                }
                else
                    Sleep(25);
            }
            CloseHandle(pi.hProcess);
        });
    }

    uint64_t Add(const uint8_t* data, size_t size)
    {
        std::lock_guard lock(g_lock);
        if (!Available()) return 0;
        Command c{ Command::Add, 0, size, 0, 0 };
        uint64_t handle = 0;
        if (!WriteAll(&c, sizeof(c)) || !WriteAll(data, size) || !ReadAll(&handle, sizeof(handle)))
        {
            Disconnect();
            return 0;
        }
        return handle;
    }

    bool Append(uint64_t handle, const uint8_t* data, size_t size)
    {
        std::lock_guard lock(g_lock);
        if (!Available()) return false;
        Command c{ Command::Append, handle, size, 0, 0 };
        if (!WriteAll(&c, sizeof(c)) || !WriteAll(data, size))
        {
            Disconnect();
            return false;
        }
        return true;
    }

    void Remove(uint64_t handle)
    {
        std::lock_guard lock(g_lock);
        if (!Available()) return;
        Command c{ Command::Remove, handle, 0, 0, 0 };
        if (!WriteAll(&c, sizeof(c))) Disconnect();
    }

    bool Read(uint64_t handle, uint64_t offset, void* buffer, uint32_t count, uint32_t& read)
    {
        read = 0;
        std::lock_guard lock(g_lock);
        if (!Available()) return false;
        Command c{ Command::Read, handle, count, 0, offset };
        uint32_t length = 0;
        if (!WriteAll(&c, sizeof(c)) || !ReadAll(&length, sizeof(length)) || length > count || !ReadAll(buffer, length))
        {
            Disconnect();
            return false;
        }
        read = length;
        return true;
    }
#else
    bool Available() { return false; }
    void Start() {}
    uint64_t Add(const uint8_t*, size_t) { return 0; }
    bool Append(uint64_t, const uint8_t*, size_t) { return false; }
    void Remove(uint64_t) {}
    bool Read(uint64_t, uint64_t, void*, uint32_t, uint32_t&) { return false; }
#endif
}

namespace ual::vfs
{
    namespace
    {
        FILETIME Now()
        {
            FILETIME ft;
            GetSystemTimeAsFileTime(&ft);
            return ft;
        }

        class MemoryData final : public FileData
        {
        public:
            MemoryData(const uint8_t* data, size_t size) : bytes_(data, data + size) { created = written = Now(); }
            uint64_t Size() const override
            {
                AcquireSRWLockShared(&lock_);
                uint64_t s = bytes_.size();
                ReleaseSRWLockShared(&lock_);
                return s;
            }
            bool Read(uint64_t offset, void* buffer, DWORD count, DWORD& read) override
            {
                AcquireSRWLockShared(&lock_);
                read = offset < bytes_.size() ? (DWORD)(std::min<uint64_t>)(count, bytes_.size() - offset) : 0;
                if (read) memcpy(buffer, bytes_.data() + offset, read);
                ReleaseSRWLockShared(&lock_);
                return true;
            }
            bool Append(const uint8_t* data, size_t size) override
            {
                AcquireSRWLockExclusive(&lock_);
                bytes_.insert(bytes_.end(), data, data + size);
                written = Now();
                ReleaseSRWLockExclusive(&lock_);
                return true;
            }

        private:
            std::vector<uint8_t> bytes_;
            mutable SRWLOCK lock_ = SRWLOCK_INIT;
        };

        class ServerData final : public FileData
        {
        public:
            ServerData(uint64_t handle, uint64_t size) : handle_(handle), size_(size) { created = written = Now(); }
            ~ServerData() override { server::Remove(handle_); }
            uint64_t Size() const override { return size_; }
            bool Read(uint64_t offset, void* buffer, DWORD count, DWORD& read) override
            {
                read = 0;
                if (offset >= size_) return true;
                uint32_t got = 0;
                bool ok = server::Read(handle_, offset, buffer, (uint32_t)(std::min<uint64_t>)(count, size_ - offset), got);
                read = got;
                return ok;
            }
            bool Append(const uint8_t* data, size_t size) override
            {
                if (!server::Append(handle_, data, size)) return false;
                size_ += size;
                written = Now();
                return true;
            }

        private:
            uint64_t handle_;
            std::atomic<uint64_t> size_;
        };
    }

    std::shared_ptr<FileData> MakeMemoryData(const uint8_t* data, size_t size)
    {
        if (server::Available())
            if (uint64_t h = server::Add(data, size)) return std::make_shared<ServerData>(h, size);
        return std::make_shared<MemoryData>(data, size);
    }
}
