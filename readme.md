[![GitHub Actions Build](https://github.com/ThirteenAG/Ultimate-ASI-Loader/actions/workflows/msbuild.yml/badge.svg)](https://github.com/ThirteenAG/Ultimate-ASI-Loader/actions/workflows/msbuild.yml)

# Ultimate ASI Loader

## DESCRIPTION

This is a DLL file which adds ASI plugin loading functionality to any game, which uses any of the following libraries:

- [d3d8.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/d3d8.zip)
- d3d9.dll ([x86](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/d3d9.zip) and [x64](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/d3d9.zip))
- d3d10.dll ([x86](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/d3d10.zip) and [x64](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/d3d10.zip))
- d3d11.dll ([x86](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/d3d11.zip) and [x64](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/d3d11.zip))
- d3d12.dll ([x86](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/d3d12.zip) and [x64](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/d3d12.zip))
- [ddraw.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/ddraw.zip)
- [dinput.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/dinput.zip)
- dinput8.dll ([x86](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/dinput8.zip) and [x64](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/dinput8.zip))
- dsound.dll ([x86](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/dsound.zip) and [x64](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/dsound.zip))
- [msacm32.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/msacm32.zip)
- [msvfw32.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/msvfw32.zip)
- version.dll ([x86](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/version.zip) and [x64](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/version.zip))
- wininet.dll ([x86](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/wininet.zip) and [x64](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/wininet.zip))
- winmm.dll ([x86](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/winmm.zip) and [x64](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/winmm.zip))
- [xlive.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/xlive.zip)
- [bink2w64.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/x64-latest/bink2w64.zip) (rename original to bink2w64Hooked.dll)
- [vorbisFile.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/vorbisFile.zip) (rename original to vorbisHooked.dll, optional)
- [binkw32.dll](https://github.com/ThirteenAG/Ultimate-ASI-Loader/releases/download/Win32-latest/binkw32.zip) (rename original to binkw32Hooked.dll, optional)

With the last two, it is possible to load the original dll by renaming it to 'vorbisHooked.dll' or 'binkw32Hooked.dll'.
Usually it is not required and you can simply replace the dll. Always make a backup before replacing any files.

## INSTALLATION

In order to install it, you just need to place DLL into game directory. Usually it works as dinput8.dll, but if it's not, there is a possibility to rename it(see the list of supported names above).

## USAGE

Put ASI files in game root directory, 'scripts' or 'plugins' folder.
If configuration is necessary, global.ini file can be placed to 'scripts' or 'plugins' folder. It can be used alongside the chosen dll and if so, it is also possible to use dll name for ini file, e.g. version.dll/version.ini.
[See example of global.ini here](https://github.com/ThirteenAG/Ultimate-ASI-Loader/blob/master/data/scripts/global.ini).

## ADDITIONAL WINDOWED MODE FEATURE

ASI loader have built-in wndmode.dll, which can be loaded, if you create empty wndmode.ini in the folder with asi loader's dll. It will be automatically filled with example configuration at the first run of the game. Settings are not universal and should be changed in every specific case, but usually it works as is.

## D3D8TO9

Some mods, like [SkyGfx](https://github.com/aap/skygfx_vc) require [d3d8to9](https://github.com/crosire/d3d8to9). It is also a part of ASI loader, so in order to use it, create global.ini inside scripts folder with the following content:

```ini
[GlobalSets]
UseD3D8to9=1
```

[See example of global.ini here](https://github.com/ThirteenAG/Ultimate-ASI-Loader/blob/master/data/scripts/global.ini#L6).

## CrashDumps

ASI loader is now capable of generating crash minidumps and crash logs. To use this feature, create a folder named `CrashDumps` in the folder with asi loader's dll. You can disable that via `DisableCrashDumps=1` ini option.

## Using with UWP games

1. Enable Developer Mode (Windows Settings -> Update and Security -> For Developers -> Developer Mode)  
   ![image](https://user-images.githubusercontent.com/4904157/136562544-6d249514-203e-40c2-808f-34786b043ec5.png)
2. Install an UWP game, for example GTA San Andreas.  
   ![image](https://user-images.githubusercontent.com/4904157/136558440-553ef1f6-cf69-413b-903b-fd4203d6cc1f.png)
3. Launch an UWP game through the start menu.
4. Open [UWPInjector.exe](https://github.com/Wunkolo/UWPDumper) from the UWPDumper download.  
   ![image](https://user-images.githubusercontent.com/4904157/136558563-6e39dd67-778e-4159-bb3b-83c499017223.png)
5. Enter the Process ID that is displayed from the injector and then hit enter.
6. Wait until the game is dumped.  
   ![image](https://user-images.githubusercontent.com/4904157/136558813-8b7c271c-2475-40b9-a432-f9640f328a43.png)
7. Go to the directory : `C:\Users\[YOUR USERNAME]\AppData\Local\Packages\[YOUR UWP GAME NAME]\TempState\DUMP`
8. Copy these files into a new folder somewhere else of your choosing.
9. Uninstall an UWP game by clicking on start menu and right clicking on its icon and uninstall.  
   ![image](https://user-images.githubusercontent.com/4904157/136559019-bdd6d278-d2ae-4acf-b119-9933baab7d96.png)
10. Go to your directory with your new dumped files (the ones you copied over) and shift + right click in the directory and "Open Powershell window here".
11. In that folder, rename **AppxBlockMap.xml** and **AppxSignature.xml** to anything else.
12. Run the following command: `Add-AppxPackage -Register AppxManifest.xml`
13. Place Ultimate ASI Loader DLL into game directory. You need to find out which name works for a specific game, in case of GTA SA I've used **d3d11.dll**, so I put **dinput8.dll** from x86 archive and renamed it to **d3d11.dll**.
14. Create an ini file with the same name, in this case: **d3d11.ini**, with the following content:

```ini
[GlobalSets]
DontLoadFromDllMain=0
```

Sometimes it may not be necessary, but UWP GTA SA didn't work with current implementation of `DontLoadFromDllMain=1`.  
15. Create **scripts** or **plugins** folder within the root directory and place your plugins in it.  
Rough code example of radio for all vehicles plugin [here](https://gist.github.com/ThirteenAG/868a964b46b82ce5cebbd4a0823c69e4). Compiled binary here - [GTASAUWP.RadioForAllVehicles.zip](https://github.com/ThirteenAG/Ultimate-ASI-Loader/files/7311505/GTASAUWP.RadioForAllVehicles.zip)  
16. Click on the start menu and launch the game!  
17. See your mods in action.  
![ApplicationFrameHost_2021-10-08_15-57-14](https://user-images.githubusercontent.com/4904157/136561208-e989119e-1ef4-42c2-8b20-c1f81f4e0931.png)
