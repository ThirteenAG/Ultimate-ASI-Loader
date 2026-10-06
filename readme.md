[![AppVeyor](https://img.shields.io/appveyor/build/ThirteenAG/Ultimate-ASI-Loader?label=AppVeyor%20Build&logo=Appveyor&logoColor=white)](https://ci.appveyor.com/project/ThirteenAG/ultimate-asi-loader)
[![GitHub Actions Build](https://github.com/ThirteenAG/Ultimate-ASI-Loader/actions/workflows/msbuild.yml/badge.svg)](https://github.com/ThirteenAG/Ultimate-ASI-Loader/actions/workflows/msbuild.yml)

<p align="center">
  <a href="https://github.com/ThirteenAG/Ultimate-ASI-Loader" target="_blank"><img width="400" src="https://raw.githubusercontent.com/ThirteenAG/Ultimate-ASI-Loader/refs/heads/master/source/loader/resources/logo.svg"></a>
  <br />
  <a href="https://patreon.fusionfix.io/" target="_blank"><picture><source media="(max-width: 768px) and (prefers-color-scheme: dark)" srcset="https://fusionlegacyinitiative.com/sponsors-progress/sponsors-progress-ual-mobile-dark.svg"><source media="(max-width: 768px)" srcset="https://fusionlegacyinitiative.com/sponsors-progress/sponsors-progress-ual-mobile.svg"><source media="(prefers-color-scheme: dark)" srcset="https://fusionlegacyinitiative.com/sponsors-progress/sponsors-progress-ual-dark.svg"><img width="100%" src="https://fusionlegacyinitiative.com/sponsors-progress/sponsors-progress-ual.svg"></picture></a>
  <br />
  <a href="https://github.com/sponsors/ThirteenAG"><picture><source media="(prefers-color-scheme: dark)" srcset="https://thirteenag.github.io/img/buttons/github-dark.svg"><img src="https://thirteenag.github.io/img/buttons/github.svg" width="250"></picture></a>
  <a href="https://ko-fi.com/thirteenag"><picture><source media="(prefers-color-scheme: dark)" srcset="https://thirteenag.github.io/img/buttons/kofi-dark.svg"><img src="https://thirteenag.github.io/img/buttons/kofi.svg" width="250"></picture></a>
  <a href="https://paypal.me/SergeyP13"><picture><source media="(prefers-color-scheme: dark)" srcset="https://thirteenag.github.io/img/buttons/paypal-dark.svg"><img src="https://thirteenag.github.io/img/buttons/paypal.svg" width="250"></picture></a>
  <a href="https://www.patreon.com/ThirteenAG"><picture><source media="(prefers-color-scheme: dark)" srcset="https://thirteenag.github.io/img/buttons/patreon-dark.svg"><img src="https://thirteenag.github.io/img/buttons/patreon.svg" width="250"></picture></a>
  <a href="https://boosty.to/thirteenag"><picture><source media="(prefers-color-scheme: dark)" srcset="https://thirteenag.github.io/img/buttons/boosty-dark.svg"><img src="https://thirteenag.github.io/img/buttons/boosty.svg" width="250"></picture></a><br><br>
  <a href="https://discord.gg/2ckFCS572Z" target="_blank"><img width="50" src="https://raw.githubusercontent.com/ThirteenAG/GTAIV.EFLC.FusionFix/refs/heads/master/installer/discord.svg"></a>
  &nbsp;&nbsp;&nbsp;
  <a href="https://t.me/fusionfix" target="_blank"><img width="50" src="https://raw.githubusercontent.com/ThirteenAG/GTAIV.EFLC.FusionFix/refs/heads/master/installer/telegram.svg"></a>
  &nbsp;&nbsp;&nbsp;
  <a href="https://www.youtube.com/@FusionFix10" target="_blank"><img width="50" src="https://raw.githubusercontent.com/ThirteenAG/GTAIV.EFLC.FusionFix/refs/heads/master/installer/youtube.svg"></a>
  &nbsp;&nbsp;&nbsp;
  <a href="https://x.com/fusionfix10" target="_blank"><img width="50" src="https://raw.githubusercontent.com/ThirteenAG/GTAIV.EFLC.FusionFix/refs/heads/master/installer/x.svg"></a>
  &nbsp;&nbsp;&nbsp;
</p>

# Ultimate ASI Loader

## DESCRIPTION

This is a DLL file that adds ASI plugin loading functionality to any game that uses any of the following libraries:

|                                                          Win32                                                            |                                                       Win64                                                           |
| :-----------------------------------------------------------------------------------------------------------------------: | :-------------------------------------------------------------------------------------------------------------------: |
| [d3d8.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/d3d8-Win32.zip)               |                                                         -                                                             |
| [d3d9.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/d3d9-Win32.zip)               |     [d3d9.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/d3d9-x64.zip)           |
| [d3d10.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/d3d10-Win32.zip)             |    [d3d10.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/d3d10-x64.zip)          |
| [d3d11.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/d3d11-Win32.zip)             |    [d3d11.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/d3d11-x64.zip)          |
| [d3d12.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/d3d12-Win32.zip)             |    [d3d12.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/d3d12-x64.zip)          |
| [dxgi.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/dxgi-Win32.zip)               |     [dxgi.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/dxgi-x64.zip)           |
| [ddraw.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/ddraw-Win32.zip)             |                                                         -                                                             |
| [dinput.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/dinput-Win32.zip)           |                                                         -                                                             |
| [dinput8.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/dinput8-Win32.zip)         |   [dinput8.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/dinput8-x64.zip)       |
| [dsound.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/dsound-Win32.zip)           |    [dsound.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/dsound-x64.zip)        |
| [msacm32.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/msacm32-Win32.zip)         |                                                         -                                                             |
| [msvfw32.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/msvfw32-Win32.zip)         |                                                         -                                                             |
| [version.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/version-Win32.zip)         |   [version.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/version-x64.zip)       |
| [wininet.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/wininet-Win32.zip)         |   [wininet.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/wininet-x64.zip)       |
| [winmm.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/winmm-Win32.zip)             |     [winmm.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/winmm-x64.zip)         |
| [winhttp.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/winhttp-Win32.zip)         |   [winhttp.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/winhttp-x64.zip)       |
| [xlive.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/xlive-Win32.zip)             |                                                         -                                                             |
| [binkw32.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/binkw32-Win32.zip)         |                                                         -                                                             |
| [bink2w32.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/bink2w32-Win32.zip)       |                                                         -                                                             |
|                                                             -                                                             |   [binkw64.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/binkw64-x64.zip)       |
|                                                             -                                                             |  [bink2w64.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/bink2w64-x64.zip)      |
| [vorbisFile.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/vorbisFile-Win32.zip)   |                                                         -                                                             |
| [xinput1_1.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/xinput1_1-Win32.zip)     |  [xinput1_1.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/xinput1_1-x64.zip)    |
| [xinput1_2.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/xinput1_2-Win32.zip)     |  [xinput1_2.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/xinput1_2-x64.zip)    |
| [xinput1_3.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/xinput1_3-Win32.zip)     |  [xinput1_3.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/xinput1_3-x64.zip)    |
| [xinput1_4.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/xinput1_4-Win32.zip)     |  [xinput1_4.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/xinput1_4-x64.zip)    |
| [xinput9_1_0.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/xinput9_1_0-Win32.zip) | [xinput9_1_0.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/xinput9_1_0-x64.zip) |
| [xinputuap.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/xinputuap-Win32.zip)     |  [xinputuap.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/xinputuap-x64.zip)    |

It is possible (and sometimes necessary) to load the original DLL by renaming it to `<dllname>Hooked.dll`, e.g. `d3d12Hooked.dll`.
The bink proxies (**binkw32.dll**, **bink2w32.dll**, **binkw64.dll**, **bink2w64.dll**) need the original: rename the game's `binkw32.dll` to `binkw32Hooked.dll` and put the loader in its place. **vorbisFile.dll** can replace the original outright. Back up any file before replacing it.
The original **vorbisFile.dll** can be kept as `vorbisFileHooked.dll` or `vorbisHooked.dll`. Without it, the loader's built-in vorbisfile (built from the official libvorbis 1.3.7 sources) is used, decoding with the game's `vorbis.dll` when the game has one.


## INSTALLATION

To install it, you just need to place the DLL into the game directory. Usually, it works as dinput8.dll, but if that doesn't work, you can rename it (see the list of supported names above).

## USAGE

Put ASI files in the game's root directory or in the `scripts`, `plugins`, or `update` folder.
If configuration is necessary, the global.ini file can be placed next to the loader or in its `scripts`, `plugins` or `update` folder. It can be used alongside the chosen DLL, and if so, it is also possible to use the DLL name for the ini file (e.g., version.dll/version.ini). When several of these files exist, the later ones in that order win.
[See an example of global.ini here](https://github.com/ThirteenAG/Ultimate-ASI-Loader/blob/master/data/plugins/global.ini).

Plugins are loaded when the game's own code starts running (`[GlobalSets] DontLoadFromDllMain=1`, the default), also for games wrapped in a protection stub (e.g. Origin) that unpacks the game and loads its DLLs itself. `DontLoadFromDllMain=0` loads them as soon as the loader itself is loaded instead. If plugins do not load, or load at the wrong moment, `[GlobalSets] DebugLog=1` writes `<loader name>.log` next to the loader: what it patched, which calls it ignored and why, what started plugin loading and the plugins it loaded.

All `[GlobalSets]` options and their defaults:

| Option | Default | |
|---|---|---|
| `LoadPlugins` | `1` | `0` disables plugin loading entirely |
| `LoadFromScriptsOnly` | `0` | `1` skips the loader's own folder, plugins load only from `scripts`, `plugins` and update folders |
| `LoadRecursively` | `1` | also loads plugins from the direct sub folders of `scripts`, `plugins` and update folders |
| `LoadExtraPlugins` | `modloader\modloader.asi` | plugins loaded first, relative to the loader, separated by `\|` |
| `DontLoadFromDllMain` | `1` | see above |
| `LoadFromAPI` | | `[module.]Function`: only this call starts plugin loading. Set to `GetSystemTimeAsFileTime` by itself for GTA V and RDR2 |
| `UseD3D8to9` | `0` | see [d3d8to9](#d3d8to9) |
| `Direct3D8DisableMaximizedWindowedModeShim` | `0` | 32-bit: turns off the compatibility shim Windows applies to Direct3D 8 games |
| `ModernUI` | `1` | `0` uses standard Windows task dialogs |
| `DebugLog` | `0` | see above |
| `CxxHotReload` | `0` | see [C++ snippets](#c-snippets-cxx) |
| `DisableCrashDumps`, `CrashDumpZip`, `CrashDumpFullMemory`, `CrashDumpMaxReports` | | see [CrashDumps](#crashdumps) |

The `[FileLoader]` section has `OverloadFromFolder` (default `update`), see [update folder](#update-folder-overload-from-folder).

## WRITING PLUGINS

An ASI plugin is a DLL renamed to `.asi`. The loader loads it with `LoadLibrary`, then calls its exported `InitializeASI` function. Do the plugin's work there, not in `DllMain`: `DllMain` runs under the Windows loader lock, where showing a window, waiting for a thread, loading DLLs or using COM can deadlock the game. `InitializeASI` runs right after `LoadLibrary`, outside the lock (with the default `DontLoadFromDllMain=1`; with `0` the whole loading happens inside the loader's own `DllMain`).

```cpp
extern "C" __declspec(dllexport) void InitializeASI()
{
    // patches, hooks, settings...
}
```

Other ASI loaders only call `LoadLibrary`. A plugin that should work with them too starts from `DllMain` when Ultimate ASI Loader is not the one loading it: the loader is then not on the call stack (it exports `IsUltimateASILoader`). A run-once guard covers loaders that do both:

```cpp
#include <stacktrace> // C++23

static bool LoadedByUltimateASILoader()
{
    for (const auto& frame : std::stacktrace::current())
    {
        HMODULE m = nullptr;
        if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)frame.native_handle(), &m) &&
            GetProcAddress(m, "IsUltimateASILoader"))
            return true;
    }
    return false;
}

static void InitOnce() { static std::once_flag once; std::call_once(once, Init); }

extern "C" __declspec(dllexport) void InitializeASI() { InitOnce(); }

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH && !LoadedByUltimateASILoader()) InitOnce(); // under the loader lock: keep Init simple
    return TRUE;
}
```

If the loader is on the stack because another plugin imported this one, the loader calls `InitializeASI` when it reaches the file in its own scan.

The demo plugins in [`source/plugins`](source/plugins) all follow this pattern; each is one source file (three also ship an `.ini`) (downloads: the [demo-plugins](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/tag/demo-plugins) release):

| Plugin | Shows |
|---|---|
| `MessageBox` | the smallest plugin: `InitializeASI`, and why nothing belongs in `DllMain` |
| `PluginTemplate` | a starting point for a fix: ini settings, a log file, an inline hook (safetyhook), a byte pattern search (Hooking.Patterns), the loader's update folder |
| `VirtualFiles` | the loader's file API: replaces game files from its ini without touching the disk |
| `FrameLimiter` | hooking a COM interface in any game: caps the frame rate of Direct3D 9 / 10 / 11 / 12 games |
| `ExeUnprotect` | makes the game's executable writable for old plugins that patch it without `VirtualProtect` |

## C++ SNIPPETS (.cxx)

Small patches can be written as plain C++ source files with the `.cxx` extension and placed wherever ASI plugins go (the game folder, `scripts`, `plugins`, update folders). The loader compiles them when the game starts; no compiler or SDK is needed. The same file also builds with Visual Studio as a DLL/ASI, so it can be debugged natively.

```cpp
// scripts\NoIntro.cxx
#include <injector/injector.hpp>
#include <Hooking.Patterns.h>

void Init()
{
    injector::WriteMemory<uint8_t>(hook::get_pattern("74 10 53 53 6A 1B"), 0xEB, true);
}
```

`Init()` runs once the snippet is compiled, and `Shutdown()` when it is unloaded. Snippets can use `injector/injector.hpp` (memory writes, NOP, JMP, CALL), `Hooking.Patterns.h` (pattern scanning) and `safetyhook.hpp` (inline, mid, VMT and VM hooks), and call any function exported by the DLLs loaded in the game (declare it with `extern "C"`). The language is a subset of C++: see [source/loader/cxx/cxxsnippets/README.md](source/loader/cxx/cxxsnippets/README.md) for what is supported.

**[examples/snippets](examples/snippets)** has one example per common task: patching values and jumps, patterns in the exe or a DLL, inline hooks (partial and full replacement, x86 calling conventions), mid hooks, hooking DLL exports and Windows functions, redirecting one call, Direct3D/virtual methods, game structs, DLLs loaded later, ini settings, threads and hotkeys, hot reload. Its README lists what snippets can't do and what to use instead.

Problems never crash the game silently:
- A compile error shows the file, line and message, and the other snippets still load.
- A crash in `Init` (or in the snippet's global initialization) is reported with the line that crashed. That snippet is unloaded, and its memory writes are undone.
- A crash later on, for example in a hook the game calls, is written to `<loader name>.log` with the snippet's file, line and function. If a `CrashDumps` folder exists, the crash report names that line too.

`[GlobalSets] CxxHotReload=1` reloads a snippet when its file is saved while the game runs (or a header in its folder changes). The edit is compiled first: if it has errors, they are shown and the running version stays. Otherwise the running version is unloaded and the new one starts. Unloading runs `Shutdown()`, removes its hooks and restores every byte it changed through `WriteMemory`, `MakeNOP`, `MakeJMP` and similar.

## UPDATE FOLDER (Overload From Folder)

It is possible to install mods that replace files via the `update` folder, allowing you to avoid replacing original game files.

For example, if a mod replaces the file located at:

```
Resident Evil 5\nativePC_MT\Image\Archive\ChapterEnd11.arc
```

With Ultimate ASI Loader installed, you can create an `update` folder and place the file at:

```
Resident Evil 5\update\nativePC_MT\Image\Archive\ChapterEnd11.arc
```

To revert the game to its initial state, simply remove the `update` folder.

Please note that the `update` folder is relative to the game's executable (where the ASI loader is installed), so you need to adjust paths accordingly. For example:

```
\Gameface\Content\Movies\1080\GTA_SA_CREDITS_FINAL_1920x1080.mp4
```

Should be adjusted to:

```
\Gameface\Binaries\Win64\update\Content\Movies\1080\GTA_SA_CREDITS_FINAL_1920x1080.mp4
```

Starting with version 7.9.0, you can use this functionality for total conversions:

![re5dx9_update](https://github.com/user-attachments/assets/a4baaa7b-d0bc-44ce-ab5e-d5fa8cc3ae43)

Two or more folders must be specified, and exist, for the selector dialog to appear. Define them inside global.ini under `[FileLoader]` section using the `OverloadFromFolder` key. Use the `|` symbol as a separator. If only one folder is specified and exists, it will be used to overload files, but the selector will not appear. Without an ini file, the `update` folder is always used if it exists. Example:

```ini
[FileLoader]
OverloadFromFolder=update | nightmare
```

To create a custom header, create `update.txt` inside `update` or total conversion folder and insert the custom name there.

`Resident Evil 5\nightmare\update.txt:`

```
Resident Evil 5 - Nightmare (Story mode mod)
```

The selector picks the first folder by itself after 10 seconds, unless you use the keyboard or the mouse. It is the loader's own window (light or dark like Windows, keyboard: arrow keys, Enter, Esc, 1-9); `[GlobalSets] ModernUI=0` uses a standard Windows task dialog instead. Error messages use the same style.

The game sees the update folder merged into its own folders: files and folders that exist only in `update` appear in the game folder too (file listings included), and files added to or removed from `update` while the game runs are picked up. New files the game creates are written to the game folder, never into `update`.

### Zip packages

Instead of a folder, the files can come from a zip archive in the `packages` folder: `packages\<anything>.zip`, or an archive split into parts (`<name>.zip.001`, `<name>.zip.002`, ... or `.zip.1`, `.zip.2`, ...). The top-level folder inside the archive names the update folder it provides (`update\...`, `nightmare\...`; case does not matter). An archive is used only when that folder does not exist on disk, and several archives providing the same folder are merged (the last one by name wins). Files are read directly from the archive and nothing is extracted to disk. The folder appears at `<game>\update`, so plugins can list and read it with the normal file functions, and `GetOverloadPath` returns that path. ASI plugins inside archives are not loaded. A zip-provided folder is marked `[ZIP]` in the selector dialog.

To get the current update path, use the ASI Loader's `GetOverloadPathA` or `GetOverloadPathW` exports from the ASI plugin.

```cpp
bool (WINAPI* GetOverloadPathW)(wchar_t* out, size_t out_size) = nullptr;

ModuleList dlls;
dlls.Enumerate(ModuleList::SearchLocation::LocalOnly);
for (auto& e : dlls.m_moduleList)
{
    auto m = std::get<HMODULE>(e);
    if (IsModuleUAL(m)) {
        GetOverloadPathW = (decltype(GetOverloadPathW))GetProcAddress(m, "GetOverloadPathW");
        break;
    }
}

std::wstring s;
s.resize(MAX_PATH, L'\0');
if (!GetOverloadPathW || !GetOverloadPathW(s.data(), s.size()))
    s = GetExeModulePath() / L"update";

auto updatePath = std::filesystem::path(s.data());
```

## ADDITIONAL WINDOWED MODE FEATURE (x86 builds only, legacy feature)

The 32-bit version of the ASI loader has a built-in window mode, which is enabled if you create an empty wndmode.ini in the folder with the ASI loader's DLL. It will be automatically filled with example configuration at the first run of the game. Settings are not universal and should be changed for each specific game, but usually, it works as is.

## D3D8TO9

Some mods, like [SkyGfx](https://github.com/aap/skygfx_vc), require [d3d8to9](https://github.com/crosire/d3d8to9). It is also a part of the ASI loader, so to use it, create [global.ini](https://github.com/ThirteenAG/Ultimate-ASI-Loader/edit/master/readme.md#usage) with the following content:

```ini
[GlobalSets]
UseD3D8to9=1
```
The ASI Loader must be named `d3d8.dll` in order for this feature to take effect.

[See an example of global.ini here](https://github.com/ThirteenAG/Ultimate-ASI-Loader/blob/master/data/plugins/global.ini#L8).

## CrashDumps

The ASI loader can write a crash report when the game crashes. To use this feature, create a folder named `CrashDumps` next to the ASI loader's DLL (or next to the game executable). Each crash produces:

- `<game>.exe.<date>_<time>.log`: a readable report. It starts with a summary of what crashed and where (module+offset, function and source line when symbols are available), and names the plugin if the crash happened inside one. It also lists system information, the exception (including the type and message of C++ exceptions), the call stack, return addresses found on the stack, registers, code and stack memory around the crash, the other threads, the loaded plugins and every loaded module with its version.
- `<game>.exe.<date>_<time>.zip`: the minidump (open it in Visual Studio or WinDbg), the same log and the loader's ini files. Attach this file when reporting a crash.

Options in `[GlobalSets]`:

| Option | Default | |
|---|---|---|
| `DisableCrashDumps` | `0` | `1` turns crash reports off even if the folder exists |
| `CrashDumpZip` | `1` | `0` keeps a plain `.dmp` file instead of the `.zip` |
| `CrashDumpFullMemory` | `0` | `1` writes a full memory dump (large, but contains everything) |
| `CrashDumpMaxReports` | `10` | number of reports kept in the folder; older ones are deleted (`0` keeps all) |

The report is written by a separate thread, so crashes such as stack overflows are captured too. Once installed, the game cannot replace the crash handler.

## Using with UWP games

1. Enable Developer Mode (Windows Settings -> Update and Security -> For Developers -> Developer Mode)
   ![image](https://user-images.githubusercontent.com/4904157/136562544-6d249514-203e-40c2-808f-34786b043ec5.png)
2. Install a UWP game, for example, GTA San Andreas.
   ![image](https://user-images.githubusercontent.com/4904157/136558440-553ef1f6-cf69-413b-903b-fd4203d6cc1f.png)
3. Launch a UWP game through the start menu.
4. Open [UWPInjector.exe](https://github.com/Wunkolo/UWPDumper) from the UWPDumper download.
   ![image](https://user-images.githubusercontent.com/4904157/136558563-6e39dd67-778e-4159-bb3b-83c499017223.png)
5. Enter the Process ID that is displayed from the injector and then hit enter.
6. Wait until the game is dumped.
   ![image](https://user-images.githubusercontent.com/4904157/136558813-8b7c271c-2475-40b9-a432-f9640f328a43.png)
7. Go to the directory : `C:\Users\[YOUR USERNAME]\AppData\Local\Packages\[YOUR UWP GAME NAME]\TempState\DUMP`
8. Copy these files into a new folder somewhere else of your choosing.
9. Uninstall a UWP game by clicking on the start menu, right-clicking on its icon, and uninstall.
   ![image](https://user-images.githubusercontent.com/4904157/136559019-bdd6d278-d2ae-4acf-b119-9933baab7d96.png)
10. Go to your directory with your new dumped files (the ones you copied over) and shift + right-click in the directory and "Open Powershell window here".
11. In that folder, rename **AppxBlockMap.xml** and **AppxSignature.xml** to anything else (to bypass UWP restrictions).
12. Run the following command: `Add-AppxPackage -Register AppxManifest.xml`
13. Place the Ultimate ASI Loader DLL into the game directory. You need to find out which name works for a specific game, in the case of GTA SA I've used **d3d11.dll**, so I put **dinput8.dll** from the x86 archive and renamed it to **d3d11.dll**.
14. Create a scripts or plugins folder within the root directory and place your plugins in it.
Rough code example of radio for all vehicles plugin [here](https://gist.github.com/ThirteenAG/868a964b46b82ce5cebbd4a0823c69e4). Compiled binary here - [GTASAUWP.RadioForAllVehicles.zip](https://github.com/ThirteenAG/Ultimate-ASI-Loader/files/7311505/GTASAUWP.RadioForAllVehicles.zip)
15. Click on the start menu and launch the game!  
16. See your mods in action.
![ApplicationFrameHost_2021-10-08_15-57-14](https://user-images.githubusercontent.com/4904157/136561208-e989119e-1ef4-42c2-8b20-c1f81f4e0931.png)
