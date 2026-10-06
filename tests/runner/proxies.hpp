// Every proxy name the loader supports, matching kNames in source/loader/proxy/proxy.cpp.
// The loading and forwarding tests are generated from this table.
#pragma once

namespace runner
{
    struct ProxyInfo
    {
        const wchar_t* file;
        bool win32;
        bool x64;
        bool staticHost; // ual_host_<name>.exe is built, see tests/premake5.lua
        bool forward;    // covered by the "forward" scenario
    };

    inline constexpr ProxyInfo kProxies[] = {
        { L"dinput8.dll", true, true, true, true },
        { L"dsound.dll", true, true, true, true },
        { L"wininet.dll", true, true, true, true },
        { L"version.dll", true, true, true, true },
        { L"d3d9.dll", true, true, true, true },
        { L"d3d10.dll", true, true, true, true },
        { L"d3d11.dll", true, true, true, true },
        { L"d3d12.dll", true, true, true, true },
        { L"dxgi.dll", true, true, true, true },
        { L"winmm.dll", true, true, true, true },
        { L"winhttp.dll", true, true, true, true },
        { L"xinput1_1.dll", true, true, false, true },
        { L"xinput1_2.dll", true, true, false, true },
        { L"xinput1_3.dll", true, true, false, true },
        { L"xinput1_4.dll", true, true, true, true },
        { L"xinput9_1_0.dll", true, true, true, true },
        { L"xinputuap.dll", true, true, true, true },
        // 32-bit only
        { L"d3d8.dll", true, false, false, true },
        { L"ddraw.dll", true, false, true, true },
        { L"dinput.dll", true, false, false, true },
        { L"msacm32.dll", true, false, true, true },
        { L"msvfw32.dll", true, false, true, true },
        { L"vorbisFile.dll", true, false, false, false },
        { L"binkw32.dll", true, false, false, false },
        { L"bink2w32.dll", true, false, false, false },
        { L"xlive.dll", true, false, false, false },
        // 64-bit only
        { L"binkw64.dll", false, true, false, false },
        { L"bink2w64.dll", false, true, false, false },
    };
}
