# Fetch vendored dependencies (SDL3) after a fresh clone.
$ErrorActionPreference = "Stop"

git submodule update --init --recursive
Write-Host "Submodules ready. Next: cmake --preset windows-debug && cmake --build --preset windows-debug" -ForegroundColor Green
