// Fake original DLL deployed as <name>Hooked.dll. Each export returns a sentinel so the
// tests can tell the loader forwarded to it instead of System32.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "../common/sentinels.hpp"

extern "C" HRESULT WINAPI DirectInput8Create(HINSTANCE, DWORD, const void* riid, void** ppvOut, void* outer)
{
    if (ppvOut) *ppvOut = nullptr;
    return ualtest::kFakeDirectInput8CreateResult;
}

extern "C" DWORD WINAPI GetFileVersionInfoSizeW(const wchar_t*, DWORD* lpdwHandle)
{
    if (lpdwHandle) *lpdwHandle = 0;
    return ualtest::kFakeFileVersionInfoSize;
}

extern "C" DWORD WINAPI timeGetTime()
{
    return ualtest::kFakeTimeGetTime;
}

extern "C" long __cdecl ov_streams(void*)
{
    return ualtest::kFakeOvStreams;
}

// Xidi.32.dll / Xidi.64.dll exports, see source/compat/xidi
extern "C" HRESULT WINAPI dinput8_DirectInput8Create(HINSTANCE, DWORD, const void*, void** ppvOut, void*)
{
    if (ppvOut) *ppvOut = nullptr;
    return ualtest::kFakeXidiDirectInput8CreateResult;
}

extern "C" HRESULT WINAPI dinput_DirectInputCreateA(HINSTANCE, DWORD, void** ppDI, void*)
{
    if (ppDI) *ppDI = nullptr;
    return ualtest::kFakeXidiDirectInputCreateResult;
}

extern "C" UINT WINAPI winmm_joyGetNumDevs()
{
    return ualtest::kFakeXidiJoyGetNumDevs;
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID)
{
    return TRUE;
}
