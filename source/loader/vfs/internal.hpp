// Paths are compared as keys: lower case, backslash separated, relative to the exe folder,
// no leading/trailing separator ("" is the exe folder). Paths elsewhere on the same drive
// map to the part after the common ancestor (<root>\bin\game.exe sees <root>\data\x as data\x).
//
// Resolution order: API virtual files, API virtual paths (resolved again), then layers
// (update folders and zips) by priority. Paths inside a layer are not overloaded;
// inside a zip layer they are served from the archive.
#pragma once
#include <windows.h>
#include <atomic>
#include <cstdint>
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "zip.hpp"

namespace ual::vfs
{
    void InitKeys(const std::wstring& gameDir);      // gameDir with trailing backslash
    const std::wstring& GameDir();

    // False for paths with no key (other drive, device path, empty).
    bool MakeKey(const wchar_t* path, std::wstring& key, std::wstring* fullPath = nullptr);
    bool MakeKeyA(const char* path, std::wstring& key, std::wstring* fullPath = nullptr);
    std::wstring KeyToPath(std::wstring_view key);     // absolute path in the game folder
    std::wstring_view ParentKey(std::wstring_view key); // "" for top-level items
    std::wstring_view NameOfKey(std::wstring_view key);

    // ANSI or OEM, whichever the file APIs currently use (SetFileApisToOEM).
    std::wstring FromFileApiString(const char* s);
    std::string ToFileApiString(std::wstring_view s);

    // FindFirstFile wildcard semantics, *.* matches everything.
    bool MatchesMask(std::wstring_view name, std::wstring_view mask);

    // Read-only contents of a virtual file (memory, out-of-process server or zip entry).
    class FileData
    {
    public:
        virtual ~FileData() = default;
        virtual uint64_t Size() const = 0;
        // read is 0 at or past the end; false only on I/O error.
        virtual bool Read(uint64_t offset, void* buffer, DWORD count, DWORD& read) = 0;
        // API files only.
        virtual bool Append(const uint8_t*, size_t) { return false; }
        FILETIME created{}, written{};
    };

    std::shared_ptr<FileData> MakeMemoryData(const uint8_t* data, size_t size); // out of process on Win32 when the server runs

    // Stored entries are read in place.
    std::shared_ptr<FileData> OpenZipEntry(const std::shared_ptr<Archive>& archive, const Archive::Entry& entry);

    struct ListedItem
    {
        std::wstring name; // original case
        bool directory;
        uint64_t size;
        FILETIME time;
    };

    struct Resolved
    {
        enum Kind
        {
            None,      // use the path as is
            Physical,  // redirected to another file on disk (path)
            Virtual,   // served from memory or zip (data)
            Directory, // folder that exists only virtually
        } kind = None;
        std::wstring path;
        std::shared_ptr<FileData> data;
        FILETIME time{};
    };

    Resolved Resolve(const std::wstring& key);
    // Items the VFS adds to or replaces in a folder listing; false if none.
    bool OverlayListing(const std::wstring& dirKey, std::vector<ListedItem>& items);
    bool IsVirtualDirectory(const std::wstring& dirKey);

    // layers.cpp
    struct LayerSpec
    {
        std::wstring name;   // as configured
        std::wstring root;   // absolute folder path, or the mount point of a zip
        int priority;
        std::vector<std::shared_ptr<Archive>> archives; // zip layer if not empty
    };
    void ActivateLayers(std::vector<LayerSpec> layers); // highest priority first
    bool HasLayers();
    bool HasZipLayers();
    Resolved ResolveInLayers(const std::wstring& key);
    bool LayerListing(const std::wstring& dirKey, std::vector<ListedItem>& items);
    std::vector<std::wstring> PhysicalLayerRoots();
    std::wstring FirstLayerRoot();

    // registry.cpp, files and paths added through the API
    bool HasApiEntries();
    std::shared_ptr<FileData> ApiFile(const std::wstring& key);
    bool ApiPath(const std::wstring& key, std::wstring& target);
    bool ApiListing(const std::wstring& dirKey, std::vector<ListedItem>& items);
    bool ApiDirectory(const std::wstring& dirKey);
    bool AddApiFile(const std::wstring& key, const std::wstring& display, const uint8_t* data, size_t size, int priority);
    void RemoveApiFile(const std::wstring& key);
    bool AddApiPath(const std::wstring& key, const std::wstring& display, const std::wstring& target, int priority);
    void RemoveApiPath(const std::wstring& key);

    // hooks.cpp
    void InstallPathHooks();   // CreateFile, GetFileAttributes, FindFirstFile, LoadLibraryEx, ...
    void InstallHandleHooks(); // ReadFile, SetFilePointer, CloseHandle, ... on virtual handles

}
