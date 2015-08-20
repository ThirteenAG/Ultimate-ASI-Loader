Ultimate_ASI_Loader
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
* dinput8.dll
* dsound.dll
* ddraw.dll


INSTALLATION
------------------------
In order to install it, you just need to place DLL into game directory. Usually it works as dinput8.dll, but if it's not, there is a possibility to rename it to d3d8.dll, d3d9.dll, d3d11.dll, winmmbase.dll, dinput8.dll, dsound.dll, vorbisFile.dll or ddraw.dll(for old games, like GTA2).
	

USAGE
------------------------
Basic Plugin Installation:
If you want to add an ASI for all Executables you have, put it either in game root directory or 'scripts' folder.
		

Advanced Plugin Installation:
If you have multiple Executables and want to make each one use different ASI plugins, you can create a new folder inside 'scripts' folder with the same name as your Executable (so Blacklist_DX11_game.exe gets Blacklist_DX11_game folder, my_fancy_exe.exe gets my_fancy_exe folder etc.). Plugins placed in this folder will be exclusive to specific Executable. You also can make specific Executables override global settings and load/ignore all plugins via settings.ini edit. Specific Executable can also ignore only some of the plugins - see 'scripts\advanced_plugin_management_example' for more info.


ADDITIONAL WINDOWED MODE FEATURE
------------------------
ASI loader have built-in wndmode.dll, which can be loaded, if you create empty wndmode.ini in the folder with asi loader's dll. It will be automatically filled with example configuration at the first run of the game, also you can see and use that example inside 'scripts\windowed_mode_ini_example' folder. Settings are not universal and should be changed in every specific case.
