// Starting point for a real plugin, works in any game. Shows an ini and a log next to the .asi,
// the loader's API, safetyhook inline hooks (adds a suffix to the window title) and a
// Hooking.Patterns byte search. Rename the project and files, delete what you don't need.
#include <windows.h>
#include <psapi.h>
#include <cstdio>
#include <mutex>
#include <stacktrace>
#include <string>
#include <safetyhook.hpp>
#include <Hooking.Patterns.h>

namespace
{
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

    // PluginTemplate.asi -> PluginTemplate<extension>
    std::wstring SiblingPath(const wchar_t* extension)
    {
        auto path = ModulePath(ThisModule());
        return path.substr(0, path.find_last_of(L'.')) + extension;
    }

    struct Settings
    {
        std::wstring titleSuffix;
        std::string pattern; // e.g. "74 ? 8B 4D 08", matches go to the log
        bool log = true;
    } g_settings;

    void LoadSettings()
    {
        auto ini = SiblingPath(L".ini");
        wchar_t value[512];
        GetPrivateProfileStringW(L"MAIN", L"TitleSuffix", L" [PluginTemplate]", value, (DWORD)std::size(value), ini.c_str());
        g_settings.titleSuffix = value;
        GetPrivateProfileStringW(L"MAIN", L"Pattern", L"", value, (DWORD)std::size(value), ini.c_str());
        for (const wchar_t* c = value; *c; ++c) g_settings.pattern += *c < 0x80 ? (char)*c : '?'; // hex digits, '?' and spaces
        g_settings.log = GetPrivateProfileIntW(L"MAIN", L"Log", 1, ini.c_str()) != 0;
    }

    // PluginTemplate.log, rewritten on every start
    void Log(const char* format, ...)
    {
        static FILE* file = [] {
            FILE* f = nullptr;
            return g_settings.log && _wfopen_s(&f, SiblingPath(L".log").c_str(), L"w") == 0 ? f : nullptr;
        }();
        if (!file) return;
        va_list args;
        va_start(args, format);
        vfprintf(file, format, args);
        va_end(args);
        fputc('\n', file);
        fflush(file);
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

    // One hook object per function. Its call/stdcall/... methods call the original.
    SafetyHookInline g_createWindowExW{}, g_createWindowExA{}, g_setWindowTextW{}, g_setWindowTextA{};

    bool IsMainWindow(HWND parent, DWORD style)
    {
        return !parent && !(style & WS_CHILD);
    }

    std::wstring WithSuffix(const wchar_t* title)
    {
        std::wstring text = title ? title : L"";
        if (!text.ends_with(g_settings.titleSuffix)) text += g_settings.titleSuffix;
        return text;
    }

    std::string WithSuffix(const char* title) // system code page
    {
        wchar_t wide[1024] = {};
        MultiByteToWideChar(CP_ACP, 0, title ? title : "", -1, wide, (int)std::size(wide) - 1);
        auto text = WithSuffix(wide);
        char narrow[1024] = {};
        WideCharToMultiByte(CP_ACP, 0, text.c_str(), -1, narrow, (int)std::size(narrow) - 1, nullptr, nullptr);
        return narrow;
    }

    // Detours must match the original's signature and calling convention exactly.
    HWND WINAPI CreateWindowExWHook(DWORD exStyle, LPCWSTR cls, LPCWSTR title, DWORD style, int x, int y, int w, int h, HWND parent, HMENU menu,
                                    HINSTANCE inst, LPVOID param)
    {
        if (IsMainWindow(parent, style) && title)
            return g_createWindowExW.stdcall<HWND>(exStyle, cls, WithSuffix(title).c_str(), style, x, y, w, h, parent, menu, inst, param);
        return g_createWindowExW.stdcall<HWND>(exStyle, cls, title, style, x, y, w, h, parent, menu, inst, param);
    }

    HWND WINAPI CreateWindowExAHook(DWORD exStyle, LPCSTR cls, LPCSTR title, DWORD style, int x, int y, int w, int h, HWND parent, HMENU menu,
                                    HINSTANCE inst, LPVOID param)
    {
        if (IsMainWindow(parent, style) && title)
            return g_createWindowExA.stdcall<HWND>(exStyle, cls, WithSuffix(title).c_str(), style, x, y, w, h, parent, menu, inst, param);
        return g_createWindowExA.stdcall<HWND>(exStyle, cls, title, style, x, y, w, h, parent, menu, inst, param);
    }

    BOOL WINAPI SetWindowTextWHook(HWND window, LPCWSTR text)
    {
        if (IsMainWindow(GetParent(window), (DWORD)GetWindowLongPtrW(window, GWL_STYLE)))
            return g_setWindowTextW.stdcall<BOOL>(window, WithSuffix(text).c_str());
        return g_setWindowTextW.stdcall<BOOL>(window, text);
    }

    BOOL WINAPI SetWindowTextAHook(HWND window, LPCSTR text)
    {
        if (IsMainWindow(GetParent(window), (DWORD)GetWindowLongPtrW(window, GWL_STYLE)))
            return g_setWindowTextA.stdcall<BOOL>(window, WithSuffix(text).c_str());
        return g_setWindowTextA.stdcall<BOOL>(window, text);
    }

    void InstallHooks()
    {
        if (g_settings.titleSuffix.empty()) return;
        HMODULE user32 = GetModuleHandleW(L"user32.dll");
        if (!user32) return; // a game without windows
        g_createWindowExW = safetyhook::create_inline(GetProcAddress(user32, "CreateWindowExW"), CreateWindowExWHook);
        g_createWindowExA = safetyhook::create_inline(GetProcAddress(user32, "CreateWindowExA"), CreateWindowExAHook);
        g_setWindowTextW = safetyhook::create_inline(GetProcAddress(user32, "SetWindowTextW"), SetWindowTextWHook);
        g_setWindowTextA = safetyhook::create_inline(GetProcAddress(user32, "SetWindowTextA"), SetWindowTextAHook);
        Log("window title hooks: %s", g_createWindowExW && g_createWindowExA && g_setWindowTextW && g_setWindowTextA ? "installed" : "FAILED");
    }

    // Finding code by its bytes ('?' is any byte) keeps a plugin working across game updates.
    // hook::pattern searches the exe, hook::module_pattern(module, ...) a DLL. Always check the
    // match count, a pattern that isn't found has no address.
    void SearchPattern()
    {
        if (g_settings.pattern.empty()) return;
        auto pattern = hook::pattern(g_settings.pattern);
        Log("pattern \"%s\": %zu match(es)", g_settings.pattern.c_str(), pattern.size());
        auto exe = (uintptr_t)GetModuleHandleW(nullptr);
        for (size_t i = 0; i < pattern.size() && i < 20; ++i)
        {
            auto address = (uintptr_t)pattern.get(i).get<void>();
            Log("  %p (exe+0x%zx)", (void*)address, (size_t)(address - exe));
        }
    }

    const char* g_startedBy = "";

    void Init()
    {
        LoadSettings();
        Log("PluginTemplate loaded from %ls, started by %s", ModulePath(ThisModule()).c_str(), g_startedBy);
        Log("game: %ls", ModulePath(nullptr).c_str());
        if (HMODULE loader = Loader())
        {
            Log("Ultimate ASI Loader: %ls", ModulePath(loader).c_str());
            // active update folder, whose files override the game's
            using GetOverloadPathW_t = bool(WINAPI*)(wchar_t* out, size_t outSize);
            auto getOverloadPath = (GetOverloadPathW_t)GetProcAddress(loader, "GetOverloadPathW");
            wchar_t update[MAX_PATH * 4];
            Log("update folder: %ls", getOverloadPath && getOverloadPath(update, std::size(update)) ? update : L"(none)");
        }
        else
            Log("not loaded by Ultimate ASI Loader");
        InstallHooks();
        SearchPattern();
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

// Called by Ultimate ASI Loader outside the loader lock, just before the game's code runs.
extern "C" __declspec(dllexport) void InitializeASI()
{
    g_startedBy = "InitializeASI (Ultimate ASI Loader)";
    InitOnce();
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID reserved)
{
    // Other ASI loaders only load the DLL, so start here. This is under the loader lock: keep Init
    // free of windows, thread waits, COM and LoadLibrary. Hooks must be in place before the game's
    // code runs, so deferring to a thread is not an option either.
    if (reason == DLL_PROCESS_ATTACH && !LoadedByUltimateASILoader())
    {
        g_startedBy = "DllMain (another ASI loader)";
        InitOnce();
    }
    // On FreeLibrary, not process exit, remove the hooks before their code is unmapped.
    if (reason == DLL_PROCESS_DETACH && !reserved)
    {
        g_setWindowTextA = {};
        g_setWindowTextW = {};
        g_createWindowExA = {};
        g_createWindowExW = {};
    }
    return TRUE;
}
