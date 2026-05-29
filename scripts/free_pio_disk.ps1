<#
.SYNOPSIS
  Free disk space for PlatformIO builds (safe cache/temp cleanup).
#>
$ErrorActionPreference = 'Continue'
$targets = @(
    'C:\Users\Owner\.platformio\penv\.cache'
    'C:\Users\Administrator\.platformio'
    'C:\Users\Owner\Projects\M5_OS-Cardputer\.pio\build\m5stack-cardputer-bootloader'
)
foreach ($t in $targets) {
    if (Test-Path -LiteralPath $t) {
        $size = (Get-ChildItem -LiteralPath $t -Recurse -Force -EA SilentlyContinue | Measure-Object -Property Length -Sum).Sum
        Remove-Item -LiteralPath $t -Recurse -Force -EA SilentlyContinue
        $mb = if ($size) { [math]::Round($size / 1MB, 1) } else { 0 }
        Write-Host "Removed $t (~${mb} MB)" -ForegroundColor Green
    }
}
$free = (Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='C:'").FreeSpace / 1GB
Write-Host ("C: free now: {0:N2} GB" -f $free) -ForegroundColor Cyan
