# mach

A game engine and game, co-developed as a single unit. Built in C with SDL3.

**Version:** v0.2.0

## Setup

```bash
./scripts/setup.sh      # One-time: build SDL3
./build.sh              # Build the game
./build/mach_debug      # Run
```

Press **Esc** or close the window to exit.

## Development

Generate tags for Emacs:
```bash
./scripts/tags.sh
```

Then add to your Emacs init:
```elisp
(setq tags-file-name "/path/to/mach/TAGS")
(define-key global-map "\C-]" 'find-tag)
(define-key global-map "\C-\M-]" 'pop-tag-mark)
```

Now you can:
- `C-]` to jump to definition
- `C-M-]` to go back

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
  mach.c                  Unity root, includes all modules
  base/                   Fundamental types and utilities
  os/                     Platform abstraction layer
  ui/                     Window, renderer, event handling
  core/                   Engine lifecycle
  game/                   Game-specific logic
  debug/                  Debug utilities (assertions, logging)
third_party/SDL/          SDL3 (git submodule)
```
