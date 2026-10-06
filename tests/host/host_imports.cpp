// ual_host_<proxy>.exe takes the address of one <proxy>.dll export so the DLL lands in the
// import table and the loader is mapped before the CRT starts, like in a real game.
// ual_host.exe has no proxy import.
#include <windows.h>

#if defined(UAL_HOST_PROXY_dinput8)
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#define UAL_PROXY "dinput8.dll"
#define UAL_ANCHOR DirectInput8Create
#elif defined(UAL_HOST_PROXY_d3d9)
#include <d3d9.h>
#define UAL_PROXY "d3d9.dll"
#define UAL_ANCHOR Direct3DCreate9
#elif defined(UAL_HOST_PROXY_d3d10)
#include <d3d10.h>
#define UAL_PROXY "d3d10.dll"
#define UAL_ANCHOR D3D10CreateDevice
#elif defined(UAL_HOST_PROXY_d3d11)
#include <d3d11.h>
#define UAL_PROXY "d3d11.dll"
#define UAL_ANCHOR D3D11CreateDevice
#elif defined(UAL_HOST_PROXY_d3d12)
#include <d3d12.h>
#define UAL_PROXY "d3d12.dll"
#define UAL_ANCHOR D3D12GetDebugInterface
#elif defined(UAL_HOST_PROXY_dxgi)
#include <dxgi.h>
#define UAL_PROXY "dxgi.dll"
#define UAL_ANCHOR CreateDXGIFactory
#elif defined(UAL_HOST_PROXY_dsound)
#include <mmsystem.h>
#include <dsound.h>
#define UAL_PROXY "dsound.dll"
#define UAL_ANCHOR DirectSoundCreate8
#elif defined(UAL_HOST_PROXY_winmm)
#include <mmsystem.h>
#define UAL_PROXY "winmm.dll"
#define UAL_ANCHOR timeGetTime
#elif defined(UAL_HOST_PROXY_version)
#include <winver.h>
#define UAL_PROXY "version.dll"
#define UAL_ANCHOR GetFileVersionInfoSizeW
#elif defined(UAL_HOST_PROXY_wininet)
#include <wininet.h>
#define UAL_PROXY "wininet.dll"
#define UAL_ANCHOR InternetOpenW
#elif defined(UAL_HOST_PROXY_winhttp)
#include <winhttp.h>
#define UAL_PROXY "winhttp.dll"
#define UAL_ANCHOR WinHttpCheckPlatform
#elif defined(UAL_HOST_PROXY_xinput1_4)
#include <xinput.h>
#define UAL_PROXY "xinput1_4.dll"
#define UAL_ANCHOR XInputGetState
#elif defined(UAL_HOST_PROXY_xinput9_1_0)
#include <xinput.h>
#define UAL_PROXY "xinput9_1_0.dll"
#define UAL_ANCHOR XInputGetState
#elif defined(UAL_HOST_PROXY_xinputuap)
#include <xinput.h>
#define UAL_PROXY "xinputuap.dll"
#define UAL_ANCHOR XInputGetState
#elif defined(UAL_HOST_PROXY_ddraw)
#include <ddraw.h>
#define UAL_PROXY "ddraw.dll"
#define UAL_ANCHOR DirectDrawCreate
#elif defined(UAL_HOST_PROXY_msacm32)
#include <mmreg.h>
#include <msacm.h>
#define UAL_PROXY "msacm32.dll"
#define UAL_ANCHOR acmGetVersion
#elif defined(UAL_HOST_PROXY_msvfw32)
#include <vfw.h>
#define UAL_PROXY "msvfw32.dll"
#define UAL_ANCHOR ICOpen
#endif

#ifdef UAL_ANCHOR
// volatile so the optimizer cannot drop the import
static const void* volatile g_anchor = reinterpret_cast<const void*>(&UAL_ANCHOR);
extern "C" const char* ual_host_static_proxy() { return UAL_PROXY; }
extern "C" const void* ual_host_static_anchor() { return g_anchor; }
#else
extern "C" const char* ual_host_static_proxy() { return ""; }
extern "C" const void* ual_host_static_anchor() { return nullptr; }
#endif
