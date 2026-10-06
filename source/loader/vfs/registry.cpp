// Virtual files and paths added by plugins through the exported API.
#include "internal.hpp"
#include "../core/strings.hpp"
#include <mutex>

namespace ual::vfs
{
    namespace
    {
        struct FileEntry
        {
            std::shared_ptr<FileData> data;
            int priority;
            std::wstring display; // key in original case, same length
        };

        struct PathEntry
        {
            std::wstring target; // absolute
            int priority;
            std::wstring display;
        };

        std::shared_mutex g_lock;
        std::unordered_map<std::wstring, FileEntry> g_files;
        std::unordered_map<std::wstring, PathEntry> g_paths;
        std::unordered_map<std::wstring, int> g_folders; // ancestor key -> number of keys below it
        std::atomic<bool> g_any{ false };

        void AddFolders(const std::wstring& key)
        {
            for (auto p = ParentKey(key);; p = ParentKey(p))
            {
                ++g_folders[std::wstring(p)];
                if (p.empty()) break;
            }
        }

        void RemoveFolders(const std::wstring& key)
        {
            for (auto p = ParentKey(key);; p = ParentKey(p))
            {
                auto it = g_folders.find(std::wstring(p));
                if (it != g_folders.end() && --it->second <= 0) g_folders.erase(it);
                if (p.empty()) break;
            }
        }

        void UpdateAny()
        {
            g_any.store(!g_files.empty() || !g_paths.empty(), std::memory_order_release);
        }
    }

    bool HasApiEntries()
    {
        return g_any.load(std::memory_order_acquire);
    }

    std::shared_ptr<FileData> ApiFile(const std::wstring& key)
    {
        if (!HasApiEntries()) return nullptr;
        std::shared_lock lock(g_lock);
        auto it = g_files.find(key);
        return it == g_files.end() ? nullptr : it->second.data;
    }

    bool ApiPath(const std::wstring& key, std::wstring& target)
    {
        if (!HasApiEntries()) return false;
        std::shared_lock lock(g_lock);
        auto it = g_paths.find(key);
        if (it == g_paths.end()) return false;
        target = it->second.target;
        return true;
    }

    bool ApiDirectory(const std::wstring& dirKey)
    {
        if (!HasApiEntries()) return false;
        std::shared_lock lock(g_lock);
        return g_folders.count(dirKey) != 0;
    }

    bool ApiListing(const std::wstring& dirKey, std::vector<ListedItem>& items)
    {
        if (!HasApiEntries()) return false;
        std::shared_lock lock(g_lock);
        if (!g_folders.count(dirKey)) return false;
        auto childOf = [&](const std::wstring& key, const std::wstring& display, std::wstring& child, bool& isDir) {
            if (!dirKey.empty() && !(key.size() > dirKey.size() && key.compare(0, dirKey.size(), dirKey) == 0 && key[dirKey.size()] == L'\\')) return false;
            size_t start = dirKey.empty() ? 0 : dirKey.size() + 1;
            size_t end = key.find(L'\\', start);
            isDir = end != std::wstring::npos;
            const std::wstring& src = display.size() == key.size() ? display : key;
            child = src.substr(start, isDir ? end - start : std::wstring::npos);
            return true;
        };
        auto add = [&](const std::wstring& name, bool isDir, uint64_t size, FILETIME t) {
            for (auto& i : items)
                if (IEquals(i.name, name)) return;
            items.push_back({ name, isDir, size, t });
        };
        std::wstring child;
        bool isDir;
        for (const auto& [key, f] : g_files)
            if (childOf(key, f.display, child, isDir)) add(child, isDir, isDir ? 0 : f.data->Size(), f.data->written);
        for (const auto& [key, p] : g_paths)
            if (childOf(key, p.display, child, isDir))
            {
                WIN32_FILE_ATTRIBUTE_DATA a{};
                uint64_t size = 0;
                if (!isDir && GetFileAttributesExW(p.target.c_str(), GetFileExInfoStandard, &a)) size = ((uint64_t)a.nFileSizeHigh << 32) | a.nFileSizeLow;
                add(child, isDir, size, a.ftLastWriteTime);
            }
        return true;
    }

    bool AddApiFile(const std::wstring& key, const std::wstring& display, const uint8_t* data, size_t size, int priority)
    {
        std::unique_lock lock(g_lock);
        if (auto p = g_paths.find(key); p != g_paths.end() && p->second.priority < priority)
        {
            g_paths.erase(p);
            RemoveFolders(key);
        }
        if (auto f = g_files.find(key); f != g_files.end())
        {
            // same file again means append, plugins add big files in chunks
            if (f->second.priority > priority) return false;
            f->second.priority = priority;
            auto existing = f->second.data;
            lock.unlock();
            return existing->Append(data, size); // may talk to the server, so not under the lock
        }
        lock.unlock();
        auto contents = MakeMemoryData(data, size); // may talk to the server, so not under the lock
        lock.lock();
        if (auto f = g_files.find(key); f != g_files.end()) // added meanwhile
        {
            if (f->second.priority > priority)
            {
                lock.unlock(); // contents is freed on return
                return false;
            }
            auto existing = f->second.data;
            lock.unlock();
            contents.reset(); // frees our copy in the server, not under the lock either
            return existing->Append(data, size);
        }
        g_files.emplace(key, FileEntry{ std::move(contents), priority, display });
        AddFolders(key);
        UpdateAny();
        lock.unlock();
        InstallPathHooks();
        InstallHandleHooks();
        return true;
    }

    void RemoveApiFile(const std::wstring& key)
    {
        std::shared_ptr<FileData> released; // open handles keep their own reference
        {
            std::unique_lock lock(g_lock);
            auto f = g_files.find(key);
            if (f == g_files.end()) return;
            released = std::move(f->second.data);
            g_files.erase(f);
            RemoveFolders(key);
            UpdateAny();
        }
    }

    bool AddApiPath(const std::wstring& key, const std::wstring& display, const std::wstring& target, int priority)
    {
        std::shared_ptr<FileData> released; // freed after the lock: it may talk to the server
        {
            std::unique_lock lock(g_lock);
            if (auto f = g_files.find(key); f != g_files.end() && f->second.priority < priority)
            {
                released = std::move(f->second.data);
                g_files.erase(f);
                RemoveFolders(key);
            }
            auto p = g_paths.find(key);
            if (p != g_paths.end() && priority <= p->second.priority) return false;
            if (p == g_paths.end()) AddFolders(key);
            g_paths[key] = PathEntry{ target, priority, display };
            UpdateAny();
        }
        InstallPathHooks();
        return true;
    }

    void RemoveApiPath(const std::wstring& key)
    {
        std::unique_lock lock(g_lock);
        if (g_paths.erase(key))
        {
            RemoveFolders(key);
            UpdateAny();
        }
    }
}
