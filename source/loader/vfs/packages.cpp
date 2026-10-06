#include "packages.hpp"
#include "../core/strings.hpp"
#include <algorithm>
#include <map>

namespace ual::vfs
{
    bool PackagePart(const std::wstring& name, std::wstring& base, uint64_t& part)
    {
        if (IEndsWith(name, L".zip"))
        {
            base = name.substr(0, name.size() - 4);
            part = 0;
            return !base.empty();
        }
        auto dot = name.find_last_of(L'.');
        if (dot == std::wstring::npos || dot + 1 >= name.size()) return false;
        auto digits = name.substr(dot + 1);
        if (!std::all_of(digits.begin(), digits.end(), [](wchar_t c) { return c >= L'0' && c <= L'9'; }) || digits.size() > 18) return false;
        auto stem = name.substr(0, dot);
        if (!IEndsWith(stem, L".zip")) return false;
        base = stem.substr(0, stem.size() - 4);
        part = std::stoull(digits);
        return !base.empty();
    }

    std::vector<Package> ScanPackages(const std::wstring& dir)
    {
        std::vector<Package> packages;
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileExW((dir + L"\\*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, FIND_FIRST_EX_LARGE_FETCH);
        if (h == INVALID_HANDLE_VALUE) return packages;
        std::map<std::wstring, std::vector<std::pair<uint64_t, std::wstring>>> groups; // lower base -> (part, path)
        std::map<std::wstring, std::wstring> displayBase;
        do
        {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            std::wstring base;
            uint64_t part;
            if (!PackagePart(fd.cFileName, base, part)) continue;
            auto lower = ToLower(base);
            groups[lower].emplace_back(part, dir + L"\\" + fd.cFileName);
            displayBase.emplace(lower, base);
        } while (FindNextFileW(h, &fd));
        FindClose(h);

        for (auto& [base, parts] : groups) // alphabetical by name
        {
            std::sort(parts.begin(), parts.end()); // name.zip first, then parts numerically (.2 before .10)
            std::vector<std::wstring> paths;
            for (auto& p : parts) paths.push_back(p.second);
            auto archive = Archive::Open(std::move(paths));
            if (!archive) continue;
            Package pkg{ archive, {} };
            for (const auto& e : archive->Entries())
            {
                auto sep = e.path.find(L'\\');
                if (sep == std::wstring::npos && !e.directory) continue; // top-level files belong to no folder
                auto root = e.path.substr(0, sep);
                if (std::none_of(pkg.roots.begin(), pkg.roots.end(), [&](const std::wstring& r) { return IEquals(r, root); })) pkg.roots.push_back(root);
            }
            packages.push_back(std::move(pkg));
        }
        return packages;
    }
}
