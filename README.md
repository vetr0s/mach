# mach

A game engine and game, co-developed as a single unit. Built in C with SDL3.

**Version:** v0.3.0

## Setup

```bash
./scripts/setup.sh      # One-time: build SDL3
./build.sh              # Build the game
./build/mach_debug      # Run
```

## Controls

- **WASD / Arrows** — pan the camera across the ground
- **Scroll wheel** — zoom in/out
- **1 / 2 / 3** — select Miner / Storage / Delete tool (press again to deselect)
- **Left click** — apply the current tool to the hovered tile
- **Esc** — quit

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

**Engine ÷ Game separation.** The engine (rendering, input, window management) lives in `src/engine/`. The game (entity types, rules, content) lives in `src/game/`. The dependency only points one way: the engine drives the game through the `Engine_App` callback interface (`engine/app.h`) and never names a game type. The game implements those callbacks (`game/app.c`) and hands them to the engine from `main()`. A future game reuses `src/engine/` untouched and writes new `src/game/` code.

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
    render/               # SDL_GPU renderer: gpu, mesh, camera, font (atlas)
    math/                 # Vec2/3/4, Mat4, projections, transforms
    app.h                 # Engine_App interface (engine drives the game via this)
    os.h                  # Platform detection
    ui.h                  # Window context + screen constants
    debug.h               # Assertions, leveled logging

  game/                   # Game-specific code (factory automation sim)
    app.c                 # Implements Engine_App: wires game into the engine
    game.h/.c             # Game state, input, iso camera, tick logic
    render_game.h/.c      # Draws the world as 3D geometry (game-side)
    world/                # Entity management, grid simulation
    
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

## Rendering

The engine renders in **true 3D** on **SDL_GPU** (Metal on macOS, Vulkan/D3D12 elsewhere). Shaders are authored in MSL and compiled at runtime; there is no shader build step.

- **`gpu.{h,c}`** — device, swapchain, depth buffer, a lit 3D pipeline, and a textured 2D overlay pipeline; frame lifecycle and draw calls.
- **`mesh.{h,c}`** — GPU vertex/index buffers and procedural primitives (cube, plane).
- **`camera.{h,c}`** — a projection-agnostic `Camera` (perspective **or** orthographic), view/projection matrices, and mouse-pick rays.
- **`font.{h,c}`** — an 8×8 bitmap font baked into a GPU atlas, drawn as textured quads through the 2D overlay.

**Isometric is a camera configuration, not a baked-in projection.** The factory view is just an orthographic `Camera` aimed down the (1,1,1) axis, with real depth buffering and directional lighting so machines have shape and occlusion. A different project can swap in a perspective or top-down camera and reuse the whole renderer — the engine is not isometric-only.

## Dependencies

**Required:**
- **SDL3** — Windowing, input, rendering. Git submodule; built by `setup.sh`.

**Single-header libraries (stb):**
- **stb_image.h** — Image loading (PNG, BMP, JPG). Downloaded by `setup.sh`.
- **stb_truetype.h** / **stb_rect_pack.h** — Vendored for future asset/font work; the current font is a hardcoded 8×8 bitmap baked into a GPU atlas (no runtime TrueType).

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
