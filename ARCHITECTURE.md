# mach — Engine Architecture

> Companion: `docs/engine-redesign-2026-06-28.md` records the earlier design
> exploration. **Note:** that doc argued for owning an SDL_GPU renderer with an
> offline HLSL shader pipeline. That direction was deliberately **reversed** in
> favor of the minimal 2D engine described here — the exploration clarified the
> call. Where the two disagree, this document wins.

## Thesis

mach is **engine-first, minimal, and 2D**.

- **A small 2D engine on SDL_Renderer that drives the game.** No shaders, no
  GPU-pipeline ownership, no offline shader tooling. SDL_Renderer gives
  hardware-accelerated 2D on the native backend (Metal/Vulkan/D3D) with zero
  extra dependencies — it's already in the SDL3 we build.
- **The game is a 2D isometric builder and does not need real 3D.** The genre
  (Factorio, RimWorld, classic SimCity) is 2D sprites in an iso projection. The
  "3D look" is faked with shaded blocks and draw order, not a depth buffer.
- **Real 3D is deferred, not refused forever.** It gets added when there's a
  concrete need *and* a solid grasp of modern GPU APIs — never carried
  speculatively. Today it would be complexity we can't yet maintain.
- **The game owns the loop; the engine is a toolbox** (raylib-style). The
  engine never names a game type.
- **Quality means coherence and taste, not capability.** The engine is allowed —
  required — to say no.

## Rendering

`render2d.{h,c}` is the whole render layer, over **SDL_Renderer**:

- **Primitives:** `r2d_begin`/`r2d_present`, `r2d_fill_rect`, `r2d_fill_poly`
  (convex polys via `SDL_RenderGeometry`), `r2d_text`, and sprite
  load/draw (`r2d_load_texture`/`r2d_sprite`) for real art later.
- **Camera2D:** a pan + zoom over the isometric plane.
- **Isometric is a pure coordinate transform**, not a 3D projection:
  `iso_to_screen` / `screen_to_iso` map grid↔pixels (2:1 diamond tiles).
  Swapping in a top-down or free 2D camera is just a different transform.

The look comes from the **game** layer (`render_game.c`) composing primitives:
ground is a viewport-culled checker of iso diamonds; machines are **shaded
blocks** (bright top + two darker side faces) drawn back-to-front by `gx+gy`.
That's the classic 2D-iso trick for faux-3D depth. Font is an `SDL_Texture`
atlas tinted via color mod.

## Engine ÷ Game boundary

The game owns control flow; the engine is a library it calls into:

```c
int main(void) {
    Engine engine = {0}; engine_init(&engine, "mach", 1280, 720);
    App    app    = {0}; app_init(&app, &engine);

    while (engine_running(&engine)) {
        f32 dt = engine_frame_begin(&engine);
        SDL_Event ev;
        while (engine_poll_event(&engine, &ev)) app_handle_event(&app, &ev);
        app_update(&app, dt);
        if (engine_render_begin(&engine)) {        // clear
            app_render(&app, &engine);             // game draws via r2d
            engine_render_end(&engine);            // FPS overlay + present
        }
        engine_frame_end(&engine);                 // FPS + frame cap
    }
    app_shutdown(&app, &engine);
    engine_shutdown(&engine);
}
```

The separation is enforced by the dependency rule, not the loop shape:
**`src/engine/` never names a type from `src/game/`.** The game still drains raw
`SDL_Event`s; an input-query layer can replace that later.

## Modules

```
engine/
  base, math, debug, os, ui         # types, vec/mat, logging, platform, window consts
  core                              # loop lifecycle + per-frame steps, frame timing
  render/
    render2d.{h,c}                  # SDL_Renderer wrapper + iso camera/transforms
    font.{h,c}                      # 8x8 bitmap font as an SDL_Texture atlas
    image.{h,c}                     # stb_image loader (for sprite art)
game/
  world                             # entity sim (fat structs + grid)
  game                              # state, input, 2D iso camera, hover-pick
  render_game                       # draws the world as iso tiles + shaded blocks
  app                               # glue: bridges engine API and game internals
```

## Memory

Arena allocation is the intended idiom — introduced to **set the pattern before
it's needed** (assets, levels, strings, undo), not to fix a present leak
(there isn't one). Latent issue for later: `world_despawn` swap-removes and
reassigns entity ids, so an id held across a despawn goes stale → generational
handles when entities are tracked over time.

## Non-goals (to stay opinionated)

- **Real-time 3D — for now.** Deferred until there's a real need and the GPU
  understanding to own it. The renderer is 2D on purpose.
- **Shaders / custom GPU pipelines / offline shader tooling.** Gone. SDL_Renderer
  handles the GPU.
- A generic component-system ECS. Entities are fat structs.
- Being a do-everything engine. Breadth only when a concrete need pulls it in.

## Migration log

1. **Game-owned loop** (raylib-style) — **done**, retained.
2. *SDL_GPU generic core + batteries split* — done, then **removed** when the 2D
   pivot superseded the GPU-stack thesis.
3. **2D SDL_Renderer engine** (this document) — **done.**

Next: gameplay depth (world sim), real sprite art on the loader that's already
wired, and arenas to set the allocation idiom.
