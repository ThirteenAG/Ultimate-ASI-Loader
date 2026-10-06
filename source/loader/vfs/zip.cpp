#include "zip.hpp"
#include "internal.hpp"
#include "../core/strings.hpp"
#include <miniz.h>
#include <unordered_map>

namespace ual::vfs
{
    namespace
    {
        size_t ReadCallback(void* opaque, mz_uint64 offset, void* buffer, size_t n)
        {
            size_t read = 0;
            static_cast<Archive*>(opaque)->ReadRaw(offset, buffer, n, read);
            return read;
        }

        FILETIME ToFileTime(time_t t)
        {
            ULARGE_INTEGER v;
            v.QuadPart = (uint64_t)t * 10000000ull + 116444736000000000ull;
            return { v.LowPart, v.HighPart };
        }
    }

    std::shared_ptr<Archive> Archive::Open(std::vector<std::wstring> parts)
    {
        std::shared_ptr<Archive> a(new Archive());
        a->parts_ = std::move(parts);
        for (const auto& p : a->parts_)
        {
            HANDLE h = CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_RANDOM_ACCESS, nullptr);
            if (h == INVALID_HANDLE_VALUE) return nullptr;
            LARGE_INTEGER size{};
            GetFileSizeEx(h, &size);
            a->handles_.push_back(h);
            a->starts_.push_back(a->total_);
            a->total_ += (uint64_t)size.QuadPart;
        }
        auto zip = new mz_zip_archive{};
        a->zip_ = zip;
        zip->m_pRead = ReadCallback;
        zip->m_pIO_opaque = a.get();
        if (!mz_zip_reader_init(zip, a->total_, 0)) return nullptr;
        mz_uint count = mz_zip_reader_get_num_files(zip);
        a->entries_.reserve(count);
        for (mz_uint i = 0; i < count; ++i)
        {
            mz_zip_archive_file_stat st;
            if (!mz_zip_reader_file_stat(zip, i, &st)) continue;
            Entry e;
            e.path = Utf8ToWide(st.m_filename);
            for (auto& c : e.path)
                if (c == L'/') c = L'\\';
            while (!e.path.empty() && e.path.back() == L'\\') e.path.pop_back();
            if (e.path.empty()) continue;
            e.index = i;
            e.size = st.m_uncomp_size;
            e.compressedSize = st.m_comp_size;
            e.localHeader = st.m_local_header_ofs;
            e.method = (uint16_t)st.m_method;
            e.directory = st.m_is_directory != 0;
            e.encrypted = st.m_is_encrypted != 0;
#ifndef MINIZ_NO_TIME
            e.time = ToFileTime(st.m_time);
#else
            e.time = {};
#endif
            a->entries_.push_back(std::move(e));
        }
        return a;
    }

    Archive::~Archive()
    {
        if (zip_)
        {
            mz_zip_reader_end((mz_zip_archive*)zip_);
            delete (mz_zip_archive*)zip_;
        }
        for (HANDLE h : handles_) CloseHandle(h);
    }

    bool Archive::ReadRaw(uint64_t offset, void* buffer, size_t count, size_t& read)
    {
        read = 0;
        auto out = (uint8_t*)buffer;
        while (count && offset < total_)
        {
            size_t part = 0;
            while (part + 1 < starts_.size() && offset >= starts_[part + 1]) ++part;
            uint64_t inPart = offset - starts_[part];
            uint64_t partSize = (part + 1 < starts_.size() ? starts_[part + 1] : total_) - starts_[part];
            DWORD chunk = (DWORD)(std::min<uint64_t>)({ (uint64_t)count, partSize - inPart, 1u << 30 });
            OVERLAPPED ov{};
            ov.Offset = (DWORD)inPart;
            ov.OffsetHigh = (DWORD)(inPart >> 32);
            DWORD got = 0;
            if (!ReadFile(handles_[part], out, chunk, &got, &ov) && GetLastError() != ERROR_HANDLE_EOF) return false;
            if (!got) break;
            out += got;
            read += got;
            count -= got;
            offset += got;
        }
        return true;
    }

    bool Archive::DataOffset(const Entry& e, uint64_t& offset)
    {
        // 30-byte local header, name and extra field lengths at 26 and 28
        uint8_t header[30];
        size_t read = 0;
        if (!ReadRaw(e.localHeader, header, sizeof(header), read) || read != sizeof(header)) return false;
        if (header[0] != 'P' || header[1] != 'K' || header[2] != 3 || header[3] != 4) return false;
        uint16_t nameLength = (uint16_t)(header[26] | (header[27] << 8));
        uint16_t extraLength = (uint16_t)(header[28] | (header[29] << 8));
        offset = e.localHeader + 30 + nameLength + extraLength;
        return offset + e.size <= total_;
    }

    bool Archive::Extract(const Entry& e, std::vector<uint8_t>& out)
    {
        if (e.size > (uint64_t)SIZE_MAX) return false;
        try
        {
            out.resize((size_t)e.size);
        }
        catch (const std::bad_alloc&)
        {
            return false; // the caller reports a read error instead of an exception escaping a hooked ReadFile
        }
        std::lock_guard lock(zipLock_);
        return mz_zip_reader_extract_to_mem((mz_zip_archive*)zip_, e.index, out.data(), out.size(), 0) != MZ_FALSE;
    }

    namespace
    {
        // stored entries are read in place
        class StoredData final : public FileData
        {
        public:
            StoredData(std::shared_ptr<Archive> a, uint64_t offset, uint64_t size, FILETIME t) : archive_(std::move(a)), offset_(offset), size_(size)
            {
                created = written = t;
            }
            uint64_t Size() const override { return size_; }
            bool Read(uint64_t offset, void* buffer, DWORD count, DWORD& read) override
            {
                read = 0;
                if (offset >= size_) return true;
                size_t n = (size_t)(std::min<uint64_t>)(count, size_ - offset), got = 0;
                bool ok = archive_->ReadRaw(offset_ + offset, buffer, n, got);
                read = (DWORD)got;
                return ok;
            }

        private:
            std::shared_ptr<Archive> archive_;
            uint64_t offset_, size_;
        };

        // decompressed on first read, shared by all handles to the entry
        class ExtractedData final : public FileData
        {
        public:
            ExtractedData(std::shared_ptr<Archive> a, Archive::Entry e) : archive_(std::move(a)), entry_(std::move(e)) { created = written = entry_.time; }
            uint64_t Size() const override { return entry_.size; }
            bool Read(uint64_t offset, void* buffer, DWORD count, DWORD& read) override
            {
                read = 0;
                std::call_once(once_, [&] {
                    std::vector<uint8_t> bytes;
                    if (!archive_->Extract(entry_, bytes)) return;
                    try
                    {
                        data_ = MakeMemoryData(bytes.data(), bytes.size()); // out of process on Win32 when possible
                    }
                    catch (const std::bad_alloc&)
                    {
                        data_.reset();
                    }
                    archive_.reset();
                });
                if (!data_) return false;
                return data_->Read(offset, buffer, count, read);
            }

        private:
            std::shared_ptr<Archive> archive_;
            Archive::Entry entry_;
            std::once_flag once_;
            std::shared_ptr<FileData> data_;
        };

        std::mutex g_cacheLock;
        std::unordered_map<const void*, std::unordered_map<uint32_t, std::weak_ptr<FileData>>> g_extracted;
    }

    std::shared_ptr<FileData> OpenZipEntry(const std::shared_ptr<Archive>& archive, const Archive::Entry& entry)
    {
        if (entry.method == 0 && !entry.encrypted)
        {
            uint64_t offset;
            if (archive->DataOffset(entry, offset)) return std::make_shared<StoredData>(archive, offset, entry.size, entry.time);
        }
        std::lock_guard lock(g_cacheLock);
        auto& slot = g_extracted[archive.get()][entry.index];
        if (auto existing = slot.lock()) return existing;
        auto data = std::make_shared<ExtractedData>(archive, entry);
        slot = data;
        return data;
    }
}
