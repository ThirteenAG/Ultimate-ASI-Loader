#include "internal.hpp"
#include "../core/strings.hpp"
#include <cwctype>

namespace ual::vfs
{
    namespace
    {
        std::wstring g_gameDir;                // with trailing backslash
        std::wstring g_gameDirLower;           // lower case, with trailing backslash
        std::vector<std::wstring> g_gameParts; // lower case components, first one is the root

        std::vector<std::wstring> Split(std::wstring_view s)
        {
            std::vector<std::wstring> parts;
            size_t start = 0;
            // the root of a UNC path is \\server\share
            if (s.starts_with(L"\\\\"))
            {
                size_t a = s.find(L'\\', 2);
                size_t b = a == std::wstring_view::npos ? a : s.find(L'\\', a + 1);
                parts.emplace_back(s.substr(0, b));
                start = b == std::wstring_view::npos ? s.size() : b + 1;
            }
            while (start < s.size())
            {
                size_t end = s.find(L'\\', start);
                if (end == std::wstring_view::npos) end = s.size();
                if (end > start) parts.emplace_back(s.substr(start, end - start));
                start = end + 1;
            }
            return parts;
        }

        wchar_t Fold(wchar_t c)
        {
            if (c < 0x80) return (c >= L'A' && c <= L'Z') ? c + 32 : c;
            wchar_t s[2] = { c, 0 };
            CharLowerBuffW(s, 1);
            return s[0];
        }

        bool MakeKeyFromFull(std::wstring_view full, std::wstring& key)
        {
            std::wstring unc; // \\?\UNC\server\share -> \\server\share
            if (full.starts_with(L"\\\\?\\UNC\\"))
            {
                unc = L"\\\\" + std::wstring(full.substr(8));
                full = unc;
            }
            else if (full.starts_with(L"\\\\?\\"))
                full.remove_prefix(4);
            else if (full.starts_with(L"\\\\.\\"))
                return false; // devices, pipes, consoles
            if (full.size() < 2) return false;

            // fast path, inside the game folder
            if (full.size() + 1 >= g_gameDirLower.size() && IStartsWith(full, std::wstring_view(g_gameDirLower).substr(0, g_gameDirLower.size() - 1)))
            {
                size_t n = g_gameDirLower.size() - 1;
                if (full.size() == n)
                {
                    key.clear();
                    return true;
                }
                if (full[n] == L'\\')
                {
                    key.assign(full.substr(n + 1));
                    while (!key.empty() && key.back() == L'\\') key.pop_back();
                    ToLowerInPlace(key);
                    return true;
                }
            }

            // elsewhere on the same drive or share, the part after the common ancestor
            auto parts = Split(full);
            if (parts.empty() || g_gameParts.empty() || !IEquals(parts[0], g_gameParts[0])) return false;
            size_t common = 1;
            while (common < parts.size() && common < g_gameParts.size() && IEquals(parts[common], g_gameParts[common])) ++common;
            // An ancestor of the game folder (the drive root, <root> for <root>\bin\game.exe) is not the
            // game folder: it must not get the key "" or its listing would show the update folder's contents.
            if (common == parts.size()) return false;
            key.clear();
            for (size_t i = common; i < parts.size(); ++i)
            {
                if (!key.empty()) key += L'\\';
                key += parts[i];
            }
            ToLowerInPlace(key);
            return true;
        }
    }

    void InitKeys(const std::wstring& gameDir)
    {
        g_gameDir = gameDir;
        if (!g_gameDir.empty() && g_gameDir.back() != L'\\') g_gameDir += L'\\';
        g_gameDirLower = ToLower(g_gameDir);
        g_gameParts = Split(g_gameDirLower);
    }

    const std::wstring& GameDir()
    {
        return g_gameDir;
    }

    bool MakeKey(const wchar_t* path, std::wstring& key, std::wstring* fullPath)
    {
        if (!path || !*path) return false;
        wchar_t stackBuf[MAX_PATH + 64];
        std::wstring heapBuf;
        wchar_t* buf = stackBuf;
        DWORD n = GetFullPathNameW(path, (DWORD)std::size(stackBuf), buf, nullptr);
        if (n >= std::size(stackBuf))
        {
            heapBuf.resize(n + 1);
            n = GetFullPathNameW(path, (DWORD)heapBuf.size(), heapBuf.data(), nullptr);
            if (n >= heapBuf.size()) return false;
            buf = heapBuf.data();
        }
        if (n == 0) return false;
        std::wstring_view full(buf, n);
        if (fullPath) fullPath->assign(full);
        return MakeKeyFromFull(full, key);
    }

    std::wstring FromFileApiString(const char* s)
    {
        if (!s) return {};
        UINT cp = AreFileApisANSI() ? CP_ACP : CP_OEMCP;
        int n = MultiByteToWideChar(cp, 0, s, -1, nullptr, 0);
        if (n <= 0) return {};
        std::wstring w(n - 1, L'\0');
        MultiByteToWideChar(cp, 0, s, -1, w.data(), n);
        return w;
    }

    std::string ToFileApiString(std::wstring_view s)
    {
        if (s.empty()) return {};
        UINT cp = AreFileApisANSI() ? CP_ACP : CP_OEMCP;
        int n = WideCharToMultiByte(cp, 0, s.data(), (int)s.size(), nullptr, 0, nullptr, nullptr);
        std::string out(n, '\0');
        WideCharToMultiByte(cp, 0, s.data(), (int)s.size(), out.data(), n, nullptr, nullptr);
        return out;
    }

    bool MakeKeyA(const char* path, std::wstring& key, std::wstring* fullPath)
    {
        if (!path || !*path) return false;
        auto w = FromFileApiString(path);
        return MakeKey(w.c_str(), key, fullPath);
    }

    std::wstring KeyToPath(std::wstring_view key)
    {
        if (key.empty()) return g_gameDir.substr(0, g_gameDir.size() - 1);
        return g_gameDir + std::wstring(key);
    }

    std::wstring_view ParentKey(std::wstring_view key)
    {
        auto p = key.find_last_of(L'\\');
        return p == std::wstring_view::npos ? std::wstring_view() : key.substr(0, p);
    }

    std::wstring_view NameOfKey(std::wstring_view key)
    {
        auto p = key.find_last_of(L'\\');
        return p == std::wstring_view::npos ? key : key.substr(p + 1);
    }

    bool MatchesMask(std::wstring_view name, std::wstring_view mask)
    {
        if (mask.empty() || mask == L"*" || mask == L"*.*") return true;
        // "*." matches names without an extension
        if (mask == L"*.") return name.find(L'.') == std::wstring_view::npos;
        size_t n = 0, m = 0, starM = std::wstring_view::npos, starN = 0;
        while (n < name.size())
        {
            if (m < mask.size() && (mask[m] == L'?' || Fold(mask[m]) == Fold(name[n])))
            {
                ++n;
                ++m;
            }
            else if (m < mask.size() && mask[m] == L'*')
            {
                starM = m++;
                starN = n;
            }
            else if (starM != std::wstring_view::npos)
            {
                m = starM + 1;
                n = ++starN;
            }
            else
                return false;
        }
        while (m < mask.size() && (mask[m] == L'*' || (mask[m] == L'.' && m + 1 < mask.size() && mask[m + 1] == L'*'))) ++m;
        return m == mask.size();
    }
}
