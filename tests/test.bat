@echo off
setlocal EnableDelayedExpansion

REM Ensure script runs from its own directory
cd /d "%~dp0" || (
    echo Error: Failed to set working directory to %~dp0
    exit /b 1
)

REM Define directories and executables
set "BASE_DIR=%CD%"

set "DIRECTORIES=ASILoading OverloadFromFolder OverloadFromFolderVirtualFile OverloadFromFolderZipFile"
set "TEST_FILES=ASILoading:dynamic:dynamic OverloadFromFolder:update_test_passed.txt:update_test_passed.txt OverloadFromFolderVirtualFile:virtual_file_test_passed.txt:virtual_file_test_passed.txt OverloadFromFolderZipFile:zip_file_test_passed.txt:zip_file_test_passed.txt"
set "X86_SAMPLES=DInput8Sample.exe D3D9Sample.exe D3D10Sample.exe D3D11Sample.exe D3D12Sample.exe"
set "X64_SAMPLES=DInput8Sample64.exe D3D9Sample64.exe D3D10Sample64.exe D3D11Sample64.exe D3D12Sample64.exe"
set "X86_DLLS=dinput8.dll d3d9.dll d3d10.dll d3d11.dll d3d12.dll"
set "X64_DLLS=dinput8.dll d3d9.dll d3d10.dll d3d11.dll d3d12.dll"

REM Process Win32 and x64 directories
for %%A in (Win32 x64) do (
    set "ARCH=%%A"
    if "!ARCH!"=="Win32" (
        set "SAMPLES=%X86_SAMPLES%"
        set "DLLS=%X86_DLLS%"
    ) else (
        set "SAMPLES=%X64_SAMPLES%"
        set "DLLS=%X64_DLLS%"
    )

    for %%D in (%DIRECTORIES%) do (
        set "D=%%D"
        REM echo Processing !D! in !ARCH!
        if not exist "%%D\!ARCH!\" (
            echo Error: Directory %%D\!ARCH! does not exist
            dir "%%D" 2>nul
            cd /d "%BASE_DIR%"
            exit /b 1
        )

        REM Change to target directory and verify
        pushd "%%D\!ARCH!" || (
            echo Error: Failed to change to directory %%D\!ARCH!
            dir "%%D" 2>nul
            cd /d "%BASE_DIR%"
            exit /b 1
        )
        set "CURRENT_DIR=%CD%"
        REM echo Current directory: !CURRENT_DIR!
        REM cd
        REM dir *.exe 2>nul

        REM Clean up existing .txt files
        del /s *.txt >nul 2>&1

        REM Set executables
        set "EXECS=!SAMPLES!"

        REM Copy required files
        if not exist "..\..\..\bin\!ARCH!\Release\dinput8.dll" (
            echo Error: dinput8.dll not found in ..\..\..\bin\!ARCH!\Release
            popd
            cd /d "%BASE_DIR%"
            exit /b 1
        )
        if not "!D!"=="ASILoading" (
            if not exist "..\..\..\bin\x64\Release\VirtualFileServer.exe" (
                echo Error: VirtualFileServer.exe not found in ..\..\..\bin\x64\Release
                popd
                cd /d "%BASE_DIR%"
                exit /b 1
            )
        )
        for %%L in (!DLLS!) do (
            copy "..\..\..\bin\!ARCH!\Release\dinput8.dll" "%%L" >nul
            if not "!D!"=="ASILoading" (
                copy "..\..\..\bin\x64\Release\VirtualFileServer.exe" "%%~nL.exe" >nul
            )
        )
        copy "..\..\..\bin\!ARCH!\Release\Test_!ARCH!.asi" "Test_!ARCH!.asi" >nul

        REM Run tests for each executable
        for %%E in (!EXECS!) do (
            set "E=%%E"
            REM echo Testing !E! in !D! (!ARCH!)
            if not exist "!E!" (
                echo Error: Executable !E! not found in !CURRENT_DIR!
                REM dir *.exe 2>nul
                popd
                cd /d "%BASE_DIR%"
                exit /b 1
            )

            REM Get the expected test file for this directory
            set "TEST_FILE="
            for %%T in (%TEST_FILES%) do (
                for /F "tokens=1,2,3 delims=:" %%X in ("%%T") do (
                    if "%%X"=="%%D" (
                        if "%%D"=="ASILoading" (
                            REM Derive test file from executable name for ASILoading
                            set "TEST_FILE=!E:~0,-4!.txt"
                        ) else if "!ARCH!"=="Win32" (
                            set "TEST_FILE=%%Y"
                        ) else (
                            set "TEST_FILE=%%Z"
                        )
                    )
                )
            )

            if not defined TEST_FILE (
                echo Error: No test file defined for !D! in !ARCH!
                popd
                cd /d "%BASE_DIR%"
                exit /b 1
            )

            REM echo Running !E!, expecting test file: !TEST_FILE!
            REM echo.
            start /W /B "" "!E!"

            if exist "!TEST_FILE!" (
                echo !ARCH! !D! Test for !E! PASSED
                del "!TEST_FILE!"
                REM Ensure the process is terminated
                taskkill /IM "!E!" /F >nul 2>&1
                REM echo.
            ) else (
                REM Process not running and no file, fail immediately
                echo !ARCH! !D! Test for !E! FAILED
                popd
                cd /d "%BASE_DIR%"
                exit /b 1
            )
        )

        popd
    )
)

echo All tests completed successfully.
endlocal
exit /b 0