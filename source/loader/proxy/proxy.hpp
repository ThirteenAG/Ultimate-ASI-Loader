// The loader proxies the DLL it is named after. Exports forward to <name>Hooked.dll
// next to the loader if present, else to the system DLL. Tables are in exports.inl,
// thunks.inl and ordinals.inl.
#pragma once
#include <windows.h>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ual::proxy
{
    // Runs once, thread-safe.
    void LoadOriginalLibrary();

    bool IsSupportedName(const std::wstring& fileName);

    // Null if not loaded or loaded from memory.
    HMODULE OriginalModule();

    // DllCanUnloadNow, DllGetClassObject, ..., shared by several proxies.
    void LoadSharedExports(HMODULE dll);

    // Maps ordinals of the system DLL to loader exports, since an exe importing by ordinal
    // means the system DLL's ordinals, not ours. Named exports are looked up in this
    // machine's system DLL, so the mapping matches its Windows version.
    std::vector<std::pair<WORD, const void*>> OrdinalEntries(std::wstring_view dllName);

    // Null if the proxy has no such export.
    const void* ExportFor(std::wstring_view dllName, std::string_view name);
}
