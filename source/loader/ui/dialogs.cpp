#include "internal.hpp"
#include "../core/loader.hpp"
#include "../core/log.hpp"
#include <memory>
#include <shellapi.h>

namespace ual::ui
{
    std::wstring ChoiceCaptionText(const Choice& c)
    {
        return c.zip ? c.caption + L" [ZIP]" : c.caption;
    }

    std::wstring ChoiceDetailText(const Choice& c)
    {
        std::wstring s = c.path;
        for (const auto& i : c.includes) s += L" + " + i;
        return s;
    }

    std::wstring PluginErrorHeader(const std::wstring& fileName, unsigned long error)
    {
        return L"Unable to load " + fileName + L". Error: " + std::to_wstring(error);
    }

    std::wstring PluginErrorContent(unsigned long error)
    {
        if (error != ERROR_MOD_NOT_FOUND) return {};
        return std::wstring(L"This ASI file requires a dependency that is missing from your system. To identify the missing dependency, download and run "
                            L"the free, open-source app, <a href=\"") + kDependenciesUrl + L"/releases/latest\">Dependencies</a>.\n\n<a href=\"" +
               kDependenciesUrl + L"\">" + kDependenciesUrl + L"</a>";
    }

    HICON ExecutableIcon()
    {
        wchar_t exe[MAX_PATH];
        if (!GetModuleFileNameW(nullptr, exe, MAX_PATH)) return nullptr;
        HICON icon = ExtractIconW(GetModuleHandleW(nullptr), exe, 0);
        return icon == (HICON)1 ? nullptr : icon;
    }

    void OpenUrl(HWND owner, const wchar_t* url)
    {
        if (url && (!_wcsnicmp(url, L"https://", 8) || !_wcsnicmp(url, L"http://", 7))) ShellExecuteW(owner, L"open", url, nullptr, nullptr, SW_SHOWNORMAL);
    }

    bool IsPackagedProcess()
    {
        using GetPackageFamilyNameFn = LONG(WINAPI*)(HANDLE, UINT32*, PWSTR);
        auto fn = (GetPackageFamilyNameFn)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetPackageFamilyName");
        UINT32 size = 0;
        return fn && fn(GetCurrentProcess(), &size, nullptr) == ERROR_INSUFFICIENT_BUFFER;
    }

    namespace
    {
        // last resort, for packaged (UWP) processes and where task dialogs fail
        void MessageBoxError(const std::wstring& header, const std::wstring& content)
        {
            // <a href="url">label</a> -> label
            std::wstring plain;
            for (size_t i = 0; i < content.size();)
            {
                if (content.compare(i, 9, L"<a href=\"") == 0)
                {
                    size_t end = content.find(L"\">", i);
                    if (end != std::wstring::npos)
                    {
                        i = end + 2;
                        continue;
                    }
                }
                if (content.compare(i, 4, L"</a>") == 0)
                {
                    i += 4;
                    continue;
                }
                plain += content[i++];
            }
            std::wstring text = header;
            if (!plain.empty()) text += L"\n\n" + plain;
            MessageBoxW(nullptr, text.c_str(), kTitle, MB_OK | MB_ICONERROR);
        }

        int MessageBoxChooseOverloadFolder(const std::vector<Choice>& choices)
        {
            for (size_t i = 0; i < choices.size(); ++i)
            {
                std::wstring msg = L"Multiple folders have been specified for file overloading.\nUse this folder?\n\n" + ChoiceCaptionText(choices[i]) + L"\n" +
                                   ChoiceDetailText(choices[i]);
                if (MessageBoxW(nullptr, msg.c_str(), kTitle, MB_YESNO | MB_ICONQUESTION) == IDYES) return (int)i;
            }
            return -1;
        }
    }

    namespace
    {
        void ShowErrorNow(const std::wstring& header, const std::wstring& content)
        {
            if (!IsPackagedProcess())
            {
                if (GetSettings().modernUI && modern::ShowError(header, content)) return;
                if (taskdialog::ShowError(header, content)) return;
            }
            MessageBoxError(header, content);
        }
    }

    // No window is shown under the loader lock (plugins loaded from DllMain). Any window can deadlock there,
    // e.g. when accessibility tools query it and the threads that would answer can't start.

    void ShowError(const std::wstring& header, const std::wstring& content)
    {
        if (!LoaderLockHeldByThisThread()) return ShowErrorNow(header, content);
        // the thread starts once the loader lock is released
        struct Args
        {
            std::wstring header, content;
        };
        auto args = new Args{ header, content };
        HANDLE t = CreateThread(nullptr, 0, [](LPVOID p) -> DWORD {
            std::unique_ptr<Args> a((Args*)p);
            ShowErrorNow(a->header, a->content);
            return 0;
        }, args, 0, nullptr);
        if (t) CloseHandle(t);
        else delete args;
    }

    void ShowPluginLoadError(const std::wstring& fileName, unsigned long error)
    {
        // packaged processes get only the first line, as before
        ShowError(PluginErrorHeader(fileName, error), IsPackagedProcess() ? std::wstring() : PluginErrorContent(error));
    }

    int ChooseOverloadFolder(const std::vector<Choice>& choices)
    {
        if (choices.empty()) return -1;
        if (LoaderLockHeldByThisThread())
        {
            UAL_LOG("several update folders, but the loader lock is held (DontLoadFromDllMain=0): the first one is used without asking");
            return 0; // same as when the countdown runs out
        }
        int result = -1;
        if (!IsPackagedProcess())
        {
            if (GetSettings().modernUI && modern::ChooseOverloadFolder(choices, result)) return result;
            if (taskdialog::ChooseOverloadFolder(choices, result)) return result;
        }
        return MessageBoxChooseOverloadFolder(choices);
    }
}
