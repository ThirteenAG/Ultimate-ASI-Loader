#pragma once
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <string>
#include <vector>

namespace ual
{
    std::wstring ModulePath(HMODULE module);         // "" if unknown
    std::vector<HMODULE> LoadedModules();
    std::wstring CurrentDirectory();
    std::wstring KnownFolder(REFKNOWNFOLDERID id);
    std::wstring SystemDirectory();                  // with trailing backslash

    bool FileExists(const std::wstring& path);       // false for directories
    bool DirectoryExists(const std::wstring& path);

    std::wstring ParentDirectory(const std::wstring& path); // with trailing backslash
    std::wstring FileNameOf(const std::wstring& path);

    // lexically_relative with case-insensitive components.
    std::filesystem::path LexicallyRelativeCaseIns(const std::filesystem::path& path, const std::filesystem::path& base);

    bool ReadWholeFile(const std::wstring& path, std::string& out);
}
