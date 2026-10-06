// Layers (update folders and zips) are indexed once, so a lookup is a hash probe per layer
// with no disk access. Folders on disk are watched with ReadDirectoryChangesW while the game runs.
#include "internal.hpp"
#include "../core/strings.hpp"
#include <thread>

namespace ual::vfs
{
    namespace
    {
        struct LayerFile
        {
            std::wstring rel;                // relative path in the layer, original case
            uint64_t size = 0;
            FILETIME time{};
            std::shared_ptr<Archive> archive; // zip layers
            const Archive::Entry* entry = nullptr;
        };

        struct Layer
        {
            std::wstring name;
            std::wstring root;    // absolute, no trailing backslash
            std::wstring rootKey;
            int priority = 0;
            std::vector<std::shared_ptr<Archive>> archives;
            bool zip = false;

            mutable SRWLOCK lock = SRWLOCK_INIT; // the watcher updates folder layers
            std::unordered_map<std::wstring, LayerFile> files;                                   // relative key -> file
            std::unordered_map<std::wstring, std::unordered_map<std::wstring, ListedItem>> dirs; // relative folder key -> children by lower name

            // also registers every folder above the item
            void AddItem(const std::wstring& relKey, const std::wstring& relOriginal, bool directory, uint64_t size, FILETIME time)
            {
                auto parent = std::wstring(ParentKey(relKey));
                auto name = std::wstring(NameOfKey(relKey));
                auto& children = dirs[parent];
                auto it = children.find(name);
                ListedItem item{ relOriginal.substr(relOriginal.size() - name.size()), directory, directory ? 0 : size, time };
                if (it == children.end()) children.emplace(name, std::move(item));
                else if (!directory) it->second = std::move(item);
                if (directory) dirs[relKey];
                // stop at the first known ancestor, everything above it is known too
                std::wstring key = parent, orig = relOriginal.substr(0, parent.size());
                while (!key.empty())
                {
                    auto up = std::wstring(ParentKey(key));
                    auto n = std::wstring(NameOfKey(key));
                    auto& siblings = dirs[up];
                    if (siblings.count(n)) break;
                    siblings.emplace(n, ListedItem{ orig.substr(orig.size() - n.size()), true, 0, time });
                    key = up;
                    orig = orig.substr(0, up.size());
                }
            }

            void RemoveItem(const std::wstring& relKey)
            {
                auto parent = std::wstring(ParentKey(relKey));
                if (auto d = dirs.find(parent); d != dirs.end()) d->second.erase(std::wstring(NameOfKey(relKey)));
                files.erase(relKey);
                // removing a folder removes everything below it
                std::wstring prefix = relKey + L"\\";
                for (auto it = files.begin(); it != files.end();)
                    it = it->first.starts_with(prefix) ? files.erase(it) : std::next(it);
                for (auto it = dirs.begin(); it != dirs.end();)
                    it = (it->first == relKey || it->first.starts_with(prefix)) ? dirs.erase(it) : std::next(it);
            }

            void IndexFolder(const std::wstring& relKey, const std::wstring& relOriginal, int depth)
            {
                if (depth > 64) return; // symbolic link loops
                std::wstring dir = relOriginal.empty() ? root : root + L"\\" + relOriginal;
                WIN32_FIND_DATAW fd;
                HANDLE h = FindFirstFileExW((dir + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
                if (h == INVALID_HANDLE_VALUE) return;
                dirs[relKey];
                std::vector<std::pair<std::wstring, std::wstring>> subdirs;
                do
                {
                    if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L"..")) continue;
                    std::wstring orig = relOriginal.empty() ? fd.cFileName : relOriginal + L"\\" + fd.cFileName;
                    std::wstring key = ToLower(orig);
                    bool isDir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                    uint64_t size = ((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
                    dirs[relKey][ToLower(fd.cFileName)] = ListedItem{ fd.cFileName, isDir, isDir ? 0 : size, fd.ftLastWriteTime };
                    if (isDir) subdirs.emplace_back(key, orig);
                    else files[key] = LayerFile{ orig, size, fd.ftLastWriteTime };
                } while (FindNextFileW(h, &fd));
                FindClose(h);
                for (const auto& [k, o] : subdirs) IndexFolder(k, o, depth + 1);
            }

            void IndexArchives()
            {
                for (const auto& a : archives) // later archives, by name, win
                    for (const auto& e : a->Entries())
                    {
                        // only entries under <layer name>\ are used
                        if (!IStartsWith(e.path, name) || (e.path.size() > name.size() && e.path[name.size()] != L'\\')) continue;
                        if (e.path.size() <= name.size() + 1) continue;
                        std::wstring orig = e.path.substr(name.size() + 1);
                        std::wstring key = ToLower(orig);
                        AddItem(key, orig, e.directory, e.size, e.time);
                        if (!e.directory) files[key] = LayerFile{ orig, e.size, e.time, a, &e };
                    }
                dirs[L""];
            }

            void Refresh(const std::wstring& relOriginal, DWORD action)
            {
                std::wstring key = ToLower(relOriginal);
                WIN32_FILE_ATTRIBUTE_DATA a;
                bool exists = GetFileAttributesExW((root + L"\\" + relOriginal).c_str(), GetFileExInfoStandard, &a) != FALSE;
                bool isDir = exists && (a.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
                AcquireSRWLockExclusive(&lock);
                // a folder reported as modified because its contents changed; those changes arrive separately
                if (!(action == FILE_ACTION_MODIFIED && isDir && dirs.count(key)))
                {
                    RemoveItem(key);
                    if (exists)
                    {
                        uint64_t size = ((uint64_t)a.nFileSizeHigh << 32) | a.nFileSizeLow;
                        AddItem(key, relOriginal, isDir, size, a.ftLastWriteTime);
                        if (isDir) IndexFolder(key, relOriginal, 0);
                        else files[key] = LayerFile{ relOriginal, size, a.ftLastWriteTime };
                    }
                }
                ReleaseSRWLockExclusive(&lock);
            }

            void Reindex()
            {
                AcquireSRWLockExclusive(&lock);
                files.clear();
                dirs.clear();
                IndexFolder(L"", L"", 0);
                ReleaseSRWLockExclusive(&lock);
            }

            // rel is the key relative to the layer root
            bool Contains(const std::wstring& key, std::wstring& rel) const
            {
                if (key.size() == rootKey.size() && key == rootKey)
                {
                    rel.clear();
                    return true;
                }
                if (key.size() > rootKey.size() && key.compare(0, rootKey.size(), rootKey) == 0 && (rootKey.empty() || key[rootKey.size()] == L'\\'))
                {
                    rel = key.substr(rootKey.empty() ? 0 : rootKey.size() + 1);
                    return true;
                }
                return false;
            }
        };

        std::vector<std::unique_ptr<Layer>> g_layers; // highest priority first, fixed once active
        std::atomic<bool> g_active{ false };
        bool g_anyZip = false;

        Resolved FromLayerFile(const Layer& layer, const LayerFile& f)
        {
            Resolved r;
            r.time = f.time;
            if (layer.zip)
            {
                r.kind = Resolved::Virtual;
                r.data = OpenZipEntry(f.archive, *f.entry);
                r.path = layer.root + L"\\" + f.rel;
                if (!r.data) r.kind = Resolved::None;
            }
            else
            {
                r.kind = Resolved::Physical;
                r.path = layer.root + L"\\" + f.rel;
            }
            return r;
        }

        struct Watched
        {
            Layer* layer;
            HANDLE dir;
            OVERLAPPED ov{};
            alignas(DWORD) BYTE buffer[64 * 1024];
        };

        struct Watcher
        {
            std::vector<std::unique_ptr<Watched>> watched;
            std::vector<HANDLE> events;
        };

        bool Arm(Watched& w)
        {
            ResetEvent(w.ov.hEvent);
            return ReadDirectoryChangesW(w.dir, w.buffer, sizeof(w.buffer), TRUE,
                                         FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE,
                                         nullptr, &w.ov, nullptr) != FALSE;
        }

        // Opens and arms the watches on the calling thread, so no change made after ActivateLayers returns is missed
        // (the folders were indexed just before). WaitForMultipleObjects limits a watcher to 64 folders.
        std::unique_ptr<Watcher> PrepareWatch()
        {
            auto watcher = std::make_unique<Watcher>();
            for (auto& l : g_layers)
            {
                if (l->zip || watcher->events.size() >= MAXIMUM_WAIT_OBJECTS) continue;
                HANDLE dir = CreateFileW(l->root.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                                         FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
                if (dir == INVALID_HANDLE_VALUE) continue;
                auto w = std::make_unique<Watched>();
                w->layer = l.get();
                w->dir = dir;
                w->ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
                if (!w->ov.hEvent || !Arm(*w))
                {
                    if (w->ov.hEvent) CloseHandle(w->ov.hEvent);
                    CloseHandle(dir);
                    continue;
                }
                watcher->events.push_back(w->ov.hEvent);
                watcher->watched.push_back(std::move(w));
            }
            return watcher;
        }

        void Watch(Watcher* self)
        {
            std::unique_ptr<Watcher> owned(self);
            auto& events = self->events;
            auto& watched = self->watched;
            while (!events.empty())
            {
                DWORD r = WaitForMultipleObjects((DWORD)events.size(), events.data(), FALSE, INFINITE);
                if (r >= WAIT_OBJECT_0 + events.size()) return;
                Watched& w = *watched[r - WAIT_OBJECT_0];
                DWORD bytes = 0;
                if (!GetOverlappedResult(w.dir, &w.ov, &bytes, FALSE) || bytes == 0)
                    w.layer->Reindex(); // buffer overflowed, too many changes at once
                else
                {
                    for (auto info = (FILE_NOTIFY_INFORMATION*)w.buffer;; info = (FILE_NOTIFY_INFORMATION*)((BYTE*)info + info->NextEntryOffset))
                    {
                        w.layer->Refresh(std::wstring(info->FileName, info->FileNameLength / sizeof(wchar_t)), info->Action);
                        if (!info->NextEntryOffset) break;
                    }
                }
                if (!Arm(w)) w.layer->Reindex();
            }
        }
    }

    void ActivateLayers(std::vector<LayerSpec> specs)
    {
        for (auto& s : specs)
        {
            auto l = std::make_unique<Layer>();
            l->name = s.name;
            l->root = s.root;
            while (!l->root.empty() && (l->root.back() == L'\\' || l->root.back() == L'/')) l->root.pop_back();
            if (!MakeKey(l->root.c_str(), l->rootKey)) l->rootKey = ToLower(l->name);
            l->priority = s.priority;
            l->archives = std::move(s.archives);
            l->zip = !l->archives.empty();
            if (l->zip)
            {
                l->IndexArchives();
                g_anyZip = true;
            }
            else
                l->IndexFolder(L"", L"", 0);
            g_layers.push_back(std::move(l));
        }
        g_active = !g_layers.empty();
        auto watcher = PrepareWatch();
        if (!watcher->events.empty()) std::thread(Watch, watcher.release()).detach();
    }

    bool HasLayers()
    {
        return g_active.load(std::memory_order_acquire);
    }

    bool HasZipLayers()
    {
        return g_anyZip;
    }

    Resolved ResolveInLayers(const std::wstring& key)
    {
        if (!HasLayers()) return {};
        std::wstring rel;
        // paths inside a layer are not overloaded, but a zip layer serves its own files
        for (const auto& l : g_layers)
            if (l->Contains(key, rel))
            {
                if (!l->zip) return {};
                Resolved r;
                AcquireSRWLockShared(&l->lock);
                if (auto f = l->files.find(rel); f != l->files.end()) r = FromLayerFile(*l, f->second);
                else if (l->dirs.count(rel))
                {
                    r.kind = Resolved::Directory;
                    r.path = rel.empty() ? l->root : l->root + L"\\" + rel;
                }
                ReleaseSRWLockShared(&l->lock);
                return r;
            }
        // game files, from the highest priority layer that has them
        for (const auto& l : g_layers)
        {
            Resolved r;
            AcquireSRWLockShared(&l->lock);
            if (auto f = l->files.find(key); f != l->files.end()) r = FromLayerFile(*l, f->second);
            else if (l->dirs.count(key) && !key.empty())
            {
                r.kind = Resolved::Directory;
                r.path = KeyToPath(key);
            }
            ReleaseSRWLockShared(&l->lock);
            if (r.kind != Resolved::None) return r;
        }
        return {};
    }

    bool LayerListing(const std::wstring& dirKey, std::vector<ListedItem>& items)
    {
        if (!HasLayers()) return false;
        bool any = false;
        auto merge = [&](const std::unordered_map<std::wstring, ListedItem>& children) {
            for (const auto& [lower, item] : children)
            {
                bool present = false;
                for (const auto& i : items)
                    if (IEquals(i.name, item.name))
                    {
                        present = true;
                        break;
                    }
                if (!present) items.push_back(item);
            }
            any = true;
        };
        std::wstring rel;
        for (const auto& l : g_layers)
            if (l->Contains(dirKey, rel))
            {
                if (!l->zip) return false; // a real folder is listed as is
                AcquireSRWLockShared(&l->lock);
                if (auto d = l->dirs.find(rel); d != l->dirs.end()) merge(d->second);
                ReleaseSRWLockShared(&l->lock);
                return any;
            }
        for (const auto& l : g_layers)
        {
            AcquireSRWLockShared(&l->lock);
            if (auto d = l->dirs.find(dirKey); d != l->dirs.end()) merge(d->second);
            ReleaseSRWLockShared(&l->lock);
            // a zip layer shows up as a folder in its parent
            if (l->zip && ParentKey(l->rootKey) == dirKey && !l->rootKey.empty())
            {
                std::wstring folder(NameOfKey(l->root));
                std::unordered_map<std::wstring, ListedItem> self{ { ToLower(folder), ListedItem{ folder, true, 0, {} } } };
                merge(self);
            }
        }
        return any;
    }

    std::vector<std::wstring> PhysicalLayerRoots()
    {
        std::vector<std::wstring> roots;
        for (const auto& l : g_layers)
            if (!l->zip) roots.push_back(l->root);
        return roots;
    }

    std::wstring FirstLayerRoot()
    {
        return g_layers.empty() ? std::wstring() : g_layers.front()->root;
    }
}
