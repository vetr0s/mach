# mach

A game engine and game, co-developed as a single unit. Built in C with SDL3.

## Setup

```bash
./scripts/setup.sh      # One-time: build SDL3
./build.sh              # Build the game
./build/mach_debug      # Run
```

Press **Esc** or close the window to exit.

## What is this

- A game and its engine, built together toward one goal.
- Pure C, no frameworks. SDL3 for windowing/input/rendering.
- Unity build: one `.c` file, one compiler invocation.
- Minimal tooling. The build is a shell script that calls the C compiler.

## Platforms

- macOS (primary development)
- Linux (scaffolding in place)
- Windows (scaffolding in place)

## Directory

```
build.sh                  Compiler invocation
scripts/setup.sh          SDL3 build (run once)
src/
  mach.c                  Game entry point and main loop
  engine.h / engine.c     Engine logic
third_party/SDL/          SDL3 (git submodule)
```
