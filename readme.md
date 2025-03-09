[![AppVeyor](https://img.shields.io/appveyor/build/ThirteenAG/Ultimate-ASI-Loader?label=AppVeyor%20Build&logo=Appveyor&logoColor=white)](https://ci.appveyor.com/project/ThirteenAG/ultimate-asi-loader)
[![GitHub Actions Build](https://github.com/ThirteenAG/Ultimate-ASI-Loader/actions/workflows/msbuild.yml/badge.svg)](https://github.com/ThirteenAG/Ultimate-ASI-Loader/actions/workflows/msbuild.yml)

# Ultimate ASI Loader

## DESCRIPTION

This is a DLL file that adds ASI plugin loading functionality to any game, which uses any of the following libraries:

|                                                          Win32                                                          |                                                      Win64                                                      |
| :---------------------------------------------------------------------------------------------------------------------: | :-------------------------------------------------------------------------------------------------------------: |
| [d3d8.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/d3d8-Win32.zip)             |                                                        -                                                        |
| [d3d9.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/d3d9-Win32.zip)             |     [d3d9.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/d3d9-x64.zip)     |
| [d3d10.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/d3d10-Win32.zip)           |    [d3d10.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/d3d10-x64.zip)    |
| [d3d11.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/d3d11-Win32.zip)           |    [d3d11.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/d3d11-x64.zip)    |
| [d3d12.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/d3d12-Win32.zip)           |    [d3d12.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/d3d12-x64.zip)    |
| [ddraw.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/ddraw-Win32.zip)           |                                                        -                                                        |
| [dinput.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/dinput-Win32.zip)         |                                                        -                                                        |
| [dinput8.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/dinput8-Win32.zip)       |  [dinput8.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/dinput8-x64.zip)  |
| [dsound.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/dsound-Win32.zip)         |   [dsound.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/dsound-x64.zip)   |
| [msacm32.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/msacm32-Win32.zip)       |                                                        -                                                        |
| [msvfw32.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/msvfw32-Win32.zip)       |                                                        -                                                        |
| [version.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/version-Win32.zip)       |  [version.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/version-x64.zip)  |
| [wininet.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/wininet-Win32.zip)       |  [wininet.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/wininet-x64.zip)  |
| [winmm.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/winmm-Win32.zip)           |    [winmm.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/winmm-x64.zip)    |
| [winhttp.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/winhttp-Win32.zip)       |  [winhttp.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/winhttp-x64.zip)  |
| [xlive.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/xlive-Win32.zip)           |                                                        -                                                        |
| [binkw32.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/binkw32-Win32.zip)       |                                                        -                                                        |
| [bink2w32.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/bink2w32-Win32.zip)     |                                                        -                                                        |
|                                                            -                                                            |  [binkw64.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/binkw64-x64.zip)  |
|                                                            -                                                            | [bink2w64.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/bink2w64-x64.zip) |
| [vorbisFile.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/vorbisFile-Win32.zip) |                                                        -                                                        |

It is possible(and sometimes necessary) to load the original dll by renaming it to `<dllname>Hooked.dll`, e.g. `d3d12Hooked.dll`.
With **binkw32.dll** and **vorbisFile.dll** it is optional and you can simply replace the dll. Always make a backup before replacing any files.


## INSTALLATION

To install it, you just need to place DLL into the game directory. Usually, it works as dinput8.dll, but if it's not, there is a possibility to rename it(see the list of supported names above).

## USAGE

Put ASI files in the game root directory, 'scripts', 'plugins', or 'update' folder.
If a configuration is necessary, the global.ini file can be placed in the 'scripts' or 'plugins' folder. It can be used alongside the chosen dll and if so, it is also possible to use the dll name for ini file, e.g. version.dll/version.ini.
[See an example of global.ini here](https://github.com/ThirteenAG/Ultimate-ASI-Loader/blob/master/data/scripts/global.ini).

## UPDATE FOLDER (Overload From Folder)

It is possible to install mods that replace files via the `update` folder, allowing you to avoid actual file replacement.

For example, if a mod replaces the file located at:

```
Resident Evil 5\nativePC_MT\Image\Archive\ChapterEnd11.arc
```

With Ultimate ASI Loader installed, you can create an `update` folder and place the file at:

```
Resident Evil 5\update\nativePC_MT\Image\Archive\ChapterEnd11.arc
```

To revert the game to its initial state, simply remove the `update` folder.

Please note that the `update` folder is relative to the location of the ASI loader, so you need to adjust paths accordingly. For example:

```
\Gameface\Content\Movies\1080\GTA_SA_CREDITS_FINAL_1920x1080.mp4
```

Should be adjusted to:

```
\Gameface\Binaries\Win64\update\Content\Movies\1080\GTA_SA_CREDITS_FINAL_1920x1080.mp4
```

Starting with version 7.9.0, you can use this functionality for total conversions:

![re5dx9_update](https://github.com/user-attachments/assets/7ec4c006-2205-444f-9a7a-8d3c8f5b62fb)

Two or more folders must be specified and exist for the selector dialog to appear. Define them inside global.ini under `[FileLoader]` section at `OverloadFromFolder` key. Use `|` symbol as a separator. If only one folder is specified and exists, it will be used to overload files, but the selector will not appear. Without ini file, `update` folder is always used if it exists. Example:

```ini
[FileLoader]
OverloadFromFolder=update | nightmare
```

To create a custom header, create `update.txt` inside update/total conversion folder and insert the custom name there.

`Resident Evil 5\nightmare\update.txt:`

```
Resident Evil 5 - Nightmare (Story mode mod)
```

## ADDITIONAL WINDOWED MODE FEATURE (x86 builds only)

32-bit version of ASI loader has built-in wndmode.dll, which can be loaded if you create empty wndmode.ini in the folder with asi loader's dll. It will be automatically filled with example configuration at the first run of the game. Settings are not universal and should be changed in every specific case, but usually, it works as is.

## D3D8TO9

Some mods, like [SkyGfx](https://github.com/aap/skygfx_vc) require [d3d8to9](https://github.com/crosire/d3d8to9). It is also a part of the ASI loader, so to use it, create [global.ini](https://github.com/ThirteenAG/Ultimate-ASI-Loader/edit/master/readme.md#usage) with the following content:

```ini
[GlobalSets]
UseD3D8to9=1
```
Asi loader must be named `d3d8.dll` in order for this feature to take effect.

[See an example of global.ini here](https://github.com/ThirteenAG/Ultimate-ASI-Loader/blob/master/data/scripts/global.ini#L6).

## CrashDumps

ASI loader is now capable of generating crash minidumps and crash logs. To use this feature, create a folder named `CrashDumps` in the folder with asi loader's dll. You can disable that via the `DisableCrashDumps=1` ini option.

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
11. In that folder, rename **AppxBlockMap.xml** and **AppxSignature.xml** to anything else.
12. Run the following command: `Add-AppxPackage -Register AppxManifest.xml`
13. Place Ultimate ASI Loader DLL into the game directory. You need to find out which name works for a specific game, in the case of GTA SA I've used **d3d11.dll**, so I put **dinput8.dll** from the x86 archive and renamed it to **d3d11.dll**.
14. Create an ini file with the same name, in this case: **d3d11.ini**, with the following content:

```ini
[GlobalSets]
DontLoadFromDllMain=0
```

Sometimes it may not be necessary, but UWP GTA SA didn't work with the current implementation of `DontLoadFromDllMain=1`.  
15. Create a **scripts** or **plugins** folder within the root directory and place your plugins in it.  
Rough code example of radio for all vehicles plugin [here](https://gist.github.com/ThirteenAG/868a964b46b82ce5cebbd4a0823c69e4). Compiled binary here - [GTASAUWP.RadioForAllVehicles.zip](https://github.com/ThirteenAG/Ultimate-ASI-Loader/files/7311505/GTASAUWP.RadioForAllVehicles.zip)  
16. Click on the start menu and launch the game!  
17. See your mods in action.  
![ApplicationFrameHost_2021-10-08_15-57-14](https://user-images.githubusercontent.com/4904157/136561208-e989119e-1ef4-42c2-8b20-c1f81f4e0931.png)
