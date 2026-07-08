# mach: architecture

This is the game's architecture. The engine is a separate project, the
**mach.h** repo, and this repo carries a copy at `src/mach.h`. Engine internals
(the batch renderer, the GL loader, arenas, input, the Clay binding) are
documented there, in that repo's `ARCHITECTURE.md`. This document covers the
game and how it sits on the engine.

## The thesis

The game is a 2D isometric builder, and it does not need real 3D. Look at the
genre: Factorio, RimWorld, old SimCity. That's 2D sprites in an iso projection.
The "3D look" is faked with shaded blocks and draw order, not a depth buffer.

The game owns the loop; the engine is a toolbox. raylib-style. The engine never
names a game type, and it never drives anything on its own.

Quality is coherence and taste, not a feature count. Say no to things the game
doesn't need.

## The engine/game boundary

The game owns control flow. The engine is a library it calls into, and the whole
engine surface the loop touches is five functions on one struct:

```c
int main(void) {
    Mach m = {0};
    if (!mach_init(&m, game_config())) return 1;   // game sets window + policy
    Game_State game = {0};
    game_init(&game, &m);

    while (mach_running(&m)) {
        mach_frame_begin(&m);     // drain events into m.input, set m.dt, clear
        game_frame(&game, &m);    // game: input -> sim -> draw via &m.r2d
        mach_frame_end(&m);       // present, count fps, apply the frame cap
    }
    game_shutdown(&game);
    mach_shutdown(&m);
}
```

Everything a frame produces is read off the `Mach` struct: `m.input`, `m.dt`,
`m.fps`. What actually enforces the separation isn't the shape of that loop.
It's one rule: **the engine never names a type from `src/game/`.** That's it.

The game never sees a raw window event. The engine folds the event queue into
`Mach.input`, a per-frame snapshot, and the game reads state (`key_pressed[...]`,
`mouse_pressed[MACH_MOUSE_LEFT]`, `wheel`) in one place, `game_process_input`.
Per-frame policy (clear color, whether Escape quits, the frame cap) is the
game's to set in `game_config`, not the engine's to hardcode.

## The look

`render_game.c` composes the engine's 2D primitives into the game's look: the
ground is a viewport-culled checker of iso diamonds with grid lines stroked on;
machines are **shaded blocks** (bright top, two darker side faces, outlined
edges) drawn back-to-front by `gx+gy`. That's the oldest trick in the 2D-iso
book for faking depth, and it holds up. Isometric is a pure coordinate
transform (`mach_iso_to_screen` / `mach_screen_to_iso`, 2:1 diamond tiles), not
a 3D projection. The HUD is Clay floating panels (`game_render_hud`), drawn
through the same renderer.

Colors come from the engine's stock palette (`MACH_COLOR_*`, modus-vivendi):
accessibility-grade contrast on a black background, which is what a game HUD
wants.

## Hot reload (dev builds)

The four `game_*` functions double as a reload seam. `./build.sh hot` splits the
same code into two artifacts instead of the monolith:

| Artifact | Source | Role |
|---|---|---|
| `build/mach_hot` | `src/host.c` | The host. Owns the window, the engine, and the `Game_State` memory, and runs the loop by calling the four functions through pointers resolved from a shared library. Never reloaded. |
| `build/libmach_game.{dylib,so}` | `src/game_lib.c` | The game logic, plus its own copy of the engine. The host watches this file; when it changes, it reloads it and keeps running against the same `Game_State` memory. |

`./build.sh hot` is the whole dev loop in one command: it builds both artifacts,
launches `mach_hot`, then turns into a watcher that rebuilds the library on every
change. Edit sim rules, tuning, or render code and it swaps in live: no restart,
no lost state. A compile error doesn't stop the watch; the game keeps running the
last good code until the next save fixes it. `./build.sh game` still exists for a
manual one-shot rebuild.

Two facts make this work with zero linker tricks:

1. **The engine has no mutable global state.** Everything lives in `Mach` /
   `Game_State`, all owned by the host and passed in by pointer. So the host and
   the library can each carry their own copy of the engine *code* and still operate
   on the same *data*. (This is an engine design guarantee; the game must keep its
   side of it: no mutable file-scope state in reloadable code.)
2. **State is host-owned.** `Game_State` sits in host memory the reload never
   touches, and arena regions are plain `malloc` (process-global), so every pointer
   survives a code swap.

The one limit: reload swaps **code, not layout**. Change a field in `Game_State`
or `World` and you need a normal restart, or the persistent memory would be
reinterpreted wrong. `src/mach.c` stays the release monolith (no `dlopen`), so
shipping is unaffected.

## The modules

```
src/mach.h                          # the engine, one committed header (its own repo has the docs)

src/game/
  world                             # entity sim (fat structs + grid)
  game                              # state, config, input, sim, the four loop entry points
  render_game                       # draws the world (iso tiles + shaded blocks) and the HUD

src/mach.c                          # unity root: MACH_IMPLEMENTATION + game + main()
src/game_lib.c, src/host.c          # the hot-reload pair (dev builds only)
```

Updating the engine means copying a new mach.h release into `src/mach.h` (and
fixing whatever its changelog says changed). Local experiments against the copy
here are fine; that's how engine changes get proven before they move upstream.
The engine's source of truth is its own repo.

## Memory

Arenas are the idiom (the engine's `mem` section). Two arenas exist today:

| Arena | Owner | Lifetime |
|---|---|---|
| World arena | Game | The entire `World` is a single block; shutdown just frees the arena. |
| Frame arena (`Mach.frame_arena`) | Engine | Per-frame scratch, reset at every `mach_frame_begin`. Anything allocated from it lives exactly one frame; the render depth-sort buffers come from here, sized to what's actually alive. |

One thing to keep in the back of your mind: `world_despawn` swap-removes and
reassigns entity ids, so an id you hold across a despawn goes stale. The fix is
generational handles, and it can wait until entities are actually tracked over
time.

## Non-goals

A generic component-system ECS. Entities are fat structs. You loop over an array.

Engine features for their own sake. The engine grows in its own repo, on its own
judgment; this game asks for things by need, not speculation.

## Migration log

1. **Game-owned loop** (raylib-style): done, kept.
2. *SDL_GPU generic core + batteries split*: done, then **ripped out** when the 2D
   pivot made the whole GPU-stack thesis moot.
3. **2D SDL_Renderer engine**: done, then replaced by (8).
4. **Fixed simulation timestep**: done; the world steps at a constant rate,
   independent of framerate.
5. **Arena allocator**: done; the world is one arena block.
6. **Edge outlines**: done; tiles and blocks get stroked edges.
7. **Value-loop sim**: done; droppers, conveyors, upgraders, and collectors with
   a belt simulation in world_tick.
8. **SDL3 to RGFW + own GL batch renderer**: done. Every dependency became a
   committed single header; the codebase is C99; there is no setup step.
9. **Single-header engine**: done. The whole engine became `mach.h`, edited
   directly, nob.h-style.
10. **Straightforward user API**: done. `Mach_Engine` became `Mach`, the four
    per-frame steps became `mach_frame_begin`/`mach_frame_end` with dt/fps read
    off the struct, and the game-side `app` bridge dissolved into the four
    `game_*` entry points.
11. **Repo split**: done. The engine (fully namespaced, with RGFW/Clay/stb_image
    embedded) moved to its own repo, **mach.h**, versioned separately from this
    game. This repo carries a copy at `src/mach.h` like any other single-header
    dependency.

Next up: the money economy (grid expansion + machine tiers), then real sprite
art dropped onto the loader that's already sitting there wired and waiting.
