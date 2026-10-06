// Zip archives in <game>\packages, whole (name.zip) or split (name.zip.001, name.zip.1, ...).
// Their top-level folders stand in for configured update folders missing on disk.
// Archives providing the same folder are merged, the later one by name wins.
#pragma once
#include "zip.hpp"
#include <memory>
#include <string>
#include <vector>

namespace ual::vfs
{
    struct Package
    {
        std::shared_ptr<Archive> archive;
        std::vector<std::wstring> roots; // top-level folders, original case
    };

    // "x.zip" -> (x, 0); "x.zip.12" -> (x, 12); otherwise false
    bool PackagePart(const std::wstring& name, std::wstring& base, uint64_t& part);

    // Ordered by name.
    std::vector<Package> ScanPackages(const std::wstring& dir);
}
