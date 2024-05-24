$aliases_array_Win32 = "d3d8", "d3d9", "d3d10", "d3d11", "d3d12", "dinput8", "ddraw", "dinput", "dsound", "msacm32", "msvfw32", "version", "wininet", "winmm", "winhttp", "xlive", "vorbisFile", "binkw32", "bink2w32"
$aliases_array_x64   = "d3d9", "d3d10", "d3d11", "d3d12", "dinput8", "dsound", "version", "wininet", "winmm", "winhttp", "binkw64", "bink2w64"
$platform_array      = "Win32", "x64"
$hash_alrg           = "SHA512"

foreach ($platform in $platform_array)
{
    mkdir "dist\$platform\dll"
    mkdir "dist\$platform\zip"
    if ($platform -eq "Win32") {
        Move-Item ".\bin\$platform\Release\dinput8.dll" ".\bin\$platform\Release\_dinput8.dll"
        foreach ($file in $aliases_array_Win32) {
            Copy-Item ".\bin\$platform\Release\_dinput8.dll" ".\bin\$platform\Release\$file.dll"
        }
    }
    else
    {
        Move-Item ".\bin\$platform\Release\dinput8.dll" ".\bin\$platform\Release\_dinput8.dll"
        foreach ($file in $aliases_array_x64) {
            Copy-Item ".\bin\$platform\Release\_dinput8.dll" ".\bin\$platform\Release\$file.dll"
        }
    }
    if ($platform -eq "Win32") {
        foreach ($file in $aliases_array_Win32) {
            Get-FileHash ".\bin\$platform\Release\$file.dll" -Algorithm "$hash_alrg" | Format-List | Out-File -Encoding "utf8" ".\bin\$file-$platform.$hash_alrg"
            Copy-Item ".\bin\$platform\Release\$file.dll", ".\bin\$file-$platform.$hash_alrg" -Destination ".\dist\$platform\dll\"
            $compress = @{
                Path             = ".\bin\$platform\Release\$file.dll", ".\bin\$file-$platform.$hash_alrg"
                CompressionLevel = "NoCompression"
                DestinationPath  = ".\dist\$platform\zip\$file-$platform.zip"
            }
            Compress-Archive @compress
        }
    }
    else
    {
        foreach ($file in $aliases_array_x64) {
            Get-FileHash ".\bin\$platform\Release\$file.dll" -Algorithm "$hash_alrg" | Format-List | Out-File -Encoding "utf8" ".\bin\$file-$platform.$hash_alrg"
            Copy-Item ".\bin\$platform\Release\$file.dll", ".\bin\$file-$platform.$hash_alrg" -Destination ".\dist\$platform\dll\"
            $compress = @{
                Path             = ".\bin\$platform\Release\$file.dll", ".\bin\$file-$platform.$hash_alrg"
                CompressionLevel = "NoCompression"
                DestinationPath  = ".\dist\$platform\zip\$file-$platform.zip"
            }
            Compress-Archive @compress
        }
    }
}
