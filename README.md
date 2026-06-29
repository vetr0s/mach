# mach

A game engine and game, co-developed as a single unit. Built in C with SDL3.

**Version:** v0.5.0

A minimal **2D isometric** engine driving a factory/automation game. Rendering is
pure 2D over SDL_Renderer — no shaders, no GPU pipeline code, no offline shader
tooling. (Real 3D is deferred until it's actually needed; see `ARCHITECTURE.md`.)

## Setup

**macOS / Linux** (bash):
```bash
./scripts/setup.sh      # One-time: build SDL3, fetch stb
./build.sh              # Build the game
./build/mach_debug      # Run
```

**Windows** — from an elevated *Visual Studio "x64 Native Tools"* `cmd` prompt:
```bat
scripts\setup.bat       :: One-time: build SDL3 (cmake + MSVC), fetch stb
build.bat               :: Build the game
build\mach_debug.exe    :: Run
```
The `.bat` files are thin wrappers around the PowerShell scripts.

### Dependencies

- **A C compiler + SDL3** is all you need to clone, build, and run. SDL3 is a
  git submodule built once by `setup`. SDL_Renderer (part of SDL3) provides
  hardware-accelerated 2D on the native backend (Metal/Vulkan/D3D) — **no shader
  toolchain, no GPU SDK, nothing extra to install.**
- **stb** single-header libraries are fetched by `setup`: `stb_image.h` for
  loading sprite/texture art. Public domain.

## Controls

- **WASD / Arrows** — pan the camera across the grid
- **Scroll wheel** — zoom in/out
- **1 / 2 / 3** — select Miner / Storage / Delete tool (press again to deselect)
- **Left click** — apply the current tool to the hovered tile
- **Esc** — quit

## Development

Generate tags for Emacs:
```bash
./scripts/tags.sh
```
Then in your Emacs init:
```elisp
(setq tags-file-name "/path/to/mach/TAGS")
(define-key global-map "\C-]" 'find-tag)
(define-key global-map "\C-\M-]" 'pop-tag-mark)
```
`C-]` jumps to a definition, `C-M-]` goes back.

## Philosophy

**Pure C, no frameworks.** SDL3 for windowing/input/rendering only.

**Unity build.** All code compiles in a single `clang` invocation. No separate
build system — just `build.sh` calling the compiler directly. Inspired by
**RADDBG** and **Handmade Hero**.

**Engine ÷ Game separation.** The engine (rendering, input, window management)
lives in `src/engine/`. The game (entity types, rules, content) lives in
`src/game/`. The dependency points one way: **`src/engine/` never names a game
type.** The game owns the loop in `main()` and calls into the engine (raylib-style);
the engine drives nothing. A future game reuses `src/engine/` untouched.

**Minimal & 2D.** SDL_Renderer for 2D; isometric is a coordinate transform, not a
3D projection. The "3D look" is faked with shaded blocks. The engine stays small
and opinionated; capability is added only when a concrete need pulls it in.

**Fat struct ECS.** No generic component system. Each entity type is a full struct
(e.g., `Entity_Miner`, `Entity_Storage`). Game logic accesses entity data directly.
Inspired by Anton Mikhailov's approach on the *Wookash Podcast*.

## Platforms

- macOS (primary development)
- Linux (scaffolding in place)
- Windows (scaffolding in place)

## Architecture

```
src/
  engine/                 # Reusable game engine
    base/                 # Fundamental types (i32, f32, Vec2, etc.)
    math/                 # Vec2/3/4, Mat4, transforms (Vec2 + iso used by 2D)
    core/                 # Frame loop steps, timing, window lifecycle
    render/               # 2D renderer: render2d (SDL_Renderer + iso), font, image
    os.h                  # Platform detection
    ui.h                  # Window context + screen constants
    debug.h               # Assertions, leveled logging

  game/                   # Game-specific code (factory automation sim)
    app.c                 # Glue: bridges the engine API and the game internals
    game.h/.c             # Game state, input, 2D iso camera, hover-pick
    render_game.h/.c      # Draws the world as iso tiles + shaded blocks
    world/                # Entity management, grid simulation

  mach.c                  # Unity root: includes engine + game, defines main()

build.sh / build.bat      # Compiler invocation (macOS-Linux / Windows)
scripts/setup.sh / .ps1   # SDL3 build + stb fetch, run once
third_party/SDL/          # SDL3 submodule
```

See **`ARCHITECTURE.md`** for the engine thesis and design rationale.

## Entity System

Entities are **fat structs**, not generic components:
```c
typedef struct {
    i32 grid_x, grid_y;
    i32 ore_stored;
    i32 ore_capacity;
} Entity_Storage;
```

The `World` tracks all entities in direct arrays plus a grid spatial index:
```c
typedef struct {
    Entity entities[MAX_ENTITIES];
    i32 entity_count;
    i32 grid[256][256];  // what's at each grid cell (entity id, or 0)
    i32 tick;
} World;
```

Game code directly iterates and updates entities. No indirection, no query
systems — readable logic and predictable performance.

## Rendering

Pure **2D over SDL_Renderer** (`src/engine/render/`):

- **`render2d.{h,c}`** — the render layer: clear/present, filled rects and convex
  polygons (`SDL_RenderGeometry`), text, sprite loading/drawing, and a 2D pan+zoom
  `Camera2D`. Plus isometric transforms (`iso_to_screen` / `screen_to_iso`).
- **`font.{h,c}`** — an 8×8 bitmap font baked into an `SDL_Texture` atlas, tinted
  per-draw with color mod.
- **`image.{h,c}`** — `stb_image` loader for sprite/texture art.

**Isometric is a coordinate transform, not a projection.** The grid maps to 2:1
diamond tiles; machines are drawn as **shaded blocks** (bright top + two darker
side faces) sorted back-to-front, which reads as 3D depth without any 3D. Swapping
to a top-down or free 2D camera is just a different transform — the renderer isn't
isometric-only.

## References

**Inspirations and design philosophy:**
- **RAD Debugger (raddbg)** — Minimal build system, unity compilation. https://github.com/EpicGames/raddebugger
- **Handmade Hero** — Pure C, simple architecture, avoid over-abstraction. https://handmadehero.org
- **Wookash Podcast** — Anton Mikhailov on engine design, fat struct ECS. https://www.youtube.com/channel/UC9J9u3apteD0EuFjzRpt71w

**External libraries:**
- **SDL3** — https://github.com/libsdl-org/SDL
- **stb** (Sean Barrett) — https://github.com/nothings/stb

## Licensing

- **mach engine & game** — MIT license
- **SDL3** — Zlib license (see `third_party/SDL/LICENSE.txt`)
- **stb** — Public domain (see header comments in each .h file)
