// This file contains code from
// GTA: SA ASI Loader 1.2
// Written by Silent
// Based on ASI Loader by Stanislav "listener" Golovin
// Initialization part made by NTAuthority

#include <windows.h>
#include <iostream>
#include <fstream>
#include <string>
#include <Shlobj.h>
#include "MemoryModule.h"
#include "vorbisHooked.h"
#include "wndmode.h"

BYTE 					originalCode[5];
BYTE* 					originalEP = 0;
HINSTANCE				hExecutableInstance;
HINSTANCE				hDLLInstance;
HMEMORYMODULE			hwndmode_dll;
TCHAR DllPath[MAX_PATH], *DllName, szSystemPath[MAX_PATH];

//=============================================================================
// vorbisfile connector
extern "C" {
	struct ov_callbacks {
		size_t(*read_func)  (void* ptr, size_t size, size_t nmemb, void* datasource);
		int(*seek_func)  (void* datasource, long long offset, int whence);
		int(*close_func) (void* datasource);
		long(*tell_func)  (void* datasource);
	};

	void* __ov_open_callbacks;
	__declspec(dllexport, naked) int ov_open_callbacks(void* datasource, void* vf, char* initial, long ibytes, ov_callbacks callbacks)
	{
		_asm jmp __ov_open_callbacks
	}

	void* __ov_clear;
	__declspec(dllexport, naked) int ov_clear(void* vf)
	{
		_asm jmp __ov_clear
	}

	void* __ov_time_total;
	__declspec(dllexport, naked) double ov_time_total(void* vf, int i)
	{
		_asm jmp __ov_time_total
	}

	void* __ov_time_tell;
	__declspec(dllexport, naked) double ov_time_tell(void* vf)
	{
		_asm jmp __ov_time_tell
	}

	void* __ov_read;
	__declspec(dllexport, naked) long ov_read(void* vf, char* buffer, int length, int bigendianp, int word, int sgned, int* bitstream)
	{
		_asm jmp __ov_read
	}

	void* __ov_info;
	__declspec(dllexport, naked) void* ov_info(void* vf, int link)
	{
		_asm jmp __ov_info
	}

	void* __ov_time_seek;
	__declspec(dllexport, naked) int ov_time_seek(void* vf, double pos)
	{
		_asm jmp __ov_time_seek
	}

	void* __ov_time_seek_page;
	__declspec(dllexport, naked) int ov_time_seek_page(void* vf, double pos)
	{
		_asm jmp __ov_time_seek_page
	}

}

struct dinput8_dll
{
	HMODULE dll;
	FARPROC DirectInput8Create;
	FARPROC DllCanUnloadNow;
	FARPROC DllGetClassObject;
	FARPROC DllRegisterServer;
	FARPROC DllUnregisterServer;
} dinput8;

struct dsound_dll
{
	HMODULE dll;
	FARPROC DirectSoundCaptureCreate;
	FARPROC DirectSoundCaptureCreate8;
	FARPROC DirectSoundCaptureEnumerateA;
	FARPROC DirectSoundCaptureEnumerateW;
	FARPROC DirectSoundCreate;
	FARPROC DirectSoundCreate8;
	FARPROC DirectSoundEnumerateA;
	FARPROC DirectSoundEnumerateW;
	FARPROC DirectSoundFullDuplexCreate;
	FARPROC DllCanUnloadNow_dsound;
	FARPROC DllGetClassObject_dsound;
	FARPROC GetDeviceID;
} dsound;

struct ddraw_dll
{
	HMODULE dll;
	FARPROC	AcquireDDThreadLock;
	FARPROC	CheckFullscreen;
	FARPROC	CompleteCreateSysmemSurface;
	FARPROC	D3DParseUnknownCommand;
	FARPROC	DDGetAttachedSurfaceLcl;
	FARPROC	DDInternalLock;
	FARPROC	DDInternalUnlock;
	FARPROC	DSoundHelp;
	FARPROC	DirectDrawCreate;
	FARPROC	DirectDrawCreateClipper;
	FARPROC	DirectDrawCreateEx;
	FARPROC	DirectDrawEnumerateA;
	FARPROC	DirectDrawEnumerateExA;
	FARPROC	DirectDrawEnumerateExW;
	FARPROC	DirectDrawEnumerateW;
	FARPROC	DllCanUnloadNow;
	FARPROC	DllGetClassObject;
	FARPROC	GetDDSurfaceLocal;
	FARPROC	GetOLEThunkData;
	FARPROC	GetSurfaceFromDC;
	FARPROC	RegisterSpecialCase;
	FARPROC	ReleaseDDThreadLock;
} ddraw;

struct d3d8_dll
{
	HMODULE dll;
	FARPROC DebugSetMute_d3d8;
	FARPROC Direct3DCreate8;
	FARPROC ValidatePixelShader;
	FARPROC ValidateVertexShader;
} d3d8;

struct d3d9_dll
{
	HMODULE dll;
	FARPROC D3DPERF_BeginEvent;
	FARPROC D3DPERF_EndEvent;
	FARPROC D3DPERF_GetStatus;
	FARPROC D3DPERF_QueryRepeatFrame;
	FARPROC D3DPERF_SetMarker;
	FARPROC D3DPERF_SetOptions;
	FARPROC D3DPERF_SetRegion;
	FARPROC DebugSetLevel;
	FARPROC DebugSetMute;
	//  FARPROC Direct3D9EnableMaximizedWindowedModeShim;
	FARPROC Direct3DCreate9;
	FARPROC Direct3DCreate9Ex;
	FARPROC Direct3DShaderValidatorCreate9;
	FARPROC PSGPError;
	FARPROC PSGPSampleTexture;
} d3d9;

struct d3d11_dll
{
	HMODULE dll;
	FARPROC D3D11CoreCreateDevice;
	FARPROC D3D11CoreCreateLayeredDevice;
	FARPROC D3D11CoreGetLayeredDeviceSize;
	FARPROC D3D11CoreRegisterLayers;
	FARPROC D3D11CreateDevice;
	FARPROC D3D11CreateDeviceAndSwapChain;
	FARPROC D3DKMTCloseAdapter;
	FARPROC D3DKMTCreateAllocation;
	FARPROC D3DKMTCreateContext;
	FARPROC D3DKMTCreateDevice;
	FARPROC D3DKMTCreateSynchronizationObject;
	FARPROC D3DKMTDestroyAllocation;
	FARPROC D3DKMTDestroyContext;
	FARPROC D3DKMTDestroyDevice;
	FARPROC D3DKMTDestroySynchronizationObject;
	FARPROC D3DKMTEscape;
	FARPROC D3DKMTGetContextSchedulingPriority;
	FARPROC D3DKMTGetDeviceState;
	FARPROC D3DKMTGetDisplayModeList;
	FARPROC D3DKMTGetMultisampleMethodList;
	FARPROC D3DKMTGetRuntimeData;
	FARPROC D3DKMTGetSharedPrimaryHandle;
	FARPROC D3DKMTLock;
	FARPROC D3DKMTOpenAdapterFromHdc;
	FARPROC D3DKMTOpenResource;
	FARPROC D3DKMTPresent;
	FARPROC D3DKMTQueryAdapterInfo;
	FARPROC D3DKMTQueryAllocationResidency;
	FARPROC D3DKMTQueryResourceInfo;
	FARPROC D3DKMTRender;
	FARPROC D3DKMTSetAllocationPriority;
	FARPROC D3DKMTSetContextSchedulingPriority;
	FARPROC D3DKMTSetDisplayMode;
	FARPROC D3DKMTSetDisplayPrivateDriverFormat;
	FARPROC D3DKMTSetGammaRamp;
	FARPROC D3DKMTSetVidPnSourceOwner;
	FARPROC D3DKMTSignalSynchronizationObject;
	FARPROC D3DKMTUnlock;
	FARPROC D3DKMTWaitForSynchronizationObject;
	FARPROC D3DKMTWaitForVerticalBlankEvent;
	FARPROC D3DPerformance_BeginEvent;
	FARPROC D3DPerformance_EndEvent;
	FARPROC D3DPerformance_GetStatus;
	FARPROC D3DPerformance_SetMarker;
	FARPROC EnableFeatureLevelUpgrade;
	FARPROC OpenAdapter10;
	FARPROC OpenAdapter10_2;
} d3d11;

__declspec(naked) void _DirectInput8Create() { _asm { jmp[dinput8.DirectInput8Create] } }
__declspec(naked) void _DllCanUnloadNow() { _asm { jmp[dinput8.DllCanUnloadNow] } }
__declspec(naked) void _DllGetClassObject() { _asm { jmp[dinput8.DllGetClassObject] } }
__declspec(naked) void _DllRegisterServer() { _asm { jmp[dinput8.DllRegisterServer] } }
__declspec(naked) void _DllUnregisterServer() { _asm { jmp[dinput8.DllUnregisterServer] } }

__declspec(naked) void _DirectSoundCaptureCreate() { _asm { jmp[dsound.DirectSoundCaptureCreate] } }
__declspec(naked) void _DirectSoundCaptureCreate8() { _asm { jmp[dsound.DirectSoundCaptureCreate8] } }
__declspec(naked) void _DirectSoundCaptureEnumerateA() { _asm { jmp[dsound.DirectSoundCaptureEnumerateA] } }
__declspec(naked) void _DirectSoundCaptureEnumerateW() { _asm { jmp[dsound.DirectSoundCaptureEnumerateW] } }
__declspec(naked) void _DirectSoundCreate() { _asm { jmp[dsound.DirectSoundCreate] } }
__declspec(naked) void _DirectSoundCreate8() { _asm { jmp[dsound.DirectSoundCreate8] } }
__declspec(naked) void _DirectSoundEnumerateA() { _asm { jmp[dsound.DirectSoundEnumerateA] } }
__declspec(naked) void _DirectSoundEnumerateW() { _asm { jmp[dsound.DirectSoundEnumerateW] } }
__declspec(naked) void _DirectSoundFullDuplexCreate() { _asm { jmp[dsound.DirectSoundFullDuplexCreate] } }
__declspec(naked) void _DllCanUnloadNow_dsound() { _asm { jmp[dsound.DllCanUnloadNow_dsound] } }
__declspec(naked) void _DllGetClassObject_dsound() { _asm { jmp[dsound.DllGetClassObject_dsound] } }
__declspec(naked) void _GetDeviceID() { _asm { jmp[dsound.GetDeviceID] } }

__declspec(naked) void FakeAcquireDDThreadLock()			{ _asm { jmp[ddraw.AcquireDDThreadLock] } }
__declspec(naked) void FakeCheckFullscreen()				{ _asm { jmp[ddraw.CheckFullscreen] } }
__declspec(naked) void FakeCompleteCreateSysmemSurface()	{ _asm { jmp[ddraw.CompleteCreateSysmemSurface] } }
__declspec(naked) void FakeD3DParseUnknownCommand()			{ _asm { jmp[ddraw.D3DParseUnknownCommand] } }
__declspec(naked) void FakeDDGetAttachedSurfaceLcl()		{ _asm { jmp[ddraw.DDGetAttachedSurfaceLcl] } }
__declspec(naked) void FakeDDInternalLock()					{ _asm { jmp[ddraw.DDInternalLock] } }
__declspec(naked) void FakeDDInternalUnlock()				{ _asm { jmp[ddraw.DDInternalUnlock] } }
__declspec(naked) void FakeDSoundHelp()						{ _asm { jmp[ddraw.DSoundHelp] } }
// HRESULT WINAPI DirectDrawCreate( GUID FAR *lpGUID, LPDIRECTDRAW FAR *lplpDD, IUnknown FAR *pUnkOuter );
__declspec(naked) void FakeDirectDrawCreate()				{ _asm { jmp[ddraw.DirectDrawCreate] } }
// HRESULT WINAPI DirectDrawCreateClipper( DWORD dwFlags, LPDIRECTDRAWCLIPPER FAR *lplpDDClipper, IUnknown FAR *pUnkOuter );
__declspec(naked) void FakeDirectDrawCreateClipper()		{ _asm { jmp[ddraw.DirectDrawCreateClipper] } }
// HRESULT WINAPI DirectDrawCreateEx( GUID FAR * lpGuid, LPVOID *lplpDD, REFIID iid,IUnknown FAR *pUnkOuter );
__declspec(naked) void FakeDirectDrawCreateEx()				{ _asm { jmp[ddraw.DirectDrawCreateEx] } }
// HRESULT WINAPI DirectDrawEnumerateA( LPDDENUMCALLBACKA lpCallback, LPVOID lpContext );
__declspec(naked) void FakeDirectDrawEnumerateA()			{ _asm { jmp[ddraw.DirectDrawEnumerateA] } }
// HRESULT WINAPI DirectDrawEnumerateExA( LPDDENUMCALLBACKEXA lpCallback, LPVOID lpContext, DWORD dwFlags );
__declspec(naked) void FakeDirectDrawEnumerateExA()			{ _asm { jmp[ddraw.DirectDrawEnumerateExA] } }
// HRESULT WINAPI DirectDrawEnumerateExW( LPDDENUMCALLBACKEXW lpCallback, LPVOID lpContext, DWORD dwFlags );
__declspec(naked) void FakeDirectDrawEnumerateExW()			{ _asm { jmp[ddraw.DirectDrawEnumerateExW] } }
// HRESULT WINAPI DirectDrawEnumerateW( LPDDENUMCALLBACKW lpCallback, LPVOID lpContext );
__declspec(naked) void FakeDirectDrawEnumerateW()			{ _asm { jmp[ddraw.DirectDrawEnumerateW] } }
__declspec(naked) void FakeDllCanUnloadNow()				{ _asm { jmp[ddraw.DllCanUnloadNow] } }
__declspec(naked) void FakeDllGetClassObject()				{ _asm { jmp[ddraw.DllGetClassObject] } }
__declspec(naked) void FakeGetDDSurfaceLocal()				{ _asm { jmp[ddraw.GetDDSurfaceLocal] } }
__declspec(naked) void FakeGetOLEThunkData()				{ _asm { jmp[ddraw.GetOLEThunkData] } }
__declspec(naked) void FakeGetSurfaceFromDC()				{ _asm { jmp[ddraw.GetSurfaceFromDC] } }
__declspec(naked) void FakeRegisterSpecialCase()			{ _asm { jmp[ddraw.RegisterSpecialCase] } }
__declspec(naked) void FakeReleaseDDThreadLock()			{ _asm { jmp[ddraw.ReleaseDDThreadLock] } }

__declspec(naked) void _DebugSetMute_d3d8() { _asm { jmp[d3d8.DebugSetMute_d3d8] } }
__declspec(naked) void _Direct3DCreate8() { _asm { jmp[d3d8.Direct3DCreate8] } }
__declspec(naked) void _ValidatePixelShader() { _asm { jmp[d3d8.ValidatePixelShader] } }
__declspec(naked) void _ValidateVertexShader() { _asm { jmp[d3d8.ValidateVertexShader] } }

__declspec(naked) void _D3DPERF_BeginEvent() { _asm { jmp[d3d9.D3DPERF_BeginEvent] } }
__declspec(naked) void _D3DPERF_EndEvent() { _asm { jmp[d3d9.D3DPERF_EndEvent] } }
__declspec(naked) void _D3DPERF_GetStatus() { _asm { jmp[d3d9.D3DPERF_GetStatus] } }
__declspec(naked) void _D3DPERF_QueryRepeatFrame() { _asm { jmp[d3d9.D3DPERF_QueryRepeatFrame] } }
__declspec(naked) void _D3DPERF_SetMarker() { _asm { jmp[d3d9.D3DPERF_SetMarker] } }
__declspec(naked) void _D3DPERF_SetOptions() { _asm { jmp[d3d9.D3DPERF_SetOptions] } }
__declspec(naked) void _D3DPERF_SetRegion() { _asm { jmp[d3d9.D3DPERF_SetRegion] } }
__declspec(naked) void _DebugSetLevel() { _asm { jmp[d3d9.DebugSetLevel] } }
__declspec(naked) void _DebugSetMute() { _asm { jmp[d3d9.DebugSetMute] } }
//__declspec(naked) void _Direct3D9EnableMaximizedWindowedModeShim() { _asm { jmp [d3d9.Direct3D9EnableMaximizedWindowedModeShim] } }
__declspec(naked) void _Direct3DCreate9() { _asm { jmp[d3d9.Direct3DCreate9] } }
__declspec(naked) void _Direct3DCreate9Ex() { _asm { jmp[d3d9.Direct3DCreate9Ex] } }
__declspec(naked) void _Direct3DShaderValidatorCreate9() { _asm { jmp[d3d9.Direct3DShaderValidatorCreate9] } }
__declspec(naked) void _PSGPError() { _asm { jmp[d3d9.PSGPError] } }
__declspec(naked) void _PSGPSampleTexture() { _asm { jmp[d3d9.PSGPSampleTexture] } }

__declspec(naked) void _D3D11CoreCreateDevice() { _asm { jmp[d3d11.D3D11CoreCreateDevice] } }
__declspec(naked) void _D3D11CoreCreateLayeredDevice() { _asm { jmp[d3d11.D3D11CoreCreateLayeredDevice] } }
__declspec(naked) void _D3D11CoreGetLayeredDeviceSize() { _asm { jmp[d3d11.D3D11CoreGetLayeredDeviceSize] } }
__declspec(naked) void _D3D11CoreRegisterLayers() { _asm { jmp[d3d11.D3D11CoreRegisterLayers] } }
__declspec(naked) void _D3D11CreateDevice() { _asm { jmp[d3d11.D3D11CreateDevice] } }
__declspec(naked) void _D3D11CreateDeviceAndSwapChain() { _asm { jmp[d3d11.D3D11CreateDeviceAndSwapChain] } }
__declspec(naked) void _D3DKMTCloseAdapter() { _asm { jmp[d3d11.D3DKMTCloseAdapter] } }
__declspec(naked) void _D3DKMTCreateAllocation() { _asm { jmp[d3d11.D3DKMTCreateAllocation] } }
__declspec(naked) void _D3DKMTCreateContext() { _asm { jmp[d3d11.D3DKMTCreateContext] } }
__declspec(naked) void _D3DKMTCreateDevice() { _asm { jmp[d3d11.D3DKMTCreateDevice] } }
__declspec(naked) void _D3DKMTCreateSynchronizationObject() { _asm { jmp[d3d11.D3DKMTCreateSynchronizationObject] } }
__declspec(naked) void _D3DKMTDestroyAllocation() { _asm { jmp[d3d11.D3DKMTDestroyAllocation] } }
__declspec(naked) void _D3DKMTDestroyContext() { _asm { jmp[d3d11.D3DKMTDestroyContext] } }
__declspec(naked) void _D3DKMTDestroyDevice() { _asm { jmp[d3d11.D3DKMTDestroyDevice] } }
__declspec(naked) void _D3DKMTDestroySynchronizationObject() { _asm { jmp[d3d11.D3DKMTDestroySynchronizationObject] } }
__declspec(naked) void _D3DKMTEscape() { _asm { jmp[d3d11.D3DKMTEscape] } }
__declspec(naked) void _D3DKMTGetContextSchedulingPriority() { _asm { jmp[d3d11.D3DKMTGetContextSchedulingPriority] } }
__declspec(naked) void _D3DKMTGetDeviceState() { _asm { jmp[d3d11.D3DKMTGetDeviceState] } }
__declspec(naked) void _D3DKMTGetDisplayModeList() { _asm { jmp[d3d11.D3DKMTGetDisplayModeList] } }
__declspec(naked) void _D3DKMTGetMultisampleMethodList() { _asm { jmp[d3d11.D3DKMTGetMultisampleMethodList] } }
__declspec(naked) void _D3DKMTGetRuntimeData() { _asm { jmp[d3d11.D3DKMTGetRuntimeData] } }
__declspec(naked) void _D3DKMTGetSharedPrimaryHandle() { _asm { jmp[d3d11.D3DKMTGetSharedPrimaryHandle] } }
__declspec(naked) void _D3DKMTLock() { _asm { jmp[d3d11.D3DKMTLock] } }
__declspec(naked) void _D3DKMTOpenAdapterFromHdc() { _asm { jmp[d3d11.D3DKMTOpenAdapterFromHdc] } }
__declspec(naked) void _D3DKMTOpenResource() { _asm { jmp[d3d11.D3DKMTOpenResource] } }
__declspec(naked) void _D3DKMTPresent() { _asm { jmp[d3d11.D3DKMTPresent] } }
__declspec(naked) void _D3DKMTQueryAdapterInfo() { _asm { jmp[d3d11.D3DKMTQueryAdapterInfo] } }
__declspec(naked) void _D3DKMTQueryAllocationResidency() { _asm { jmp[d3d11.D3DKMTQueryAllocationResidency] } }
__declspec(naked) void _D3DKMTQueryResourceInfo() { _asm { jmp[d3d11.D3DKMTQueryResourceInfo] } }
__declspec(naked) void _D3DKMTRender() { _asm { jmp[d3d11.D3DKMTRender] } }
__declspec(naked) void _D3DKMTSetAllocationPriority() { _asm { jmp[d3d11.D3DKMTSetAllocationPriority] } }
__declspec(naked) void _D3DKMTSetContextSchedulingPriority() { _asm { jmp[d3d11.D3DKMTSetContextSchedulingPriority] } }
__declspec(naked) void _D3DKMTSetDisplayMode() { _asm { jmp[d3d11.D3DKMTSetDisplayMode] } }
__declspec(naked) void _D3DKMTSetDisplayPrivateDriverFormat() { _asm { jmp[d3d11.D3DKMTSetDisplayPrivateDriverFormat] } }
__declspec(naked) void _D3DKMTSetGammaRamp() { _asm { jmp[d3d11.D3DKMTSetGammaRamp] } }
__declspec(naked) void _D3DKMTSetVidPnSourceOwner() { _asm { jmp[d3d11.D3DKMTSetVidPnSourceOwner] } }
__declspec(naked) void _D3DKMTSignalSynchronizationObject() { _asm { jmp[d3d11.D3DKMTSignalSynchronizationObject] } }
__declspec(naked) void _D3DKMTUnlock() { _asm { jmp[d3d11.D3DKMTUnlock] } }
__declspec(naked) void _D3DKMTWaitForSynchronizationObject() { _asm { jmp[d3d11.D3DKMTWaitForSynchronizationObject] } }
__declspec(naked) void _D3DKMTWaitForVerticalBlankEvent() { _asm { jmp[d3d11.D3DKMTWaitForVerticalBlankEvent] } }
__declspec(naked) void _D3DPerformance_BeginEvent() { _asm { jmp[d3d11.D3DPerformance_BeginEvent] } }
__declspec(naked) void _D3DPerformance_EndEvent() { _asm { jmp[d3d11.D3DPerformance_EndEvent] } }
__declspec(naked) void _D3DPerformance_GetStatus() { _asm { jmp[d3d11.D3DPerformance_GetStatus] } }
__declspec(naked) void _D3DPerformance_SetMarker() { _asm { jmp[d3d11.D3DPerformance_SetMarker] } }
__declspec(naked) void _EnableFeatureLevelUpgrade() { _asm { jmp[d3d11.EnableFeatureLevelUpgrade] } }
__declspec(naked) void _OpenAdapter10() { _asm { jmp[d3d11.OpenAdapter10] } }
__declspec(naked) void _OpenAdapter10_2() { _asm { jmp[d3d11.OpenAdapter10_2] } }

struct ExcludedEntry {
	char*			entry;
	ExcludedEntry*	prev;
	ExcludedEntry*	next;
};

struct ExcludedEntriesList {
	ExcludedEntry*	first;
	ExcludedEntry*	last;
};