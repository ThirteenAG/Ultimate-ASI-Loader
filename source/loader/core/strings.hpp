// Case-insensitive comparisons use Windows ordinal (file system) rules, not the C locale.
#pragma once
#include <windows.h>
#include <string>
#include <string_view>
#include <vector>

namespace ual
{
    bool IEquals(std::wstring_view a, std::wstring_view b);
    bool IStartsWith(std::wstring_view s, std::wstring_view prefix);
    bool IEndsWith(std::wstring_view s, std::wstring_view suffix);

    // File system case folding.
    void ToLowerInPlace(std::wstring& s);
    std::wstring ToLower(std::wstring s);

    std::wstring Utf8ToWide(std::string_view s);
    std::string WideToUtf8(std::wstring_view s);
    std::wstring AnsiToWide(std::string_view s); // CP_ACP, as the -A APIs do
    std::string WideToAnsi(std::wstring_view s);

    // UTF-8 with or without BOM, UTF-16 LE/BE with BOM. Invalid UTF-8 falls back to ANSI.
    std::wstring DecodeText(std::string_view bytes);

    std::wstring Trim(std::wstring_view s);
    std::wstring Unquote(std::wstring s); // "a b" -> a b

    // "a | b|c" -> {a, b, c}. Items are trimmed and unquoted; empties and duplicates are dropped.
    std::vector<std::wstring> SplitList(std::wstring_view s, wchar_t separator = L'|');
}
