// Forces old D3D8/9, DirectDraw and GDI fullscreen games into a window. 32-bit only.
// Reads wndmode.ini in the format of the legacy "DirectX Windower" wndmode.dll it replaces.
#pragma once

#include <string>

namespace wndmode
{
    // Reads iniPath and installs the hooks. False if the file can't be read. Call once per process.
    bool Initialize(const std::wstring& iniPath);

    // Removes all hooks. For FreeLibrary; at process exit removing hooks is neither needed nor safe.
    void Shutdown();

    // At process exit: restores the real display mode and gamma ramp a DirectDraw game changed.
    void ShutdownAtExit();
}
