// [FileLoader] OverloadFromFolder: folders separated by '|', with optional
// priority relations: "a < b" (b overrides a), "a > b" (a overrides b).
// Selecting a folder also activates the folders it overrides, transitively.
// Without relations, earlier folders win.
#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace ual::vfs
{
    struct OverloadSpec
    {
        std::vector<std::wstring> folders;                 // in order of appearance, no duplicates
        std::vector<std::pair<size_t, size_t>> overrides;  // (higher, lower)
        std::vector<size_t> priorityOrder;                 // all folders, highest priority first

        static OverloadSpec Parse(std::wstring_view text);

        // highest priority first
        std::vector<size_t> Activate(size_t selected) const;
    };
}
