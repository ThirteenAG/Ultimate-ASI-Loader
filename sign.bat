powershell -NoProfile -ExecutionPolicy Bypass -File "sign.ps1" ^
    -SearchPaths ".\bin\*.dll .\bin\*.asi" ^
    -MaxParallel 8