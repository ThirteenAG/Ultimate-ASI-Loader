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

        // tokens and the operator before each
        size_t start = 0;
        wchar_t op = L'|';
        size_t previous = SIZE_MAX;
        while (start <= text.size())
        {
            size_t end = text.find_first_of(L"|<>", start);
            if (end == std::wstring_view::npos) end = text.size();
            auto folder = Unquote(Trim(text.substr(start, end - start)));
            if (!folder.empty())
            {
                size_t current = indexOf(folder);
                if (previous != SIZE_MAX && current != previous)
                {
                    if (op == L'<') s.overrides.emplace_back(current, previous); // previous < current
                    else if (op == L'>') s.overrides.emplace_back(previous, current);
                }
                previous = current;
            }
            if (end >= text.size()) break;
            op = text[end];
            if (op == L'|') previous = SIZE_MAX; // '|' starts an independent group
            start = end + 1;
        }

        // Topological order of the relations, ties broken by order of appearance.
        // Relations that would form a cycle are dropped.
        size_t n = s.folders.size();
        std::vector<std::vector<size_t>> below(n);
        auto reaches = [&](size_t from, size_t to) {
            std::vector<size_t> stack{ from };
            std::vector<bool> seen(n);
            while (!stack.empty())
            {
                size_t x = stack.back();
                stack.pop_back();
                if (x == to) return true;
                if (seen[x]) continue;
                seen[x] = true;
                for (size_t y : below[x]) stack.push_back(y);
            }
            return false;
        };
        std::vector<std::pair<size_t, size_t>> kept;
        for (auto [hi, lo] : s.overrides)
            if (!reaches(lo, hi) && std::find(below[hi].begin(), below[hi].end(), lo) == below[hi].end())
            {
                below[hi].push_back(lo);
                kept.emplace_back(hi, lo);
            }
        s.overrides = std::move(kept);

        std::vector<int> above(n, 0);
        for (auto [hi, lo] : s.overrides) ++above[lo];
        std::vector<bool> placed(n);
        for (size_t k = 0; k < n; ++k)
        {
            size_t pick = SIZE_MAX;
            for (size_t i = 0; i < n && pick == SIZE_MAX; ++i)
                if (!placed[i] && above[i] == 0) pick = i;
            if (pick == SIZE_MAX) break; // unreachable, cycles were removed
            placed[pick] = true;
            s.priorityOrder.push_back(pick);
            for (size_t lo : below[pick]) --above[lo];
        }
        return s;
    }

    std::vector<size_t> OverloadSpec::Activate(size_t selected) const
    {
        std::vector<bool> active(folders.size());
        std::vector<size_t> stack{ selected };
        while (!stack.empty())
        {
            size_t x = stack.back();
            stack.pop_back();
            if (x >= active.size() || active[x]) continue;
            active[x] = true;
            for (auto [hi, lo] : overrides)
                if (hi == x) stack.push_back(lo);
        }
        std::vector<size_t> result;
        for (size_t i : priorityOrder)
            if (active[i]) result.push_back(i);
        return result;
    }
}
