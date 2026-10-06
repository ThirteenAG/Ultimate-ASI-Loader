#include "paths.hpp"
#include "strings.hpp"
#include <shlobj.h>

namespace ual
{
    std::wstring ModulePath(HMODULE module)
    {
        std::wstring s(MAX_PATH, L'\0');
        for (int i = 0; i < 8; ++i)
        {
            DWORD n = GetModuleFileNameW(module, s.data(), (DWORD)s.size());
            if (n == 0) return {};
            if (n < s.size())
            {
                s.resize(n);
                return s;
            }
            s.resize(s.size() * 2);
        }
        return {};
    }

    std::wstring CurrentDirectory()
    {
        DWORD n = GetCurrentDirectoryW(0, nullptr);
        if (!n) return {};
        std::wstring s(n, L'\0');
        n = GetCurrentDirectoryW(n, s.data());
        s.resize(n);
        return s;
    }

    std::wstring KnownFolder(REFKNOWNFOLDERID id)
    {
        std::wstring r;
        PWSTR p = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(id, 0, nullptr, &p)) && p) r = p;
        CoTaskMemFree(p);
        return r;
    }

    std::wstring SystemDirectory()
    {
        wchar_t buf[MAX_PATH];
        UINT n = GetSystemDirectoryW(buf, MAX_PATH);
        std::wstring s = n && n < MAX_PATH ? std::wstring(buf, n) : KnownFolder(FOLDERID_System);
        if (!s.empty() && s.back() != L'\\') s += L'\\';
        return s;
    }

    bool FileExists(const std::wstring& path)
    {
        DWORD a = GetFileAttributesW(path.c_str());
        return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
    }

    bool DirectoryExists(const std::wstring& path)
    {
        DWORD a = GetFileAttributesW(path.c_str());
        return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
    }

    std::wstring ParentDirectory(const std::wstring& path)
    {
        auto p = path.find_last_of(L"\\/");
        return p == std::wstring::npos ? std::wstring() : path.substr(0, p + 1);
    }

    std::wstring FileNameOf(const std::wstring& path)
    {
        auto p = path.find_last_of(L"\\/");
        return p == std::wstring::npos ? path : path.substr(p + 1);
    }

    std::filesystem::path LexicallyRelativeCaseIns(const std::filesystem::path& path, const std::filesystem::path& base)
    {
        if (!IEquals(path.root_name().native(), base.root_name().native()) || path.is_absolute() != base.is_absolute() ||
            (!path.has_root_directory() && base.has_root_directory()))
            return {};
        auto a = path.begin(), b = base.begin();
        while (a != path.end() && b != base.end() && IEquals(a->native(), b->native()))
        {
            ++a;
            ++b;
        }
        if (a == path.end() && b == base.end()) return L".";
        int count = 0;
        for (auto it = b; it != base.end(); ++it)
        {
            const auto& e = it->native();
            if (e == L"..") --count;
            else if (!e.empty() && e != L".") ++count;
        }
        if (count < 0) return {};
        std::filesystem::path result;
        for (int i = 0; i < count; ++i) result /= L"..";
        for (auto it = a; it != path.end(); ++it) result /= *it;
        return result;
    }

    bool ReadWholeFile(const std::wstring& path, std::string& out)
    {
        HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (f == INVALID_HANDLE_VALUE) return false;
        LARGE_INTEGER size{};
        bool ok = GetFileSizeEx(f, &size) && size.QuadPart < (1ll << 30);
        if (ok)
        {
            out.resize((size_t)size.QuadPart);
            DWORD read = 0;
            ok = out.empty() || (ReadFile(f, out.data(), (DWORD)out.size(), &read, nullptr) && read == out.size());
        }
        CloseHandle(f);
        return ok;
    }
}
