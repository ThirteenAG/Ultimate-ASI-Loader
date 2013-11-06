#define APP_NAME    "ASI Loader"
#define APP_VERSION "1.0 [06.11.2013]"

#include <windows.h>
#include <stdio.h>
#include <iostream>
#include <Shlobj.h>
#include <string.h>
#include "snip_str.h"
#include "MemoryModule\MemoryModule.h"
#include "MemoryModule\wndmode_dll_hex.h"
#include <fstream>

#include <cstdio>
#include <windows.h>
#include <tlhelp32.h>
using namespace std;

// This file contains code from
// GTA: SA ASI Loader 1.1
// Written by Silent
// Based on ASI Loader by Stanislav "listener" Golovin
// Initialization part made by NTAuthority
// Proxy library template created with Proxy DLL Templater (http://habrahabr.ru/post/139065/)

BYTE 					originalCode[5];
BYTE* 					originalEP = 0;
HINSTANCE				hExecutableInstance;
HINSTANCE Inst;
HMEMORYMODULE h_wndmode_dll;
char ini_buf[550];
void StartDXWND();
	STARTUPINFO si;
	PROCESS_INFORMATION pi;
bool ProcessRunning( const char* name );

struct ExcludedEntry {
	char*			entry;
	ExcludedEntry*	prev;
	ExcludedEntry*	next;
};

struct ExcludedEntriesList {
	ExcludedEntry*	first;
	ExcludedEntry*	last;
};

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

__declspec(naked) void _DirectInput8Create() { _asm { jmp [dinput8.DirectInput8Create] } }
__declspec(naked) void _DllCanUnloadNow() { _asm { jmp [dinput8.DllCanUnloadNow] } }
__declspec(naked) void _DllGetClassObject() { _asm { jmp [dinput8.DllGetClassObject] } }
__declspec(naked) void _DllRegisterServer() { _asm { jmp [dinput8.DllRegisterServer] } }
__declspec(naked) void _DllUnregisterServer() { _asm { jmp [dinput8.DllUnregisterServer] } }

__declspec(naked) void _DirectSoundCaptureCreate() { _asm { jmp [dsound.DirectSoundCaptureCreate] } }
__declspec(naked) void _DirectSoundCaptureCreate8() { _asm { jmp [dsound.DirectSoundCaptureCreate8] } }
__declspec(naked) void _DirectSoundCaptureEnumerateA() { _asm { jmp [dsound.DirectSoundCaptureEnumerateA] } }
__declspec(naked) void _DirectSoundCaptureEnumerateW() { _asm { jmp [dsound.DirectSoundCaptureEnumerateW] } }
__declspec(naked) void _DirectSoundCreate() { _asm { jmp [dsound.DirectSoundCreate] } }
__declspec(naked) void _DirectSoundCreate8() { _asm { jmp [dsound.DirectSoundCreate8] } }
__declspec(naked) void _DirectSoundEnumerateA() { _asm { jmp [dsound.DirectSoundEnumerateA] } }
__declspec(naked) void _DirectSoundEnumerateW() { _asm { jmp [dsound.DirectSoundEnumerateW] } }
__declspec(naked) void _DirectSoundFullDuplexCreate() { _asm { jmp [dsound.DirectSoundFullDuplexCreate] } }
__declspec(naked) void _DllCanUnloadNow_dsound() { _asm { jmp [dsound.DllCanUnloadNow_dsound] } }
__declspec(naked) void _DllGetClassObject_dsound() { _asm { jmp [dsound.DllGetClassObject_dsound] } }
__declspec(naked) void _GetDeviceID() { _asm { jmp [dsound.GetDeviceID] } }

__declspec(naked) void FakeAcquireDDThreadLock()			{ _asm { jmp [ddraw.AcquireDDThreadLock] } }
__declspec(naked) void FakeCheckFullscreen()				{ _asm { jmp [ddraw.CheckFullscreen] } }
__declspec(naked) void FakeCompleteCreateSysmemSurface()	{ _asm { jmp [ddraw.CompleteCreateSysmemSurface] } }
__declspec(naked) void FakeD3DParseUnknownCommand()			{ _asm { jmp [ddraw.D3DParseUnknownCommand] } }
__declspec(naked) void FakeDDGetAttachedSurfaceLcl()		{ _asm { jmp [ddraw.DDGetAttachedSurfaceLcl] } }
__declspec(naked) void FakeDDInternalLock()					{ _asm { jmp [ddraw.DDInternalLock] } }
__declspec(naked) void FakeDDInternalUnlock()				{ _asm { jmp [ddraw.DDInternalUnlock] } }
__declspec(naked) void FakeDSoundHelp()						{ _asm { jmp [ddraw.DSoundHelp] } }
// HRESULT WINAPI DirectDrawCreate( GUID FAR *lpGUID, LPDIRECTDRAW FAR *lplpDD, IUnknown FAR *pUnkOuter );
__declspec(naked) void FakeDirectDrawCreate()				{ _asm { jmp [ddraw.DirectDrawCreate] } }
// HRESULT WINAPI DirectDrawCreateClipper( DWORD dwFlags, LPDIRECTDRAWCLIPPER FAR *lplpDDClipper, IUnknown FAR *pUnkOuter );
__declspec(naked) void FakeDirectDrawCreateClipper()		{ _asm { jmp [ddraw.DirectDrawCreateClipper] } }
// HRESULT WINAPI DirectDrawCreateEx( GUID FAR * lpGuid, LPVOID *lplpDD, REFIID iid,IUnknown FAR *pUnkOuter );
__declspec(naked) void FakeDirectDrawCreateEx()				{ _asm { jmp [ddraw.DirectDrawCreateEx] } }
// HRESULT WINAPI DirectDrawEnumerateA( LPDDENUMCALLBACKA lpCallback, LPVOID lpContext );
__declspec(naked) void FakeDirectDrawEnumerateA()			{ _asm { jmp [ddraw.DirectDrawEnumerateA] } }
// HRESULT WINAPI DirectDrawEnumerateExA( LPDDENUMCALLBACKEXA lpCallback, LPVOID lpContext, DWORD dwFlags );
__declspec(naked) void FakeDirectDrawEnumerateExA()			{ _asm { jmp [ddraw.DirectDrawEnumerateExA] } }
// HRESULT WINAPI DirectDrawEnumerateExW( LPDDENUMCALLBACKEXW lpCallback, LPVOID lpContext, DWORD dwFlags );
__declspec(naked) void FakeDirectDrawEnumerateExW()			{ _asm { jmp [ddraw.DirectDrawEnumerateExW] } }
// HRESULT WINAPI DirectDrawEnumerateW( LPDDENUMCALLBACKW lpCallback, LPVOID lpContext );
__declspec(naked) void FakeDirectDrawEnumerateW()			{ _asm { jmp [ddraw.DirectDrawEnumerateW] } }
__declspec(naked) void FakeDllCanUnloadNow()				{ _asm { jmp [ddraw.DllCanUnloadNow] } }
__declspec(naked) void FakeDllGetClassObject()				{ _asm { jmp [ddraw.DllGetClassObject] } }
__declspec(naked) void FakeGetDDSurfaceLocal()				{ _asm { jmp [ddraw.GetDDSurfaceLocal] } }
__declspec(naked) void FakeGetOLEThunkData()				{ _asm { jmp [ddraw.GetOLEThunkData] } }
__declspec(naked) void FakeGetSurfaceFromDC()				{ _asm { jmp [ddraw.GetSurfaceFromDC] } }
__declspec(naked) void FakeRegisterSpecialCase()			{ _asm { jmp [ddraw.RegisterSpecialCase] } }
__declspec(naked) void FakeReleaseDDThreadLock()			{ _asm { jmp [ddraw.ReleaseDDThreadLock] } }

__declspec(naked) void _DebugSetMute_d3d8() { _asm { jmp [d3d8.DebugSetMute_d3d8] } }
__declspec(naked) void _Direct3DCreate8() { _asm { jmp [d3d8.Direct3DCreate8] } }
__declspec(naked) void _ValidatePixelShader() { _asm { jmp [d3d8.ValidatePixelShader] } }
__declspec(naked) void _ValidateVertexShader() { _asm { jmp [d3d8.ValidateVertexShader] } }

__declspec(naked) void _D3DPERF_BeginEvent() { _asm { jmp [d3d9.D3DPERF_BeginEvent] } }
__declspec(naked) void _D3DPERF_EndEvent() { _asm { jmp [d3d9.D3DPERF_EndEvent] } }
__declspec(naked) void _D3DPERF_GetStatus() { _asm { jmp [d3d9.D3DPERF_GetStatus] } }
__declspec(naked) void _D3DPERF_QueryRepeatFrame() { _asm { jmp [d3d9.D3DPERF_QueryRepeatFrame] } }
__declspec(naked) void _D3DPERF_SetMarker() { _asm { jmp [d3d9.D3DPERF_SetMarker] } }
__declspec(naked) void _D3DPERF_SetOptions() { _asm { jmp [d3d9.D3DPERF_SetOptions] } }
__declspec(naked) void _D3DPERF_SetRegion() { _asm { jmp [d3d9.D3DPERF_SetRegion] } }
__declspec(naked) void _DebugSetLevel() { _asm { jmp [d3d9.DebugSetLevel] } }
__declspec(naked) void _DebugSetMute() { _asm { jmp [d3d9.DebugSetMute] } }
//__declspec(naked) void _Direct3D9EnableMaximizedWindowedModeShim() { _asm { jmp [d3d9.Direct3D9EnableMaximizedWindowedModeShim] } }
__declspec(naked) void _Direct3DCreate9() { _asm { jmp [d3d9.Direct3DCreate9] } }
__declspec(naked) void _Direct3DCreate9Ex() { _asm { jmp [d3d9.Direct3DCreate9Ex] } }
__declspec(naked) void _Direct3DShaderValidatorCreate9() { _asm { jmp [d3d9.Direct3DShaderValidatorCreate9] } }
__declspec(naked) void _PSGPError() { _asm { jmp [d3d9.PSGPError] } }
__declspec(naked) void _PSGPSampleTexture() { _asm { jmp [d3d9.PSGPSampleTexture] } }

__declspec(naked) void _D3D11CoreCreateDevice() { _asm { jmp [d3d11.D3D11CoreCreateDevice] } }
__declspec(naked) void _D3D11CoreCreateLayeredDevice() { _asm { jmp [d3d11.D3D11CoreCreateLayeredDevice] } }
__declspec(naked) void _D3D11CoreGetLayeredDeviceSize() { _asm { jmp [d3d11.D3D11CoreGetLayeredDeviceSize] } }
__declspec(naked) void _D3D11CoreRegisterLayers() { _asm { jmp [d3d11.D3D11CoreRegisterLayers] } }
__declspec(naked) void _D3D11CreateDevice() { _asm { jmp [d3d11.D3D11CreateDevice] } }
__declspec(naked) void _D3D11CreateDeviceAndSwapChain() { _asm { jmp [d3d11.D3D11CreateDeviceAndSwapChain] } }
__declspec(naked) void _D3DKMTCloseAdapter() { _asm { jmp [d3d11.D3DKMTCloseAdapter] } }
__declspec(naked) void _D3DKMTCreateAllocation() { _asm { jmp [d3d11.D3DKMTCreateAllocation] } }
__declspec(naked) void _D3DKMTCreateContext() { _asm { jmp [d3d11.D3DKMTCreateContext] } }
__declspec(naked) void _D3DKMTCreateDevice() { _asm { jmp [d3d11.D3DKMTCreateDevice] } }
__declspec(naked) void _D3DKMTCreateSynchronizationObject() { _asm { jmp [d3d11.D3DKMTCreateSynchronizationObject] } }
__declspec(naked) void _D3DKMTDestroyAllocation() { _asm { jmp [d3d11.D3DKMTDestroyAllocation] } }
__declspec(naked) void _D3DKMTDestroyContext() { _asm { jmp [d3d11.D3DKMTDestroyContext] } }
__declspec(naked) void _D3DKMTDestroyDevice() { _asm { jmp [d3d11.D3DKMTDestroyDevice] } }
__declspec(naked) void _D3DKMTDestroySynchronizationObject() { _asm { jmp [d3d11.D3DKMTDestroySynchronizationObject] } }
__declspec(naked) void _D3DKMTEscape() { _asm { jmp [d3d11.D3DKMTEscape] } }
__declspec(naked) void _D3DKMTGetContextSchedulingPriority() { _asm { jmp [d3d11.D3DKMTGetContextSchedulingPriority] } }
__declspec(naked) void _D3DKMTGetDeviceState() { _asm { jmp [d3d11.D3DKMTGetDeviceState] } }
__declspec(naked) void _D3DKMTGetDisplayModeList() { _asm { jmp [d3d11.D3DKMTGetDisplayModeList] } }
__declspec(naked) void _D3DKMTGetMultisampleMethodList() { _asm { jmp [d3d11.D3DKMTGetMultisampleMethodList] } }
__declspec(naked) void _D3DKMTGetRuntimeData() { _asm { jmp [d3d11.D3DKMTGetRuntimeData] } }
__declspec(naked) void _D3DKMTGetSharedPrimaryHandle() { _asm { jmp [d3d11.D3DKMTGetSharedPrimaryHandle] } }
__declspec(naked) void _D3DKMTLock() { _asm { jmp [d3d11.D3DKMTLock] } }
__declspec(naked) void _D3DKMTOpenAdapterFromHdc() { _asm { jmp [d3d11.D3DKMTOpenAdapterFromHdc] } }
__declspec(naked) void _D3DKMTOpenResource() { _asm { jmp [d3d11.D3DKMTOpenResource] } }
__declspec(naked) void _D3DKMTPresent() { _asm { jmp [d3d11.D3DKMTPresent] } }
__declspec(naked) void _D3DKMTQueryAdapterInfo() { _asm { jmp [d3d11.D3DKMTQueryAdapterInfo] } }
__declspec(naked) void _D3DKMTQueryAllocationResidency() { _asm { jmp [d3d11.D3DKMTQueryAllocationResidency] } }
__declspec(naked) void _D3DKMTQueryResourceInfo() { _asm { jmp [d3d11.D3DKMTQueryResourceInfo] } }
__declspec(naked) void _D3DKMTRender() { _asm { jmp [d3d11.D3DKMTRender] } }
__declspec(naked) void _D3DKMTSetAllocationPriority() { _asm { jmp [d3d11.D3DKMTSetAllocationPriority] } }
__declspec(naked) void _D3DKMTSetContextSchedulingPriority() { _asm { jmp [d3d11.D3DKMTSetContextSchedulingPriority] } }
__declspec(naked) void _D3DKMTSetDisplayMode() { _asm { jmp [d3d11.D3DKMTSetDisplayMode] } }
__declspec(naked) void _D3DKMTSetDisplayPrivateDriverFormat() { _asm { jmp [d3d11.D3DKMTSetDisplayPrivateDriverFormat] } }
__declspec(naked) void _D3DKMTSetGammaRamp() { _asm { jmp [d3d11.D3DKMTSetGammaRamp] } }
__declspec(naked) void _D3DKMTSetVidPnSourceOwner() { _asm { jmp [d3d11.D3DKMTSetVidPnSourceOwner] } }
__declspec(naked) void _D3DKMTSignalSynchronizationObject() { _asm { jmp [d3d11.D3DKMTSignalSynchronizationObject] } }
__declspec(naked) void _D3DKMTUnlock() { _asm { jmp [d3d11.D3DKMTUnlock] } }
__declspec(naked) void _D3DKMTWaitForSynchronizationObject() { _asm { jmp [d3d11.D3DKMTWaitForSynchronizationObject] } }
__declspec(naked) void _D3DKMTWaitForVerticalBlankEvent() { _asm { jmp [d3d11.D3DKMTWaitForVerticalBlankEvent] } }
__declspec(naked) void _D3DPerformance_BeginEvent() { _asm { jmp [d3d11.D3DPerformance_BeginEvent] } }
__declspec(naked) void _D3DPerformance_EndEvent() { _asm { jmp [d3d11.D3DPerformance_EndEvent] } }
__declspec(naked) void _D3DPerformance_GetStatus() { _asm { jmp [d3d11.D3DPerformance_GetStatus] } }
__declspec(naked) void _D3DPerformance_SetMarker() { _asm { jmp [d3d11.D3DPerformance_SetMarker] } }
__declspec(naked) void _EnableFeatureLevelUpgrade() { _asm { jmp [d3d11.EnableFeatureLevelUpgrade] } }
__declspec(naked) void _OpenAdapter10() { _asm { jmp [d3d11.OpenAdapter10] } }
__declspec(naked) void _OpenAdapter10_2() { _asm { jmp [d3d11.OpenAdapter10_2] } }

TCHAR DllPath[MAX_PATH + 1], *DllName, szPath[MAX_PATH];

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

void Main_DoInit()
{
  GetModuleFileName(Inst, DllPath, MAX_PATH);
  DllName = strrchr(DllPath,'\\');
  SHGetFolderPath(NULL, CSIDL_SYSTEM, NULL, 0, szPath);
  strcat(szPath, DllName);
 
	  if (stristr(DllName, "dsound") != NULL) {
		  dsound.dll = LoadLibrary(szPath);
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
	  } else {
	  if (stristr(DllName, "dinput") != NULL) {
		  dinput8.dll = LoadLibrary(szPath);
      dinput8.DirectInput8Create = GetProcAddress(dinput8.dll, "DirectInput8Create");
      dinput8.DllCanUnloadNow = GetProcAddress(dinput8.dll, "DllCanUnloadNow");
      dinput8.DllGetClassObject = GetProcAddress(dinput8.dll, "DllGetClassObject");
      dinput8.DllRegisterServer = GetProcAddress(dinput8.dll, "DllRegisterServer");
      dinput8.DllUnregisterServer = GetProcAddress(dinput8.dll, "DllUnregisterServer");
	  } else {
	  if (stristr(DllName, "ddraw") != NULL) {
		  ddraw.dll = LoadLibrary(szPath);
			ddraw.AcquireDDThreadLock			= GetProcAddress(ddraw.dll, "AcquireDDThreadLock");
			ddraw.CheckFullscreen				= GetProcAddress(ddraw.dll, "CheckFullscreen");
			ddraw.CompleteCreateSysmemSurface	= GetProcAddress(ddraw.dll, "CompleteCreateSysmemSurface");
			ddraw.D3DParseUnknownCommand		= GetProcAddress(ddraw.dll, "D3DParseUnknownCommand");
			ddraw.DDGetAttachedSurfaceLcl		= GetProcAddress(ddraw.dll, "DDGetAttachedSurfaceLcl");
			ddraw.DDInternalLock				= GetProcAddress(ddraw.dll, "DDInternalLock");
			ddraw.DDInternalUnlock				= GetProcAddress(ddraw.dll, "DDInternalUnlock");
			ddraw.DSoundHelp					= GetProcAddress(ddraw.dll, "DSoundHelp");
			ddraw.DirectDrawCreate				= GetProcAddress(ddraw.dll, "DirectDrawCreate");
			ddraw.DirectDrawCreateClipper		= GetProcAddress(ddraw.dll, "DirectDrawCreateClipper");
			ddraw.DirectDrawCreateEx			= GetProcAddress(ddraw.dll, "DirectDrawCreateEx");
			ddraw.DirectDrawEnumerateA			= GetProcAddress(ddraw.dll, "DirectDrawEnumerateA");
			ddraw.DirectDrawEnumerateExA		= GetProcAddress(ddraw.dll, "DirectDrawEnumerateExA");
			ddraw.DirectDrawEnumerateExW		= GetProcAddress(ddraw.dll, "DirectDrawEnumerateExW");
			ddraw.DirectDrawEnumerateW			= GetProcAddress(ddraw.dll, "DirectDrawEnumerateW");
			ddraw.DllCanUnloadNow				= GetProcAddress(ddraw.dll, "DllCanUnloadNow");
			ddraw.DllGetClassObject				= GetProcAddress(ddraw.dll, "DllGetClassObject");
			ddraw.GetDDSurfaceLocal				= GetProcAddress(ddraw.dll, "GetDDSurfaceLocal");
			ddraw.GetOLEThunkData				= GetProcAddress(ddraw.dll, "GetOLEThunkData");
			ddraw.GetSurfaceFromDC				= GetProcAddress(ddraw.dll, "GetSurfaceFromDC");
			ddraw.RegisterSpecialCase			= GetProcAddress(ddraw.dll, "RegisterSpecialCase");
			ddraw.ReleaseDDThreadLock			= GetProcAddress(ddraw.dll, "ReleaseDDThreadLock");
	  } else {	  
	  if (stristr(DllName, "d3d8.dll") != NULL) {
		  d3d8.dll = LoadLibrary(szPath);
      d3d8.DebugSetMute_d3d8 = GetProcAddress(d3d8.dll, "DebugSetMute_d3d8");
      d3d8.Direct3DCreate8 = GetProcAddress(d3d8.dll, "Direct3DCreate8");
      d3d8.ValidatePixelShader = GetProcAddress(d3d8.dll, "ValidatePixelShader");
      d3d8.ValidateVertexShader = GetProcAddress(d3d8.dll, "ValidateVertexShader");
	  } else {	  
	  if (stristr(DllName, "d3d9.dll") != NULL) {
		  d3d9.dll = LoadLibrary(szPath);
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
	  } else {	  
	  if (stristr(DllName, "d3d11.dll") != NULL) {
		  d3d11.dll = LoadLibrary(szPath);
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
	  } else {	 
				MessageBox(0, "This library isn't supported. Try to rename it to dinput8.dll, dsound.dll or ddraw.dll.", "ASI Loader", MB_ICONERROR);
				ExitProcess(0);
	  }
	  }
	  }
	  }
	  }
	  }

	  fstream wndmode_ini;
	  wndmode_ini.open("wndmode.ini", ios_base::out | ios_base::in);  // will not create wndmode.ini
	  if (wndmode_ini.is_open())
	  {
		  wndmode_ini.read(ini_buf, 550);
			if (stristr(ini_buf, "[WINDOWMODE]") == NULL) {
				wndmode_ini.clear();
				wndmode_ini.write(wndmode_ini_hex, 539);
			} 
			wndmode_ini.close();
			h_wndmode_dll = MemoryLoadLibrary(wndmode_dll_hex);
	  } else {
	  fstream dxwnd_ini;
	  dxwnd_ini.open("dxwnd.ini", ios_base::out | ios_base::in);
	  if (dxwnd_ini.is_open())
	  {
		  dxwnd_ini.read(ini_buf, 550);
			if (stristr(ini_buf, "[target]") == NULL) {
				dxwnd_ini.clear();
				dxwnd_ini.write(dxwnd_ini_hex, 285);
			} 
			dxwnd_ini.close();
			//adding game path
			TCHAR exepath[MAX_PATH+1];
			if(0 != GetModuleFileName(0, exepath, MAX_PATH+1)) 
			{
			WritePrivateProfileString("target", "title0", exepath, ".\\dxwnd.ini");
			WritePrivateProfileString("target", "path0", exepath, ".\\dxwnd.ini");
			StartDXWND();
			}
	  }
	  }

	  
		// Regular ASI Loader
        WIN32_FIND_DATA fd;
		char			moduleName[MAX_PATH];
		char			preparedPath[128];	// stores scripts\*exename*\settings.ini
		char			dllPath[128];
		char*			tempPointer;
		int 			nWantsToLoadPlugins;
		int				nThatExeWantsPlugins;
		int 			nWndModeWantsToLoadPlugins;

		GetModuleFileName(NULL, moduleName, MAX_PATH);
		tempPointer = strrchr(moduleName, '.');
		*tempPointer  = '\0';

		tempPointer = strrchr(moduleName, '\\');

		strncpy(dllPath, moduleName, (tempPointer - moduleName + 1));
		dllPath[tempPointer - moduleName + 1] = '\0';
		SetCurrentDirectory(dllPath);

		strncpy(preparedPath, "scripts", 8);
		strcat(preparedPath, tempPointer);
		strcat(preparedPath, "\\settings.ini");

		// Before we load any ASI files, let's see if user wants to do it at all
		nWantsToLoadPlugins = GetPrivateProfileInt("globalsets", "loadplugins", TRUE, "scripts\\global.ini");
		// Or perhaps this EXE wants to override global settings?
		nThatExeWantsPlugins = GetPrivateProfileInt("exclusivesets", "loadplugins", -1, preparedPath);
		// Windowed mode config can disable plugin loading aswell
		if (stristr(ini_buf, "LoadPlugins=0") == NULL) {
		nWndModeWantsToLoadPlugins = TRUE;
		}

		if ( nThatExeWantsPlugins && nWndModeWantsToLoadPlugins == TRUE )	// Will not process only if this EXE wishes not to load anything but its exclusive plugins
		{
			if ( nWantsToLoadPlugins || nThatExeWantsPlugins == TRUE )
			{
				// Load excludes
				ExcludedEntriesList	excludes;

				ExcludedEntriesListInit(&excludes);
				if ( FILE* iniFile = fopen(preparedPath, "rt"))
				{
					char	line[256];
					bool	bItsExcludesList = false;

					while ( fgets(line, 256, iniFile) )
					{
						char*	newline = strchr(line, '\n');

						if ( newline )
							*newline = '\0';

						if ( bItsExcludesList )
						{
							if ( line[0] && line[0] != ';' )
								ExcludedEntriesListPush(&excludes, line);
						}
						else
						{
							if ( !_stricmp(line, "[excludes]") )
								bItsExcludesList = true;
						}
					}

					fclose(iniFile);
				}

				FindFiles(&fd, &excludes);
				if ( SetCurrentDirectory("scripts\\") )
				{
					FindFiles(&fd, &excludes);
					if ( SetCurrentDirectory(tempPointer + 1) )
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
			*(tempPointer + 1) = '\0';
			if ( SetCurrentDirectory(preparedPath) )
			{
				FindFiles(&fd, NULL);
				SetCurrentDirectory("..\\..\\");
			}
		}

	/*// Unprotect the module NOW (CLEO 4.1.1.30f crash fix)
    IMAGE_NT_HEADERS* ntHeader = (IMAGE_NT_HEADERS*)((DWORD)hExecutableInstance + ((IMAGE_DOS_HEADER*)hExecutableInstance)->e_lfanew);

    SIZE_T size = ntHeader->OptionalHeader.SizeOfImage;
	DWORD oldProtect;
	VirtualProtect((VOID*)hExecutableInstance, size, PAGE_EXECUTE_READWRITE, &oldProtect);

    // return to the original EP
	*(DWORD*)originalEP = *(DWORD*)&originalCode;
	*(BYTE*)(originalEP+4) = originalCode[4];
	_asm
	{
		mov		esp, ebp
		pop		ebp
		jmp		originalEP
	}*/
}

void main(){ }


BOOL WINAPI DllMain(HINSTANCE hInst,DWORD reason,LPVOID)
	{
	if ( reason == DLL_PROCESS_ATTACH )
	{
	    hExecutableInstance = GetModuleHandle(NULL); // passing NULL should be safe even with the loader lock being held (according to ReactOS ldr.c)
		Inst = hInst;
        if (hExecutableInstance)
        {
            /*IMAGE_NT_HEADERS* ntHeader = (IMAGE_NT_HEADERS*)((DWORD)hExecutableInstance + ((IMAGE_DOS_HEADER*)hExecutableInstance)->e_lfanew);
			BYTE* ep = (BYTE*)((DWORD)hExecutableInstance + ntHeader->OptionalHeader.AddressOfEntryPoint);

			// Unprotect the entry point
			DWORD oldProtect;
			VirtualProtect(ep, 5, PAGE_EXECUTE_READWRITE, &oldProtect);

            // back up original code
			*(DWORD*)&originalCode = *(DWORD*)ep;
			originalCode[4] = *(ep+4);

            // patch to call our EP
            int newEP = (int)Main_DoInit - ((int)ep + 5);
            ep[0] = 0xE9; // for some reason this doesn't work properly when run under the debugger

			*(int*)&ep[1] = newEP;

            originalEP = ep;*/
			Main_DoInit();
		}
	}

	if ( reason == DLL_PROCESS_DETACH )
	{
		FreeLibrary(dsound.dll);
		FreeLibrary(dinput8.dll);
		FreeLibrary(ddraw.dll);
		FreeLibrary(d3d8.dll);
		FreeLibrary(d3d9.dll);
		FreeLibrary(d3d11.dll);
		MemoryFreeLibrary(h_wndmode_dll);
		TerminateProcess(pi.hProcess,0);
	}
    return TRUE;
}


void StartDXWND() {
	if(!ProcessRunning("dxwnd.exe")) {
	DWORD dwExitCode = 0;
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		si.dwFlags = STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;
		ZeroMemory(&pi, sizeof(pi));
		CreateProcess("dxwnd.exe", NULL, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);
	}
	return;
}



bool ProcessRunning( const char* name )
{
	HANDLE SnapShot = CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );

	if( SnapShot == INVALID_HANDLE_VALUE )
		return false;

	PROCESSENTRY32 procEntry;
	procEntry.dwSize = sizeof( PROCESSENTRY32 );

	if( !Process32First( SnapShot, &procEntry ) )
		return false;

	do
	{
		if( strcmp( procEntry.szExeFile, name ) == 0 )
			return true;
	}
	while( Process32Next( SnapShot, &procEntry ) );

	return false;
}