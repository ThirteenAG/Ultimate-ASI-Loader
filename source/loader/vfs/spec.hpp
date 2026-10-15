// [FileLoader] OverloadFromFolder: '|' separates the choices of the selection window, each part is one choice.
// Inside a part, '>' means "wins over": "A > B > C" uses A, B and C, A's files winning over B's and B's over C's.
// A part with a single folder is a choice with just that folder. Parts are independent, a folder can be in several.
// '<' is the old reverse form ("A < B" is "B > A"); a part mixing '<' and '>' is not a chain and is ignored.
// Example: "update | nightmare > update | hdtextures > nightmare" offers "update", "nightmare + update"
// and "hdtextures + nightmare".
#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace ual::vfs
{
    struct OverloadSpec
    {
        struct Entry
        {
            std::vector<size_t> folders; // indices into OverloadSpec::folders, highest priority first, no duplicates
            std::wstring id;             // "A > B > C", identifies the choice
        };

        std::vector<std::wstring> folders; // every folder once, in order of appearance
        std::vector<Entry> entries;        // the choices in order of appearance, without repeated ones
        std::vector<std::wstring> ignored; // parts mixing '<' and '>', as written

        static OverloadSpec Parse(std::wstring_view text);
    };
}
