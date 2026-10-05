@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0BUILD_VISUAL_SNAPSHOTS.ps1"
set "rc=%ERRORLEVEL%"
if not "%rc%"=="0" echo SpaceTrace visual snapshot build failed with exit code %rc%.
exit /b %rc%
