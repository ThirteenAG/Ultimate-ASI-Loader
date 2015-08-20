#include "includes\structs.h"

void ExcludedEntriesListInit(ExcludedEntriesList* list)
{
	list->first = NULL;
	list->last = NULL;
}

void ExcludedEntriesListPush(ExcludedEntriesList* list, const char* entryName)
{
	ExcludedEntry* 	newEntry = (ExcludedEntry*)malloc(sizeof(ExcludedEntry));
	int 			length = strlen(entryName) + 1;
	if ( !list->first )
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
	while ( it )
	{
		if ( !_stricmp(it->entry, entryName) )
		{
			// It has an entry, we can pop it now
			if ( it->next )
				it->next->prev = it->prev;
			if ( it->prev )
				it->prev->next = it->next;

			if ( list->first == it )
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
	while ( it )
	{
		ExcludedEntry* nextEntry = it->next;
		free(it->entry);
		free(it);
		it = nextEntry;
	}
}

void FindFiles(WIN32_FIND_DATA* fd, ExcludedEntriesList* list)
{
	HANDLE asiFile = FindFirstFile ("*.asi", fd);
	if (asiFile != INVALID_HANDLE_VALUE)
	{

		do {
			if (!(fd->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {

				unsigned int pos = 5;
				while (fd->cFileName[pos])
					++pos;
				if (fd->cFileName[pos-4] == '.' &&
					(fd->cFileName[pos-3] == 'a' || fd->cFileName[pos-3] == 'A') &&
					(fd->cFileName[pos-2] == 's' || fd->cFileName[pos-2] == 'S') &&
					(fd->cFileName[pos-1] == 'i' || fd->cFileName[pos-1] == 'I'))
				{
					if ( !list || !ExcludedEntriesListHasEntry(list, fd->cFileName) )
						LoadLibrary (fd->cFileName);
				}
			}

		} while (FindNextFile (asiFile, fd));
		FindClose (asiFile);
	}
}

void LoadOriginalLibrary()
{
	if (_stricmp(DllName + 1, "vorbisFile.dll") == NULL)
	{
		HMEMORYMODULE vorbisHooked = MemoryLoadLibrary(vorbisHookedhex);
		__ov_open_callbacks = MemoryGetProcAddress(vorbisHooked, "ov_open_callbacks");
		__ov_clear = MemoryGetProcAddress(vorbisHooked, "ov_clear");
		__ov_time_total = MemoryGetProcAddress(vorbisHooked, "ov_time_total");
		__ov_time_tell = MemoryGetProcAddress(vorbisHooked, "ov_time_tell");
		__ov_read = MemoryGetProcAddress(vorbisHooked, "ov_read");
		__ov_info = MemoryGetProcAddress(vorbisHooked, "ov_info");
		__ov_time_seek = MemoryGetProcAddress(vorbisHooked, "ov_time_seek");
		__ov_time_seek_page = MemoryGetProcAddress(vorbisHooked, "ov_time_seek_page");
	}
	else
	{
		if (_stricmp(DllName + 1, "dsound.dll") == NULL) {
			dsound.dll = LoadLibrary(szSystemPath);
			dsound.DirectSoundCaptureCreate = GetProcAddress(dsound.dll, "DirectSoundCaptureCreate");
			dsound.DirectSoundCaptureCreate8 = GetProcAddress(dsound.dll, "DirectSoundCaptureCreate8");
			dsound.DirectSoundCaptureEnumerateA = GetProcAddress(dsound.dll, "DirectSoundCaptureEnumerateA");
			dsound.DirectSoundCaptureEnumerateW = GetProcAddress(dsound.dll, "DirectSoundCaptureEnumerateW");
			dsound.DirectSoundCreate = GetProcAddress(dsound.dll, "DirectSoundCreate");
			dsound.DirectSoundCreate8 = GetProcAddress(dsound.dll, "DirectSoundCreate8");
			dsound.DirectSoundEnumerateA = GetProcAddress(dsound.dll, "DirectSoundEnumerateA");
			dsound.DirectSoundEnumerateW = GetProcAddress(dsound.dll, "DirectSoundEnumerateW");
			dsound.DirectSoundFullDuplexCreate = GetProcAddress(dsound.dll, "DirectSoundFullDuplexCreate");
			dsound.DllCanUnloadNow_dsound = GetProcAddress(dsound.dll, "DllCanUnloadNow_dsound");
			dsound.DllGetClassObject_dsound = GetProcAddress(dsound.dll, "DllGetClassObject_dsound");
			dsound.GetDeviceID = GetProcAddress(dsound.dll, "GetDeviceID");
		}
		else
		{
			if (_stricmp(DllName + 1, "dinput8.dll") == NULL) {
				dinput8.dll = LoadLibrary(szSystemPath);
				dinput8.DirectInput8Create = GetProcAddress(dinput8.dll, "DirectInput8Create");
				dinput8.DllCanUnloadNow = GetProcAddress(dinput8.dll, "DllCanUnloadNow");
				dinput8.DllGetClassObject = GetProcAddress(dinput8.dll, "DllGetClassObject");
				dinput8.DllRegisterServer = GetProcAddress(dinput8.dll, "DllRegisterServer");
				dinput8.DllUnregisterServer = GetProcAddress(dinput8.dll, "DllUnregisterServer");
			}
			else
			{
				if (_stricmp(DllName + 1, "ddraw.dll") == NULL) {
					ddraw.dll = LoadLibrary(szSystemPath);
					ddraw.AcquireDDThreadLock = GetProcAddress(ddraw.dll, "AcquireDDThreadLock");
					ddraw.CheckFullscreen = GetProcAddress(ddraw.dll, "CheckFullscreen");
					ddraw.CompleteCreateSysmemSurface = GetProcAddress(ddraw.dll, "CompleteCreateSysmemSurface");
					ddraw.D3DParseUnknownCommand = GetProcAddress(ddraw.dll, "D3DParseUnknownCommand");
					ddraw.DDGetAttachedSurfaceLcl = GetProcAddress(ddraw.dll, "DDGetAttachedSurfaceLcl");
					ddraw.DDInternalLock = GetProcAddress(ddraw.dll, "DDInternalLock");
					ddraw.DDInternalUnlock = GetProcAddress(ddraw.dll, "DDInternalUnlock");
					ddraw.DSoundHelp = GetProcAddress(ddraw.dll, "DSoundHelp");
					ddraw.DirectDrawCreate = GetProcAddress(ddraw.dll, "DirectDrawCreate");
					ddraw.DirectDrawCreateClipper = GetProcAddress(ddraw.dll, "DirectDrawCreateClipper");
					ddraw.DirectDrawCreateEx = GetProcAddress(ddraw.dll, "DirectDrawCreateEx");
					ddraw.DirectDrawEnumerateA = GetProcAddress(ddraw.dll, "DirectDrawEnumerateA");
					ddraw.DirectDrawEnumerateExA = GetProcAddress(ddraw.dll, "DirectDrawEnumerateExA");
					ddraw.DirectDrawEnumerateExW = GetProcAddress(ddraw.dll, "DirectDrawEnumerateExW");
					ddraw.DirectDrawEnumerateW = GetProcAddress(ddraw.dll, "DirectDrawEnumerateW");
					ddraw.DllCanUnloadNow = GetProcAddress(ddraw.dll, "DllCanUnloadNow");
					ddraw.DllGetClassObject = GetProcAddress(ddraw.dll, "DllGetClassObject");
					ddraw.GetDDSurfaceLocal = GetProcAddress(ddraw.dll, "GetDDSurfaceLocal");
					ddraw.GetOLEThunkData = GetProcAddress(ddraw.dll, "GetOLEThunkData");
					ddraw.GetSurfaceFromDC = GetProcAddress(ddraw.dll, "GetSurfaceFromDC");
					ddraw.RegisterSpecialCase = GetProcAddress(ddraw.dll, "RegisterSpecialCase");
					ddraw.ReleaseDDThreadLock = GetProcAddress(ddraw.dll, "ReleaseDDThreadLock");
				}
				else
				{
					if (_stricmp(DllName + 1, "d3d8.dll") == NULL) {
						d3d8.dll = LoadLibrary(szSystemPath);
						d3d8.DebugSetMute_d3d8 = GetProcAddress(d3d8.dll, "DebugSetMute_d3d8");
						d3d8.Direct3DCreate8 = GetProcAddress(d3d8.dll, "Direct3DCreate8");
						d3d8.ValidatePixelShader = GetProcAddress(d3d8.dll, "ValidatePixelShader");
						d3d8.ValidateVertexShader = GetProcAddress(d3d8.dll, "ValidateVertexShader");
					}
					else {
						if (_stricmp(DllName + 1, "d3d9.dll") == NULL) {
							d3d9.dll = LoadLibrary(szSystemPath);
							d3d9.D3DPERF_BeginEvent = GetProcAddress(d3d9.dll, "D3DPERF_BeginEvent");
							d3d9.D3DPERF_EndEvent = GetProcAddress(d3d9.dll, "D3DPERF_EndEvent");
							d3d9.D3DPERF_GetStatus = GetProcAddress(d3d9.dll, "D3DPERF_GetStatus");
							d3d9.D3DPERF_QueryRepeatFrame = GetProcAddress(d3d9.dll, "D3DPERF_QueryRepeatFrame");
							d3d9.D3DPERF_SetMarker = GetProcAddress(d3d9.dll, "D3DPERF_SetMarker");
							d3d9.D3DPERF_SetOptions = GetProcAddress(d3d9.dll, "D3DPERF_SetOptions");
							d3d9.D3DPERF_SetRegion = GetProcAddress(d3d9.dll, "D3DPERF_SetRegion");
							d3d9.DebugSetLevel = GetProcAddress(d3d9.dll, "DebugSetLevel");
							d3d9.DebugSetMute = GetProcAddress(d3d9.dll, "DebugSetMute");
							//d3d9.Direct3D9EnableMaximizedWindowedModeShim = GetProcAddress(d3d9.dll, "Direct3D9EnableMaximizedWindowedModeShim");
							d3d9.Direct3DCreate9 = GetProcAddress(d3d9.dll, "Direct3DCreate9");
							d3d9.Direct3DCreate9Ex = GetProcAddress(d3d9.dll, "Direct3DCreate9Ex");
							d3d9.Direct3DShaderValidatorCreate9 = GetProcAddress(d3d9.dll, "Direct3DShaderValidatorCreate9");
							d3d9.PSGPError = GetProcAddress(d3d9.dll, "PSGPError");
							d3d9.PSGPSampleTexture = GetProcAddress(d3d9.dll, "PSGPSampleTexture");
						}
						else
						{
							if (_stricmp(DllName + 1, "d3d11.dll") == NULL) {
								d3d11.dll = LoadLibrary(szSystemPath);
								d3d11.D3D11CoreCreateDevice = GetProcAddress(d3d11.dll, "D3D11CoreCreateDevice");
								d3d11.D3D11CoreCreateLayeredDevice = GetProcAddress(d3d11.dll, "D3D11CoreCreateLayeredDevice");
								d3d11.D3D11CoreGetLayeredDeviceSize = GetProcAddress(d3d11.dll, "D3D11CoreGetLayeredDeviceSize");
								d3d11.D3D11CoreRegisterLayers = GetProcAddress(d3d11.dll, "D3D11CoreRegisterLayers");
								d3d11.D3D11CreateDevice = GetProcAddress(d3d11.dll, "D3D11CreateDevice");
								d3d11.D3D11CreateDeviceAndSwapChain = GetProcAddress(d3d11.dll, "D3D11CreateDeviceAndSwapChain");
								d3d11.D3DKMTCloseAdapter = GetProcAddress(d3d11.dll, "D3DKMTCloseAdapter");
								d3d11.D3DKMTCreateAllocation = GetProcAddress(d3d11.dll, "D3DKMTCreateAllocation");
								d3d11.D3DKMTCreateContext = GetProcAddress(d3d11.dll, "D3DKMTCreateContext");
								d3d11.D3DKMTCreateDevice = GetProcAddress(d3d11.dll, "D3DKMTCreateDevice");
								d3d11.D3DKMTCreateSynchronizationObject = GetProcAddress(d3d11.dll, "D3DKMTCreateSynchronizationObject");
								d3d11.D3DKMTDestroyAllocation = GetProcAddress(d3d11.dll, "D3DKMTDestroyAllocation");
								d3d11.D3DKMTDestroyContext = GetProcAddress(d3d11.dll, "D3DKMTDestroyContext");
								d3d11.D3DKMTDestroyDevice = GetProcAddress(d3d11.dll, "D3DKMTDestroyDevice");
								d3d11.D3DKMTDestroySynchronizationObject = GetProcAddress(d3d11.dll, "D3DKMTDestroySynchronizationObject");
								d3d11.D3DKMTEscape = GetProcAddress(d3d11.dll, "D3DKMTEscape");
								d3d11.D3DKMTGetContextSchedulingPriority = GetProcAddress(d3d11.dll, "D3DKMTGetContextSchedulingPriority");
								d3d11.D3DKMTGetDeviceState = GetProcAddress(d3d11.dll, "D3DKMTGetDeviceState");
								d3d11.D3DKMTGetDisplayModeList = GetProcAddress(d3d11.dll, "D3DKMTGetDisplayModeList");
								d3d11.D3DKMTGetMultisampleMethodList = GetProcAddress(d3d11.dll, "D3DKMTGetMultisampleMethodList");
								d3d11.D3DKMTGetRuntimeData = GetProcAddress(d3d11.dll, "D3DKMTGetRuntimeData");
								d3d11.D3DKMTGetSharedPrimaryHandle = GetProcAddress(d3d11.dll, "D3DKMTGetSharedPrimaryHandle");
								d3d11.D3DKMTLock = GetProcAddress(d3d11.dll, "D3DKMTLock");
								d3d11.D3DKMTOpenAdapterFromHdc = GetProcAddress(d3d11.dll, "D3DKMTOpenAdapterFromHdc");
								d3d11.D3DKMTOpenResource = GetProcAddress(d3d11.dll, "D3DKMTOpenResource");
								d3d11.D3DKMTPresent = GetProcAddress(d3d11.dll, "D3DKMTPresent");
								d3d11.D3DKMTQueryAdapterInfo = GetProcAddress(d3d11.dll, "D3DKMTQueryAdapterInfo");
								d3d11.D3DKMTQueryAllocationResidency = GetProcAddress(d3d11.dll, "D3DKMTQueryAllocationResidency");
								d3d11.D3DKMTQueryResourceInfo = GetProcAddress(d3d11.dll, "D3DKMTQueryResourceInfo");
								d3d11.D3DKMTRender = GetProcAddress(d3d11.dll, "D3DKMTRender");
								d3d11.D3DKMTSetAllocationPriority = GetProcAddress(d3d11.dll, "D3DKMTSetAllocationPriority");
								d3d11.D3DKMTSetContextSchedulingPriority = GetProcAddress(d3d11.dll, "D3DKMTSetContextSchedulingPriority");
								d3d11.D3DKMTSetDisplayMode = GetProcAddress(d3d11.dll, "D3DKMTSetDisplayMode");
								d3d11.D3DKMTSetDisplayPrivateDriverFormat = GetProcAddress(d3d11.dll, "D3DKMTSetDisplayPrivateDriverFormat");
								d3d11.D3DKMTSetGammaRamp = GetProcAddress(d3d11.dll, "D3DKMTSetGammaRamp");
								d3d11.D3DKMTSetVidPnSourceOwner = GetProcAddress(d3d11.dll, "D3DKMTSetVidPnSourceOwner");
								d3d11.D3DKMTSignalSynchronizationObject = GetProcAddress(d3d11.dll, "D3DKMTSignalSynchronizationObject");
								d3d11.D3DKMTUnlock = GetProcAddress(d3d11.dll, "D3DKMTUnlock");
								d3d11.D3DKMTWaitForSynchronizationObject = GetProcAddress(d3d11.dll, "D3DKMTWaitForSynchronizationObject");
								d3d11.D3DKMTWaitForVerticalBlankEvent = GetProcAddress(d3d11.dll, "D3DKMTWaitForVerticalBlankEvent");
								d3d11.D3DPerformance_BeginEvent = GetProcAddress(d3d11.dll, "D3DPerformance_BeginEvent");
								d3d11.D3DPerformance_EndEvent = GetProcAddress(d3d11.dll, "D3DPerformance_EndEvent");
								d3d11.D3DPerformance_GetStatus = GetProcAddress(d3d11.dll, "D3DPerformance_GetStatus");
								d3d11.D3DPerformance_SetMarker = GetProcAddress(d3d11.dll, "D3DPerformance_SetMarker");
								d3d11.EnableFeatureLevelUpgrade = GetProcAddress(d3d11.dll, "EnableFeatureLevelUpgrade");
								d3d11.OpenAdapter10 = GetProcAddress(d3d11.dll, "OpenAdapter10");
								d3d11.OpenAdapter10_2 = GetProcAddress(d3d11.dll, "OpenAdapter10_2");
							}
							else
							{
								if (_stricmp(DllName + 1, "winmmbase.dll") == NULL) {
									winmmbase.dll = LoadLibrary(szSystemPath);
									winmmbase.CloseDriver = GetProcAddress(winmmbase.dll, "CloseDriver");
									winmmbase.DefDriverProc = GetProcAddress(winmmbase.dll, "DefDriverProc");
									winmmbase.DriverCallback = GetProcAddress(winmmbase.dll, "DriverCallback");
									winmmbase.DrvGetModuleHandle = GetProcAddress(winmmbase.dll, "DrvGetModuleHandle");
									winmmbase.GetDriverModuleHandle = GetProcAddress(winmmbase.dll, "GetDriverModuleHandle");
									winmmbase.OpenDriver = GetProcAddress(winmmbase.dll, "OpenDriver");
									winmmbase.SendDriverMessage = GetProcAddress(winmmbase.dll, "SendDriverMessage");
									winmmbase.auxGetDevCapsA = GetProcAddress(winmmbase.dll, "auxGetDevCapsA");
									winmmbase.auxGetDevCapsW = GetProcAddress(winmmbase.dll, "auxGetDevCapsW");
									winmmbase.auxGetNumDevs = GetProcAddress(winmmbase.dll, "auxGetNumDevs");
									winmmbase.auxGetVolume = GetProcAddress(winmmbase.dll, "auxGetVolume");
									winmmbase.auxOutMessage = GetProcAddress(winmmbase.dll, "auxOutMessage");
									winmmbase.auxSetVolume = GetProcAddress(winmmbase.dll, "auxSetVolume");
									winmmbase.joyConfigChanged = GetProcAddress(winmmbase.dll, "joyConfigChanged");
									winmmbase.joyGetDevCapsA = GetProcAddress(winmmbase.dll, "joyGetDevCapsA");
									winmmbase.joyGetDevCapsW = GetProcAddress(winmmbase.dll, "joyGetDevCapsW");
									winmmbase.joyGetNumDevs = GetProcAddress(winmmbase.dll, "joyGetNumDevs");
									winmmbase.joyGetPos = GetProcAddress(winmmbase.dll, "joyGetPos");
									winmmbase.joyGetPosEx = GetProcAddress(winmmbase.dll, "joyGetPosEx");
									winmmbase.joyGetThreshold = GetProcAddress(winmmbase.dll, "joyGetThreshold");
									winmmbase.joyReleaseCapture = GetProcAddress(winmmbase.dll, "joyReleaseCapture");
									winmmbase.joySetCapture = GetProcAddress(winmmbase.dll, "joySetCapture");
									winmmbase.joySetThreshold = GetProcAddress(winmmbase.dll, "joySetThreshold");
									winmmbase.midiConnect = GetProcAddress(winmmbase.dll, "midiConnect");
									winmmbase.midiDisconnect = GetProcAddress(winmmbase.dll, "midiDisconnect");
									winmmbase.midiInAddBuffer = GetProcAddress(winmmbase.dll, "midiInAddBuffer");
									winmmbase.midiInClose = GetProcAddress(winmmbase.dll, "midiInClose");
									winmmbase.midiInGetDevCapsA = GetProcAddress(winmmbase.dll, "midiInGetDevCapsA");
									winmmbase.midiInGetDevCapsW = GetProcAddress(winmmbase.dll, "midiInGetDevCapsW");
									winmmbase.midiInGetErrorTextA = GetProcAddress(winmmbase.dll, "midiInGetErrorTextA");
									winmmbase.midiInGetErrorTextW = GetProcAddress(winmmbase.dll, "midiInGetErrorTextW");
									winmmbase.midiInGetID = GetProcAddress(winmmbase.dll, "midiInGetID");
									winmmbase.midiInGetNumDevs = GetProcAddress(winmmbase.dll, "midiInGetNumDevs");
									winmmbase.midiInMessage = GetProcAddress(winmmbase.dll, "midiInMessage");
									winmmbase.midiInOpen = GetProcAddress(winmmbase.dll, "midiInOpen");
									winmmbase.midiInPrepareHeader = GetProcAddress(winmmbase.dll, "midiInPrepareHeader");
									winmmbase.midiInReset = GetProcAddress(winmmbase.dll, "midiInReset");
									winmmbase.midiInStart = GetProcAddress(winmmbase.dll, "midiInStart");
									winmmbase.midiInStop = GetProcAddress(winmmbase.dll, "midiInStop");
									winmmbase.midiInUnprepareHeader = GetProcAddress(winmmbase.dll, "midiInUnprepareHeader");
									winmmbase.midiOutCacheDrumPatches = GetProcAddress(winmmbase.dll, "midiOutCacheDrumPatches");
									winmmbase.midiOutCachePatches = GetProcAddress(winmmbase.dll, "midiOutCachePatches");
									winmmbase.midiOutClose = GetProcAddress(winmmbase.dll, "midiOutClose");
									winmmbase.midiOutGetDevCapsA = GetProcAddress(winmmbase.dll, "midiOutGetDevCapsA");
									winmmbase.midiOutGetDevCapsW = GetProcAddress(winmmbase.dll, "midiOutGetDevCapsW");
									winmmbase.midiOutGetErrorTextA = GetProcAddress(winmmbase.dll, "midiOutGetErrorTextA");
									winmmbase.midiOutGetErrorTextW = GetProcAddress(winmmbase.dll, "midiOutGetErrorTextW");
									winmmbase.midiOutGetID = GetProcAddress(winmmbase.dll, "midiOutGetID");
									winmmbase.midiOutGetNumDevs = GetProcAddress(winmmbase.dll, "midiOutGetNumDevs");
									winmmbase.midiOutGetVolume = GetProcAddress(winmmbase.dll, "midiOutGetVolume");
									winmmbase.midiOutLongMsg = GetProcAddress(winmmbase.dll, "midiOutLongMsg");
									winmmbase.midiOutMessage = GetProcAddress(winmmbase.dll, "midiOutMessage");
									winmmbase.midiOutOpen = GetProcAddress(winmmbase.dll, "midiOutOpen");
									winmmbase.midiOutPrepareHeader = GetProcAddress(winmmbase.dll, "midiOutPrepareHeader");
									winmmbase.midiOutReset = GetProcAddress(winmmbase.dll, "midiOutReset");
									winmmbase.midiOutSetVolume = GetProcAddress(winmmbase.dll, "midiOutSetVolume");
									winmmbase.midiOutShortMsg = GetProcAddress(winmmbase.dll, "midiOutShortMsg");
									winmmbase.midiOutUnprepareHeader = GetProcAddress(winmmbase.dll, "midiOutUnprepareHeader");
									winmmbase.midiStreamClose = GetProcAddress(winmmbase.dll, "midiStreamClose");
									winmmbase.midiStreamOpen = GetProcAddress(winmmbase.dll, "midiStreamOpen");
									winmmbase.midiStreamOut = GetProcAddress(winmmbase.dll, "midiStreamOut");
									winmmbase.midiStreamPause = GetProcAddress(winmmbase.dll, "midiStreamPause");
									winmmbase.midiStreamPosition = GetProcAddress(winmmbase.dll, "midiStreamPosition");
									winmmbase.midiStreamProperty = GetProcAddress(winmmbase.dll, "midiStreamProperty");
									winmmbase.midiStreamRestart = GetProcAddress(winmmbase.dll, "midiStreamRestart");
									winmmbase.midiStreamStop = GetProcAddress(winmmbase.dll, "midiStreamStop");
									winmmbase.mixerClose = GetProcAddress(winmmbase.dll, "mixerClose");
									winmmbase.mixerGetControlDetailsA = GetProcAddress(winmmbase.dll, "mixerGetControlDetailsA");
									winmmbase.mixerGetControlDetailsW = GetProcAddress(winmmbase.dll, "mixerGetControlDetailsW");
									winmmbase.mixerGetDevCapsA = GetProcAddress(winmmbase.dll, "mixerGetDevCapsA");
									winmmbase.mixerGetDevCapsW = GetProcAddress(winmmbase.dll, "mixerGetDevCapsW");
									winmmbase.mixerGetID = GetProcAddress(winmmbase.dll, "mixerGetID");
									winmmbase.mixerGetLineControlsA = GetProcAddress(winmmbase.dll, "mixerGetLineControlsA");
									winmmbase.mixerGetLineControlsW = GetProcAddress(winmmbase.dll, "mixerGetLineControlsW");
									winmmbase.mixerGetLineInfoA = GetProcAddress(winmmbase.dll, "mixerGetLineInfoA");
									winmmbase.mixerGetLineInfoW = GetProcAddress(winmmbase.dll, "mixerGetLineInfoW");
									winmmbase.mixerGetNumDevs = GetProcAddress(winmmbase.dll, "mixerGetNumDevs");
									winmmbase.mixerMessage = GetProcAddress(winmmbase.dll, "mixerMessage");
									winmmbase.mixerOpen = GetProcAddress(winmmbase.dll, "mixerOpen");
									winmmbase.mixerSetControlDetails = GetProcAddress(winmmbase.dll, "mixerSetControlDetails");
									winmmbase.mmDrvInstall = GetProcAddress(winmmbase.dll, "mmDrvInstall");
									winmmbase.mmGetCurrentTask = GetProcAddress(winmmbase.dll, "mmGetCurrentTask");
									winmmbase.mmTaskBlock = GetProcAddress(winmmbase.dll, "mmTaskBlock");
									winmmbase.mmTaskCreate = GetProcAddress(winmmbase.dll, "mmTaskCreate");
									winmmbase.mmTaskSignal = GetProcAddress(winmmbase.dll, "mmTaskSignal");
									winmmbase.mmTaskYield = GetProcAddress(winmmbase.dll, "mmTaskYield");
									winmmbase.mmioAdvance = GetProcAddress(winmmbase.dll, "mmioAdvance");
									winmmbase.mmioAscend = GetProcAddress(winmmbase.dll, "mmioAscend");
									winmmbase.mmioClose = GetProcAddress(winmmbase.dll, "mmioClose");
									winmmbase.mmioCreateChunk = GetProcAddress(winmmbase.dll, "mmioCreateChunk");
									winmmbase.mmioDescend = GetProcAddress(winmmbase.dll, "mmioDescend");
									winmmbase.mmioFlush = GetProcAddress(winmmbase.dll, "mmioFlush");
									winmmbase.mmioGetInfo = GetProcAddress(winmmbase.dll, "mmioGetInfo");
									winmmbase.mmioInstallIOProcA = GetProcAddress(winmmbase.dll, "mmioInstallIOProcA");
									winmmbase.mmioInstallIOProcW = GetProcAddress(winmmbase.dll, "mmioInstallIOProcW");
									winmmbase.mmioOpenA = GetProcAddress(winmmbase.dll, "mmioOpenA");
									winmmbase.mmioOpenW = GetProcAddress(winmmbase.dll, "mmioOpenW");
									winmmbase.mmioRead = GetProcAddress(winmmbase.dll, "mmioRead");
									winmmbase.mmioRenameA = GetProcAddress(winmmbase.dll, "mmioRenameA");
									winmmbase.mmioRenameW = GetProcAddress(winmmbase.dll, "mmioRenameW");
									winmmbase.mmioSeek = GetProcAddress(winmmbase.dll, "mmioSeek");
									winmmbase.mmioSendMessage = GetProcAddress(winmmbase.dll, "mmioSendMessage");
									winmmbase.mmioSetBuffer = GetProcAddress(winmmbase.dll, "mmioSetBuffer");
									winmmbase.mmioSetInfo = GetProcAddress(winmmbase.dll, "mmioSetInfo");
									winmmbase.mmioStringToFOURCCA = GetProcAddress(winmmbase.dll, "mmioStringToFOURCCA");
									winmmbase.mmioStringToFOURCCW = GetProcAddress(winmmbase.dll, "mmioStringToFOURCCW");
									winmmbase.mmioWrite = GetProcAddress(winmmbase.dll, "mmioWrite");
									winmmbase.sndOpenSound = GetProcAddress(winmmbase.dll, "sndOpenSound");
									winmmbase.waveInAddBuffer = GetProcAddress(winmmbase.dll, "waveInAddBuffer");
									winmmbase.waveInClose = GetProcAddress(winmmbase.dll, "waveInClose");
									winmmbase.waveInGetDevCapsA = GetProcAddress(winmmbase.dll, "waveInGetDevCapsA");
									winmmbase.waveInGetDevCapsW = GetProcAddress(winmmbase.dll, "waveInGetDevCapsW");
									winmmbase.waveInGetErrorTextA = GetProcAddress(winmmbase.dll, "waveInGetErrorTextA");
									winmmbase.waveInGetErrorTextW = GetProcAddress(winmmbase.dll, "waveInGetErrorTextW");
									winmmbase.waveInGetID = GetProcAddress(winmmbase.dll, "waveInGetID");
									winmmbase.waveInGetNumDevs = GetProcAddress(winmmbase.dll, "waveInGetNumDevs");
									winmmbase.waveInGetPosition = GetProcAddress(winmmbase.dll, "waveInGetPosition");
									winmmbase.waveInMessage = GetProcAddress(winmmbase.dll, "waveInMessage");
									winmmbase.waveInOpen = GetProcAddress(winmmbase.dll, "waveInOpen");
									winmmbase.waveInPrepareHeader = GetProcAddress(winmmbase.dll, "waveInPrepareHeader");
									winmmbase.waveInReset = GetProcAddress(winmmbase.dll, "waveInReset");
									winmmbase.waveInStart = GetProcAddress(winmmbase.dll, "waveInStart");
									winmmbase.waveInStop = GetProcAddress(winmmbase.dll, "waveInStop");
									winmmbase.waveInUnprepareHeader = GetProcAddress(winmmbase.dll, "waveInUnprepareHeader");
									winmmbase.waveOutBreakLoop = GetProcAddress(winmmbase.dll, "waveOutBreakLoop");
									winmmbase.waveOutClose = GetProcAddress(winmmbase.dll, "waveOutClose");
									winmmbase.waveOutGetDevCapsA = GetProcAddress(winmmbase.dll, "waveOutGetDevCapsA");
									winmmbase.waveOutGetDevCapsW = GetProcAddress(winmmbase.dll, "waveOutGetDevCapsW");
									winmmbase.waveOutGetErrorTextA = GetProcAddress(winmmbase.dll, "waveOutGetErrorTextA");
									winmmbase.waveOutGetErrorTextW = GetProcAddress(winmmbase.dll, "waveOutGetErrorTextW");
									winmmbase.waveOutGetID = GetProcAddress(winmmbase.dll, "waveOutGetID");
									winmmbase.waveOutGetNumDevs = GetProcAddress(winmmbase.dll, "waveOutGetNumDevs");
									winmmbase.waveOutGetPitch = GetProcAddress(winmmbase.dll, "waveOutGetPitch");
									winmmbase.waveOutGetPlaybackRate = GetProcAddress(winmmbase.dll, "waveOutGetPlaybackRate");
									winmmbase.waveOutGetPosition = GetProcAddress(winmmbase.dll, "waveOutGetPosition");
									winmmbase.waveOutGetVolume = GetProcAddress(winmmbase.dll, "waveOutGetVolume");
									winmmbase.waveOutMessage = GetProcAddress(winmmbase.dll, "waveOutMessage");
									winmmbase.waveOutOpen = GetProcAddress(winmmbase.dll, "waveOutOpen");
									winmmbase.waveOutPause = GetProcAddress(winmmbase.dll, "waveOutPause");
									winmmbase.waveOutPrepareHeader = GetProcAddress(winmmbase.dll, "waveOutPrepareHeader");
									winmmbase.waveOutReset = GetProcAddress(winmmbase.dll, "waveOutReset");
									winmmbase.waveOutRestart = GetProcAddress(winmmbase.dll, "waveOutRestart");
									winmmbase.waveOutSetPitch = GetProcAddress(winmmbase.dll, "waveOutSetPitch");
									winmmbase.waveOutSetPlaybackRate = GetProcAddress(winmmbase.dll, "waveOutSetPlaybackRate");
									winmmbase.waveOutSetVolume = GetProcAddress(winmmbase.dll, "waveOutSetVolume");
									winmmbase.waveOutUnprepareHeader = GetProcAddress(winmmbase.dll, "waveOutUnprepareHeader");
									winmmbase.waveOutWrite = GetProcAddress(winmmbase.dll, "waveOutWrite");
									winmmbase.winmmbaseFreeMMEHandles = GetProcAddress(winmmbase.dll, "winmmbaseFreeMMEHandles");
									winmmbase.winmmbaseGetWOWHandle = GetProcAddress(winmmbase.dll, "winmmbaseGetWOWHandle");
									winmmbase.winmmbaseHandle32FromHandle16 = GetProcAddress(winmmbase.dll, "winmmbaseHandle32FromHandle16");
									winmmbase.winmmbaseSetWOWHandle = GetProcAddress(winmmbase.dll, "winmmbaseSetWOWHandle");
								}
								else
								{
									if (_stricmp(DllName + 1, "msacm32.dll") == NULL) {
										msacm32.dll = LoadLibrary(szSystemPath);
										msacm32.acmDriverAddA = GetProcAddress(msacm32.dll, "acmDriverAddA");
										msacm32.acmDriverAddW = GetProcAddress(msacm32.dll, "acmDriverAddW");
										msacm32.acmDriverClose = GetProcAddress(msacm32.dll, "acmDriverClose");
										msacm32.acmDriverDetailsA = GetProcAddress(msacm32.dll, "acmDriverDetailsA");
										msacm32.acmDriverDetailsW = GetProcAddress(msacm32.dll, "acmDriverDetailsW");
										msacm32.acmDriverEnum = GetProcAddress(msacm32.dll, "acmDriverEnum");
										msacm32.acmDriverID = GetProcAddress(msacm32.dll, "acmDriverID");
										msacm32.acmDriverMessage = GetProcAddress(msacm32.dll, "acmDriverMessage");
										msacm32.acmDriverOpen = GetProcAddress(msacm32.dll, "acmDriverOpen");
										msacm32.acmDriverPriority = GetProcAddress(msacm32.dll, "acmDriverPriority");
										msacm32.acmDriverRemove = GetProcAddress(msacm32.dll, "acmDriverRemove");
										msacm32.acmFilterChooseA = GetProcAddress(msacm32.dll, "acmFilterChooseA");
										msacm32.acmFilterChooseW = GetProcAddress(msacm32.dll, "acmFilterChooseW");
										msacm32.acmFilterDetailsA = GetProcAddress(msacm32.dll, "acmFilterDetailsA");
										msacm32.acmFilterDetailsW = GetProcAddress(msacm32.dll, "acmFilterDetailsW");
										msacm32.acmFilterEnumA = GetProcAddress(msacm32.dll, "acmFilterEnumA");
										msacm32.acmFilterEnumW = GetProcAddress(msacm32.dll, "acmFilterEnumW");
										msacm32.acmFilterTagDetailsA = GetProcAddress(msacm32.dll, "acmFilterTagDetailsA");
										msacm32.acmFilterTagDetailsW = GetProcAddress(msacm32.dll, "acmFilterTagDetailsW");
										msacm32.acmFilterTagEnumA = GetProcAddress(msacm32.dll, "acmFilterTagEnumA");
										msacm32.acmFilterTagEnumW = GetProcAddress(msacm32.dll, "acmFilterTagEnumW");
										msacm32.acmFormatChooseA = GetProcAddress(msacm32.dll, "acmFormatChooseA");
										msacm32.acmFormatChooseW = GetProcAddress(msacm32.dll, "acmFormatChooseW");
										msacm32.acmFormatDetailsA = GetProcAddress(msacm32.dll, "acmFormatDetailsA");
										msacm32.acmFormatDetailsW = GetProcAddress(msacm32.dll, "acmFormatDetailsW");
										msacm32.acmFormatEnumA = GetProcAddress(msacm32.dll, "acmFormatEnumA");
										msacm32.acmFormatEnumW = GetProcAddress(msacm32.dll, "acmFormatEnumW");
										msacm32.acmFormatSuggest = GetProcAddress(msacm32.dll, "acmFormatSuggest");
										msacm32.acmFormatTagDetailsA = GetProcAddress(msacm32.dll, "acmFormatTagDetailsA");
										msacm32.acmFormatTagDetailsW = GetProcAddress(msacm32.dll, "acmFormatTagDetailsW");
										msacm32.acmFormatTagEnumA = GetProcAddress(msacm32.dll, "acmFormatTagEnumA");
										msacm32.acmFormatTagEnumW = GetProcAddress(msacm32.dll, "acmFormatTagEnumW");
										msacm32.acmGetVersion = GetProcAddress(msacm32.dll, "acmGetVersion");
										msacm32.acmMetrics = GetProcAddress(msacm32.dll, "acmMetrics");
										msacm32.acmStreamClose = GetProcAddress(msacm32.dll, "acmStreamClose");
										msacm32.acmStreamConvert = GetProcAddress(msacm32.dll, "acmStreamConvert");
										msacm32.acmStreamMessage = GetProcAddress(msacm32.dll, "acmStreamMessage");
										msacm32.acmStreamOpen = GetProcAddress(msacm32.dll, "acmStreamOpen");
										msacm32.acmStreamPrepareHeader = GetProcAddress(msacm32.dll, "acmStreamPrepareHeader");
										msacm32.acmStreamReset = GetProcAddress(msacm32.dll, "acmStreamReset");
										msacm32.acmStreamSize = GetProcAddress(msacm32.dll, "acmStreamSize");
										msacm32.acmStreamUnprepareHeader = GetProcAddress(msacm32.dll, "acmStreamUnprepareHeader");
									}
									else
									{
										MessageBox(0, "This library isn't supported. Try to rename it to d3d8.dll, d3d9.dll, d3d11.dll, winmmbase.dll, msacm32.dll, dinput8.dll, dsound.dll, vorbisFile.dll or ddraw.dll.", "ASI Loader", MB_ICONERROR);
										ExitProcess(0);
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

void LoadPlugins()
{
	LoadOriginalLibrary();

	// Regular ASI Loader
	WIN32_FIND_DATA fd;
	char			moduleName[MAX_PATH];
	char			dllPath[MAX_PATH];
	char			preparedPath[MAX_PATH];	// stores scripts\*exename*\settings.ini
	char*			tempPointer;
	int 			nWantsToLoadPlugins;
	int				nThatExeWantsPlugins;
	int 			nWantsToLoadFromScriptsOnly;

	char oldDir[MAX_PATH]; // store the current directory
	GetCurrentDirectory(MAX_PATH, oldDir);

	GetModuleFileName(NULL, moduleName, MAX_PATH);
	tempPointer = strrchr(moduleName, '.');
	*tempPointer = '\0';
	tempPointer = strrchr(moduleName, '\\');
	strncpy(dllPath, moduleName, (tempPointer - moduleName + 1));
	dllPath[tempPointer - moduleName + 1] = '\0';
	SetCurrentDirectory(dllPath);

	LoadLibrary(".\\modloader\\modloader.asi");

	std::fstream wndmode_ini;
	wndmode_ini.open("wndmode.ini", std::ios_base::out | std::ios_base::in);  // will not create wndmode.ini
	if (wndmode_ini.is_open())
	{
		std::string line;
		while (!wndmode_ini.eof())
		{
			std::getline(wndmode_ini, line);
			if ((line.find("[WINDOWMODE]", 0)) == std::string::npos)
			{
				wndmode_ini.clear();
				wndmode_ini.write(wndmode_ini_hex, sizeof(wndmode_ini_hex));
				break;
			}
		}
		wndmode_ini.close();
		hwndmode_dll = MemoryLoadLibrary(wndmode_dll_hex);
	}

	GetModuleFileName(NULL, moduleName, MAX_PATH);
	tempPointer = strrchr(moduleName, '.');
	*tempPointer = '\0';

	tempPointer = strrchr(moduleName, '\\');
	strncpy(preparedPath, "scripts", 8);
	strcat(preparedPath, tempPointer);
	strcat(preparedPath, "\\settings.ini");

	// Before we load any ASI files, let's see if user wants to do it at all
	nWantsToLoadPlugins = GetPrivateProfileInt("globalsets", "loadplugins", TRUE, "scripts\\global.ini");
	nWantsToLoadFromScriptsOnly = GetPrivateProfileInt("globalsets", "loadfromscriptsonly", FALSE, "scripts\\global.ini");
	// Or perhaps this EXE wants to override global settings?
	nThatExeWantsPlugins = GetPrivateProfileInt("exclusivesets", "loadplugins", -1, preparedPath);

	if (nThatExeWantsPlugins)	// Will not process only if this EXE wishes not to load anything but its exclusive plugins
	{
		if (nWantsToLoadPlugins || nThatExeWantsPlugins == TRUE)
		{
			// Load excludes
			ExcludedEntriesList	excludes;

			ExcludedEntriesListInit(&excludes);
			if (FILE* iniFile = fopen(preparedPath, "rt"))
			{
				char	line[256];
				bool	bItsExcludesList = false;

				while (fgets(line, 256, iniFile))
				{
					char*	newline = strchr(line, '\n');

					if (newline)
						*newline = '\0';

					if (bItsExcludesList)
					{
						if (line[0] && line[0] != ';')
							ExcludedEntriesListPush(&excludes, line);
					}
					else
					{
						if (!_stricmp(line, "[excludes]"))
							bItsExcludesList = true;
					}
				}

				fclose(iniFile);
			}
			if (!nWantsToLoadFromScriptsOnly)
			{
				FindFiles(&fd, &excludes);
			}
			if (SetCurrentDirectory("scripts\\"))
			{
				FindFiles(&fd, &excludes);
				if (SetCurrentDirectory(tempPointer + 1))
				{
					FindFiles(&fd, NULL);	// Exclusive plugins are not being excluded
					SetCurrentDirectory("..\\..\\");
				}
				else
					SetCurrentDirectory("..\\");
			}

			// Free the remaining excludes
			ExcludedEntriesListFree(&excludes);
		}
	}
	else
	{
		// Load only exclusive plugins, if exists
		// We need to cut settings.ini from the path again
		tempPointer = strrchr(preparedPath, '\\');
		tempPointer[1] = '\0';
		if (SetCurrentDirectory(preparedPath))
		{
			FindFiles(&fd, NULL);
			SetCurrentDirectory("..\\..\\");
		}
	}
	SetCurrentDirectory(oldDir); // Reset the current directory

	// Unprotect the module NOW (CLEO 4.1.1.30f crash fix)
	IMAGE_NT_HEADERS* ntHeader = (IMAGE_NT_HEADERS*)((DWORD)hExecutableInstance + ((IMAGE_DOS_HEADER*)hExecutableInstance)->e_lfanew);

	SIZE_T size = ntHeader->OptionalHeader.SizeOfImage;
	DWORD oldProtect;
	VirtualProtect((VOID*)hExecutableInstance, size, PAGE_EXECUTE_READWRITE, &oldProtect);
}

static bool			bLoadedPluginsYet = false;

void WINAPI CustomGetStartupInfoA(LPSTARTUPINFOA lpStartupInfo)
{
	if ( !bLoadedPluginsYet )
	{
		// At the time this is called, the EXE is fully decrypted - we don't need any tricks from the ASI side
		LoadPlugins();
		bLoadedPluginsYet = true;
	}
	GetStartupInfoA(lpStartupInfo);
}

void WINAPI CustomGetStartupInfoW(LPSTARTUPINFOW lpStartupInfo)
{
	if ( !bLoadedPluginsYet )
	{
		// At the time this is called, the EXE is fully decrypted - we don't need any tricks from the ASI side
		LoadPlugins();
		bLoadedPluginsYet = true;
	}
	GetStartupInfoW(lpStartupInfo);
}

void PatchIAT()
{
	// Find IAT
	IMAGE_NT_HEADERS*			ntHeader = (IMAGE_NT_HEADERS*)((DWORD)hExecutableInstance + ((IMAGE_DOS_HEADER*)hExecutableInstance)->e_lfanew);
	IMAGE_IMPORT_DESCRIPTOR*	pImports = (IMAGE_IMPORT_DESCRIPTOR*)((DWORD)hExecutableInstance + ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
	DWORD						nNumImports = ntHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].Size / sizeof(IMAGE_IMPORT_DESCRIPTOR) - 1;

	// Find kernel32.dll
	for ( DWORD i = 0; i < nNumImports; i++ )
	{
		if ( !_stricmp((const char*)((DWORD)hExecutableInstance + pImports->Name), "KERNEL32.DLL") )
		{
			IMAGE_IMPORT_BY_NAME**		pFunctions = (IMAGE_IMPORT_BY_NAME**)((DWORD)hExecutableInstance + pImports->OriginalFirstThunk);

			// kernel32.dll found, find GetStartupInfoA
			for ( DWORD j = 0; pFunctions[j]; j++ )
			{
				if ( !_stricmp((const char*)((DWORD)hExecutableInstance + pFunctions[j]->Name), "GetStartupInfoA") )
				{
					// Overwrite the address with the address to a custom GetStartupInfoA
					DWORD		dwProtect[2];
					DWORD*		pAddress = &((DWORD*)((DWORD)hExecutableInstance + pImports->FirstThunk))[j];

					VirtualProtect(pAddress, sizeof(DWORD), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
					*pAddress = (DWORD)CustomGetStartupInfoA;
					VirtualProtect(pAddress, sizeof(DWORD), dwProtect[0], &dwProtect[1]);

					// return to the original EP
					*(DWORD*)originalEP = *(DWORD*)&originalCode;
					*(BYTE*)(originalEP+4) = originalCode[4];
					return;
				}

				// For new SA Steam EXE
				if ( !_stricmp((const char*)((DWORD)hExecutableInstance + pFunctions[j]->Name), "GetStartupInfoW") )
				{
					// Overwrite the address with the address to a custom GetStartupInfoA
					DWORD		dwProtect[2];
					DWORD*		pAddress = &((DWORD*)((DWORD)hExecutableInstance + pImports->FirstThunk))[j];

					VirtualProtect(pAddress, sizeof(DWORD), PAGE_EXECUTE_READWRITE, &dwProtect[0]);
					*pAddress = (DWORD)CustomGetStartupInfoW;
					VirtualProtect(pAddress, sizeof(DWORD), dwProtect[0], &dwProtect[1]);

					// return to the original EP
					*(DWORD*)originalEP = *(DWORD*)&originalCode;
					*(BYTE*)(originalEP+4) = originalCode[4];
					return;
				}
			}
		}
		pImports++;
	}

	// No luck. Oh well.
	// return to the original EP anyway
	*(DWORD*)originalEP = *(DWORD*)&originalCode;
	*(BYTE*)(originalEP+4) = originalCode[4];
}

void __declspec(naked) Main_DoInit()
{
	_asm
	{
		call	PatchIAT
		jmp		originalEP
	}
}

BOOL WINAPI DllMain(HINSTANCE hInst, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		hExecutableInstance = GetModuleHandle(NULL); // passing NULL should be safe even with the loader lock being held (according to ReactOS ldr.c)
		GetModuleFileName(hInst, DllPath, MAX_PATH);
		DllName = strrchr(DllPath, '\\');
		SHGetFolderPath(NULL, CSIDL_SYSTEM, NULL, 0, szSystemPath);
		strcat(szSystemPath, DllName);

		if (hExecutableInstance && _stricmp(DllName + 1, "vorbisFile.dll") == NULL)
		{
			IMAGE_NT_HEADERS* ntHeader = (IMAGE_NT_HEADERS*)((DWORD)hExecutableInstance + ((IMAGE_DOS_HEADER*)hExecutableInstance)->e_lfanew);
			BYTE* ep = (BYTE*)((DWORD)hExecutableInstance + ntHeader->OptionalHeader.AddressOfEntryPoint);

			// Unprotect the entry point
			DWORD oldProtect;
			VirtualProtect(ep, 5, PAGE_EXECUTE_READWRITE, &oldProtect);

			// back up original code
			*(DWORD*)&originalCode = *(DWORD*)ep;
			originalCode[4] = *(ep + 4);

			// patch to call our EP
			int newEP = (int)Main_DoInit - ((int)ep + 5);
			ep[0] = 0xE9;

			*(int*)&ep[1] = newEP;

			originalEP = ep;
		}
		else
		{
			LoadPlugins();
			bLoadedPluginsYet = true;
		}
	}

	if (reason == DLL_PROCESS_DETACH)
	{
		FreeLibrary(dsound.dll);
		FreeLibrary(dinput8.dll);
		FreeLibrary(ddraw.dll);
		FreeLibrary(d3d8.dll);
		FreeLibrary(d3d9.dll);
		FreeLibrary(d3d11.dll);
		FreeLibrary(winmmbase.dll);
		FreeLibrary(msacm32.dll);
		MemoryFreeLibrary(hwndmode_dll);
	}
	return TRUE;
}