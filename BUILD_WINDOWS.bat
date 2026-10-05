@echo off
setlocal
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "[void][scriptblock]::Create((Get-Content -Raw '.\FETCH_JUCE_WINDOWS.ps1')); [void][scriptblock]::Create((Get-Content -Raw '.\BUILD_WINDOWS.ps1')); Write-Host 'PowerShell syntax preflight: PASS'" || exit /b 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File ".\BUILD_WINDOWS.ps1"
exit /b %ERRORLEVEL%
