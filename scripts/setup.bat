@echo off
REM Convenience wrapper so setup runs from a plain elevated cmd prompt without
REM typing the PowerShell incantation. Forwards args to setup.ps1.
REM %~dp0 is this script's own folder, so it works from any cwd.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup.ps1" %*
exit /b %ERRORLEVEL%
