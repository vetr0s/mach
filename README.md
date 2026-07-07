# mach

A game engine and the game it exists to run, built together as one thing. Pure C99,
three single-header libraries, and not much else.

![mach — isometric factory scene rendered by the engine](assets/mach-engine-screenshot.png)

**Version:** v0.5.1

It's a small **2D isometric** engine driving a factory/automation game. RGFW
opens the window and delivers input; on top of that sits the engine's own little
OpenGL batch renderer — one shader, one draw stream, done. No engine framework,
no GPU pipeline, no offline shader tooling. (Real 3D comes back when there's an
actual reason for it and not a day sooner; see `ARCHITECTURE.md` for why.)

## Building it

You need a C compiler. That's the whole list — every dependency is a single
header committed to `third_party/`, and OpenGL comes from the OS. There is no
setup step.

**macOS / Linux** (bash):
```bash
./build.sh              # build the game
./build/mach_debug      # run it
```

**Windows** — from a *Visual Studio "x64 Native Tools"* `cmd` prompt:
```bat
build.bat               :: build the game
build\mach_debug.exe    :: run it
```

### What you actually need

- **A C compiler.** The renderer is a few hundred lines of OpenGL 3.3 core,
  loaded at runtime. No shader toolchain to install, no GPU SDK, nothing extra.
- **RGFW**, vendored in `third_party/rgfw/`: a single-header windowing library —
  window, GL context, keyboard, mouse. What SDL used to do here, minus the
  building of SDL.
- **stb**, vendored in `third_party/stb/`: `stb_image.h` for loading sprite art.
  Public domain, single header, nothing to build or fetch.
- **Clay**, vendored in `third_party/clay/`: a single-header C UI layout library that
  drives the HUD. Clay does layout; the engine walks its render commands and draws them
  with `render2d` (the `clay_ui` section of `mach.h`). Nothing to build or fetch.

## Playing with it

The game is a value loop: droppers spit out items, conveyors carry them through
upgraders that raise their worth, and collectors bank it. Build a route that runs
items past as many upgraders as you can before they get collected. Each upgrader
only counts once per item, so the puzzle is reaching lots of different ones.

- **WASD / Arrows** — pan the camera
- **Scroll wheel** — zoom
- **1 / 2 / 3 / 4 / 5** — pick Dropper / Conveyor / Upgrader / Collector / Delete (hit it again to drop it)
- **R** — rotate the piece you're hovering over in place; over an empty tile it rotates the facing of the next piece you place
- **Left click** — place the current tool on the tile you're hovering
- **Space** — pause / resume the simulation (freezes the world; you can still build and pan)
- **F3** — toggle the debug/info overlay (FPS, selected tool/facing, tick + entity/item counts, hover cell, camera). Money is always on screen; everything else lives here.
- **Esc** — quit

## Hacking on it

If you use Emacs, generate tags:
```bash
./scripts/tags.sh
```
and point your init at them:
```elisp
(setq tags-file-name "/path/to/mach/TAGS")
(define-key global-map "\C-]" 'find-tag)
(define-key global-map "\C-\M-]" 'pop-tag-mark)
```
`C-]` jumps to a definition, `C-M-]` jumps back.

## Why it's built this way

**Pure C, no frameworks.** RGFW handles the window and input, OpenGL puts pixels
on screen. Past that it's just C99.

**Unity build.** The whole thing compiles in one `clang` call. `mach.c` includes
every other `.c` file and the compiler sees it all at once. There's no build
system to speak of; `build.sh` is the compiler invocation. If that sounds strange,
go watch some Handmade Hero and look at how RAD Debugger builds. It's freeing.

**Engine and game, kept apart.** The engine is one header, `mach.h`, in the
stb style: declarations always, implementation in the one translation unit that
defines `MACH_IMPLEMENTATION`. The game (entities, rules, content) lives in
`src/game/`. The dependency only ever points one way: **`mach.h` never names a
game type.** The game owns the loop in `main()` and calls into the engine,
raylib-style; the engine drives nothing on its own. Swap in a different game
and `mach.h` doesn't have to notice.

**Minimal, and 2D on purpose.** Isometric is a coordinate transform, not a 3D
projection. The "3D look" is faked with shaded, outlined blocks. The engine stays
small and gets new capability only when something concrete actually pulls it in.

**Fat-struct ECS.** No generic component system, no query engine. Each entity type
is a full struct (`Entity_Miner`, `Entity_Storage`) and the game logic touches the
data directly. The idea is lifted from Anton Mikhailov on the *Wookash Podcast*.

## Where it runs

- macOS — primary, this is where it's developed and tested
- Linux — toolchain's wired, needs a real run on real hardware
- Windows — same story

The cross-platform story is boring in the best way: RGFW is one header that
speaks Win32, X11, and Cocoa; the renderer is GL 3.3 core, which all three
platforms still ship. Bring a C compiler, go.

## How the code is laid out

```
mach.h                    # the engine: one header, stb-style, C99

src/
  game/                   # the factory sim
    game.h/.c             # game state, input, the sim, and the four loop entry points
    render_game.h/.c      # draws the world (iso tiles, shaded blocks) and the HUD
    world/                # entities and the grid simulation

  mach.c                  # unity root: MACH_IMPLEMENTATION + game sources + main()

build.sh / build.bat      # the compiler invocation (macOS-Linux / Windows)
third_party/              # rgfw, clay, stb — single headers, committed
```

`ARCHITECTURE.md` has the longer version of why any of this is the way it is.

## The entity system

Entities are **fat structs**, not generic bags of components:
```c
typedef struct {
    i32 grid_x, grid_y;
    Direction dir;        // upgraders move items too
    i32 upgrader_id;      // the bit it sets in an item's "already upgraded" mask
} Entity_Upgrader;
```

The `World` keeps entities and items in flat arrays, with grid indices for "what's
on this cell":
```c
typedef struct {
    Entity entities[MAX_ENTITIES];
    i32 entity_count;
    i32 grid[256][256];       // entity id at each cell, or 0 for empty

    Item items[MAX_ITEMS];
    i32 item_grid[256][256];  // item id at each cell, or 0 for empty
    i64 money;
    i32 tick;
} World;
```

Game code just loops over the arrays and updates things. No indirection to chase,
no query system to fight, and the performance is whatever you can read off the
page. The whole `World` is a single allocation out of an arena.

## The rendering

It's all **2D on a small GL batch renderer**, the render sections of `mach.h`:

- **`render2d`** — the actual render layer: one shader, one stream of
  textured vertex-colored triangles, flushed per texture/scissor change. Filled
  rects, convex polygons, outlines, text, sprite loading and drawing, and a 2D
  pan/zoom `Mach_Camera2D`. Plus the isometric transforms, `iso_to_screen` and
  `screen_to_iso`.
- **`gl`** — the ~40 GL 3.3 core entry points the renderer uses, declared by
  hand and loaded at runtime into the `Mach_Renderer` struct. No system GL headers.
- **`font`** — an 8×8 bitmap font baked into a GL texture atlas, tinted
  per vertex.
- **`image`** — the `stb_image` loader for sprite art.

**Isometric is a coordinate transform, not a projection.** The grid maps to 2:1
diamond tiles. Machines are **shaded blocks** (a bright top, two darker side faces,
outlined edges) sorted back-to-front. That reads as depth without a single line of
3D code. Want a top-down or free 2D camera instead? That's just a different
transform; nothing here is wired to be isometric-only.

## Standing on other people's shoulders

The ideas:
- **RAD Debugger (raddbg)** — the minimal build system and unity compilation.
  https://github.com/EpicGames/raddebugger
- **Handmade Hero** — pure C, simple architecture, suspicion of abstraction.
  https://handmadehero.org
- **Wookash Podcast** — Anton Mikhailov on engine design and the fat-struct ECS.
  https://www.youtube.com/channel/UC9J9u3apteD0EuFjzRpt71w

The libraries:
- **RGFW** (Riley Mabb) — https://github.com/ColleagueRiley/RGFW
- **stb** (Sean Barrett) — https://github.com/nothings/stb
- **Clay** (Nic Barker) — https://github.com/nicbarker/clay

## Licensing

- **mach engine and game** — Zlib (see `LICENSE`)
- **RGFW** — Zlib (see the header)
- **stb** — public domain (see each header)
- **Clay** — Zlib (see the header)
