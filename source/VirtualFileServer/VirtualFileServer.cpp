// Holds virtual file contents outside a 32-bit game's address space (protocol in
// source/loader/vfs/server.hpp). Serves the one game whose pid is on the command line and exits with it.
#include <windows.h>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

#pragma pack(push, 4)
struct Command
{
    enum Type : uint32_t
    {
        Add = 1,
        Append = 2,
        Remove = 3,
        Read = 5,
    };
    uint32_t command;
    uint64_t handle;
    uint64_t size;
    int32_t reserved;
    uint64_t offset;
};
#pragma pack(pop)

static bool ReadAll(HANDLE pipe, void* data, size_t size)
{
    auto p = (uint8_t*)data;
    while (size)
    {
        DWORD got = 0;
        if (!ReadFile(pipe, p, (DWORD)(size < (1u << 24) ? size : (1u << 24)), &got, nullptr) || !got) return false;
        p += got;
        size -= got;
    }
    return true;
}

static bool WriteAll(HANDLE pipe, const void* data, size_t size)
{
    auto p = (const uint8_t*)data;
    while (size)
    {
        DWORD written = 0;
        if (!WriteFile(pipe, p, (DWORD)(size < (1u << 24) ? size : (1u << 24)), &written, nullptr) || !written) return false;
        p += written;
        size -= written;
    }
    return true;
}

int wmain(int argc, wchar_t** argv)
{
    if (argc < 2) return 1;
    DWORD clientPid = (DWORD)wcstoul(argv[1], nullptr, 10);
    HANDLE client = OpenProcess(SYNCHRONIZE, FALSE, clientPid);
    if (!client) return 1;

    std::wstring name = L"\\\\.\\pipe\\Ultimate-ASI-Loader-VirtualFileServer-" + std::to_wstring(clientPid);
    HANDLE pipe = CreateNamedPipeW(name.c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE,
                                   PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 1 << 16, 1 << 16, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE) return 1;

    // the game put us in a kill-on-close job, so if it exits before connecting we die with it
    if (!ConnectNamedPipe(pipe, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) return 1;
    // only the game that started us may use the store
    ULONG peer = 0;
    if (!GetNamedPipeClientProcessId(pipe, &peer) || peer != clientPid) return 1;

    constexpr uint64_t kMaxTransfer = 1ull << 32; // a single virtual file or append

    std::unordered_map<uint64_t, std::vector<uint8_t>> files;
    uint64_t nextHandle = 1;
    std::vector<uint8_t> buffer;
    for (;;)
    {
        Command c{};
        if (!ReadAll(pipe, &c, sizeof(c))) break; // the game exited or closed the pipe
        switch (c.command)
        {
        case Command::Add:
        {
            if (c.size > kMaxTransfer) return 0;
            std::vector<uint8_t> data((size_t)c.size);
            if (!ReadAll(pipe, data.data(), data.size())) return 0;
            uint64_t h = nextHandle++;
            files.emplace(h, std::move(data));
            if (!WriteAll(pipe, &h, sizeof(h))) return 0;
            break;
        }
        case Command::Append:
        {
            if (c.size > kMaxTransfer) return 0;
            buffer.resize((size_t)c.size);
            if (!ReadAll(pipe, buffer.data(), buffer.size())) return 0;
            if (auto it = files.find(c.handle); it != files.end()) it->second.insert(it->second.end(), buffer.begin(), buffer.end());
            break;
        }
        case Command::Remove: files.erase(c.handle); break;
        case Command::Read:
        {
            uint32_t length = 0;
            const uint8_t* src = nullptr;
            if (auto it = files.find(c.handle); it != files.end() && c.offset < it->second.size())
            {
                length = (uint32_t)(c.size < it->second.size() - c.offset ? c.size : it->second.size() - c.offset);
                src = it->second.data() + c.offset;
            }
            if (!WriteAll(pipe, &length, sizeof(length)) || (length && !WriteAll(pipe, src, length))) return 0;
            break;
        }
        default: return 0;
        }
    }
    CloseHandle(pipe);
    CloseHandle(client);
    return 0;
}
