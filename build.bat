@echo off
REM Build the mach engine with MSVC (cl.exe).
REM Usage: build.bat [debug|release]
REM Defaults to debug. Run from a Visual Studio "x64 Native Tools" prompt.
REM
REM No setup step: RGFW is embedded in mach.h (windowing), GL comes from the
REM OS. MSVC has no /std:c99 switch; the code is C99 and compiles under c11.

setlocal enabledelayedexpansion

set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=debug

if /I "%BUILD_TYPE%"=="debug" (
    set OPT_FLAGS=/Od /Zi
    set SUFFIX=_debug
) else if /I "%BUILD_TYPE%"=="release" (
    set OPT_FLAGS=/O2 /DNDEBUG
    set SUFFIX=_release
) else (
    echo Unknown build type: %BUILD_TYPE%. Use 'debug' or 'release'.
    exit /b 1
)

REM Compile the unity build.
if not exist build mkdir build
set OUT_FILE=build\mach%SUFFIX%.exe

echo Compiling: %OUT_FILE%
cl.exe /std:c11 ^
    %OPT_FLAGS% /W4 ^
    /Isrc ^
    /Fe"%OUT_FILE%" ^
    src\mach.c ^
    /link opengl32.lib winmm.lib

echo Build complete: %OUT_FILE%
