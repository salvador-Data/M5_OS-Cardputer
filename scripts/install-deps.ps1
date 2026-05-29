# Install Python packages into PlatformIO Core's venv (Windows).
# Required for esptool.py when building bootloader.bin (intelhex).
# CI installs the same deps in .github/workflows/build.yml.

$ErrorActionPreference = "Stop"

$pip = Join-Path $env:USERPROFILE ".platformio\penv\Scripts\pip.exe"
if (-not (Test-Path $pip)) {
    Write-Error @"
PlatformIO pip not found at:
  $pip
Install PlatformIO Core (CLI) or the VS Code PlatformIO extension, then re-run this script.
"@
}

Write-Host "Installing intelhex into PlatformIO penv..."
& $pip install intelhex

Write-Host "Done. Build with:"
Write-Host '  & "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run -e m5stack-cardputer'
