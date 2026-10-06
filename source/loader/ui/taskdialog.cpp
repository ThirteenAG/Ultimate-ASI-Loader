#include "internal.hpp"
#include "../core/loader.hpp"
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#pragma comment(lib, "Comctl32.lib")
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace ual::ui::taskdialog
{
    namespace
    {
        const wchar_t* const kFooter = L"<a href=\"" UAL_WIDEN(rsc_UpdateUrl) L"\">" UAL_WIDEN(rsc_UpdateUrl) L"</a>";

        // only one dialog is on screen at a time
        struct DialogState
        {
            HWND hwnd = nullptr;
            bool countdown = false;
            bool interacted = false;
            POINT lastMouse{ -1, -1 };
            HHOOK mouseHook = nullptr;
            HHOOK messageHook = nullptr;
            HICON taskbarIcon = nullptr;
        } g_dialog;

        void Unhook()
        {
            if (g_dialog.mouseHook) UnhookWindowsHookEx(g_dialog.mouseHook);
            if (g_dialog.messageHook) UnhookWindowsHookEx(g_dialog.messageHook);
            g_dialog.mouseHook = g_dialog.messageHook = nullptr;
        }

        // any user activity stops the countdown
        void StopCountdown()
        {
            if (g_dialog.interacted) return;
            g_dialog.interacted = true;
            if (g_dialog.countdown)
            {
                SendMessageW(g_dialog.hwnd, TDM_SET_ELEMENT_TEXT, TDE_FOOTER, (LPARAM)kFooter);
                SendMessageW(g_dialog.hwnd, TDM_SET_PROGRESS_BAR_POS, 0, 0);
            }
            Unhook();
        }

        LRESULT CALLBACK MouseProc(int code, WPARAM wp, LPARAM lp)
        {
            if (code == HC_ACTION && wp == WM_MOUSEMOVE && lp)
            {
                POINT pt = ((MSLLHOOKSTRUCT*)lp)->pt;
                if (g_dialog.lastMouse.x != -1 && (pt.x != g_dialog.lastMouse.x || pt.y != g_dialog.lastMouse.y)) StopCountdown();
                g_dialog.lastMouse = pt;
            }
            return CallNextHookEx(nullptr, code, wp, lp);
        }

        LRESULT CALLBACK MessageProc(int code, WPARAM wp, LPARAM lp)
        {
            if (code >= 0 && wp == PM_REMOVE && lp)
            {
                switch (((MSG*)lp)->message)
                {
                case WM_KEYDOWN:
                case WM_LBUTTONDOWN:
                case WM_RBUTTONDOWN:
                case WM_MBUTTONDOWN: StopCountdown(); break;
                }
            }
            return CallNextHookEx(nullptr, code, wp, lp);
        }

        HRESULT CALLBACK DialogProc(HWND hwnd, UINT note, WPARAM wp, LPARAM lp, LONG_PTR)
        {
            switch (note)
            {
            case TDN_CREATED:
                g_dialog.hwnd = hwnd;
                g_dialog.interacted = false;
                g_dialog.lastMouse = { -1, -1 };
                if ((g_dialog.taskbarIcon = ExecutableIcon()) != nullptr) SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)g_dialog.taskbarIcon);
                if (g_dialog.countdown)
                {
                    SendMessageW(hwnd, TDM_SET_PROGRESS_BAR_RANGE, 0, MAKELPARAM(0, kCountdownSeconds * 10));
                    SendMessageW(hwnd, TDM_SET_PROGRESS_BAR_POS, kCountdownSeconds * 10, 0);
                    g_dialog.mouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseProc, GetModuleHandleW(nullptr), 0);
                    g_dialog.messageHook = SetWindowsHookExW(WH_GETMESSAGE, MessageProc, nullptr, GetCurrentThreadId());
                }
                break;
            case TDN_TIMER: // wp is ms since creation, fires about every 200 ms
                if (g_dialog.countdown && !g_dialog.interacted)
                {
                    int remainingTenths = kCountdownSeconds * 10 - (int)(wp / 100);
                    if (remainingTenths <= 0)
                    {
                        g_dialog.interacted = true;
                        Unhook();
                        SendMessageW(hwnd, TDM_CLICK_BUTTON, kFirstButton, 0);
                        break;
                    }
                    SendMessageW(hwnd, TDM_SET_PROGRESS_BAR_POS, remainingTenths, 0);
                    std::wstring text = L"Auto-closing in " + std::to_wstring((remainingTenths + 9) / 10) + L" seconds...";
                    SendMessageW(hwnd, TDM_SET_ELEMENT_TEXT, TDE_FOOTER, (LPARAM)text.c_str());
                }
                break;
            case TDN_BUTTON_CLICKED:
                StopCountdown();
                break;
            case TDN_HYPERLINK_CLICKED:
                OpenUrl(hwnd, (LPCWSTR)lp);
                StopCountdown();
                break;
            case TDN_DESTROYED:
                Unhook();
                if (g_dialog.taskbarIcon) DestroyIcon(g_dialog.taskbarIcon);
                g_dialog = {};
                break;
            }
            return S_OK;
        }

        void SetMainIcon(TASKDIALOGCONFIG& tdc, PCWSTR fallback)
        {
            if (auto icon = (HICON)LoadImageW(Self().module, MAKEINTRESOURCEW(kCustomIconId), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED))
            {
                tdc.dwFlags |= TDF_USE_HICON_MAIN;
                tdc.hMainIcon = icon;
            }
            else
                tdc.pszMainIcon = fallback;
        }
    }

    bool ShowError(const std::wstring& header, const std::wstring& content)
    {
        TASKDIALOG_BUTTON buttons[] = { { kFirstButton, L"Continue" } };
        TASKDIALOGCONFIG tdc{ sizeof(tdc) };
        tdc.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ENABLE_HYPERLINKS | TDF_SIZE_TO_CONTENT | TDF_CAN_BE_MINIMIZED;
        tdc.pButtons = buttons;
        tdc.cButtons = _countof(buttons);
        tdc.pszWindowTitle = kTitle;
        tdc.pszMainInstruction = header.c_str();
        tdc.pszContent = content.c_str();
        tdc.pfCallback = DialogProc;
        SetMainIcon(tdc, TD_ERROR_ICON);
        g_dialog.countdown = false;
        int clicked = 0;
        return SUCCEEDED(TaskDialogIndirect(&tdc, &clicked, nullptr, nullptr));
    }

    bool ChooseOverloadFolder(const std::vector<Choice>& choices, int& result)
    {
        std::vector<std::wstring> texts;
        std::vector<TASKDIALOG_BUTTON> buttons;
        texts.reserve(choices.size());
        for (size_t i = 0; i < choices.size(); ++i)
        {
            texts.push_back(L"&" + ChoiceCaptionText(choices[i]) + L"\n" + ChoiceDetailText(choices[i]));
            buttons.push_back({ kFirstButton + (int)i, texts.back().c_str() });
        }
        TASKDIALOGCONFIG tdc{ sizeof(tdc) };
        tdc.dwFlags = TDF_USE_COMMAND_LINKS | TDF_ENABLE_HYPERLINKS | TDF_SIZE_TO_CONTENT | TDF_CAN_BE_MINIMIZED | TDF_SHOW_PROGRESS_BAR | TDF_CALLBACK_TIMER;
        tdc.pButtons = buttons.data();
        tdc.cButtons = (UINT)buttons.size();
        tdc.nDefaultButton = kFirstButton;
        tdc.pszWindowTitle = kTitle;
        tdc.pszMainInstruction = kChooseInstruction;
        tdc.pszContent = kChooseContent;
        tdc.pszFooter = kFooter;
        tdc.pszFooterIcon = TD_INFORMATION_ICON;
        tdc.pfCallback = DialogProc;
        SetMainIcon(tdc, TD_WARNING_ICON);
        g_dialog.countdown = true;
        int clicked = 0;
        if (FAILED(TaskDialogIndirect(&tdc, &clicked, nullptr, nullptr))) return false;
        result = clicked >= kFirstButton && clicked < kFirstButton + (int)choices.size() ? clicked - kFirstButton : -1;
        return true;
    }
}
