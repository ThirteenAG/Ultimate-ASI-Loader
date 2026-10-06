<#
.SYNOPSIS
    Packages the Ultimate ASI Loader and the demo plugins for release.
.DESCRIPTION
    Run after building both platforms. bin\ is left untouched: shipped files are staged, get
    their PDBs embedded by tools\embedpdb.exe, are signed by tools\sign.ps1 when the secrets
    are set, and are archived into dist\:

      dist\Ultimate-ASI-Loader.zip, Ultimate-ASI-Loader_x64.zip             dinput8.dll with embedded PDB
      dist\Ultimate-ASI-Loader-NoPDB.zip, Ultimate-ASI-Loader-NoPDB_x64.zip dinput8.dll without PDB
      dist\<Win32|x64>\zip\<name>-<platform>.zip                            the loader renamed to every
      dist\<Win32|x64>\dll\<name>.dll + <name>-<platform>.SHA512            supported proxy name
      dist\plugins\*.zip                                                    demo plugins
.EXAMPLE
    tools\package.ps1
#>
param(
    [string]$Configuration = "Release",
    [string]$OutDir = "dist"
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem

$root = Split-Path $PSScriptRoot -Parent
$bin = Join-Path $root "bin"
$dist = Join-Path $root $OutDir
$stage = Join-Path $dist "stage"

# Published proxy names, matching the readme and release.md download tables
$aliases = @{
    Win32 = "d3d8", "d3d9", "d3d10", "d3d11", "d3d12", "dxgi", "dinput8", "ddraw", "dinput", "dsound", "msacm32", "msvfw32",
            "version", "wininet", "winmm", "winhttp", "xlive", "vorbisFile", "binkw32", "bink2w32",
            "xinput1_1", "xinput1_2", "xinput1_3", "xinput1_4", "xinput9_1_0", "xinputuap"
    x64   = "d3d9", "d3d10", "d3d11", "d3d12", "dxgi", "dinput8", "dsound", "version", "wininet", "winmm", "winhttp",
            "binkw64", "bink2w64", "xinput1_1", "xinput1_2", "xinput1_3", "xinput1_4", "xinput9_1_0", "xinputuap"
}

# Relative to bin\<platform>\<configuration>
$shipped = @{
    Win32 = "dinput8.dll", "scripts\ExeUnprotect.asi", "scripts\MessageBox.asi", "scripts\PluginTemplate.asi", "scripts\VirtualFiles.asi", "scripts\FrameLimiter.asi"
    x64   = "dinput8.dll", "scripts\MessageBox_x64.asi", "scripts\PluginTemplate.asi", "scripts\VirtualFiles.asi", "scripts\FrameLimiter.asi", "VirtualFileServer.exe"
}

function New-Zip([string]$Path, [System.Collections.IDictionary]$Entries, [switch]$Store) {
    # $Entries: archive name -> file on disk, $null for an empty marker entry
    New-Item -ItemType Directory -Force -Path (Split-Path $Path) | Out-Null
    if (Test-Path $Path) { Remove-Item $Path -Force }
    $level = if ($Store) { [IO.Compression.CompressionLevel]::NoCompression } else { [IO.Compression.CompressionLevel]::Optimal }
    $zip = [IO.Compression.ZipFile]::Open($Path, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($name in $Entries.Keys) {
            $src = $Entries[$name]
            if ($src) { [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $src, $name, $level) }
            else { [void]$zip.CreateEntry($name) }
        }
    }
    finally { $zip.Dispose() }
    Write-Host "  $($Path.Substring($root.Length + 1))"
}

# $dist is wiped below, so it must be a folder of its own inside the repo
$distFull = [IO.Path]::GetFullPath($dist)
$rootFull = [IO.Path]::GetFullPath($root)
if (-not $distFull.StartsWith($rootFull + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase) -or
    (Test-Path (Join-Path $distFull ".git")) -or (Test-Path (Join-Path $distFull "premake5.lua"))) {
    throw "OutDir must be a sub folder of the repository that holds only packaging output: $distFull"
}
if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }

# 1. stage shipped binaries with their PDBs, plus PDB-less copies of the loader
$toEmbed = @()
$toSign = @()
foreach ($platform in "Win32", "x64") {
    $src = Join-Path $bin "$platform\$Configuration"
    foreach ($rel in $shipped[$platform]) {
        $file = Join-Path $src $rel
        if (-not (Test-Path $file)) { throw "missing build output: $file (build $platform|$Configuration first)" }
        $dst = Join-Path $stage "$platform\$rel"
        New-Item -ItemType Directory -Force -Path (Split-Path $dst) | Out-Null
        Copy-Item $file $dst
        $pdb = [IO.Path]::ChangeExtension($file, ".pdb")
        if (Test-Path $pdb) { Copy-Item $pdb ([IO.Path]::ChangeExtension($dst, ".pdb")); $toEmbed += $dst }
        $toSign += $dst
    }
    $noPdb = Join-Path $dist "NoPDB\$platform\dinput8.dll"
    New-Item -ItemType Directory -Force -Path (Split-Path $noPdb) | Out-Null
    Copy-Item (Join-Path $src "dinput8.dll") $noPdb
    $toSign += $noPdb
}

# 2. embed debug info
Write-Host "Embedding PDBs..."
& (Join-Path $PSScriptRoot "embedpdb.exe") @toEmbed | Write-Host
if ($LASTEXITCODE) { throw "embedpdb failed with exit code $LASTEXITCODE" }
Get-ChildItem $stage -Recurse -Filter *.pdb | Remove-Item

# 3. sign, a no-op without the secrets
& (Join-Path $PSScriptRoot "sign.ps1") -Files $toSign

# 4. archives
Write-Host "Creating archives..."
$w32 = Join-Path $stage "Win32"
$x64 = Join-Path $stage "x64"
New-Zip "$dist\Ultimate-ASI-Loader.zip" @{ "dinput8.dll" = "$w32\dinput8.dll" }
New-Zip "$dist\Ultimate-ASI-Loader_x64.zip" @{ "dinput8.dll" = "$x64\dinput8.dll" }
New-Zip "$dist\Ultimate-ASI-Loader-NoPDB.zip" @{ "dinput8.dll" = "$dist\NoPDB\Win32\dinput8.dll" }
New-Zip "$dist\Ultimate-ASI-Loader-NoPDB_x64.zip" @{ "dinput8.dll" = "$dist\NoPDB\x64\dinput8.dll" }

foreach ($platform in "Win32", "x64") {
    $dllDir = Join-Path $dist "$platform\dll"
    New-Item -ItemType Directory -Force -Path $dllDir | Out-Null
    foreach ($name in $aliases[$platform]) {
        $dll = Join-Path $dllDir "$name.dll"
        Copy-Item (Join-Path $stage "$platform\dinput8.dll") $dll
        $hash = Join-Path $dllDir "$name-$platform.SHA512"
        Get-FileHash $dll -Algorithm SHA512 | Format-List | Out-File -Encoding utf8 $hash
        New-Zip "$dist\$platform\zip\$name-$platform.zip" ([ordered]@{ "$name.dll" = $dll; "$name-$platform.SHA512" = $hash }) -Store
    }
}

$plugins = Join-Path $dist "plugins"
New-Zip "$plugins\ExeUnprotect-Win32.zip" @{ "ExeUnprotect.asi" = "$w32\scripts\ExeUnprotect.asi" }
New-Zip "$plugins\MessageBox-Win32.zip" @{ "MessageBox.asi" = "$w32\scripts\MessageBox.asi" }
New-Zip "$plugins\MessageBox-x64.zip" @{ "MessageBox_x64.asi" = "$x64\scripts\MessageBox_x64.asi" }
# plugins with an .ini
foreach ($name in "PluginTemplate", "VirtualFiles", "FrameLimiter") {
    foreach ($platform in "Win32", "x64") {
        $dir = if ($platform -eq "Win32") { $w32 } else { $x64 }
        $ini = Join-Path $bin "$platform\$Configuration\scripts\$name.ini" # not staged, nothing to sign
        New-Zip "$plugins\$name-$platform.zip" ([ordered]@{ "$name.asi" = "$dir\scripts\$name.asi"; "$name.ini" = $ini })
    }
}
New-Zip "$plugins\VirtualFileServer-x64.zip" @{ "VirtualFileServer.exe" = "$x64\VirtualFileServer.exe" }

Remove-Item $stage -Recurse -Force
Write-Host "Done: $dist"
