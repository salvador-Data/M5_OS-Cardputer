# Write session gateway firmware to app1 @ 0x400000 (not inside app0).
param(
    [string]$Port = "COM13"
)
$ErrorActionPreference = "Stop"
$pio = Join-Path $env:USERPROFILE ".platformio\penv\Scripts\pio.exe"
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
& "$root\scripts\build_session_gateway.ps1"
$bin = Join-Path $root ".pio\build\m5os-session-gateway\firmware.bin"
$esptool = Join-Path $env:USERPROFILE ".platformio\packages\tool-esptoolpy\esptool.py"
if (-not (Test-Path $esptool)) {
    & $pio pkg install -g -p espressif32 tool-esptoolpy
}
python $esptool --chip esp32s3 --port $Port write_flash 0x400000 $bin
Write-Host "Gateway written to app1 @ 0x400000 on $Port"
