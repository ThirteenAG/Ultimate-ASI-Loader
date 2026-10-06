// UseDirectInput (DirectInput 1-8, A and W): devices are made non-exclusive, since an exclusive
// mouse is captured and hidden in a window and an exclusive keyboard blocks Alt+Tab. While the game
// window is inactive they report no input, so keys typed into other windows don't reach the game.
#include "internal.hpp"
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

namespace wndmode
{
    namespace
    {
        // vtable slots (identical for IDirectInputDevice, 2, 7 and 8, A and W)
        enum { DEV_Acquire = 7, DEV_GetDeviceState = 9, DEV_GetDeviceData = 10, DEV_SetCooperativeLevel = 13 };
        enum { DI_CreateDevice = 3, DI7_CreateDeviceEx = 9 };

        // IIDs (from dinput.h) compared by value to avoid linking dxguid.lib
        constexpr GUID kIID_IDirectInput7A = { 0x9a4cb684, 0x236d, 0x11d3, { 0x8e, 0x9d, 0x00, 0xc0, 0x4f, 0x68, 0x44, 0xae } };
        constexpr GUID kIID_IDirectInput7W = { 0x9a4cb685, 0x236d, 0x11d3, { 0x8e, 0x9d, 0x00, 0xc0, 0x4f, 0x68, 0x44, 0xae } };

        using Create8Fn = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
        using CreateFn = HRESULT(WINAPI*)(HINSTANCE, DWORD, LPVOID*, LPUNKNOWN);
        using CreateExFn = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
        Create8Fn oDirectInput8Create;
        CreateFn oDirectInputCreateA, oDirectInputCreateW;
        CreateExFn oDirectInputCreateEx;

        HRESULT STDMETHODCALLTYPE Dev_Acquire(IUnknown* dev)
        {
            if (!IsAppActive()) return DI_OK; // do not grab the device while in the background
            return Orig<decltype(&Dev_Acquire)>(dev, DEV_Acquire)(dev);
        }

        HRESULT STDMETHODCALLTYPE Dev_GetDeviceState(IUnknown* dev, DWORD size, LPVOID data)
        {
            if (IsAppActive())
            {
                HRESULT hr = Orig<decltype(&Dev_Acquire)>(dev, DEV_Acquire)(dev);
                if (hr == DI_OK || hr == S_FALSE)
                    return Orig<decltype(&Dev_GetDeviceState)>(dev, DEV_GetDeviceState)(dev, size, data);
            }
            if (data) memset(data, 0, size);
            return DI_OK;
        }

        HRESULT STDMETHODCALLTYPE Dev_GetDeviceData(IUnknown* dev, DWORD objSize, LPDIDEVICEOBJECTDATA rgdod, LPDWORD inOut, DWORD flags)
        {
            if (IsAppActive())
            {
                HRESULT hr = Orig<decltype(&Dev_Acquire)>(dev, DEV_Acquire)(dev);
                if (hr == DI_OK || hr == S_FALSE)
                    return Orig<decltype(&Dev_GetDeviceData)>(dev, DEV_GetDeviceData)(dev, objSize, rgdod, inOut, flags);
            }
            if (inOut) *inOut = 0;
            return DI_OK;
        }

        HRESULT STDMETHODCALLTYPE Dev_SetCooperativeLevel(IUnknown* dev, HWND hwnd, DWORD flags)
        {
            auto orig = Orig<decltype(&Dev_SetCooperativeLevel)>(dev, DEV_SetCooperativeLevel);
            // Background access stays: input is filtered while the game is
            // inactive anyway, and foreground access fails for child windows.
            DWORD fixed = flags;
            if (fixed & DISCL_EXCLUSIVE) fixed = (fixed & ~DISCL_EXCLUSIVE) | DISCL_NONEXCLUSIVE;
            fixed &= ~DISCL_NOWINKEY;
            HWND top = hwnd ? GetAncestor(hwnd, GA_ROOT) : nullptr;
            HRESULT hr = orig(dev, top ? top : hwnd, fixed);
            if (FAILED(hr) && (fixed & DISCL_FOREGROUND))
                hr = orig(dev, top ? top : hwnd, (fixed & ~DISCL_FOREGROUND) | DISCL_BACKGROUND);
            if (FAILED(hr))
                hr = orig(dev, hwnd, flags); // what the game asked for
            return hr;
        }

        void PatchDevice(void* dev)
        {
            PatchVtable(dev, DEV_Acquire, (void*)Dev_Acquire);
            PatchVtable(dev, DEV_GetDeviceState, (void*)Dev_GetDeviceState);
            PatchVtable(dev, DEV_GetDeviceData, (void*)Dev_GetDeviceData);
            PatchVtable(dev, DEV_SetCooperativeLevel, (void*)Dev_SetCooperativeLevel);
        }

        HRESULT STDMETHODCALLTYPE DI_CreateDeviceHook(IUnknown* di, REFGUID guid, IUnknown** out, IUnknown* outer)
        {
            HRESULT hr = Orig<decltype(&DI_CreateDeviceHook)>(di, DI_CreateDevice)(di, guid, out, outer);
            if (SUCCEEDED(hr) && out && *out) PatchDevice(*out);
            return hr;
        }

        HRESULT STDMETHODCALLTYPE DI7_CreateDeviceExHook(IUnknown* di, REFGUID guid, REFIID iid, void** out, IUnknown* outer)
        {
            HRESULT hr = Orig<decltype(&DI7_CreateDeviceExHook)>(di, DI7_CreateDeviceEx)(di, guid, iid, out, outer);
            if (SUCCEEDED(hr) && out && *out) PatchDevice(*out);
            return hr;
        }

        void PatchInterface(void* di, bool v7)
        {
            PatchVtable(di, DI_CreateDevice, (void*)DI_CreateDeviceHook);
            if (v7) PatchVtable(di, DI7_CreateDeviceEx, (void*)DI7_CreateDeviceExHook);
        }

        HRESULT WINAPI hkDirectInput8Create(HINSTANCE inst, DWORD ver, REFIID iid, LPVOID* out, LPUNKNOWN outer)
        {
            HRESULT hr = oDirectInput8Create(inst, ver, iid, out, outer);
            if (SUCCEEDED(hr) && out && *out) PatchInterface(*out, false);
            return hr;
        }

        HRESULT WINAPI hkDirectInputCreateA(HINSTANCE inst, DWORD ver, LPVOID* out, LPUNKNOWN outer)
        {
            HRESULT hr = oDirectInputCreateA(inst, ver, out, outer);
            if (SUCCEEDED(hr) && out && *out) PatchInterface(*out, false);
            return hr;
        }

        HRESULT WINAPI hkDirectInputCreateW(HINSTANCE inst, DWORD ver, LPVOID* out, LPUNKNOWN outer)
        {
            HRESULT hr = oDirectInputCreateW(inst, ver, out, outer);
            if (SUCCEEDED(hr) && out && *out) PatchInterface(*out, false);
            return hr;
        }

        HRESULT WINAPI hkDirectInputCreateEx(HINSTANCE inst, DWORD ver, REFIID iid, LPVOID* out, LPUNKNOWN outer)
        {
            HRESULT hr = oDirectInputCreateEx(inst, ver, iid, out, outer);
            if (SUCCEEDED(hr) && out && *out)
                PatchInterface(*out, IsEqualGUID(iid, kIID_IDirectInput7A) || IsEqualGUID(iid, kIID_IDirectInput7W));
            return hr;
        }
    }

    void InstallDInputHooks()
    {
        if (!g_cfg.UseDirectInput) return;
        HookExport(L"dinput8.dll", "DirectInput8Create", (void*)hkDirectInput8Create, (void**)&oDirectInput8Create);
        HookExport(L"dinput.dll", "DirectInputCreateA", (void*)hkDirectInputCreateA, (void**)&oDirectInputCreateA);
        HookExport(L"dinput.dll", "DirectInputCreateW", (void*)hkDirectInputCreateW, (void**)&oDirectInputCreateW);
        HookExport(L"dinput.dll", "DirectInputCreateEx", (void*)hkDirectInputCreateEx, (void**)&oDirectInputCreateEx);
    }
}
