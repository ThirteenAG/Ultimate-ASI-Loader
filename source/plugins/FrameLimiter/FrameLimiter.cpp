// Caps the frame rate of Direct3D 9-12 games, settings in FrameLimiter.ini.
//
// Shows how to hook a COM method in any game. Create a throwaway device or swap chain on a hidden
// window, read the method address from its vtable (all objects of the class share the code), hook
// it with safetyhook and release the throwaway objects. The hook then delays each Present.
//
// D3D10-12 all present through IDXGISwapChain, implemented once in dxgi.dll. D3D9 presents
// through IDirect3DDevice9::Present/PresentEx and IDirect3DSwapChain9::Present.
#include <windows.h>
#include <d3d9.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <mutex>
#include <stacktrace>
#include <string>
#include <type_traits>
#include <safetyhook.hpp>

namespace
{
    LONGLONG g_frequency = 0; // performance counter ticks per second
    LONGLONG g_period = 0;    // ticks per frame, 0 for no limit
    LONGLONG g_due = 0;       // earliest time for the next Present
    HANDLE g_timer = nullptr;

    LONGLONG Now()
    {
        LARGE_INTEGER t;
        QueryPerformanceCounter(&t);
        return t.QuadPart;
    }

    // Sleeps, then spins the last millisecond, since Sleep alone is too coarse for 144 fps.
    void WaitForFrame()
    {
        LONGLONG now = Now();
        if (now >= g_due + g_period) // first frame, or the game fell behind; don't catch up
        {
            g_due = now + g_period;
            return;
        }
        LONGLONG spin = g_frequency / 1000;
        if (g_due - now > spin && g_timer)
        {
            LARGE_INTEGER wait;
            wait.QuadPart = -((g_due - now - spin) * 10000000 / g_frequency); // relative, 100 ns units
            if (SetWaitableTimerEx(g_timer, &wait, 0, nullptr, nullptr, nullptr, 0)) WaitForSingleObject(g_timer, INFINITE);
        }
        while (Now() < g_due) YieldProcessor();
        g_due += g_period;
    }

    // The device's Present may call the swap chain's internally. Only the outermost call waits.
    thread_local int t_depth = 0;
    struct Presenting
    {
        Presenting() { if (t_depth++ == 0) WaitForFrame(); }
        ~Presenting() { --t_depth; }
    };

    SafetyHookInline g_d3d9Present{}, g_d3d9PresentEx{}, g_d3d9SwapChainPresent{}, g_dxgiPresent{}, g_dxgiPresent1{};

    HRESULT STDMETHODCALLTYPE D3D9Present(IDirect3DDevice9* self, const RECT* src, const RECT* dst, HWND window, const RGNDATA* dirty)
    {
        Presenting presenting;
        return g_d3d9Present.stdcall<HRESULT>(self, src, dst, window, dirty);
    }

    HRESULT STDMETHODCALLTYPE D3D9PresentEx(IDirect3DDevice9Ex* self, const RECT* src, const RECT* dst, HWND window, const RGNDATA* dirty, DWORD flags)
    {
        Presenting presenting;
        return g_d3d9PresentEx.stdcall<HRESULT>(self, src, dst, window, dirty, flags);
    }

    HRESULT STDMETHODCALLTYPE D3D9SwapChainPresent(IDirect3DSwapChain9* self, const RECT* src, const RECT* dst, HWND window, const RGNDATA* dirty,
                                                   DWORD flags)
    {
        Presenting presenting;
        return g_d3d9SwapChainPresent.stdcall<HRESULT>(self, src, dst, window, dirty, flags);
    }

    HRESULT STDMETHODCALLTYPE DXGIPresent(IDXGISwapChain* self, UINT syncInterval, UINT flags)
    {
        if (flags & DXGI_PRESENT_TEST) return g_dxgiPresent.stdcall<HRESULT>(self, syncInterval, flags); // shows nothing
        Presenting presenting;
        return g_dxgiPresent.stdcall<HRESULT>(self, syncInterval, flags);
    }

    HRESULT STDMETHODCALLTYPE DXGIPresent1(IDXGISwapChain1* self, UINT syncInterval, UINT flags, const DXGI_PRESENT_PARAMETERS* params)
    {
        if (flags & DXGI_PRESENT_TEST) return g_dxgiPresent1.stdcall<HRESULT>(self, syncInterval, flags, params);
        Presenting presenting;
        return g_dxgiPresent1.stdcall<HRESULT>(self, syncInterval, flags, params);
    }

    // index counts base interface methods too
    void* Method(void* object, int index)
    {
        return (*reinterpret_cast<void***>(object))[index];
    }

    // Vtable slots are IDirect3DDevice9::Present 17, IDirect3DSwapChain9::Present 3 and
    // IDirect3DDevice9Ex::PresentEx 121. Plain and Ex devices can be different classes, so each
    // method comes from the device type games call it on.
    template<class Device>
    Device* CreateDevice(IDirect3D9* d3d, HWND window)
    {
        D3DPRESENT_PARAMETERS pp{};
        pp.Windowed = TRUE;
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.hDeviceWindow = window;
        DWORD flags = D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_DISABLE_DRIVER_MANAGEMENT;
        Device* device = nullptr;
        if constexpr (std::is_same_v<Device, IDirect3DDevice9Ex>)
            static_cast<IDirect3D9Ex*>(d3d)->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, flags, &pp, nullptr, &device);
        else
            d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, window, flags, &pp, &device);
        return device;
    }

    void HookDirect3D9(HWND window)
    {
        HMODULE d3d9 = LoadLibraryW(L"d3d9.dll");
        if (!d3d9) return;
        if (auto create9 = (decltype(&Direct3DCreate9))GetProcAddress(d3d9, "Direct3DCreate9"))
            if (IDirect3D9* d3d = create9(D3D_SDK_VERSION))
            {
                if (auto device = CreateDevice<IDirect3DDevice9>(d3d, window))
                {
                    g_d3d9Present = safetyhook::create_inline(Method(device, 17), D3D9Present);
                    IDirect3DSwapChain9* chain = nullptr;
                    if (SUCCEEDED(device->GetSwapChain(0, &chain)))
                    {
                        g_d3d9SwapChainPresent = safetyhook::create_inline(Method(chain, 3), D3D9SwapChainPresent);
                        chain->Release();
                    }
                    device->Release();
                }
                d3d->Release();
            }
        IDirect3D9Ex* d3dEx = nullptr;
        if (auto create9Ex = (decltype(&Direct3DCreate9Ex))GetProcAddress(d3d9, "Direct3DCreate9Ex"))
            if (SUCCEEDED(create9Ex(D3D_SDK_VERSION, &d3dEx)))
            {
                if (auto device = CreateDevice<IDirect3DDevice9Ex>(d3dEx, window))
                {
                    g_d3d9PresentEx = safetyhook::create_inline(Method(device, 121), D3D9PresentEx);
                    device->Release();
                }
                d3dEx->Release();
            }
    }

    // vtable slots: IDXGISwapChain::Present 8, IDXGISwapChain1::Present1 22
    void HookDXGI(HWND window)
    {
        HMODULE d3d11 = LoadLibraryW(L"d3d11.dll");
        auto create = d3d11 ? (PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN)GetProcAddress(d3d11, "D3D11CreateDeviceAndSwapChain") : nullptr;
        if (!create) return;
        DXGI_SWAP_CHAIN_DESC desc{};
        desc.BufferCount = 1;
        desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.OutputWindow = window;
        desc.SampleDesc.Count = 1;
        desc.Windowed = TRUE;
        IDXGISwapChain* chain = nullptr;
        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* context = nullptr;
        for (auto type : { D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP })
            if (SUCCEEDED(create(nullptr, type, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &desc, &chain, &device, nullptr, &context))) break;
        if (!chain) return;
        g_dxgiPresent = safetyhook::create_inline(Method(chain, 8), DXGIPresent);
        IDXGISwapChain1* chain1 = nullptr;
        if (SUCCEEDED(chain->QueryInterface(IID_PPV_ARGS(&chain1))))
        {
            g_dxgiPresent1 = safetyhook::create_inline(Method(chain1, 22), DXGIPresent1);
            chain1->Release();
        }
        context->Release();
        device->Release();
        chain->Release();
    }

    HMODULE ThisModule()
    {
        HMODULE m = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&ThisModule, &m);
        return m;
    }

    void Init()
    {
        // FrameLimiter.asi -> FrameLimiter.ini
        wchar_t path[MAX_PATH * 4];
        std::wstring ini(path, GetModuleFileNameW(ThisModule(), path, (DWORD)std::size(path)));
        ini = ini.substr(0, ini.find_last_of(L'.')) + L".ini";

        int fps = (int)GetPrivateProfileIntW(L"MAIN", L"FPSLimit", 60, ini.c_str());
        if (fps <= 0) return;
        LARGE_INTEGER frequency;
        QueryPerformanceFrequency(&frequency);
        g_frequency = frequency.QuadPart;
        g_period = g_frequency / fps;
        g_timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        if (!g_timer) g_timer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS); // before Windows 10 1803

        // auto picks the API whose DLL the game already loaded, or both if neither is loaded
        wchar_t api[16];
        GetPrivateProfileStringW(L"MAIN", L"Api", L"auto", api, (DWORD)std::size(api), ini.c_str());
        bool d3d9 = !_wcsicmp(api, L"d3d9") || !_wcsicmp(api, L"all");
        bool dxgi = !_wcsicmp(api, L"dxgi") || !_wcsicmp(api, L"all");
        if (!d3d9 && !dxgi)
        {
            d3d9 = GetModuleHandleW(L"d3d9.dll") != nullptr;
            for (auto dll : { L"dxgi.dll", L"d3d10.dll", L"d3d10_1.dll", L"d3d11.dll", L"d3d12.dll" }) dxgi |= GetModuleHandleW(dll) != nullptr;
            if (!d3d9 && !dxgi) d3d9 = dxgi = true;
        }

        WNDCLASSEXW wc{ sizeof(wc) };
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = ThisModule();
        wc.lpszClassName = L"FrameLimiterDummy";
        RegisterClassExW(&wc);
        HWND window = CreateWindowExW(0, wc.lpszClassName, L"", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, wc.hInstance, nullptr);
        if (!window) return;
        if (d3d9) HookDirect3D9(window);
        if (dxgi) HookDXGI(window);
        DestroyWindow(window);
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

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID reserved)
{
    // Other ASI loaders only load the DLL. Creating D3D devices under the loader lock can deadlock,
    // so Init runs on a thread that starts once the lock is released, long before the first frame.
    if (reason == DLL_PROCESS_ATTACH && !LoadedByUltimateASILoader())
        if (HANDLE thread = CreateThread(nullptr, 0, [](LPVOID) -> DWORD { InitOnce(); return 0; }, nullptr, 0, nullptr))
            CloseHandle(thread);
    // on FreeLibrary, remove the hooks before their code is unmapped
    if (reason == DLL_PROCESS_DETACH && !reserved)
    {
        g_dxgiPresent1 = {};
        g_dxgiPresent = {};
        g_d3d9SwapChainPresent = {};
        g_d3d9PresentEx = {};
        g_d3d9Present = {};
    }
    return TRUE;
}
