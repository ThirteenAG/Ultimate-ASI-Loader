// Shared by the dialog implementations. dialogs.cpp dispatches between them.
#pragma once
#include "dialogs.hpp"
#include <windows.h>

#define UAL_WIDEN2(x) L##x
#define UAL_WIDEN(x) UAL_WIDEN2(x)

namespace ual::ui
{
    constexpr int kFirstButton = 1000;     // choice i is button kFirstButton + i (TDM_CLICK_BUTTON)
    constexpr int kCountdownSeconds = 10;
    constexpr int kCustomIconId = 102;
    constexpr const wchar_t* kTitle = L"ASI Loader";
    constexpr const wchar_t* kChooseInstruction = L"Select Override (Update) Folder";
    constexpr const wchar_t* kChooseContent = L"Multiple folders have been specified for file overloading.\nPlease select which folder you want to use:";
    constexpr const wchar_t* kUpdateUrl = UAL_WIDEN(rsc_UpdateUrl);
    constexpr const wchar_t* kDependenciesUrl = L"https://github.com/lucasg/Dependencies";

    // "caption [ZIP]" and "path + a + b"
    std::wstring ChoiceCaptionText(const Choice& c);
    std::wstring ChoiceDetailText(const Choice& c);

    // "Unable to load <file>. Error: <n>", plus an explanation with <a href> links if there is one
    std::wstring PluginErrorHeader(const std::wstring& fileName, unsigned long error);
    std::wstring PluginErrorContent(unsigned long error);

    // For the taskbar button. Null if none, otherwise free with DestroyIcon.
    HICON ExecutableIcon();

    // http(s) only
    void OpenUrl(HWND owner, const wchar_t* url);

    // False if nothing could be shown. Task dialogs fail under the loader lock during process start.
    namespace taskdialog
    {
        bool ShowError(const std::wstring& header, const std::wstring& content);
        bool ChooseOverloadFolder(const std::vector<Choice>& choices, int& result);
    }

    namespace modern
    {
        // False if the window can't be shown here (no Direct2D, loader lock held, ...).
        bool ShowError(const std::wstring& header, const std::wstring& content);
        bool ChooseOverloadFolder(const std::vector<Choice>& choices, int& result);
    }
}
