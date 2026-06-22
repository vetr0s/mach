# mach

Machina Game Engine using C++ & SDL3 (embedded Lua and a level editor to come).

## Prerequisites

- CMake ≥ 3.21
- Ninja
- A C++17 compiler (Apple clang / clang / gcc / MSVC)

## Getting started

```sh
# 1. Fetch the vendored SDL3 source (git submodule)
scripts/setup.sh        # or: git submodule update --init --recursive

# 2. Configure + build (pick the preset for your platform)
cmake --preset macos-debug
cmake --build --preset macos-debug
```

Or use the wrapper, which picks a sensible default preset for the host:

```sh
scripts/build.sh                 # macOS / Linux
scripts/build.ps1                # Windows (from a VS "x64 Native Tools" prompt)
```

Available presets: `macos-debug`, `macos-release`, `linux-debug`,
`linux-release`, `windows-debug`, `windows-release`.

The runnable binary lands in `build/<preset>/bin/` (e.g.
`build/macos-debug/bin/mach_game`), with the SDL3 shared library copied beside
it.

## How the build works

It's a CMake **superbuild**. SDL3 is vendored as a git submodule under
`third_party/SDL`, built once as a **shared** library, and installed to a known
per-platform prefix (`third_party/SDL/install/<platform>`). The engine is then
built as a separate step that consumes SDL3 via `find_package(SDL3 CONFIG)`.

This keeps the dependency build independent of the engine: editing engine code
never rebuilds SDL3.

## Layout

```
CMakeLists.txt        # dual-mode root: superbuild | engine
CMakePresets.json
cmake/                # Superbuild.cmake, CompilerWarnings.cmake
src/                  # engine library (mach) + sample executable (mach_game)
third_party/SDL/      # SDL3 submodule (pinned release tag)
scripts/              # setup + build wrappers
```
