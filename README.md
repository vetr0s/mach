# mach

A factory/automation game in pure C99: droppers spit out ore, conveyors carry
it, upgraders raise its worth, collectors bank it. Built on **mach.h**, the
single-header 2D engine that grew out of this project and now lives in its own
repo. A copy sits in `src/mach.h`, so this repo still builds with nothing but a
C compiler.

[![Linux](https://github.com/vetr0s/mach/actions/workflows/linux.yml/badge.svg)](https://github.com/vetr0s/mach/actions/workflows/linux.yml)
[![macOS](https://github.com/vetr0s/mach/actions/workflows/macos.yml/badge.svg)](https://github.com/vetr0s/mach/actions/workflows/macos.yml)
[![Windows](https://github.com/vetr0s/mach/actions/workflows/windows.yml/badge.svg)](https://github.com/vetr0s/mach/actions/workflows/windows.yml)

> [!WARNING]
> **Personal project, under active development.** This is a hobby project, not a
> commercial or professional product. It changes frequently, isn't stable, and
> carries no support or warranty: expect breaking changes and rough edges.

![mach: isometric factory scene rendered by the engine](assets/mach-engine-screenshot.png)

## Building it

You need a C compiler. That's the whole list. The engine (with windowing, GL,
UI, and image loading embedded) is one committed header at `src/mach.h`, and
OpenGL comes from the OS. There is no setup step.

The build tool is [nob](https://github.com/tsoding/nob.h): a small C program
(`nob.c`) you compile once, after which it rebuilds itself whenever it changes.

**macOS / Linux** (bash):
```bash
cc -o nob nob.c         # bootstrap the build tool (one time)
./nob                   # build the game
./build/mach_debug      # run it
```

**Windows**, from a *Visual Studio "x64 Native Tools"* `cmd` prompt:
```bat
cl nob.c                :: bootstrap the build tool (one time)
nob                     :: build the game
build\mach_debug.exe    :: run it
```

`./nob` takes a target: `debug` (default), `release`, `hot`, or `game`. On Linux
you need the X11/GL dev headers once; `./nob` checks and prints the install
command for your distro.

## Download

Prebuilt binaries are at
[github.com/vetr0s/mach/releases](https://github.com/vetr0s/mach/releases): one
static, self-contained executable per platform, built and published on each git
tag. macOS is universal (arm64 + x86_64), Linux is x86_64 (glibc 2.35+), Windows
is x86_64. Nothing else to install: assets are baked into the binary.

They are unsigned. On macOS, clear the quarantine flag before running:
```bash
xattr -d com.apple.quarantine mach
```
On Windows, SmartScreen will warn; click "More info" then "Run anyway".

## Playing with it

The game is a value loop: droppers spit out ore, conveyors route it, upgraders
raise its value toward a ceiling, and collectors bank it. Ore climbs its
ceiling a fraction per upgrader pass, and each *distinct* upgrader it meets
raises that ceiling. The puzzle is routing loops past as many different
upgraders as you can before cashing out.

| Input | Action |
|---|---|
| WASD / Arrows | Pan the camera |
| Scroll wheel | Zoom |
| 1 / 2 / 3 / 4 / 5 | Pick Dropper / Conveyor / Upgrader / Collector / Delete (press again to drop it) |
| R | Rotate the piece under the cursor in place; over an empty tile, rotate the facing of the next piece placed |
| Left click | Place the current tool on the hovered tile |
| Space | Pause / resume the simulation (freezes the world; you can still build and pan) |
| `` ` `` | Toggle the debug overlay (FPS, tool/facing, tick + entity/item counts, hover cell, camera) |
| Esc | Quit |

Money is always on screen; everything else in that list lives behind the debug
overlay.

## Hacking on it

`./nob hot` is the dev loop: it builds, runs the game, and watches the
sources. Every save rebuilds the game library and the running game swaps it in
live, keeping its state. See `ARCHITECTURE.md` for how.

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

**Pure C, no frameworks.** mach.h handles window, input, and drawing; past that
it's just C99.

**Unity build.** The whole thing compiles in one `clang` call. `mach.c` includes
every other `.c` file and the compiler sees it all at once. `nob.c` is a handful
of compiler invocations, not a build system in the CMake sense. If that sounds
strange, go watch some Handmade Hero and look at how RAD Debugger builds. It's
freeing.

**Engine and game, kept apart, now literally.** The engine is its own project
(the mach.h repo) and this repo consumes it like any other single-header
library. The dependency only ever points one way: the engine never names a game
type. The game owns the loop in `main()` and calls into the engine,
raylib-style. Engine updates arrive by copying a new release into `src/mach.h`.

**Fat-struct ECS.** No generic component system, no query engine. Each entity type
is a full struct and the game logic touches the data directly. The idea is lifted
from Anton Mikhailov on the *Wookash Podcast*.

## Where it runs

| Platform | Status |
|---|---|
| macOS | Primary: the development machine. Universal binary, Apple silicon and Intel |
| Linux | Builds in CI, runs on real hardware. x86_64, glibc 2.35+ |
| Windows | Builds in CI, runs on real hardware. x86_64, no redistributable needed |

The v0.6.2 release binaries were run on all three.

## How the code is laid out

```
src/
  mach.h                  # the engine, one committed header (its own repo has the docs)
  game/                   # the factory sim
    game.h/.c             # game state, config, input, the sim, and the four loop entry points
    render_game.h/.c      # draws the world (iso tiles, shaded blocks) and the HUD
    world/                # entities and the grid simulation

  mach.c                  # unity root: MACH_IMPLEMENTATION + game sources + main()
  game_lib.c / host.c     # the hot-reload pair (dev builds only)

nob.c / nob.h             # the build tool (tsoding's nob), and its vendored header
docs/                     # the game design doc (gdd.typ; docs/VERSION is its version)
```

`ARCHITECTURE.md` has the longer version of why any of this is the way it is.
Engine internals (the batch renderer, the GL loader, arenas) are documented in
the mach.h repo, not here.

## The entity system

Entities are **fat structs**, not generic bags of components:
```c
typedef struct {
    i32 grid_x, grid_y;
    Direction dir;        // upgraders move items too
    i32 upgrader_id;      // the bit it sets in an item's "distinct upgraders" mask
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

## The look

Everything draws through mach.h's 2D batch renderer. **Isometric is a
coordinate transform, not a projection**: the grid maps to 2:1 diamond tiles,
and machines are **shaded blocks** (a bright top, two darker side faces,
outlined edges) sorted back-to-front. That reads as depth without a single line
of 3D code. `render_game.c` composes the engine's primitives into that look;
the HUD is Clay panels drawn through the same renderer.

## Standing on other people's shoulders

| Source | What it gave the project | Link |
|---|---|---|
| RAD Debugger (raddbg) | The minimal build system and unity compilation | https://github.com/EpicGames/raddebugger |
| Handmade Hero | Pure C, simple architecture, suspicion of abstraction | https://handmadehero.org |
| Wookash Podcast | Anton Mikhailov on engine design and the fat-struct ECS | https://www.youtube.com/channel/UC9J9u3apteD0EuFjzRpt71w |

## Version

Current version: **v0.6.2** (see `VERSION`). The game is versioned by git tag;
`VERSION` tracks the same number in-tree. `docs/VERSION` is the design doc's own
version, tracked separately.

## Licensing

| Component | License |
|---|---|
| This repo (the game) | PolyForm Noncommercial 1.0.0 (see `LICENSE`): free to use, modify, and share for any noncommercial purpose; you may not sell it |
| `src/mach.h` (the engine) | Zlib, permissive, and unaffected by the game's license. Its embedded libraries' notices (RGFW zlib, Clay zlib, stb_image public domain) are carried inside the header and in the mach.h repo's LICENSE |
