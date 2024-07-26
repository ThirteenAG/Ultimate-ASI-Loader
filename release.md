This is a DLL file which adds ASI plugin loading functionality to any game, which uses any of the following libraries:

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
