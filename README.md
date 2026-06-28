# mach

A game engine and game, co-developed as a single unit. Built in C with SDL3.

**Version:** v0.2.1

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

## Philosophy

**Pure C, no frameworks.** SDL3 for windowing/input/rendering only.

**Unity build.** All code compiles in a single `clang` invocation. No separate build system—just `build.sh` (a shell script calling the compiler directly). Inspired by **RADDBG** and **Handmade Hero**.

**Engine ÷ Game separation.** The engine (rendering, input, window management) lives in `src/engine/`. The game (entity types, rules, content) lives in `src/game/`. A future game would reuse `src/engine/` and write new `src/game/` code.

**Fat struct ECS.** No generic component system. Each entity type is a full struct (e.g., `Entity_Miner`, `Entity_Storage`). Game logic directly accesses entity data. Inspired by Anton Kaplanyan's approach in *Working on the Wookash podcast*.

**Modular subsystems.** Each subsystem (math, render, input, core) is self-contained. Headers declare the API; implementations are included into the unity root. Follows RADDBG's internal organization style.

## Platforms

- macOS (primary development)
- Linux (scaffolding in place)
- Windows (scaffolding in place)

## Architecture

```
src/
  engine/                 # Reusable game engine
    base/                 # Fundamental types (i32, f32, Vec2, etc.)
    math/                 # Vector math, utilities
    core/                 # Game loop, timing, window lifecycle
    render/               # Drawing primitives, camera, font rendering
    os.h                  # Platform detection
    ui.h                  # Window/renderer context
    debug.h               # Assertions, debug logging
    
  game/                   # Game-specific code (factory automation sim)
    game.h/.c             # Game state and tick logic
    world/                # Entity management, grid simulation
    entities/             # (Future) Miner, conveyor, factory entities
    systems/              # (Future) Resource flow, production rules
    
  mach.c                  # Unity root: includes engine + game, defines main()

build.sh                  # Compiler invocation (macOS/Linux)
build.bat                 # Compiler invocation (Windows)
scripts/setup.sh          # SDL3 build (run once)
third_party/SDL/          # SDL3 submodule
```

## Entity System

Entities are **fat structs**, not generic components:
```c
typedef struct {
    i32 grid_x, grid_y;
    i32 ore_stored;
    i32 ore_capacity;
} Entity_Storage;
```

The `World` tracks all entities in direct arrays and a grid-based spatial index:
```c
typedef struct {
    Entity entities[MAX_ENTITIES];
    i32 entity_count;
    i32 grid[256][256];  // Spatial hash: what's at each grid cell
    i32 tick;
} World;
```

Game code directly iterates and updates entities. No indirection, no query systems. This keeps the game logic readable and performance predictable.

## Dependencies

**Required:**
- **SDL3** — Windowing, input, rendering. Git submodule; built by `setup.sh`.

**Single-header libraries (stb):**
- **stb_image.h** — Image loading (PNG, BMP, JPG). Downloaded by `setup.sh`.
- **stb_truetype.h** — TrueType font rendering. Downloaded by `setup.sh`.
- **stb_rect_pack.h** — Texture atlasing. Downloaded by `setup.sh`.

All stb headers are public domain. See licensing section below.

## References

**Inspirations and design philosophy:**
- **RADDBG** — Minimal build system, unity compilation, internal code organization. https://github.com/iomeone/raddbg
- **Handmade Hero** — Pure C, simple architecture, avoid over-abstraction. https://handmadehero.org
- **Wookash podcast** — Anton Kaplanyan's talks on game engine design, fat struct ECS. https://www.youtube.com/@wookashvideos

**External libraries:**
- **SDL3** — https://github.com/libsdl-org/SDL
- **stb** (Sean Barret) — https://github.com/nothings/stb

## Licensing

- **mach engine & game** — MIT license
- **SDL3** — Zlib license (see `third_party/SDL/LICENSE.txt`)
- **stb** — Public domain (see header comments in each .h file)
