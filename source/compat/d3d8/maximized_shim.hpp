// [GlobalSets] Direct3D8DisableMaximizedWindowedModeShim (Win32): turns off the maximized
// windowed mode shim Windows applies to D3D8 games via Direct3D8EnableMaximizedWindowedModeShim.
#pragma once
#include <windows.h>

namespace ual::compat::d3d8
{
    // d3d8 is the d3d8.dll in use (the proxied original if the loader is d3d8.dll), or null to load it.
    void DisableMaximizedWindowedModeShim(HMODULE d3d8);
}
