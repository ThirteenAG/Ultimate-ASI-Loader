set list=d3d9, d3d10, d3d11, d3d12, dinput8, dsound, version, wininet, winmm, bink2w64
cd ./bin/x64/Release/
(for %%a in (%list%) do (
   copy dinput8.dll %%a.dll
))
cd ../../../
(for %%a in (%list%) do (
   7za a -tzip ".\bin\%%a.zip" ".\bin\x64\Release\%%a.dll"
))