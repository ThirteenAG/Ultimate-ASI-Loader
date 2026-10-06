// Scenarios for the demo plugins in source/plugins
#include "host.hpp"
#include <chrono>
#include <d3d9.h>
#include <d3d11.h>
#include <dxgi.h>

namespace
{
    HWND MakeWindow(const wchar_t* title, int w = 320, int h = 240)
    {
        WNDCLASSEXW wc{ sizeof(wc) };
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"UalPluginScenario";
        RegisterClassExW(&wc);
        return CreateWindowExW(0, wc.lpszClassName, title, WS_OVERLAPPEDWINDOW, 0, 0, w, h, nullptr, nullptr, wc.hInstance, nullptr);
    }

    std::wstring Title(HWND window)
    {
        wchar_t text[256] = {};
        GetWindowTextW(window, text, 256);
        return text;
    }

    std::wstring Arg(const std::wstring& args, size_t index)
    {
        size_t start = 0;
        for (size_t i = 0; i < index; ++i)
        {
            start = args.find(L'|', start);
            if (start == std::wstring::npos) return {};
            ++start;
        }
        return args.substr(start, args.find(L'|', start) - start);
    }
}

// PluginTemplate hooks CreateWindowEx and SetWindowText. args is the expected title suffix.
HOST_SCENARIO(window_title)
{
    std::wstring suffix = args;
    HWND w = MakeWindow(L"Game");
    if (!host.Check(w != nullptr, "a window is created")) return;
    host.CheckEq(Title(w), L"Game" + suffix, "CreateWindowExW: the title has the suffix");
    SetWindowTextW(w, L"Renamed");
    host.CheckEq(Title(w), L"Renamed" + suffix, "SetWindowTextW: the title has the suffix");
    SetWindowTextA(w, "Ansi");
    host.CheckEq(Title(w), L"Ansi" + suffix, "SetWindowTextA: the title has the suffix");
    DestroyWindow(w);
}

// Presents frames as fast as possible and checks the rate.
// args: d3d9|dxgi, max fps (0 for none), min fps
HOST_SCENARIO(present_rate)
{
    std::wstring api = Arg(args, 0);
    double maxFps = _wtof(Arg(args, 1).c_str()), minFps = _wtof(Arg(args, 2).c_str());
    constexpr int frames = 30;
    HWND w = MakeWindow(L"present", 64, 64);
    if (!w) return host.Skip("no window");
    std::function<void()> present;
    IUnknown* objects[3] = {};

    if (api == L"d3d9")
    {
        auto create = (decltype(&Direct3DCreate9))GetProcAddress(LoadLibraryW(L"d3d9.dll"), "Direct3DCreate9");
        IDirect3D9* d3d = create ? create(D3D_SDK_VERSION) : nullptr;
        if (!d3d) return host.Skip("no Direct3D 9");
        D3DPRESENT_PARAMETERS pp{};
        pp.Windowed = TRUE;
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.hDeviceWindow = w;
        pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
        IDirect3DDevice9* device = nullptr;
        if (FAILED(d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, w, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &device)))
        {
            d3d->Release();
            return host.Skip("no Direct3D 9 device");
        }
        objects[0] = device, objects[1] = d3d;
        present = [device] { device->Present(nullptr, nullptr, nullptr, nullptr); };
    }
    else
    {
        auto create = (PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN)GetProcAddress(LoadLibraryW(L"d3d11.dll"), "D3D11CreateDeviceAndSwapChain");
        if (!create) return host.Skip("no Direct3D 11");
        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferCount = 1;
        desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.OutputWindow = w;
        desc.SampleDesc.Count = 1;
        desc.Windowed = TRUE;
        IDXGISwapChain* chain = nullptr;
        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* context = nullptr;
        for (auto type : { D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP })
            if (SUCCEEDED(create(nullptr, type, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &desc, &chain, &device, nullptr, &context))) break;
        if (!chain) return host.Skip("no Direct3D 11 swap chain");
        objects[0] = chain, objects[1] = context, objects[2] = device;
        present = [chain] { chain->Present(0, 0); };
    }

    present(); // first frame starts the pacing
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < frames; ++i) present();
    double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    double fps = frames / seconds;
    for (auto o : objects)
        if (o) o->Release();
    DestroyWindow(w);

    char detail[64];
    sprintf_s(detail, "%.1f fps", fps);
    if (maxFps > 0) host.Check(fps <= maxFps * 1.05, "presents are limited", detail);
    if (minFps > 0) host.Check(fps >= minFps, "presents are not slowed down more than the limit", detail);
}
