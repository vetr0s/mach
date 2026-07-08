#!/usr/bin/env bash
# Build the mach engine with platform-native compiler toolchain.
# Usage: ./build.sh [debug|release|hot|game]
# Defaults to debug.

set -euo pipefail

# Platform toolchain and the libs RGFW + OpenGL need. No setup step: RGFW is
# embedded in mach.h (windowing), GL comes from the OS.
os_name="$(uname -s)"

case "$os_name" in
  Darwin)
    cc="clang"
    libs="-framework Cocoa -framework CoreVideo -framework IOKit -framework OpenGL"
    lib_ext=".dylib"
    shared_flags="-dynamiclib"
    ;;
  Linux)
    cc="clang"
    libs="-lX11 -lXrandr -lGL -lm -ldl"
    lib_ext=".so"
    shared_flags="-shared -fPIC"

    # Fail early with install hints when the X11/GL dev headers are missing.
    # The usual first-run failure on a fresh box (RGFW needs Xlib, Xrandr,
    # Xcursor, and GLX headers; the libs themselves ship with the OS).
    if ! printf '%s\n' \
        '#include <X11/Xlib.h>' \
        '#include <X11/Xcursor/Xcursor.h>' \
        '#include <X11/extensions/Xrandr.h>' \
        '#include <X11/extensions/sync.h>' \
        '#include <GL/glx.h>' \
        | "$cc" -fsyntax-only -x c - 2>/dev/null; then
      cat >&2 <<'EOF'
Missing X11/OpenGL development headers. Install them:
  Void:          sudo xbps-install -S libX11-devel libXrandr-devel libXcursor-devel libglvnd-devel
  Debian/Ubuntu: sudo apt install libx11-dev libxrandr-dev libxcursor-dev libgl1-mesa-dev
  Fedora:        sudo dnf install libX11-devel libXrandr-devel libXcursor-devel mesa-libGL-devel
  Arch:          sudo pacman -S libx11 libxrandr libxcursor mesa
EOF
      exit 1
    fi
    ;;
  *)
    echo "Unsupported platform: $os_name" >&2
    exit 1
    ;;
esac

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
common_flags="-std=c99 -Wall -Wextra -Isrc"

# Build the hot-reloadable game library from src/game_lib.c.
build_game_lib() {
  echo "Compiling: $game_lib"
  # shellcheck disable=SC2086
  $cc $common_flags -g -O0 $shared_flags \
    -o "$game_lib" src/game_lib.c $libs
}

case "$build_type" in
  debug|release)
    if [ "$build_type" = release ]; then opt_flags="-O3 -DNDEBUG"; suffix="_release"
    else opt_flags="-g -O0"; suffix="_debug"; fi
    out_file="build/mach${suffix}"
    echo "Compiling: $out_file"
    # shellcheck disable=SC2086
    $cc $common_flags $opt_flags -o "$out_file" src/mach.c $libs
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
    $cc $common_flags -g -O0 \
      -DGAME_LIB_PATH="\"$game_lib\"" \
      -o "$out_file" src/host.c $libs
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
      # (npt): No `| head -1` here. Under pipefail, find dying of SIGPIPE when
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
