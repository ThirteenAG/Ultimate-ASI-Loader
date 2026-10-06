// File overloading API exported to plugins (x86.def / x64.def).
#include "internal.hpp"
#include "../core/paths.hpp"
#include "../core/strings.hpp"
#include <cstring>

namespace ual::vfs
{
    namespace
    {
        // False, with an empty string, if the result does not fit.
        template<class C>
        bool CopyOut(const std::basic_string<C>& s, C* out, size_t outSize)
        {
            if (!out || !outSize) return false;
            if (s.size() >= outSize)
            {
                out[0] = 0;
                return false;
            }
            std::memcpy(out, s.c_str(), (s.size() + 1) * sizeof(C));
            return true;
        }

        // Path the file is served from, "" if not overloaded. Relative input gives a path relative to the current directory.
        std::wstring OverloadedPath(const std::wstring& input)
        {
            std::wstring key;
            if (input.empty() || !MakeKey(input.c_str(), key)) return {};
            Resolved r = Resolve(key);
            if (r.kind != Resolved::Physical && !(r.kind == Resolved::Virtual && HasZipLayers() && !ApiFile(key))) return {};
            std::filesystem::path in(input);
            if (in.is_absolute()) return r.path;
            auto rel = LexicallyRelativeCaseIns(std::filesystem::path(r.path), std::filesystem::path(CurrentDirectory()));
            return rel.empty() ? r.path : rel.wstring();
        }

        // key and original-case display form of a plugin path
        bool KeyOf(const std::wstring& path, std::wstring& key, std::wstring& display)
        {
            std::wstring full;
            if (path.empty() || !MakeKey(path.c_str(), key, &full) || key.empty()) return false;
            display = full.size() >= key.size() ? full.substr(full.size() - key.size()) : key;
            return true;
        }

        bool AddFile(const std::wstring& path, const uint8_t* data, size_t size, int priority)
        {
            std::wstring key, display;
            if (!data || !size || !KeyOf(path, key, display)) return false;
            return AddApiFile(key, display, data, size, priority);
        }

        void RemoveFile(const std::wstring& path)
        {
            std::wstring key, display;
            if (KeyOf(path, key, display)) RemoveApiFile(key);
        }

        bool AddPath(const std::wstring& original, const std::wstring& target, int priority)
        {
            std::wstring key, display;
            if (target.empty() || !KeyOf(original, key, display)) return false;
            std::filesystem::path t(target);
            std::wstring absolute = t.is_relative() ? GameDir() + target : target; // relative to the game folder
            return AddApiPath(key, display, absolute, priority);
        }

        void RemovePath(const std::wstring& original)
        {
            std::wstring key, display;
            if (KeyOf(original, key, display)) RemoveApiPath(key);
        }
    }
}

using namespace ual;
using namespace ual::vfs;

extern "C"
{
    bool WINAPI GetOverloadPathW(wchar_t* out, size_t outSize)
    {
        auto root = FirstLayerRoot();
        return !root.empty() && CopyOut(root, out, outSize);
    }

    bool WINAPI GetOverloadPathA(char* out, size_t outSize)
    {
        auto root = FirstLayerRoot();
        return !root.empty() && CopyOut(ToFileApiString(root), out, outSize);
    }

    bool WINAPI GetOverloadedFilePathW(const wchar_t* file, wchar_t* out, size_t outSize)
    {
        if (!file) return false;
        auto path = OverloadedPath(file);
        if (path.empty()) return false;
        return !out || !outSize ? true : CopyOut(path, out, outSize);
    }

    bool WINAPI GetOverloadedFilePathA(const char* file, char* out, size_t outSize)
    {
        if (!file) return false;
        auto path = OverloadedPath(FromFileApiString(file));
        if (path.empty()) return false;
        return !out || !outSize ? true : CopyOut(ToFileApiString(path), out, outSize);
    }

    bool WINAPI AddVirtualFileForOverloadW(const wchar_t* path, const uint8_t* data, size_t size, int priority)
    {
        return path && AddFile(path, data, size, priority);
    }

    bool WINAPI AddVirtualFileForOverloadA(const char* path, const uint8_t* data, size_t size, int priority)
    {
        return path && AddFile(FromFileApiString(path), data, size, priority);
    }

    void WINAPI RemoveVirtualFileFromOverloadW(const wchar_t* path)
    {
        if (path) RemoveFile(path);
    }

    void WINAPI RemoveVirtualFileFromOverloadA(const char* path)
    {
        if (path) RemoveFile(FromFileApiString(path));
    }

    bool WINAPI AddVirtualPathForOverloadW(const wchar_t* original, const wchar_t* target, int priority)
    {
        return original && target && AddPath(original, target, priority);
    }

    bool WINAPI AddVirtualPathForOverloadA(const char* original, const char* target, int priority)
    {
        return original && target && AddPath(FromFileApiString(original), FromFileApiString(target), priority);
    }

    void WINAPI RemoveVirtualPathFromOverloadW(const wchar_t* original)
    {
        if (original) RemovePath(original);
    }

    void WINAPI RemoveVirtualPathFromOverloadA(const char* original)
    {
        if (original) RemovePath(FromFileApiString(original));
    }
}
