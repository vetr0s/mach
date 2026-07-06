# mach — architecture

> There's a companion doc, `docs/engine-redesign-2026-06-28.md`, that records an
> earlier design exploration. Heads up: it argued for owning an SDL_GPU renderer
> with an offline HLSL shader pipeline, and that whole direction got **reversed**
> in favor of the minimal 2D engine described here. The exploration was useful (it
> made the call obvious), but where the two docs disagree, believe this one.

## The thesis

mach is **engine-first, minimal, and 2D**. In practice that means a few things,
and they're load-bearing, so here they are spelled out.

- **A small 2D engine on its own GL batch renderer drives the game.** RGFW (a
  single header, vendored) opens the window and delivers input; the engine owns
  everything above that: one shader, one stream of textured vertex-colored
  triangles. No engine framework to build, no GPU-pipeline ownership beyond
  those few hundred lines, no offline shader tooling.
- **The game is a 2D isometric builder, and it does not need real 3D.** Look at the
  genre: Factorio, RimWorld, old SimCity. That's 2D sprites in an iso projection.
  The "3D look" is faked with shaded blocks and draw order, not a depth buffer.
- **Real 3D is deferred, not sworn off.** It comes back when there's a concrete need
  *and* a real grasp of modern GPU APIs to back it up. Carrying it speculatively
  today would just be complexity nobody here can maintain yet.
- **The game owns the loop; the engine is a toolbox.** raylib-style. The engine
  never names a game type, and it never drives anything on its own.
- **Quality is coherence and taste, not a feature count.** The engine is allowed,
  encouraged even, to say no to things it doesn't need.

## Rendering

`render2d` is the entire render layer, a **batch renderer on OpenGL 3.3
core**. Every draw call appends textured, vertex-colored triangles to one
CPU-side batch; the batch flushes to a single `glDrawElements` when the texture
or scissor changes, the batch fills, or the frame presents. Solid fills sample a
1×1 white texture, so everything — fills, outlines, text, sprites — goes through
the same shader. The `gl` section declares the ~40 GL entry points by hand and `r2d_init`
loads them at runtime through RGFW's proc loader into the `Renderer` struct
(pointer-passed like all engine state, which is what keeps hot reload honest —
see below). No system GL headers.

- **Primitives:** `r2d_begin` / `r2d_present`, `r2d_fill_rect`, `r2d_fill_poly`
  (convex fans), `r2d_poly_outline` (closed loops as thin edge quads — core GL
  has no reliable line width), `r2d_text`, a clip rect for the UI
  (`r2d_clip_begin`/`end`), and sprite load/draw
  (`r2d_load_texture` / `r2d_sprite`) for real art when it shows up.
- **Camera2D:** pan and zoom over the isometric plane.
- **Isometric is a pure coordinate transform**, not a 3D projection. `iso_to_screen`
  and `screen_to_iso` map grid to pixels and back (2:1 diamond tiles). Want a
  top-down or free 2D camera instead? Swap the transform. Nothing downstream cares.

Colors are the `Color` type (the `color` section) — a `Vec4` alias, RGBA in [0,1] —
plus a stock palette and small helpers (`color_shade`, `color_lighten`,
`color_lerp`, `color_alpha`). The palette is **modus-vivendi** (Protesilaos
Stavrou's Emacs theme): accessibility-grade contrast on a black background, with
the theme's own names (`COLOR_BG_DIM`, `COLOR_RED_WARMER`, ...) so its docs apply.
A game can lean on it wholesale or define its own colors with `COLOR_HEX`.

The actual *look* lives in the **game** layer, where `render_game.c` composes those
primitives: the ground is a viewport-culled checker of iso diamonds with grid lines
stroked on; machines are **shaded blocks** (bright top, two darker side faces,
outlined edges) drawn back-to-front by `gx+gy`. That's the oldest trick in the
2D-iso book for faking depth, and it holds up. The font is a GL texture atlas of
8×8 glyphs, tinted per vertex.

## The engine ÷ game boundary

The game owns control flow. The engine is a library it calls into. Here's the whole
loop:

```c
int main(void) {
    Engine engine = {0}; engine_init(&engine, game_engine_config());  // game sets window + policy
    App    app    = {0}; app_init(&app, &engine);

    while (engine_running(&engine)) {
        f32 dt = engine_frame_begin(&engine);      // drain events into engine.input, return dt
        app_update(&app, &engine, dt);             // game reads engine.input, steps the sim
        if (engine_render_begin(&engine)) {        // clear
            app_render(&app, &engine);             // game draws via r2d
            engine_render_end(&engine);            // present
        }
        engine_frame_end(&engine);                 // FPS + frame cap
    }
    app_shutdown(&app, &engine);
    engine_shutdown(&engine);
}
```

What actually enforces the separation isn't the shape of that loop. It's one rule:
**`mach.h` never names a type from `src/game/`.** That's it.

Input follows the same toolbox shape. The game never sees an `RGFW_event`: the
engine drains the queue in `engine_frame_begin` (consuming window lifecycle —
quit, Escape, resize) and folds the rest into `Engine.input`, a per-frame
snapshot (the `input` section of `mach.h`). The game reads state — `key_pressed[RGFW_key1]`,
`mouse_pressed[MOUSE_LEFT]`, `wheel` — in one place, `game_process_input`.
"Pressed"/"released" are this frame's edges (key repeats excluded); "down"
persists while held.

Policy the engine applies each frame is the game's to set, not the engine's to
hardcode: `Engine_Config` carries the window setup plus the clear color, whether
Escape quits (a dev convenience the game can turn off once Escape means "close
menu"), and the soft frame cap (`target_fps`, `<= 0` for uncapped).

## Hot reload (dev builds)

That five-function boundary (`game_engine_config` + the four `app_*` calls) doubles
as a reload seam. `./build.sh hot` splits the same code into two artifacts instead
of the monolith:

- **`build/mach_hot`** (`src/host.c`) — the host. Owns the window, the engine, and
  the `App` memory, and runs the loop by calling the five functions through pointers
  resolved from a shared library. Never reloaded.
- **`build/libmach_game.{dylib,so}`** (`src/game_lib.c`) — the game logic, plus its
  own copy of the engine. The host watches this file; when it changes, it reloads it
  and keeps running against the same `App` memory.

`./build.sh hot` is the whole dev loop in one command: it builds both artifacts,
launches `mach_hot`, then turns into a watcher over `mach.h` and `src/` that rebuilds the
library on every change. Edit sim rules, tuning, or render code and it swaps in
live — no restart, no lost state. A compile error doesn't stop the watch; the
game keeps running the last good code until the next save fixes it. Ctrl-C (or
closing the game) ends the loop. `./build.sh game` still exists for a manual
one-shot rebuild.

Two facts make this work with zero linker tricks:

- **The engine has no mutable global state.** Everything lives in `Engine` /
  `Renderer` / `Arena` / `App`, all owned by the host and passed in by pointer. So
  the host and the library can each carry their own copy of the engine *code* and
  still operate on the same *data*. (Keep it that way: no mutable file-scope state.)
  This is also why the GL function pointers live *inside* `Renderer`: the library's
  copy of the draw code batches and flushes through the pointers the host loaded.
  RGFW itself does keep internal globals, but only the host ever calls it — the
  library's engine copy stops at `r2d_*`, and the frame lifecycle (events, swap)
  runs host-side.
- **State is host-owned.** `App` sits in host memory the reload never touches, and
  the arena's regions are plain `malloc` (process-global), so every pointer survives
  a code swap.

The one limit: reload swaps **code, not layout**. Change a field in `App`,
`Game_State`, or `World` and you need a normal restart — the persistent memory would
be reinterpreted wrong otherwise. `src/mach.c` stays the release monolith (no
`dlopen`), so shipping is unaffected.

## The modules

The engine is **one header, `mach.h`**, stb-style: declarations always,
implementation compiled where `MACH_IMPLEMENTATION` is defined (the unity root,
the dev host, and the hot-reload library each do it once). The header is the
source of truth — there is no per-module file tree behind it and no
amalgamation step. Its sections, in order:

```
mach.h
  base, debug                       # types, logging/assertions
  RGFW declarations                 # windowing/input/GL context (impl in core)
  mem                               # arena allocator (region list, whole-arena free/reset)
  math                              # Vec2 + scalar helpers
  color                             # Color type, helpers, stock palette (modus-vivendi)
  gl                                # hand-declared GL 3.3 core surface, runtime-loaded
  font                              # 8x8 bitmap font as a GL texture atlas
  image                             # stb_image loader (for sprite art)
  render2d                          # GL batch renderer + iso camera/transforms
  input                             # per-frame input snapshot (keys, mouse, wheel)
  clay_ui                           # Clay layout bound to render2d (HUD + future UI)
  core                              # loop lifecycle + per-frame steps, event drain, timing
                                    #   (single home of the RGFW implementation)

src/game/
  world                             # entity sim (fat structs + grid)
  game                              # state, input, 2D iso camera, hover-pick
  render_game                       # draws the world as iso tiles + shaded blocks
  app                               # glue: bridges engine API and game internals
```

The single-header deps (`RGFW.h`, `stb_image.h`, `clay.h`) sit on the include
path from `third_party/`; `mach.h` includes them by bare name so it works
wherever it's vendored.

## Memory

Arenas are the idiom: the `mem` section of `mach.h`, modeled on Tsoding's arena.h. It's a
linked list of malloc'd regions with bump allocation, freed or reset as a whole
rather than one allocation at a time. Two arenas exist today:

- **The world arena** (game-owned): the entire `World` is a single block, and
  shutdown just frees the arena.
- **The frame arena** (`Engine.frame_arena`): per-frame scratch, reset at every
  `engine_frame_begin`. Anything allocated from it lives exactly one frame — the
  render depth-sort buffers come from here, sized to what's actually alive. The
  regions are reused across frames, so steady state allocates nothing.

To be clear about the original motivation: this was set up to **establish the
pattern before anything needs it** (assets, levels, strings), not to plug a leak,
because there wasn't one. One thing to keep in the back of your mind:
`world_despawn` swap-removes and reassigns entity ids, so an id you hold across a
despawn goes stale. The fix is generational handles, and it can wait until entities
are actually tracked over time.

## Non-goals (this is how you stay opinionated)

- **Real-time 3D, for now.** Deferred until there's a real need and the GPU
  understanding to own it. The renderer is 2D on purpose, not by accident.
- **Shaders, custom GPU pipelines, offline shader tooling.** The renderer owns
  exactly one shader, compiled from a string at startup, and that's the ceiling.
- **A generic component-system ECS.** Entities are fat structs. You loop over an
  array.
- **Being a do-everything engine.** Breadth shows up only when a concrete need drags
  it in.

## Migration log

1. **Game-owned loop** (raylib-style) — done, kept.
2. *SDL_GPU generic core + batteries split* — done, then **ripped out** when the 2D
   pivot made the whole GPU-stack thesis moot.
3. **2D SDL_Renderer engine** — done, then replaced by (8).
4. **Fixed simulation timestep** — done; the world steps at a constant rate,
   independent of framerate.
5. **Arena allocator** — done; the world is one arena block.
6. **Edge outlines** — done; tiles and blocks get stroked edges.
7. **Value-loop sim** — done; droppers, conveyors, upgraders, and collectors with
   a belt simulation in world_tick. Upgraders are bounded "once per item" so the
   game is about routing, not depletion.
8. **SDL3 → RGFW + own GL batch renderer** (this document) — done. Every
   dependency is now a committed single header; the codebase is C99; there is no
   setup step.
9. **Single-header engine** (this document) — done. The whole engine is `mach.h`,
   edited directly, nob.h-style. Groundwork for splitting engine and game into
   their own repos.

Next up: machine tiers and special upgraders, and real sprite art dropped onto the
loader that's already sitting there wired and waiting.
