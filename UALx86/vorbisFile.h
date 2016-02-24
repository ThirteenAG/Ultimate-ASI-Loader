#pragma once

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

struct winmmbase_dll
{
	HMODULE dll;
	FARPROC CloseDriver;
	FARPROC DefDriverProc;
	FARPROC DriverCallback;
	FARPROC DrvGetModuleHandle;
	FARPROC GetDriverModuleHandle;
	FARPROC OpenDriver;
	FARPROC SendDriverMessage;
	FARPROC auxGetDevCapsA;
	FARPROC auxGetDevCapsW;
	FARPROC auxGetNumDevs;
	FARPROC auxGetVolume;
	FARPROC auxOutMessage;
	FARPROC auxSetVolume;
	FARPROC joyConfigChanged;
	FARPROC joyGetDevCapsA;
	FARPROC joyGetDevCapsW;
	FARPROC joyGetNumDevs;
	FARPROC joyGetPos;
	FARPROC joyGetPosEx;
	FARPROC joyGetThreshold;
	FARPROC joyReleaseCapture;
	FARPROC joySetCapture;
	FARPROC joySetThreshold;
	FARPROC midiConnect;
	FARPROC midiDisconnect;
	FARPROC midiInAddBuffer;
	FARPROC midiInClose;
	FARPROC midiInGetDevCapsA;
	FARPROC midiInGetDevCapsW;
	FARPROC midiInGetErrorTextA;
	FARPROC midiInGetErrorTextW;
	FARPROC midiInGetID;
	FARPROC midiInGetNumDevs;
	FARPROC midiInMessage;
	FARPROC midiInOpen;
	FARPROC midiInPrepareHeader;
	FARPROC midiInReset;
	FARPROC midiInStart;
	FARPROC midiInStop;
	FARPROC midiInUnprepareHeader;
	FARPROC midiOutCacheDrumPatches;
	FARPROC midiOutCachePatches;
	FARPROC midiOutClose;
	FARPROC midiOutGetDevCapsA;
	FARPROC midiOutGetDevCapsW;
	FARPROC midiOutGetErrorTextA;
	FARPROC midiOutGetErrorTextW;
	FARPROC midiOutGetID;
	FARPROC midiOutGetNumDevs;
	FARPROC midiOutGetVolume;
	FARPROC midiOutLongMsg;
	FARPROC midiOutMessage;
	FARPROC midiOutOpen;
	FARPROC midiOutPrepareHeader;
	FARPROC midiOutReset;
	FARPROC midiOutSetVolume;
	FARPROC midiOutShortMsg;
	FARPROC midiOutUnprepareHeader;
	FARPROC midiStreamClose;
	FARPROC midiStreamOpen;
	FARPROC midiStreamOut;
	FARPROC midiStreamPause;
	FARPROC midiStreamPosition;
	FARPROC midiStreamProperty;
	FARPROC midiStreamRestart;
	FARPROC midiStreamStop;
	FARPROC mixerClose;
	FARPROC mixerGetControlDetailsA;
	FARPROC mixerGetControlDetailsW;
	FARPROC mixerGetDevCapsA;
	FARPROC mixerGetDevCapsW;
	FARPROC mixerGetID;
	FARPROC mixerGetLineControlsA;
	FARPROC mixerGetLineControlsW;
	FARPROC mixerGetLineInfoA;
	FARPROC mixerGetLineInfoW;
	FARPROC mixerGetNumDevs;
	FARPROC mixerMessage;
	FARPROC mixerOpen;
	FARPROC mixerSetControlDetails;
	FARPROC mmDrvInstall;
	FARPROC mmGetCurrentTask;
	FARPROC mmTaskBlock;
	FARPROC mmTaskCreate;
	FARPROC mmTaskSignal;
	FARPROC mmTaskYield;
	FARPROC mmioAdvance;
	FARPROC mmioAscend;
	FARPROC mmioClose;
	FARPROC mmioCreateChunk;
	FARPROC mmioDescend;
	FARPROC mmioFlush;
	FARPROC mmioGetInfo;
	FARPROC mmioInstallIOProcA;
	FARPROC mmioInstallIOProcW;
	FARPROC mmioOpenA;
	FARPROC mmioOpenW;
	FARPROC mmioRead;
	FARPROC mmioRenameA;
	FARPROC mmioRenameW;
	FARPROC mmioSeek;
	FARPROC mmioSendMessage;
	FARPROC mmioSetBuffer;
	FARPROC mmioSetInfo;
	FARPROC mmioStringToFOURCCA;
	FARPROC mmioStringToFOURCCW;
	FARPROC mmioWrite;
	FARPROC sndOpenSound;
	FARPROC waveInAddBuffer;
	FARPROC waveInClose;
	FARPROC waveInGetDevCapsA;
	FARPROC waveInGetDevCapsW;
	FARPROC waveInGetErrorTextA;
	FARPROC waveInGetErrorTextW;
	FARPROC waveInGetID;
	FARPROC waveInGetNumDevs;
	FARPROC waveInGetPosition;
	FARPROC waveInMessage;
	FARPROC waveInOpen;
	FARPROC waveInPrepareHeader;
	FARPROC waveInReset;
	FARPROC waveInStart;
	FARPROC waveInStop;
	FARPROC waveInUnprepareHeader;
	FARPROC waveOutBreakLoop;
	FARPROC waveOutClose;
	FARPROC waveOutGetDevCapsA;
	FARPROC waveOutGetDevCapsW;
	FARPROC waveOutGetErrorTextA;
	FARPROC waveOutGetErrorTextW;
	FARPROC waveOutGetID;
	FARPROC waveOutGetNumDevs;
	FARPROC waveOutGetPitch;
	FARPROC waveOutGetPlaybackRate;
	FARPROC waveOutGetPosition;
	FARPROC waveOutGetVolume;
	FARPROC waveOutMessage;
	FARPROC waveOutOpen;
	FARPROC waveOutPause;
	FARPROC waveOutPrepareHeader;
	FARPROC waveOutReset;
	FARPROC waveOutRestart;
	FARPROC waveOutSetPitch;
	FARPROC waveOutSetPlaybackRate;
	FARPROC waveOutSetVolume;
	FARPROC waveOutUnprepareHeader;
	FARPROC waveOutWrite;
	FARPROC winmmbaseFreeMMEHandles;
	FARPROC winmmbaseGetWOWHandle;
	FARPROC winmmbaseHandle32FromHandle16;
	FARPROC winmmbaseSetWOWHandle;
} winmmbase;

struct msacm32_dll
{
	HMODULE dll;
	FARPROC acmDriverAddA;
	FARPROC acmDriverAddW;
	FARPROC acmDriverClose;
	FARPROC acmDriverDetailsA;
	FARPROC acmDriverDetailsW;
	FARPROC acmDriverEnum;
	FARPROC acmDriverID;
	FARPROC acmDriverMessage;
	FARPROC acmDriverOpen;
	FARPROC acmDriverPriority;
	FARPROC acmDriverRemove;
	FARPROC acmFilterChooseA;
	FARPROC acmFilterChooseW;
	FARPROC acmFilterDetailsA;
	FARPROC acmFilterDetailsW;
	FARPROC acmFilterEnumA;
	FARPROC acmFilterEnumW;
	FARPROC acmFilterTagDetailsA;
	FARPROC acmFilterTagDetailsW;
	FARPROC acmFilterTagEnumA;
	FARPROC acmFilterTagEnumW;
	FARPROC acmFormatChooseA;
	FARPROC acmFormatChooseW;
	FARPROC acmFormatDetailsA;
	FARPROC acmFormatDetailsW;
	FARPROC acmFormatEnumA;
	FARPROC acmFormatEnumW;
	FARPROC acmFormatSuggest;
	FARPROC acmFormatTagDetailsA;
	FARPROC acmFormatTagDetailsW;
	FARPROC acmFormatTagEnumA;
	FARPROC acmFormatTagEnumW;
	FARPROC acmGetVersion;
	FARPROC acmMetrics;
	FARPROC acmStreamClose;
	FARPROC acmStreamConvert;
	FARPROC acmStreamMessage;
	FARPROC acmStreamOpen;
	FARPROC acmStreamPrepareHeader;
	FARPROC acmStreamReset;
	FARPROC acmStreamSize;
	FARPROC acmStreamUnprepareHeader;
} msacm32;

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

__declspec(naked) void FakeAcquireDDThreadLock() { _asm { jmp[ddraw.AcquireDDThreadLock] } }
__declspec(naked) void FakeCheckFullscreen() { _asm { jmp[ddraw.CheckFullscreen] } }
__declspec(naked) void FakeCompleteCreateSysmemSurface() { _asm { jmp[ddraw.CompleteCreateSysmemSurface] } }
__declspec(naked) void FakeD3DParseUnknownCommand() { _asm { jmp[ddraw.D3DParseUnknownCommand] } }
__declspec(naked) void FakeDDGetAttachedSurfaceLcl() { _asm { jmp[ddraw.DDGetAttachedSurfaceLcl] } }
__declspec(naked) void FakeDDInternalLock() { _asm { jmp[ddraw.DDInternalLock] } }
__declspec(naked) void FakeDDInternalUnlock() { _asm { jmp[ddraw.DDInternalUnlock] } }
__declspec(naked) void FakeDSoundHelp() { _asm { jmp[ddraw.DSoundHelp] } }
// HRESULT WINAPI DirectDrawCreate( GUID FAR *lpGUID, LPDIRECTDRAW FAR *lplpDD, IUnknown FAR *pUnkOuter );
__declspec(naked) void FakeDirectDrawCreate() { _asm { jmp[ddraw.DirectDrawCreate] } }
// HRESULT WINAPI DirectDrawCreateClipper( DWORD dwFlags, LPDIRECTDRAWCLIPPER FAR *lplpDDClipper, IUnknown FAR *pUnkOuter );
__declspec(naked) void FakeDirectDrawCreateClipper() { _asm { jmp[ddraw.DirectDrawCreateClipper] } }
// HRESULT WINAPI DirectDrawCreateEx( GUID FAR * lpGuid, LPVOID *lplpDD, REFIID iid,IUnknown FAR *pUnkOuter );
__declspec(naked) void FakeDirectDrawCreateEx() { _asm { jmp[ddraw.DirectDrawCreateEx] } }
// HRESULT WINAPI DirectDrawEnumerateA( LPDDENUMCALLBACKA lpCallback, LPVOID lpContext );
__declspec(naked) void FakeDirectDrawEnumerateA() { _asm { jmp[ddraw.DirectDrawEnumerateA] } }
// HRESULT WINAPI DirectDrawEnumerateExA( LPDDENUMCALLBACKEXA lpCallback, LPVOID lpContext, DWORD dwFlags );
__declspec(naked) void FakeDirectDrawEnumerateExA() { _asm { jmp[ddraw.DirectDrawEnumerateExA] } }
// HRESULT WINAPI DirectDrawEnumerateExW( LPDDENUMCALLBACKEXW lpCallback, LPVOID lpContext, DWORD dwFlags );
__declspec(naked) void FakeDirectDrawEnumerateExW() { _asm { jmp[ddraw.DirectDrawEnumerateExW] } }
// HRESULT WINAPI DirectDrawEnumerateW( LPDDENUMCALLBACKW lpCallback, LPVOID lpContext );
__declspec(naked) void FakeDirectDrawEnumerateW() { _asm { jmp[ddraw.DirectDrawEnumerateW] } }
__declspec(naked) void FakeDllCanUnloadNow() { _asm { jmp[ddraw.DllCanUnloadNow] } }
__declspec(naked) void FakeDllGetClassObject() { _asm { jmp[ddraw.DllGetClassObject] } }
__declspec(naked) void FakeGetDDSurfaceLocal() { _asm { jmp[ddraw.GetDDSurfaceLocal] } }
__declspec(naked) void FakeGetOLEThunkData() { _asm { jmp[ddraw.GetOLEThunkData] } }
__declspec(naked) void FakeGetSurfaceFromDC() { _asm { jmp[ddraw.GetSurfaceFromDC] } }
__declspec(naked) void FakeRegisterSpecialCase() { _asm { jmp[ddraw.RegisterSpecialCase] } }
__declspec(naked) void FakeReleaseDDThreadLock() { _asm { jmp[ddraw.ReleaseDDThreadLock] } }

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

__declspec(naked) void _CloseDriver() { _asm { jmp[winmmbase.CloseDriver] } }
__declspec(naked) void _DefDriverProc() { _asm { jmp[winmmbase.DefDriverProc] } }
__declspec(naked) void _DriverCallback() { _asm { jmp[winmmbase.DriverCallback] } }
__declspec(naked) void _DrvGetModuleHandle() { _asm { jmp[winmmbase.DrvGetModuleHandle] } }
__declspec(naked) void _GetDriverModuleHandle() { _asm { jmp[winmmbase.GetDriverModuleHandle] } }
__declspec(naked) void _OpenDriver() { _asm { jmp[winmmbase.OpenDriver] } }
__declspec(naked) void _SendDriverMessage() { _asm { jmp[winmmbase.SendDriverMessage] } }
__declspec(naked) void _auxGetDevCapsA() { _asm { jmp[winmmbase.auxGetDevCapsA] } }
__declspec(naked) void _auxGetDevCapsW() { _asm { jmp[winmmbase.auxGetDevCapsW] } }
__declspec(naked) void _auxGetNumDevs() { _asm { jmp[winmmbase.auxGetNumDevs] } }
__declspec(naked) void _auxGetVolume() { _asm { jmp[winmmbase.auxGetVolume] } }
__declspec(naked) void _auxOutMessage() { _asm { jmp[winmmbase.auxOutMessage] } }
__declspec(naked) void _auxSetVolume() { _asm { jmp[winmmbase.auxSetVolume] } }
__declspec(naked) void _joyConfigChanged() { _asm { jmp[winmmbase.joyConfigChanged] } }
__declspec(naked) void _joyGetDevCapsA() { _asm { jmp[winmmbase.joyGetDevCapsA] } }
__declspec(naked) void _joyGetDevCapsW() { _asm { jmp[winmmbase.joyGetDevCapsW] } }
__declspec(naked) void _joyGetNumDevs() { _asm { jmp[winmmbase.joyGetNumDevs] } }
__declspec(naked) void _joyGetPos() { _asm { jmp[winmmbase.joyGetPos] } }
__declspec(naked) void _joyGetPosEx() { _asm { jmp[winmmbase.joyGetPosEx] } }
__declspec(naked) void _joyGetThreshold() { _asm { jmp[winmmbase.joyGetThreshold] } }
__declspec(naked) void _joyReleaseCapture() { _asm { jmp[winmmbase.joyReleaseCapture] } }
__declspec(naked) void _joySetCapture() { _asm { jmp[winmmbase.joySetCapture] } }
__declspec(naked) void _joySetThreshold() { _asm { jmp[winmmbase.joySetThreshold] } }
__declspec(naked) void _midiConnect() { _asm { jmp[winmmbase.midiConnect] } }
__declspec(naked) void _midiDisconnect() { _asm { jmp[winmmbase.midiDisconnect] } }
__declspec(naked) void _midiInAddBuffer() { _asm { jmp[winmmbase.midiInAddBuffer] } }
__declspec(naked) void _midiInClose() { _asm { jmp[winmmbase.midiInClose] } }
__declspec(naked) void _midiInGetDevCapsA() { _asm { jmp[winmmbase.midiInGetDevCapsA] } }
__declspec(naked) void _midiInGetDevCapsW() { _asm { jmp[winmmbase.midiInGetDevCapsW] } }
__declspec(naked) void _midiInGetErrorTextA() { _asm { jmp[winmmbase.midiInGetErrorTextA] } }
__declspec(naked) void _midiInGetErrorTextW() { _asm { jmp[winmmbase.midiInGetErrorTextW] } }
__declspec(naked) void _midiInGetID() { _asm { jmp[winmmbase.midiInGetID] } }
__declspec(naked) void _midiInGetNumDevs() { _asm { jmp[winmmbase.midiInGetNumDevs] } }
__declspec(naked) void _midiInMessage() { _asm { jmp[winmmbase.midiInMessage] } }
__declspec(naked) void _midiInOpen() { _asm { jmp[winmmbase.midiInOpen] } }
__declspec(naked) void _midiInPrepareHeader() { _asm { jmp[winmmbase.midiInPrepareHeader] } }
__declspec(naked) void _midiInReset() { _asm { jmp[winmmbase.midiInReset] } }
__declspec(naked) void _midiInStart() { _asm { jmp[winmmbase.midiInStart] } }
__declspec(naked) void _midiInStop() { _asm { jmp[winmmbase.midiInStop] } }
__declspec(naked) void _midiInUnprepareHeader() { _asm { jmp[winmmbase.midiInUnprepareHeader] } }
__declspec(naked) void _midiOutCacheDrumPatches() { _asm { jmp[winmmbase.midiOutCacheDrumPatches] } }
__declspec(naked) void _midiOutCachePatches() { _asm { jmp[winmmbase.midiOutCachePatches] } }
__declspec(naked) void _midiOutClose() { _asm { jmp[winmmbase.midiOutClose] } }
__declspec(naked) void _midiOutGetDevCapsA() { _asm { jmp[winmmbase.midiOutGetDevCapsA] } }
__declspec(naked) void _midiOutGetDevCapsW() { _asm { jmp[winmmbase.midiOutGetDevCapsW] } }
__declspec(naked) void _midiOutGetErrorTextA() { _asm { jmp[winmmbase.midiOutGetErrorTextA] } }
__declspec(naked) void _midiOutGetErrorTextW() { _asm { jmp[winmmbase.midiOutGetErrorTextW] } }
__declspec(naked) void _midiOutGetID() { _asm { jmp[winmmbase.midiOutGetID] } }
__declspec(naked) void _midiOutGetNumDevs() { _asm { jmp[winmmbase.midiOutGetNumDevs] } }
__declspec(naked) void _midiOutGetVolume() { _asm { jmp[winmmbase.midiOutGetVolume] } }
__declspec(naked) void _midiOutLongMsg() { _asm { jmp[winmmbase.midiOutLongMsg] } }
__declspec(naked) void _midiOutMessage() { _asm { jmp[winmmbase.midiOutMessage] } }
__declspec(naked) void _midiOutOpen() { _asm { jmp[winmmbase.midiOutOpen] } }
__declspec(naked) void _midiOutPrepareHeader() { _asm { jmp[winmmbase.midiOutPrepareHeader] } }
__declspec(naked) void _midiOutReset() { _asm { jmp[winmmbase.midiOutReset] } }
__declspec(naked) void _midiOutSetVolume() { _asm { jmp[winmmbase.midiOutSetVolume] } }
__declspec(naked) void _midiOutShortMsg() { _asm { jmp[winmmbase.midiOutShortMsg] } }
__declspec(naked) void _midiOutUnprepareHeader() { _asm { jmp[winmmbase.midiOutUnprepareHeader] } }
__declspec(naked) void _midiStreamClose() { _asm { jmp[winmmbase.midiStreamClose] } }
__declspec(naked) void _midiStreamOpen() { _asm { jmp[winmmbase.midiStreamOpen] } }
__declspec(naked) void _midiStreamOut() { _asm { jmp[winmmbase.midiStreamOut] } }
__declspec(naked) void _midiStreamPause() { _asm { jmp[winmmbase.midiStreamPause] } }
__declspec(naked) void _midiStreamPosition() { _asm { jmp[winmmbase.midiStreamPosition] } }
__declspec(naked) void _midiStreamProperty() { _asm { jmp[winmmbase.midiStreamProperty] } }
__declspec(naked) void _midiStreamRestart() { _asm { jmp[winmmbase.midiStreamRestart] } }
__declspec(naked) void _midiStreamStop() { _asm { jmp[winmmbase.midiStreamStop] } }
__declspec(naked) void _mixerClose() { _asm { jmp[winmmbase.mixerClose] } }
__declspec(naked) void _mixerGetControlDetailsA() { _asm { jmp[winmmbase.mixerGetControlDetailsA] } }
__declspec(naked) void _mixerGetControlDetailsW() { _asm { jmp[winmmbase.mixerGetControlDetailsW] } }
__declspec(naked) void _mixerGetDevCapsA() { _asm { jmp[winmmbase.mixerGetDevCapsA] } }
__declspec(naked) void _mixerGetDevCapsW() { _asm { jmp[winmmbase.mixerGetDevCapsW] } }
__declspec(naked) void _mixerGetID() { _asm { jmp[winmmbase.mixerGetID] } }
__declspec(naked) void _mixerGetLineControlsA() { _asm { jmp[winmmbase.mixerGetLineControlsA] } }
__declspec(naked) void _mixerGetLineControlsW() { _asm { jmp[winmmbase.mixerGetLineControlsW] } }
__declspec(naked) void _mixerGetLineInfoA() { _asm { jmp[winmmbase.mixerGetLineInfoA] } }
__declspec(naked) void _mixerGetLineInfoW() { _asm { jmp[winmmbase.mixerGetLineInfoW] } }
__declspec(naked) void _mixerGetNumDevs() { _asm { jmp[winmmbase.mixerGetNumDevs] } }
__declspec(naked) void _mixerMessage() { _asm { jmp[winmmbase.mixerMessage] } }
__declspec(naked) void _mixerOpen() { _asm { jmp[winmmbase.mixerOpen] } }
__declspec(naked) void _mixerSetControlDetails() { _asm { jmp[winmmbase.mixerSetControlDetails] } }
__declspec(naked) void _mmDrvInstall() { _asm { jmp[winmmbase.mmDrvInstall] } }
__declspec(naked) void _mmGetCurrentTask() { _asm { jmp[winmmbase.mmGetCurrentTask] } }
__declspec(naked) void _mmTaskBlock() { _asm { jmp[winmmbase.mmTaskBlock] } }
__declspec(naked) void _mmTaskCreate() { _asm { jmp[winmmbase.mmTaskCreate] } }
__declspec(naked) void _mmTaskSignal() { _asm { jmp[winmmbase.mmTaskSignal] } }
__declspec(naked) void _mmTaskYield() { _asm { jmp[winmmbase.mmTaskYield] } }
__declspec(naked) void _mmioAdvance() { _asm { jmp[winmmbase.mmioAdvance] } }
__declspec(naked) void _mmioAscend() { _asm { jmp[winmmbase.mmioAscend] } }
__declspec(naked) void _mmioClose() { _asm { jmp[winmmbase.mmioClose] } }
__declspec(naked) void _mmioCreateChunk() { _asm { jmp[winmmbase.mmioCreateChunk] } }
__declspec(naked) void _mmioDescend() { _asm { jmp[winmmbase.mmioDescend] } }
__declspec(naked) void _mmioFlush() { _asm { jmp[winmmbase.mmioFlush] } }
__declspec(naked) void _mmioGetInfo() { _asm { jmp[winmmbase.mmioGetInfo] } }
__declspec(naked) void _mmioInstallIOProcA() { _asm { jmp[winmmbase.mmioInstallIOProcA] } }
__declspec(naked) void _mmioInstallIOProcW() { _asm { jmp[winmmbase.mmioInstallIOProcW] } }
__declspec(naked) void _mmioOpenA() { _asm { jmp[winmmbase.mmioOpenA] } }
__declspec(naked) void _mmioOpenW() { _asm { jmp[winmmbase.mmioOpenW] } }
__declspec(naked) void _mmioRead() { _asm { jmp[winmmbase.mmioRead] } }
__declspec(naked) void _mmioRenameA() { _asm { jmp[winmmbase.mmioRenameA] } }
__declspec(naked) void _mmioRenameW() { _asm { jmp[winmmbase.mmioRenameW] } }
__declspec(naked) void _mmioSeek() { _asm { jmp[winmmbase.mmioSeek] } }
__declspec(naked) void _mmioSendMessage() { _asm { jmp[winmmbase.mmioSendMessage] } }
__declspec(naked) void _mmioSetBuffer() { _asm { jmp[winmmbase.mmioSetBuffer] } }
__declspec(naked) void _mmioSetInfo() { _asm { jmp[winmmbase.mmioSetInfo] } }
__declspec(naked) void _mmioStringToFOURCCA() { _asm { jmp[winmmbase.mmioStringToFOURCCA] } }
__declspec(naked) void _mmioStringToFOURCCW() { _asm { jmp[winmmbase.mmioStringToFOURCCW] } }
__declspec(naked) void _mmioWrite() { _asm { jmp[winmmbase.mmioWrite] } }
__declspec(naked) void _sndOpenSound() { _asm { jmp[winmmbase.sndOpenSound] } }
__declspec(naked) void _waveInAddBuffer() { _asm { jmp[winmmbase.waveInAddBuffer] } }
__declspec(naked) void _waveInClose() { _asm { jmp[winmmbase.waveInClose] } }
__declspec(naked) void _waveInGetDevCapsA() { _asm { jmp[winmmbase.waveInGetDevCapsA] } }
__declspec(naked) void _waveInGetDevCapsW() { _asm { jmp[winmmbase.waveInGetDevCapsW] } }
__declspec(naked) void _waveInGetErrorTextA() { _asm { jmp[winmmbase.waveInGetErrorTextA] } }
__declspec(naked) void _waveInGetErrorTextW() { _asm { jmp[winmmbase.waveInGetErrorTextW] } }
__declspec(naked) void _waveInGetID() { _asm { jmp[winmmbase.waveInGetID] } }
__declspec(naked) void _waveInGetNumDevs() { _asm { jmp[winmmbase.waveInGetNumDevs] } }
__declspec(naked) void _waveInGetPosition() { _asm { jmp[winmmbase.waveInGetPosition] } }
__declspec(naked) void _waveInMessage() { _asm { jmp[winmmbase.waveInMessage] } }
__declspec(naked) void _waveInOpen() { _asm { jmp[winmmbase.waveInOpen] } }
__declspec(naked) void _waveInPrepareHeader() { _asm { jmp[winmmbase.waveInPrepareHeader] } }
__declspec(naked) void _waveInReset() { _asm { jmp[winmmbase.waveInReset] } }
__declspec(naked) void _waveInStart() { _asm { jmp[winmmbase.waveInStart] } }
__declspec(naked) void _waveInStop() { _asm { jmp[winmmbase.waveInStop] } }
__declspec(naked) void _waveInUnprepareHeader() { _asm { jmp[winmmbase.waveInUnprepareHeader] } }
__declspec(naked) void _waveOutBreakLoop() { _asm { jmp[winmmbase.waveOutBreakLoop] } }
__declspec(naked) void _waveOutClose() { _asm { jmp[winmmbase.waveOutClose] } }
__declspec(naked) void _waveOutGetDevCapsA() { _asm { jmp[winmmbase.waveOutGetDevCapsA] } }
__declspec(naked) void _waveOutGetDevCapsW() { _asm { jmp[winmmbase.waveOutGetDevCapsW] } }
__declspec(naked) void _waveOutGetErrorTextA() { _asm { jmp[winmmbase.waveOutGetErrorTextA] } }
__declspec(naked) void _waveOutGetErrorTextW() { _asm { jmp[winmmbase.waveOutGetErrorTextW] } }
__declspec(naked) void _waveOutGetID() { _asm { jmp[winmmbase.waveOutGetID] } }
__declspec(naked) void _waveOutGetNumDevs() { _asm { jmp[winmmbase.waveOutGetNumDevs] } }
__declspec(naked) void _waveOutGetPitch() { _asm { jmp[winmmbase.waveOutGetPitch] } }
__declspec(naked) void _waveOutGetPlaybackRate() { _asm { jmp[winmmbase.waveOutGetPlaybackRate] } }
__declspec(naked) void _waveOutGetPosition() { _asm { jmp[winmmbase.waveOutGetPosition] } }
__declspec(naked) void _waveOutGetVolume() { _asm { jmp[winmmbase.waveOutGetVolume] } }
__declspec(naked) void _waveOutMessage() { _asm { jmp[winmmbase.waveOutMessage] } }
__declspec(naked) void _waveOutOpen() { _asm { jmp[winmmbase.waveOutOpen] } }
__declspec(naked) void _waveOutPause() { _asm { jmp[winmmbase.waveOutPause] } }
__declspec(naked) void _waveOutPrepareHeader() { _asm { jmp[winmmbase.waveOutPrepareHeader] } }
__declspec(naked) void _waveOutReset() { _asm { jmp[winmmbase.waveOutReset] } }
__declspec(naked) void _waveOutRestart() { _asm { jmp[winmmbase.waveOutRestart] } }
__declspec(naked) void _waveOutSetPitch() { _asm { jmp[winmmbase.waveOutSetPitch] } }
__declspec(naked) void _waveOutSetPlaybackRate() { _asm { jmp[winmmbase.waveOutSetPlaybackRate] } }
__declspec(naked) void _waveOutSetVolume() { _asm { jmp[winmmbase.waveOutSetVolume] } }
__declspec(naked) void _waveOutUnprepareHeader() { _asm { jmp[winmmbase.waveOutUnprepareHeader] } }
__declspec(naked) void _waveOutWrite() { _asm { jmp[winmmbase.waveOutWrite] } }
__declspec(naked) void _winmmbaseFreeMMEHandles() { _asm { jmp[winmmbase.winmmbaseFreeMMEHandles] } }
__declspec(naked) void _winmmbaseGetWOWHandle() { _asm { jmp[winmmbase.winmmbaseGetWOWHandle] } }
__declspec(naked) void _winmmbaseHandle32FromHandle16() { _asm { jmp[winmmbase.winmmbaseHandle32FromHandle16] } }
__declspec(naked) void _winmmbaseSetWOWHandle() { _asm { jmp[winmmbase.winmmbaseSetWOWHandle] } }

__declspec(naked) void _acmDriverAddA() { _asm { jmp[msacm32.acmDriverAddA] } }
__declspec(naked) void _acmDriverAddW() { _asm { jmp[msacm32.acmDriverAddW] } }
__declspec(naked) void _acmDriverClose() { _asm { jmp[msacm32.acmDriverClose] } }
__declspec(naked) void _acmDriverDetailsA() { _asm { jmp[msacm32.acmDriverDetailsA] } }
__declspec(naked) void _acmDriverDetailsW() { _asm { jmp[msacm32.acmDriverDetailsW] } }
__declspec(naked) void _acmDriverEnum() { _asm { jmp[msacm32.acmDriverEnum] } }
__declspec(naked) void _acmDriverID() { _asm { jmp[msacm32.acmDriverID] } }
__declspec(naked) void _acmDriverMessage() { _asm { jmp[msacm32.acmDriverMessage] } }
__declspec(naked) void _acmDriverOpen() { _asm { jmp[msacm32.acmDriverOpen] } }
__declspec(naked) void _acmDriverPriority() { _asm { jmp[msacm32.acmDriverPriority] } }
__declspec(naked) void _acmDriverRemove() { _asm { jmp[msacm32.acmDriverRemove] } }
__declspec(naked) void _acmFilterChooseA() { _asm { jmp[msacm32.acmFilterChooseA] } }
__declspec(naked) void _acmFilterChooseW() { _asm { jmp[msacm32.acmFilterChooseW] } }
__declspec(naked) void _acmFilterDetailsA() { _asm { jmp[msacm32.acmFilterDetailsA] } }
__declspec(naked) void _acmFilterDetailsW() { _asm { jmp[msacm32.acmFilterDetailsW] } }
__declspec(naked) void _acmFilterEnumA() { _asm { jmp[msacm32.acmFilterEnumA] } }
__declspec(naked) void _acmFilterEnumW() { _asm { jmp[msacm32.acmFilterEnumW] } }
__declspec(naked) void _acmFilterTagDetailsA() { _asm { jmp[msacm32.acmFilterTagDetailsA] } }
__declspec(naked) void _acmFilterTagDetailsW() { _asm { jmp[msacm32.acmFilterTagDetailsW] } }
__declspec(naked) void _acmFilterTagEnumA() { _asm { jmp[msacm32.acmFilterTagEnumA] } }
__declspec(naked) void _acmFilterTagEnumW() { _asm { jmp[msacm32.acmFilterTagEnumW] } }
__declspec(naked) void _acmFormatChooseA() { _asm { jmp[msacm32.acmFormatChooseA] } }
__declspec(naked) void _acmFormatChooseW() { _asm { jmp[msacm32.acmFormatChooseW] } }
__declspec(naked) void _acmFormatDetailsA() { _asm { jmp[msacm32.acmFormatDetailsA] } }
__declspec(naked) void _acmFormatDetailsW() { _asm { jmp[msacm32.acmFormatDetailsW] } }
__declspec(naked) void _acmFormatEnumA() { _asm { jmp[msacm32.acmFormatEnumA] } }
__declspec(naked) void _acmFormatEnumW() { _asm { jmp[msacm32.acmFormatEnumW] } }
__declspec(naked) void _acmFormatSuggest() { _asm { jmp[msacm32.acmFormatSuggest] } }
__declspec(naked) void _acmFormatTagDetailsA() { _asm { jmp[msacm32.acmFormatTagDetailsA] } }
__declspec(naked) void _acmFormatTagDetailsW() { _asm { jmp[msacm32.acmFormatTagDetailsW] } }
__declspec(naked) void _acmFormatTagEnumA() { _asm { jmp[msacm32.acmFormatTagEnumA] } }
__declspec(naked) void _acmFormatTagEnumW() { _asm { jmp[msacm32.acmFormatTagEnumW] } }
__declspec(naked) void _acmGetVersion() { _asm { jmp[msacm32.acmGetVersion] } }
__declspec(naked) void _acmMetrics() { _asm { jmp[msacm32.acmMetrics] } }
__declspec(naked) void _acmStreamClose() { _asm { jmp[msacm32.acmStreamClose] } }
__declspec(naked) void _acmStreamConvert() { _asm { jmp[msacm32.acmStreamConvert] } }
__declspec(naked) void _acmStreamMessage() { _asm { jmp[msacm32.acmStreamMessage] } }
__declspec(naked) void _acmStreamOpen() { _asm { jmp[msacm32.acmStreamOpen] } }
__declspec(naked) void _acmStreamPrepareHeader() { _asm { jmp[msacm32.acmStreamPrepareHeader] } }
__declspec(naked) void _acmStreamReset() { _asm { jmp[msacm32.acmStreamReset] } }
__declspec(naked) void _acmStreamSize() { _asm { jmp[msacm32.acmStreamSize] } }
__declspec(naked) void _acmStreamUnprepareHeader() { _asm { jmp[msacm32.acmStreamUnprepareHeader] } }

struct ExcludedEntry {
	char*			entry;
	ExcludedEntry*	prev;
	ExcludedEntry*	next;
};

struct ExcludedEntriesList {
	ExcludedEntry*	first;
	ExcludedEntry*	last;
};

void ExcludedEntriesListInit(ExcludedEntriesList* list)
{
	list->first = NULL;
	list->last = NULL;
}

void ExcludedEntriesListPush(ExcludedEntriesList* list, const char* entryName)
{
	ExcludedEntry* 	newEntry = (ExcludedEntry*)malloc(sizeof(ExcludedEntry));
	size_t 			length = strlen(entryName) + 1;
	if (!list->first)
		list->first = newEntry;
	else
		list->last->next = newEntry;

	newEntry->prev = list->last;
	newEntry->next = NULL;
	list->last = newEntry;

	newEntry->entry = (char*)malloc(length);
	strncpy(newEntry->entry, entryName, length);
}

bool ExcludedEntriesListHasEntry(ExcludedEntriesList* list, const char* entryName)
{
	ExcludedEntry*	it = list->first;
	while (it)
	{
		if (!_stricmp(it->entry, entryName))
		{
			// It has an entry, we can pop it now
			if (it->next)
				it->next->prev = it->prev;
			if (it->prev)
				it->prev->next = it->next;

			if (list->first == it)
				list->first = it->next;

			free(it->entry);
			free(it);
			return true;
		}
		it = it->next;
	}

	return false;
}

void ExcludedEntriesListFree(ExcludedEntriesList* list)
{
	ExcludedEntry*	it = list->first;
	while (it)
	{
		ExcludedEntry* nextEntry = it->next;
		free(it->entry);
		free(it);
		it = nextEntry;
	}
}

void FindFiles(WIN32_FIND_DATA* fd, ExcludedEntriesList* list)
{
	HANDLE asiFile = FindFirstFile("*.asi", fd);
	if (asiFile != INVALID_HANDLE_VALUE)
	{

		do {
			if (!(fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {

				unsigned int pos = 5;
				while (fd->cFileName[pos])
					++pos;
				if (fd->cFileName[pos - 4] == '.' &&
					(fd->cFileName[pos - 3] == 'a' || fd->cFileName[pos - 3] == 'A') &&
					(fd->cFileName[pos - 2] == 's' || fd->cFileName[pos - 2] == 'S') &&
					(fd->cFileName[pos - 1] == 'i' || fd->cFileName[pos - 1] == 'I'))
				{
					if (!list || !ExcludedEntriesListHasEntry(list, fd->cFileName))
						LoadLibrary(fd->cFileName);
				}
			}

		} while (FindNextFile(asiFile, fd));
		FindClose(asiFile);
	}
}