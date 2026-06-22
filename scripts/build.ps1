# Convenience wrapper: configure + build a preset.
#   scripts/build.ps1 [preset]
# Defaults to windows-debug. Run from a Visual Studio "x64 Native Tools" prompt
# so the MSVC toolchain is on PATH.
param(
    [string]$Preset = "windows-debug"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

cmake --preset $Preset
cmake --build --preset $Preset
