#include "strings.hpp"
#include <algorithm>

namespace ual
{
    bool IEquals(std::wstring_view a, std::wstring_view b)
    {
        return a.size() == b.size() &&
               (a.empty() || CompareStringOrdinal(a.data(), (int)a.size(), b.data(), (int)b.size(), TRUE) == CSTR_EQUAL);
    }

    bool IStartsWith(std::wstring_view s, std::wstring_view prefix)
    {
        return s.size() >= prefix.size() && IEquals(s.substr(0, prefix.size()), prefix);
    }

    bool IEndsWith(std::wstring_view s, std::wstring_view suffix)
    {
        return s.size() >= suffix.size() && IEquals(s.substr(s.size() - suffix.size()), suffix);
    }

    void ToLowerInPlace(std::wstring& s)
    {
        if (!s.empty()) CharLowerBuffW(s.data(), (DWORD)s.size());
    }

    std::wstring ToLower(std::wstring s)
    {
        ToLowerInPlace(s);
        return s;
    }

    static std::wstring ToWide(UINT cp, DWORD flags, std::string_view s)
    {
        if (s.empty()) return {};
        int n = MultiByteToWideChar(cp, flags, s.data(), (int)s.size(), nullptr, 0);
        if (n <= 0) return {};
        std::wstring w(n, L'\0');
        MultiByteToWideChar(cp, flags, s.data(), (int)s.size(), w.data(), n);
        return w;
    }

    static std::string FromWide(UINT cp, std::wstring_view s)
    {
        if (s.empty()) return {};
        int n = WideCharToMultiByte(cp, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
        if (n <= 0) return {};
        std::string out(n, '\0');
        WideCharToMultiByte(cp, 0, s.data(), (int)s.size(), out.data(), n, nullptr, nullptr);
        return out;
    }

    std::wstring Utf8ToWide(std::string_view s) { return ToWide(CP_UTF8, 0, s); }
    std::string WideToUtf8(std::wstring_view s) { return FromWide(CP_UTF8, s); }
    std::wstring AnsiToWide(std::string_view s) { return ToWide(CP_ACP, 0, s); }
    std::string WideToAnsi(std::wstring_view s) { return FromWide(CP_ACP, s); }

    std::wstring DecodeText(std::string_view b)
    {
        if (b.size() >= 2 && (unsigned char)b[0] == 0xFF && (unsigned char)b[1] == 0xFE)
            return std::wstring((const wchar_t*)(b.data() + 2), (b.size() - 2) / 2);
        if (b.size() >= 2 && (unsigned char)b[0] == 0xFE && (unsigned char)b[1] == 0xFF)
        {
            std::wstring w((b.size() - 2) / 2, L'\0');
            for (size_t i = 0; i < w.size(); ++i) w[i] = (wchar_t)(((unsigned char)b[2 + i * 2] << 8) | (unsigned char)b[3 + i * 2]);
            return w;
        }
        if (b.size() >= 3 && (unsigned char)b[0] == 0xEF && (unsigned char)b[1] == 0xBB && (unsigned char)b[2] == 0xBF) b.remove_prefix(3);
        if (b.empty()) return {};
        auto w = ToWide(CP_UTF8, MB_ERR_INVALID_CHARS, b);
        return w.empty() ? ToWide(CP_ACP, 0, b) : w;
    }

    std::wstring Trim(std::wstring_view s)
    {
        constexpr std::wstring_view ws = L" \t\r\n";
        auto first = s.find_first_not_of(ws);
        if (first == std::wstring_view::npos) return {};
        auto last = s.find_last_not_of(ws);
        return std::wstring(s.substr(first, last - first + 1));
    }

    std::wstring Unquote(std::wstring s)
    {
        if (s.size() >= 2 && s.front() == L'"' && s.back() == L'"') return s.substr(1, s.size() - 2);
        return s;
    }

    std::vector<std::wstring> SplitList(std::wstring_view s, wchar_t separator)
    {
        std::vector<std::wstring> out;
        size_t start = 0;
        while (start <= s.size())
        {
            size_t end = s.find(separator, start);
            if (end == std::wstring_view::npos) end = s.size();
            auto item = Unquote(Trim(s.substr(start, end - start)));
            if (!item.empty() && std::none_of(out.begin(), out.end(), [&](const std::wstring& o) { return IEquals(o, item); })) out.push_back(std::move(item));
            start = end + 1;
        }
        return out;
    }
}
