#pragma once
#include <windows.h>
#include <Shlobj.h>
#if X64
#include <dsound.h>
#endif
#if !X64
#include <MemoryModule\MemoryModule.h>
#include <xliveless.h>
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

struct vorbisfile_dll
{
    HMEMORYMODULE dll;
    FARPROC ov_bitrate;
    FARPROC ov_bitrate_instant;
    FARPROC ov_clear;
    FARPROC ov_comment;
    FARPROC ov_crosslap;
    FARPROC ov_halfrate;
    FARPROC ov_halfrate_p;
    FARPROC ov_info;
    FARPROC ov_open;
    FARPROC ov_open_callbacks;
    FARPROC ov_pcm_seek;
    FARPROC ov_pcm_seek_lap;
    FARPROC ov_pcm_seek_page;
    FARPROC ov_pcm_seek_page_lap;
    FARPROC ov_pcm_tell;
    FARPROC ov_pcm_total;
    FARPROC ov_raw_seek;
    FARPROC ov_raw_seek_lap;
    FARPROC ov_raw_tell;
    FARPROC ov_raw_total;
    FARPROC ov_read;
    FARPROC ov_read_float;
    FARPROC ov_seekable;
    FARPROC ov_serialnumber;
    FARPROC ov_streams;
    FARPROC ov_test;
    FARPROC ov_test_callbacks;
    FARPROC ov_test_open;
    FARPROC ov_time_seek;
    FARPROC ov_time_seek_lap;
    FARPROC ov_time_seek_page;
    FARPROC ov_time_seek_page_lap;
    FARPROC ov_time_tell;
    FARPROC ov_time_total;

    void LoadOriginalLibrary(HMEMORYMODULE module)
    {
        dll = module;
        ov_bitrate = MemoryGetProcAddress(dll, "ov_bitrate");
        ov_bitrate_instant = MemoryGetProcAddress(dll, "ov_bitrate_instant");
        ov_clear = MemoryGetProcAddress(dll, "ov_clear");
        ov_comment = MemoryGetProcAddress(dll, "ov_comment");
        ov_crosslap = MemoryGetProcAddress(dll, "ov_crosslap");
        ov_halfrate = MemoryGetProcAddress(dll, "ov_halfrate");
        ov_halfrate_p = MemoryGetProcAddress(dll, "ov_halfrate_p");
        ov_info = MemoryGetProcAddress(dll, "ov_info");
        ov_open = MemoryGetProcAddress(dll, "ov_open");
        ov_open_callbacks = MemoryGetProcAddress(dll, "ov_open_callbacks");
        ov_pcm_seek = MemoryGetProcAddress(dll, "ov_pcm_seek");
        ov_pcm_seek_lap = MemoryGetProcAddress(dll, "ov_pcm_seek_lap");
        ov_pcm_seek_page = MemoryGetProcAddress(dll, "ov_pcm_seek_page");
        ov_pcm_seek_page_lap = MemoryGetProcAddress(dll, "ov_pcm_seek_page_lap");
        ov_pcm_tell = MemoryGetProcAddress(dll, "ov_pcm_tell");
        ov_pcm_total = MemoryGetProcAddress(dll, "ov_pcm_total");
        ov_raw_seek = MemoryGetProcAddress(dll, "ov_raw_seek");
        ov_raw_seek_lap = MemoryGetProcAddress(dll, "ov_raw_seek_lap");
        ov_raw_tell = MemoryGetProcAddress(dll, "ov_raw_tell");
        ov_raw_total = MemoryGetProcAddress(dll, "ov_raw_total");
        ov_read = MemoryGetProcAddress(dll, "ov_read");
        ov_read_float = MemoryGetProcAddress(dll, "ov_read_float");
        ov_seekable = MemoryGetProcAddress(dll, "ov_seekable");
        ov_serialnumber = MemoryGetProcAddress(dll, "ov_serialnumber");
        ov_streams = MemoryGetProcAddress(dll, "ov_streams");
        ov_test = MemoryGetProcAddress(dll, "ov_test");
        ov_test_callbacks = MemoryGetProcAddress(dll, "ov_test_callbacks");
        ov_test_open = MemoryGetProcAddress(dll, "ov_test_open");
        ov_time_seek = MemoryGetProcAddress(dll, "ov_time_seek");
        ov_time_seek_lap = MemoryGetProcAddress(dll, "ov_time_seek_lap");
        ov_time_seek_page = MemoryGetProcAddress(dll, "ov_time_seek_page");
        ov_time_seek_page_lap = MemoryGetProcAddress(dll, "ov_time_seek_page_lap");
        ov_time_tell = MemoryGetProcAddress(dll, "ov_time_tell");
        ov_time_total = MemoryGetProcAddress(dll, "ov_time_total");
    }
} vorbisfile;
#endif

struct dinput8_dll
{
    HMODULE dll;
    FARPROC DirectInput8Create;
    FARPROC DllCanUnloadNow;
    FARPROC DllGetClassObject;
    FARPROC DllRegisterServer;
    FARPROC DllUnregisterServer;

    void LoadOriginalLibrary(HMODULE module)
    {
        dll = module;
        DirectInput8Create = GetProcAddress(dll, "DirectInput8Create");
        DllCanUnloadNow = GetProcAddress(dll, "DllCanUnloadNow");
        DllGetClassObject = GetProcAddress(dll, "DllGetClassObject");
        DllRegisterServer = GetProcAddress(dll, "DllRegisterServer");
        DllUnregisterServer = GetProcAddress(dll, "DllUnregisterServer");
    }
} dinput8;

struct dinput_dll
{
    HMODULE dll;
    FARPROC DirectInputCreateA;
    FARPROC DirectInputCreateEx;
    FARPROC DirectInputCreateW;
    FARPROC DllCanUnloadNow;
    FARPROC DllGetClassObject;
    FARPROC DllRegisterServer;
    FARPROC DllUnregisterServer;

    void LoadOriginalLibrary(HMODULE module)
    {
        dll = module;
        DirectInputCreateA = GetProcAddress(dll, "DirectInputCreateA");
        DirectInputCreateEx = GetProcAddress(dll, "DirectInputCreateEx");
        DirectInputCreateW = GetProcAddress(dll, "DirectInputCreateW");
        DllCanUnloadNow = GetProcAddress(dll, "DllCanUnloadNow");
        DllGetClassObject = GetProcAddress(dll, "DllGetClassObject");
        DllRegisterServer = GetProcAddress(dll, "DllRegisterServer");
        DllUnregisterServer = GetProcAddress(dll, "DllUnregisterServer");
    }
} dinput;

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
    FARPROC DllCanUnloadNow;
    FARPROC DllGetClassObject;
    FARPROC GetDeviceID;

    void LoadOriginalLibrary(HMODULE module)
    {
        dll = module;
        DirectSoundCaptureCreate = GetProcAddress(dll, "DirectSoundCaptureCreate");
        DirectSoundCaptureCreate8 = GetProcAddress(dll, "DirectSoundCaptureCreate8");
        DirectSoundCaptureEnumerateA = GetProcAddress(dll, "DirectSoundCaptureEnumerateA");
        DirectSoundCaptureEnumerateW = GetProcAddress(dll, "DirectSoundCaptureEnumerateW");
        DirectSoundCreate = GetProcAddress(dll, "DirectSoundCreate");
        DirectSoundCreate8 = GetProcAddress(dll, "DirectSoundCreate8");
        DirectSoundEnumerateA = GetProcAddress(dll, "DirectSoundEnumerateA");
        DirectSoundEnumerateW = GetProcAddress(dll, "DirectSoundEnumerateW");
        DirectSoundFullDuplexCreate = GetProcAddress(dll, "DirectSoundFullDuplexCreate");
        DllCanUnloadNow = GetProcAddress(dll, "DllCanUnloadNow");
        DllGetClassObject = GetProcAddress(dll, "DllGetClassObject");
        GetDeviceID = GetProcAddress(dll, "GetDeviceID");
    }
} dsound;

#if !X64
struct d3d8_dll
{
    HMODULE dll;
    FARPROC DebugSetMute;
    FARPROC Direct3D8EnableMaximizedWindowedModeShim;
    FARPROC Direct3DCreate8;
    FARPROC ValidatePixelShader;
    FARPROC ValidateVertexShader;

    void LoadOriginalLibrary(HMODULE module)
    {
        dll = module;
        DebugSetMute = GetProcAddress(dll, "DebugSetMute");
        Direct3D8EnableMaximizedWindowedModeShim = GetProcAddress(dll, "Direct3D8EnableMaximizedWindowedModeShim");
        Direct3DCreate8 = GetProcAddress(dll, "Direct3DCreate8");
        ValidatePixelShader = GetProcAddress(dll, "ValidatePixelShader");
        ValidateVertexShader = GetProcAddress(dll, "ValidateVertexShader");
    }
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
    FARPROC Direct3D9EnableMaximizedWindowedModeShim;
    FARPROC Direct3DCreate9;
    FARPROC Direct3DCreate9Ex;
    FARPROC Direct3DShaderValidatorCreate9;
    FARPROC PSGPError;
    FARPROC PSGPSampleTexture;

    void LoadOriginalLibrary(HMODULE module)
    {
        dll = module;
        D3DPERF_BeginEvent = GetProcAddress(dll, "D3DPERF_BeginEvent");
        D3DPERF_EndEvent = GetProcAddress(dll, "D3DPERF_EndEvent");
        D3DPERF_GetStatus = GetProcAddress(dll, "D3DPERF_GetStatus");
        D3DPERF_QueryRepeatFrame = GetProcAddress(dll, "D3DPERF_QueryRepeatFrame");
        D3DPERF_SetMarker = GetProcAddress(dll, "D3DPERF_SetMarker");
        D3DPERF_SetOptions = GetProcAddress(dll, "D3DPERF_SetOptions");
        D3DPERF_SetRegion = GetProcAddress(dll, "D3DPERF_SetRegion");
        DebugSetLevel = GetProcAddress(dll, "DebugSetLevel");
        DebugSetMute = GetProcAddress(dll, "DebugSetMute");
        Direct3D9EnableMaximizedWindowedModeShim = GetProcAddress(dll, "Direct3D9EnableMaximizedWindowedModeShim");
        Direct3DCreate9 = GetProcAddress(dll, "Direct3DCreate9");
        Direct3DCreate9Ex = GetProcAddress(dll, "Direct3DCreate9Ex");
        Direct3DShaderValidatorCreate9 = GetProcAddress(dll, "Direct3DShaderValidatorCreate9");
        PSGPError = GetProcAddress(dll, "PSGPError");
        PSGPSampleTexture = GetProcAddress(dll, "PSGPSampleTexture");
    }
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

    void LoadOriginalLibrary(HMODULE module)
    {
        dll = module;
        D3D11CoreCreateDevice = GetProcAddress(dll, "D3D11CoreCreateDevice");
        D3D11CoreCreateLayeredDevice = GetProcAddress(dll, "D3D11CoreCreateLayeredDevice");
        D3D11CoreGetLayeredDeviceSize = GetProcAddress(dll, "D3D11CoreGetLayeredDeviceSize");
        D3D11CoreRegisterLayers = GetProcAddress(dll, "D3D11CoreRegisterLayers");
        D3D11CreateDevice = GetProcAddress(dll, "D3D11CreateDevice");
        D3D11CreateDeviceAndSwapChain = GetProcAddress(dll, "D3D11CreateDeviceAndSwapChain");
        D3DKMTCloseAdapter = GetProcAddress(dll, "D3DKMTCloseAdapter");
        D3DKMTCreateAllocation = GetProcAddress(dll, "D3DKMTCreateAllocation");
        D3DKMTCreateContext = GetProcAddress(dll, "D3DKMTCreateContext");
        D3DKMTCreateDevice = GetProcAddress(dll, "D3DKMTCreateDevice");
        D3DKMTCreateSynchronizationObject = GetProcAddress(dll, "D3DKMTCreateSynchronizationObject");
        D3DKMTDestroyAllocation = GetProcAddress(dll, "D3DKMTDestroyAllocation");
        D3DKMTDestroyContext = GetProcAddress(dll, "D3DKMTDestroyContext");
        D3DKMTDestroyDevice = GetProcAddress(dll, "D3DKMTDestroyDevice");
        D3DKMTDestroySynchronizationObject = GetProcAddress(dll, "D3DKMTDestroySynchronizationObject");
        D3DKMTEscape = GetProcAddress(dll, "D3DKMTEscape");
        D3DKMTGetContextSchedulingPriority = GetProcAddress(dll, "D3DKMTGetContextSchedulingPriority");
        D3DKMTGetDeviceState = GetProcAddress(dll, "D3DKMTGetDeviceState");
        D3DKMTGetDisplayModeList = GetProcAddress(dll, "D3DKMTGetDisplayModeList");
        D3DKMTGetMultisampleMethodList = GetProcAddress(dll, "D3DKMTGetMultisampleMethodList");
        D3DKMTGetRuntimeData = GetProcAddress(dll, "D3DKMTGetRuntimeData");
        D3DKMTGetSharedPrimaryHandle = GetProcAddress(dll, "D3DKMTGetSharedPrimaryHandle");
        D3DKMTLock = GetProcAddress(dll, "D3DKMTLock");
        D3DKMTOpenAdapterFromHdc = GetProcAddress(dll, "D3DKMTOpenAdapterFromHdc");
        D3DKMTOpenResource = GetProcAddress(dll, "D3DKMTOpenResource");
        D3DKMTPresent = GetProcAddress(dll, "D3DKMTPresent");
        D3DKMTQueryAdapterInfo = GetProcAddress(dll, "D3DKMTQueryAdapterInfo");
        D3DKMTQueryAllocationResidency = GetProcAddress(dll, "D3DKMTQueryAllocationResidency");
        D3DKMTQueryResourceInfo = GetProcAddress(dll, "D3DKMTQueryResourceInfo");
        D3DKMTRender = GetProcAddress(dll, "D3DKMTRender");
        D3DKMTSetAllocationPriority = GetProcAddress(dll, "D3DKMTSetAllocationPriority");
        D3DKMTSetContextSchedulingPriority = GetProcAddress(dll, "D3DKMTSetContextSchedulingPriority");
        D3DKMTSetDisplayMode = GetProcAddress(dll, "D3DKMTSetDisplayMode");
        D3DKMTSetDisplayPrivateDriverFormat = GetProcAddress(dll, "D3DKMTSetDisplayPrivateDriverFormat");
        D3DKMTSetGammaRamp = GetProcAddress(dll, "D3DKMTSetGammaRamp");
        D3DKMTSetVidPnSourceOwner = GetProcAddress(dll, "D3DKMTSetVidPnSourceOwner");
        D3DKMTSignalSynchronizationObject = GetProcAddress(dll, "D3DKMTSignalSynchronizationObject");
        D3DKMTUnlock = GetProcAddress(dll, "D3DKMTUnlock");
        D3DKMTWaitForSynchronizationObject = GetProcAddress(dll, "D3DKMTWaitForSynchronizationObject");
        D3DKMTWaitForVerticalBlankEvent = GetProcAddress(dll, "D3DKMTWaitForVerticalBlankEvent");
        D3DPerformance_BeginEvent = GetProcAddress(dll, "D3DPerformance_BeginEvent");
        D3DPerformance_EndEvent = GetProcAddress(dll, "D3DPerformance_EndEvent");
        D3DPerformance_GetStatus = GetProcAddress(dll, "D3DPerformance_GetStatus");
        D3DPerformance_SetMarker = GetProcAddress(dll, "D3DPerformance_SetMarker");
        EnableFeatureLevelUpgrade = GetProcAddress(dll, "EnableFeatureLevelUpgrade");
        OpenAdapter10 = GetProcAddress(dll, "OpenAdapter10");
        OpenAdapter10_2 = GetProcAddress(dll, "OpenAdapter10_2");
    }
} d3d11;

struct ddraw_dll
{
    HMODULE dll;
    FARPROC AcquireDDThreadLock;
    FARPROC CompleteCreateSysmemSurface;
    FARPROC D3DParseUnknownCommand;
    FARPROC DDGetAttachedSurfaceLcl;
    FARPROC DDInternalLock;
    FARPROC DDInternalUnlock;
    FARPROC DSoundHelp;
    FARPROC DirectDrawCreate;
    FARPROC DirectDrawCreateClipper;
    FARPROC DirectDrawCreateEx;
    FARPROC DirectDrawEnumerateA;
    FARPROC DirectDrawEnumerateExA;
    FARPROC DirectDrawEnumerateExW;
    FARPROC DirectDrawEnumerateW;
    FARPROC DllCanUnloadNow;
    FARPROC DllGetClassObject;
    FARPROC GetDDSurfaceLocal;
    FARPROC GetOLEThunkData;
    FARPROC GetSurfaceFromDC;
    FARPROC RegisterSpecialCase;
    FARPROC ReleaseDDThreadLock;
    FARPROC SetAppCompatData;

    void LoadOriginalLibrary(HMODULE module)
    {
        dll = module;
        AcquireDDThreadLock = GetProcAddress(dll, "AcquireDDThreadLock");
        CompleteCreateSysmemSurface = GetProcAddress(dll, "CompleteCreateSysmemSurface");
        D3DParseUnknownCommand = GetProcAddress(dll, "D3DParseUnknownCommand");
        DDGetAttachedSurfaceLcl = GetProcAddress(dll, "DDGetAttachedSurfaceLcl");
        DDInternalLock = GetProcAddress(dll, "DDInternalLock");
        DDInternalUnlock = GetProcAddress(dll, "DDInternalUnlock");
        DSoundHelp = GetProcAddress(dll, "DSoundHelp");
        DirectDrawCreate = GetProcAddress(dll, "DirectDrawCreate");
        DirectDrawCreateClipper = GetProcAddress(dll, "DirectDrawCreateClipper");
        DirectDrawCreateEx = GetProcAddress(dll, "DirectDrawCreateEx");
        DirectDrawEnumerateA = GetProcAddress(dll, "DirectDrawEnumerateA");
        DirectDrawEnumerateExA = GetProcAddress(dll, "DirectDrawEnumerateExA");
        DirectDrawEnumerateExW = GetProcAddress(dll, "DirectDrawEnumerateExW");
        DirectDrawEnumerateW = GetProcAddress(dll, "DirectDrawEnumerateW");
        DllCanUnloadNow = GetProcAddress(dll, "DllCanUnloadNow");
        DllGetClassObject = GetProcAddress(dll, "DllGetClassObject");
        GetDDSurfaceLocal = GetProcAddress(dll, "GetDDSurfaceLocal");
        GetOLEThunkData = GetProcAddress(dll, "GetOLEThunkData");
        GetSurfaceFromDC = GetProcAddress(dll, "GetSurfaceFromDC");
        RegisterSpecialCase = GetProcAddress(dll, "RegisterSpecialCase");
        ReleaseDDThreadLock = GetProcAddress(dll, "ReleaseDDThreadLock");
        SetAppCompatData = GetProcAddress(dll, "SetAppCompatData");
    }
} ddraw;

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

    void LoadOriginalLibrary(HMODULE module)
    {
        dll = module;
        CloseDriver = GetProcAddress(dll, "CloseDriver");
        DefDriverProc = GetProcAddress(dll, "DefDriverProc");
        DriverCallback = GetProcAddress(dll, "DriverCallback");
        DrvGetModuleHandle = GetProcAddress(dll, "DrvGetModuleHandle");
        GetDriverModuleHandle = GetProcAddress(dll, "GetDriverModuleHandle");
        OpenDriver = GetProcAddress(dll, "OpenDriver");
        SendDriverMessage = GetProcAddress(dll, "SendDriverMessage");
        auxGetDevCapsA = GetProcAddress(dll, "auxGetDevCapsA");
        auxGetDevCapsW = GetProcAddress(dll, "auxGetDevCapsW");
        auxGetNumDevs = GetProcAddress(dll, "auxGetNumDevs");
        auxGetVolume = GetProcAddress(dll, "auxGetVolume");
        auxOutMessage = GetProcAddress(dll, "auxOutMessage");
        auxSetVolume = GetProcAddress(dll, "auxSetVolume");
        joyConfigChanged = GetProcAddress(dll, "joyConfigChanged");
        joyGetDevCapsA = GetProcAddress(dll, "joyGetDevCapsA");
        joyGetDevCapsW = GetProcAddress(dll, "joyGetDevCapsW");
        joyGetNumDevs = GetProcAddress(dll, "joyGetNumDevs");
        joyGetPos = GetProcAddress(dll, "joyGetPos");
        joyGetPosEx = GetProcAddress(dll, "joyGetPosEx");
        joyGetThreshold = GetProcAddress(dll, "joyGetThreshold");
        joyReleaseCapture = GetProcAddress(dll, "joyReleaseCapture");
        joySetCapture = GetProcAddress(dll, "joySetCapture");
        joySetThreshold = GetProcAddress(dll, "joySetThreshold");
        midiConnect = GetProcAddress(dll, "midiConnect");
        midiDisconnect = GetProcAddress(dll, "midiDisconnect");
        midiInAddBuffer = GetProcAddress(dll, "midiInAddBuffer");
        midiInClose = GetProcAddress(dll, "midiInClose");
        midiInGetDevCapsA = GetProcAddress(dll, "midiInGetDevCapsA");
        midiInGetDevCapsW = GetProcAddress(dll, "midiInGetDevCapsW");
        midiInGetErrorTextA = GetProcAddress(dll, "midiInGetErrorTextA");
        midiInGetErrorTextW = GetProcAddress(dll, "midiInGetErrorTextW");
        midiInGetID = GetProcAddress(dll, "midiInGetID");
        midiInGetNumDevs = GetProcAddress(dll, "midiInGetNumDevs");
        midiInMessage = GetProcAddress(dll, "midiInMessage");
        midiInOpen = GetProcAddress(dll, "midiInOpen");
        midiInPrepareHeader = GetProcAddress(dll, "midiInPrepareHeader");
        midiInReset = GetProcAddress(dll, "midiInReset");
        midiInStart = GetProcAddress(dll, "midiInStart");
        midiInStop = GetProcAddress(dll, "midiInStop");
        midiInUnprepareHeader = GetProcAddress(dll, "midiInUnprepareHeader");
        midiOutCacheDrumPatches = GetProcAddress(dll, "midiOutCacheDrumPatches");
        midiOutCachePatches = GetProcAddress(dll, "midiOutCachePatches");
        midiOutClose = GetProcAddress(dll, "midiOutClose");
        midiOutGetDevCapsA = GetProcAddress(dll, "midiOutGetDevCapsA");
        midiOutGetDevCapsW = GetProcAddress(dll, "midiOutGetDevCapsW");
        midiOutGetErrorTextA = GetProcAddress(dll, "midiOutGetErrorTextA");
        midiOutGetErrorTextW = GetProcAddress(dll, "midiOutGetErrorTextW");
        midiOutGetID = GetProcAddress(dll, "midiOutGetID");
        midiOutGetNumDevs = GetProcAddress(dll, "midiOutGetNumDevs");
        midiOutGetVolume = GetProcAddress(dll, "midiOutGetVolume");
        midiOutLongMsg = GetProcAddress(dll, "midiOutLongMsg");
        midiOutMessage = GetProcAddress(dll, "midiOutMessage");
        midiOutOpen = GetProcAddress(dll, "midiOutOpen");
        midiOutPrepareHeader = GetProcAddress(dll, "midiOutPrepareHeader");
        midiOutReset = GetProcAddress(dll, "midiOutReset");
        midiOutSetVolume = GetProcAddress(dll, "midiOutSetVolume");
        midiOutShortMsg = GetProcAddress(dll, "midiOutShortMsg");
        midiOutUnprepareHeader = GetProcAddress(dll, "midiOutUnprepareHeader");
        midiStreamClose = GetProcAddress(dll, "midiStreamClose");
        midiStreamOpen = GetProcAddress(dll, "midiStreamOpen");
        midiStreamOut = GetProcAddress(dll, "midiStreamOut");
        midiStreamPause = GetProcAddress(dll, "midiStreamPause");
        midiStreamPosition = GetProcAddress(dll, "midiStreamPosition");
        midiStreamProperty = GetProcAddress(dll, "midiStreamProperty");
        midiStreamRestart = GetProcAddress(dll, "midiStreamRestart");
        midiStreamStop = GetProcAddress(dll, "midiStreamStop");
        mixerClose = GetProcAddress(dll, "mixerClose");
        mixerGetControlDetailsA = GetProcAddress(dll, "mixerGetControlDetailsA");
        mixerGetControlDetailsW = GetProcAddress(dll, "mixerGetControlDetailsW");
        mixerGetDevCapsA = GetProcAddress(dll, "mixerGetDevCapsA");
        mixerGetDevCapsW = GetProcAddress(dll, "mixerGetDevCapsW");
        mixerGetID = GetProcAddress(dll, "mixerGetID");
        mixerGetLineControlsA = GetProcAddress(dll, "mixerGetLineControlsA");
        mixerGetLineControlsW = GetProcAddress(dll, "mixerGetLineControlsW");
        mixerGetLineInfoA = GetProcAddress(dll, "mixerGetLineInfoA");
        mixerGetLineInfoW = GetProcAddress(dll, "mixerGetLineInfoW");
        mixerGetNumDevs = GetProcAddress(dll, "mixerGetNumDevs");
        mixerMessage = GetProcAddress(dll, "mixerMessage");
        mixerOpen = GetProcAddress(dll, "mixerOpen");
        mixerSetControlDetails = GetProcAddress(dll, "mixerSetControlDetails");
        mmDrvInstall = GetProcAddress(dll, "mmDrvInstall");
        mmGetCurrentTask = GetProcAddress(dll, "mmGetCurrentTask");
        mmTaskBlock = GetProcAddress(dll, "mmTaskBlock");
        mmTaskCreate = GetProcAddress(dll, "mmTaskCreate");
        mmTaskSignal = GetProcAddress(dll, "mmTaskSignal");
        mmTaskYield = GetProcAddress(dll, "mmTaskYield");
        mmioAdvance = GetProcAddress(dll, "mmioAdvance");
        mmioAscend = GetProcAddress(dll, "mmioAscend");
        mmioClose = GetProcAddress(dll, "mmioClose");
        mmioCreateChunk = GetProcAddress(dll, "mmioCreateChunk");
        mmioDescend = GetProcAddress(dll, "mmioDescend");
        mmioFlush = GetProcAddress(dll, "mmioFlush");
        mmioGetInfo = GetProcAddress(dll, "mmioGetInfo");
        mmioInstallIOProcA = GetProcAddress(dll, "mmioInstallIOProcA");
        mmioInstallIOProcW = GetProcAddress(dll, "mmioInstallIOProcW");
        mmioOpenA = GetProcAddress(dll, "mmioOpenA");
        mmioOpenW = GetProcAddress(dll, "mmioOpenW");
        mmioRead = GetProcAddress(dll, "mmioRead");
        mmioRenameA = GetProcAddress(dll, "mmioRenameA");
        mmioRenameW = GetProcAddress(dll, "mmioRenameW");
        mmioSeek = GetProcAddress(dll, "mmioSeek");
        mmioSendMessage = GetProcAddress(dll, "mmioSendMessage");
        mmioSetBuffer = GetProcAddress(dll, "mmioSetBuffer");
        mmioSetInfo = GetProcAddress(dll, "mmioSetInfo");
        mmioStringToFOURCCA = GetProcAddress(dll, "mmioStringToFOURCCA");
        mmioStringToFOURCCW = GetProcAddress(dll, "mmioStringToFOURCCW");
        mmioWrite = GetProcAddress(dll, "mmioWrite");
        sndOpenSound = GetProcAddress(dll, "sndOpenSound");
        waveInAddBuffer = GetProcAddress(dll, "waveInAddBuffer");
        waveInClose = GetProcAddress(dll, "waveInClose");
        waveInGetDevCapsA = GetProcAddress(dll, "waveInGetDevCapsA");
        waveInGetDevCapsW = GetProcAddress(dll, "waveInGetDevCapsW");
        waveInGetErrorTextA = GetProcAddress(dll, "waveInGetErrorTextA");
        waveInGetErrorTextW = GetProcAddress(dll, "waveInGetErrorTextW");
        waveInGetID = GetProcAddress(dll, "waveInGetID");
        waveInGetNumDevs = GetProcAddress(dll, "waveInGetNumDevs");
        waveInGetPosition = GetProcAddress(dll, "waveInGetPosition");
        waveInMessage = GetProcAddress(dll, "waveInMessage");
        waveInOpen = GetProcAddress(dll, "waveInOpen");
        waveInPrepareHeader = GetProcAddress(dll, "waveInPrepareHeader");
        waveInReset = GetProcAddress(dll, "waveInReset");
        waveInStart = GetProcAddress(dll, "waveInStart");
        waveInStop = GetProcAddress(dll, "waveInStop");
        waveInUnprepareHeader = GetProcAddress(dll, "waveInUnprepareHeader");
        waveOutBreakLoop = GetProcAddress(dll, "waveOutBreakLoop");
        waveOutClose = GetProcAddress(dll, "waveOutClose");
        waveOutGetDevCapsA = GetProcAddress(dll, "waveOutGetDevCapsA");
        waveOutGetDevCapsW = GetProcAddress(dll, "waveOutGetDevCapsW");
        waveOutGetErrorTextA = GetProcAddress(dll, "waveOutGetErrorTextA");
        waveOutGetErrorTextW = GetProcAddress(dll, "waveOutGetErrorTextW");
        waveOutGetID = GetProcAddress(dll, "waveOutGetID");
        waveOutGetNumDevs = GetProcAddress(dll, "waveOutGetNumDevs");
        waveOutGetPitch = GetProcAddress(dll, "waveOutGetPitch");
        waveOutGetPlaybackRate = GetProcAddress(dll, "waveOutGetPlaybackRate");
        waveOutGetPosition = GetProcAddress(dll, "waveOutGetPosition");
        waveOutGetVolume = GetProcAddress(dll, "waveOutGetVolume");
        waveOutMessage = GetProcAddress(dll, "waveOutMessage");
        waveOutOpen = GetProcAddress(dll, "waveOutOpen");
        waveOutPause = GetProcAddress(dll, "waveOutPause");
        waveOutPrepareHeader = GetProcAddress(dll, "waveOutPrepareHeader");
        waveOutReset = GetProcAddress(dll, "waveOutReset");
        waveOutRestart = GetProcAddress(dll, "waveOutRestart");
        waveOutSetPitch = GetProcAddress(dll, "waveOutSetPitch");
        waveOutSetPlaybackRate = GetProcAddress(dll, "waveOutSetPlaybackRate");
        waveOutSetVolume = GetProcAddress(dll, "waveOutSetVolume");
        waveOutUnprepareHeader = GetProcAddress(dll, "waveOutUnprepareHeader");
        waveOutWrite = GetProcAddress(dll, "waveOutWrite");
        winmmbaseFreeMMEHandles = GetProcAddress(dll, "winmmbaseFreeMMEHandles");
        winmmbaseGetWOWHandle = GetProcAddress(dll, "winmmbaseGetWOWHandle");
        winmmbaseHandle32FromHandle16 = GetProcAddress(dll, "winmmbaseHandle32FromHandle16");
        winmmbaseSetWOWHandle = GetProcAddress(dll, "winmmbaseSetWOWHandle");
    }
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

    void LoadOriginalLibrary(HMODULE module)
    {
        dll = module;
        acmDriverAddA = GetProcAddress(dll, "acmDriverAddA");
        acmDriverAddW = GetProcAddress(dll, "acmDriverAddW");
        acmDriverClose = GetProcAddress(dll, "acmDriverClose");
        acmDriverDetailsA = GetProcAddress(dll, "acmDriverDetailsA");
        acmDriverDetailsW = GetProcAddress(dll, "acmDriverDetailsW");
        acmDriverEnum = GetProcAddress(dll, "acmDriverEnum");
        acmDriverID = GetProcAddress(dll, "acmDriverID");
        acmDriverMessage = GetProcAddress(dll, "acmDriverMessage");
        acmDriverOpen = GetProcAddress(dll, "acmDriverOpen");
        acmDriverPriority = GetProcAddress(dll, "acmDriverPriority");
        acmDriverRemove = GetProcAddress(dll, "acmDriverRemove");
        acmFilterChooseA = GetProcAddress(dll, "acmFilterChooseA");
        acmFilterChooseW = GetProcAddress(dll, "acmFilterChooseW");
        acmFilterDetailsA = GetProcAddress(dll, "acmFilterDetailsA");
        acmFilterDetailsW = GetProcAddress(dll, "acmFilterDetailsW");
        acmFilterEnumA = GetProcAddress(dll, "acmFilterEnumA");
        acmFilterEnumW = GetProcAddress(dll, "acmFilterEnumW");
        acmFilterTagDetailsA = GetProcAddress(dll, "acmFilterTagDetailsA");
        acmFilterTagDetailsW = GetProcAddress(dll, "acmFilterTagDetailsW");
        acmFilterTagEnumA = GetProcAddress(dll, "acmFilterTagEnumA");
        acmFilterTagEnumW = GetProcAddress(dll, "acmFilterTagEnumW");
        acmFormatChooseA = GetProcAddress(dll, "acmFormatChooseA");
        acmFormatChooseW = GetProcAddress(dll, "acmFormatChooseW");
        acmFormatDetailsA = GetProcAddress(dll, "acmFormatDetailsA");
        acmFormatDetailsW = GetProcAddress(dll, "acmFormatDetailsW");
        acmFormatEnumA = GetProcAddress(dll, "acmFormatEnumA");
        acmFormatEnumW = GetProcAddress(dll, "acmFormatEnumW");
        acmFormatSuggest = GetProcAddress(dll, "acmFormatSuggest");
        acmFormatTagDetailsA = GetProcAddress(dll, "acmFormatTagDetailsA");
        acmFormatTagDetailsW = GetProcAddress(dll, "acmFormatTagDetailsW");
        acmFormatTagEnumA = GetProcAddress(dll, "acmFormatTagEnumA");
        acmFormatTagEnumW = GetProcAddress(dll, "acmFormatTagEnumW");
        acmGetVersion = GetProcAddress(dll, "acmGetVersion");
        acmMetrics = GetProcAddress(dll, "acmMetrics");
        acmStreamClose = GetProcAddress(dll, "acmStreamClose");
        acmStreamConvert = GetProcAddress(dll, "acmStreamConvert");
        acmStreamMessage = GetProcAddress(dll, "acmStreamMessage");
        acmStreamOpen = GetProcAddress(dll, "acmStreamOpen");
        acmStreamPrepareHeader = GetProcAddress(dll, "acmStreamPrepareHeader");
        acmStreamReset = GetProcAddress(dll, "acmStreamReset");
        acmStreamSize = GetProcAddress(dll, "acmStreamSize");
        acmStreamUnprepareHeader = GetProcAddress(dll, "acmStreamUnprepareHeader");
    }
} msacm32;

struct msvfw32_dll
{
    HMODULE dll;
    FARPROC DrawDibBegin;
    FARPROC DrawDibChangePalette;
    FARPROC DrawDibClose;
    FARPROC DrawDibDraw;
    FARPROC DrawDibEnd;
    FARPROC DrawDibGetBuffer;
    FARPROC DrawDibGetPalette;
    FARPROC DrawDibOpen;
    FARPROC DrawDibProfileDisplay;
    FARPROC DrawDibRealize;
    FARPROC DrawDibSetPalette;
    FARPROC DrawDibStart;
    FARPROC DrawDibStop;
    FARPROC DrawDibTime;
    FARPROC GetOpenFileNamePreview;
    FARPROC GetOpenFileNamePreviewA;
    FARPROC GetOpenFileNamePreviewW;
    FARPROC GetSaveFileNamePreviewA;
    FARPROC GetSaveFileNamePreviewW;
    FARPROC ICClose;
    FARPROC ICCompress;
    FARPROC ICCompressorChoose;
    FARPROC ICCompressorFree;
    FARPROC ICDecompress;
    FARPROC ICDraw;
    FARPROC ICDrawBegin;
    FARPROC ICGetDisplayFormat;
    FARPROC ICGetInfo;
    FARPROC ICImageCompress;
    FARPROC ICImageDecompress;
    FARPROC ICInfo;
    FARPROC ICInstall;
    FARPROC ICLocate;
    FARPROC ICMThunk32;
    FARPROC ICOpen;
    FARPROC ICOpenFunction;
    FARPROC ICRemove;
    FARPROC ICSendMessage;
    FARPROC ICSeqCompressFrame;
    FARPROC ICSeqCompressFrameEnd;
    FARPROC ICSeqCompressFrameStart;
    FARPROC MCIWndCreate;
    FARPROC MCIWndCreateA;
    FARPROC MCIWndCreateW;
    FARPROC MCIWndRegisterClass;
    FARPROC StretchDIB;
    FARPROC VideoForWindowsVersion;

    void LoadOriginalLibrary(HMODULE module)
    {
        dll = module;
        DrawDibBegin = GetProcAddress(dll, "DrawDibBegin");
        DrawDibChangePalette = GetProcAddress(dll, "DrawDibChangePalette");
        DrawDibClose = GetProcAddress(dll, "DrawDibClose");
        DrawDibDraw = GetProcAddress(dll, "DrawDibDraw");
        DrawDibEnd = GetProcAddress(dll, "DrawDibEnd");
        DrawDibGetBuffer = GetProcAddress(dll, "DrawDibGetBuffer");
        DrawDibGetPalette = GetProcAddress(dll, "DrawDibGetPalette");
        DrawDibOpen = GetProcAddress(dll, "DrawDibOpen");
        DrawDibProfileDisplay = GetProcAddress(dll, "DrawDibProfileDisplay");
        DrawDibRealize = GetProcAddress(dll, "DrawDibRealize");
        DrawDibSetPalette = GetProcAddress(dll, "DrawDibSetPalette");
        DrawDibStart = GetProcAddress(dll, "DrawDibStart");
        DrawDibStop = GetProcAddress(dll, "DrawDibStop");
        DrawDibTime = GetProcAddress(dll, "DrawDibTime");
        GetOpenFileNamePreview = GetProcAddress(dll, "GetOpenFileNamePreview");
        GetOpenFileNamePreviewA = GetProcAddress(dll, "GetOpenFileNamePreviewA");
        GetOpenFileNamePreviewW = GetProcAddress(dll, "GetOpenFileNamePreviewW");
        GetSaveFileNamePreviewA = GetProcAddress(dll, "GetSaveFileNamePreviewA");
        GetSaveFileNamePreviewW = GetProcAddress(dll, "GetSaveFileNamePreviewW");
        ICClose = GetProcAddress(dll, "ICClose");
        ICCompress = GetProcAddress(dll, "ICCompress");
        ICCompressorChoose = GetProcAddress(dll, "ICCompressorChoose");
        ICCompressorFree = GetProcAddress(dll, "ICCompressorFree");
        ICDecompress = GetProcAddress(dll, "ICDecompress");
        ICDraw = GetProcAddress(dll, "ICDraw");
        ICDrawBegin = GetProcAddress(dll, "ICDrawBegin");
        ICGetDisplayFormat = GetProcAddress(dll, "ICGetDisplayFormat");
        ICGetInfo = GetProcAddress(dll, "ICGetInfo");
        ICImageCompress = GetProcAddress(dll, "ICImageCompress");
        ICImageDecompress = GetProcAddress(dll, "ICImageDecompress");
        ICInfo = GetProcAddress(dll, "ICInfo");
        ICInstall = GetProcAddress(dll, "ICInstall");
        ICLocate = GetProcAddress(dll, "ICLocate");
        ICMThunk32 = GetProcAddress(dll, "ICMThunk32");
        ICOpen = GetProcAddress(dll, "ICOpen");
        ICOpenFunction = GetProcAddress(dll, "ICOpenFunction");
        ICRemove = GetProcAddress(dll, "ICRemove");
        ICSendMessage = GetProcAddress(dll, "ICSendMessage");
        ICSeqCompressFrame = GetProcAddress(dll, "ICSeqCompressFrame");
        ICSeqCompressFrameEnd = GetProcAddress(dll, "ICSeqCompressFrameEnd");
        ICSeqCompressFrameStart = GetProcAddress(dll, "ICSeqCompressFrameStart");
        MCIWndCreate = GetProcAddress(dll, "MCIWndCreate");
        MCIWndCreateA = GetProcAddress(dll, "MCIWndCreateA");
        MCIWndCreateW = GetProcAddress(dll, "MCIWndCreateW");
        MCIWndRegisterClass = GetProcAddress(dll, "MCIWndRegisterClass");
        StretchDIB = GetProcAddress(dll, "StretchDIB");
        VideoForWindowsVersion = GetProcAddress(dll, "VideoForWindowsVersion");
    }
} msvfw32;


__declspec(naked) void _ov_bitrate() { _asm { jmp[vorbisfile.ov_bitrate] } }
__declspec(naked) void _ov_bitrate_instant() { _asm { jmp[vorbisfile.ov_bitrate_instant] } }
__declspec(naked) void _ov_clear() { _asm { jmp[vorbisfile.ov_clear] } }
__declspec(naked) void _ov_comment() { _asm { jmp[vorbisfile.ov_comment] } }
__declspec(naked) void _ov_crosslap() { _asm { jmp[vorbisfile.ov_crosslap] } }
__declspec(naked) void _ov_halfrate() { _asm { jmp[vorbisfile.ov_halfrate] } }
__declspec(naked) void _ov_halfrate_p() { _asm { jmp[vorbisfile.ov_halfrate_p] } }
__declspec(naked) void _ov_info() { _asm { jmp[vorbisfile.ov_info] } }
__declspec(naked) void _ov_open() { _asm { jmp[vorbisfile.ov_open] } }
__declspec(naked) void _ov_open_callbacks() { _asm { jmp[vorbisfile.ov_open_callbacks] } }
__declspec(naked) void _ov_pcm_seek() { _asm { jmp[vorbisfile.ov_pcm_seek] } }
__declspec(naked) void _ov_pcm_seek_lap() { _asm { jmp[vorbisfile.ov_pcm_seek_lap] } }
__declspec(naked) void _ov_pcm_seek_page() { _asm { jmp[vorbisfile.ov_pcm_seek_page] } }
__declspec(naked) void _ov_pcm_seek_page_lap() { _asm { jmp[vorbisfile.ov_pcm_seek_page_lap] } }
__declspec(naked) void _ov_pcm_tell() { _asm { jmp[vorbisfile.ov_pcm_tell] } }
__declspec(naked) void _ov_pcm_total() { _asm { jmp[vorbisfile.ov_pcm_total] } }
__declspec(naked) void _ov_raw_seek() { _asm { jmp[vorbisfile.ov_raw_seek] } }
__declspec(naked) void _ov_raw_seek_lap() { _asm { jmp[vorbisfile.ov_raw_seek_lap] } }
__declspec(naked) void _ov_raw_tell() { _asm { jmp[vorbisfile.ov_raw_tell] } }
__declspec(naked) void _ov_raw_total() { _asm { jmp[vorbisfile.ov_raw_total] } }
__declspec(naked) void _ov_read() { _asm { jmp[vorbisfile.ov_read] } }
__declspec(naked) void _ov_read_float() { _asm { jmp[vorbisfile.ov_read_float] } }
__declspec(naked) void _ov_seekable() { _asm { jmp[vorbisfile.ov_seekable] } }
__declspec(naked) void _ov_serialnumber() { _asm { jmp[vorbisfile.ov_serialnumber] } }
__declspec(naked) void _ov_streams() { _asm { jmp[vorbisfile.ov_streams] } }
__declspec(naked) void _ov_test() { _asm { jmp[vorbisfile.ov_test] } }
__declspec(naked) void _ov_test_callbacks() { _asm { jmp[vorbisfile.ov_test_callbacks] } }
__declspec(naked) void _ov_test_open() { _asm { jmp[vorbisfile.ov_test_open] } }
__declspec(naked) void _ov_time_seek() { _asm { jmp[vorbisfile.ov_time_seek] } }
__declspec(naked) void _ov_time_seek_lap() { _asm { jmp[vorbisfile.ov_time_seek_lap] } }
__declspec(naked) void _ov_time_seek_page() { _asm { jmp[vorbisfile.ov_time_seek_page] } }
__declspec(naked) void _ov_time_seek_page_lap() { _asm { jmp[vorbisfile.ov_time_seek_page_lap] } }
__declspec(naked) void _ov_time_tell() { _asm { jmp[vorbisfile.ov_time_tell] } }
__declspec(naked) void _ov_time_total() { _asm { jmp[vorbisfile.ov_time_total] } }

__declspec(naked) void _DirectInput8Create() { _asm { jmp[dinput8.DirectInput8Create] } }

__declspec(naked) void _DirectInputCreateA() { _asm { jmp[dinput.DirectInputCreateA] } }
__declspec(naked) void _DirectInputCreateEx() { _asm { jmp[dinput.DirectInputCreateEx] } }
__declspec(naked) void _DirectInputCreateW() { _asm { jmp[dinput.DirectInputCreateW] } }

__declspec(naked) void _DllCanUnloadNow()
{
    if (dinput8.DllCanUnloadNow)
        _asm { jmp[dinput8.DllCanUnloadNow] }
    else
        if (dsound.DllCanUnloadNow)
            _asm { jmp[dsound.DllCanUnloadNow] }
        else
            if (ddraw.DllCanUnloadNow)
                _asm { jmp[ddraw.DllCanUnloadNow] }
}
__declspec(naked) void _DllGetClassObject()
{
    if (dinput8.DllGetClassObject)
        _asm { jmp[dinput8.DllGetClassObject] }
    else
        if (dsound.DllGetClassObject)
            _asm { jmp[dsound.DllGetClassObject] }
        else
            if (ddraw.DllGetClassObject)
                _asm { jmp[ddraw.DllGetClassObject] }
}

__declspec(naked) void _DllRegisterServer()
{
    if (dinput8.DllRegisterServer)
        _asm { jmp[dinput8.DllRegisterServer] }
    else
        if (dinput.DllRegisterServer)
            _asm { jmp[dinput8.DllRegisterServer] }
}

__declspec(naked) void _DllUnregisterServer()
{
    if (dinput8.DllUnregisterServer)
        _asm { jmp[dinput8.DllUnregisterServer] }
    else
        if (dinput.DllUnregisterServer)
            _asm { jmp[dinput8.DllUnregisterServer] }
}

__declspec(naked) void _DirectSoundCaptureCreate() { _asm { jmp[dsound.DirectSoundCaptureCreate] } }
__declspec(naked) void _DirectSoundCaptureCreate8() { _asm { jmp[dsound.DirectSoundCaptureCreate8] } }
__declspec(naked) void _DirectSoundCaptureEnumerateA() { _asm { jmp[dsound.DirectSoundCaptureEnumerateA] } }
__declspec(naked) void _DirectSoundCaptureEnumerateW() { _asm { jmp[dsound.DirectSoundCaptureEnumerateW] } }
__declspec(naked) void _DirectSoundCreate() { _asm { jmp[dsound.DirectSoundCreate] } }
__declspec(naked) void _DirectSoundCreate8() { _asm { jmp[dsound.DirectSoundCreate8] } }
__declspec(naked) void _DirectSoundEnumerateA() { _asm { jmp[dsound.DirectSoundEnumerateA] } }
__declspec(naked) void _DirectSoundEnumerateW() { _asm { jmp[dsound.DirectSoundEnumerateW] } }
__declspec(naked) void _DirectSoundFullDuplexCreate() { _asm { jmp[dsound.DirectSoundFullDuplexCreate] } }
//__declspec(naked) void _DllCanUnloadNow() { _asm { jmp[dsound.DllCanUnloadNow] } }
//__declspec(naked) void _DllGetClassObject() { _asm { jmp[dsound.DllGetClassObject] } }
__declspec(naked) void _GetDeviceID() { _asm { jmp[dsound.GetDeviceID] } }

__declspec(naked) void _DebugSetMute()
{
    if (d3d8.DebugSetMute)
        _asm { jmp[d3d8.DebugSetMute] }
    else
        if (d3d9.DebugSetMute)
            _asm { jmp[d3d9.DebugSetMute] }
}
__declspec(naked) void _Direct3D8EnableMaximizedWindowedModeShim() { _asm { jmp[d3d8.Direct3D8EnableMaximizedWindowedModeShim] } }
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
//__declspec(naked) void _DebugSetMute() { _asm { jmp[d3d9.DebugSetMute] } }
__declspec(naked) void _Direct3D9EnableMaximizedWindowedModeShim() { _asm { jmp[d3d9.Direct3D9EnableMaximizedWindowedModeShim] } }
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

__declspec(naked) void _AcquireDDThreadLock() { _asm { jmp[ddraw.AcquireDDThreadLock] } }
__declspec(naked) void _CompleteCreateSysmemSurface() { _asm { jmp[ddraw.CompleteCreateSysmemSurface] } }
__declspec(naked) void _D3DParseUnknownCommand() { _asm { jmp[ddraw.D3DParseUnknownCommand] } }
__declspec(naked) void _DDGetAttachedSurfaceLcl() { _asm { jmp[ddraw.DDGetAttachedSurfaceLcl] } }
__declspec(naked) void _DDInternalLock() { _asm { jmp[ddraw.DDInternalLock] } }
__declspec(naked) void _DDInternalUnlock() { _asm { jmp[ddraw.DDInternalUnlock] } }
__declspec(naked) void _DSoundHelp() { _asm { jmp[ddraw.DSoundHelp] } }
__declspec(naked) void _DirectDrawCreate() { _asm { jmp[ddraw.DirectDrawCreate] } }
__declspec(naked) void _DirectDrawCreateClipper() { _asm { jmp[ddraw.DirectDrawCreateClipper] } }
__declspec(naked) void _DirectDrawCreateEx() { _asm { jmp[ddraw.DirectDrawCreateEx] } }
__declspec(naked) void _DirectDrawEnumerateA() { _asm { jmp[ddraw.DirectDrawEnumerateA] } }
__declspec(naked) void _DirectDrawEnumerateExA() { _asm { jmp[ddraw.DirectDrawEnumerateExA] } }
__declspec(naked) void _DirectDrawEnumerateExW() { _asm { jmp[ddraw.DirectDrawEnumerateExW] } }
__declspec(naked) void _DirectDrawEnumerateW() { _asm { jmp[ddraw.DirectDrawEnumerateW] } }
//__declspec(naked) void _DllCanUnloadNow() { _asm { jmp[ddraw.DllCanUnloadNow] } }
//__declspec(naked) void _DllGetClassObject() { _asm { jmp[ddraw.DllGetClassObject] } }
__declspec(naked) void _GetDDSurfaceLocal() { _asm { jmp[ddraw.GetDDSurfaceLocal] } }
__declspec(naked) void _GetOLEThunkData() { _asm { jmp[ddraw.GetOLEThunkData] } }
__declspec(naked) void _GetSurfaceFromDC() { _asm { jmp[ddraw.GetSurfaceFromDC] } }
__declspec(naked) void _RegisterSpecialCase() { _asm { jmp[ddraw.RegisterSpecialCase] } }
__declspec(naked) void _ReleaseDDThreadLock() { _asm { jmp[ddraw.ReleaseDDThreadLock] } }
__declspec(naked) void _SetAppCompatData() { _asm { jmp[ddraw.SetAppCompatData] } }

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

__declspec(naked) void _DrawDibBegin() { _asm { jmp[msvfw32.DrawDibBegin] } }
__declspec(naked) void _DrawDibChangePalette() { _asm { jmp[msvfw32.DrawDibChangePalette] } }
__declspec(naked) void _DrawDibClose() { _asm { jmp[msvfw32.DrawDibClose] } }
__declspec(naked) void _DrawDibDraw() { _asm { jmp[msvfw32.DrawDibDraw] } }
__declspec(naked) void _DrawDibEnd() { _asm { jmp[msvfw32.DrawDibEnd] } }
__declspec(naked) void _DrawDibGetBuffer() { _asm { jmp[msvfw32.DrawDibGetBuffer] } }
__declspec(naked) void _DrawDibGetPalette() { _asm { jmp[msvfw32.DrawDibGetPalette] } }
__declspec(naked) void _DrawDibOpen() { _asm { jmp[msvfw32.DrawDibOpen] } }
__declspec(naked) void _DrawDibProfileDisplay() { _asm { jmp[msvfw32.DrawDibProfileDisplay] } }
__declspec(naked) void _DrawDibRealize() { _asm { jmp[msvfw32.DrawDibRealize] } }
__declspec(naked) void _DrawDibSetPalette() { _asm { jmp[msvfw32.DrawDibSetPalette] } }
__declspec(naked) void _DrawDibStart() { _asm { jmp[msvfw32.DrawDibStart] } }
__declspec(naked) void _DrawDibStop() { _asm { jmp[msvfw32.DrawDibStop] } }
__declspec(naked) void _DrawDibTime() { _asm { jmp[msvfw32.DrawDibTime] } }
__declspec(naked) void _GetOpenFileNamePreview() { _asm { jmp[msvfw32.GetOpenFileNamePreview] } }
__declspec(naked) void _GetOpenFileNamePreviewA() { _asm { jmp[msvfw32.GetOpenFileNamePreviewA] } }
__declspec(naked) void _GetOpenFileNamePreviewW() { _asm { jmp[msvfw32.GetOpenFileNamePreviewW] } }
__declspec(naked) void _GetSaveFileNamePreviewA() { _asm { jmp[msvfw32.GetSaveFileNamePreviewA] } }
__declspec(naked) void _GetSaveFileNamePreviewW() { _asm { jmp[msvfw32.GetSaveFileNamePreviewW] } }
__declspec(naked) void _ICClose() { _asm { jmp[msvfw32.ICClose] } }
__declspec(naked) void _ICCompress() { _asm { jmp[msvfw32.ICCompress] } }
__declspec(naked) void _ICCompressorChoose() { _asm { jmp[msvfw32.ICCompressorChoose] } }
__declspec(naked) void _ICCompressorFree() { _asm { jmp[msvfw32.ICCompressorFree] } }
__declspec(naked) void _ICDecompress() { _asm { jmp[msvfw32.ICDecompress] } }
__declspec(naked) void _ICDraw() { _asm { jmp[msvfw32.ICDraw] } }
__declspec(naked) void _ICDrawBegin() { _asm { jmp[msvfw32.ICDrawBegin] } }
__declspec(naked) void _ICGetDisplayFormat() { _asm { jmp[msvfw32.ICGetDisplayFormat] } }
__declspec(naked) void _ICGetInfo() { _asm { jmp[msvfw32.ICGetInfo] } }
__declspec(naked) void _ICImageCompress() { _asm { jmp[msvfw32.ICImageCompress] } }
__declspec(naked) void _ICImageDecompress() { _asm { jmp[msvfw32.ICImageDecompress] } }
__declspec(naked) void _ICInfo() { _asm { jmp[msvfw32.ICInfo] } }
__declspec(naked) void _ICInstall() { _asm { jmp[msvfw32.ICInstall] } }
__declspec(naked) void _ICLocate() { _asm { jmp[msvfw32.ICLocate] } }
__declspec(naked) void _ICMThunk32() { _asm { jmp[msvfw32.ICMThunk32] } }
__declspec(naked) void _ICOpen() { _asm { jmp[msvfw32.ICOpen] } }
__declspec(naked) void _ICOpenFunction() { _asm { jmp[msvfw32.ICOpenFunction] } }
__declspec(naked) void _ICRemove() { _asm { jmp[msvfw32.ICRemove] } }
__declspec(naked) void _ICSendMessage() { _asm { jmp[msvfw32.ICSendMessage] } }
__declspec(naked) void _ICSeqCompressFrame() { _asm { jmp[msvfw32.ICSeqCompressFrame] } }
__declspec(naked) void _ICSeqCompressFrameEnd() { _asm { jmp[msvfw32.ICSeqCompressFrameEnd] } }
__declspec(naked) void _ICSeqCompressFrameStart() { _asm { jmp[msvfw32.ICSeqCompressFrameStart] } }
__declspec(naked) void _MCIWndCreate() { _asm { jmp[msvfw32.MCIWndCreate] } }
__declspec(naked) void _MCIWndCreateA() { _asm { jmp[msvfw32.MCIWndCreateA] } }
__declspec(naked) void _MCIWndCreateW() { _asm { jmp[msvfw32.MCIWndCreateW] } }
__declspec(naked) void _MCIWndRegisterClass() { _asm { jmp[msvfw32.MCIWndRegisterClass] } }
__declspec(naked) void _StretchDIB() { _asm { jmp[msvfw32.StretchDIB] } }
__declspec(naked) void _VideoForWindowsVersion() { _asm { jmp[msvfw32.VideoForWindowsVersion] } }
#endif

#if X64
typedef HRESULT(*fn_DirectSoundCaptureCreate)(LPGUID lpGUID, LPDIRECTSOUNDCAPTURE *lplpDSC, LPUNKNOWN pUnkOuter);
void _DirectSoundCaptureCreate() { (fn_DirectSoundCaptureCreate)dsound.DirectSoundCaptureCreate(); }

typedef HRESULT(*fn_DirectSoundCaptureCreate8)(LPCGUID lpcGUID, LPDIRECTSOUNDCAPTURE8 * lplpDSC, LPUNKNOWN pUnkOuter);
void _DirectSoundCaptureCreate8() { (fn_DirectSoundCaptureCreate8)dsound.DirectSoundCaptureCreate8(); }

typedef HRESULT(*fn_DirectSoundCaptureEnumerateA)(LPDSENUMCALLBACKA lpDSEnumCallback, LPVOID lpContext);
void _DirectSoundCaptureEnumerateA() { (fn_DirectSoundCaptureEnumerateA)dsound.DirectSoundCaptureEnumerateA(); }

typedef HRESULT(*fn_DirectSoundCaptureEnumerateW)(LPDSENUMCALLBACKW lpDSEnumCallback, LPVOID lpContext);
void _DirectSoundCaptureEnumerateW() { (fn_DirectSoundCaptureEnumerateW)dsound.DirectSoundCaptureEnumerateW(); }

typedef HRESULT(*fn_DirectSoundCreate)(LPCGUID lpcGUID, LPDIRECTSOUND* ppDS, IUnknown* pUnkOuter);
void _DirectSoundCreate() { (fn_DirectSoundCreate)dsound.DirectSoundCreate(); }

typedef HRESULT(*fn_DirectSoundCreate8)(LPCGUID lpcGUID, LPDIRECTSOUND8* ppDS, IUnknown* pUnkOuter);
void _DirectSoundCreate8() { (fn_DirectSoundCreate8)dsound.DirectSoundCreate8(); }

typedef HRESULT(*fn_DirectSoundEnumerateA)(LPDSENUMCALLBACKA lpDSEnumCallback, LPVOID lpContext);
void _DirectSoundEnumerateA() { (fn_DirectSoundEnumerateA)dsound.DirectSoundEnumerateA(); }

typedef HRESULT(*fn_DirectSoundEnumerateW)(LPDSENUMCALLBACKW lpDSEnumCallback, LPVOID lpContext);
void _DirectSoundEnumerateW() { (fn_DirectSoundEnumerateW)dsound.DirectSoundEnumerateW(); }

typedef HRESULT(*fn_DirectSoundFullDuplexCreate)(const GUID* capture_dev, const GUID* render_dev, const DSCBUFFERDESC* cbufdesc, const DSBUFFERDESC* bufdesc, HWND  hwnd, DWORD level, IDirectSoundFullDuplex**  dsfd, IDirectSoundCaptureBuffer8** dscb8, IDirectSoundBuffer8** dsb8, IUnknown* outer_unk);
void _DirectSoundFullDuplexCreate() { (fn_DirectSoundFullDuplexCreate)dsound.DirectSoundFullDuplexCreate(); }

typedef HRESULT(*fn_GetDeviceID)(LPCGUID pGuidSrc, LPGUID pGuidDest);
void _GetDeviceID() { (fn_GetDeviceID)dsound.GetDeviceID(); }


typedef HRESULT(*fn_DirectInput8Create)(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID * ppvOut, LPUNKNOWN punkOuter);
void _DirectInput8Create() { (fn_DirectInput8Create)dinput8.DirectInput8Create(); }

typedef HRESULT(*fn_DllRegisterServer)();
void _DllRegisterServer() { (fn_DllRegisterServer)dinput8.DllRegisterServer(); }

typedef HRESULT(*fn_DllUnregisterServer)();
void _DllUnregisterServer() { (fn_DllUnregisterServer)dinput8.DllUnregisterServer(); }


typedef HRESULT(*fn_DllCanUnloadNow)();
void _DllCanUnloadNow()
{
    if (dinput8.DllCanUnloadNow)
        (fn_DllCanUnloadNow)dinput8.DllCanUnloadNow();
    else
        (fn_DllCanUnloadNow)dsound.DllCanUnloadNow();
}

typedef HRESULT(*fn_DllGetClassObject)(REFCLSID rclsid, REFIID riid, LPVOID *ppv);
void _DllGetClassObject()
{
    if (dinput8.DllGetClassObject)
        (fn_DllGetClassObject)dinput8.DllGetClassObject();
    else
        (fn_DllGetClassObject)dsound.DllGetClassObject();
}
#endif