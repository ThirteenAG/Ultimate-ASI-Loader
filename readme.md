Ultimate ASI Loader
===================

DESCRIPTION
------------------------
This is a DLL file which adds ASI plugin loading functionality to any game, which uses any of the following libraries:
* vorbisFile.dll
* d3d8.dll
* d3d9.dll
* d3d11.dll
* winmmbase.dll
* msacm32.dll
* dinput8.dll (x86 and x64)
* dsound.dll (x86 and x64)
* ddraw.dll
* xlive.dll


INSTALLATION
------------------------
In order to install it, you just need to place DLL into game directory. Usually it works as dinput8.dll, but if it's not, there is a possibility to rename it to d3d8.dll, d3d9.dll, d3d11.dll, winmmbase.dll, dinput8.dll, dsound.dll, ddraw.dll, vorbisFile.dll, xlive.dll.


USAGE
------------------------
Put ASI files in game root directory, 'scripts' or 'plugins' folder.


ADDITIONAL WINDOWED MODE FEATURE
------------------------
ASI loader have built-in wndmode.dll, which can be loaded, if you create empty wndmode.ini in the folder with asi loader's dll. It will be automatically filled with example configuration at the first run of the game. Settings are not universal and should be changed in every specific case.

D3D8TO9
------------------------
Some mods, like ![skygfx](https://github.com/aap/skygfx_vc) require ![d3d8to9](https://github.com/crosire/d3d8to9). It is also a part of ASI loader, so in order to use it, create global.ini inside scripts folder with the following content:
```
[GlobalSets]
UseD3D8to9=1
```

![See example of global.ini here](https://github.com/ThirteenAG/Ultimate-ASI-Loader/blob/master/data/scripts/global.ini).
