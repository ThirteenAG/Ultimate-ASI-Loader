// Virtual file system behind [FileLoader] OverloadFromFolder, zip packages and the plugin overload API.
#pragma once
#include <string>
#include <vector>

namespace ual::vfs
{
    // Reads the config, scans packages, asks which folder to use when several are
    // configured and installs the file hooks if needed.
    void Setup();

    // Active update folders on disk, absolute, no trailing backslash. ASI plugins load from them too.
    std::vector<std::wstring> PluginFolders();

    // The process is crashing: from now on every file hook calls the original directly, so the crash
    // reporter cannot block on a lock held by one of the suspended threads. Not reversible.
    void PassThroughForCrash();
}
