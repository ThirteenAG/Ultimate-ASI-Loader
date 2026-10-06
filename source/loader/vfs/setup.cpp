// Picks the active update folders from the config, zip packages and the selection dialog.
#include "internal.hpp"
#include "packages.hpp"
#include "server.hpp"
#include "spec.hpp"
#include "../core/loader.hpp"
#include "../core/paths.hpp"
#include "../core/strings.hpp"
#include "../ui/dialogs.hpp"
#include <algorithm>
#include <mutex>

namespace ual::vfs
{
    namespace
    {
        std::wstring FirstLine(std::wstring text)
        {
            auto end = text.find_first_of(L"\r\n");
            if (end != std::wstring::npos) text.resize(end);
            if (text.size() > 100) text.resize(100);
            return Trim(text);
        }

        struct Candidate
        {
            size_t folder; // index in the spec
            LayerSpec layer;
            LayerSpec zips;       // packages for a folder that exists on disk, below it (no archives: none)
            std::wstring caption; // first line of update.txt
        };

        // update.txt from the folder on disk, else from its archives
        std::wstring Caption(const LayerSpec& l)
        {
            std::string bytes;
            if (l.archives.empty())
            {
                if (ReadWholeFile(l.root + L"\\update.txt", bytes)) return FirstLine(DecodeText(bytes));
                return {};
            }
            for (auto it = l.archives.rbegin(); it != l.archives.rend(); ++it)
                for (const auto& e : (*it)->Entries())
                    if (!e.directory && IEquals(e.path, l.name + L"\\update.txt"))
                    {
                        std::vector<uint8_t> data;
                        if ((*it)->Extract(e, data)) return FirstLine(DecodeText(std::string_view((const char*)data.data(), data.size())));
                    }
            return {};
        }

        // One dialog per game at a time. Another process of the same game started meanwhile,
        // for example relaunched by its launcher, waits and reuses the choice.
        int ChooseOnce(const std::vector<ui::Choice>& choices, const std::vector<std::wstring>& ids)
        {
            struct Shared
            {
                LONG sequence;
                wchar_t chosen[1024];
            };
            uint64_t hash = 1469598103934665603ull;
            for (wchar_t c : ToLower(Self().exePath)) hash = (hash ^ c) * 1099511628211ull;
            wchar_t name[96];
            swprintf_s(name, L"Local\\UltimateASILoader-FolderSelection-%016llx", (unsigned long long)hash);
            HANDLE mutex = CreateMutexW(nullptr, FALSE, (std::wstring(name) + L"-lock").c_str());
            HANDLE mapping = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, sizeof(Shared), (std::wstring(name) + L"-state").c_str());
            auto shared = mapping ? (Shared*)MapViewOfFile(mapping, FILE_MAP_WRITE, 0, 0, sizeof(Shared)) : nullptr;
            int result = -1;
            if (mutex && shared)
            {
                LONG before = shared->sequence;
                DWORD w = WaitForSingleObject(mutex, 0);
                bool waited = w == WAIT_TIMEOUT;
                if (waited) w = WaitForSingleObject(mutex, INFINITE); // a dialog is open in another process
                if (w == WAIT_OBJECT_0 || w == WAIT_ABANDONED)
                {
                    // reuse the other process's answer, "none" included, if it applies here
                    bool reused = false;
                    if (waited && w == WAIT_OBJECT_0 && shared->sequence != before)
                    {
                        if (!shared->chosen[0]) reused = true;
                        for (size_t i = 0; i < ids.size() && !reused; ++i)
                            if (IEquals(ids[i], shared->chosen))
                            {
                                result = (int)i;
                                reused = true;
                            }
                    }
                    if (!reused)
                    {
                        result = ui::ChooseOverloadFolder(choices);
                        wcsncpy_s(shared->chosen, result >= 0 ? ids[result].c_str() : L"", _TRUNCATE);
                        InterlockedIncrement(&shared->sequence);
                    }
                    ReleaseMutex(mutex);
                }
            }
            else
                result = ui::ChooseOverloadFolder(choices);
            if (shared) UnmapViewOfFile(shared);
            if (mapping) CloseHandle(mapping);
            if (mutex) CloseHandle(mutex);
            return result;
        }

        std::once_flag g_setup;
    }

    void Setup()
    {
        std::call_once(g_setup, [] {
            auto spec = OverloadSpec::Parse(GetSettings().overloadFromFolder);
            if (spec.folders.empty()) return;
            auto packages = ScanPackages(GameDir() + L"packages");

            // a configured folder comes from disk, from packages, or both: files on disk win over the
            // packages, which are mounted on the same folder
            std::vector<Candidate> candidates;
            for (size_t i = 0; i < spec.folders.size(); ++i)
            {
                const auto& folder = spec.folders[i];
                bool absolute = folder.size() > 1 && (folder[1] == L':' || (folder[0] == L'\\' && folder[1] == L'\\'));
                Candidate c{ i, LayerSpec{ folder, absolute ? folder : GameDir() + folder, 0, {} }, {}, {} };
                std::vector<std::shared_ptr<Archive>> archives;
                for (const auto& p : packages)
                    if (std::any_of(p.roots.begin(), p.roots.end(), [&](const std::wstring& r) { return IEquals(r, folder); }))
                        archives.push_back(p.archive);
                if (DirectoryExists(c.layer.root))
                {
                    if (!archives.empty()) c.zips = LayerSpec{ folder, c.layer.root, 0, std::move(archives) };
                }
                else
                {
                    if (archives.empty()) continue;
                    c.layer.archives = std::move(archives);
                    c.layer.root = GameDir() + folder; // mounted where the folder would be on disk
                }
                candidates.push_back(std::move(c));
            }
            if (candidates.empty()) return;

            size_t selected = candidates.front().folder;
            if (candidates.size() > 1)
            {
                std::vector<ui::Choice> choices;
                std::vector<std::wstring> ids;
                for (auto& c : candidates)
                {
                    c.caption = Caption(c.layer);
                    if (c.caption.empty() && !c.zips.archives.empty()) c.caption = Caption(c.zips);
                    ui::Choice choice{ c.caption.empty() ? c.layer.name : c.caption, c.layer.root, !c.layer.archives.empty() || !c.zips.archives.empty(), {} };
                    for (size_t r : spec.Activate(c.folder))
                    {
                        if (r == c.folder) continue;
                        choice.includes.push_back(FileNameOf(spec.folders[r]));
                        for (auto& o : candidates)
                            if (o.folder == r) choice.zip |= !o.layer.archives.empty() || !o.zips.archives.empty();
                    }
                    choices.push_back(std::move(choice));
                    ids.push_back(c.layer.name);
                }
                int chosen = ChooseOnce(choices, ids);
                if (chosen < 0) return;
                selected = candidates[chosen].folder;
            }

            std::vector<LayerSpec> layers;
            int priority = 1000;
            for (size_t f : spec.Activate(selected))
                for (auto& c : candidates)
                    if (c.folder == f)
                    {
                        c.layer.priority = priority--;
                        layers.push_back(c.layer);
                        if (!c.zips.archives.empty())
                        {
                            c.zips.priority = priority--;
                            layers.push_back(c.zips);
                        }
                    }
            if (layers.empty()) return;
#ifndef _WIN64
            server::Start(); // keeps zip and plugin files out of the 32-bit address space
#endif
            ActivateLayers(std::move(layers));
            InstallPathHooks();
        });
    }

    std::vector<std::wstring> PluginFolders()
    {
        return PhysicalLayerRoots();
    }
}
