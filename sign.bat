powershell -NoProfile -ExecutionPolicy Bypass -File "sign.ps1" ^
    -SearchPaths ".\bin\*.dll .\bin\*.asi .\dist\NoPDB\*.dll" ^
    -MaxParallel 8