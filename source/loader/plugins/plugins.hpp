// Load order: LoadExtraPlugins, the loader's folder (unless LoadFromScriptsOnly), scripts\, plugins\,
// active update folders. LoadRecursively adds one level of sub folders. Each plugin loads with the
// current directory set to its folder, then its InitializeASI export is called if present.
#pragma once
#include <string>
#include <vector>

namespace ual::plugins
{
    // extraFolders are absolute, e.g. active update folders
    void LoadAll(const std::vector<std::wstring>& extraFolders);
}
