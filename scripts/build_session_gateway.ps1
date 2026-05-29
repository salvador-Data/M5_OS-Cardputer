# Build session gateway and copy firmware to data/ + SD seed path.
$ErrorActionPreference = "Stop"
$pio = Join-Path $env:USERPROFILE ".platformio\penv\Scripts\pio.exe"
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
& $pio run -e m5os-session-gateway
$src = Join-Path $root ".pio\build\m5os-session-gateway\firmware.bin"
$dataDir = Join-Path $root "data"
$null = New-Item -ItemType Directory -Force -Path $dataDir
Copy-Item -Force $src (Join-Path $dataDir "m5os_session_gateway.bin")
Write-Host "Gateway bin: $dataDir\m5os_session_gateway.bin"
