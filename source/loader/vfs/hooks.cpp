// KernelBase file API hooks. Calls from the loader itself are never redirected.
// Handle hooks pass foreign handles straight through, costing one atomic load while no virtual file is open.
#include "internal.hpp"
#include "vfs.hpp"
#include "../core/hooks.hpp"
#include "../core/loader.hpp"
#include "../core/paths.hpp"
#include "../core/strings.hpp"
#include <intrin.h>
#include <mutex>

namespace ual::vfs
{
    namespace
    {
        decltype(&CreateFileA) oCreateFileA;
        decltype(&CreateFileW) oCreateFileW;
        decltype(&CreateFile2) oCreateFile2;
        using CreateFile3Fn = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, DWORD, void*);
        CreateFile3Fn oCreateFile3;
        decltype(&GetFileAttributesA) oGetFileAttributesA;
        decltype(&GetFileAttributesW) oGetFileAttributesW;
        decltype(&GetFileAttributesExA) oGetFileAttributesExA;
        decltype(&GetFileAttributesExW) oGetFileAttributesExW;
        decltype(&FindFirstFileA) oFindFirstFileA;
        decltype(&FindFirstFileW) oFindFirstFileW;
        decltype(&FindFirstFileExA) oFindFirstFileExA;
        decltype(&FindFirstFileExW) oFindFirstFileExW;
        decltype(&FindNextFileA) oFindNextFileA;
        decltype(&FindNextFileW) oFindNextFileW;
        decltype(&FindClose) oFindClose;
        decltype(&LoadLibraryExA) oLoadLibraryExA;
        decltype(&LoadLibraryExW) oLoadLibraryExW;

        decltype(&ReadFile) oReadFile;
        decltype(&ReadFileEx) oReadFileEx;
        decltype(&WriteFile) oWriteFile;
        decltype(&WriteFileEx) oWriteFileEx;
        decltype(&GetFileSize) oGetFileSize;
        decltype(&GetFileSizeEx) oGetFileSizeEx;
        decltype(&SetFilePointer) oSetFilePointer;
        decltype(&SetFilePointerEx) oSetFilePointerEx;
        decltype(&SetEndOfFile) oSetEndOfFile;
        decltype(&FlushFileBuffers) oFlushFileBuffers;
        decltype(&CloseHandle) oCloseHandle;
        decltype(&DuplicateHandle) oDuplicateHandle;
        decltype(&GetFileType) oGetFileType;
        decltype(&GetFileTime) oGetFileTime;
        decltype(&GetFileInformationByHandle) oGetFileInformationByHandle;
        decltype(&GetFileInformationByHandleEx) oGetFileInformationByHandleEx;
        decltype(&GetFinalPathNameByHandleA) oGetFinalPathNameByHandleA;
        decltype(&GetFinalPathNameByHandleW) oGetFinalPathNameByHandleW;
        decltype(&CreateFileMappingA) oCreateFileMappingA;
        decltype(&CreateFileMappingW) oCreateFileMappingW;

        // --- virtual handles

        struct VirtualHandle
        {
            std::shared_ptr<FileData> data; // null for folders
            std::wstring path;
            std::atomic<uint64_t> position{ 0 };
            bool overlapped = false;
            FILETIME time{};
            bool Directory() const { return !data; }
            uint64_t Size() const { return data ? data->Size() : 0; }
            DWORD Attributes() const { return data ? FILE_ATTRIBUTE_NORMAL : FILE_ATTRIBUTE_DIRECTORY; }
        };

        std::shared_mutex g_handleLock;
        std::unordered_map<HANDLE, std::shared_ptr<VirtualHandle>> g_handles;
        std::atomic<size_t> g_handleCount{ 0 };

        // Set when the process crashes: the crash reporter writes files while every other thread is suspended,
        // possibly inside one of these locks, so from then on every hook passes straight through.
        std::atomic<bool> g_passThrough{ false };

        std::shared_ptr<VirtualHandle> FindHandle(HANDLE h)
        {
            if (g_handleCount.load(std::memory_order_relaxed) == 0 || !h || h == INVALID_HANDLE_VALUE) return nullptr;
            if (g_passThrough.load(std::memory_order_relaxed)) return nullptr;
            std::shared_lock lock(g_handleLock);
            auto it = g_handles.find(h);
            return it == g_handles.end() ? nullptr : it->second;
        }

        HANDLE Register(std::shared_ptr<VirtualHandle> vh)
        {
            // an unused event gives a unique, closable handle value
            HANDLE h = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            if (!h) return INVALID_HANDLE_VALUE;
            std::unique_lock lock(g_handleLock);
            g_handles[h] = std::move(vh);
            g_handleCount.fetch_add(1, std::memory_order_relaxed);
            return h;
        }

        bool Unregister(HANDLE h)
        {
            // Released after the lock: the last reference may free a file kept in the virtual file server, and that
            // request's WriteFile would wait for this lock in FindHandle.
            std::shared_ptr<VirtualHandle> released;
            std::unique_lock lock(g_handleLock);
            auto it = g_handles.find(h);
            if (it == g_handles.end()) return false;
            released = std::move(it->second);
            g_handles.erase(it);
            g_handleCount.fetch_sub(1, std::memory_order_relaxed);
            return true;
        }

        uint64_t FileIndex(const std::wstring& path)
        {
            uint64_t h = 1469598103934665603ull; // FNV-1a of the lower case path
            for (wchar_t c : ToLower(path)) h = (h ^ c) * 1099511628211ull;
            return h | (1ull << 63);
        }

        constexpr DWORD kWriteRights =
            GENERIC_WRITE | GENERIC_ALL | FILE_WRITE_DATA | FILE_APPEND_DATA | FILE_WRITE_EA | FILE_WRITE_ATTRIBUTES | DELETE | WRITE_DAC | WRITE_OWNER;

        HANDLE OpenVirtual(const Resolved& r, DWORD access, DWORD disposition, DWORD flags)
        {
            bool directory = r.kind == Resolved::Directory;
            if (disposition == CREATE_NEW)
            {
                SetLastError(ERROR_FILE_EXISTS);
                return INVALID_HANDLE_VALUE;
            }
            // refuse writes, as for a read-only file
            if ((access & kWriteRights) || disposition == CREATE_ALWAYS || disposition == TRUNCATE_EXISTING ||
                (directory && !(flags & FILE_FLAG_BACKUP_SEMANTICS)))
            {
                SetLastError(ERROR_ACCESS_DENIED);
                return INVALID_HANDLE_VALUE;
            }
            auto vh = std::make_shared<VirtualHandle>();
            vh->data = directory ? nullptr : r.data;
            vh->path = r.path;
            vh->overlapped = (flags & FILE_FLAG_OVERLAPPED) != 0;
            vh->time = directory ? r.time : r.data->written;
            HANDLE h = Register(std::move(vh));
            if (h != INVALID_HANDLE_VALUE) SetLastError(disposition == OPEN_ALWAYS ? ERROR_ALREADY_EXISTS : ERROR_SUCCESS);
            return h;
        }

        // --- CreateFile*

        struct CreateArgs
        {
            int kind; // 0 CreateFileA, 1 CreateFileW, 2 CreateFile2, 3 CreateFile3
            LPCSTR nameA;
            LPCWSTR nameW;
            DWORD access, share, disposition, flags;
            LPSECURITY_ATTRIBUTES sa;
            HANDLE templ;
            void* ex;
        };

        HANDLE CallCreate(const CreateArgs& a, LPCWSTR path)
        {
            switch (a.kind)
            {
            case 0:
                return path ? oCreateFileW(path, a.access, a.share, a.sa, a.disposition, a.flags, a.templ)
                            : oCreateFileA(a.nameA, a.access, a.share, a.sa, a.disposition, a.flags, a.templ);
            case 1: return oCreateFileW(path ? path : a.nameW, a.access, a.share, a.sa, a.disposition, a.flags, a.templ);
            case 2: return oCreateFile2(path ? path : a.nameW, a.access, a.share, a.disposition, (LPCREATEFILE2_EXTENDED_PARAMETERS)a.ex);
            default: return oCreateFile3(path ? path : a.nameW, a.access, a.share, a.disposition, a.ex);
            }
        }

        // The game may create a file in a folder it only saw in an update folder or zip.
        // Create the missing folders in the game folder, where new files go.
        bool CreateMissingFolders(const std::wstring& key, const std::wstring& fullPath)
        {
            std::vector<std::wstring> missing;
            std::wstring_view dirKey = ParentKey(key);
            size_t last = fullPath.find_last_of(L'\\');
            if (last == std::wstring::npos) return false;
            std::wstring dir = fullPath.substr(0, last);
            while (!dirKey.empty() && dir.size() > 3 && !DirectoryExists(dir))
            {
                if (!IsVirtualDirectory(std::wstring(dirKey))) return false; // really missing, keep the caller's error
                missing.push_back(dir);
                dirKey = ParentKey(dirKey);
                size_t cut = dir.find_last_of(L'\\');
                if (cut == std::wstring::npos) return false;
                dir.resize(cut);
            }
            for (auto it = missing.rbegin(); it != missing.rend(); ++it)
                if (!CreateDirectoryW(it->c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
            return !missing.empty();
        }

        HANDLE Create(const void* caller, const CreateArgs& a)
        {
            if (IsSelfAddress(caller) || g_passThrough.load(std::memory_order_relaxed)) return CallCreate(a, nullptr);
            std::wstring key, full;
            bool keyed = a.kind == 0 ? MakeKeyA(a.nameA, key, &full) : MakeKey(a.nameW, key, &full);
            if (!keyed) return CallCreate(a, nullptr);
            Resolved r = Resolve(key);
            switch (r.kind)
            {
            case Resolved::Physical: return CallCreate(a, r.path.c_str());
            case Resolved::Virtual: return OpenVirtual(r, a.access, a.disposition, a.flags);
            case Resolved::Directory:
            {
                HANDLE h = CallCreate(a, nullptr); // a real folder of that name wins
                return h != INVALID_HANDLE_VALUE ? h : OpenVirtual(r, a.access, a.disposition, a.flags);
            }
            default:
            {
                HANDLE h = CallCreate(a, nullptr);
                if (h == INVALID_HANDLE_VALUE && GetLastError() == ERROR_PATH_NOT_FOUND && a.disposition != OPEN_EXISTING &&
                    a.disposition != TRUNCATE_EXISTING && HasLayers())
                {
                    if (CreateMissingFolders(key, full)) return CallCreate(a, nullptr);
                    SetLastError(ERROR_PATH_NOT_FOUND); // report the original error
                }
                return h;
            }
            }
        }

        HANDLE WINAPI hkCreateFileA(LPCSTR n, DWORD a, DWORD s, LPSECURITY_ATTRIBUTES sa, DWORD d, DWORD f, HANDLE t)
        {
            return Create(_ReturnAddress(), { 0, n, nullptr, a, s, d, f, sa, t, nullptr });
        }
        HANDLE WINAPI hkCreateFileW(LPCWSTR n, DWORD a, DWORD s, LPSECURITY_ATTRIBUTES sa, DWORD d, DWORD f, HANDLE t)
        {
            return Create(_ReturnAddress(), { 1, nullptr, n, a, s, d, f, sa, t, nullptr });
        }
        HANDLE WINAPI hkCreateFile2(LPCWSTR n, DWORD a, DWORD s, DWORD d, LPCREATEFILE2_EXTENDED_PARAMETERS p)
        {
            DWORD flags = p ? p->dwFileFlags : 0;
            return Create(_ReturnAddress(), { 2, nullptr, n, a, s, d, flags, nullptr, nullptr, p });
        }
        HANDLE WINAPI hkCreateFile3(LPCWSTR n, DWORD a, DWORD s, DWORD d, void* p)
        {
            // CREATEFILE3_EXTENDED_PARAMETERS: dwSize, dwFileAttributes, dwFileFlags, ...
            DWORD flags = p ? ((const DWORD*)p)[2] : 0;
            return Create(_ReturnAddress(), { 3, nullptr, n, a, s, d, flags, nullptr, nullptr, p });
        }

        // --- GetFileAttributes*

        template<class Original>
        BOOL AttributesEx(const void* caller, const std::wstring* keyOrNull, Original&& original, WIN32_FILE_ATTRIBUTE_DATA* out)
        {
            if (IsSelfAddress(caller) || !keyOrNull) return original(nullptr);
            Resolved r = Resolve(*keyOrNull);
            switch (r.kind)
            {
            case Resolved::Physical: return original(r.path.c_str());
            case Resolved::Virtual:
            {
                uint64_t size = r.data->Size();
                if (out) *out = { FILE_ATTRIBUTE_NORMAL, r.data->created, r.data->written, r.data->written, (DWORD)(size >> 32), (DWORD)size };
                return TRUE;
            }
            case Resolved::Directory:
                if (original(nullptr)) return TRUE;
                if (out) *out = { FILE_ATTRIBUTE_DIRECTORY, r.time, r.time, r.time, 0, 0 };
                return TRUE;
            default: return original(nullptr);
            }
        }

        DWORD WINAPI hkGetFileAttributesW(LPCWSTR n)
        {
            std::wstring key;
            bool keyed = !IsSelfAddress(_ReturnAddress()) && MakeKey(n, key);
            WIN32_FILE_ATTRIBUTE_DATA d{};
            DWORD result = INVALID_FILE_ATTRIBUTES;
            BOOL ok = AttributesEx(nullptr, keyed ? &key : nullptr, [&](LPCWSTR p) {
                result = oGetFileAttributesW(p ? p : n);
                return result != INVALID_FILE_ATTRIBUTES;
            }, &d);
            return ok && result == INVALID_FILE_ATTRIBUTES ? d.dwFileAttributes : result;
        }

        DWORD WINAPI hkGetFileAttributesA(LPCSTR n)
        {
            std::wstring key;
            bool keyed = !IsSelfAddress(_ReturnAddress()) && MakeKeyA(n, key);
            WIN32_FILE_ATTRIBUTE_DATA d{};
            DWORD result = INVALID_FILE_ATTRIBUTES;
            BOOL ok = AttributesEx(nullptr, keyed ? &key : nullptr, [&](LPCWSTR p) {
                result = p ? oGetFileAttributesW(p) : oGetFileAttributesA(n);
                return result != INVALID_FILE_ATTRIBUTES;
            }, &d);
            return ok && result == INVALID_FILE_ATTRIBUTES ? d.dwFileAttributes : result;
        }

        BOOL WINAPI hkGetFileAttributesExW(LPCWSTR n, GET_FILEEX_INFO_LEVELS level, LPVOID info)
        {
            std::wstring key;
            bool keyed = level == GetFileExInfoStandard && !IsSelfAddress(_ReturnAddress()) && MakeKey(n, key);
            return AttributesEx(nullptr, keyed ? &key : nullptr, [&](LPCWSTR p) { return oGetFileAttributesExW(p ? p : n, level, info); },
                                (WIN32_FILE_ATTRIBUTE_DATA*)info);
        }

        BOOL WINAPI hkGetFileAttributesExA(LPCSTR n, GET_FILEEX_INFO_LEVELS level, LPVOID info)
        {
            std::wstring key;
            bool keyed = level == GetFileExInfoStandard && !IsSelfAddress(_ReturnAddress()) && MakeKeyA(n, key);
            return AttributesEx(nullptr, keyed ? &key : nullptr,
                                [&](LPCWSTR p) { return p ? oGetFileAttributesExW(p, level, info) : oGetFileAttributesExA(n, level, info); },
                                (WIN32_FILE_ATTRIBUTE_DATA*)info);
        }

        // --- FindFirstFile*

        struct VirtualFind
        {
            std::vector<WIN32_FIND_DATAW> items;
            size_t next = 1; // items[0] was returned by FindFirstFile
        };

        std::shared_mutex g_findLock;
        std::unordered_map<HANDLE, std::shared_ptr<VirtualFind>> g_finds;
        std::atomic<size_t> g_findCount{ 0 };

        std::shared_ptr<VirtualFind> FindSearch(HANDLE h)
        {
            if (g_findCount.load(std::memory_order_relaxed) == 0) return nullptr;
            std::shared_lock lock(g_findLock);
            auto it = g_finds.find(h);
            return it == g_finds.end() ? nullptr : it->second;
        }

        WIN32_FIND_DATAW MakeFindData(const ListedItem& i)
        {
            WIN32_FIND_DATAW d{};
            d.dwFileAttributes = i.directory ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
            d.ftCreationTime = d.ftLastAccessTime = d.ftLastWriteTime = i.time;
            d.nFileSizeHigh = (DWORD)(i.size >> 32);
            d.nFileSizeLow = (DWORD)i.size;
            wcsncpy_s(d.cFileName, i.name.c_str(), _TRUNCATE);
            return d;
        }

        void ToFindDataA(const WIN32_FIND_DATAW& w, WIN32_FIND_DATAA& a)
        {
            memcpy(&a, &w, offsetof(WIN32_FIND_DATAA, cFileName)); // attributes, times, sizes, reserved
            strncpy_s(a.cFileName, ToFileApiString(w.cFileName).c_str(), _TRUNCATE);
            strncpy_s(a.cAlternateFileName, ToFileApiString(w.cAlternateFileName).c_str(), _TRUNCATE);
        }

        // False if the folder has no virtual items and the original call should run.
        bool MergedListing(const std::wstring& pattern, FINDEX_INFO_LEVELS level, FINDEX_SEARCH_OPS op, DWORD flags,
                           std::vector<WIN32_FIND_DATAW>& out)
        {
            size_t sep = pattern.find_last_of(L"\\/");
            std::wstring dir = sep == std::wstring::npos ? L"." : (sep == 0 ? pattern.substr(0, 1) : pattern.substr(0, sep));
            if (sep != std::wstring::npos && sep == 2 && pattern[1] == L':') dir = pattern.substr(0, 3); // "C:\x"
            std::wstring mask = sep == std::wstring::npos ? pattern : pattern.substr(sep + 1);
            std::wstring dirKey;
            if (!MakeKey(dir.c_str(), dirKey)) return false;
            std::vector<ListedItem> overlay;
            if (!OverlayListing(dirKey, overlay)) return false;

            // real items, if the folder exists, with overloaded files showing their replacement
            WIN32_FIND_DATAW fd;
            HANDLE h = oFindFirstFileExW(pattern.c_str(), level, &fd, op, nullptr, flags);
            bool realFolder = h != INVALID_HANDLE_VALUE || GetLastError() == ERROR_FILE_NOT_FOUND;
            if (h != INVALID_HANDLE_VALUE)
            {
                do
                {
                    for (auto it = overlay.begin(); it != overlay.end(); ++it)
                    {
                        if (!IEquals(it->name, fd.cFileName)) continue;
                        if (!it->directory)
                        {
                            fd.dwFileAttributes = (fd.dwFileAttributes & ~FILE_ATTRIBUTE_DIRECTORY) | FILE_ATTRIBUTE_NORMAL;
                            fd.nFileSizeHigh = (DWORD)(it->size >> 32);
                            fd.nFileSizeLow = (DWORD)it->size;
                            fd.ftLastWriteTime = it->time;
                        }
                        overlay.erase(it);
                        break;
                    }
                    out.push_back(fd);
                } while (oFindNextFileW(h, &fd));
                oFindClose(h);
            }
            else if (!realFolder && !dirKey.empty() && (MatchesMask(L".", mask) || mask == L"*"))
            {
                // a folder that exists only virtually still has . and ..
                for (const wchar_t* dot : { L".", L".." }) out.push_back(MakeFindData({ dot, true, 0, {} }));
            }
            for (const auto& i : overlay)
                if (MatchesMask(i.name, mask) && (op != FindExSearchLimitToDirectories || i.directory)) out.push_back(MakeFindData(i));
            return true;
        }

        HANDLE BeginVirtualFind(std::vector<WIN32_FIND_DATAW>&& items, WIN32_FIND_DATAW* first)
        {
            if (items.empty())
            {
                SetLastError(ERROR_FILE_NOT_FOUND);
                return INVALID_HANDLE_VALUE;
            }
            *first = items[0];
            HANDLE h = CreateEventW(nullptr, TRUE, FALSE, nullptr);
            if (!h) return INVALID_HANDLE_VALUE;
            auto search = std::make_shared<VirtualFind>();
            search->items = std::move(items);
            std::unique_lock lock(g_findLock);
            g_finds[h] = std::move(search);
            g_findCount.fetch_add(1, std::memory_order_relaxed);
            SetLastError(ERROR_SUCCESS);
            return h;
        }

        HANDLE FindFirstW(const void* caller, LPCWSTR name, FINDEX_INFO_LEVELS level, WIN32_FIND_DATAW* data, FINDEX_SEARCH_OPS op, LPVOID filter, DWORD flags)
        {
            std::vector<WIN32_FIND_DATAW> items;
            if (IsSelfAddress(caller) || !name || !data || !MergedListing(name, level, op, flags, items))
                return oFindFirstFileExW(name, level, data, op, filter, flags);
            return BeginVirtualFind(std::move(items), data);
        }

        HANDLE FindFirstA(const void* caller, LPCSTR name, FINDEX_INFO_LEVELS level, WIN32_FIND_DATAA* data, FINDEX_SEARCH_OPS op, LPVOID filter, DWORD flags)
        {
            std::vector<WIN32_FIND_DATAW> items;
            if (IsSelfAddress(caller) || !name || !data || !MergedListing(FromFileApiString(name), level, op, flags, items))
                return oFindFirstFileExA(name, level, data, op, filter, flags);
            WIN32_FIND_DATAW first;
            HANDLE h = BeginVirtualFind(std::move(items), &first);
            if (h != INVALID_HANDLE_VALUE) ToFindDataA(first, *data);
            return h;
        }

        HANDLE WINAPI hkFindFirstFileW(LPCWSTR n, LPWIN32_FIND_DATAW d) { return FindFirstW(_ReturnAddress(), n, FindExInfoStandard, d, FindExSearchNameMatch, nullptr, 0); }
        HANDLE WINAPI hkFindFirstFileA(LPCSTR n, LPWIN32_FIND_DATAA d) { return FindFirstA(_ReturnAddress(), n, FindExInfoStandard, d, FindExSearchNameMatch, nullptr, 0); }
        HANDLE WINAPI hkFindFirstFileExW(LPCWSTR n, FINDEX_INFO_LEVELS l, LPVOID d, FINDEX_SEARCH_OPS o, LPVOID f, DWORD x)
        {
            return FindFirstW(_ReturnAddress(), n, l, (WIN32_FIND_DATAW*)d, o, f, x);
        }
        HANDLE WINAPI hkFindFirstFileExA(LPCSTR n, FINDEX_INFO_LEVELS l, LPVOID d, FINDEX_SEARCH_OPS o, LPVOID f, DWORD x)
        {
            return FindFirstA(_ReturnAddress(), n, l, (WIN32_FIND_DATAA*)d, o, f, x);
        }

        BOOL WINAPI hkFindNextFileW(HANDLE h, LPWIN32_FIND_DATAW d)
        {
            auto s = FindSearch(h);
            if (!s) return oFindNextFileW(h, d);
            if (s->next >= s->items.size())
            {
                SetLastError(ERROR_NO_MORE_FILES);
                return FALSE;
            }
            *d = s->items[s->next++];
            return TRUE;
        }

        BOOL WINAPI hkFindNextFileA(HANDLE h, LPWIN32_FIND_DATAA d)
        {
            auto s = FindSearch(h);
            if (!s) return oFindNextFileA(h, d);
            if (s->next >= s->items.size())
            {
                SetLastError(ERROR_NO_MORE_FILES);
                return FALSE;
            }
            ToFindDataA(s->items[s->next++], *d);
            return TRUE;
        }

        BOOL WINAPI hkFindClose(HANDLE h)
        {
            if (g_findCount.load(std::memory_order_relaxed))
            {
                std::unique_lock lock(g_findLock);
                if (g_finds.erase(h))
                {
                    g_findCount.fetch_sub(1, std::memory_order_relaxed);
                    lock.unlock();
                    oCloseHandle ? oCloseHandle(h) : CloseHandle(h);
                    return TRUE;
                }
            }
            return oFindClose(h);
        }

        // --- LoadLibraryEx*

        // LoadLibrary needs a real file, so a DLL from a zip or memory is copied to a cache folder.
        std::wstring MaterializeDll(const Resolved& r)
        {
            wchar_t temp[MAX_PATH];
            DWORD n = GetTempPathW(MAX_PATH, temp);
            if (!n || n >= MAX_PATH) return {};
            uint64_t size = r.data->Size();
            uint64_t id = FileIndex(r.path) ^ (size * 0x9E3779B97F4A7C15ull) ^ (((uint64_t)r.data->written.dwHighDateTime << 32) | r.data->written.dwLowDateTime);
            wchar_t sub[32];
            swprintf_s(sub, L"%016llx", (unsigned long long)id);
            std::wstring dir = std::wstring(temp, n) + L"Ultimate-ASI-Loader\\" + sub;
            std::wstring path = dir + L"\\" + FileNameOf(r.path);
            // %TEMP% is writable by every process of the user, so a cached copy counts only if its bytes still match
            WIN32_FILE_ATTRIBUTE_DATA a;
            if (oGetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &a) && (((uint64_t)a.nFileSizeHigh << 32) | a.nFileSizeLow) == size)
            {
                HANDLE existing = oCreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
                if (existing != INVALID_HANDLE_VALUE)
                {
                    std::vector<uint8_t> mine(1 << 20), theirs(1 << 20);
                    bool same = true;
                    for (uint64_t offset = 0; same && offset < size;)
                    {
                        DWORD got = 0, read = 0;
                        same = r.data->Read(offset, mine.data(), (DWORD)mine.size(), got) && got && oReadFile(existing, theirs.data(), got, &read, nullptr) &&
                               read == got && memcmp(mine.data(), theirs.data(), got) == 0;
                        offset += got;
                    }
                    oCloseHandle(existing);
                    if (same) return path;
                }
            }
            CreateDirectoryW((std::wstring(temp, n) + L"Ultimate-ASI-Loader").c_str(), nullptr);
            CreateDirectoryW(dir.c_str(), nullptr);
            HANDLE f = oCreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
            if (f == INVALID_HANDLE_VALUE) return {};
            std::vector<uint8_t> buf(1 << 20);
            bool ok = true;
            for (uint64_t offset = 0; ok && offset < size;)
            {
                DWORD got = 0, written = 0;
                ok = r.data->Read(offset, buf.data(), (DWORD)buf.size(), got) && got && oWriteFile(f, buf.data(), got, &written, nullptr) && written == got;
                offset += got;
            }
            oCloseHandle(f);
            if (!ok) DeleteFileW(path.c_str());
            return ok ? path : std::wstring();
        }

        HMODULE LoadLibraryCommon(const void* caller, const std::wstring* key, LPCWSTR nameW, LPCSTR nameA, HANDLE file, DWORD flags)
        {
            auto original = [&](LPCWSTR p) { return p ? oLoadLibraryExW(p, file, flags) : (nameA ? oLoadLibraryExA(nameA, file, flags) : oLoadLibraryExW(nameW, file, flags)); };
            if (IsSelfAddress(caller) || !key) return original(nullptr);
            Resolved r = Resolve(*key);
            if (r.kind == Resolved::Physical) return original(r.path.c_str());
            if (r.kind == Resolved::Virtual)
            {
                auto path = MaterializeDll(r);
                if (!path.empty()) return original(path.c_str());
            }
            return original(nullptr);
        }

        HMODULE WINAPI hkLoadLibraryExW(LPCWSTR n, HANDLE f, DWORD x)
        {
            std::wstring key;
            bool keyed = MakeKey(n, key);
            return LoadLibraryCommon(_ReturnAddress(), keyed ? &key : nullptr, n, nullptr, f, x);
        }

        HMODULE WINAPI hkLoadLibraryExA(LPCSTR n, HANDLE f, DWORD x)
        {
            std::wstring key;
            bool keyed = MakeKeyA(n, key);
            return LoadLibraryCommon(_ReturnAddress(), keyed ? &key : nullptr, nullptr, n, f, x);
        }

        // --- reading

        constexpr LONG kStatusEndOfFile = (LONG)0xC0000011;

        BOOL ReadVirtual(VirtualHandle& vh, LPVOID buffer, DWORD count, LPDWORD read, LPOVERLAPPED ov, LPOVERLAPPED_COMPLETION_ROUTINE completion)
        {
            if (vh.Directory())
            {
                SetLastError(ERROR_INVALID_FUNCTION);
                return FALSE;
            }
            if ((!buffer && count) || (vh.overlapped && !ov))
            {
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }
            uint64_t offset = ov ? ((uint64_t)ov->OffsetHigh << 32) | ov->Offset : vh.position.load();
            DWORD got = 0;
            if (!vh.data->Read(offset, buffer, count, got))
            {
                SetLastError(ERROR_READ_FAULT);
                return FALSE;
            }
            if (!vh.overlapped) vh.position = offset + got; // synchronous handles advance the pointer even with an OVERLAPPED offset
            bool eof = got == 0 && count != 0 && offset >= vh.Size();
            if (read) *read = got;
            if (ov)
            {
                ov->Internal = eof ? kStatusEndOfFile : 0;
                ov->InternalHigh = got;
                if (ov->hEvent) SetEvent(ov->hEvent);
                // EOF is an error for OVERLAPPED reads, not for plain synchronous ones
                if (eof)
                {
                    SetLastError(ERROR_HANDLE_EOF);
                    return FALSE;
                }
            }
            if (completion)
            {
                // ReadFileEx completion runs as an APC on this thread during an alertable wait
                struct Apc
                {
                    LPOVERLAPPED_COMPLETION_ROUTINE routine;
                    DWORD bytes;
                    LPOVERLAPPED ov;
                };
                auto apc = new Apc{ completion, got, ov };
                if (!QueueUserAPC(
                        [](ULONG_PTR p) {
                            auto a = (Apc*)p;
                            a->routine(ERROR_SUCCESS, a->bytes, a->ov);
                            delete a;
                        },
                        GetCurrentThread(), (ULONG_PTR)apc))
                {
                    delete apc;
                    return FALSE;
                }
            }
            SetLastError(ERROR_SUCCESS);
            return TRUE;
        }

        BOOL WINAPI hkReadFile(HANDLE h, LPVOID b, DWORD n, LPDWORD r, LPOVERLAPPED ov)
        {
            // the loader's own reads (zip parts, the server pipe) are never virtual, see Create
            if (IsSelfAddress(_ReturnAddress())) return oReadFile(h, b, n, r, ov);
            auto vh = FindHandle(h);
            return vh ? ReadVirtual(*vh, b, n, r, ov, nullptr) : oReadFile(h, b, n, r, ov);
        }

        BOOL WINAPI hkReadFileEx(HANDLE h, LPVOID b, DWORD n, LPOVERLAPPED ov, LPOVERLAPPED_COMPLETION_ROUTINE c)
        {
            auto vh = FindHandle(h);
            if (!vh) return oReadFileEx(h, b, n, ov, c);
            if (!ov)
            {
                SetLastError(ERROR_INVALID_PARAMETER);
                return FALSE;
            }
            return ReadVirtual(*vh, b, n, nullptr, ov, c);
        }

        BOOL WINAPI hkWriteFile(HANDLE h, LPCVOID b, DWORD n, LPDWORD w, LPOVERLAPPED ov)
        {
            if (IsSelfAddress(_ReturnAddress()) || !FindHandle(h)) return oWriteFile(h, b, n, w, ov);
            if (w) *w = 0;
            SetLastError(ERROR_ACCESS_DENIED);
            return FALSE;
        }

        BOOL WINAPI hkWriteFileEx(HANDLE h, LPCVOID b, DWORD n, LPOVERLAPPED ov, LPOVERLAPPED_COMPLETION_ROUTINE c)
        {
            if (!FindHandle(h)) return oWriteFileEx(h, b, n, ov, c);
            SetLastError(ERROR_ACCESS_DENIED);
            return FALSE;
        }

        BOOL WINAPI hkSetEndOfFile(HANDLE h)
        {
            if (!FindHandle(h)) return oSetEndOfFile(h);
            SetLastError(ERROR_ACCESS_DENIED);
            return FALSE;
        }

        BOOL WINAPI hkFlushFileBuffers(HANDLE h)
        {
            if (!FindHandle(h)) return oFlushFileBuffers(h);
            SetLastError(ERROR_ACCESS_DENIED);
            return FALSE;
        }

        // --- size and position

        DWORD WINAPI hkGetFileSize(HANDLE h, LPDWORD high)
        {
            auto vh = FindHandle(h);
            if (!vh) return oGetFileSize(h, high);
            uint64_t size = vh->Size();
            if (high) *high = (DWORD)(size >> 32);
            SetLastError(ERROR_SUCCESS); // INVALID_FILE_SIZE may be a valid low part
            return (DWORD)size;
        }

        BOOL WINAPI hkGetFileSizeEx(HANDLE h, PLARGE_INTEGER size)
        {
            auto vh = FindHandle(h);
            if (!vh) return oGetFileSizeEx(h, size);
            if (!size)
            {
                SetLastError(ERROR_NOACCESS);
                return FALSE;
            }
            size->QuadPart = (LONGLONG)vh->Size();
            return TRUE;
        }

        bool Seek(VirtualHandle& vh, int64_t distance, DWORD method, uint64_t& result)
        {
            int64_t base;
            switch (method)
            {
            case FILE_BEGIN: base = 0; break;
            case FILE_CURRENT: base = (int64_t)vh.position.load(); break;
            case FILE_END: base = (int64_t)vh.Size(); break;
            default: SetLastError(ERROR_INVALID_PARAMETER); return false;
            }
            int64_t target = base + distance;
            if (target < 0)
            {
                SetLastError(ERROR_NEGATIVE_SEEK);
                return false;
            }
            result = (uint64_t)target; // past the end is allowed, as for real files
            vh.position = result;
            return true;
        }

        BOOL WINAPI hkSetFilePointerEx(HANDLE h, LARGE_INTEGER distance, PLARGE_INTEGER newPosition, DWORD method)
        {
            auto vh = FindHandle(h);
            if (!vh) return oSetFilePointerEx(h, distance, newPosition, method);
            uint64_t pos;
            if (!Seek(*vh, distance.QuadPart, method, pos)) return FALSE;
            if (newPosition) newPosition->QuadPart = (LONGLONG)pos;
            SetLastError(ERROR_SUCCESS);
            return TRUE;
        }

        DWORD WINAPI hkSetFilePointer(HANDLE h, LONG low, PLONG high, DWORD method)
        {
            auto vh = FindHandle(h);
            if (!vh) return oSetFilePointer(h, low, high, method);
            // without a high part the distance is a signed 32-bit value
            int64_t distance = high ? (int64_t)(((uint64_t)(uint32_t)*high << 32) | (uint32_t)low) : (int64_t)low;
            uint64_t pos;
            if (!Seek(*vh, distance, method, pos)) return INVALID_SET_FILE_POINTER;
            if (!high && pos > 0xFFFFFFFEull)
            {
                SetLastError(ERROR_INVALID_PARAMETER);
                return INVALID_SET_FILE_POINTER;
            }
            if (high) *high = (LONG)(pos >> 32);
            SetLastError(ERROR_SUCCESS);
            return (DWORD)pos;
        }

        // --- handle lifetime and information

        BOOL WINAPI hkCloseHandle(HANDLE h)
        {
            // probe under the shared lock first: every CloseHandle in the process passes here, and taking the
            // exclusive lock for foreign handles would stall every ReadFile of the other threads
            if (FindHandle(h)) Unregister(h);
            return oCloseHandle(h);
        }

        BOOL WINAPI hkDuplicateHandle(HANDLE srcProcess, HANDLE src, HANDLE dstProcess, LPHANDLE dst, DWORD access, BOOL inherit, DWORD options)
        {
            auto vh = FindHandle(src);
            BOOL ok = oDuplicateHandle(srcProcess, src, dstProcess, dst, access, inherit, options);
            if (!vh || !ok) return ok;
            bool local = GetProcessId(srcProcess) == GetCurrentProcessId() && GetProcessId(dstProcess) == GetCurrentProcessId();
            if (options & DUPLICATE_CLOSE_SOURCE) Unregister(src);
            if (local && dst && *dst)
            {
                // the copy shares the file position, like a duplicated file handle
                std::shared_ptr<VirtualHandle> replaced; // released after the lock, see Unregister
                std::unique_lock lock(g_handleLock);
                auto& slot = g_handles[*dst];
                if (slot) replaced = std::move(slot);
                else g_handleCount.fetch_add(1, std::memory_order_relaxed);
                slot = vh;
            }
            return ok;
        }

        DWORD WINAPI hkGetFileType(HANDLE h)
        {
            if (!FindHandle(h)) return oGetFileType(h);
            SetLastError(ERROR_SUCCESS);
            return FILE_TYPE_DISK;
        }

        BOOL WINAPI hkGetFileTime(HANDLE h, LPFILETIME c, LPFILETIME a, LPFILETIME w)
        {
            auto vh = FindHandle(h);
            if (!vh) return oGetFileTime(h, c, a, w);
            FILETIME created = vh->data ? vh->data->created : vh->time, written = vh->data ? vh->data->written : vh->time;
            if (c) *c = created;
            if (a) *a = written;
            if (w) *w = written;
            return TRUE;
        }

        BOOL WINAPI hkGetFileInformationByHandle(HANDLE h, LPBY_HANDLE_FILE_INFORMATION info)
        {
            auto vh = FindHandle(h);
            if (!vh) return oGetFileInformationByHandle(h, info);
            if (!info)
            {
                SetLastError(ERROR_NOACCESS);
                return FALSE;
            }
            ZeroMemory(info, sizeof(*info));
            info->dwFileAttributes = vh->Attributes();
            hkGetFileTime(h, &info->ftCreationTime, &info->ftLastAccessTime, &info->ftLastWriteTime);
            uint64_t size = vh->Size(), index = FileIndex(vh->path);
            info->nFileSizeHigh = (DWORD)(size >> 32);
            info->nFileSizeLow = (DWORD)size;
            info->nNumberOfLinks = 1;
            info->nFileIndexHigh = (DWORD)(index >> 32);
            info->nFileIndexLow = (DWORD)index;
            return TRUE;
        }

        BOOL WINAPI hkGetFileInformationByHandleEx(HANDLE h, FILE_INFO_BY_HANDLE_CLASS cls, LPVOID out, DWORD size)
        {
            auto vh = FindHandle(h);
            if (!vh) return oGetFileInformationByHandleEx(h, cls, out, size);
            FILETIME c, a, w;
            hkGetFileTime(h, &c, &a, &w);
            auto large = [](FILETIME t) {
                LARGE_INTEGER v;
                v.LowPart = t.dwLowDateTime;
                v.HighPart = (LONG)t.dwHighDateTime;
                return v;
            };
            auto need = [&](DWORD n) {
                if (out && size >= n) return true;
                SetLastError(ERROR_BAD_LENGTH);
                return false;
            };
            switch (cls)
            {
            case FileBasicInfo:
            {
                if (!need(sizeof(FILE_BASIC_INFO))) return FALSE;
                auto i = (FILE_BASIC_INFO*)out;
                *i = {};
                i->CreationTime = large(c);
                i->LastAccessTime = i->LastWriteTime = i->ChangeTime = large(w);
                i->FileAttributes = vh->Attributes();
                return TRUE;
            }
            case FileStandardInfo:
            {
                if (!need(sizeof(FILE_STANDARD_INFO))) return FALSE;
                auto i = (FILE_STANDARD_INFO*)out;
                *i = {};
                i->AllocationSize.QuadPart = i->EndOfFile.QuadPart = (LONGLONG)vh->Size();
                i->NumberOfLinks = 1;
                i->Directory = vh->Directory();
                return TRUE;
            }
            case FileAttributeTagInfo:
            {
                if (!need(sizeof(FILE_ATTRIBUTE_TAG_INFO))) return FALSE;
                *(FILE_ATTRIBUTE_TAG_INFO*)out = { vh->Attributes(), 0 };
                return TRUE;
            }
            case FileNameInfo:
            {
                // the path without the drive, like the real call
                std::wstring p = vh->path.size() > 2 && vh->path[1] == L':' ? vh->path.substr(2) : vh->path;
                DWORD bytes = (DWORD)(p.size() * sizeof(wchar_t));
                if (!need(sizeof(FILE_NAME_INFO))) return FALSE;
                auto i = (FILE_NAME_INFO*)out;
                i->FileNameLength = bytes;
                if (size < offsetof(FILE_NAME_INFO, FileName) + bytes)
                {
                    memcpy(i->FileName, p.data(), size - offsetof(FILE_NAME_INFO, FileName));
                    SetLastError(ERROR_MORE_DATA);
                    return FALSE;
                }
                memcpy(i->FileName, p.data(), bytes);
                return TRUE;
            }
            case FileIdInfo:
            {
                if (!need(sizeof(FILE_ID_INFO))) return FALSE;
                auto i = (FILE_ID_INFO*)out;
                *i = {};
                uint64_t index = FileIndex(vh->path);
                memcpy(i->FileId.Identifier, &index, sizeof(index));
                return TRUE;
            }
            default: SetLastError(ERROR_INVALID_PARAMETER); return FALSE;
            }
        }

        DWORD FinalPath(const VirtualHandle& vh, DWORD flags, std::wstring& out)
        {
            out = vh.path;
            if ((flags & 0x7) == VOLUME_NAME_NONE) out = out.size() > 2 && out[1] == L':' ? out.substr(2) : out;
            else out = L"\\\\?\\" + out;
            return (DWORD)out.size();
        }

        DWORD WINAPI hkGetFinalPathNameByHandleW(HANDLE h, LPWSTR buf, DWORD len, DWORD flags)
        {
            auto vh = FindHandle(h);
            if (!vh) return oGetFinalPathNameByHandleW(h, buf, len, flags);
            std::wstring p;
            DWORD n = FinalPath(*vh, flags, p);
            if (!buf || len <= n) return n + 1;
            wmemcpy(buf, p.c_str(), n + 1);
            return n;
        }

        DWORD WINAPI hkGetFinalPathNameByHandleA(HANDLE h, LPSTR buf, DWORD len, DWORD flags)
        {
            auto vh = FindHandle(h);
            if (!vh) return oGetFinalPathNameByHandleA(h, buf, len, flags);
            std::wstring p;
            FinalPath(*vh, flags, p);
            std::string a = ToFileApiString(p);
            if (!buf || len <= a.size()) return (DWORD)a.size() + 1;
            memcpy(buf, a.c_str(), a.size() + 1);
            return (DWORD)a.size();
        }

        // Virtual files are mapped as a page-file backed copy. The section is created writable to fill it, then the
        // handle is reduced to the access the requested protection allows, so views behave as for a real file:
        // no writable view of a read-only mapping, no executable view without PAGE_EXECUTE_*.
        HANDLE MapVirtual(VirtualHandle& vh, LPSECURITY_ATTRIBUTES sa, DWORD protect, DWORD maxHigh, DWORD maxLow, LPCWSTR name)
        {
            uint64_t size = vh.Size(), wanted = ((uint64_t)maxHigh << 32) | maxLow;
            if (wanted > size) size = wanted;
            if (vh.Directory() || size == 0)
            {
                SetLastError(ERROR_FILE_INVALID);
                return nullptr;
            }
            DWORD page = protect & 0xFF;
            bool execute = page == PAGE_EXECUTE_READ || page == PAGE_EXECUTE_READWRITE || page == PAGE_EXECUTE_WRITECOPY;
            bool writable = page == PAGE_READWRITE || page == PAGE_EXECUTE_READWRITE;
            DWORD attributes = protect & (SEC_COMMIT | SEC_NOCACHE | SEC_WRITECOMBINE | SEC_LARGE_PAGES);
            HANDLE m = oCreateFileMappingW(INVALID_HANDLE_VALUE, sa, (execute ? PAGE_EXECUTE_READWRITE : PAGE_READWRITE) | attributes, (DWORD)(size >> 32), (DWORD)size, name);
            if (!m) return nullptr;
            if (GetLastError() == ERROR_ALREADY_EXISTS) return m; // a named mapping that already exists
            void* view = MapViewOfFile(m, FILE_MAP_WRITE, 0, 0, 0);
            if (!view)
            {
                oCloseHandle(m);
                return nullptr;
            }
            bool ok = true;
            for (uint64_t offset = 0, total = vh.Size(); ok && offset < total;)
            {
                DWORD got = 0;
                ok = vh.data->Read(offset, (uint8_t*)view + offset, (DWORD)(std::min<uint64_t>)(total - offset, 1u << 30), got) && got;
                offset += got;
            }
            UnmapViewOfFile(view);
            if (!ok)
            {
                oCloseHandle(m);
                SetLastError(ERROR_READ_FAULT);
                return nullptr;
            }
            if (!writable)
            {
                // drop SECTION_MAP_WRITE: MapViewOfFile(FILE_MAP_WRITE) then fails with ERROR_ACCESS_DENIED like on a real read-only mapping
                DWORD access = STANDARD_RIGHTS_REQUIRED | SECTION_QUERY | SECTION_MAP_READ | (execute ? SECTION_MAP_EXECUTE : 0);
                HANDLE reduced = nullptr;
                if (!oDuplicateHandle(GetCurrentProcess(), m, GetCurrentProcess(), &reduced, access, FALSE, DUPLICATE_CLOSE_SOURCE))
                {
                    oCloseHandle(m);
                    return nullptr;
                }
                m = reduced;
            }
            SetLastError(ERROR_SUCCESS);
            return m;
        }

        HANDLE WINAPI hkCreateFileMappingW(HANDLE h, LPSECURITY_ATTRIBUTES sa, DWORD prot, DWORD hi, DWORD lo, LPCWSTR name)
        {
            auto vh = FindHandle(h);
            return vh ? MapVirtual(*vh, sa, prot, hi, lo, name) : oCreateFileMappingW(h, sa, prot, hi, lo, name);
        }

        HANDLE WINAPI hkCreateFileMappingA(HANDLE h, LPSECURITY_ATTRIBUTES sa, DWORD prot, DWORD hi, DWORD lo, LPCSTR name)
        {
            auto vh = FindHandle(h);
            if (!vh) return oCreateFileMappingA(h, sa, prot, hi, lo, name);
            std::wstring w = name ? AnsiToWide(name) : std::wstring();
            return MapVirtual(*vh, sa, prot, hi, lo, name ? w.c_str() : nullptr);
        }

        std::once_flag g_pathHooks, g_handleHooks;
    }

    void InstallPathHooks()
    {
        std::call_once(g_pathHooks, [] {
            InstallHooks({
                { L"kernelbase.dll", "CreateFileA", (void*)hkCreateFileA, (void**)&oCreateFileA },
                { L"kernelbase.dll", "CreateFileW", (void*)hkCreateFileW, (void**)&oCreateFileW },
                { L"kernelbase.dll", "CreateFile2", (void*)hkCreateFile2, (void**)&oCreateFile2 },
                { L"kernelbase.dll", "CreateFile3", (void*)hkCreateFile3, (void**)&oCreateFile3 },
                { L"kernelbase.dll", "GetFileAttributesA", (void*)hkGetFileAttributesA, (void**)&oGetFileAttributesA },
                { L"kernelbase.dll", "GetFileAttributesW", (void*)hkGetFileAttributesW, (void**)&oGetFileAttributesW },
                { L"kernelbase.dll", "GetFileAttributesExA", (void*)hkGetFileAttributesExA, (void**)&oGetFileAttributesExA },
                { L"kernelbase.dll", "GetFileAttributesExW", (void*)hkGetFileAttributesExW, (void**)&oGetFileAttributesExW },
                { L"kernelbase.dll", "FindFirstFileA", (void*)hkFindFirstFileA, (void**)&oFindFirstFileA },
                { L"kernelbase.dll", "FindFirstFileW", (void*)hkFindFirstFileW, (void**)&oFindFirstFileW },
                { L"kernelbase.dll", "FindFirstFileExA", (void*)hkFindFirstFileExA, (void**)&oFindFirstFileExA },
                { L"kernelbase.dll", "FindFirstFileExW", (void*)hkFindFirstFileExW, (void**)&oFindFirstFileExW },
                { L"kernelbase.dll", "FindNextFileA", (void*)hkFindNextFileA, (void**)&oFindNextFileA },
                { L"kernelbase.dll", "FindNextFileW", (void*)hkFindNextFileW, (void**)&oFindNextFileW },
                { L"kernelbase.dll", "FindClose", (void*)hkFindClose, (void**)&oFindClose },
                { L"kernelbase.dll", "LoadLibraryExA", (void*)hkLoadLibraryExA, (void**)&oLoadLibraryExA },
                { L"kernelbase.dll", "LoadLibraryExW", (void*)hkLoadLibraryExW, (void**)&oLoadLibraryExW },
            });
        });
        InstallHandleHooks(); // virtual files (zip, plugin-added) can appear at any time
    }

    void InstallHandleHooks()
    {
        std::call_once(g_handleHooks, [] {
            InstallHooks({
                { L"kernelbase.dll", "ReadFile", (void*)hkReadFile, (void**)&oReadFile },
                { L"kernelbase.dll", "ReadFileEx", (void*)hkReadFileEx, (void**)&oReadFileEx },
                { L"kernelbase.dll", "WriteFile", (void*)hkWriteFile, (void**)&oWriteFile },
                { L"kernelbase.dll", "WriteFileEx", (void*)hkWriteFileEx, (void**)&oWriteFileEx },
                { L"kernelbase.dll", "GetFileSize", (void*)hkGetFileSize, (void**)&oGetFileSize },
                { L"kernelbase.dll", "GetFileSizeEx", (void*)hkGetFileSizeEx, (void**)&oGetFileSizeEx },
                { L"kernelbase.dll", "SetFilePointer", (void*)hkSetFilePointer, (void**)&oSetFilePointer },
                { L"kernelbase.dll", "SetFilePointerEx", (void*)hkSetFilePointerEx, (void**)&oSetFilePointerEx },
                { L"kernelbase.dll", "SetEndOfFile", (void*)hkSetEndOfFile, (void**)&oSetEndOfFile },
                { L"kernelbase.dll", "FlushFileBuffers", (void*)hkFlushFileBuffers, (void**)&oFlushFileBuffers },
                { L"kernelbase.dll", "CloseHandle", (void*)hkCloseHandle, (void**)&oCloseHandle },
                { L"kernelbase.dll", "DuplicateHandle", (void*)hkDuplicateHandle, (void**)&oDuplicateHandle },
                { L"kernelbase.dll", "GetFileType", (void*)hkGetFileType, (void**)&oGetFileType },
                { L"kernelbase.dll", "GetFileTime", (void*)hkGetFileTime, (void**)&oGetFileTime },
                { L"kernelbase.dll", "GetFileInformationByHandle", (void*)hkGetFileInformationByHandle, (void**)&oGetFileInformationByHandle },
                { L"kernelbase.dll", "GetFileInformationByHandleEx", (void*)hkGetFileInformationByHandleEx, (void**)&oGetFileInformationByHandleEx },
                { L"kernelbase.dll", "GetFinalPathNameByHandleA", (void*)hkGetFinalPathNameByHandleA, (void**)&oGetFinalPathNameByHandleA },
                { L"kernelbase.dll", "GetFinalPathNameByHandleW", (void*)hkGetFinalPathNameByHandleW, (void**)&oGetFinalPathNameByHandleW },
                { L"kernelbase.dll", "CreateFileMappingW", (void*)hkCreateFileMappingW, (void**)&oCreateFileMappingW },
                { L"kernelbase.dll", "CreateFileMappingA", (void*)hkCreateFileMappingA, (void**)&oCreateFileMappingA },
            });
        });
    }

    void PassThroughForCrash()
    {
        g_passThrough.store(true, std::memory_order_relaxed);
    }
}
