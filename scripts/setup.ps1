# Initialize SDL3 submodule and build it once to a known install prefix (Windows).
# Run once after cloning from a Visual Studio "x64 Native Tools" prompt.

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

# Init the SDL3 submodule.
Write-Host "Initializing SDL3 submodule..."
git submodule update --init --recursive

# Windows-specific platform tag.
$platform = "windows-x86_64"

$SDL_SOURCE = "third_party\SDL"
$SDL_BUILD_DIR = Join-Path $SDL_SOURCE "build\$platform"
$SDL_INSTALL_PREFIX = Join-Path $SDL_SOURCE "install\$platform"

# Build SDL3 with cmake (one-time, explicit configuration).
Write-Host "Building SDL3 for $platform..."
if (-not (Test-Path $SDL_BUILD_DIR)) {
    New-Item -ItemType Directory -Path $SDL_BUILD_DIR -Force | Out-Null
}

cmake -S $SDL_SOURCE -B $SDL_BUILD_DIR `
  -DCMAKE_INSTALL_PREFIX="$SDL_INSTALL_PREFIX" `
  -DSDL_SHARED=ON `
  -DSDL_STATIC=OFF `
  -DSDL_TEST_LIBRARY=OFF `
  -DSDL_TESTS=OFF `
  -DSDL_EXAMPLES=OFF

cmake --build $SDL_BUILD_DIR --config Release
cmake --install $SDL_BUILD_DIR --config Release

Write-Host "SDL3 ready at $SDL_INSTALL_PREFIX" -ForegroundColor Green
Write-Host "Next: .\build.bat" -ForegroundColor Green
