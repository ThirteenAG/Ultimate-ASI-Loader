<#
.SYNOPSIS
    Authenticode-signs files in parallel. Called by package.ps1.
.DESCRIPTION
    Signs only when CODE_SIGNING_PFX (base64 PFX) and CODE_SIGNING_PASSWORD are set,
    otherwise succeeds without doing anything so local builds work.
#>
param(
    [Parameter(Mandatory)][string[]]$Files,
    [int]$MaxParallel = 8
)

$ErrorActionPreference = "Stop"

if (-not ($env:CODE_SIGNING_PFX -and $env:CODE_SIGNING_PASSWORD)) {
    Write-Host "sign: CODE_SIGNING_PFX / CODE_SIGNING_PASSWORD not set, skipping signing"
    return
}

$signtool = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin" -Recurse -Filter signtool.exe -ErrorAction SilentlyContinue |
    Where-Object { $_.FullName -match '\\x64\\' } | Select-Object -Last 1 -ExpandProperty FullName
if (-not $signtool) { throw "sign: signtool.exe not found" }

$pfx = Join-Path $env:TEMP "signing_$(Get-Random).pfx"
[IO.File]::WriteAllBytes($pfx, [Convert]::FromBase64String(($env:CODE_SIGNING_PFX -replace '\s+', '')))
$password = $env:CODE_SIGNING_PASSWORD.Trim()

try {
    $jobs = foreach ($file in $Files | Select-Object -Unique) {
        while (@(Get-Job -State Running).Count -ge $MaxParallel) { Start-Sleep -Milliseconds 200 }
        Start-Job -ArgumentList $signtool, $pfx, $password, (Resolve-Path $file).Path -ScriptBlock {
            param($tool, $pfx, $pass, $path)
            $out = & $tool sign /fd SHA256 /f $pfx /p $pass /tr http://timestamp.digicert.com /td SHA256 $path 2>&1
            if ($LASTEXITCODE -ne 0) { throw "signtool failed for ${path}: $out" }
            $path
        }
    }
    $failed = 0
    foreach ($job in $jobs) {
        try { Write-Host "sign: $(Receive-Job $job -Wait -ErrorAction Stop)" }
        catch { Write-Warning $_; $failed++ }
        Remove-Job $job -Force
    }
    if ($failed) { throw "sign: $failed file(s) failed to sign" }
}
finally {
    Remove-Item $pfx -Force -ErrorAction SilentlyContinue
}
