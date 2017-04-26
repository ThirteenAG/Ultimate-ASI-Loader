#include <windows.h>
#include <dsound.h>
#include <Shlobj.h>

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