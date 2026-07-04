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
#   hot                the dev loop: build host + game library, run the host, then
#                      watch src/ and rebuild the library on every change so the
#                      running game swaps it in live. Ctrl-C (or closing the game)
#                      stops the watch.
#   game               rebuild only the game library (one shot, for manual reloads)
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
    echo "Build complete: $out_file"

    # Now become the dev loop: run the host and watch the sources, rebuilding the
    # game library on every change so the running host swaps it in live. Polling
    # with find -newer keeps this dependency-free and portable (no fswatch).
    "$out_file" &
    game_pid=$!
    trap 'kill "$game_pid" 2>/dev/null; exit 0' INT TERM

    stamp="build/.hot_stamp"
    touch "$stamp"
    echo "Watching src/ for changes (Ctrl-C or close the game to stop)"
    while kill -0 "$game_pid" 2>/dev/null; do
      sleep 0.5
      # (npt): No `| head -1` here — under pipefail, find dying of SIGPIPE when
      # many files changed would kill the whole watch. Take the list, keep line 1.
      changed_list="$(find src -type f \( -name '*.c' -o -name '*.h' \) -newer "$stamp")"
      changed="${changed_list%%$'\n'*}"
      [ -z "$changed" ] && continue
      # Re-stamp before compiling so edits made mid-build trigger another pass.
      touch "$stamp"
      echo "Changed: $changed"
      if [ "$changed" = "src/host.c" ]; then
        echo "note: host.c is not hot-reloadable; restart \`./build.sh hot\` to pick it up"
      fi
      if build_game_lib; then
        echo "Reload ready ($(date +%H:%M:%S)); the running game picks it up"
      else
        echo "Build failed; the game keeps the last good code. Still watching."
      fi
    done
    wait "$game_pid" 2>/dev/null || true
    echo "Game exited; watch stopped."
    ;;
  *)
    echo "Unknown build type: $build_type. Use debug | release | hot | game." >&2
    exit 1
    ;;
esac
