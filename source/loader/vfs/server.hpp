// Win32 client of VirtualFileServer, a 64-bit helper that keeps virtual file contents
// out of the game's 32-bit address space. It is VirtualFileServer.exe renamed to <loader name>.exe next to the loader,
// started on demand as "<server>.exe <pid>" and ended with the game.
// Pipe \\.\pipe\Ultimate-ASI-Loader-VirtualFileServer-<pid>. Each request is a Command plus
// optional data. Add replies with a handle, Read with a DWORD length and the bytes.
#pragma once
#include <cstdint>

namespace ual::vfs::server
{
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

    bool Available();
    void Start(); // Win32 only, runs once
    uint64_t Add(const uint8_t* data, size_t size);
    bool Append(uint64_t handle, const uint8_t* data, size_t size);
    void Remove(uint64_t handle);
    bool Read(uint64_t handle, uint64_t offset, void* buffer, uint32_t count, uint32_t& read);
}
