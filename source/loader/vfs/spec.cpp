#include "spec.hpp"
#include "../core/strings.hpp"
#include <algorithm>

namespace ual::vfs
{
    OverloadSpec OverloadSpec::Parse(std::wstring_view text)
    {
        OverloadSpec s;
        auto indexOf = [&](const std::wstring& folder) {
            for (size_t i = 0; i < s.folders.size(); ++i)
                if (IEquals(s.folders[i], folder)) return i;
            s.folders.push_back(folder);
            return s.folders.size() - 1;
        };

        size_t start = 0;
        while (start <= text.size())
        {
            size_t end = text.find(L'|', start);
            if (end == std::wstring_view::npos) end = text.size();
            auto part = text.substr(start, end - start);
            start = end + 1;

            // folders of the part as written, and its operators
            std::vector<std::wstring> names;
            bool less = false, greater = false;
            size_t from = 0;
            while (from <= part.size())
            {
                size_t to = part.find_first_of(L"<>", from);
                if (to == std::wstring_view::npos) to = part.size();
                auto folder = Unquote(Trim(part.substr(from, to - from)));
                if (!folder.empty()) names.push_back(std::move(folder));
                if (to >= part.size()) break;
                (part[to] == L'<' ? less : greater) = true;
                from = to + 1;
            }
            if (names.empty()) continue;
            if (less && greater)
            {
                s.ignored.push_back(Trim(part));
                continue;
            }
            if (less) std::reverse(names.begin(), names.end()); // "a < b" is "b > a"

            // the first mention of a folder sets its priority
            std::vector<std::wstring> chain;
            for (auto& n : names)
                if (std::none_of(chain.begin(), chain.end(), [&](const std::wstring& c) { return IEquals(c, n); })) chain.push_back(std::move(n));

            Entry e;
            for (const auto& n : chain)
            {
                if (!e.id.empty()) e.id += L" > ";
                e.id += n;
            }
            if (std::any_of(s.entries.begin(), s.entries.end(), [&](const Entry& o) { return IEquals(o.id, e.id); })) continue;
            for (const auto& n : chain) e.folders.push_back(indexOf(n));
            s.entries.push_back(std::move(e));
        }
        return s;
    }
}
