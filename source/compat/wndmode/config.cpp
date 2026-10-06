#include "internal.hpp"
#include <cstdarg>
#include <cstdio>
#include <map>

namespace wndmode
{
    Config g_cfg;

    void Log(const char* fmt, ...)
    {
        char buf[1024] = "[wndmode] ";
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf + 10, sizeof(buf) - 12, fmt, ap);
        va_end(ap);
        strcat_s(buf, "\n");
        OutputDebugStringA(buf);
    }

    namespace
    {
        using Section = std::map<std::wstring, std::wstring>;
        using Ini = std::map<std::wstring, Section>;

        std::wstring Lower(std::wstring s)
        {
            if (!s.empty()) CharLowerBuffW(s.data(), (DWORD)s.size());
            return s;
        }

        std::wstring Trim(const std::wstring& s)
        {
            auto b = s.find_first_not_of(L" \t\r\n");
            if (b == std::wstring::npos) return {};
            auto e = s.find_last_not_of(L" \t\r\n");
            return s.substr(b, e - b + 1);
        }

        std::wstring Decode(const std::string& text)
        {
            UINT cp = CP_ACP;
            size_t skip = 0;
            if (text.size() >= 3 && (unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF)
                cp = CP_UTF8, skip = 3;
            int n = MultiByteToWideChar(cp, 0, text.data() + skip, (int)(text.size() - skip), nullptr, 0);
            std::wstring w(n, L'\0');
            MultiByteToWideChar(cp, 0, text.data() + skip, (int)(text.size() - skip), w.data(), n);
            return w;
        }

        Ini Parse(const std::wstring& text)
        {
            Ini ini;
            Section* cur = nullptr;
            size_t pos = 0;
            while (pos <= text.size())
            {
                auto end = text.find(L'\n', pos);
                if (end == std::wstring::npos) end = text.size();
                auto line = Trim(text.substr(pos, end - pos));
                pos = end + 1;
                if (line.empty() || line[0] == L';' || line[0] == L'#') continue;
                if (line[0] == L'[')
                {
                    auto close = line.find(L']');
                    cur = &ini[Lower(Trim(line.substr(1, close == std::wstring::npos ? std::wstring::npos : close - 1)))];
                    continue;
                }
                auto eq = line.find(L'=');
                if (!cur || eq == std::wstring::npos) continue;
                auto key = Lower(Trim(line.substr(0, eq)));
                if (!cur->count(key)) // first occurrence wins, like TMemIniFile
                    (*cur)[key] = Trim(line.substr(eq + 1));
            }
            return ini;
        }

        // VCL StrToIntDef: decimal, or hexadecimal with a '$' (or 0x) prefix
        int ToInt(const std::wstring& s, int def)
        {
            if (s.empty()) return def;
            const wchar_t* p = s.c_str();
            bool neg = false;
            if (*p == L'-' || *p == L'+') neg = *p++ == L'-';
            int base = 10;
            if (*p == L'$') base = 16, ++p;
            else if (p[0] == L'0' && (p[1] == L'x' || p[1] == L'X')) base = 16, p += 2;
            if (!*p) return def;
            wchar_t* end = nullptr;
            unsigned long v = wcstoul(p, &end, base);
            if (*end) return def;
            return neg ? -(int)v : (int)v;
        }
    }

    Config ParseConfig(const std::string& text, const std::wstring& exePath)
    {
        Ini ini = Parse(Decode(text));
        std::wstring suffix;
        if (auto prof = ini.find(L"wndmode.ini"); prof != ini.end())
            if (auto it = prof->second.find(Lower(exePath)); it != prof->second.end())
                suffix = it->second;

        Config c;
        static const Section empty;
        auto sec = ini.find(Lower(L"WINDOWMODE" + suffix));
        const Section& s = sec != ini.end() ? sec->second : empty;
        auto str = [&](const wchar_t* key) -> std::wstring {
            auto it = s.find(Lower(key));
            return it == s.end() ? std::wstring() : it->second;
        };
        auto i = [&](const wchar_t* key, int def) { return ToInt(str(key), def); };
        auto b = [&](const wchar_t* key, bool def) { return i(key, def ? 1 : 0) != 0; };

        c.UseWindowMode = b(L"UseWindowMode", c.UseWindowMode);
        c.UseGDI = b(L"UseGDI", c.UseGDI);
        c.UseDirect3D = b(L"UseDirect3D", c.UseDirect3D);
        c.UseDirectInput = b(L"UseDirectInput", c.UseDirectInput);
        c.UseDirectDraw = b(L"UseDirectDraw", c.UseDirectDraw);
        c.UseDDrawEmulate = b(L"UseDDrawEmulate", c.UseDDrawEmulate);
        c.UseDDrawFlipBlt = b(L"UseDDrawFlipBlt", c.UseDDrawFlipBlt);
        c.UseDDrawColorConvert = b(L"UseDDrawColorConvert", c.UseDDrawColorConvert);
        c.UseDDrawPrimaryBlt = b(L"UseDDrawPrimaryBlt", c.UseDDrawPrimaryBlt);
        c.UseDDrawPrimaryLost = b(L"UseDDrawPrimaryLost", c.UseDDrawPrimaryLost);
        c.UseDDrawAutoBlt = b(L"UseDDrawAutoBlt", c.UseDDrawAutoBlt);
        c.DDrawBltWait = i(L"DDrawBltWait", c.DDrawBltWait);
        c.UseDDrawColorEmulate = b(L"UseDDrawColorEmulate", c.UseDDrawColorEmulate);
        c.UseForegroundControl = b(L"UseForegroundControl", c.UseForegroundControl);
        c.UseFGCGetForegroundWindow = b(L"UseFGCGetForegroundWindow", c.UseFGCGetForegroundWindow);
        c.UseFGCGetActiveWindow = b(L"UseFGCGetActiveWindow", c.UseFGCGetActiveWindow);
        c.UseFGCFixedWindowPosition = b(L"UseFGCFixedWindowPosition", c.UseFGCFixedWindowPosition);
        c.EnableExtraKey = b(L"EnableExtraKey", c.EnableExtraKey);
        c.UseCursorMsg = b(L"UseCursorMsg", c.UseCursorMsg);
        c.UseCursorSet = b(L"UseCursorSet", c.UseCursorSet);
        c.UseCursorGet = b(L"UseCursorGet", c.UseCursorGet);
        c.UseCursorClip = b(L"UseCursorClip", c.UseCursorClip);
        c.UseSpeedHack = b(L"UseSpeedHack", c.UseSpeedHack);
        c.SpeedHackMultiple = i(L"SpeedHackMultiple", c.SpeedHackMultiple);
        if (c.SpeedHackMultiple <= 0) c.SpeedHackMultiple = 10;
        c.UseBackgroundPriority = b(L"UseBackgroundPriority", c.UseBackgroundPriority);
        c.UseBackgroundResize = b(L"UseBackgroundResize", c.UseBackgroundResize);
        c.ShowFps = b(L"ShowFps", c.ShowFps);
        c.Border = b(L"Border", c.Border);
        c.UseFakeScreenMetrics = b(L"UseFakeScreenMetrics", c.UseFakeScreenMetrics);
        c.DpiAware = i(L"DpiAware", c.DpiAware);
        auto menu = str(L"MenuId");
        if (!menu.empty())
        {
            int n = WideCharToMultiByte(CP_ACP, 0, menu.c_str(), (int)menu.size(), nullptr, 0, nullptr, nullptr);
            c.MenuId.resize(n);
            WideCharToMultiByte(CP_ACP, 0, menu.c_str(), (int)menu.size(), c.MenuId.data(), n, nullptr, nullptr);
        }
        for (int n = 0;; ++n)
        {
            auto v = str((L"SubModule" + std::to_wstring(n)).c_str());
            if (v.empty()) break;
            c.SubModules.push_back(v);
        }
        return c;
    }

    bool LoadConfig(const std::wstring& iniPath, const std::wstring& exePath, Config& out)
    {
        HANDLE f = CreateFileW(iniPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (f == INVALID_HANDLE_VALUE) return false;
        std::string text;
        LARGE_INTEGER size{};
        if (GetFileSizeEx(f, &size) && size.QuadPart < (1 << 20))
        {
            text.resize((size_t)size.QuadPart);
            DWORD read = 0;
            if (!ReadFile(f, text.data(), (DWORD)text.size(), &read, nullptr)) read = 0;
            text.resize(read);
        }
        CloseHandle(f);
        out = ParseConfig(text, exePath);
        return true;
    }
}
