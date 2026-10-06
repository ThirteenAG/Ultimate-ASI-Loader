// Read-only zip archives, single or split into numbered parts.
#pragma once
#include <windows.h>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ual::vfs
{
    class Archive
    {
    public:
        struct Entry
        {
            std::wstring path;  // inside the archive, backslash separated, original case
            uint32_t index;
            uint64_t size;      // uncompressed
            uint64_t compressedSize;
            uint64_t localHeader;
            uint16_t method;    // 0 = stored
            bool directory;
            bool encrypted;
            FILETIME time;
        };

        // parts in order; nullptr if not a readable zip
        static std::shared_ptr<Archive> Open(std::vector<std::wstring> parts);
        ~Archive();

        const std::vector<Entry>& Entries() const { return entries_; }
        const std::wstring& Name() const { return parts_.front(); }

        // raw bytes across all parts, thread-safe
        bool ReadRaw(uint64_t offset, void* buffer, size_t count, size_t& read);
        bool DataOffset(const Entry& e, uint64_t& offset);
        bool Extract(const Entry& e, std::vector<uint8_t>& out);

    private:
        Archive() = default;
        std::vector<std::wstring> parts_;
        std::vector<HANDLE> handles_;
        std::vector<uint64_t> starts_; // offset of each part
        uint64_t total_ = 0;
        std::vector<Entry> entries_;
        std::mutex zipLock_; // the miniz reader is not thread-safe
        void* zip_ = nullptr; // mz_zip_archive
    };
}
