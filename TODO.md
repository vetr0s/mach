# mach engine — roadmap

## NOTES

- RE: gdd, think city skylines mixed with miners haven and a pinch of factorio

## Core engine
- [x] Game loop: update/render separation, variable timestep with soft cap
- [x] Fixed simulation timestep: world steps at a constant rate (10/s) decoupled
      from the render framerate, so the sim plays identically on any monitor
- [x] Input system: keyboard, mouse
- [ ] Event system: input, collision, game events
- [x] Memory: arena allocator (Tsoding-style region list); the world is one
      arena block, freed whole at shutdown. `arena_reset` for reuse is in place.

## Rendering (2D on SDL_Renderer)
- [x] **[v0.5.0] Pivot to a minimal 2D engine.** Dropped SDL_GPU + the offline
      HLSL shader pipeline entirely (gpu/draw/mesh/camera/shaders all deleted).
      The game is a 2D iso builder and doesn't need real 3D; defer 3D until it's
      needed and understood. See `ARCHITECTURE.md`.
- [x] 2D renderer on SDL_Renderer (`render2d`): clear/present, fill_rect,
      fill_poly via SDL_RenderGeometry, text, sprite load/draw
- [x] Isometric projection as a coordinate transform (iso_to_screen / screen_to_iso)
- [x] 2D pan+zoom camera (Camera2D)
- [x] Shaded iso blocks (top + two side faces) for faux-3D depth
- [x] Ground as a viewport-culled checker of iso tiles
- [x] Depth-sorted machine draw (painter's, back-to-front by gx+gy)
- [x] Bitmap font as an SDL_Texture atlas (tinted via color mod)
- [x] Placement validation: red/green hover preview
- [ ] Real sprite art (loader is wired; drop PNGs into assets/sprites)
- [x] Tile/block edge outlines for crisper separation (grid lines on the ground,
      darkened seams on blocks, a bright edge on the hover tile)
- [ ] Sprite batching / atlas for many entities
- [x] Window resize handling (renderer re-tracks size + logical presentation)
- [ ] (later) Real 3D — only when there's a concrete need and the GPU grasp to own it

## Math
- [x] Vec2 type + ops; Vec4 (color)
- [x] Scalar helpers (min, max, clamp, lerp)
- [ ] 3D vector/matrix math — returns with 3D, if it does

## Gameplay (value-loop belt builder)
- [x] Entity system (fat structs + direct arrays)
- [x] Core loop: droppers emit items, conveyors route them, upgraders raise value,
      collectors bank it. world_tick runs the belt sim.
- [x] Belt movement: one item per cell, gap-aware advance (a settle pass so a belt
      with a gap fully advances in one tick; a packed loop correctly jams)
- [x] Upgraders bound by "once per upgrader, per item" (u64 touched-mask, 64 cap),
      so the puzzle is routing through many *distinct* upgraders
- [x] Directional placement with rotation (R), money HUD, item/arrow rendering
- [ ] Machine tiers: better droppers/conveyors/upgraders (a `tier` field scaling
      one stat)
- [ ] Special upgraders (caps, value gates, multipliers with conditions)
- [ ] Save/load a layout

## Feel & polish (backlog)
Things to make it play and look right, batched for later sessions. Not urgent.
- [x] Smooth item travel: items slide between cells along the belt instead of
      snapping cell to cell, so they read as real objects being carried
- [ ] Belt surface animation: the conveyor top scrolls in its facing direction
- [ ] Sprites for pieces and items, replacing the flat shaded blocks/diamonds
- [ ] Item despawn handling: drop items off a dead-end or a deleted belt instead of
      jamming forever, with a small despawn effect
- [ ] Verify/fix the facing arrows so they read clearly in the iso projection
- [ ] Tune speed and feel: drop rate, belt speed, upgrader strength, item value
      curve (all one-constant knobs in world.c right now)

## Content
- [ ] Asset loading pipeline
- [ ] Level/tilemap format and loader
- [ ] Data serialization

## Debugging
- [x] Leveled logging (INFO / ERROR / DEBUG)
- [ ] Debug draw: collision bounds, vectors
- [ ] Performance profiler
- [ ] Immediate-mode debug UI

## Audio (later)
- [ ] Audio system
- [ ] Sound and music loading
- [ ] Spatial audio

## Scripting (future)
- [ ] Embedded Lua integration
- [ ] Script hot-reload

## Tools (future)
- [ ] Level editor
- [ ] Sprite editor
- [ ] Scene editor

## Platform
- [x] macOS: building and running
- [ ] Linux: build and test (toolchain wired: setup.sh + build.sh)
- [ ] Windows: build and test (toolchain wired: setup.bat + build.bat)

> **Note: cross-platform got much simpler.** Dropping SDL_GPU means there's no
> shader toolchain and no per-backend bytecode to worry about — SDL_Renderer
> picks the native 2D backend (Metal/Vulkan/D3D) on each platform on its own.
> Linux/Windows just need SDL3 built and a C compiler; verify on real hardware
> when available.
