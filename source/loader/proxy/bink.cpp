// binkw32/bink2w32 (Win32) and binkw64/bink2w64 (x64) proxies. They forward to the
// game's original, renamed to <name>Hooked.dll.
#include "bink.hpp"
#include "proxy.hpp"
#include "../core/loader.hpp"
#include "../core/log.hpp"
#include "../core/paths.hpp"
#include "../core/strings.hpp"

using namespace ual;

namespace
{
    // shared COM exports, filled from the bink DLL like other proxies
    void LoadShared(HMODULE dll) { ual::proxy::LoadSharedExports(dll); }
}

#ifndef _WIN64
struct bink2w32_dll
{
    HMODULE dll;
    FARPROC BinkAllocateFrameBuffers;
    FARPROC BinkBufferBlit;
    FARPROC BinkBufferCheckWinPos;
    FARPROC BinkBufferClear;
    FARPROC BinkBufferClose;
    FARPROC BinkBufferGetDescription;
    FARPROC BinkBufferGetError;
    FARPROC BinkBufferLock;
    FARPROC BinkBufferOpen;
    FARPROC BinkBufferSetDirectDraw;
    FARPROC BinkBufferSetHWND;
    FARPROC BinkBufferSetOffset;
    FARPROC BinkBufferSetResolution;
    FARPROC BinkBufferSetScale;
    FARPROC BinkBufferUnlock;
    FARPROC BinkCheckCursor;
    FARPROC BinkClose;
    FARPROC BinkCloseTrack;
    FARPROC BinkControlBackgroundIO;
    FARPROC BinkControlPlatformFeatures;
    FARPROC BinkCopyToBuffer;
    FARPROC BinkCopyToBufferRect;
    FARPROC BinkCurrentSubtitle;
    FARPROC BinkDDSurfaceType;
    FARPROC BinkDX8SurfaceType;
    FARPROC BinkDX9SurfaceType;
    FARPROC BinkDoFrame;
    FARPROC BinkDoFrameAsync;
    FARPROC BinkDoFrameAsyncMulti;
    FARPROC BinkDoFrameAsyncWait;
    FARPROC BinkDoFramePlane;
    FARPROC BinkFindXAudio2WinDevice;
    FARPROC BinkFreeGlobals;
    FARPROC BinkGetError;
    FARPROC BinkGetFrameBuffersInfo;
    FARPROC BinkGetGPUDataBuffersInfo;
    FARPROC BinkGetKeyFrame;
    FARPROC BinkGetPalette;
    FARPROC BinkGetPlatformInfo;
    FARPROC BinkGetRealtime;
    FARPROC BinkGetRects;
    FARPROC BinkGetRects4;
    FARPROC BinkGetSubtitleByIndex;
    FARPROC BinkGetSummary;
    FARPROC BinkGetTrackData;
    FARPROC BinkGetTrackData12;
    FARPROC BinkGetTrackID;
    FARPROC BinkGetTrackMaxSize;
    FARPROC BinkGetTrackType;
    FARPROC BinkGoto;
    FARPROC BinkIsSoftwareCursor;
    FARPROC BinkLoadSubtitles;
    FARPROC BinkLogoAddress;
    FARPROC BinkNextFrame;
    FARPROC BinkOpen;
    FARPROC BinkOpenDirectSound;
    FARPROC BinkOpenMiles;
    FARPROC BinkOpenTrack;
    FARPROC BinkOpenWaveOut;
    FARPROC BinkOpenWithOptions;
    FARPROC BinkOpenXAudio27;
    FARPROC BinkOpenXAudio28;
    FARPROC BinkOpenXAudio29;
    FARPROC BinkOpenXAudio2;
    FARPROC BinkOpenXAudio2_8;
    FARPROC BinkPause;
    FARPROC BinkRegisterFrameBuffers;
    FARPROC BinkRegisterGPUDataBuffers;
    FARPROC BinkRequestStopAsyncThread;
    FARPROC BinkRequestStopAsyncThreadsMulti;
    FARPROC BinkRestoreCursor;
    FARPROC BinkService;
    FARPROC BinkServiceSound;
    FARPROC BinkSetError;
    FARPROC BinkSetFileOffset;
    FARPROC BinkSetFrameRate;
    FARPROC BinkSetIO;
    FARPROC BinkSetIOSize;
    FARPROC BinkSetIOSize8;
    FARPROC BinkSetMemory;
    FARPROC BinkSetMixBinVolumes;
    FARPROC BinkSetMixBins;
    FARPROC BinkSetOSFileCallbacks;
    FARPROC BinkSetPan;
    FARPROC BinkSetSimulate;
    FARPROC BinkSetSoundOnOff;
    FARPROC BinkSetSoundSystem2;
    FARPROC BinkSetSoundSystem;
    FARPROC BinkSetSoundTrack;
    FARPROC BinkSetSpeakerVolumes;
    FARPROC BinkSetVideoOnOff;
    FARPROC BinkSetVolume;
    FARPROC BinkSetWillLoop;
    FARPROC BinkShouldSkip;
    FARPROC BinkStartAsyncThread;
    FARPROC BinkUseTelemetry;
    FARPROC BinkUseTmLite;
    FARPROC BinkUtilCPUs;
    FARPROC BinkUtilFree;
    FARPROC BinkUtilMalloc;
    FARPROC BinkUtilMutexCreate;
    FARPROC BinkUtilMutexDestroy;
    FARPROC BinkUtilMutexLock;
    FARPROC BinkUtilMutexLockTimeOut;
    FARPROC BinkUtilMutexUnlock;
    FARPROC BinkUtilSoundGlobalLock;
    FARPROC BinkUtilSoundGlobalUnlock;
    FARPROC BinkWait;
    FARPROC BinkWaitStopAsyncThread;
    FARPROC BinkWaitStopAsyncThreadsMulti;
    FARPROC RADSetMemory;
    FARPROC RADTimerRead;
    FARPROC YUV_blit_16a1bpp40;
    FARPROC YUV_blit_16a1bpp52;
    FARPROC YUV_blit_16a1bpp_mask48;
    FARPROC YUV_blit_16a1bpp_mask60;
    FARPROC YUV_blit_16a4bpp40;
    FARPROC YUV_blit_16a4bpp52;
    FARPROC YUV_blit_16a4bpp_mask48;
    FARPROC YUV_blit_16a4bpp_mask60;
    FARPROC YUV_blit_16bpp40;
    FARPROC YUV_blit_16bpp48;
    FARPROC YUV_blit_16bpp52;
    FARPROC YUV_blit_16bpp_mask48;
    FARPROC YUV_blit_16bpp_mask56;
    FARPROC YUV_blit_16bpp_mask60;
    FARPROC YUV_blit_24bpp40;
    FARPROC YUV_blit_24bpp48;
    FARPROC YUV_blit_24bpp52;
    FARPROC YUV_blit_24bpp_mask48;
    FARPROC YUV_blit_24bpp_mask56;
    FARPROC YUV_blit_24bpp_mask60;
    FARPROC YUV_blit_24rbpp40;
    FARPROC YUV_blit_24rbpp48;
    FARPROC YUV_blit_24rbpp52;
    FARPROC YUV_blit_24rbpp_mask48;
    FARPROC YUV_blit_24rbpp_mask56;
    FARPROC YUV_blit_24rbpp_mask60;
    FARPROC YUV_blit_32abpp40;
    FARPROC YUV_blit_32abpp52;
    FARPROC YUV_blit_32abpp_mask48;
    FARPROC YUV_blit_32abpp_mask60;
    FARPROC YUV_blit_32bpp40;
    FARPROC YUV_blit_32bpp48;
    FARPROC YUV_blit_32bpp52;
    FARPROC YUV_blit_32bpp_mask48;
    FARPROC YUV_blit_32bpp_mask56;
    FARPROC YUV_blit_32bpp_mask60;
    FARPROC YUV_blit_32rabpp40;
    FARPROC YUV_blit_32rabpp52;
    FARPROC YUV_blit_32rabpp_mask48;
    FARPROC YUV_blit_32rabpp_mask60;
    FARPROC YUV_blit_32rbpp40;
    FARPROC YUV_blit_32rbpp48;
    FARPROC YUV_blit_32rbpp52;
    FARPROC YUV_blit_32rbpp_mask48;
    FARPROC YUV_blit_32rbpp_mask56;
    FARPROC YUV_blit_32rbpp_mask60;
    FARPROC YUV_blit_UYVY40;
    FARPROC YUV_blit_UYVY48;
    FARPROC YUV_blit_UYVY52;
    FARPROC YUV_blit_UYVY_mask48;
    FARPROC YUV_blit_UYVY_mask56;
    FARPROC YUV_blit_UYVY_mask60;
    FARPROC YUV_blit_YUY240;
    FARPROC YUV_blit_YUY248;
    FARPROC YUV_blit_YUY252;
    FARPROC YUV_blit_YUY2_mask48;
    FARPROC YUV_blit_YUY2_mask56;
    FARPROC YUV_blit_YUY2_mask60;
    FARPROC YUV_blit_YV1244;
    FARPROC YUV_blit_YV1252;
    FARPROC YUV_blit_YV1256;
    FARPROC YUV_init4;
    FARPROC radfree;
    FARPROC radmalloc;

    void LoadOriginalLibrary(HMODULE module)
    {
        dll = module;
        auto AutoGetProcAddress = [](HMODULE lib, LPCSTR s) { return GetProcAddress(lib, s); };

        BinkAllocateFrameBuffers = AutoGetProcAddress(dll, "_BinkAllocateFrameBuffers@12");
        BinkBufferBlit = AutoGetProcAddress(dll, "_BinkBufferBlit@12");
        BinkBufferCheckWinPos = AutoGetProcAddress(dll, "_BinkBufferCheckWinPos@12");
        BinkBufferClear = AutoGetProcAddress(dll, "_BinkBufferClear@8");
        BinkBufferClose = AutoGetProcAddress(dll, "_BinkBufferClose@4");
        BinkBufferGetDescription = AutoGetProcAddress(dll, "_BinkBufferGetDescription@4");
        BinkBufferGetError = AutoGetProcAddress(dll, "_BinkBufferGetError@0");
        BinkBufferLock = AutoGetProcAddress(dll, "_BinkBufferLock@4");
        BinkBufferOpen = AutoGetProcAddress(dll, "_BinkBufferOpen@16");
        BinkBufferSetDirectDraw = AutoGetProcAddress(dll, "_BinkBufferSetDirectDraw@8");
        BinkBufferSetHWND = AutoGetProcAddress(dll, "_BinkBufferSetHWND@8");
        BinkBufferSetOffset = AutoGetProcAddress(dll, "_BinkBufferSetOffset@12");
        BinkBufferSetResolution = AutoGetProcAddress(dll, "_BinkBufferSetResolution@12");
        BinkBufferSetScale = AutoGetProcAddress(dll, "_BinkBufferSetScale@12");
        BinkBufferUnlock = AutoGetProcAddress(dll, "_BinkBufferUnlock@4");
        BinkCheckCursor = AutoGetProcAddress(dll, "_BinkCheckCursor@20");
        BinkClose = AutoGetProcAddress(dll, "_BinkClose@4");
        BinkCloseTrack = AutoGetProcAddress(dll, "_BinkCloseTrack@4");
        BinkControlBackgroundIO = AutoGetProcAddress(dll, "_BinkControlBackgroundIO@8");
        BinkControlPlatformFeatures = AutoGetProcAddress(dll, "_BinkControlPlatformFeatures@8");
        BinkCopyToBuffer = AutoGetProcAddress(dll, "_BinkCopyToBuffer@28");
        BinkCopyToBufferRect = AutoGetProcAddress(dll, "_BinkCopyToBufferRect@44");
        BinkCurrentSubtitle = AutoGetProcAddress(dll, "_BinkCurrentSubtitle@16");
        BinkDDSurfaceType = AutoGetProcAddress(dll, "_BinkDDSurfaceType@4");
        BinkDX8SurfaceType = AutoGetProcAddress(dll, "_BinkDX8SurfaceType@4");
        BinkDX9SurfaceType = AutoGetProcAddress(dll, "_BinkDX9SurfaceType@4");
        BinkDoFrame = AutoGetProcAddress(dll, "_BinkDoFrame@4");
        BinkDoFrameAsync = AutoGetProcAddress(dll, "_BinkDoFrameAsync@12");
        BinkDoFrameAsyncMulti = AutoGetProcAddress(dll, "_BinkDoFrameAsyncMulti@12");
        BinkDoFrameAsyncWait = AutoGetProcAddress(dll, "_BinkDoFrameAsyncWait@8");
        BinkDoFramePlane = AutoGetProcAddress(dll, "_BinkDoFramePlane@8");
        BinkFindXAudio2WinDevice = AutoGetProcAddress(dll, "_BinkFindXAudio2WinDevice@8");
        BinkFreeGlobals = AutoGetProcAddress(dll, "_BinkFreeGlobals@0");
        BinkGetError = AutoGetProcAddress(dll, "_BinkGetError@0");
        BinkGetFrameBuffersInfo = AutoGetProcAddress(dll, "_BinkGetFrameBuffersInfo@8");
        BinkGetGPUDataBuffersInfo = AutoGetProcAddress(dll, "_BinkGetGPUDataBuffersInfo@8");
        BinkGetKeyFrame = AutoGetProcAddress(dll, "_BinkGetKeyFrame@12");
        BinkGetPalette = AutoGetProcAddress(dll, "_BinkGetPalette@4");
        BinkGetPlatformInfo = AutoGetProcAddress(dll, "_BinkGetPlatformInfo@8");
        BinkGetRealtime = AutoGetProcAddress(dll, "_BinkGetRealtime@12");
        BinkGetRects = AutoGetProcAddress(dll, "_BinkGetRects@8");
        BinkGetRects4 = AutoGetProcAddress(dll, "_BinkGetRects@4");
        BinkGetSubtitleByIndex = AutoGetProcAddress(dll, "_BinkGetSubtitleByIndex@16");
        BinkGetSummary = AutoGetProcAddress(dll, "_BinkGetSummary@8");
        BinkGetTrackData = AutoGetProcAddress(dll, "_BinkGetTrackData@8");
        BinkGetTrackData12 = AutoGetProcAddress(dll, "_BinkGetTrackData@12");
        BinkGetTrackID = AutoGetProcAddress(dll, "_BinkGetTrackID@8");
        BinkGetTrackMaxSize = AutoGetProcAddress(dll, "_BinkGetTrackMaxSize@8");
        BinkGetTrackType = AutoGetProcAddress(dll, "_BinkGetTrackType@8");
        BinkGoto = AutoGetProcAddress(dll, "_BinkGoto@12");
        BinkIsSoftwareCursor = AutoGetProcAddress(dll, "_BinkIsSoftwareCursor@8");
        BinkLoadSubtitles = AutoGetProcAddress(dll, "_BinkLoadSubtitles@8");
        BinkLogoAddress = AutoGetProcAddress(dll, "_BinkLogoAddress@0");
        BinkNextFrame = AutoGetProcAddress(dll, "_BinkNextFrame@4");
        BinkOpen = AutoGetProcAddress(dll, "_BinkOpen@8");
        BinkOpenDirectSound = AutoGetProcAddress(dll, "_BinkOpenDirectSound@4");
        BinkOpenMiles = AutoGetProcAddress(dll, "_BinkOpenMiles@4");
        BinkOpenTrack = AutoGetProcAddress(dll, "_BinkOpenTrack@8");
        BinkOpenWaveOut = AutoGetProcAddress(dll, "_BinkOpenWaveOut@4");
        BinkOpenWithOptions = AutoGetProcAddress(dll, "_BinkOpenWithOptions@12");
        BinkOpenXAudio2 = AutoGetProcAddress(dll, "_BinkOpenXAudio2@4");
        BinkOpenXAudio2_8 = AutoGetProcAddress(dll, "_BinkOpenXAudio2@8");
        BinkOpenXAudio27 = AutoGetProcAddress(dll, "_BinkOpenXAudio27@8");
        BinkOpenXAudio28 = AutoGetProcAddress(dll, "_BinkOpenXAudio28@8");
        BinkOpenXAudio29 = AutoGetProcAddress(dll, "_BinkOpenXAudio29@8");
        BinkPause = AutoGetProcAddress(dll, "_BinkPause@8");
        BinkRegisterFrameBuffers = AutoGetProcAddress(dll, "_BinkRegisterFrameBuffers@8");
        BinkRegisterGPUDataBuffers = AutoGetProcAddress(dll, "_BinkRegisterGPUDataBuffers@8");
        BinkRequestStopAsyncThread = AutoGetProcAddress(dll, "_BinkRequestStopAsyncThread@4");
        BinkRequestStopAsyncThreadsMulti = AutoGetProcAddress(dll, "_BinkRequestStopAsyncThreadsMulti@8");
        BinkRestoreCursor = AutoGetProcAddress(dll, "_BinkRestoreCursor@4");
        BinkService = AutoGetProcAddress(dll, "_BinkService@4");
        BinkServiceSound = AutoGetProcAddress(dll, "_BinkServiceSound@0");
        BinkSetError = AutoGetProcAddress(dll, "_BinkSetError@4");
        BinkSetFileOffset = AutoGetProcAddress(dll, "_BinkSetFileOffset@8");
        BinkSetFrameRate = AutoGetProcAddress(dll, "_BinkSetFrameRate@8");
        BinkSetIO = AutoGetProcAddress(dll, "_BinkSetIO@4");
        BinkSetIOSize = AutoGetProcAddress(dll, "_BinkSetIOSize@4");
        BinkSetIOSize8 = AutoGetProcAddress(dll, "_BinkSetIOSize@8");
        BinkSetMemory = AutoGetProcAddress(dll, "_BinkSetMemory@8");
        BinkSetMixBinVolumes = AutoGetProcAddress(dll, "_BinkSetMixBinVolumes@20");
        BinkSetMixBins = AutoGetProcAddress(dll, "_BinkSetMixBins@16");
        BinkSetOSFileCallbacks = AutoGetProcAddress(dll, "_BinkSetOSFileCallbacks@16");
        BinkSetPan = AutoGetProcAddress(dll, "_BinkSetPan@12");
        BinkSetSimulate = AutoGetProcAddress(dll, "_BinkSetSimulate@4");
        BinkSetSoundOnOff = AutoGetProcAddress(dll, "_BinkSetSoundOnOff@8");
        BinkSetSoundSystem = AutoGetProcAddress(dll, "_BinkSetSoundSystem@8");
        BinkSetSoundSystem2 = AutoGetProcAddress(dll, "_BinkSetSoundSystem2@12");
        BinkSetSoundTrack = AutoGetProcAddress(dll, "_BinkSetSoundTrack@8");
        BinkSetSpeakerVolumes = AutoGetProcAddress(dll, "_BinkSetSpeakerVolumes@20");
        BinkSetVideoOnOff = AutoGetProcAddress(dll, "_BinkSetVideoOnOff@8");
        BinkSetVolume = AutoGetProcAddress(dll, "_BinkSetVolume@12");
        BinkSetWillLoop = AutoGetProcAddress(dll, "_BinkSetWillLoop@8");
        BinkShouldSkip = AutoGetProcAddress(dll, "_BinkShouldSkip@4");
        BinkStartAsyncThread = AutoGetProcAddress(dll, "_BinkStartAsyncThread@8");
        BinkUseTelemetry = AutoGetProcAddress(dll, "_BinkUseTelemetry@4");
        BinkUseTmLite = AutoGetProcAddress(dll, "_BinkUseTmLite@4");
        BinkUtilCPUs = AutoGetProcAddress(dll, "_BinkUtilCPUs@0");
        BinkUtilFree = AutoGetProcAddress(dll, "_BinkUtilFree@4");
        BinkUtilMalloc = AutoGetProcAddress(dll, "_BinkUtilMalloc@8");
        BinkUtilMutexCreate = AutoGetProcAddress(dll, "_BinkUtilMutexCreate@8");
        BinkUtilMutexDestroy = AutoGetProcAddress(dll, "_BinkUtilMutexDestroy@4");
        BinkUtilMutexLock = AutoGetProcAddress(dll, "_BinkUtilMutexLock@4");
        BinkUtilMutexLockTimeOut = AutoGetProcAddress(dll, "_BinkUtilMutexLockTimeOut@8");
        BinkUtilMutexUnlock = AutoGetProcAddress(dll, "_BinkUtilMutexUnlock@4");
        BinkUtilSoundGlobalLock = AutoGetProcAddress(dll, "_BinkUtilSoundGlobalLock@0");
        BinkUtilSoundGlobalUnlock = AutoGetProcAddress(dll, "_BinkUtilSoundGlobalUnlock@0");
        BinkWait = AutoGetProcAddress(dll, "_BinkWait@4");
        BinkWaitStopAsyncThread = AutoGetProcAddress(dll, "_BinkWaitStopAsyncThread@4");
        BinkWaitStopAsyncThreadsMulti = AutoGetProcAddress(dll, "_BinkWaitStopAsyncThreadsMulti@8");
        RADSetMemory = AutoGetProcAddress(dll, "_RADSetMemory@8");
        RADTimerRead = AutoGetProcAddress(dll, "_RADTimerRead@0");
        YUV_blit_16a1bpp40 = AutoGetProcAddress(dll, "_YUV_blit_16a1bpp@40");
        YUV_blit_16a1bpp52 = AutoGetProcAddress(dll, "_YUV_blit_16a1bpp@52");
        YUV_blit_16a1bpp_mask48 = AutoGetProcAddress(dll, "_YUV_blit_16a1bpp_mask@48");
        YUV_blit_16a1bpp_mask60 = AutoGetProcAddress(dll, "_YUV_blit_16a1bpp_mask@60");
        YUV_blit_16a4bpp40 = AutoGetProcAddress(dll, "_YUV_blit_16a4bpp@40");
        YUV_blit_16a4bpp52 = AutoGetProcAddress(dll, "_YUV_blit_16a4bpp@52");
        YUV_blit_16a4bpp_mask48 = AutoGetProcAddress(dll, "_YUV_blit_16a4bpp_mask@48");
        YUV_blit_16a4bpp_mask60 = AutoGetProcAddress(dll, "_YUV_blit_16a4bpp_mask@60");
        YUV_blit_16bpp40 = AutoGetProcAddress(dll, "_YUV_blit_16bpp@40");
        YUV_blit_16bpp48 = AutoGetProcAddress(dll, "_YUV_blit_16bpp@48");
        YUV_blit_16bpp52 = AutoGetProcAddress(dll, "_YUV_blit_16bpp@52");
        YUV_blit_16bpp_mask48 = AutoGetProcAddress(dll, "_YUV_blit_16bpp_mask@48");
        YUV_blit_16bpp_mask56 = AutoGetProcAddress(dll, "_YUV_blit_16bpp_mask@56");
        YUV_blit_16bpp_mask60 = AutoGetProcAddress(dll, "_YUV_blit_16bpp_mask@60");
        YUV_blit_24bpp40 = AutoGetProcAddress(dll, "_YUV_blit_24bpp@40");
        YUV_blit_24bpp48 = AutoGetProcAddress(dll, "_YUV_blit_24bpp@48");
        YUV_blit_24bpp52 = AutoGetProcAddress(dll, "_YUV_blit_24bpp@52");
        YUV_blit_24bpp_mask48 = AutoGetProcAddress(dll, "_YUV_blit_24bpp_mask@48");
        YUV_blit_24bpp_mask56 = AutoGetProcAddress(dll, "_YUV_blit_24bpp_mask@56");
        YUV_blit_24bpp_mask60 = AutoGetProcAddress(dll, "_YUV_blit_24bpp_mask@60");
        YUV_blit_24rbpp40 = AutoGetProcAddress(dll, "_YUV_blit_24rbpp@40");
        YUV_blit_24rbpp48 = AutoGetProcAddress(dll, "_YUV_blit_24rbpp@48");
        YUV_blit_24rbpp52 = AutoGetProcAddress(dll, "_YUV_blit_24rbpp@52");
        YUV_blit_24rbpp_mask48 = AutoGetProcAddress(dll, "_YUV_blit_24rbpp_mask@48");
        YUV_blit_24rbpp_mask56 = AutoGetProcAddress(dll, "_YUV_blit_24rbpp_mask@56");
        YUV_blit_24rbpp_mask60 = AutoGetProcAddress(dll, "_YUV_blit_24rbpp_mask@60");
        YUV_blit_32abpp40 = AutoGetProcAddress(dll, "_YUV_blit_32abpp@40");
        YUV_blit_32abpp52 = AutoGetProcAddress(dll, "_YUV_blit_32abpp@52");
        YUV_blit_32abpp_mask48 = AutoGetProcAddress(dll, "_YUV_blit_32abpp_mask@48");
        YUV_blit_32abpp_mask60 = AutoGetProcAddress(dll, "_YUV_blit_32abpp_mask@60");
        YUV_blit_32bpp40 = AutoGetProcAddress(dll, "_YUV_blit_32bpp@40");
        YUV_blit_32bpp48 = AutoGetProcAddress(dll, "_YUV_blit_32bpp@48");
        YUV_blit_32bpp52 = AutoGetProcAddress(dll, "_YUV_blit_32bpp@52");
        YUV_blit_32bpp_mask48 = AutoGetProcAddress(dll, "_YUV_blit_32bpp_mask@48");
        YUV_blit_32bpp_mask56 = AutoGetProcAddress(dll, "_YUV_blit_32bpp_mask@56");
        YUV_blit_32bpp_mask60 = AutoGetProcAddress(dll, "_YUV_blit_32bpp_mask@60");
        YUV_blit_32rabpp40 = AutoGetProcAddress(dll, "_YUV_blit_32rabpp@40");
        YUV_blit_32rabpp52 = AutoGetProcAddress(dll, "_YUV_blit_32rabpp@52");
        YUV_blit_32rabpp_mask48 = AutoGetProcAddress(dll, "_YUV_blit_32rabpp_mask@48");
        YUV_blit_32rabpp_mask60 = AutoGetProcAddress(dll, "_YUV_blit_32rabpp_mask@60");
        YUV_blit_32rbpp40 = AutoGetProcAddress(dll, "_YUV_blit_32rbpp@40");
        YUV_blit_32rbpp48 = AutoGetProcAddress(dll, "_YUV_blit_32rbpp@48");
        YUV_blit_32rbpp52 = AutoGetProcAddress(dll, "_YUV_blit_32rbpp@52");
        YUV_blit_32rbpp_mask48 = AutoGetProcAddress(dll, "_YUV_blit_32rbpp_mask@48");
        YUV_blit_32rbpp_mask56 = AutoGetProcAddress(dll, "_YUV_blit_32rbpp_mask@56");
        YUV_blit_32rbpp_mask60 = AutoGetProcAddress(dll, "_YUV_blit_32rbpp_mask@60");
        YUV_blit_UYVY40 = AutoGetProcAddress(dll, "_YUV_blit_UYVY@40");
        YUV_blit_UYVY48 = AutoGetProcAddress(dll, "_YUV_blit_UYVY@48");
        YUV_blit_UYVY52 = AutoGetProcAddress(dll, "_YUV_blit_UYVY@52");
        YUV_blit_UYVY_mask48 = AutoGetProcAddress(dll, "_YUV_blit_UYVY_mask@48");
        YUV_blit_UYVY_mask56 = AutoGetProcAddress(dll, "_YUV_blit_UYVY_mask@56");
        YUV_blit_UYVY_mask60 = AutoGetProcAddress(dll, "_YUV_blit_UYVY_mask@60");
        YUV_blit_YUY240 = AutoGetProcAddress(dll, "_YUV_blit_YUY2@40");
        YUV_blit_YUY248 = AutoGetProcAddress(dll, "_YUV_blit_YUY2@48");
        YUV_blit_YUY252 = AutoGetProcAddress(dll, "_YUV_blit_YUY2@52");
        YUV_blit_YUY2_mask48 = AutoGetProcAddress(dll, "_YUV_blit_YUY2_mask@48");
        YUV_blit_YUY2_mask56 = AutoGetProcAddress(dll, "_YUV_blit_YUY2_mask@56");
        YUV_blit_YUY2_mask60 = AutoGetProcAddress(dll, "_YUV_blit_YUY2_mask@60");
        YUV_blit_YV1244 = AutoGetProcAddress(dll, "_YUV_blit_YV12@44");
        YUV_blit_YV1252 = AutoGetProcAddress(dll, "_YUV_blit_YV12@52");
        YUV_blit_YV1256 = AutoGetProcAddress(dll, "_YUV_blit_YV12@56");
        YUV_init4 = AutoGetProcAddress(dll, "_YUV_init@4");
        radfree = AutoGetProcAddress(dll, "_radfree@4");
        radmalloc = AutoGetProcAddress(dll, "_radmalloc@4");
    }
} bink2w32;

extern "C" __declspec(naked) void __stdcall _BinkAllocateFrameBuffers(int, int, int) { _asm { jmp[bink2w32.BinkAllocateFrameBuffers] } }
extern "C" __declspec(naked) void __stdcall _BinkBufferBlit(int, int, int) { _asm { jmp[bink2w32.BinkBufferBlit] } }
extern "C" __declspec(naked) void __stdcall _BinkBufferCheckWinPos(int, int, int) { _asm { jmp[bink2w32.BinkBufferCheckWinPos] } }
extern "C" __declspec(naked) void __stdcall _BinkBufferClear(int, int) { _asm { jmp[bink2w32.BinkBufferClear] } }
extern "C" __declspec(naked) void __stdcall _BinkBufferClose(int) { _asm { jmp[bink2w32.BinkBufferClose] } }
extern "C" __declspec(naked) void __stdcall _BinkBufferGetDescription(int) { _asm { jmp[bink2w32.BinkBufferGetDescription] } }
extern "C" __declspec(naked) void __stdcall _BinkBufferGetError() { _asm { jmp[bink2w32.BinkBufferGetError] } }
extern "C" __declspec(naked) void __stdcall _BinkBufferLock(int) { _asm { jmp[bink2w32.BinkBufferLock] } }
extern "C" __declspec(naked) void __stdcall _BinkBufferOpen(int, int, int, int) { _asm { jmp[bink2w32.BinkBufferOpen] } }
extern "C" __declspec(naked) void __stdcall _BinkBufferSetDirectDraw(int, int) { _asm { jmp[bink2w32.BinkBufferSetDirectDraw] } }
extern "C" __declspec(naked) void __stdcall _BinkBufferSetHWND(int, int) { _asm { jmp[bink2w32.BinkBufferSetHWND] } }
extern "C" __declspec(naked) void __stdcall _BinkBufferSetOffset(int, int, int) { _asm { jmp[bink2w32.BinkBufferSetOffset] } }
extern "C" __declspec(naked) void __stdcall _BinkBufferSetResolution(int, int, int) { _asm { jmp[bink2w32.BinkBufferSetResolution] } }
extern "C" __declspec(naked) void __stdcall _BinkBufferSetScale(int, int, int) { _asm { jmp[bink2w32.BinkBufferSetScale] } }
extern "C" __declspec(naked) void __stdcall _BinkBufferUnlock(int) { _asm { jmp[bink2w32.BinkBufferUnlock] } }
extern "C" __declspec(naked) void __stdcall _BinkCheckCursor(int, int, int, int, int) { _asm { jmp[bink2w32.BinkCheckCursor] } }
extern "C" __declspec(naked) void __stdcall _BinkClose(int) { _asm { jmp[bink2w32.BinkClose] } }
extern "C" __declspec(naked) void __stdcall _BinkCloseTrack(int) { _asm { jmp[bink2w32.BinkCloseTrack] } }
extern "C" __declspec(naked) void __stdcall _BinkControlBackgroundIO(int, int) { _asm { jmp[bink2w32.BinkControlBackgroundIO] } }
extern "C" __declspec(naked) void __stdcall _BinkControlPlatformFeatures(int, int) { _asm { jmp[bink2w32.BinkControlPlatformFeatures] } }
extern "C" __declspec(naked) void __stdcall _BinkCopyToBuffer(int, int, int, int, int, int, int) { _asm { jmp[bink2w32.BinkCopyToBuffer] } }
extern "C" __declspec(naked) void __stdcall _BinkCopyToBufferRect(int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.BinkCopyToBufferRect] } }
extern "C" __declspec(naked) void __stdcall _BinkCurrentSubtitle(int, int, int, int) { _asm { jmp[bink2w32.BinkCurrentSubtitle] } }
extern "C" __declspec(naked) void __stdcall _BinkDDSurfaceType(int) { _asm { jmp[bink2w32.BinkDDSurfaceType] } }
extern "C" __declspec(naked) void __stdcall _BinkDX8SurfaceType(int) { _asm { jmp[bink2w32.BinkDX8SurfaceType] } }
extern "C" __declspec(naked) void __stdcall _BinkDX9SurfaceType(int) { _asm { jmp[bink2w32.BinkDX9SurfaceType] } }
extern "C" __declspec(naked) void __stdcall _BinkDoFrame(int) { _asm { jmp[bink2w32.BinkDoFrame] } }
extern "C" __declspec(naked) void __stdcall _BinkDoFrameAsync(int, int, int) { _asm { jmp[bink2w32.BinkDoFrameAsync] } }
extern "C" __declspec(naked) void __stdcall _BinkDoFrameAsyncMulti(int, int, int) { _asm { jmp[bink2w32.BinkDoFrameAsyncMulti] } }
extern "C" __declspec(naked) void __stdcall _BinkDoFrameAsyncWait(int, int) { _asm { jmp[bink2w32.BinkDoFrameAsyncWait] } }
extern "C" __declspec(naked) void __stdcall _BinkDoFramePlane(int, int) { _asm { jmp[bink2w32.BinkDoFramePlane] } }
extern "C" __declspec(naked) void __stdcall _BinkFindXAudio2WinDevice(int, int) { _asm { jmp[bink2w32.BinkFindXAudio2WinDevice] } }
extern "C" __declspec(naked) void __stdcall _BinkFreeGlobals() { _asm { jmp[bink2w32.BinkFreeGlobals] } }
extern "C" __declspec(naked) void __stdcall _BinkGetError() { _asm { jmp[bink2w32.BinkGetError] } }
extern "C" __declspec(naked) void __stdcall _BinkGetFrameBuffersInfo(int, int) { _asm { jmp[bink2w32.BinkGetFrameBuffersInfo] } }
extern "C" __declspec(naked) void __stdcall _BinkGetGPUDataBuffersInfo(int, int) { _asm { jmp[bink2w32.BinkGetGPUDataBuffersInfo] } }
extern "C" __declspec(naked) void __stdcall _BinkGetKeyFrame(int, int, int) { _asm { jmp[bink2w32.BinkGetKeyFrame] } }
extern "C" __declspec(naked) void __stdcall _BinkGetPalette(int) { _asm { jmp[bink2w32.BinkGetPalette] } }
extern "C" __declspec(naked) void __stdcall _BinkGetPlatformInfo(int, int) { _asm { jmp[bink2w32.BinkGetPlatformInfo] } }
extern "C" __declspec(naked) void __stdcall _BinkGetRealtime(int, int, int) { _asm { jmp[bink2w32.BinkGetRealtime] } }
extern "C" __declspec(naked) void __stdcall _BinkGetRects(int, int) { _asm { jmp[bink2w32.BinkGetRects] } }
extern "C" __declspec(naked) void __stdcall _BinkGetRects4(int) { _asm { jmp[bink2w32.BinkGetRects4] } }
extern "C" __declspec(naked) void __stdcall _BinkGetSubtitleByIndex(int, int, int, int) { _asm { jmp[bink2w32.BinkGetSubtitleByIndex] } }
extern "C" __declspec(naked) void __stdcall _BinkGetSummary(int, int) { _asm { jmp[bink2w32.BinkGetSummary] } }
extern "C" __declspec(naked) void __stdcall _BinkGetTrackData(int, int) { _asm { jmp[bink2w32.BinkGetTrackData] } }
extern "C" __declspec(naked) void __stdcall _BinkGetTrackData12(int, int, int) { _asm { jmp[bink2w32.BinkGetTrackData12] } }
extern "C" __declspec(naked) void __stdcall _BinkGetTrackID(int, int) { _asm { jmp[bink2w32.BinkGetTrackID] } }
extern "C" __declspec(naked) void __stdcall _BinkGetTrackMaxSize(int, int) { _asm { jmp[bink2w32.BinkGetTrackMaxSize] } }
extern "C" __declspec(naked) void __stdcall _BinkGetTrackType(int, int) { _asm { jmp[bink2w32.BinkGetTrackType] } }
extern "C" __declspec(naked) void __stdcall _BinkGoto(int, int, int) { _asm { jmp[bink2w32.BinkGoto] } }
extern "C" __declspec(naked) void __stdcall _BinkIsSoftwareCursor(int, int) { _asm { jmp[bink2w32.BinkIsSoftwareCursor] } }
extern "C" __declspec(naked) void __stdcall _BinkLoadSubtitles(int, int) { _asm { jmp[bink2w32.BinkLoadSubtitles] } }
extern "C" __declspec(naked) void __stdcall _BinkLogoAddress() { _asm { jmp[bink2w32.BinkLogoAddress] } }
extern "C" __declspec(naked) void __stdcall _BinkNextFrame(int) { _asm { jmp[bink2w32.BinkNextFrame] } }
extern "C" __declspec(naked) void __stdcall _BinkOpen(int, int) { _asm { jmp[bink2w32.BinkOpen] } }
extern "C" __declspec(naked) void __stdcall _BinkOpenDirectSound(int) { _asm { jmp[bink2w32.BinkOpenDirectSound] } }
extern "C" __declspec(naked) void __stdcall _BinkOpenMiles(int) { _asm { jmp[bink2w32.BinkOpenMiles] } }
extern "C" __declspec(naked) void __stdcall _BinkOpenTrack(int, int) { _asm { jmp[bink2w32.BinkOpenTrack] } }
extern "C" __declspec(naked) void __stdcall _BinkOpenWaveOut(int) { _asm { jmp[bink2w32.BinkOpenWaveOut] } }
extern "C" __declspec(naked) void __stdcall _BinkOpenWithOptions(int, int, int) { _asm { jmp[bink2w32.BinkOpenWithOptions] } }
extern "C" __declspec(naked) void __stdcall _BinkOpenXAudio2(int) { _asm { jmp[bink2w32.BinkOpenXAudio2] } }
extern "C" __declspec(naked) void __stdcall _BinkOpenXAudio2_8(int, int) { _asm { jmp[bink2w32.BinkOpenXAudio2_8] } }
extern "C" __declspec(naked) void __stdcall _BinkOpenXAudio27(int, int) { _asm { jmp[bink2w32.BinkOpenXAudio27] } }
extern "C" __declspec(naked) void __stdcall _BinkOpenXAudio28(int, int) { _asm { jmp[bink2w32.BinkOpenXAudio28] } }
extern "C" __declspec(naked) void __stdcall _BinkOpenXAudio29(int, int) { _asm { jmp[bink2w32.BinkOpenXAudio29] } }
extern "C" __declspec(naked) void __stdcall _BinkPause(int, int) { _asm { jmp[bink2w32.BinkPause] } }
extern "C" __declspec(naked) void __stdcall _BinkRegisterFrameBuffers(int, int) { _asm { jmp[bink2w32.BinkRegisterFrameBuffers] } }
extern "C" __declspec(naked) void __stdcall _BinkRegisterGPUDataBuffers(int, int) { _asm { jmp[bink2w32.BinkRegisterGPUDataBuffers] } }
extern "C" __declspec(naked) void __stdcall _BinkRequestStopAsyncThread(int) { _asm { jmp[bink2w32.BinkRequestStopAsyncThread] } }
extern "C" __declspec(naked) void __stdcall _BinkRequestStopAsyncThreadsMulti(int, int) { _asm { jmp[bink2w32.BinkRequestStopAsyncThreadsMulti] } }
extern "C" __declspec(naked) void __stdcall _BinkRestoreCursor(int) { _asm { jmp[bink2w32.BinkRestoreCursor] } }
extern "C" __declspec(naked) void __stdcall _BinkService(int) { _asm { jmp[bink2w32.BinkService] } }
extern "C" __declspec(naked) void __stdcall _BinkServiceSound() { _asm { jmp[bink2w32.BinkServiceSound] } }
extern "C" __declspec(naked) void __stdcall _BinkSetError(int) { _asm { jmp[bink2w32.BinkSetError] } }
extern "C" __declspec(naked) void __stdcall _BinkSetFileOffset(int, int) { _asm { jmp[bink2w32.BinkSetFileOffset] } }
extern "C" __declspec(naked) void __stdcall _BinkSetFrameRate(int, int) { _asm { jmp[bink2w32.BinkSetFrameRate] } }
extern "C" __declspec(naked) void __stdcall _BinkSetIO(int) { _asm { jmp[bink2w32.BinkSetIO] } }
extern "C" __declspec(naked) void __stdcall _BinkSetIOSize(int) { _asm { jmp[bink2w32.BinkSetIOSize] } }
extern "C" __declspec(naked) void __stdcall _BinkSetIOSize8(int, int) { _asm { jmp[bink2w32.BinkSetIOSize8] } }
extern "C" __declspec(naked) void __stdcall _BinkSetMemory(int, int) { _asm { jmp[bink2w32.BinkSetMemory] } }
extern "C" __declspec(naked) void __stdcall _BinkSetMixBinVolumes(int, int, int, int, int) { _asm { jmp[bink2w32.BinkSetMixBinVolumes] } }
extern "C" __declspec(naked) void __stdcall _BinkSetMixBins(int, int, int, int) { _asm { jmp[bink2w32.BinkSetMixBins] } }
extern "C" __declspec(naked) void __stdcall _BinkSetOSFileCallbacks(int, int, int, int) { _asm { jmp[bink2w32.BinkSetOSFileCallbacks] } }
extern "C" __declspec(naked) void __stdcall _BinkSetPan(int, int, int) { _asm { jmp[bink2w32.BinkSetPan] } }
extern "C" __declspec(naked) void __stdcall _BinkSetSimulate(int) { _asm { jmp[bink2w32.BinkSetSimulate] } }
extern "C" __declspec(naked) void __stdcall _BinkSetSoundOnOff(int, int) { _asm { jmp[bink2w32.BinkSetSoundOnOff] } }
extern "C" __declspec(naked) void __stdcall _BinkSetSoundSystem(int, int) { _asm { jmp[bink2w32.BinkSetSoundSystem] } }
extern "C" __declspec(naked) void __stdcall _BinkSetSoundSystem2(int, int, int) { _asm { jmp[bink2w32.BinkSetSoundSystem2] } }
extern "C" __declspec(naked) void __stdcall _BinkSetSoundTrack(int, int) { _asm { jmp[bink2w32.BinkSetSoundTrack] } }
extern "C" __declspec(naked) void __stdcall _BinkSetSpeakerVolumes(int, int, int, int, int) { _asm { jmp[bink2w32.BinkSetSpeakerVolumes] } }
extern "C" __declspec(naked) void __stdcall _BinkSetVideoOnOff(int, int) { _asm { jmp[bink2w32.BinkSetVideoOnOff] } }
extern "C" __declspec(naked) void __stdcall _BinkSetVolume(int, int, int) { _asm { jmp[bink2w32.BinkSetVolume] } }
extern "C" __declspec(naked) void __stdcall _BinkSetWillLoop(int, int) { _asm { jmp[bink2w32.BinkSetWillLoop] } }
extern "C" __declspec(naked) void __stdcall _BinkShouldSkip(int) { _asm { jmp[bink2w32.BinkShouldSkip] } }
extern "C" __declspec(naked) void __stdcall _BinkStartAsyncThread(int, int) { _asm { jmp[bink2w32.BinkStartAsyncThread] } }
extern "C" __declspec(naked) void __stdcall _BinkUseTelemetry(int) { _asm { jmp[bink2w32.BinkUseTelemetry] } }
extern "C" __declspec(naked) void __stdcall _BinkUseTmLite(int) { _asm { jmp[bink2w32.BinkUseTmLite] } }
extern "C" __declspec(naked) void __stdcall _BinkUtilCPUs() { _asm { jmp[bink2w32.BinkUtilCPUs] } }
extern "C" __declspec(naked) void __stdcall _BinkUtilFree(int) { _asm { jmp[bink2w32.BinkUtilFree] } }
extern "C" __declspec(naked) void __stdcall _BinkUtilMalloc(int, int) { _asm { jmp[bink2w32.BinkUtilMalloc] } }
extern "C" __declspec(naked) void __stdcall _BinkUtilMutexCreate(int, int) { _asm { jmp[bink2w32.BinkUtilMutexCreate] } }
extern "C" __declspec(naked) void __stdcall _BinkUtilMutexDestroy(int) { _asm { jmp[bink2w32.BinkUtilMutexDestroy] } }
extern "C" __declspec(naked) void __stdcall _BinkUtilMutexLock(int) { _asm { jmp[bink2w32.BinkUtilMutexLock] } }
extern "C" __declspec(naked) void __stdcall _BinkUtilMutexLockTimeOut(int, int) { _asm { jmp[bink2w32.BinkUtilMutexLockTimeOut] } }
extern "C" __declspec(naked) void __stdcall _BinkUtilMutexUnlock(int) { _asm { jmp[bink2w32.BinkUtilMutexUnlock] } }
extern "C" __declspec(naked) void __stdcall _BinkUtilSoundGlobalLock() { _asm { jmp[bink2w32.BinkUtilSoundGlobalLock] } }
extern "C" __declspec(naked) void __stdcall _BinkUtilSoundGlobalUnlock() { _asm { jmp[bink2w32.BinkUtilSoundGlobalUnlock] } }
extern "C" __declspec(naked) void __stdcall _BinkWait(int) { _asm { jmp[bink2w32.BinkWait] } }
extern "C" __declspec(naked) void __stdcall _BinkWaitStopAsyncThread(int) { _asm { jmp[bink2w32.BinkWaitStopAsyncThread] } }
extern "C" __declspec(naked) void __stdcall _BinkWaitStopAsyncThreadsMulti(int, int) { _asm { jmp[bink2w32.BinkWaitStopAsyncThreadsMulti] } }
extern "C" __declspec(naked) void __stdcall _RADSetMemory(int, int) { _asm { jmp[bink2w32.RADSetMemory] } }
extern "C" __declspec(naked) void __stdcall _RADTimerRead() { _asm { jmp[bink2w32.RADTimerRead] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_16a1bpp40(int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_16a1bpp40] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_16a1bpp52(int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_16a1bpp52] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_16a1bpp_mask48(int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_16a1bpp_mask48] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_16a1bpp_mask60(int, int, int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_16a1bpp_mask60] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_16a4bpp40(int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_16a4bpp40] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_16a4bpp52(int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_16a4bpp52] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_16a4bpp_mask48(int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_16a4bpp_mask48] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_16a4bpp_mask60(int, int, int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_16a4bpp_mask60] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_16bpp40(int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_16bpp40] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_16bpp48(int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_16bpp48] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_16bpp52(int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_16bpp52] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_16bpp_mask48(int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_16bpp_mask48] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_16bpp_mask56(int, int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_16bpp_mask56] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_16bpp_mask60(int, int, int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_16bpp_mask60] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_24bpp40(int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_24bpp40] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_24bpp48(int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_24bpp48] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_24bpp52(int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_24bpp52] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_24bpp_mask48(int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_24bpp_mask48] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_24bpp_mask56(int, int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_24bpp_mask56] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_24bpp_mask60(int, int, int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_24bpp_mask60] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_24rbpp40(int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_24rbpp40] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_24rbpp48(int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_24rbpp48] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_24rbpp52(int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_24rbpp52] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_24rbpp_mask48(int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_24rbpp_mask48] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_24rbpp_mask56(int, int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_24rbpp_mask56] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_24rbpp_mask60(int, int, int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_24rbpp_mask60] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_32abpp40(int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_32abpp40] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_32abpp52(int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_32abpp52] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_32abpp_mask48(int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_32abpp_mask48] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_32abpp_mask60(int, int, int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_32abpp_mask60] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_32bpp40(int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_32bpp40] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_32bpp48(int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_32bpp48] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_32bpp52(int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_32bpp52] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_32bpp_mask48(int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_32bpp_mask48] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_32bpp_mask56(int, int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_32bpp_mask56] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_32bpp_mask60(int, int, int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_32bpp_mask60] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_32rabpp40(int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_32rabpp40] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_32rabpp52(int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_32rabpp52] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_32rabpp_mask48(int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_32rabpp_mask48] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_32rabpp_mask60(int, int, int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_32rabpp_mask60] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_32rbpp40(int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_32rbpp40] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_32rbpp48(int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_32rbpp48] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_32rbpp52(int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_32rbpp52] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_32rbpp_mask48(int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_32rbpp_mask48] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_32rbpp_mask56(int, int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_32rbpp_mask56] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_32rbpp_mask60(int, int, int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_32rbpp_mask60] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_UYVY40(int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_UYVY40] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_UYVY48(int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_UYVY48] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_UYVY52(int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_UYVY52] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_UYVY_mask48(int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_UYVY_mask48] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_UYVY_mask56(int, int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_UYVY_mask56] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_UYVY_mask60(int, int, int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_UYVY_mask60] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_YUY240(int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_YUY240] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_YUY248(int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_YUY248] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_YUY252(int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_YUY252] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_YUY2_mask48(int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_YUY2_mask48] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_YUY2_mask56(int, int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_YUY2_mask56] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_YUY2_mask60(int, int, int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_YUY2_mask60] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_YV1244(int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_YV1244] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_YV1252(int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_YV1252] } }
extern "C" __declspec(naked) void __stdcall _YUV_blit_YV1256(int, int, int, int, int, int, int, int, int, int, int, int, int, int) { _asm { jmp[bink2w32.YUV_blit_YV1256] } }
extern "C" __declspec(naked) void __stdcall _YUV_init4(int) { _asm { jmp[bink2w32.YUV_init4] } }
extern "C" __declspec(naked) void __stdcall _radfree(int) { _asm { jmp[bink2w32.radfree] } }
extern "C" __declspec(naked) void __stdcall _radmalloc(int) { _asm { jmp[bink2w32.radmalloc] } }
#else
#pragma runtime_checks( "", off )
struct bink2w64_dll
{
    HMODULE dll;
    FARPROC BinkAllocateFrameBuffers;
    FARPROC BinkBufferBlit;
    FARPROC BinkBufferCheckWinPos;
    FARPROC BinkBufferClear;
    FARPROC BinkBufferClose;
    FARPROC BinkBufferGetDescription;
    FARPROC BinkBufferGetError;
    FARPROC BinkBufferLock;
    FARPROC BinkBufferOpen;
    FARPROC BinkBufferSetDirectDraw;
    FARPROC BinkBufferSetHWND;
    FARPROC BinkBufferSetOffset;
    FARPROC BinkBufferSetResolution;
    FARPROC BinkBufferSetScale;
    FARPROC BinkBufferUnlock;
    FARPROC BinkCheckCursor;
    FARPROC BinkClose;
    FARPROC BinkCloseTrack;
    FARPROC BinkControlBackgroundIO;
    FARPROC BinkControlPlatformFeatures;
    FARPROC BinkCopyToBuffer;
    FARPROC BinkCopyToBufferRect;
    FARPROC BinkCurrentSubtitle;
    FARPROC BinkDDSurfaceType;
    FARPROC BinkDX8SurfaceType;
    FARPROC BinkDX9SurfaceType;
    FARPROC BinkDoFrame;
    FARPROC BinkDoFrameAsync;
    FARPROC BinkDoFrameAsyncMulti;
    FARPROC BinkDoFrameAsyncWait;
    FARPROC BinkDoFramePlane;
    FARPROC BinkFindXAudio2WinDevice;
    FARPROC BinkFreeGlobals;
    FARPROC BinkGetError;
    FARPROC BinkGetFrameBuffersInfo;
    FARPROC BinkGetGPUDataBuffersInfo;
    FARPROC BinkGetKeyFrame;
    FARPROC BinkGetPalette;
    FARPROC BinkGetPlatformInfo;
    FARPROC BinkGetRealtime;
    FARPROC BinkGetRects;
    FARPROC BinkGetSubtitleByIndex;
    FARPROC BinkGetSummary;
    FARPROC BinkGetTrackData;
    FARPROC BinkGetTrackID;
    FARPROC BinkGetTrackMaxSize;
    FARPROC BinkGetTrackType;
    FARPROC BinkGoto;
    FARPROC BinkIsSoftwareCursor;
    FARPROC BinkLoadSubtitles;
    FARPROC BinkLogoAddress;
    FARPROC BinkNextFrame;
    FARPROC BinkOpen;
    FARPROC BinkOpenDirectSound;
    FARPROC BinkOpenMiles;
    FARPROC BinkOpenTrack;
    FARPROC BinkOpenWaveOut;
    FARPROC BinkOpenWithOptions;
    FARPROC BinkOpenXAudio2;
    FARPROC BinkOpenXAudio27;
    FARPROC BinkOpenXAudio28;
    FARPROC BinkOpenXAudio29;
    FARPROC BinkPause;
    FARPROC BinkRegisterFrameBuffers;
    FARPROC BinkRegisterGPUDataBuffers;
    FARPROC BinkRequestStopAsyncThread;
    FARPROC BinkRequestStopAsyncThreadsMulti;
    FARPROC BinkRestoreCursor;
    FARPROC BinkService;
    FARPROC BinkServiceSound;
    FARPROC BinkSetError;
    FARPROC BinkSetFileOffset;
    FARPROC BinkSetFrameRate;
    FARPROC BinkSetIO;
    FARPROC BinkSetIOSize;
    FARPROC BinkSetMemory;
    FARPROC BinkSetOSFileCallbacks;
    FARPROC BinkSetPan;
    FARPROC BinkSetSimulate;
    FARPROC BinkSetSoundOnOff;
    FARPROC BinkSetSoundSystem;
    FARPROC BinkSetSoundSystem2;
    FARPROC BinkSetSoundTrack;
    FARPROC BinkSetSpeakerVolumes;
    FARPROC BinkSetVideoOnOff;
    FARPROC BinkSetVolume;
    FARPROC BinkSetWillLoop;
    FARPROC BinkShouldSkip;
    FARPROC BinkStartAsyncThread;
    FARPROC BinkUtilCPUs;
    FARPROC BinkUtilFree;
    FARPROC BinkUtilMalloc;
    FARPROC BinkUtilMutexCreate;
    FARPROC BinkUtilMutexDestroy;
    FARPROC BinkUtilMutexLock;
    FARPROC BinkUtilMutexLockTimeOut;
    FARPROC BinkUtilMutexUnlock;
    FARPROC BinkUtilSoundGlobalLock;
    FARPROC BinkUtilSoundGlobalUnlock;
    FARPROC BinkWait;
    FARPROC BinkWaitStopAsyncThread;
    FARPROC BinkWaitStopAsyncThreadsMulti;
    FARPROC RADTimerRead;

    void LoadOriginalLibrary(HMODULE module)
    {
        dll = module;
        LoadShared((HMODULE)dll);
        BinkAllocateFrameBuffers = GetProcAddress(dll, "BinkAllocateFrameBuffers");
        BinkBufferBlit = GetProcAddress(dll, "BinkBufferBlit");
        BinkBufferCheckWinPos = GetProcAddress(dll, "BinkBufferCheckWinPos");
        BinkBufferClear = GetProcAddress(dll, "BinkBufferClear");
        BinkBufferClose = GetProcAddress(dll, "BinkBufferClose");
        BinkBufferGetDescription = GetProcAddress(dll, "BinkBufferGetDescription");
        BinkBufferGetError = GetProcAddress(dll, "BinkBufferGetError");
        BinkBufferLock = GetProcAddress(dll, "BinkBufferLock");
        BinkBufferOpen = GetProcAddress(dll, "BinkBufferOpen");
        BinkBufferSetDirectDraw = GetProcAddress(dll, "BinkBufferSetDirectDraw");
        BinkBufferSetHWND = GetProcAddress(dll, "BinkBufferSetHWND");
        BinkBufferSetOffset = GetProcAddress(dll, "BinkBufferSetOffset");
        BinkBufferSetResolution = GetProcAddress(dll, "BinkBufferSetResolution");
        BinkBufferSetScale = GetProcAddress(dll, "BinkBufferSetScale");
        BinkBufferUnlock = GetProcAddress(dll, "BinkBufferUnlock");
        BinkCheckCursor = GetProcAddress(dll, "BinkCheckCursor");
        BinkClose = GetProcAddress(dll, "BinkClose");
        BinkCloseTrack = GetProcAddress(dll, "BinkCloseTrack");
        BinkControlBackgroundIO = GetProcAddress(dll, "BinkControlBackgroundIO");
        BinkControlPlatformFeatures = GetProcAddress(dll, "BinkControlPlatformFeatures");
        BinkCopyToBuffer = GetProcAddress(dll, "BinkCopyToBuffer");
        BinkCopyToBufferRect = GetProcAddress(dll, "BinkCopyToBufferRect");
        BinkCurrentSubtitle = GetProcAddress(dll, "BinkCurrentSubtitle");
        BinkDDSurfaceType = GetProcAddress(dll, "BinkDDSurfaceType");
        BinkDX8SurfaceType = GetProcAddress(dll, "BinkDX8SurfaceType");
        BinkDX9SurfaceType = GetProcAddress(dll, "BinkDX9SurfaceType");
        BinkDoFrame = GetProcAddress(dll, "BinkDoFrame");
        BinkDoFrameAsync = GetProcAddress(dll, "BinkDoFrameAsync");
        BinkDoFrameAsyncMulti = GetProcAddress(dll, "BinkDoFrameAsyncMulti");
        BinkDoFrameAsyncWait = GetProcAddress(dll, "BinkDoFrameAsyncWait");
        BinkDoFramePlane = GetProcAddress(dll, "BinkDoFramePlane");
        BinkFindXAudio2WinDevice = GetProcAddress(dll, "BinkFindXAudio2WinDevice");
        BinkFreeGlobals = GetProcAddress(dll, "BinkFreeGlobals");
        BinkGetError = GetProcAddress(dll, "BinkGetError");
        BinkGetFrameBuffersInfo = GetProcAddress(dll, "BinkGetFrameBuffersInfo");
        BinkGetGPUDataBuffersInfo = GetProcAddress(dll, "BinkGetGPUDataBuffersInfo");
        BinkGetKeyFrame = GetProcAddress(dll, "BinkGetKeyFrame");
        BinkGetPalette = GetProcAddress(dll, "BinkGetPalette");
        BinkGetPlatformInfo = GetProcAddress(dll, "BinkGetPlatformInfo");
        BinkGetRealtime = GetProcAddress(dll, "BinkGetRealtime");
        BinkGetRects = GetProcAddress(dll, "BinkGetRects");
        BinkGetSubtitleByIndex = GetProcAddress(dll, "BinkGetSubtitleByIndex");
        BinkGetSummary = GetProcAddress(dll, "BinkGetSummary");
        BinkGetTrackData = GetProcAddress(dll, "BinkGetTrackData");
        BinkGetTrackID = GetProcAddress(dll, "BinkGetTrackID");
        BinkGetTrackMaxSize = GetProcAddress(dll, "BinkGetTrackMaxSize");
        BinkGetTrackType = GetProcAddress(dll, "BinkGetTrackType");
        BinkGoto = GetProcAddress(dll, "BinkGoto");
        BinkIsSoftwareCursor = GetProcAddress(dll, "BinkIsSoftwareCursor");
        BinkLoadSubtitles = GetProcAddress(dll, "BinkLoadSubtitles");
        BinkLogoAddress = GetProcAddress(dll, "BinkLogoAddress");
        BinkNextFrame = GetProcAddress(dll, "BinkNextFrame");
        BinkOpen = GetProcAddress(dll, "BinkOpen");
        BinkOpenDirectSound = GetProcAddress(dll, "BinkOpenDirectSound");
        BinkOpenMiles = GetProcAddress(dll, "BinkOpenMiles");
        BinkOpenTrack = GetProcAddress(dll, "BinkOpenTrack");
        BinkOpenWaveOut = GetProcAddress(dll, "BinkOpenWaveOut");
        BinkOpenWithOptions = GetProcAddress(dll, "BinkOpenWithOptions");
        BinkOpenXAudio2 = GetProcAddress(dll, "BinkOpenXAudio2");
        BinkOpenXAudio27 = GetProcAddress(dll, "BinkOpenXAudio27");
        BinkOpenXAudio28 = GetProcAddress(dll, "BinkOpenXAudio28");
        BinkOpenXAudio29 = GetProcAddress(dll, "BinkOpenXAudio29");
        BinkPause = GetProcAddress(dll, "BinkPause");
        BinkRegisterFrameBuffers = GetProcAddress(dll, "BinkRegisterFrameBuffers");
        BinkRegisterGPUDataBuffers = GetProcAddress(dll, "BinkRegisterGPUDataBuffers");
        BinkRequestStopAsyncThread = GetProcAddress(dll, "BinkRequestStopAsyncThread");
        BinkRequestStopAsyncThreadsMulti = GetProcAddress(dll, "BinkRequestStopAsyncThreadsMulti");
        BinkRestoreCursor = GetProcAddress(dll, "BinkRestoreCursor");
        BinkService = GetProcAddress(dll, "BinkService");
        BinkServiceSound = GetProcAddress(dll, "BinkServiceSound");
        BinkSetError = GetProcAddress(dll, "BinkSetError");
        BinkSetFileOffset = GetProcAddress(dll, "BinkSetFileOffset");
        BinkSetFrameRate = GetProcAddress(dll, "BinkSetFrameRate");
        BinkSetIO = GetProcAddress(dll, "BinkSetIO");
        BinkSetIOSize = GetProcAddress(dll, "BinkSetIOSize");
        BinkSetMemory = GetProcAddress(dll, "BinkSetMemory");
        BinkSetOSFileCallbacks = GetProcAddress(dll, "BinkSetOSFileCallbacks");
        BinkSetPan = GetProcAddress(dll, "BinkSetPan");
        BinkSetSimulate = GetProcAddress(dll, "BinkSetSimulate");
        BinkSetSoundOnOff = GetProcAddress(dll, "BinkSetSoundOnOff");
        BinkSetSoundSystem = GetProcAddress(dll, "BinkSetSoundSystem");
        BinkSetSoundSystem2 = GetProcAddress(dll, "BinkSetSoundSystem2");
        BinkSetSoundTrack = GetProcAddress(dll, "BinkSetSoundTrack");
        BinkSetSpeakerVolumes = GetProcAddress(dll, "BinkSetSpeakerVolumes");
        BinkSetVideoOnOff = GetProcAddress(dll, "BinkSetVideoOnOff");
        BinkSetVolume = GetProcAddress(dll, "BinkSetVolume");
        BinkSetWillLoop = GetProcAddress(dll, "BinkSetWillLoop");
        BinkShouldSkip = GetProcAddress(dll, "BinkShouldSkip");
        BinkStartAsyncThread = GetProcAddress(dll, "BinkStartAsyncThread");
        BinkUtilCPUs = GetProcAddress(dll, "BinkUtilCPUs");
        BinkUtilFree = GetProcAddress(dll, "BinkUtilFree");
        BinkUtilMalloc = GetProcAddress(dll, "BinkUtilMalloc");
        BinkUtilMutexCreate = GetProcAddress(dll, "BinkUtilMutexCreate");
        BinkUtilMutexDestroy = GetProcAddress(dll, "BinkUtilMutexDestroy");
        BinkUtilMutexLock = GetProcAddress(dll, "BinkUtilMutexLock");
        BinkUtilMutexLockTimeOut = GetProcAddress(dll, "BinkUtilMutexLockTimeOut");
        BinkUtilMutexUnlock = GetProcAddress(dll, "BinkUtilMutexUnlock");
        BinkUtilSoundGlobalLock = GetProcAddress(dll, "BinkUtilSoundGlobalLock");
        BinkUtilSoundGlobalUnlock = GetProcAddress(dll, "BinkUtilSoundGlobalUnlock");
        BinkWait = GetProcAddress(dll, "BinkWait");
        BinkWaitStopAsyncThread = GetProcAddress(dll, "BinkWaitStopAsyncThread");
        BinkWaitStopAsyncThreadsMulti = GetProcAddress(dll, "BinkWaitStopAsyncThreadsMulti");
        RADTimerRead = GetProcAddress(dll, "RADTimerRead");
    }
} bink2w64;

void _BinkAllocateFrameBuffers() { bink2w64.BinkAllocateFrameBuffers(); }
void _BinkBufferBlit() { bink2w64.BinkBufferBlit(); }
void _BinkBufferCheckWinPos() { bink2w64.BinkBufferCheckWinPos(); }
void _BinkBufferClear() { bink2w64.BinkBufferClear(); }
void _BinkBufferClose() { bink2w64.BinkBufferClose(); }
void _BinkBufferGetDescription() { bink2w64.BinkBufferGetDescription(); }
void _BinkBufferGetError() { bink2w64.BinkBufferGetError(); }
void _BinkBufferLock() { bink2w64.BinkBufferLock(); }
void _BinkBufferOpen() { bink2w64.BinkBufferOpen(); }
void _BinkBufferSetDirectDraw() { bink2w64.BinkBufferSetDirectDraw(); }
void _BinkBufferSetHWND() { bink2w64.BinkBufferSetHWND(); }
void _BinkBufferSetOffset() { bink2w64.BinkBufferSetOffset(); }
void _BinkBufferSetResolution() { bink2w64.BinkBufferSetResolution(); }
void _BinkBufferSetScale() { bink2w64.BinkBufferSetScale(); }
void _BinkBufferUnlock() { bink2w64.BinkBufferUnlock(); }
void _BinkCheckCursor() { bink2w64.BinkCheckCursor(); }
void _BinkClose() { bink2w64.BinkClose(); }
void _BinkCloseTrack() { bink2w64.BinkCloseTrack(); }
void _BinkControlBackgroundIO() { bink2w64.BinkControlBackgroundIO(); }
void _BinkControlPlatformFeatures() { bink2w64.BinkControlPlatformFeatures(); }
void _BinkCopyToBuffer() { bink2w64.BinkCopyToBuffer(); }
void _BinkCopyToBufferRect() { bink2w64.BinkCopyToBufferRect(); }
void _BinkCurrentSubtitle() { bink2w64.BinkCurrentSubtitle(); }
void _BinkDDSurfaceType() { bink2w64.BinkDDSurfaceType(); }
void _BinkDX8SurfaceType() { bink2w64.BinkDX8SurfaceType(); }
void _BinkDX9SurfaceType() { bink2w64.BinkDX9SurfaceType(); }
void _BinkDoFrame() { bink2w64.BinkDoFrame(); }
void _BinkDoFrameAsync() { bink2w64.BinkDoFrameAsync(); }
void _BinkDoFrameAsyncMulti() { bink2w64.BinkDoFrameAsyncMulti(); }
void _BinkDoFrameAsyncWait() { bink2w64.BinkDoFrameAsyncWait(); }
void _BinkDoFramePlane() { bink2w64.BinkDoFramePlane(); }
void _BinkFindXAudio2WinDevice() { bink2w64.BinkFindXAudio2WinDevice(); }
void _BinkFreeGlobals() { bink2w64.BinkFreeGlobals(); }
void _BinkGetError() { bink2w64.BinkGetError(); }
void _BinkGetFrameBuffersInfo() { bink2w64.BinkGetFrameBuffersInfo(); }
void _BinkGetGPUDataBuffersInfo() { bink2w64.BinkGetGPUDataBuffersInfo(); }
void _BinkGetKeyFrame() { bink2w64.BinkGetKeyFrame(); }
void _BinkGetPalette() { bink2w64.BinkGetPalette(); }
void _BinkGetPlatformInfo() { bink2w64.BinkGetPlatformInfo(); }
void _BinkGetRealtime() { bink2w64.BinkGetRealtime(); }
void _BinkGetRects() { bink2w64.BinkGetRects(); }
void _BinkGetSubtitleByIndex() { bink2w64.BinkGetSubtitleByIndex(); }
void _BinkGetSummary() { bink2w64.BinkGetSummary(); }
void _BinkGetTrackData() { bink2w64.BinkGetTrackData(); }
void _BinkGetTrackID() { bink2w64.BinkGetTrackID(); }
void _BinkGetTrackMaxSize() { bink2w64.BinkGetTrackMaxSize(); }
void _BinkGetTrackType() { bink2w64.BinkGetTrackType(); }
void _BinkGoto() { bink2w64.BinkGoto(); }
void _BinkIsSoftwareCursor() { bink2w64.BinkIsSoftwareCursor(); }
void _BinkLoadSubtitles() { bink2w64.BinkLoadSubtitles(); }
void _BinkLogoAddress() { bink2w64.BinkLogoAddress(); }
void _BinkNextFrame() { bink2w64.BinkNextFrame(); }
void _BinkOpen() { bink2w64.BinkOpen(); }
void _BinkOpenDirectSound() { bink2w64.BinkOpenDirectSound(); }
void _BinkOpenMiles() { bink2w64.BinkOpenMiles(); }
void _BinkOpenTrack() { bink2w64.BinkOpenTrack(); }
void _BinkOpenWaveOut() { bink2w64.BinkOpenWaveOut(); }
void _BinkOpenWithOptions() { bink2w64.BinkOpenWithOptions(); }
void _BinkOpenXAudio2() { bink2w64.BinkOpenXAudio2(); }
void _BinkOpenXAudio27() { bink2w64.BinkOpenXAudio27(); }
void _BinkOpenXAudio28() { bink2w64.BinkOpenXAudio28(); }
void _BinkOpenXAudio29() { bink2w64.BinkOpenXAudio29(); }
void _BinkPause() { bink2w64.BinkPause(); }
void _BinkRegisterFrameBuffers() { bink2w64.BinkRegisterFrameBuffers(); }
void _BinkRegisterGPUDataBuffers() { bink2w64.BinkRegisterGPUDataBuffers(); }
void _BinkRequestStopAsyncThread() { bink2w64.BinkRequestStopAsyncThread(); }
void _BinkRequestStopAsyncThreadsMulti() { bink2w64.BinkRequestStopAsyncThreadsMulti(); }
void _BinkRestoreCursor() { bink2w64.BinkRestoreCursor(); }
void _BinkService() { bink2w64.BinkService(); }
void _BinkServiceSound() { bink2w64.BinkServiceSound(); }
void _BinkSetError() { bink2w64.BinkSetError(); }
void _BinkSetFileOffset() { bink2w64.BinkSetFileOffset(); }
void _BinkSetFrameRate() { bink2w64.BinkSetFrameRate(); }
void _BinkSetIO() { bink2w64.BinkSetIO(); }
void _BinkSetIOSize() { bink2w64.BinkSetIOSize(); }
void _BinkSetMemory() { bink2w64.BinkSetMemory(); }
void _BinkSetOSFileCallbacks() { bink2w64.BinkSetOSFileCallbacks(); }
void _BinkSetPan() { bink2w64.BinkSetPan(); }
void _BinkSetSimulate() { bink2w64.BinkSetSimulate(); }
void _BinkSetSoundOnOff() { bink2w64.BinkSetSoundOnOff(); }
void _BinkSetSoundSystem() { bink2w64.BinkSetSoundSystem(); }
void _BinkSetSoundSystem2() { bink2w64.BinkSetSoundSystem2(); }
void _BinkSetSoundTrack() { bink2w64.BinkSetSoundTrack(); }
void _BinkSetSpeakerVolumes() { bink2w64.BinkSetSpeakerVolumes(); }
void _BinkSetVideoOnOff() { bink2w64.BinkSetVideoOnOff(); }
void _BinkSetVolume() { bink2w64.BinkSetVolume(); }
void _BinkSetWillLoop() { bink2w64.BinkSetWillLoop(); }
void _BinkShouldSkip() { bink2w64.BinkShouldSkip(); }
void _BinkStartAsyncThread() { bink2w64.BinkStartAsyncThread(); }
void _BinkUtilCPUs() { bink2w64.BinkUtilCPUs(); }
void _BinkUtilFree() { bink2w64.BinkUtilFree(); }
void _BinkUtilMalloc() { bink2w64.BinkUtilMalloc(); }
void _BinkUtilMutexCreate() { bink2w64.BinkUtilMutexCreate(); }
void _BinkUtilMutexDestroy() { bink2w64.BinkUtilMutexDestroy(); }
void _BinkUtilMutexLock() { bink2w64.BinkUtilMutexLock(); }
void _BinkUtilMutexLockTimeOut() { bink2w64.BinkUtilMutexLockTimeOut(); }
void _BinkUtilMutexUnlock() { bink2w64.BinkUtilMutexUnlock(); }
void _BinkUtilSoundGlobalLock() { bink2w64.BinkUtilSoundGlobalLock(); }
void _BinkUtilSoundGlobalUnlock() { bink2w64.BinkUtilSoundGlobalUnlock(); }
void _BinkWait() { bink2w64.BinkWait(); }
void _BinkWaitStopAsyncThread() { bink2w64.BinkWaitStopAsyncThread(); }
void _BinkWaitStopAsyncThreadsMulti() { bink2w64.BinkWaitStopAsyncThreadsMulti(); }
void _RADTimerRead() { bink2w64.RADTimerRead(); }
#pragma runtime_checks( "", restore )
#endif
namespace ual::proxy::bink
{
    namespace
    {
        // There is no system Bink DLL to fall back to, so without the original every export jumps to null.
        // That is always reported in <loader>.log, since the game crashes at its first video otherwise.
        HMODULE LoadHooked(const std::wstring& hooked)
        {
            const auto& self = Self();
            if (!FileExists(hooked))
            {
                log::EnsureOpen(self.dir + self.stem + L".log");
                UAL_LOG("%ls not found: rename the game's %ls to it, the Bink functions have nothing to forward to and will crash", hooked.c_str(),
                        self.name.c_str());
                return nullptr;
            }
            HMODULE m = LoadLibraryW(hooked.c_str());
            if (!m)
            {
                DWORD error = GetLastError();
                log::EnsureOpen(self.dir + self.stem + L".log");
                UAL_LOG("%ls could not be loaded (error %lu), the Bink functions have nothing to forward to and will crash", hooked.c_str(), error);
            }
            return m;
        }
    }

    bool Load(const std::wstring& selfName)
    {
        const auto& self = Self();
#ifndef _WIN64
        if (IEquals(selfName, L"binkw32.dll") || IEquals(selfName, L"bink2w32.dll"))
        {
            auto hooked = self.dir + self.stem + L"Hooked.dll";
            if (HMODULE m = LoadHooked(hooked)) bink2w32.LoadOriginalLibrary(m);
            return true;
        }
#else
        if (IEquals(selfName, L"bink2w64.dll") || IEquals(selfName, L"binkw64.dll"))
        {
            auto hooked = self.dir + self.stem + L"Hooked.dll";
            if (HMODULE m = LoadHooked(hooked)) bink2w64.LoadOriginalLibrary(m);
            return true;
        }
#endif
        return false;
    }
}
