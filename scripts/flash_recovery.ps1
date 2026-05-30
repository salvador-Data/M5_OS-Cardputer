<#
.SYNOPSIS
  Recover Cardputer from black screen / bad otadata - full flash erase + M5 OS upload.
.DESCRIPTION
  Use when the device shows a black screen after Load app, partition changes, or a partial USB flash.
  Requires USB on COM13 (or pass -Port). Run from Owner account PowerShell when possible.
#>
param(
    [string]$Port = 'COM13',
    [switch]$SkipErase,
    [switch]$SkipBootloaderBuild,
    [switch]$SkipBuild
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$Repo = Split-Path -Parent $PSScriptRoot
$OwnerHome = 'C:\Users\Owner'
$PioCandidates = @(
    (Join-Path $OwnerHome '.platformio\penv\Scripts\pio.exe')
    (Join-Path $env:USERPROFILE '.platformio\penv\Scripts\pio.exe')
)
$Pio = $PioCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
if (-not $Pio) { throw 'PlatformIO not found. Install under Owner or set PATH to pio.exe.' }
$env:PLATFORMIO_CORE_DIR = Split-Path (Split-Path $Pio -Parent) -Parent

Write-Host '=== M5 OS Cardputer recovery ===' -ForegroundColor Cyan
Write-Host "Repo: $Repo"
Write-Host "Port: $Port"

# Stop stray PIO builds (cross-account races corrupt M5GFX .o files)
Get-CimInstance Win32_Process -Filter "Name='python.exe'" -ErrorAction SilentlyContinue |
    Where-Object { $_.CommandLine -match 'platformio' } |
    ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }

$pioBuild = Join-Path $Repo '.pio\build'
if ((Get-Command Add-MpPreference -ErrorAction SilentlyContinue) -and (Test-Path $pioBuild)) {
    Add-MpPreference -ExclusionPath $pioBuild -ErrorAction SilentlyContinue | Out-Null
    Write-Host "Defender exclusion: $pioBuild" -ForegroundColor Gray
}
Write-Host ''
Write-Host 'Before upload: hold BtnA (top-left) at power-on if the device still misbehaves after flash.' -ForegroundColor Yellow
Write-Host 'DuckDuckGo VPN on this PC is unchanged (USB flash is local to the Cardputer only).' -ForegroundColor Gray
Write-Host ''

if ($SkipBootloaderBuild) {
    $env:M5OS_SKIP_BOOTLOADER_BUILD = '1'
    Write-Host 'SkipBootloaderBuild: reuse variants\m5os-cardputer\bootloader.bin if present' -ForegroundColor Yellow
}

Push-Location $Repo
try {
    if (-not $SkipBuild) {
        Write-Host '[1/3] Build m5stack-cardputer...' -ForegroundColor Cyan
        & $Pio run -e m5stack-cardputer -j 1
        if ($LASTEXITCODE -ne 0) { throw "Build failed with exit $LASTEXITCODE" }
    } else {
        Write-Host '[1/3] Skip build (-SkipBuild)' -ForegroundColor Yellow
        $fw = Join-Path $Repo '.pio\build\m5stack-cardputer\firmware.bin'
        if (-not (Test-Path -LiteralPath $fw)) { throw 'Missing firmware.bin - run build after freeing disk space.' }
    }

    if (-not $SkipErase) {
        Write-Host '[2/3] Erase flash (fixes bad otadata / partition mismatch)...' -ForegroundColor Cyan
        & $Pio run -e m5stack-cardputer -t erase --upload-port $Port
        if ($LASTEXITCODE -ne 0) { throw "Erase failed with exit $LASTEXITCODE" }
    } else {
        Write-Host '[2/3] Skip erase (-SkipErase)' -ForegroundColor Yellow
    }

    Write-Host '[3/4] Upload M5 OS + custom bootloader...' -ForegroundColor Cyan
    & $Pio run -e m5stack-cardputer -t upload --upload-port $Port
    if ($LASTEXITCODE -ne 0) { throw "Upload failed with exit $LASTEXITCODE" }

    Write-Host '[4/4] Upload session gateway to app1...' -ForegroundColor Cyan
    & "$Repo\scripts\flash_session_gateway.ps1" -Port $Port
    if ($LASTEXITCODE -ne 0) { throw "Gateway flash failed with exit $LASTEXITCODE" }

    Write-Host ''
    Write-Host 'Done. Open serial monitor (115200) to confirm boot:' -ForegroundColor Green
    Write-Host "  $Pio device monitor -p $Port -b 115200"
}
finally {
    Pop-Location
}
