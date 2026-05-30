# Build M5 OS + session gateway, then flash app0 and app1 over USB.
param(
    [string]$Port = "COM13",
    [switch]$SkipBuild
)
$ErrorActionPreference = "Stop"
$pio = Join-Path $env:USERPROFILE ".platformio\penv\Scripts\pio.exe"
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
if (-not $SkipBuild) {
    & $pio run -e m5stack-cardputer -j 1
    if ($LASTEXITCODE -ne 0) { throw "M5 OS build failed" }
}
& $pio run -e m5stack-cardputer -t upload-all --upload-port $Port
if ($LASTEXITCODE -ne 0) { throw "upload-all failed" }
Write-Host "Flashed M5 OS (app0) + session gateway (app1) on $Port"
