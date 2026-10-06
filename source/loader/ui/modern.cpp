// ModernUI=1 dialogs drawn with Direct2D / DirectWrite, matching the task dialog features, light/dark theme and DPI.
// D2D and DWrite load only when a dialog is shown. Under the loader lock the task dialog is used instead.
// For tests and screen readers every text and choice is also a hidden child window, and TDM_CLICK_BUTTON
// works with the task dialog's button ids.
#include "internal.hpp"
#include "../core/loader.hpp"
#include <windowsx.h>
#include <commctrl.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <intrin.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace ual::ui::modern
{
    using Microsoft::WRL::ComPtr;

    namespace
    {
        constexpr wchar_t kWindowClass[] = L"UltimateASILoaderDialog";
        constexpr UINT_PTR kTimerFrame = 1;
        constexpr float kWidth = 600.0f;
        constexpr float kPad = 28.0f;
        constexpr float kTitleBarHeight = 40.0f;
        constexpr float kWindowButtonWidth = 46.0f;
        constexpr float kLogoSize = 48.0f;
        constexpr float kRowGap = 6.0f;
        constexpr float kRowRadius = 8.0f;
        constexpr float kRowIconWidth = 52.0f; // folder glyph column
        constexpr float kFooterHeight = 52.0f;
        constexpr float kButtonWidth = 128.0f;
        constexpr float kButtonHeight = 34.0f;
        constexpr float kHoverMs = 120.0f;

        // loader lock / system

        HMODULE LoadSystemLibrary(const wchar_t* name)
        {
            if (HMODULE m = LoadLibraryExW(name, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32)) return m;
            wchar_t path[MAX_PATH]; // systems without LOAD_LIBRARY_SEARCH_SYSTEM32 support
            UINT n = GetSystemDirectoryW(path, MAX_PATH);
            if (!n || n + wcslen(name) + 2 > MAX_PATH) return nullptr;
            wcscat_s(path, L"\\");
            wcscat_s(path, name);
            return LoadLibraryW(path);
        }

        template<class F>
        F SystemFunction(const wchar_t* dll, const char* name)
        {
            HMODULE m = GetModuleHandleW(dll);
            if (!m) m = LoadSystemLibrary(dll);
            return m ? reinterpret_cast<F>(GetProcAddress(m, name)) : nullptr;
        }

        struct Factories
        {
            ComPtr<ID2D1Factory> d2d;
            ComPtr<IDWriteFactory> dwrite;
            std::wstring font = L"Segoe UI";
        };

        Factories* GetFactories()
        {
            static Factories* factories = []() -> Factories* {
                using D2D1CreateFactoryFn = HRESULT(WINAPI*)(D2D1_FACTORY_TYPE, REFIID, const D2D1_FACTORY_OPTIONS*, void**);
                using DWriteCreateFactoryFn = HRESULT(WINAPI*)(DWRITE_FACTORY_TYPE, REFIID, IUnknown**);
                auto createD2D = SystemFunction<D2D1CreateFactoryFn>(L"d2d1.dll", "D2D1CreateFactory");
                auto createDWrite = SystemFunction<DWriteCreateFactoryFn>(L"dwrite.dll", "DWriteCreateFactory");
                if (!createD2D || !createDWrite) return nullptr;
                auto f = new Factories();
                D2D1_FACTORY_OPTIONS options{};
                // several dialogs may run on their own threads at once (deferred errors), and they share this factory
                if (FAILED(createD2D(D2D1_FACTORY_TYPE_MULTI_THREADED, __uuidof(ID2D1Factory), &options, (void**)f->d2d.GetAddressOf())) ||
                    FAILED(createDWrite(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), (IUnknown**)f->dwrite.GetAddressOf())))
                {
                    delete f;
                    return nullptr;
                }
                ComPtr<IDWriteFontCollection> fonts;
                UINT32 index;
                BOOL exists = FALSE;
                if (SUCCEEDED(f->dwrite->GetSystemFontCollection(&fonts)) && SUCCEEDED(fonts->FindFamilyName(L"Segoe UI Variable Text", &index, &exists)) && exists)
                    f->font = L"Segoe UI Variable Text"; // Windows 11
                return f;
            }();
            return factories;
        }

        bool IsDarkMode()
        {
            DWORD light = 1, size = sizeof(light);
            if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme", RRF_RT_REG_DWORD,
                             nullptr, &light, &size) != ERROR_SUCCESS)
                return false;
            return light == 0;
        }

        COLORREF AccentColor()
        {
            DWORD abgr = 0, size = sizeof(abgr);
            if (RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM", L"AccentColor", RRF_RT_REG_DWORD, nullptr, &abgr, &size) == ERROR_SUCCESS)
                return abgr & 0xFFFFFF; // 0xAABBGGRR, low 24 bits are a COLORREF
            return RGB(0, 103, 192);   // Windows 11 default
        }

        D2D1_COLOR_F Color(COLORREF c, float a = 1.0f)
        {
            return D2D1::ColorF(GetRValue(c) / 255.0f, GetGValue(c) / 255.0f, GetBValue(c) / 255.0f, a);
        }

        D2D1_COLOR_F Mix(D2D1_COLOR_F a, D2D1_COLOR_F b, float t)
        {
            return D2D1::ColorF(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, a.a + (b.a - a.a) * t);
        }

        D2D1_COLOR_F WithAlpha(D2D1_COLOR_F c, float a)
        {
            c.a = a;
            return c;
        }

        float Luminance(D2D1_COLOR_F c)
        {
            return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b;
        }

        struct Theme
        {
            bool dark;
            D2D1_COLOR_F background, surface, surfaceHot, border, footer, text, textDim, accent, accentText, accentHot, link, danger, closeHot;

            static Theme Make()
            {
                Theme t{};
                t.dark = IsDarkMode();
                D2D1_COLOR_F accent = Color(AccentColor());
                if (t.dark)
                {
                    t.background = Color(RGB(32, 32, 32));
                    t.surface = Color(RGB(45, 45, 45));
                    t.border = Color(RGB(62, 62, 62));
                    t.footer = Color(RGB(39, 39, 39));
                    t.text = Color(RGB(255, 255, 255));
                    t.textDim = Color(RGB(200, 200, 200));
                    // a lighter accent reads better on dark backgrounds
                    t.accent = Luminance(accent) < 0.45f ? Mix(accent, D2D1::ColorF(1, 1, 1), 0.35f) : accent;
                }
                else
                {
                    t.background = Color(RGB(249, 249, 249));
                    t.surface = Color(RGB(255, 255, 255));
                    t.border = Color(RGB(224, 224, 224));
                    t.footer = Color(RGB(240, 240, 240));
                    t.text = Color(RGB(26, 26, 26));
                    t.textDim = Color(RGB(96, 96, 96));
                    t.accent = Luminance(accent) > 0.6f ? Mix(accent, D2D1::ColorF(0, 0, 0), 0.35f) : accent;
                }
                t.surfaceHot = Mix(t.surface, t.accent, t.dark ? 0.14f : 0.08f);
                t.accentText = Luminance(t.accent) > 0.55f ? D2D1::ColorF(0, 0, 0) : D2D1::ColorF(1, 1, 1);
                t.accentHot = Mix(t.accent, t.dark ? D2D1::ColorF(1, 1, 1) : D2D1::ColorF(0, 0, 0), 0.12f);
                t.link = t.accent;
                t.danger = t.dark ? Color(RGB(255, 99, 97)) : Color(RGB(196, 43, 28));
                t.closeHot = Color(RGB(196, 43, 28));
                return t;
            }
        };

        // text

        // text with <a href="...">links</a>, same format as the task dialog
        struct RichText
        {
            struct Link
            {
                UINT32 start, length;
                std::wstring url;
            };
            std::wstring text;
            std::vector<Link> links;

            static RichText Parse(const std::wstring& s)
            {
                RichText r;
                size_t i = 0;
                while (i < s.size())
                {
                    size_t open = s.find(L"<a href=\"", i);
                    if (open == std::wstring::npos)
                    {
                        r.text += s.substr(i);
                        break;
                    }
                    size_t urlEnd = s.find(L"\">", open + 9);
                    size_t close = urlEnd == std::wstring::npos ? std::wstring::npos : s.find(L"</a>", urlEnd + 2);
                    if (close == std::wstring::npos)
                    {
                        r.text += s.substr(i);
                        break;
                    }
                    r.text += s.substr(i, open - i);
                    std::wstring label = s.substr(urlEnd + 2, close - urlEnd - 2);
                    r.links.push_back({ (UINT32)r.text.size(), (UINT32)label.size(), s.substr(open + 9, urlEnd - open - 9) });
                    r.text += label;
                    i = close + 4;
                }
                return r;
            }
        };

        // wrap paths after a separator, not inside a folder name
        std::wstring WrappablePath(const std::wstring& path)
        {
            std::wstring s;
            s.reserve(path.size() + 16);
            for (wchar_t c : path)
            {
                s.push_back(c == L' ' ? L' ' : c);
                if (c == L'\\' || c == L'/') s.push_back(L'​'); // zero width space
            }
            return s;
        }

        // input that stops the countdown

        HWND g_window = nullptr; // one dialog at a time
        POINT g_lastMouse{ -1, -1 };
        HHOOK g_mouseHook = nullptr;
        void NotifyActivity();

        LRESULT CALLBACK MouseHook(int code, WPARAM wp, LPARAM lp)
        {
            if (code == HC_ACTION && wp == WM_MOUSEMOVE && lp)
            {
                POINT pt = ((MSLLHOOKSTRUCT*)lp)->pt;
                if (g_lastMouse.x != -1 && (pt.x != g_lastMouse.x || pt.y != g_lastMouse.y)) NotifyActivity();
                g_lastMouse = pt;
            }
            return CallNextHookEx(nullptr, code, wp, lp);
        }

        // the dialog

        class Dialog
        {
        public:
            std::wstring instruction;
            RichText content;
            bool error = false;              // error dialog: Continue button, no choices
            std::vector<Choice> choices;
            bool countdown = false;

            int result = -1;                 // chosen index, or 0 for Continue

            explicit Dialog(Factories& f) : f_(f) {}

            bool Run()
            {
                using SetContextFn = DPI_AWARENESS_CONTEXT(WINAPI*)(DPI_AWARENESS_CONTEXT);
                auto setContext = SystemFunction<SetContextFn>(L"user32.dll", "SetThreadDpiAwarenessContext");
                DPI_AWARENESS_CONTEXT previous = setContext ? setContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) : nullptr;
                bool shown = Create();
                if (shown) Loop();
                if (previous) setContext(previous);
                return shown;
            }

            void Activity()
            {
                if (!countdown || stopped_) return;
                stopped_ = true;
                if (g_mouseHook)
                {
                    UnhookWindowsHookEx(g_mouseHook);
                    g_mouseHook = nullptr;
                }
                UpdateFooterText();
                Invalidate();
            }

        private:
            enum class Part
            {
                None,
                Minimize,
                Close,
                Row,
                ContentLink,
                FooterLink,
                Button,
            };
            struct Hit
            {
                Part part = Part::None;
                int index = 0;
                bool operator==(const Hit&) const = default;
            };

            struct Row
            {
                ComPtr<IDWriteTextLayout> caption, path;
                struct Pill
                {
                    ComPtr<IDWriteTextLayout> text;
                    float width;
                };
                std::vector<Pill> pills;
                float top = 0, height = 0; // in the list
                float hot = 0;             // hover, animated 0..1
            };

            Factories& f_;
            Theme theme_ = Theme::Make();
            HWND hwnd_ = nullptr;
            UINT dpi_ = 96;
            ComPtr<ID2D1HwndRenderTarget> rt_;
            ComPtr<ID2D1SolidColorBrush> brush_;
            ComPtr<ID2D1Bitmap> logo_;
            ComPtr<IDWriteTextFormat> fTitle_, fInstruction_, fBody_, fCaption_, fPath_, fPill_, fFooter_, fButton_, fGlyph_;
            ComPtr<IDWriteTextLayout> lInstruction_, lContent_, lFooter_, lButton_, lGlyphInfo_, lGlyphError_, lTitle_;
            RichText footer_;
            std::vector<Row> rows_;
            HICON taskbarIcon_ = nullptr, smallIcon_ = nullptr;
            std::vector<HWND> automation_; // hidden children: [0] instruction, [1] content, [2] footer, then choices / button

            // layout, in DIPs
            float height_ = 0, headerTextX_ = 0, instructionTop_ = 0, contentTop_ = 0, listTop_ = 0, listHeight_ = 0, listContent_ = 0, barTop_ = 0, footerTop_ = 0;
            float scroll_ = 0;

            Hit hover_, pressed_;
            int focus_ = 0;            // row
            bool keyboard_ = false;    // show the focus ring after keyboard use
            bool stopped_ = false;     // countdown
            bool done_ = false;
            ULONGLONG shownAt_ = 0, lastFrame_ = 0;
            float hotMinimize_ = 0, hotClose_ = 0, hotButton_ = 0, hotFooterLink_ = 0;
            int lastSecond_ = 0;

            // setup

            ComPtr<IDWriteTextFormat> Format(float size, DWRITE_FONT_WEIGHT weight, bool wrap = true)
            {
                ComPtr<IDWriteTextFormat> fmt;
                f_.dwrite->CreateTextFormat(f_.font.c_str(), nullptr, weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, size, L"", &fmt);
                if (fmt && !wrap) fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
                return fmt;
            }

            ComPtr<IDWriteTextLayout> Layout(const std::wstring& text, IDWriteTextFormat* fmt, float width)
            {
                ComPtr<IDWriteTextLayout> l;
                f_.dwrite->CreateTextLayout(text.c_str(), (UINT32)text.size(), fmt, (std::max)(width, 1.0f), 10000.0f, &l);
                return l;
            }

            static float TextHeight(IDWriteTextLayout* l)
            {
                DWRITE_TEXT_METRICS m{};
                if (l) l->GetMetrics(&m);
                return m.height;
            }

            static float TextWidth(IDWriteTextLayout* l)
            {
                DWRITE_TEXT_METRICS m{};
                if (l) l->GetMetrics(&m);
                return m.widthIncludingTrailingWhitespace;
            }

            void UnderlineLinks(IDWriteTextLayout* l, const RichText& r)
            {
                for (const auto& link : r.links) l->SetUnderline(TRUE, { link.start, link.length });
            }

            void BuildText()
            {
                fTitle_ = Format(12.0f, DWRITE_FONT_WEIGHT_NORMAL, false);
                fInstruction_ = Format(20.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
                fBody_ = Format(14.0f, DWRITE_FONT_WEIGHT_NORMAL);
                fCaption_ = Format(15.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD);
                fPath_ = Format(12.5f, DWRITE_FONT_WEIGHT_NORMAL);
                fPill_ = Format(11.5f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false);
                fFooter_ = Format(12.5f, DWRITE_FONT_WEIGHT_NORMAL, false);
                fButton_ = Format(14.0f, DWRITE_FONT_WEIGHT_SEMI_BOLD, false);
                fGlyph_ = Format(11.0f, DWRITE_FONT_WEIGHT_BOLD, false);
                for (auto* fmt : { fButton_.Get(), fGlyph_.Get(), fPill_.Get() })
                {
                    fmt->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
                    fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
                }

                lTitle_ = Layout(kTitle, fTitle_.Get(), 400);
                headerTextX_ = kPad + kLogoSize + 18.0f;
                float textWidth = kWidth - headerTextX_ - kPad;
                lInstruction_ = Layout(instruction, fInstruction_.Get(), textWidth);
                lContent_ = Layout(content.text, fBody_.Get(), textWidth);
                UnderlineLinks(lContent_.Get(), content);
                lButton_ = Layout(L"Continue", fButton_.Get(), kButtonWidth);
                lButton_->SetMaxHeight(kButtonHeight); // centered in the button
                lGlyphInfo_ = Layout(L"i", fGlyph_.Get(), 16);
                lGlyphError_ = Layout(L"!", fGlyph_.Get(), 16);

                float rowWidth = kWidth - 2 * kPad;
                for (const auto& c : choices)
                {
                    Row r;
                    if (c.zip) r.pills.push_back({ Layout(L"ZIP", fPill_.Get(), 200), 0 });
                    for (const auto& inc : c.includes) r.pills.push_back({ Layout(L"+ " + inc, fPill_.Get(), 400), 0 });
                    float pillsWidth = 0;
                    for (auto& p : r.pills)
                    {
                        p.width = TextWidth(p.text.Get()) + 16.0f;
                        p.text->SetMaxWidth(p.width);
                        p.text->SetMaxHeight(20.0f);
                        pillsWidth += p.width + 6.0f;
                    }
                    float textWidth2 = rowWidth - kRowIconWidth - 16.0f;
                    r.caption = Layout(c.caption, fCaption_.Get(), textWidth2 - pillsWidth);
                    r.path = Layout(WrappablePath(c.path), fPath_.Get(), textWidth2);
                    r.height = (std::max)(58.0f, 12.0f + TextHeight(r.caption.Get()) + 3.0f + TextHeight(r.path.Get()) + 12.0f);
                    rows_.push_back(std::move(r));
                }
                UpdateFooterText();
            }

            void UpdateFooterText()
            {
                if (error) return;
                if (countdown && !stopped_)
                {
                    int remaining = (std::max)(1, kCountdownSeconds - (int)((GetTickCount64() - shownAt_) / 1000));
                    footer_ = RichText::Parse(L"Auto-closing in " + std::to_wstring(remaining) + L" seconds...");
                }
                else
                    footer_ = RichText::Parse(std::wstring(L"<a href=\"") + kUpdateUrl + L"\">" + kUpdateUrl + L"</a>");
                lFooter_ = Layout(footer_.text, fFooter_.Get(), kWidth - 2 * kPad - 28.0f);
                UnderlineLinks(lFooter_.Get(), footer_);
                if (automation_.size() > 2) SetWindowTextW(automation_[2], footer_.text.c_str());
            }

            // vertical positions, given the height available for the window
            void ComputeLayout(float maxHeight)
            {
                instructionTop_ = kTitleBarHeight + 14.0f;
                contentTop_ = instructionTop_ + TextHeight(lInstruction_.Get()) + (content.text.empty() ? 0.0f : 6.0f);
                float headerBottom = (std::max)(instructionTop_ + kLogoSize, contentTop_ + TextHeight(lContent_.Get())) + 22.0f;
                listTop_ = headerBottom;
                listContent_ = 0;
                for (auto& r : rows_)
                {
                    r.top = listContent_;
                    listContent_ += r.height + kRowGap;
                }
                if (!rows_.empty()) listContent_ -= kRowGap;
                float rest = (countdown ? 18.0f + 4.0f : 0.0f) + 18.0f + kFooterHeight;
                float available = (std::max)(rows_.empty() ? 0.0f : rows_.front().height, maxHeight - listTop_ - rest);
                listHeight_ = (std::min)(listContent_, available);
                barTop_ = listTop_ + listHeight_ + 18.0f;
                footerTop_ = (rows_.empty() ? listTop_ - 4.0f : barTop_ + (countdown ? 4.0f + 18.0f : 0.0f));
                height_ = footerTop_ + kFooterHeight;
                ClampScroll();
            }

            void ClampScroll()
            {
                scroll_ = std::clamp(scroll_, 0.0f, (std::max)(0.0f, listContent_ - listHeight_));
            }

            void LoadLogo()
            {
                int px = (int)std::lround(kLogoSize * dpi_ / 96.0f);
                HICON icon = (HICON)LoadImageW(Self().module, MAKEINTRESOURCEW(kCustomIconId), IMAGE_ICON, px, px, 0);
                if (!icon) return;
                BITMAPINFO bi{};
                bi.bmiHeader = { sizeof(BITMAPINFOHEADER), px, -px, 1, 32, BI_RGB };
                void* bits = nullptr;
                HDC dc = CreateCompatibleDC(nullptr);
                HBITMAP dib = CreateDIBSection(dc, &bi, DIB_RGB_COLORS, &bits, nullptr, 0);
                if (dib && bits)
                {
                    HGDIOBJ old = SelectObject(dc, dib);
                    DrawIconEx(dc, 0, 0, icon, px, px, 0, nullptr, DI_NORMAL);
                    SelectObject(dc, old);
                    auto p = (BYTE*)bits;
                    bool alpha = false;
                    for (int i = 0; i < px * px && !alpha; ++i) alpha = p[i * 4 + 3] != 0;
                    if (!alpha)
                        for (int i = 0; i < px * px; ++i) p[i * 4 + 3] = 255; // icon without alpha channel
                    D2D1_BITMAP_PROPERTIES props = D2D1::BitmapProperties(D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED), 96, 96);
                    logo_.Reset();
                    rt_->CreateBitmap(D2D1::SizeU(px, px), bits, px * 4, props, &logo_);
                }
                if (dib) DeleteObject(dib);
                DeleteDC(dc);
                DestroyIcon(icon);
            }

            static void RegisterClassOnce()
            {
                static bool registered = [] {
                    WNDCLASSEXW wc{ sizeof(wc) };
                    wc.lpfnWndProc = WndProc;
                    wc.hInstance = Self().module ? Self().module : GetModuleHandleW(nullptr);
                    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
                    wc.lpszClassName = kWindowClass;
                    return RegisterClassExW(&wc) != 0;
                }();
                (void)registered;
            }

            bool Create()
            {
                RegisterClassOnce();
                POINT cursor{};
                GetCursorPos(&cursor);
                HMONITOR monitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTOPRIMARY);
                MONITORINFO mi{ sizeof(mi) };
                GetMonitorInfoW(monitor, &mi);
                using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
                UINT dx = 96, dy = 96;
                if (auto getDpi = SystemFunction<GetDpiForMonitorFn>(L"shcore.dll", "GetDpiForMonitor"); !getDpi || FAILED(getDpi(monitor, 0, &dx, &dy)))
                {
                    HDC screen = GetDC(nullptr);
                    dx = GetDeviceCaps(screen, LOGPIXELSY);
                    ReleaseDC(nullptr, screen);
                }
                dpi_ = dx;

                BuildText();
                float s = dpi_ / 96.0f;
                ComputeLayout((mi.rcWork.bottom - mi.rcWork.top) * 0.9f / s);
                int w = (int)std::lround(kWidth * s), h = (int)std::lround(height_ * s);
                int x = mi.rcWork.left + (mi.rcWork.right - mi.rcWork.left - w) / 2;
                int y = mi.rcWork.top + (mi.rcWork.bottom - mi.rcWork.top - h) / 2;

                hwnd_ = CreateWindowExW(WS_EX_APPWINDOW, kWindowClass, kTitle, WS_POPUP | WS_MINIMIZEBOX | WS_SYSMENU, x, y, w, h, nullptr, nullptr,
                                        Self().module ? Self().module : GetModuleHandleW(nullptr), this);
                if (!hwnd_) return false;
                using GetDpiForWindowFn = UINT(WINAPI*)(HWND);
                if (auto getDpi = SystemFunction<GetDpiForWindowFn>(L"user32.dll", "GetDpiForWindow"))
                    if (UINT actual = getDpi(hwnd_); actual && actual != dpi_) dpi_ = actual, ResizeToContent(nullptr);

                D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
                                                                                   D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
                RECT rc;
                GetClientRect(hwnd_, &rc);
                if (FAILED(f_.d2d->CreateHwndRenderTarget(props, D2D1::HwndRenderTargetProperties(hwnd_, D2D1::SizeU(rc.right, rc.bottom)), &rt_)))
                {
                    DestroyWindow(hwnd_);
                    hwnd_ = nullptr;
                    return false;
                }
                rt_->SetDpi((float)dpi_, (float)dpi_);
                rt_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
                rt_->CreateSolidColorBrush(theme_.text, &brush_);
                LoadLogo();

                // rounded corners (Windows 11), dark title bar, taskbar icons
                using SetAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
                if (auto setAttribute = SystemFunction<SetAttributeFn>(L"dwmapi.dll", "DwmSetWindowAttribute"))
                {
                    int corners = 2; // DWMWCP_ROUND
                    setAttribute(hwnd_, 33, &corners, sizeof(corners));
                    BOOL dark = theme_.dark;
                    if (FAILED(setAttribute(hwnd_, 20, &dark, sizeof(dark)))) setAttribute(hwnd_, 19, &dark, sizeof(dark));
                }
                if ((taskbarIcon_ = ExecutableIcon()) != nullptr) SendMessageW(hwnd_, WM_SETICON, ICON_BIG, (LPARAM)taskbarIcon_);
                smallIcon_ = (HICON)LoadImageW(Self().module, MAKEINTRESOURCEW(kCustomIconId), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0);
                if (smallIcon_) SendMessageW(hwnd_, WM_SETICON, ICON_SMALL, (LPARAM)smallIcon_);

                CreateAutomationTexts();
                g_window = hwnd_;
                shownAt_ = lastFrame_ = GetTickCount64();
                UpdateFooterText();
                if (countdown)
                {
                    g_lastMouse = { -1, -1 };
                    g_mouseHook = SetWindowsHookExW(WH_MOUSE_LL, MouseHook, GetModuleHandleW(nullptr), 0);
                    SetTimer(hwnd_, kTimerFrame, 16, nullptr);
                }
                ShowWindow(hwnd_, SW_SHOW);
                SetForegroundWindow(hwnd_);
                SetFocus(hwnd_);
                return true;
            }

            void CreateAutomationTexts()
            {
                auto add = [&](const std::wstring& text, int id) {
                    automation_.push_back(CreateWindowExW(0, L"Static", text.c_str(), WS_CHILD | SS_NOPREFIX, 0, 0, 0, 0, hwnd_, (HMENU)(INT_PTR)id, nullptr, nullptr));
                };
                add(instruction, 1);
                add(content.text, 2);
                add(footer_.text, 3);
                for (size_t i = 0; i < choices.size(); ++i) add(L"&" + ChoiceCaptionText(choices[i]) + L"\n" + ChoiceDetailText(choices[i]), kFirstButton + (int)i);
                if (error) add(L"Continue", kFirstButton);
            }

            void ResizeToContent(const RECT* suggested)
            {
                HMONITOR monitor = MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi{ sizeof(mi) };
                GetMonitorInfoW(monitor, &mi);
                float s = dpi_ / 96.0f;
                ComputeLayout((mi.rcWork.bottom - mi.rcWork.top) * 0.9f / s);
                int w = (int)std::lround(kWidth * s), h = (int)std::lround(height_ * s);
                RECT now;
                GetWindowRect(hwnd_, &now);
                int x = suggested ? suggested->left : now.left, y = suggested ? suggested->top : now.top;
                SetWindowPos(hwnd_, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
                if (rt_)
                {
                    rt_->SetDpi((float)dpi_, (float)dpi_);
                    LoadLogo();
                }
            }

            void Loop()
            {
                MSG msg;
                while (!done_)
                {
                    BOOL r = GetMessageW(&msg, nullptr, 0, 0);
                    if (r == 0) // the game's WM_QUIT, repost it after closing
                    {
                        PostQuitMessage((int)msg.wParam);
                        break;
                    }
                    if (r < 0) break;
                    if (msg.hwnd == hwnd_ || IsChild(hwnd_, msg.hwnd))
                        switch (msg.message)
                        {
                        case WM_KEYDOWN:
                        case WM_SYSKEYDOWN:
                        case WM_LBUTTONDOWN:
                        case WM_RBUTTONDOWN:
                        case WM_MBUTTONDOWN:
                        case WM_NCLBUTTONDOWN:
                        case WM_NCRBUTTONDOWN:
                        case WM_MOUSEWHEEL: Activity(); break;
                        }
                    TranslateMessage(&msg);
                    DispatchMessageW(&msg);
                }
                if (g_mouseHook)
                {
                    UnhookWindowsHookEx(g_mouseHook);
                    g_mouseHook = nullptr;
                }
                g_window = nullptr;
                if (hwnd_) DestroyWindow(hwnd_);
                if (taskbarIcon_) DestroyIcon(taskbarIcon_);
                if (smallIcon_) DestroyIcon(smallIcon_);
            }

            void Finish(int value)
            {
                result = value;
                done_ = true;
                ShowWindow(hwnd_, SW_HIDE);
                PostMessageW(hwnd_, WM_NULL, 0, 0); // wake the loop
            }

            void Invalidate()
            {
                if (hwnd_) InvalidateRect(hwnd_, nullptr, FALSE);
            }

            // geometry

            D2D1_RECT_F MinimizeRect() const { return D2D1::RectF(kWidth - 2 * kWindowButtonWidth, 0, kWidth - kWindowButtonWidth, kTitleBarHeight); }
            D2D1_RECT_F CloseRect() const { return D2D1::RectF(kWidth - kWindowButtonWidth, 0, kWidth, kTitleBarHeight); }
            D2D1_RECT_F ListRect() const { return D2D1::RectF(kPad, listTop_, kWidth - kPad, listTop_ + listHeight_); }
            D2D1_RECT_F RowRect(const Row& r) const
            {
                float top = listTop_ + r.top - scroll_;
                return D2D1::RectF(kPad, top, kWidth - kPad - (listContent_ > listHeight_ ? 10.0f : 0.0f), top + r.height);
            }
            D2D1_RECT_F ButtonRect() const
            {
                float top = footerTop_ + (kFooterHeight - kButtonHeight) / 2;
                return D2D1::RectF(kWidth - kPad - kButtonWidth, top, kWidth - kPad, top + kButtonHeight);
            }
            D2D1_POINT_2F FooterTextOrigin() const { return D2D1::Point2F(kPad + 28.0f, footerTop_ + (kFooterHeight - TextHeight(lFooter_.Get())) / 2); }

            static bool Inside(const D2D1_RECT_F& r, D2D1_POINT_2F p) { return p.x >= r.left && p.x < r.right && p.y >= r.top && p.y < r.bottom; }

            // link index at p for a layout drawn at origin, or -1
            static int LinkAt(IDWriteTextLayout* l, const RichText& r, D2D1_POINT_2F origin, D2D1_POINT_2F p)
            {
                if (!l || r.links.empty()) return -1;
                BOOL trailing = FALSE, inside = FALSE;
                DWRITE_HIT_TEST_METRICS m{};
                l->HitTestPoint(p.x - origin.x, p.y - origin.y, &trailing, &inside, &m);
                if (!inside) return -1;
                for (size_t i = 0; i < r.links.size(); ++i)
                    if (m.textPosition >= r.links[i].start && m.textPosition < r.links[i].start + r.links[i].length) return (int)i;
                return -1;
            }

            Hit HitTest(D2D1_POINT_2F p) const
            {
                if (Inside(CloseRect(), p)) return { Part::Close };
                if (Inside(MinimizeRect(), p)) return { Part::Minimize };
                if (Inside(ListRect(), p))
                    for (size_t i = 0; i < rows_.size(); ++i)
                        if (Inside(RowRect(rows_[i]), p)) return { Part::Row, (int)i };
                if (int l = LinkAt(lContent_.Get(), content, D2D1::Point2F(headerTextX_, contentTop_), p); l >= 0) return { Part::ContentLink, l };
                if (error && Inside(ButtonRect(), p)) return { Part::Button };
                if (!error)
                    if (int l = LinkAt(lFooter_.Get(), footer_, FooterTextOrigin(), p); l >= 0) return { Part::FooterLink, l };
                return {};
            }

            D2D1_POINT_2F ToDip(LPARAM lp) const
            {
                float s = 96.0f / dpi_;
                return D2D1::Point2F(GET_X_LPARAM(lp) * s, GET_Y_LPARAM(lp) * s);
            }

            // drawing

            void Fill(const D2D1_RECT_F& r, D2D1_COLOR_F c)
            {
                brush_->SetColor(c);
                rt_->FillRectangle(r, brush_.Get());
            }

            void FillRounded(const D2D1_RECT_F& r, float radius, D2D1_COLOR_F c)
            {
                brush_->SetColor(c);
                rt_->FillRoundedRectangle(D2D1::RoundedRect(r, radius, radius), brush_.Get());
            }

            void StrokeRounded(const D2D1_RECT_F& r, float radius, D2D1_COLOR_F c, float width)
            {
                brush_->SetColor(c);
                float h = width / 2;
                rt_->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(r.left + h, r.top + h, r.right - h, r.bottom - h), radius, radius), brush_.Get(), width);
            }

            void Line(float x0, float y0, float x1, float y1, D2D1_COLOR_F c, float width)
            {
                brush_->SetColor(c);
                rt_->DrawLine(D2D1::Point2F(x0, y0), D2D1::Point2F(x1, y1), brush_.Get(), width);
            }

            void Text(IDWriteTextLayout* l, float x, float y, D2D1_COLOR_F c)
            {
                if (!l) return;
                brush_->SetColor(c);
                rt_->DrawTextLayout(D2D1::Point2F(x, y), l, brush_.Get(), D2D1_DRAW_TEXT_OPTIONS_NONE);
            }

            // links get their color through layout drawing effects
            void TextWithLinks(IDWriteTextLayout* l, const RichText& r, float x, float y, D2D1_COLOR_F text, D2D1_COLOR_F link)
            {
                if (!l) return;
                ComPtr<ID2D1SolidColorBrush> linkBrush;
                rt_->CreateSolidColorBrush(link, &linkBrush);
                for (const auto& k : r.links) l->SetDrawingEffect(linkBrush.Get(), { k.start, k.length });
                Text(l, x, y, text);
                for (const auto& k : r.links) l->SetDrawingEffect(nullptr, { k.start, k.length });
            }

            void FolderGlyph(float x, float y, D2D1_COLOR_F c, bool zip)
            {
                // tab and body of a folder, 22 x 17
                FillRounded(D2D1::RectF(x, y, x + 10, y + 6), 1.5f, c);
                FillRounded(D2D1::RectF(x, y + 3, x + 22, y + 17), 2.5f, c);
                if (zip) // a zipper down the middle
                    for (int i = 0; i < 3; ++i) Fill(D2D1::RectF(x + 10, y + 5 + i * 4, x + 12, y + 7 + i * 4), theme_.surface);
            }

            void BadgeGlyph(float cx, float cy, float radius, D2D1_COLOR_F fill, IDWriteTextLayout* glyph)
            {
                brush_->SetColor(fill);
                rt_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, cy), radius, radius), brush_.Get());
                glyph->SetMaxWidth(radius * 2);
                glyph->SetMaxHeight(radius * 2);
                Text(glyph, cx - radius, cy - radius - 0.5f, D2D1::ColorF(1, 1, 1));
            }

            void Paint()
            {
                if (!rt_) return;
                rt_->BeginDraw();
                rt_->Clear(theme_.background);

                // soft accent band behind the header
                ComPtr<ID2D1GradientStopCollection> stops;
                D2D1_GRADIENT_STOP gs[] = { { 0.0f, WithAlpha(theme_.accent, theme_.dark ? 0.20f : 0.13f) }, { 1.0f, WithAlpha(theme_.accent, 0.0f) } };
                rt_->CreateGradientStopCollection(gs, 2, &stops);
                ComPtr<ID2D1LinearGradientBrush> band;
                if (stops) rt_->CreateLinearGradientBrush(D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, 0), D2D1::Point2F(0, 160)), stops.Get(), &band);
                if (band) rt_->FillRectangle(D2D1::RectF(0, 0, kWidth, 160), band.Get());

                // title bar
                Text(lTitle_.Get(), 16, (kTitleBarHeight - TextHeight(lTitle_.Get())) / 2, theme_.textDim);
                Fill(MinimizeRect(), WithAlpha(theme_.text, 0.08f * hotMinimize_));
                Fill(CloseRect(), WithAlpha(theme_.closeHot, hotClose_));
                {
                    auto m = MinimizeRect();
                    float cx = (m.left + m.right) / 2, cy = kTitleBarHeight / 2;
                    Line(cx - 5, cy, cx + 5, cy, theme_.text, 1.0f);
                    auto c = CloseRect();
                    cx = (c.left + c.right) / 2;
                    auto glyph = Mix(theme_.text, D2D1::ColorF(1, 1, 1), hotClose_);
                    Line(cx - 5, cy - 5, cx + 5, cy + 5, glyph, 1.0f);
                    Line(cx - 5, cy + 5, cx + 5, cy - 5, glyph, 1.0f);
                }

                // header
                if (logo_) rt_->DrawBitmap(logo_.Get(), D2D1::RectF(kPad, instructionTop_, kPad + kLogoSize, instructionTop_ + kLogoSize));
                if (error) BadgeGlyph(kPad + kLogoSize - 4, instructionTop_ + kLogoSize - 4, 10, theme_.danger, lGlyphError_.Get());
                Text(lInstruction_.Get(), headerTextX_, instructionTop_, theme_.text);
                TextWithLinks(lContent_.Get(), content, headerTextX_, contentTop_, theme_.textDim, theme_.link);

                // choices
                if (!rows_.empty())
                {
                    rt_->PushAxisAlignedClip(Inflate(ListRect(), 2), D2D1_ANTIALIAS_MODE_ALIASED);
                    for (size_t i = 0; i < rows_.size(); ++i)
                    {
                        const Row& r = rows_[i];
                        auto rc = RowRect(r);
                        if (rc.bottom < listTop_ - 2 || rc.top > listTop_ + listHeight_ + 2) continue;
                        bool focused = (int)i == focus_;
                        bool down = pressed_.part == Part::Row && pressed_.index == (int)i && hover_ == pressed_;
                        D2D1_COLOR_F fill = Mix(theme_.surface, theme_.surfaceHot, (std::max)(r.hot, down ? 1.0f : 0.0f));
                        if (down) fill = Mix(fill, theme_.accent, 0.10f);
                        FillRounded(rc, kRowRadius, fill);
                        StrokeRounded(rc, kRowRadius, Mix(theme_.border, theme_.accent, r.hot * 0.6f), 1.0f);
                        if (focused && keyboard_) StrokeRounded(Inflate(rc, 2), kRowRadius + 2, theme_.text, 2.0f);
                        if (focused) FillRounded(D2D1::RectF(rc.left + 1, rc.top + 14, rc.left + 4, rc.bottom - 14), 1.5f, theme_.accent);

                        float textX = rc.left + kRowIconWidth;
                        float captionY = rc.top + 12;
                        FolderGlyph(rc.left + 18, rc.top + (r.height - 17) / 2, Mix(theme_.textDim, theme_.accent, (std::max)(r.hot, focused ? 1.0f : 0.0f)), choices[i].zip);
                        Text(r.caption.Get(), textX, captionY, theme_.text);
                        Text(r.path.Get(), textX, captionY + TextHeight(r.caption.Get()) + 3, theme_.textDim);
                        float pillX = rc.right - 14;
                        for (auto it = r.pills.rbegin(); it != r.pills.rend(); ++it)
                        {
                            pillX -= it->width;
                            auto pill = D2D1::RectF(pillX, captionY + 1, pillX + it->width, captionY + 21);
                            FillRounded(pill, 10, WithAlpha(theme_.accent, theme_.dark ? 0.28f : 0.14f));
                            Text(it->text.Get(), pill.left, pill.top, theme_.dark ? theme_.text : theme_.accent);
                            pillX -= 6;
                        }
                    }
                    rt_->PopAxisAlignedClip();
                    if (listContent_ > listHeight_) // scrollbar
                    {
                        float x = kWidth - kPad - 3;
                        float thumb = (std::max)(24.0f, listHeight_ * listHeight_ / listContent_);
                        float y = listTop_ + (listHeight_ - thumb) * scroll_ / (listContent_ - listHeight_);
                        FillRounded(D2D1::RectF(x, listTop_, x + 3, listTop_ + listHeight_), 1.5f, WithAlpha(theme_.text, 0.06f));
                        FillRounded(D2D1::RectF(x, y, x + 3, y + thumb), 1.5f, WithAlpha(theme_.text, 0.35f));
                    }
                }

                // countdown
                if (countdown)
                {
                    float total = kCountdownSeconds * 1000.0f;
                    float left = stopped_ ? 0.0f : (std::max)(0.0f, 1.0f - (GetTickCount64() - shownAt_) / total);
                    auto track = D2D1::RectF(kPad, barTop_, kWidth - kPad, barTop_ + 4);
                    FillRounded(track, 2, WithAlpha(theme_.text, 0.08f));
                    if (left > 0) FillRounded(D2D1::RectF(track.left, track.top, track.left + (track.right - track.left) * left, track.bottom), 2, theme_.accent);
                }

                // footer
                Fill(D2D1::RectF(0, footerTop_, kWidth, footerTop_ + kFooterHeight), theme_.footer);
                Line(0, footerTop_ + 0.5f, kWidth, footerTop_ + 0.5f, theme_.border, 1.0f);
                if (error)
                {
                    auto b = ButtonRect();
                    FillRounded(b, 6, Mix(theme_.accent, theme_.accentHot, hotButton_));
                    if (keyboard_) StrokeRounded(Inflate(b, 2), 8, theme_.text, 2.0f);
                    Text(lButton_.Get(), b.left, b.top, theme_.accentText);
                }
                else
                {
                    float cy = footerTop_ + kFooterHeight / 2;
                    BadgeGlyph(kPad + 8, cy, 8, theme_.textDim, lGlyphInfo_.Get());
                    auto o = FooterTextOrigin();
                    if (hotFooterLink_ > 0.5f && !footer_.links.empty()) lFooter_->SetUnderline(TRUE, { 0, (UINT32)footer_.text.size() });
                    TextWithLinks(lFooter_.Get(), footer_, o.x, o.y, theme_.textDim, theme_.link);
                }

                // hairline border, Windows 10 draws none
                StrokeRounded(D2D1::RectF(0, 0, kWidth, height_), 0, WithAlpha(theme_.text, 0.12f), 1.0f);

                if (rt_->EndDraw() == D2DERR_RECREATE_TARGET)
                {
                    rt_.Reset();
                    brush_.Reset();
                    logo_.Reset();
                    RecreateTarget();
                }
            }

            static D2D1_RECT_F Inflate(D2D1_RECT_F r, float d) { return D2D1::RectF(r.left - d, r.top - d, r.right + d, r.bottom + d); }

            void RecreateTarget()
            {
                RECT rc;
                GetClientRect(hwnd_, &rc);
                D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
                                                                                   D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
                if (FAILED(f_.d2d->CreateHwndRenderTarget(props, D2D1::HwndRenderTargetProperties(hwnd_, D2D1::SizeU(rc.right, rc.bottom)), &rt_))) return;
                rt_->SetDpi((float)dpi_, (float)dpi_);
                rt_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
                rt_->CreateSolidColorBrush(theme_.text, &brush_);
                LoadLogo();
                Invalidate();
            }

            // animation

            // true while something still moves
            bool Animate()
            {
                ULONGLONG now = GetTickCount64();
                float step = (std::min)(1.0f, (now - lastFrame_) / kHoverMs);
                lastFrame_ = now;
                bool moving = false;
                auto approach = [&](float& v, bool on) {
                    float target = on ? 1.0f : 0.0f;
                    if (v == target) return;
                    v = v < target ? (std::min)(target, v + step) : (std::max)(target, v - step);
                    moving = true;
                };
                for (size_t i = 0; i < rows_.size(); ++i) approach(rows_[i].hot, hover_.part == Part::Row && hover_.index == (int)i);
                approach(hotMinimize_, hover_.part == Part::Minimize);
                approach(hotClose_, hover_.part == Part::Close);
                approach(hotButton_, hover_.part == Part::Button);
                approach(hotFooterLink_, hover_.part == Part::FooterLink);
                return moving;
            }

            void Tick()
            {
                bool moving = Animate();
                if (countdown && !stopped_)
                {
                    if (GetTickCount64() - shownAt_ >= (ULONGLONG)kCountdownSeconds * 1000)
                    {
                        stopped_ = true;
                        Finish(0); // first folder
                        return;
                    }
                    int second = (int)((GetTickCount64() - shownAt_) / 1000);
                    if (second != lastSecond_)
                    {
                        lastSecond_ = second;
                        UpdateFooterText();
                    }
                    moving = true;
                }
                Invalidate();
                if (!moving) KillTimer(hwnd_, kTimerFrame);
            }

            void StartAnimation()
            {
                lastFrame_ = GetTickCount64();
                SetTimer(hwnd_, kTimerFrame, 16, nullptr);
            }

            void SetHover(Hit h)
            {
                if (h == hover_) return;
                hover_ = h;
                StartAnimation();
                SetCursor(LoadCursorW(nullptr, (h.part == Part::ContentLink || h.part == Part::FooterLink || h.part == Part::Row || h.part == Part::Button)
                                                   ? IDC_HAND
                                                   : IDC_ARROW));
            }

            // input

            void Activate(Hit h)
            {
                switch (h.part)
                {
                case Part::Close: Finish(error ? 0 : -1); break;
                case Part::Minimize: ShowWindow(hwnd_, SW_MINIMIZE); break;
                case Part::Row: Finish(h.index); break;
                case Part::Button: Finish(0); break;
                case Part::ContentLink: OpenUrl(hwnd_, content.links[h.index].url.c_str()); break;
                case Part::FooterLink: OpenUrl(hwnd_, footer_.links[h.index].url.c_str()); break;
                default: break;
                }
            }

            void EnsureVisible(int row)
            {
                if (row < 0 || row >= (int)rows_.size()) return;
                const Row& r = rows_[row];
                if (r.top < scroll_) scroll_ = r.top;
                else if (r.top + r.height > scroll_ + listHeight_) scroll_ = r.top + r.height - listHeight_;
                ClampScroll();
            }

            void Key(WPARAM key)
            {
                keyboard_ = true;
                int count = (int)rows_.size();
                switch (key)
                {
                case VK_ESCAPE: Finish(error ? 0 : -1); return;
                case VK_RETURN:
                case VK_SPACE: Finish(error ? 0 : focus_); return;
                case VK_UP:
                case VK_LEFT:
                    if (count) focus_ = (focus_ + count - 1) % count;
                    break;
                case VK_DOWN:
                case VK_RIGHT:
                case VK_TAB:
                    if (count) focus_ = (focus_ + (key == VK_TAB && GetKeyState(VK_SHIFT) < 0 ? count - 1 : 1)) % count;
                    break;
                case VK_HOME: focus_ = 0; break;
                case VK_END: focus_ = (std::max)(0, count - 1); break;
                default:
                    if (key >= '1' && key <= '9' && (int)(key - '1') < count) return Finish((int)(key - '1'));
                    if (key >= VK_NUMPAD1 && key <= VK_NUMPAD9 && (int)(key - VK_NUMPAD1) < count) return Finish((int)(key - VK_NUMPAD1));
                    return;
                }
                EnsureVisible(focus_);
                Invalidate();
            }

            static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
            {
                Dialog* d = (Dialog*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);
                if (msg == WM_NCCREATE)
                {
                    d = (Dialog*)((CREATESTRUCTW*)lp)->lpCreateParams;
                    d->hwnd_ = hwnd;
                    SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)d);
                }
                if (!d) return DefWindowProcW(hwnd, msg, wp, lp);
                switch (msg)
                {
                case WM_PAINT:
                {
                    PAINTSTRUCT ps;
                    BeginPaint(hwnd, &ps);
                    d->Paint();
                    EndPaint(hwnd, &ps);
                    return 0;
                }
                case WM_ERASEBKGND: return 1;
                case WM_SIZE:
                    if (d->rt_) d->rt_->Resize(D2D1::SizeU(LOWORD(lp), HIWORD(lp)));
                    d->Invalidate();
                    return 0;
                case WM_DPICHANGED:
                    d->dpi_ = HIWORD(wp);
                    d->ResizeToContent((RECT*)lp);
                    d->Invalidate();
                    return 0;
                case WM_NCHITTEST:
                {
                    LRESULT hit = DefWindowProcW(hwnd, msg, wp, lp);
                    if (hit != HTCLIENT) return hit;
                    POINT pt{ GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                    ScreenToClient(hwnd, &pt);
                    float s = 96.0f / d->dpi_;
                    return d->HitTest(D2D1::Point2F(pt.x * s, pt.y * s)).part == Part::None ? HTCAPTION : HTCLIENT; // drag by the background
                }
                case WM_NCLBUTTONDBLCLK: return 0; // no maximizing
                case WM_NCMOUSEMOVE:
                    d->SetHover({});
                    break;
                case WM_MOUSEMOVE:
                {
                    TRACKMOUSEEVENT tme{ sizeof(tme), TME_LEAVE, hwnd, 0 };
                    TrackMouseEvent(&tme);
                    d->SetHover(d->HitTest(d->ToDip(lp)));
                    return 0;
                }
                case WM_MOUSELEAVE:
                    d->SetHover({});
                    return 0;
                case WM_SETCURSOR:
                    if (LOWORD(lp) == HTCLIENT)
                    {
                        auto p = d->hover_.part;
                        SetCursor(LoadCursorW(nullptr, (p == Part::ContentLink || p == Part::FooterLink || p == Part::Row || p == Part::Button) ? IDC_HAND : IDC_ARROW));
                        return TRUE;
                    }
                    break;
                case WM_LBUTTONDOWN:
                    d->keyboard_ = false;
                    d->pressed_ = d->HitTest(d->ToDip(lp));
                    if (d->pressed_.part == Part::Row) d->focus_ = d->pressed_.index;
                    SetCapture(hwnd);
                    d->Invalidate();
                    return 0;
                case WM_LBUTTONUP:
                {
                    ReleaseCapture();
                    Hit h = d->HitTest(d->ToDip(lp));
                    Hit pressed = d->pressed_;
                    d->pressed_ = {};
                    if (h == pressed) d->Activate(h);
                    d->Invalidate();
                    return 0;
                }
                case WM_MOUSEWHEEL:
                    d->scroll_ -= GET_WHEEL_DELTA_WPARAM(wp) / (float)WHEEL_DELTA * 60.0f;
                    d->ClampScroll();
                    d->Invalidate();
                    return 0;
                case WM_GETDLGCODE: return DLGC_WANTALLKEYS;
                case WM_KEYDOWN:
                    d->Key(wp);
                    return 0;
                case WM_SYSCOMMAND:
                    if ((wp & 0xFFF0) == SC_CLOSE)
                    {
                        d->Finish(d->error ? 0 : -1);
                        return 0;
                    }
                    break;
                case WM_CLOSE: d->Finish(d->error ? 0 : -1); return 0;
                case WM_TIMER:
                    if (wp == kTimerFrame) d->Tick();
                    return 0;
                case TDM_CLICK_BUTTON: // automation, same ids as the task dialog
                {
                    int id = (int)wp;
                    if (d->error) d->Finish(0);
                    else if (id >= kFirstButton && id < kFirstButton + (int)d->choices.size()) d->Finish(id - kFirstButton);
                    return 0;
                }
                case WM_SETTINGCHANGE:
                case WM_THEMECHANGED:
                    d->theme_ = Theme::Make();
                    d->Invalidate();
                    break;
                }
                return DefWindowProcW(hwnd, msg, wp, lp);
            }
        };

        Dialog* g_current = nullptr;

        void NotifyActivity()
        {
            if (g_current) g_current->Activity();
        }

        bool Usable()
        {
            return !LoaderLockHeldByThisThread() && GetFactories() != nullptr;
        }
    }

    bool ShowError(const std::wstring& header, const std::wstring& content)
    {
        if (!Usable()) return false;
        Dialog d(*GetFactories());
        d.error = true;
        d.instruction = header;
        d.content = RichText::Parse(content);
        g_current = &d;
        bool shown = d.Run();
        g_current = nullptr;
        return shown;
    }

    bool ChooseOverloadFolder(const std::vector<Choice>& choices, int& result)
    {
        if (!Usable()) return false;
        Dialog d(*GetFactories());
        d.instruction = kChooseInstruction;
        d.content = RichText::Parse(kChooseContent);
        d.choices = choices;
        d.countdown = true;
        g_current = &d;
        bool shown = d.Run();
        g_current = nullptr;
        if (!shown) return false;
        result = d.result;
        return true;
    }
}
