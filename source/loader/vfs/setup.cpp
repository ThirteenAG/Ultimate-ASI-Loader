// Picks the active update folders from the config, zip packages and the selection dialog.
#include "internal.hpp"
#include "packages.hpp"
#include "server.hpp"
#include "spec.hpp"
#include "../core/loader.hpp"
#include "../core/log.hpp"
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

        constexpr wchar_t kCancelledMark[] = L"|cancelled"; // no choice id contains '|'

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
                    // reuse the other process's answer, "none" and "quit" included, if it applies here
                    bool reused = false;
                    if (waited && w == WAIT_OBJECT_0 && shared->sequence != before)
                    {
                        if (!shared->chosen[0]) reused = true;
                        if (!wcscmp(shared->chosen, kCancelledMark))
                        {
                            result = ui::kChoiceCancelled;
                            reused = true;
                        }
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
                        wcsncpy_s(shared->chosen, result >= 0 ? ids[result].c_str() : result == ui::kChoiceCancelled ? kCancelledMark : L"", _TRUNCATE);
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
            if (!spec.ignored.empty())
            {
                std::wstring parts;
                for (const auto& p : spec.ignored)
                {
                    UAL_LOG("OverloadFromFolder: \"%s\" mixes '<' and '>', ignored", WideToUtf8(p).c_str());
                    parts += L"\n    " + p;
                }
                ui::ShowError(L"OverloadFromFolder: choice ignored",
                              L"These parts of [FileLoader] OverloadFromFolder mix '<' and '>', so their priority order is unclear:" + parts +
                                  L"\n\nWrite each choice as a chain with '>' only, highest priority first, for example: mod > update");
            }
            if (spec.entries.empty()) return;
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
            auto candidateOf = [&](size_t folder) -> Candidate* {
                for (auto& c : candidates)
                    if (c.folder == folder) return &c;
                return nullptr;
            };

            // the choices: each entry with its folders that exist, highest priority first. Missing folders are
            // skipped; an entry left with nothing, or with the same folders as an earlier one, is not offered.
            struct Option
            {
                const OverloadSpec::Entry* entry;
                std::vector<Candidate*> folders;
            };
            std::vector<Option> options;
            for (const auto& e : spec.entries)
            {
                Option o{ &e, {} };
                for (size_t f : e.folders)
                    if (auto c = candidateOf(f)) o.folders.push_back(c);
                if (o.folders.empty()) continue;
                if (std::any_of(options.begin(), options.end(), [&](const Option& p) { return p.folders == o.folders; })) continue;
                options.push_back(std::move(o));
            }
            if (options.empty()) return;

            size_t selected = 0;
            if (options.size() > 1)
            {
                std::vector<ui::Choice> choices;
                std::vector<std::wstring> ids;
                for (const auto& o : options)
                {
                    Candidate& top = *o.folders.front();
                    if (top.caption.empty()) top.caption = Caption(top.layer);
                    if (top.caption.empty() && !top.zips.archives.empty()) top.caption = Caption(top.zips);
                    ui::Choice choice{ top.caption.empty() ? top.layer.name : top.caption, top.layer.root, false, {} };
                    for (const Candidate* c : o.folders)
                    {
                        if (c != &top) choice.includes.push_back(FileNameOf(c->layer.name));
                        choice.zip |= !c->layer.archives.empty() || !c->zips.archives.empty();
                    }
                    choices.push_back(std::move(choice));
                    ids.push_back(o.entry->id);
                }
                int chosen = ChooseOnce(choices, ids);
                if (chosen == ui::kChoiceCancelled)
                {
                    // Nothing of the game has run yet. TerminateProcess skips the DLL detach notifications, which could
                    // reach this half-initialized loader (we are inside its one-time setup) or code of the game.
                    UAL_LOG("folder selection window closed: quitting the game");
                    TerminateProcess(GetCurrentProcess(), 0);
                }
                if (chosen < 0) return;
                selected = (size_t)chosen;
            }

            std::vector<LayerSpec> layers;
            int priority = 1000;
            for (Candidate* c : options[selected].folders)
            {
                c->layer.priority = priority--;
                layers.push_back(c->layer);
                if (!c->zips.archives.empty())
                {
                    c->zips.priority = priority--;
                    layers.push_back(c->zips);
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
