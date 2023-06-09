set list=d3d8, d3d9, d3d10, d3d11, d3d12, ddraw, dinput, dinput8, dsound, msacm32, msvfw32, version, wininet, winmm, xlive, vorbisFile, binkw32
mkdir dist\Win32
cd .\bin\Win32\Release
(for %%a in (%list%) do (
   copy dinput8.dll %%a.dll
))
cd ../../../
(for %%a in (%list%) do (
   7za a -tzip ".\bin\%%a-Win32.zip" ".\bin\Win32\Release\%%a.dll"
))
