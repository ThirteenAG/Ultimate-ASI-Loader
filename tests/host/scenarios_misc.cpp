// Scenarios for export forwarding, IAT handling, COM hook, overload query exports,
// x86-only features, zip packages and the virtual file server.
#include "host.hpp"
#include "../common/pattern.hpp"
#include "../common/sentinels.hpp"
#include <atomic>
#include <filesystem>
#include <thread>
#include <tlhelp32.h>
#include <initguid.h>
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <mmsystem.h>
#include <dsound.h>
#include <wininet.h>
#include <d3d9.h>
#include <d3d10_1.h>
#include <d3d11.h>
#include <d3d12.h>
#include <dxgi.h>
#include <xinput.h>

#pragma comment(lib, "ole32.lib")

using namespace host;

namespace
{
    template<class F>
    F Fn(HMODULE m, const char* name)
    {
        return reinterpret_cast<F>(GetProcAddress(m, name));
    }

    std::wstring Lower(std::wstring s)
    {
        CharLowerBuffW(s.data(), (DWORD)s.size());
        return s;
    }

    // Loader and system DLL, to compare an export's results
    struct Pair
    {
        HMODULE proxy = nullptr;
        HMODULE system = nullptr;
    };

    bool GetPair(Host& host, const std::wstring& dll, Pair& p)
    {
        if (!host.RequireUal()) return false;
        p.proxy = host.ual.module;
        p.system = LoadLibraryW((SystemDir() + L"\\" + dll).c_str());
        if (!p.system)
        {
            host.Skip("system " + ualtest::utf8(dll) + " is not available on this machine");
            return false;
        }
        host.Check(p.system != p.proxy, "the original DLL is a different module than the loader");
        return true;
    }

    PIMAGE_NT_HEADERS Nt(HMODULE m)
    {
        auto base = reinterpret_cast<BYTE*>(m);
        return reinterpret_cast<PIMAGE_NT_HEADERS>(base + reinterpret_cast<PIMAGE_DOS_HEADER>(base)->e_lfanew);
    }

    // fn(dll, function or "#ordinal", IAT slot) for each import of m
    template<class F>
    void ForEachImport(HMODULE m, F fn)
    {
        auto base = reinterpret_cast<BYTE*>(m);
        auto& dir = Nt(m)->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
        if (!dir.VirtualAddress) return;
        for (auto d = reinterpret_cast<PIMAGE_IMPORT_DESCRIPTOR>(base + dir.VirtualAddress); d->Name; ++d)
        {
            std::string dll = reinterpret_cast<const char*>(base + d->Name);
            auto names = reinterpret_cast<PIMAGE_THUNK_DATA>(base + (d->OriginalFirstThunk ? d->OriginalFirstThunk : d->FirstThunk));
            auto iat = reinterpret_cast<PIMAGE_THUNK_DATA>(base + d->FirstThunk);
            for (; names->u1.AddressOfData; ++names, ++iat)
            {
                std::string fname;
                if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal))
                    fname = "#" + std::to_string(IMAGE_ORDINAL(names->u1.Ordinal));
                else if (d->OriginalFirstThunk)
                    fname = reinterpret_cast<PIMAGE_IMPORT_BY_NAME>(base + names->u1.AddressOfData)->Name;
                fn(dll, fname, reinterpret_cast<void**>(&iat->u1.Function));
            }
        }
    }

    bool IsWritable(const void* p)
    {
        MEMORY_BASIC_INFORMATION mbi{};
        if (!VirtualQuery(p, &mbi, sizeof(mbi))) return false;
        return (mbi.Protect & (PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
    }

    void* SectionAddress(HMODULE m, const char* name)
    {
        auto nt = Nt(m);
        auto sec = IMAGE_FIRST_SECTION(nt);
        for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec)
            if (strncmp(reinterpret_cast<const char*>(sec->Name), name, IMAGE_SIZEOF_SHORT_NAME) == 0)
                return reinterpret_cast<BYTE*>(m) + sec->VirtualAddress;
        return nullptr;
    }
}

HOST_SCENARIO(exports)
{
    if (!host.RequireUal()) return;
    host.Check(host.ual.IsUltimateASILoader(), "IsUltimateASILoader() returns true");
}

// --- forwarding

namespace
{
    int CountDsoundDevices(HMODULE m, HRESULT* hr)
    {
        auto f = Fn<HRESULT(WINAPI*)(LPDSENUMCALLBACKW, LPVOID)>(m, "DirectSoundEnumerateW");
        int count = 0;
        *hr = f ? f([](LPGUID, LPCWSTR, LPCWSTR, LPVOID ctx) -> BOOL { ++*static_cast<int*>(ctx); return TRUE; }, &count) : E_NOTIMPL;
        return count;
    }

    struct Cracked { BOOL ok; INTERNET_PORT port; int scheme; std::wstring hostName; };
}

// One export of <proxy> through the loader and the system DLL must give the same result.
// Covers the export trampolines, including the x64 tail-call wrappers.
HOST_SCENARIO(forward)
{
    std::wstring proxy = Lower(args);
    Pair p;
    if (proxy == L"dinput8.dll")
    {
        if (!GetPair(host, proxy, p)) return;
        using T = HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
        auto call = [&](HMODULE m, bool& gotObj) {
            void* obj = nullptr;
            HRESULT hr = Fn<T>(m, "DirectInput8Create")(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W, &obj, nullptr);
            gotObj = obj != nullptr;
            if (obj) static_cast<IUnknown*>(obj)->Release();
            return hr;
        };
        bool a, b;
        HRESULT hp = call(p.proxy, a), hs = call(p.system, b);
        host.CheckEq(hp, hs, "DirectInput8Create result matches the system DLL");
        host.Check(SUCCEEDED(hp) && a, "DirectInput8Create through the loader returns an interface");
    }
    else if (proxy == L"dinput.dll")
    {
        if (!GetPair(host, proxy, p)) return;
        using T = HRESULT(WINAPI*)(HINSTANCE, DWORD, LPVOID*, LPUNKNOWN);
        auto call = [&](HMODULE m) {
            void* obj = nullptr;
            HRESULT hr = Fn<T>(m, "DirectInputCreateW")(GetModuleHandleW(nullptr), 0x0700, &obj, nullptr);
            if (obj) static_cast<IUnknown*>(obj)->Release();
            return hr;
        };
        HRESULT hp = call(p.proxy), hs = call(p.system);
        host.CheckEq(hp, hs, "DirectInputCreateW result matches the system DLL");
        host.Check(SUCCEEDED(hp), "DirectInputCreateW through the loader succeeds");
    }
    else if (proxy == L"dsound.dll")
    {
        if (!GetPair(host, proxy, p)) return;
        HRESULT hp, hs;
        int cp = CountDsoundDevices(p.proxy, &hp), cs = CountDsoundDevices(p.system, &hs);
        host.CheckEq(hp, hs, "DirectSoundEnumerateW result matches");
        host.CheckEq(cp, cs, "DirectSoundEnumerateW enumerates the same devices");
    }
    else if (proxy == L"wininet.dll" || proxy == L"winhttp.dll")
    {
        if (!GetPair(host, proxy, p)) return;
        bool inet = proxy == L"wininet.dll";
        auto crack = [&](HMODULE m) {
            wchar_t hostBuf[128] = {};
            Cracked c{};
            if (inet)
            {
                URL_COMPONENTSW uc{ sizeof(uc) };
                uc.lpszHostName = hostBuf;
                uc.dwHostNameLength = 128;
                c.ok = Fn<BOOL(WINAPI*)(LPCWSTR, DWORD, DWORD, LPURL_COMPONENTSW)>(m, "InternetCrackUrlW")(L"https://example.com:8443/a/b?c=d", 0, 0, &uc);
                c.port = uc.nPort;
                c.scheme = uc.nScheme;
            }
            else
            {
                URL_COMPONENTSW uc{ sizeof(uc) };
                uc.lpszHostName = hostBuf;
                uc.dwHostNameLength = 128;
                c.ok = Fn<BOOL(WINAPI*)(LPCWSTR, DWORD, DWORD, LPURL_COMPONENTSW)>(m, "WinHttpCrackUrl")(L"https://example.com:8443/a/b?c=d", 0, 0, &uc);
                c.port = uc.nPort;
                c.scheme = uc.nScheme;
            }
            c.hostName = hostBuf;
            return c;
        };
        auto a = crack(p.proxy), b = crack(p.system);
        host.Check(a.ok && b.ok, "URL cracking succeeds through loader and system DLL");
        host.CheckEq((int)a.port, 8443, "port through the loader");
        host.Check(a.port == b.port && a.scheme == b.scheme && a.hostName == b.hostName, "URL components match the system DLL");
    }
    else if (proxy == L"version.dll")
    {
        if (!GetPair(host, proxy, p)) return;
        auto target = SystemDir() + L"\\kernel32.dll";
        DWORD h1 = 0, h2 = 0;
        using T = DWORD(WINAPI*)(LPCWSTR, LPDWORD);
        DWORD a = Fn<T>(p.proxy, "GetFileVersionInfoSizeW")(target.c_str(), &h1);
        DWORD b = Fn<T>(p.system, "GetFileVersionInfoSizeW")(target.c_str(), &h2);
        host.Check(a != 0, "GetFileVersionInfoSizeW through the loader returns a size");
        host.CheckEq(a, b, "GetFileVersionInfoSizeW matches the system DLL");
    }
    else if (proxy == L"winmm.dll")
    {
        if (!GetPair(host, proxy, p)) return;
        using T = MMRESULT(WINAPI*)(LPTIMECAPS, UINT);
        TIMECAPS a{}, b{};
        MMRESULT ra = Fn<T>(p.proxy, "timeGetDevCaps")(&a, sizeof(a));
        MMRESULT rb = Fn<T>(p.system, "timeGetDevCaps")(&b, sizeof(b));
        host.CheckEq(ra, rb, "timeGetDevCaps result matches");
        host.Check(a.wPeriodMin == b.wPeriodMin && a.wPeriodMax == b.wPeriodMax, "timeGetDevCaps data matches");
        DWORD t = Fn<DWORD(WINAPI*)()>(p.proxy, "timeGetTime")();
        host.Check(t != 0, "timeGetTime through the loader");
    }
    else if (proxy == L"d3d9.dll")
    {
        if (!GetPair(host, proxy, p)) return;
        using T = IDirect3D9*(WINAPI*)(UINT);
        auto a = Fn<T>(p.proxy, "Direct3DCreate9")(D3D_SDK_VERSION);
        auto b = Fn<T>(p.system, "Direct3DCreate9")(D3D_SDK_VERSION);
        host.CheckEq(a != nullptr, b != nullptr, "Direct3DCreate9 availability matches");
        if (a && b) host.CheckEq(a->GetAdapterCount(), b->GetAdapterCount(), "adapter count matches");
        if (a) a->Release();
        if (b) b->Release();
    }
    else if (proxy == L"d3d10.dll")
    {
        if (!GetPair(host, proxy, p)) return;
        using T = HRESULT(WINAPI*)(SIZE_T, LPD3D10BLOB*);
        ID3D10Blob *a = nullptr, *b = nullptr;
        HRESULT ha = Fn<T>(p.proxy, "D3D10CreateBlob")(64, &a), hb = Fn<T>(p.system, "D3D10CreateBlob")(64, &b);
        host.CheckEq(ha, hb, "D3D10CreateBlob result matches");
        host.Check(a && a->GetBufferSize() == 64, "D3D10CreateBlob through the loader");
        if (a) a->Release();
        if (b) b->Release();
    }
    else if (proxy == L"d3d11.dll")
    {
        if (!GetPair(host, proxy, p)) return;
        auto create = [&](HMODULE m, D3D_FEATURE_LEVEL& fl) {
            ID3D11Device* dev = nullptr;
            HRESULT hr = Fn<PFN_D3D11_CREATE_DEVICE>(m, "D3D11CreateDevice")(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &dev, &fl, nullptr);
            if (dev) dev->Release();
            return hr;
        };
        D3D_FEATURE_LEVEL fa{}, fb{};
        HRESULT ha = create(p.proxy, fa), hb = create(p.system, fb);
        host.CheckEq(ha, hb, "D3D11CreateDevice(WARP) result matches");
        host.CheckEq((int)fa, (int)fb, "feature level matches");
    }
    else if (proxy == L"d3d12.dll")
    {
        if (!GetPair(host, proxy, p)) return;
        D3D12_ROOT_SIGNATURE_DESC desc{};
        desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        ID3DBlob *a = nullptr, *b = nullptr;
        HRESULT ha = Fn<PFN_D3D12_SERIALIZE_ROOT_SIGNATURE>(p.proxy, "D3D12SerializeRootSignature")(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &a, nullptr);
        HRESULT hb = Fn<PFN_D3D12_SERIALIZE_ROOT_SIGNATURE>(p.system, "D3D12SerializeRootSignature")(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &b, nullptr);
        host.CheckEq(ha, hb, "D3D12SerializeRootSignature result matches");
        host.Check(a && b && a->GetBufferSize() == b->GetBufferSize(), "serialized root signature matches");
        if (a) a->Release();
        if (b) b->Release();
    }
    else if (proxy == L"dxgi.dll")
    {
        if (!GetPair(host, proxy, p)) return;
        using T = HRESULT(WINAPI*)(REFIID, void**);
        IDXGIFactory1 *a = nullptr, *b = nullptr;
        HRESULT ha = Fn<T>(p.proxy, "CreateDXGIFactory1")(__uuidof(IDXGIFactory1), (void**)&a);
        HRESULT hb = Fn<T>(p.system, "CreateDXGIFactory1")(__uuidof(IDXGIFactory1), (void**)&b);
        host.CheckEq(ha, hb, "CreateDXGIFactory1 result matches");
        host.Check(SUCCEEDED(ha) && a, "CreateDXGIFactory1 through the loader");
        if (a) a->Release();
        if (b) b->Release();
    }
    else if (proxy.starts_with(L"xinput"))
    {
        if (!GetPair(host, proxy, p)) return;
        using T = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
        auto fa = Fn<T>(p.proxy, "XInputGetState");
        auto fb = Fn<T>(p.system, "XInputGetState");
        if (!fb) { host.Skip("system DLL does not export XInputGetState"); return; }
        bool same = true;
        for (DWORD i = 0; i < 4; ++i)
        {
            XINPUT_STATE sa{}, sb{};
            same &= fa(i, &sa) == fb(i, &sb);
        }
        host.Check(same, "XInputGetState(0..3) results match the system DLL");
    }
#if !defined(_WIN64)
    else if (proxy == L"d3d8.dll")
    {
        if (!GetPair(host, proxy, p)) return;
        using T = void*(WINAPI*)(UINT);
        void* a = Fn<T>(p.proxy, "Direct3DCreate8")(220);
        void* b = Fn<T>(p.system, "Direct3DCreate8")(220);
        host.CheckEq(a != nullptr, b != nullptr, "Direct3DCreate8 availability matches");
        using GetCount = UINT(WINAPI*)(void*);
        using Release = ULONG(WINAPI*)(void*);
        if (a && b) host.CheckEq(reinterpret_cast<GetCount*>(*(void**)a)[4](a), reinterpret_cast<GetCount*>(*(void**)b)[4](b), "adapter count matches");
        if (a) reinterpret_cast<Release*>(*(void**)a)[2](a);
        if (b) reinterpret_cast<Release*>(*(void**)b)[2](b);
    }
    else if (proxy == L"ddraw.dll")
    {
        if (!GetPair(host, proxy, p)) return;
        using CB = BOOL(WINAPI*)(GUID*, LPSTR, LPSTR, LPVOID);
        using T = HRESULT(WINAPI*)(CB, LPVOID);
        int ca = 0, cb = 0;
        CB counter = [](GUID*, LPSTR, LPSTR, LPVOID ctx) -> BOOL { ++*static_cast<int*>(ctx); return TRUE; };
        HRESULT ha = Fn<T>(p.proxy, "DirectDrawEnumerateA")(counter, &ca);
        HRESULT hb = Fn<T>(p.system, "DirectDrawEnumerateA")(counter, &cb);
        host.CheckEq(ha, hb, "DirectDrawEnumerateA result matches");
        host.CheckEq(ca, cb, "DirectDrawEnumerateA device count matches");
    }
    else if (proxy == L"msacm32.dll")
    {
        if (!GetPair(host, proxy, p)) return;
        using T = DWORD(WINAPI*)();
        host.CheckEq(Fn<T>(p.proxy, "acmGetVersion")(), Fn<T>(p.system, "acmGetVersion")(), "acmGetVersion matches");
    }
    else if (proxy == L"msvfw32.dll")
    {
        if (!GetPair(host, proxy, p)) return;
        using T = DWORD(WINAPI*)();
        host.CheckEq(Fn<T>(p.proxy, "VideoForWindowsVersion")(), Fn<T>(p.system, "VideoForWindowsVersion")(), "VideoForWindowsVersion matches");
    }
#endif
    else
        host.Skip("no forwarding check for " + ualtest::utf8(proxy));
}

// After plugin loading, no exe kernel32 import may still point into the loader
HOST_SCENARIO(iat_restored)
{
    if (!host.RequireUal()) return;
    int kernelSlots = 0;
    std::string offenders;
    ForEachImport(GetModuleHandleW(nullptr), [&](const std::string& dll, const std::string& fn, void** slot) {
        std::string d = dll;
        for (auto& c : d) c = (char)tolower(c);
        if (d != "kernel32.dll" && !d.starts_with("api-ms-win-")) return;
        ++kernelSlots;
        if (AddressInModule(*slot, host.ual.module))
            offenders += fn + " ";
    });
    host.Check(kernelSlots > 0, "the host imports kernel32 functions");
    host.Check(offenders.empty(), "no kernel32 import of the executable points into the loader after plugin loading", offenders);
}

// CoCreateInstance is redirected so DirectInput/DirectSound/WinInet objects come from a
// local wrapper DLL. They must still work. "hook=1" expects the IAT slot in the loader.
HOST_SCENARIO(cocreate)
{
    if (!host.RequireUal()) return;
    bool expectHook = args.find(L"hook=1") != std::wstring::npos;
    bool found = false, hooked = false;
    ForEachImport(GetModuleHandleW(nullptr), [&](const std::string&, const std::string& fn, void** slot) {
        if (fn == "CoCreateInstance") { found = true; hooked = AddressInModule(*slot, host.ual.module); }
    });
    host.Check(found, "the host imports CoCreateInstance");
    host.CheckEq(hooked, expectHook, "CoCreateInstance IAT slot redirection");

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    IDirectInput8W* di = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_DirectInput8, nullptr, CLSCTX_INPROC_SERVER, IID_IDirectInput8W, (void**)&di);
    host.Check(SUCCEEDED(hr) && di, "CoCreateInstance(CLSID_DirectInput8) succeeds", "hr=" + std::to_string(hr));
    if (di)
    {
        host.Check(SUCCEEDED(di->Initialize(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION)), "the DirectInput8 object is usable");
        di->Release();
    }
    CoUninitialize();
}

// --- overload query exports

// GetOverloadPath*/GetOverloadedFilePath* write the NUL past out_size when the result doesn't fit
HOST_SCENARIO(ovr_export_overflow)
{
    if (!host.RequireUal()) return;
    constexpr size_t kSize = 4;
    {
        wchar_t buf[16];
        std::fill(std::begin(buf), std::end(buf), L'#');
        bool ok = host.ual.GetOverloadPathW(buf, kSize);
        host.Check(buf[kSize] == L'#', "GetOverloadPathW does not write past out_size", "ok=" + std::to_string(ok) + " buf[out_size]=" + std::to_string((int)buf[kSize]));
    }
    {
        char buf[16];
        std::fill(std::begin(buf), std::end(buf), '#');
        bool ok = host.ual.GetOverloadPathA(buf, kSize);
        host.Check(buf[kSize] == '#', "GetOverloadPathA does not write past out_size", "ok=" + std::to_string(ok) + " buf[out_size]=" + std::to_string((int)buf[kSize]));
    }
    {
        wchar_t buf[16];
        std::fill(std::begin(buf), std::end(buf), L'#');
        bool ok = host.ual.GetOverloadedFilePathW(L"ov.txt", buf, kSize);
        host.Check(buf[kSize] == L'#', "GetOverloadedFilePathW does not write past out_size", "ok=" + std::to_string(ok) + " buf[out_size]=" + std::to_string((int)buf[kSize]));
    }
    {
        char buf[16];
        std::fill(std::begin(buf), std::end(buf), '#');
        bool ok = host.ual.GetOverloadedFilePathA("ov.txt", buf, kSize);
        host.Check(buf[kSize] == '#', "GetOverloadedFilePathA does not write past out_size", "ok=" + std::to_string(ok) + " buf[out_size]=" + std::to_string((int)buf[kSize]));
    }
}

// for relative input GetOverloadedFilePathW returns a path relative to the
// game directory instead of the caller's current directory.
HOST_SCENARIO(ovr_relative_cwd)
{
    if (!host.RequireUal()) return;
    auto sub = ExeDir() + L"\\sub";
    host.Check(SetCurrentDirectoryW(sub.c_str()) != FALSE, "change the current directory to <game>\\sub");
    std::wstring out(1024, L'\0');
    bool ok = host.ual.GetOverloadedFilePathW(L"data.txt", out.data(), out.size());
    out.resize(wcslen(out.c_str()));
    host.Check(ok, "sub\\data.txt has an overload");
    DWORD err = 0;
    auto content = ReadFileW(out, &err);
    host.CheckEq(content, std::string("UPDATE-SUB"), "the returned path can be opened from the caller's current directory (returned \"" + ualtest::utf8(out) + "\")");
    SetCurrentDirectoryW(ExeDir().c_str());
}

// --- x86 features

HOST_SCENARIO(d3d8to9)
{
#if defined(_WIN64)
    host.Skip("d3d8 is 32-bit only");
#else
    if (!host.RequireUal()) return;
    bool expectOn = args == L"on";
    if (!LoadLibraryW((SystemDir() + L"\\d3d8.dll").c_str())) { host.Skip("system d3d8.dll not available"); return; }
    auto create = Fn<void*(WINAPI*)(UINT)>(host.ual.module, "Direct3DCreate8");
    void* d3d = create ? create(220) : nullptr;
    if (!d3d) { host.Skip("Direct3DCreate8 returned NULL on this machine"); return; }
    void* vtbl = *(void**)d3d;
    bool fromLoader = AddressInModule(vtbl, host.ual.module);
    host.CheckEq(fromLoader, expectOn, expectOn ? "UseD3D8to9=1: Direct3DCreate8 returns the built-in d3d8to9 implementation"
                                                : "UseD3D8to9=0: Direct3DCreate8 returns the system implementation");
    reinterpret_cast<ULONG(WINAPI**)(void*)>(vtbl)[2](d3d);
#endif
}

HOST_SCENARIO(exe_text_writable)
{
    bool expect = args == L"1";
    void* text = SectionAddress(GetModuleHandleW(nullptr), ".text");
    host.Check(text != nullptr, "the host has a .text section");
    host.CheckEq(IsWritable(text), expect, expect ? ".text of the executable was made writable" : ".text of the executable is not writable");
}

// vorbisFile.dll with no local original uses the built-in vorbisfile
HOST_SCENARIO(vorbisfile_builtin)
{
    if (!host.RequireUal()) return;
    struct Source
    {
        const char* data;
        size_t size, pos;
    };
    struct Callbacks
    {
        size_t(__cdecl* read)(void*, size_t, size_t, void*);
        int(__cdecl* seek)(void*, long long, int);
        int(__cdecl* close)(void*);
        long(__cdecl* tell)(void*);
    };
    Callbacks cb{
        [](void* p, size_t size, size_t n, void* src) -> size_t {
            auto s = (Source*)src;
            size_t bytes = (std::min)(size * n, s->size - s->pos);
            memcpy(p, s->data + s->pos, bytes);
            s->pos += bytes;
            return size ? bytes / size : 0;
        },
        nullptr, nullptr, nullptr, // not seekable
    };
    using TestCallbacks = int(__cdecl*)(void*, void*, const char*, long, Callbacks);
    using Clear = int(__cdecl*)(void*);
    auto test = Fn<TestCallbacks>(host.ual.module, "ov_test_callbacks");
    auto clear = Fn<Clear>(host.ual.module, "ov_clear");
    if (!host.Check(test && clear, "ov_test_callbacks and ov_clear are exported")) return;
    static const char garbage[8192] = "this is not an ogg vorbis stream";
    Source src{ garbage, sizeof(garbage), 0 };
    alignas(16) static unsigned char vf[16384] = {}; // opaque OggVorbis_File
    int r = test(&src, vf, nullptr, 0, cb);
    host.CheckEq(r, -132, "ov_test_callbacks rejects data that is not Ogg Vorbis (OV_ENOTVORBIS)");
    clear(vf);
}

// A local <name>Hooked.dll takes precedence over System32
HOST_SCENARIO(hooked_sentinel)
{
    if (!host.RequireUal()) return;
    std::wstring proxy = Lower(args);
    if (proxy == L"dinput8.dll")
    {
        void* obj = (void*)1;
        HRESULT hr = Fn<HRESULT(WINAPI*)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN)>(host.ual.module, "DirectInput8Create")(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W, &obj, nullptr);
        host.CheckEq((long)hr, ualtest::kFakeDirectInput8CreateResult, "DirectInput8Create is forwarded to dinput8Hooked.dll");
    }
    else if (proxy == L"version.dll")
    {
        DWORD h = 0;
        DWORD r = Fn<DWORD(WINAPI*)(LPCWSTR, LPDWORD)>(host.ual.module, "GetFileVersionInfoSizeW")(L"x", &h);
        host.CheckEq(r, ualtest::kFakeFileVersionInfoSize, "GetFileVersionInfoSizeW is forwarded to versionHooked.dll");
    }
    else if (proxy == L"winmm.dll")
    {
        DWORD r = Fn<DWORD(WINAPI*)()>(host.ual.module, "timeGetTime")();
        host.CheckEq(r, ualtest::kFakeTimeGetTime, "timeGetTime is forwarded to winmmHooked.dll");
    }
    else if (proxy == L"vorbisfile.dll")
    {
        auto f = Fn<long(__cdecl*)(void*)>(host.ual.module, "ov_streams");
        long r = f ? f(nullptr) : -1;
        host.CheckEq(r, ualtest::kFakeOvStreams, "ov_streams is forwarded to the local original vorbisfile");
    }
    else
        host.Skip("no sentinel for " + ualtest::utf8(proxy));
}

// --- zip packages

// AddVirtualFileForOverload returns true but the zip package's file still wins
HOST_SCENARIO(zip_vf_override)
{
    if (!host.RequireUal()) return;
    host.CheckEq(ReadFileW(L"zipfile.txt"), std::string("FROMZIP"), "precondition: zipfile.txt is served from the zip package");
    std::string data = "OVERRIDE";
    bool ok = host.ual.AddVirtualFileForOverloadW(L"zipfile.txt", (const uint8_t*)data.data(), data.size(), 100000);
    host.Check(ok, "AddVirtualFileForOverloadW with a very high priority returns true");
    host.CheckEq(ReadFileW(L"zipfile.txt"), data, "the higher priority virtual file replaces the zip-backed file");
}

// the first read of a zip-backed file decompresses it into the variant storage
// without locking, so concurrent first reads race.
HOST_SCENARIO(zip_concurrent)
{
    if (!host.RequireUal()) return;
    std::vector<std::string> expected;
    for (int i = 0; i < ualtest::kZipConcurrentFiles; ++i)
        expected.push_back(ualtest::Pattern(ualtest::kZipConcurrentFileSize, 500 + i));

    constexpr int kThreads = 8;
    std::atomic<int> ready{ 0 }, bad{ 0 };
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t)
        threads.emplace_back([&, t] {
            ++ready;
            while (ready < kThreads) YieldProcessor();
            for (int k = 0; k < ualtest::kZipConcurrentFiles; ++k)
            {
                int i = (k + t) % ualtest::kZipConcurrentFiles;
                if (ReadFileW(L"zc_" + std::to_wstring(i) + L".bin") != expected[i]) ++bad;
            }
        });
    for (auto& t : threads) t.join();
    host.CheckEq(bad.load(), 0, "concurrent first reads of zip-backed files return correct data");
}

// --- virtual file server (x86)

// the x86 loader starts "<proxy>.exe" (VirtualFileServer) to keep virtual files out of
// its address space, but the server exits at once, looking for a mutex without the client's PID suffix.
HOST_SCENARIO(vfs_server)
{
#if defined(_WIN64)
    host.Skip("the virtual file server is only used by 32-bit processes");
#else
    if (!host.RequireUal()) return;
    // One pipe per game process, its only instance is connected to the loader
    std::wstring pipeName = L"\\\\.\\pipe\\Ultimate-ASI-Loader-VirtualFileServer-" + std::to_wstring(GetCurrentProcessId());
    BOOL pipe = WaitNamedPipeW(pipeName.c_str(), 0);
    DWORD pipeErr = GetLastError();
    bool pipeExists = pipe || pipeErr == ERROR_SEM_TIMEOUT;

    // Started as "<loader name>.exe <pid>", args holds that file name
    int children = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    PROCESSENTRY32W pe{ sizeof(pe) };
    if (snap != INVALID_HANDLE_VALUE && Process32FirstW(snap, &pe))
        do
            if (pe.th32ParentProcessID == GetCurrentProcessId() && _wcsicmp(pe.szExeFile, args.c_str()) == 0) ++children;
        while (Process32NextW(snap, &pe));
    if (snap != INVALID_HANDLE_VALUE) CloseHandle(snap);

    host.Check(children > 0, "the virtual file server child process (" + ualtest::utf8(args) + ") is running");
    host.Check(pipeExists, "the virtual file server pipe exists", "WaitNamedPipe error " + std::to_string(pipeErr));

    std::string data = "served";
    host.ual.AddVirtualFileForOverloadW(L"vfs_server.bin", (const uint8_t*)data.data(), data.size(), 1000);
    host.CheckEq(ReadFileW(L"vfs_server.bin"), data, "virtual files work (server or local fallback)");
#endif
}
