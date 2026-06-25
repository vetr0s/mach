#!/usr/bin/env bash
# Initialize SDL3 submodule and build it once to a known install prefix.
# Run once after cloning; the engine build scripts then link against it.

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

# Init the SDL3 submodule.
echo "Initializing SDL3 submodule..."
git submodule update --init --recursive

# Derive platform tag.
os_name="$(uname -s)"
machine="$(uname -m)"
platform="$(echo "$os_name-$machine" | tr '[:upper:]' '[:lower:]')"

SDL_SOURCE="third_party/SDL"
SDL_BUILD_DIR="$SDL_SOURCE/build/$platform"
SDL_INSTALL_PREFIX="$SDL_SOURCE/install/$platform"

# Build SDL3 with cmake (one-time, explicit configuration).
echo "Building SDL3 for $platform..."
mkdir -p "$SDL_BUILD_DIR"
cmake -S "$SDL_SOURCE" -B "$SDL_BUILD_DIR" \
  -DCMAKE_INSTALL_PREFIX="$SDL_INSTALL_PREFIX" \
  -DSDL_SHARED=ON \
  -DSDL_STATIC=OFF \
  -DSDL_TEST_LIBRARY=OFF \
  -DSDL_TESTS=OFF \
  -DSDL_EXAMPLES=OFF

cmake --build "$SDL_BUILD_DIR" --config Release
cmake --install "$SDL_BUILD_DIR" --config Release

echo "SDL3 ready at $SDL_INSTALL_PREFIX"
echo "Next: ./build.sh"
