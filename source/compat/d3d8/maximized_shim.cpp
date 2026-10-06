#include "maximized_shim.hpp"
#include <cstdint>
#include <string>

namespace ual::compat::d3d8
{
    void DisableMaximizedWindowedModeShim(HMODULE d3d8)
    {
        if (!d3d8)
        {
            d3d8 = LoadLibraryW(L"d3d8.dll");
            if (!d3d8)
            {
                wchar_t sys[MAX_PATH];
                UINT n = GetSystemDirectoryW(sys, MAX_PATH);
                if (n && n < MAX_PATH) d3d8 = LoadLibraryW((std::wstring(sys, n) + L"\\d3d8.dll").c_str());
            }
        }
        if (!d3d8) return;

        // The export starts with mov dword ptr [flag], imm32 (C7 05 <flag> <imm32>).
        // Patch the immediate to 0 and clear the flag.
        auto addr = (uintptr_t)GetProcAddress(d3d8, "Direct3D8EnableMaximizedWindowedModeShim");
        if (!addr) return;
        DWORD protect;
        if (!VirtualProtect((void*)(addr + 6), 4, PAGE_EXECUTE_READWRITE, &protect)) return;
        *(uint32_t*)(addr + 6) = 0;
        *(uint32_t*)(*(uint32_t*)(addr + 2)) = 0;
        VirtualProtect((void*)(addr + 6), 4, protect, &protect);
    }
}
