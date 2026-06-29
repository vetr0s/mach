@echo off
REM Build the mach engine with MSVC (cl.exe).
REM Usage: build.bat [debug|release]
REM Defaults to debug. Run from a Visual Studio "x64 Native Tools" prompt.

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

REM SDL3 install prefix (windows-x86_64 or windows-amd64 depending on target).
set SDL_PREFIX=third_party\SDL\install\windows-x86_64

if not exist "%SDL_PREFIX%" (
    echo SDL3 not found at %SDL_PREFIX%. Run scripts\setup.ps1 first.
    exit /b 1
)

REM Compile the unity build.
if not exist build mkdir build
set OUT_FILE=build\mach%SUFFIX%.exe

echo Compiling: %OUT_FILE%
cl.exe /std:c11 ^
    %OPT_FLAGS% /W4 ^
    /I"%SDL_PREFIX%\include" ^
    /Fe"%OUT_FILE%" ^
    src\mach.c ^
    /link /LIBPATH:"%SDL_PREFIX%\lib" SDL3.lib

REM Copy the SDL3 DLL next to the executable (CMake installs it under \bin).
copy /Y "%SDL_PREFIX%\bin\SDL3.dll" build\

echo Build complete: %OUT_FILE%
