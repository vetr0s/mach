#!/usr/bin/env bash
# Build the mach engine with platform-native compiler toolchain.
# Usage: ./build.sh [debug|release]
# Defaults to debug.

set -euo pipefail

# Derive platform tag (same scheme as SDL3 install prefix).
os_name="$(uname -s)"
machine="$(uname -m)"

case "$os_name" in
  Darwin)
    platform="${os_name}-${machine}"
    cc="clang"
    ;;
  Linux)
    platform="${os_name}-${machine}"
    cc="clang"
    ;;
  *)
    echo "Unsupported platform: $os_name" >&2
    exit 1
    ;;
esac

# SDL3 install prefix (must exist; run ./scripts/setup.sh first).
SDL_PREFIX="third_party/SDL/install/${platform}"
if [ ! -d "$SDL_PREFIX" ]; then
  echo "SDL3 not found at $SDL_PREFIX. Run ./scripts/setup.sh first." >&2
  exit 1
fi

# Build configuration (debug or release).
build_type="${1:-debug}"
case "$build_type" in
  debug)
    opt_flags="-g -O0"
    suffix="_debug"
    ;;
  release)
    opt_flags="-O3 -DNDEBUG"
    suffix="_release"
    ;;
  *)
    echo "Unknown build type: $build_type. Use 'debug' or 'release'." >&2
    exit 1
    ;;
esac

# Compile the unity build.
mkdir -p build
out_file="build/mach${suffix}"
echo "Compiling: $out_file"
$cc -std=c11 \
  $opt_flags \
  -Wall -Wextra \
  -I"$SDL_PREFIX/include" \
  -L"$SDL_PREFIX/lib" \
  -o "$out_file" \
  src/mach.c \
  -lSDL3

# Copy the SDL3 shared library next to the binary so it runs without LD_LIBRARY_PATH.
case "$os_name" in
  Darwin)
    cp -f "$SDL_PREFIX/lib/libSDL3.0.dylib" "build/"
    ;;
  Linux)
    cp -f "$SDL_PREFIX/lib/libSDL3.so" "build/"
    ;;
esac

echo "Build complete: $out_file"
