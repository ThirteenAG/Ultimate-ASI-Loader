# .cxx snippet examples

A `.cxx` file next to your `.asi` plugins is compiled and run by Ultimate ASI Loader when the game starts. You don't need Visual Studio or a build step: edit the file, start the game. With `CxxHotReload=1` you don't even need to restart.

Each example covers one task and explains itself in its comments. Find what you want to do below, open the file, and copy the part you need.

> The examples patch made-up addresses and patterns. Don't drop them into a game folder as they are.

## What do you want to do?

| I want to... | Example |
|---|---|
| See what a snippet looks like, and when `Init`/`Shutdown` run | [01_basics.cxx](01_basics.cxx) |
| Change a value: FOV, aspect ratio, a limit, a float constant | [02_memory_patches.cxx](02_memory_patches.cxx) |
| Flip a jump (`jz` → `jmp`), NOP an instruction, write a few bytes | [02_memory_patches.cxx](02_memory_patches.cxx) |
| Convert an address from IDA/Ghidra so it works with ASLR | [02_memory_patches.cxx](02_memory_patches.cxx) |
| Find code by bytes, so it works across game versions | [03_patterns.cxx](03_patterns.cxx) |
| Search a game DLL instead of the exe (`GetModuleHandle` + pattern) | [03_patterns.cxx](03_patterns.cxx) |
| Patch every match, or the Nth match, of a pattern | [03_patterns.cxx](03_patterns.cxx) |
| Follow a `call`/`jmp` to its function, or an instruction to a global variable | [03_patterns.cxx](03_patterns.cxx) |
| Change a function's arguments or result | [04_inline_hooks.cxx](04_inline_hooks.cxx) |
| Replace a function completely (never call the original) | [04_inline_hooks.cxx](04_inline_hooks.cxx) |
| Hook a `__thiscall` / `__stdcall` / `__fastcall` function on x86 | [04_inline_hooks.cxx](04_inline_hooks.cxx) |
| Change a register (or an SSE float) in the middle of a function | [05_mid_hooks.cxx](05_mid_hooks.cxx) |
| Grab a pointer to a game object from a register | [05_mid_hooks.cxx](05_mid_hooks.cxx) |
| Hook a function a game DLL exports (Unreal `Engine.dll`, `D3DDrv.dll`, ...) | [06_dll_and_winapi_hooks.cxx](06_dll_and_winapi_hooks.cxx) |
| Hook a Windows function (screen size, cursor clipping, window style) | [06_dll_and_winapi_hooks.cxx](06_dll_and_winapi_hooks.cxx) |
| Call a Windows function the built-in `<windows.h>` doesn't have | [06_dll_and_winapi_hooks.cxx](06_dll_and_winapi_hooks.cxx), [11_settings_ini.cxx](11_settings_ini.cxx) |
| Change what ONE call site does, leaving other callers alone | [07_call_site_redirect.cxx](07_call_site_redirect.cxx) |
| Skip a call (intro videos, checks) | [07_call_site_redirect.cxx](07_call_site_redirect.cxx) |
| Hook a Direct3D/COM method (`EndScene`, `Present`, `Reset`) or a virtual method | [08_virtual_methods.cxx](08_virtual_methods.cxx) |
| Read/write game structs, follow pointer chains | [09_game_objects.cxx](09_game_objects.cxx) |
| Call a game function yourself | [09_game_objects.cxx](09_game_objects.cxx) |
| Deal with a struct passed by value | [09_game_objects.cxx](09_game_objects.cxx) |
| Patch a DLL that the game loads later | [10_dll_loaded_later.cxx](10_dll_loaded_later.cxx) |
| Read settings from an `.ini` next to the snippet | [11_settings_ini.cxx](11_settings_ini.cxx) |
| Wait for the game to create something; toggle with a hotkey | [12_threads_and_hotkeys.cxx](12_threads_and_hotkeys.cxx) |
| Edit the snippet while the game is running | [13_hot_reload.cxx](13_hot_reload.cxx) |

## What you have

- **Headers:**
  - `<injector/injector.hpp>`: `WriteMemory`, `ReadMemory`, `WriteMemoryRaw`, `MakeNOP`, `MakeJMP`, `MakeCALL`.
  - `<Hooking.Patterns.h>`: `hook::pattern`, `module_pattern`, `get_pattern`.
  - `<safetyhook.hpp>`: inline, mid, VMT and VM hooks, plus calling the original in any calling convention.
  - A small `<windows.h>`, `<cstdint>`, `<cstddef>`, `<cstdio>` (`printf`, `snprintf`) and `<cstring>`.
- **Any exported function of any loaded DLL:** declare it in `extern "C"` and call it (see 06). For a DLL that may not be loaded yet, use `LoadLibrary` + `GetProcAddress`.
- **The C++ you need for this kind of code:**
  - functions, globals, statics, pointers, references, arrays;
  - structs with in-class methods, enums, namespaces;
  - captureless lambdas, `static_assert`, `offsetof`;
  - all control flow, floats and doubles, 64-bit integers;
  - calling conventions `__cdecl`, `__stdcall`, `__fastcall`, `__thiscall`.
- **Safety:**
  - A compile error shows the file and line, and the game still starts.
  - A crash in snippet code is logged with the line that crashed, and crash reports name the snippet.
  - Unloading undoes hooks and `injector::` writes.

## What you don't have (and what to do instead)

| Not available | Instead |
|---|---|
| STL (`std::string`, `std::vector`, `std::map`, `std::format`, `std::filesystem`) | Fixed-size arrays, `snprintf`, `strlen`/`memcpy`, and the Windows API (`GetPrivateProfile*` for ini files, `CreateFile`, ...) |
| Classes with virtual functions, inheritance, constructors/destructors, operator overloading | Plain structs describing the game's memory, and free functions as hook targets |
| Templates you write yourself | Write the function for the type you need |
| Lambdas that capture variables | Captureless lambdas reading globals |
| Structs passed or returned by value in your functions | Pass a pointer, or split the struct into its fields (see 09) |
| Defining variadic functions (`void Log(const char*, ...)`) | Calling them works (`printf`, `wsprintfA`); format with `snprintf` first |
| `new` / `delete`, exceptions | Globals and arrays; `VirtualAlloc`/`HeapAlloc` if you must; check return values |
| `atof`, `sscanf`, most of the C library | Small helpers (see `ParseFloat` in 11), or a function a loaded DLL exports |
| Global hook objects destroyed at game exit | Nothing runs at exit; `Shutdown` is for unloading (hot reload) |

If a snippet grows past this, build it as a normal `.asi`. A snippet is already valid C++ for Visual Studio: the real headers have the same names. Wrap the extra declarations a snippet needs in `#ifdef __CXXSNIPPETS__`, and call `Init()` from `InitializeASI` (or the `DllMain` fallback pattern shown in the readme's WRITING PLUGINS section; see 01).

All the examples are compiled by the test suite on x86 and x64, both by the snippet engine and by MSVC.
