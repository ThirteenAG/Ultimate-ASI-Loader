// ModernUI=0 uses task dialogs (taskdialog.cpp), ModernUI=1 our own Direct2D window (modern.cpp) with task dialogs
// as fallback. Packaged (UWP) processes get message boxes because task dialogs aren't available there.
#pragma once
#include <string>
#include <vector>

namespace ual::ui
{
    void ShowPluginLoadError(const std::wstring& fileName, unsigned long error);

    // content may contain <a href="...">links</a>. Under the loader lock it is shown later from its own thread.
    void ShowError(const std::wstring& header, const std::wstring& content);

    struct Choice
    {
        std::wstring caption;               // update.txt caption or the folder name
        std::wstring path;                  // folder, or where a zip package is mounted
        bool zip = false;                   // from a zip package, also if only an included folder is
        std::vector<std::wstring> includes; // activated with it, at lower priority
    };

    // Picks the first folder after 10 seconds unless the user interacts. Returns -1 if nothing was chosen.
    int ChooseOverloadFolder(const std::vector<Choice>& choices);

    bool IsPackagedProcess();
}
