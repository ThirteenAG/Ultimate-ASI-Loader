// Replaces game files without touching the disk, using AddVirtualPathForOverloadW and
// AddVirtualFileForOverloadW with entries from VirtualFiles.ini:
//   [Paths]   game file = replacement file
//   [Text]    game file = text to serve as its content
// The game sees replacements in every file function (open, attributes, listings), like update
// folder files, and plugin entries take precedence over the update folder. The same API lets
// you generate files at run time, such as a patched config, and serve them from memory.
#include <windows.h>
#include <psapi.h>
#include <cstdint>
#include <mutex>
#include <stacktrace>
#include <string>
#include <vector>

namespace
{
    // Pass absolute paths, relative ones resolve against the current directory.
    // For the same path, the plugin with the highest priority wins.
    using AddVirtualPathForOverloadW_t = bool(WINAPI*)(const wchar_t* original, const wchar_t* target, int priority);
    using AddVirtualFileForOverloadW_t = bool(WINAPI*)(const wchar_t* path, const uint8_t* data, size_t size, int priority);
    constexpr int kPriority = 0;

    HMODULE ThisModule()
    {
        HMODULE m = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&ThisModule, &m);
        return m;
    }

    std::wstring ModulePath(HMODULE m)
    {
        wchar_t path[MAX_PATH * 4];
        return std::wstring(path, GetModuleFileNameW(m, path, (DWORD)std::size(path)));
    }

    // VirtualFiles.asi -> VirtualFiles.ini
    std::wstring IniPath()
    {
        auto path = ModulePath(ThisModule());
        return path.substr(0, path.find_last_of(L'.')) + L".ini";
    }

    // relative paths are taken from the game folder
    std::wstring GamePath(const std::wstring& path)
    {
        if (path.size() > 1 && (path[1] == L':' || (path[0] == L'\\' && path[1] == L'\\'))) return path;
        auto exe = ModulePath(nullptr);
        return exe.substr(0, exe.find_last_of(L'\\') + 1) + path;
    }

    // The loader may be named dinput8.dll, version.dll, etc., so find it by its export.
    HMODULE Loader()
    {
        HMODULE modules[1024];
        DWORD needed = 0;
        K32EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &needed);
        for (DWORD i = 0; i < needed / sizeof(HMODULE) && i < 1024; ++i)
            if (GetProcAddress(modules[i], "IsUltimateASILoader")) return modules[i];
        return nullptr;
    }

    // "key = value" pairs of an ini section, surrounding quotes removed
    std::vector<std::pair<std::wstring, std::wstring>> Section(const wchar_t* name)
    {
        std::wstring buffer(32768, L'\0');
        DWORD n = GetPrivateProfileSectionW(name, buffer.data(), (DWORD)buffer.size(), IniPath().c_str());
        auto trim = [](std::wstring s) {
            auto b = s.find_first_not_of(L" \t"), e = s.find_last_not_of(L" \t");
            s = b == std::wstring::npos ? std::wstring() : s.substr(b, e - b + 1);
            if (s.size() >= 2 && s.front() == L'"' && s.back() == L'"') s = s.substr(1, s.size() - 2);
            return s;
        };
        std::vector<std::pair<std::wstring, std::wstring>> entries;
        for (const wchar_t* line = buffer.c_str(); line < buffer.c_str() + n && *line; line += wcslen(line) + 1)
        {
            std::wstring text = line;
            auto eq = text.find(L'=');
            if (text[0] != L';' && eq != std::wstring::npos) entries.emplace_back(trim(text.substr(0, eq)), trim(text.substr(eq + 1)));
        }
        return entries;
    }

    // expands \n, \t and \\, returns UTF-8
    std::string Utf8Text(const std::wstring& value)
    {
        std::wstring text;
        for (size_t i = 0; i < value.size(); ++i)
        {
            wchar_t next = i + 1 < value.size() ? value[i + 1] : 0;
            if (value[i] == L'\\' && (next == L'n' || next == L't' || next == L'\\'))
            {
                text += next == L'n' ? L"\r\n" : next == L't' ? L"\t" : L"\\";
                ++i;
            }
            else
                text += value[i];
        }
        int n = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), nullptr, 0, nullptr, nullptr);
        std::string utf8(n, '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), (int)text.size(), utf8.data(), n, nullptr, nullptr);
        return utf8;
    }

    void Init()
    {
        HMODULE loader = Loader();
        auto addPath = loader ? (AddVirtualPathForOverloadW_t)GetProcAddress(loader, "AddVirtualPathForOverloadW") : nullptr;
        auto addFile = loader ? (AddVirtualFileForOverloadW_t)GetProcAddress(loader, "AddVirtualFileForOverloadW") : nullptr;
        if (!addPath || !addFile)
        {
            OutputDebugStringW(L"VirtualFiles: needs Ultimate ASI Loader\n");
            return;
        }
        for (const auto& [original, replacement] : Section(L"Paths"))
            if (!addPath(GamePath(original).c_str(), GamePath(replacement).c_str(), kPriority))
                OutputDebugStringW((L"VirtualFiles: cannot redirect " + original + L"\n").c_str());
        for (const auto& [path, value] : Section(L"Text"))
        {
            auto data = Utf8Text(value); // the loader keeps its own copy
            if (data.empty() || !addFile(GamePath(path).c_str(), (const uint8_t*)data.data(), data.size(), kPriority))
                OutputDebugStringW((L"VirtualFiles: cannot add " + path + L"\n").c_str());
        }
    }

    void InitOnce()
    {
        static std::once_flag once;
        std::call_once(once, Init);
    }

    // Ultimate ASI Loader is on the call stack while it loads the plugin.
    bool LoadedByUltimateASILoader()
    {
        for (const auto& frame : std::stacktrace::current())
        {
            HMODULE m = nullptr;
            if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)frame.native_handle(), &m) &&
                GetProcAddress(m, "IsUltimateASILoader"))
                return true;
        }
        return false;
    }
}

// Called by Ultimate ASI Loader right after loading the plugin, outside the loader lock.
extern "C" __declspec(dllexport) void InitializeASI()
{
    InitOnce();
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID)
{
    // Other ASI loaders only load the DLL, so start here. Reading the ini and calling the loader's
    // functions is safe under the loader lock. Without Ultimate ASI Loader, Init does nothing.
    if (reason == DLL_PROCESS_ATTACH && !LoadedByUltimateASILoader()) InitOnce();
    return TRUE;
}
