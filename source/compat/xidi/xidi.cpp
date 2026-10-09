#include "xidi.hpp"
#include <cstring>

namespace ual::compat::xidi
{
    HMODULE Load(const char* proxy, const std::wstring& dir)
    {
        if (strcmp(proxy, "dinput") && strcmp(proxy, "dinput8") && strcmp(proxy, "winmm")) return nullptr;
#ifdef _WIN64
        auto path = dir + L"Xidi.64.dll";
#else
        auto path = dir + L"Xidi.32.dll";
#endif
        if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) return nullptr;
        return LoadLibraryW(path.c_str()); // Xidi loads the system DLL itself
    }

    FARPROC Export(HMODULE xidi, const char* proxy, const char* name)
    {
        std::string exported = std::string(proxy) + "_" + name;
        return GetProcAddress(xidi, exported.c_str());
    }
}
