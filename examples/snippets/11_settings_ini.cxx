// 11 - Settings: reading an .ini file next to the snippet
//
// Use for: letting users change values without editing the code.
//
// __FILE__ is the full path of the snippet, so "MyFix.cxx" can read
// "MyFix.ini" from the same folder. The Windows ini functions do the
// parsing; declare the ones you need (they are in kernel32).
//
// MyFix.ini:
//     [MAIN]
//     Width = 2560
//     Height = 1080
//     FOVFactor = 1.25
//     SkipIntro = 1
#include <windows.h>
#include <cstdint>
#include <cstring>

#ifdef __CXXSNIPPETS__ // already in the real <windows.h>
extern "C" {
UINT WINAPI GetPrivateProfileIntW(LPCWSTR section, LPCWSTR key, int fallback, LPCWSTR path);
DWORD WINAPI GetPrivateProfileStringW(LPCWSTR section, LPCWSTR key, LPCWSTR fallback, LPWSTR out, DWORD size, LPCWSTR path);
int WINAPI MultiByteToWideChar(UINT codePage, DWORD flags, char const *text, int length, wchar_t *out, int size);
}
#define CP_UTF8 65001
#endif

wchar_t iniPath[MAX_PATH];

int width = 0;
int height = 0;
float fovFactor = 1.0f;
bool skipIntro = false;

// "<this file without .cxx>.ini". The path is UTF-8; Windows wants UTF-16.
void FindIniPath()
{
    char path[MAX_PATH];
    size_t length = strlen(__FILE__);
    if (length + 1 > sizeof(path))
        return;
    memcpy(path, __FILE__, length + 1);
    for (size_t i = length; i > 0; --i)
    {
        if (path[i - 1] == '.')
        {
            path[i - 1] = 0;
            break;
        }
    }
    size_t base = strlen(path);
    if (base + 5 > sizeof(path))
        return;
    memcpy(path + base, ".ini", 5);
    MultiByteToWideChar(CP_UTF8, 0, path, -1, iniPath, MAX_PATH);
}

// "1.25" -> 1.25f: there is no atof, but this is enough for ini values.
float ParseFloat(wchar_t const *text, float fallback)
{
    float value = 0.0f, sign = 1.0f, scale = 0.0f;
    bool any = false;
    if (*text == L'-')
    {
        sign = -1.0f;
        ++text;
    }
    for (; *text; ++text)
    {
        if (*text >= L'0' && *text <= L'9')
        {
            any = true;
            if (scale == 0.0f)
                value = value * 10.0f + (float)(*text - L'0');
            else
            {
                value += (float)(*text - L'0') * scale;
                scale *= 0.1f;
            }
        }
        else if (*text == L'.' && scale == 0.0f)
            scale = 0.1f;
        else
            break;
    }
    return any ? value * sign : fallback;
}

float ReadFloat(wchar_t const *section, wchar_t const *key, float fallback)
{
    wchar_t text[64];
    GetPrivateProfileStringW(section, key, L"", text, 64, iniPath);
    return ParseFloat(text, fallback);
}

void Init()
{
    FindIniPath();
    width = (int)GetPrivateProfileIntW(L"MAIN", L"Width", 0, iniPath);
    height = (int)GetPrivateProfileIntW(L"MAIN", L"Height", 0, iniPath);
    fovFactor = ReadFloat(L"MAIN", L"FOVFactor", 1.0f);
    skipIntro = GetPrivateProfileIntW(L"MAIN", L"SkipIntro", 1, iniPath) != 0;

    // 0 = use the desktop resolution
    if (width == 0 || height == 0)
    {
        // ... see 06_dll_and_winapi_hooks.cxx for GetSystemMetrics
    }
}
