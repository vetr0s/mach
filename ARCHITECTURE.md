# mach — architecture

> There's a companion doc, `docs/engine-redesign-2026-06-28.md`, that records an
> earlier design exploration. Heads up: it argued for owning an SDL_GPU renderer
> with an offline HLSL shader pipeline, and that whole direction got **reversed**
> in favor of the minimal 2D engine described here. The exploration was useful (it
> made the call obvious), but where the two docs disagree, believe this one.

## The thesis

mach is **engine-first, minimal, and 2D**. In practice that means a few things,
and they're load-bearing, so here they are spelled out.

- **A small 2D engine on SDL_Renderer drives the game.** No shaders, no
  GPU-pipeline ownership, no offline shader tooling. SDL_Renderer already gives you
  hardware-accelerated 2D on the native backend (Metal, Vulkan, D3D) for zero
  extra dependencies, because it's part of the SDL3 you're already building.
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

`render2d.{h,c}` is the entire render layer, sitting on **SDL_Renderer**:

- **Primitives:** `r2d_begin` / `r2d_present`, `r2d_fill_rect`, `r2d_fill_poly`
  (convex polys through `SDL_RenderGeometry`), `r2d_poly_outline` (closed loops
  through `SDL_RenderLines`), `r2d_text`, and sprite load/draw
  (`r2d_load_texture` / `r2d_sprite`) for real art when it shows up.
- **Camera2D:** pan and zoom over the isometric plane.
- **Isometric is a pure coordinate transform**, not a 3D projection. `iso_to_screen`
  and `screen_to_iso` map grid to pixels and back (2:1 diamond tiles). Want a
  top-down or free 2D camera instead? Swap the transform. Nothing downstream cares.

The actual *look* lives in the **game** layer, where `render_game.c` composes those
primitives: the ground is a viewport-culled checker of iso diamonds with grid lines
stroked on; machines are **shaded blocks** (bright top, two darker side faces,
outlined edges) drawn back-to-front by `gx+gy`. That's the oldest trick in the
2D-iso book for faking depth, and it holds up. The font is an `SDL_Texture` atlas,
tinted per draw with color mod.

## The engine ÷ game boundary

The game owns control flow. The engine is a library it calls into. Here's the whole
loop:

```c
int main(void) {
    Engine engine = {0}; engine_init(&engine, game_window_config());  // game sizes the window
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
**`src/engine/` never names a type from `src/game/`.** That's it.

Input follows the same toolbox shape. The game never sees an `SDL_Event`: the
engine drains the queue in `engine_frame_begin` (consuming window lifecycle —
quit, Escape, resize) and folds the rest into `Engine.input`, a per-frame
snapshot (`engine/input`). The game reads state — `key_pressed[SDL_SCANCODE_1]`,
`mouse_pressed[MOUSE_LEFT]`, `wheel` — in one place, `game_process_input`.
"Pressed"/"released" are this frame's edges (key repeats excluded); "down"
persists while held.

## Hot reload (dev builds)

That five-function boundary (`game_window_config` + the four `app_*` calls) doubles
as a reload seam. `./build.sh hot` splits the same code into two artifacts instead
of the monolith:

- **`build/mach_hot`** (`src/host.c`) — the host. Owns the window, the engine, and
  the `App` memory, and runs the loop by calling the five functions through pointers
  resolved from a shared library. Never reloaded.
- **`build/libmach_game.{dylib,so}`** (`src/game_lib.c`) — the game logic, plus its
  own copy of the engine. The host watches this file; when it changes, it reloads it
  and keeps running against the same `App` memory.

Iterate by leaving `mach_hot` running and rebuilding just the library in another
terminal: `./build.sh game`. Edit sim rules, tuning, or render code and it swaps in
live — no restart, no lost state.

Two facts make this work with zero linker tricks:

- **The engine has no mutable global state.** Everything lives in `Engine` /
  `Renderer` / `Arena` / `App`, all owned by the host and passed in by pointer. So
  the host and the library can each carry their own copy of the engine *code* and
  still operate on the same *data*. (Keep it that way: no mutable file-scope state.)
- **State is host-owned.** `App` sits in host memory the reload never touches, and
  the arena's regions are plain `malloc` (process-global), so every pointer survives
  a code swap.

The one limit: reload swaps **code, not layout**. Change a field in `App`,
`Game_State`, or `World` and you need a normal restart — the persistent memory would
be reinterpreted wrong otherwise. `src/mach.c` stays the release monolith (no
`dlopen`), so shipping is unaffected.

## The modules

```
engine/
  base, math, debug, ui             # types, Vec2/scalar math, logging, window context
  mem                               # arena allocator (region list, whole-arena free/reset)
  input                             # per-frame input snapshot (keys, mouse, wheel)
  core                              # loop lifecycle + per-frame steps, event drain, frame timing
  render/
    render2d.{h,c}                  # SDL_Renderer wrapper + iso camera/transforms
    font.{h,c}                      # 8x8 bitmap font as an SDL_Texture atlas
    image.{h,c}                     # stb_image loader (for sprite art)
  ui/
    clay_ui.{h,c}                   # Clay layout bound to render2d (HUD + future UI)
game/
  world                             # entity sim (fat structs + grid)
  game                              # state, input, 2D iso camera, hover-pick
  render_game                       # draws the world as iso tiles + shaded blocks
  app                               # glue: bridges engine API and game internals
```

## Memory

Arenas are the idiom: `engine/mem/arena`, modeled on Tsoding's arena.h. It's a
linked list of malloc'd regions with bump allocation, freed or reset as a whole
rather than one allocation at a time. The entire `World` is a single arena block
owned by the game, and shutdown just frees the arena.

To be clear about the motivation: this was set up to **establish the pattern before
anything needs it** (assets, levels, strings, per-frame scratch), not to plug a
leak, because there wasn't one. One thing to keep in the back of your mind:
`world_despawn` swap-removes and reassigns entity ids, so an id you hold across a
despawn goes stale. The fix is generational handles, and it can wait until entities
are actually tracked over time.

## Non-goals (this is how you stay opinionated)

- **Real-time 3D, for now.** Deferred until there's a real need and the GPU
  understanding to own it. The renderer is 2D on purpose, not by accident.
- **Shaders, custom GPU pipelines, offline shader tooling.** Gone, and good
  riddance. SDL_Renderer talks to the GPU so you don't have to.
- **A generic component-system ECS.** Entities are fat structs. You loop over an
  array.
- **Being a do-everything engine.** Breadth shows up only when a concrete need drags
  it in.

## Migration log

1. **Game-owned loop** (raylib-style) — done, kept.
2. *SDL_GPU generic core + batteries split* — done, then **ripped out** when the 2D
   pivot made the whole GPU-stack thesis moot.
3. **2D SDL_Renderer engine** (this document) — done.
4. **Fixed simulation timestep** — done; the world steps at a constant rate,
   independent of framerate.
5. **Arena allocator** — done; the world is one arena block.
6. **Edge outlines** — done; tiles and blocks get stroked edges.
7. **Value-loop sim** — done; droppers, conveyors, upgraders, and collectors with
   a belt simulation in world_tick. Upgraders are bounded "once per item" so the
   game is about routing, not depletion.

Next up: machine tiers and special upgraders, and real sprite art dropped onto the
loader that's already sitting there wired and waiting.
