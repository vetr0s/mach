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
    ld_flags="-Wl,-rpath,@loader_path"
    lib_ext=".dylib"
    shared_flags="-dynamiclib"
    dl_lib=""              # dlopen lives in libSystem
    ;;
  Linux)
    platform="${os_name}-${machine}"
    cc="clang"
    ld_flags="-Wl,-rpath,\$ORIGIN"
    lib_ext=".so"
    shared_flags="-shared -fPIC"
    dl_lib="-ldl"
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

# Build target:
#   debug   (default)  static monolith, -g -O0   -> build/mach_debug
#   release            static monolith, -O3       -> build/mach_release
#   hot                dev host + game library (hot reload) -> build/mach_hot (+ lib)
#   game               rebuild only the game library (for the reload loop)
build_type="${1:-debug}"

mkdir -p build

game_lib="build/libmach_game${lib_ext}"
common_flags="-std=c11 -Wall -Wextra -I$SDL_PREFIX/include -L$SDL_PREFIX/lib"

# Build the hot-reloadable game library from src/game_lib.c.
build_game_lib() {
  echo "Compiling: $game_lib"
  # shellcheck disable=SC2086
  $cc $common_flags -g -O0 $shared_flags $ld_flags \
    -o "$game_lib" src/game_lib.c -lSDL3
}

# Copy the SDL3 shared library next to the binaries so they run without LD_LIBRARY_PATH.
copy_sdl() {
  case "$os_name" in
    Darwin) cp -f "$SDL_PREFIX/lib/libSDL3.0.dylib" "build/" ;;
    Linux)  cp -f "$SDL_PREFIX/lib/libSDL3.so" "build/" ;;
  esac
}

case "$build_type" in
  debug|release)
    if [ "$build_type" = release ]; then opt_flags="-O3 -DNDEBUG"; suffix="_release"
    else opt_flags="-g -O0"; suffix="_debug"; fi
    out_file="build/mach${suffix}"
    echo "Compiling: $out_file"
    # shellcheck disable=SC2086
    $cc $common_flags $opt_flags $ld_flags -o "$out_file" src/mach.c -lSDL3
    copy_sdl
    echo "Build complete: $out_file"
    ;;
  game)
    # Rebuild just the library; a running `mach_hot` picks it up on save.
    build_game_lib
    echo "Build complete: $game_lib"
    ;;
  hot)
    build_game_lib
    out_file="build/mach_hot"
    echo "Compiling: $out_file"
    # shellcheck disable=SC2086
    $cc $common_flags -g -O0 $ld_flags \
      -DGAME_LIB_PATH="\"$game_lib\"" \
      -o "$out_file" src/host.c -lSDL3 $dl_lib
    copy_sdl
    echo "Build complete: $out_file (run it, then \`./build.sh game\` to hot reload)"
    ;;
  *)
    echo "Unknown build type: $build_type. Use debug | release | hot | game." >&2
    exit 1
    ;;
esac
