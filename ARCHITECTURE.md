# mach — Engine Architecture

> Status: **design target**, not yet fully implemented. This document describes
> where the engine is going. Where the current code differs, the "Migration"
> section at the bottom tracks the gap. Companion: `docs/engine-redesign-2026-06-28.md`
> records the reasoning that produced this thesis.

## Thesis

mach is **engine-first**. The engine is the thing being built for its own sake;
the factory game is the first client that exercises it, not the reason it exists.

The engine's point of view, stated so it can be defended against feature-creep:

- **Own the GPU stack.** The renderer is built on SDL_GPU (native Metal / Vulkan
  / D3D12) and we understand every line of it. The explicit, modern backend is a
  *deliberate* choice, not overhead to hide. Owning it is part of the point.
- **Minimal & opinionated.** A small public surface with conventions we pick on
  purpose. Quality means *coherence and taste*, never *maximal capability*. The
  engine is allowed — required — to say no to things.
- **The game drives; the engine is a toolbox.** raylib-style control flow: the
  game owns `main()` and the loop and calls into the engine. The engine never
  owns the game's loop and never names a game type.
- **The game owns its shaders and materials.** The engine ships a generic
  pipeline interface and an offline shader-compile tool. The game authors its
  own shaders, compiles them with that tool, and hands the result to the engine.
  The engine does not bake game shaders into itself.
- **Batteries are optional, not mandatory.** Common cases (sprites, text, a
  basic lit mesh) work with zero shader-writing through a convenience layer. But
  that layer is built *on* the generic core and can always be bypassed.

### Lineage

The design is, deliberately, **sokol's architecture rebuilt on SDL_GPU as our
own owned code, with raylib's batteries on top.** The mapping:

| Layer            | sokol analogue | What it is                                            |
| ---------------- | -------------- | ----------------------------------------------------- |
| `gpu` core       | `sokol_gfx`    | Generic GPU API: pipelines, buffers, bindings, passes |
| shader tool      | `sokol-shdc`   | Offline HLSL → native bytecode **+ reflection**       |
| batteries        | `sokol_gl` / raylib defaults | Sprite/text/mesh draw helpers w/ shipped shaders |
| windowing/input  | `sokol_app`    | Provided by SDL3 (we do not reinvent this)            |

Where this lands relative to the two reference points:

- **More ergonomic than bare sokol** — we ship batteries, so trivial games don't
  touch a shader.
- **More principled than the engine's first draft** — the core is generic and
  the game owns its shaders, instead of the engine hardcoding two fixed pipelines.

## Layered rendering

### Layer 1 — `gpu` core (we own it; sokol_gfx-shaped)

Generic and game-agnostic. Knows about pipelines, buffers, textures, passes, and
bindings. Knows *nothing* about "lit geometry," "isometric," or "sprite."

```c
Gpu_Pipeline pipe = gpu_make_pipeline(&dev, &(Gpu_PipelineDesc){
    .shader        = my_shader,          // game- or batteries-supplied
    .vertex_layout = { ... },
    .depth_test    = true,
    .blend         = GPU_BLEND_ALPHA,
});

gpu_begin_pass(&dev, &pass_desc);
    gpu_apply_pipeline(pipe);
    gpu_apply_bindings(&bindings);       // vbuf/ibuf/textures/uniforms
    gpu_draw(index_count, instance_count);
gpu_end_pass(&dev);
```

The six concepts this core exposes (and that any reader must hold to understand
the renderer): **device/swapchain, shader, pipeline, frame lifecycle, draw,
buffer/transfer.** Everything in the core is an instance of one of these.

### Layer 2 — batteries (optional; raylib-shaped, built on Layer 1)

Convenience drawing with engine-shipped shaders, so a game can render without
authoring any shader of its own.

```c
draw_sprite(tex, src_rect, dst_rect, color);
draw_text(font, x, y, scale, "hello", color);
draw_mesh(cube, model_matrix, color);      // basic lit pipeline
```

A pure-2D game uses **only** Layer 2 and never writes a shader. A game that wants
custom rendering reaches into Layer 1 and supplies its own pipeline. Both are
first-class. This is how the "2D sprite option" and "the game owns its shaders"
goals coexist without contradiction — they live in different layers.

## Shaders

Shaders are authored in **HLSL** and cross-compiled **offline** to native
bytecode (SPIR-V for Vulkan, MSL for Metal). SDL_GPU consumes native bytecode and
has no runtime shader compiler, so an offline bake is mandatory; baking ahead of
time is also the choice that keeps a clean build with no runtime shader tooling.

### The shader tool (sokol-shdc-shaped)

A **single** cross-platform tool replaces the current pair of drifting scripts
(`scripts/shaders.sh` + `scripts/shaders.ps1`). One source of truth. It emits, per
shader, a header containing:

1. The SPIR-V and MSL **blobs** (as today), and
2. **Reflection metadata** as generated C: uniform-block layouts, binding slots,
   and the vertex-attribute layout.

Reflection is the upgrade that makes the bake "professional." Today `gpu.c`
hardcodes what should be data — `num_uniform_buffers = 2`, binding slots, vertex
offsets are all typed by hand in `gpu_create_pipeline_3d`. With reflection, the
pipeline-create call *reads* the generated description instead of the author
retyping it. Fewer footguns, and — critically — it is what lets the **game** own a
shader: the game runs the tool on its `.hlsl`, gets a self-describing header, and
passes it to `gpu_make_pipeline` without the engine knowing anything about it.

Ownership:

- The **game** runs the tool on its own shaders. The engine never sees them until
  the game hands over a compiled `Gpu_Shader`.
- The **engine** runs the same tool on the batteries' built-in shaders.

The generated header(s) stay committed, so a normal build needs no shader
toolchain (`glslang` + `spirv-cross`) — only editing a `.hlsl` does.

## Engine ÷ Game boundary

The game owns control flow. The engine is a library it calls into.

As shipped in step 1, the loop exposes the frame as discrete steps the game
calls in order:

```c
int main(void) {
    Engine engine = {0}; engine_init(&engine, "mach", 1280, 720);
    App    app    = {0}; app_init(&app, &engine);

    while (engine_running(&engine)) {
        f32 dt = engine_frame_begin(&engine);          // delta time

        SDL_Event ev;
        while (engine_poll_event(&engine, &ev))         // drain game events
            app_handle_event(&app, &ev);                // (quit/Esc handled in engine)

        app_update(&app, dt);

        if (engine_render_begin(&engine)) {             // false => skip (minimized)
            app_render(&app, &engine);                  // batteries, or own pipelines
            engine_render_end(&engine);                 // engine overlay + present
        }
        engine_frame_end(&engine);                      // FPS + frame cap
    }

    app_shutdown(&app, &engine);
    engine_shutdown(&engine);
    return 0;
}
```

This replaces the `Engine_App` callback vtable. The directory/dependency rule is
unchanged and is what actually enforces separation: **`src/engine/` never names a
type from `src/game/`.** Inverting the loop back to the game does not weaken that
— the boundary was always the dependency direction, not the callbacks.

> Transitional note: the game still drains **raw `SDL_Event`s** via
> `engine_poll_event`. Replacing that with an engine-owned input-query layer
> (raylib's `IsKeyDown`-style state, no raw events in game code) is deliberately
> deferred to a later step — step 1 only flips loop ownership.

## Memory

Arena allocation is the engine's allocation idiom. Rationale and timing:

- The current code is **not** leaking — `world_create` does one fixed-size malloc
  and frees it once; everything else is fixed arrays or stack. There is no bomb
  to defuse today.
- Arenas are introduced anyway to **set the pattern before it is needed**, so
  every subsystem added later (assets, levels, strings, undo) reaches for an
  arena instead of ad-hoc `malloc`. Expected shape: a per-frame scratch arena
  reset each frame, and longer-lived arenas for level/world data.

Related latent issue to address when entities are tracked across time:
`world_despawn` swap-removes and **reassigns** the moved entity's id, so a raw id
held across a despawn becomes wrong. Generational handles are the fix when the
game starts retaining entity references between frames.

## Non-goals (what this engine refuses, to stay opinionated)

- A generic component-system ECS. Entities are fat structs by design.
- A general material/PBR system. Materials are whatever shader the game writes.
- Runtime shader compilation / hot-reload of GLSL strings. Offline bake only.
- Engine-owned game loop, scenes, or game object model. The game owns those.
- Being a "do-everything" engine. Breadth is added only when a concrete need
  pulls it in, never speculatively.

## Migration

Current state vs. this target, lowest-risk change first:

1. ~~**Flip the loop** to game-owned (raylib-style), retire the `Engine_App`
   vtable.~~ **Done.** `Core_Engine`→`Engine`, `core_*`→`engine_*`, the loop now
   lives in `main()`; the engine exposes per-frame steps. Input is still raw
   `SDL_Event` (query layer deferred).
2. **Split `gpu.c`** into the generic Layer 1 core + the Layer 2 batteries
   (sprite/text/mesh helpers move into batteries on top of the generic core).
3. **Rebuild the shader bake** as one cross-platform tool emitting reflection;
   move pipeline configuration from hardcoded constants to generated data; let
   the game own its shaders through it.
4. **Prove the layering** with a 2D sprite path that writes no engine shaders.
5. **Introduce arenas** once the layering has settled the allocation idiom.

Nothing above discards working substance — the SDL_GPU renderer, math, world,
and font work all survive. This is a deliberate re-cutting of three seams (loop,
render layering, shader ownership), not a rewrite.
