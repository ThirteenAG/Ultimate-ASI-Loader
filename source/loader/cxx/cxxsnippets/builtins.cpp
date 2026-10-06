#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "builtins.hpp"
#include <windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include "Hooking.Patterns.h"

namespace cxxsnippets
{
void AddBindingHeaders(std::map<std::string, std::string> &h);
void *HostSymbol(const std::string &name);
std::map<std::string, std::string> HeaderSources()
{
    std::map<std::string, std::string> h;
    h["cstdint"] = R"(#pragma once
typedef signed char int8_t; typedef unsigned char uint8_t;
typedef short int16_t; typedef unsigned short uint16_t;
typedef int int32_t; typedef unsigned int uint32_t;
typedef long long int64_t; typedef unsigned long long uint64_t;
#ifdef _WIN64
typedef long long intptr_t; typedef unsigned long long uintptr_t;
#else
typedef int intptr_t; typedef unsigned int uintptr_t;
#endif
namespace std { using int8_t=::int8_t; using uint8_t=::uint8_t; using int16_t=::int16_t; using uint16_t=::uint16_t; using int32_t=::int32_t; using uint32_t=::uint32_t; using int64_t=::int64_t; using uint64_t=::uint64_t; using intptr_t=::intptr_t; using uintptr_t=::uintptr_t; }
#define INT8_MAX 127
#define UINT8_MAX 255
#define INT16_MAX 32767
#define UINT16_MAX 65535
#define INT32_MAX 2147483647
#define UINT32_MAX 4294967295U
#define INT64_MAX 9223372036854775807LL
#define UINT64_MAX 18446744073709551615ULL
)";
    h["stdint.h"] = h["cstdint"];
    h["cstddef"] = R"(#pragma once
#ifdef _WIN64
typedef unsigned long long size_t; typedef long long ptrdiff_t;
#else
typedef unsigned int size_t; typedef int ptrdiff_t;
#endif
#define NULL 0
namespace std { using size_t=::size_t; using ptrdiff_t=::ptrdiff_t; }
)";
    h["stddef.h"] = h["cstddef"];
    h["cstdio"] = R"(#pragma once
#include <cstddef>
extern "C" { int printf(char const* format,...); int puts(char const* text); int putchar(int c); int snprintf(char* buffer,size_t count,char const* format,...); }
namespace std { [[cxxsnippets::link("printf")]] int printf(char const* format,...); [[cxxsnippets::link("puts")]] int puts(char const* text); [[cxxsnippets::link("snprintf")]] int snprintf(char* buffer,size_t count,char const* format,...); }
)";
    h["stdio.h"] = h["cstdio"];
    h["cstring"] = R"(#pragma once
#include <cstddef>
extern "C" { void* memcpy(void* dst,void const* src,size_t count); void* memmove(void* dst,void const* src,size_t count); void* memset(void* dst,int value,size_t count); int memcmp(void const* a,void const* b,size_t count); size_t strlen(char const* text); int strcmp(char const* a,char const* b); }
namespace std { [[cxxsnippets::link("memcpy")]] void* memcpy(void* dst,void const* src,size_t count); [[cxxsnippets::link("memset")]] void* memset(void* dst,int value,size_t count); [[cxxsnippets::link("strlen")]] size_t strlen(char const* text); }
)";
    h["string.h"] = h["cstring"];
    h["windows.h"] = R"(#pragma once
#include <cstdint>
#include <cstddef>
#define WINAPI __stdcall
#define CALLBACK __stdcall
#define APIENTRY __stdcall
#define TRUE 1
#define FALSE 0
#define MAX_PATH 260
#define PAGE_EXECUTE_READWRITE 64
#define PAGE_READWRITE 4
#define PAGE_EXECUTE_READ 32
#define MB_OK 0
#define PM_REMOVE 1
#define PM_NOREMOVE 0
typedef void* HANDLE; typedef void* HMODULE; typedef void* HINSTANCE; typedef void* HWND; typedef void* LPVOID; typedef void const* LPCVOID;
typedef unsigned long DWORD; typedef int BOOL; typedef unsigned char BYTE; typedef unsigned short WORD; typedef unsigned int UINT; typedef long LONG; typedef size_t SIZE_T;
typedef uintptr_t WPARAM; typedef intptr_t LPARAM; typedef intptr_t LRESULT; typedef char const* LPCSTR; typedef wchar_t const* LPCWSTR;
typedef void (__stdcall *FARPROC)();
typedef DWORD (__stdcall *LPTHREAD_START_ROUTINE)(void*);
struct POINT { LONG x; LONG y; };
struct MSG { HWND hwnd; UINT message; WPARAM wParam; LPARAM lParam; DWORD time; POINT pt; DWORD lPrivate; };
typedef MSG* LPMSG; typedef DWORD* LPDWORD; typedef BYTE* LPBYTE; typedef char* LPSTR; typedef wchar_t* LPWSTR;
#define DLL_PROCESS_ATTACH 1
#define DLL_PROCESS_DETACH 0
#define DLL_THREAD_ATTACH 2
#define DLL_THREAD_DETACH 3
#define INFINITE 4294967295U
extern "C" {
HMODULE WINAPI GetModuleHandleA(char const* name); HMODULE WINAPI GetModuleHandleW(wchar_t const* name);
FARPROC WINAPI GetProcAddress(HMODULE module,char const* name);
void WINAPI OutputDebugStringA(char const* text); void WINAPI OutputDebugStringW(wchar_t const* text);
void WINAPI Sleep(DWORD ms); BOOL WINAPI VirtualProtect(void* address,size_t size,DWORD protection,DWORD* previous);
int WINAPI MessageBoxA(HWND window,char const* text,char const* caption,UINT flags); int WINAPI MessageBoxW(HWND window,wchar_t const* text,wchar_t const* caption,UINT flags);
BOOL WINAPI PeekMessageA(MSG* message,HWND window,UINT first,UINT last,UINT flags); BOOL WINAPI PeekMessageW(MSG* message,HWND window,UINT first,UINT last,UINT flags);
HANDLE WINAPI CreateThread(void* attributes,size_t stack,LPTHREAD_START_ROUTINE entry,void* arg,DWORD flags,DWORD* id);
BOOL WINAPI CloseHandle(HANDLE object); DWORD WINAPI WaitForSingleObject(HANDLE object,DWORD timeout); DWORD WINAPI GetLastError();
}
#ifdef UNICODE
#define GetModuleHandle GetModuleHandleW
#define OutputDebugString OutputDebugStringW
#define MessageBox MessageBoxW
#define PeekMessage PeekMessageW
#else
#define GetModuleHandle GetModuleHandleA
#define OutputDebugString OutputDebugStringA
#define MessageBox MessageBoxA
#define PeekMessage PeekMessageA
#endif
)";
    AddBindingHeaders(h);
    return h;
}
void *BuiltinSymbol(const std::string &name)
{
    static const std::map<std::string, void *> symbols = {{"printf", (void *)std::printf},
                                                          {"snprintf", (void *)std::snprintf},
                                                          {"puts", (void *)std::puts},
                                                          {"putchar", (void *)std::putchar},
                                                          {"memcpy", (void *)std::memcpy},
                                                          {"memmove", (void *)std::memmove},
                                                          {"memset", (void *)std::memset},
                                                          {"memcmp", (void *)std::memcmp},
                                                          {"strlen", (void *)std::strlen},
                                                          {"strcmp", (void *)std::strcmp},
                                                          {"GetModuleHandleA", (void *)GetModuleHandleA},
                                                          {"GetModuleHandleW", (void *)GetModuleHandleW},
                                                          {"GetProcAddress", (void *)GetProcAddress},
                                                          {"OutputDebugStringA", (void *)OutputDebugStringA},
                                                          {"OutputDebugStringW", (void *)OutputDebugStringW},
                                                          {"Sleep", (void *)Sleep},
                                                          {"VirtualProtect", (void *)VirtualProtect},
                                                          {"MessageBoxA", (void *)MessageBoxA},
                                                          {"MessageBoxW", (void *)MessageBoxW},
                                                          {"PeekMessageA", (void *)PeekMessageA},
                                                          {"PeekMessageW", (void *)PeekMessageW},
                                                          {"CreateThread", (void *)CreateThread},
                                                          {"CloseHandle", (void *)CloseHandle},
                                                          {"WaitForSingleObject", (void *)WaitForSingleObject},
                                                          {"GetLastError", (void *)GetLastError}};
    auto it = symbols.find(name);
    return it == symbols.end() ? HostSymbol(name) : it->second;
}
} // namespace cxxsnippets
