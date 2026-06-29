@echo off
REM Convenience wrapper for shaders.ps1 (regenerate the baked shader header).
REM Run from a prompt with glslangValidator + spirv-cross on PATH.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0shaders.ps1" %*
exit /b %ERRORLEVEL%
