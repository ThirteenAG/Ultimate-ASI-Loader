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
#include <dsound.h>

HINSTANCE				hExecutableInstance, hDLLInstance;
TCHAR DllPath[MAX_PATH], *DllName, szSystemPath[MAX_PATH];

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

typedef HRESULT(*fn_DllCanUnloadNow_dsound)();
void _DllCanUnloadNow_dsound() { (fn_DllCanUnloadNow_dsound)dsound.DllCanUnloadNow_dsound(); }

typedef HRESULT(*fn_DllGetClassObject_dsound)(REFCLSID rclsid, REFIID   riid, LPVOID   *ppv);
void _DllGetClassObject_dsound() { (fn_DllGetClassObject_dsound)dsound.DllGetClassObject_dsound(); }

typedef HRESULT(*fn_GetDeviceID)(LPCGUID pGuidSrc, LPGUID pGuidDest);
void _GetDeviceID() { (fn_GetDeviceID)dsound.GetDeviceID(); }

struct ExcludedEntry {
	char*			entry;
	ExcludedEntry*	prev;
	ExcludedEntry*	next;
};

struct ExcludedEntriesList {
	ExcludedEntry*	first;
	ExcludedEntry*	last;
};