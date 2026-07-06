# mach engine — roadmap

## NOTES

- RE: gdd, think city skylines mixed with miners haven and a pinch of factorio.
  The design doc (docs/gdd.typ, v0.2.0) is authoritative and now describes the
  value-loop builder in full; the Gameplay section below tracks catching the code
  up to it.

## Core engine
- [x] Game loop: update/render separation, variable timestep with soft cap
- [x] Fixed simulation timestep: world steps at a constant rate (3/s) decoupled
      from the render framerate, so the sim plays identically on any monitor
- [x] Input system: keyboard, mouse
- [x] Input layer: engine-owned per-frame snapshot (Engine.input, engine/input).
      engine_frame_begin drains the event queue; the game reads state (down/pressed/
      released, mouse, wheel) in game_process_input instead of handling events.
      Reload seam shrank to five functions (app_handle_event deleted).
- [ ] Event system: collision, game events (input is covered by the snapshot)
- [x] Memory: arena allocator (Tsoding-style region list); the world is one
      arena block, freed whole at shutdown. `arena_reset` for reuse is in place.
- [x] Frame-scratch arena (Engine.frame_arena): reset each engine_frame_begin;
      backs the render depth-sort buffers (which were 176KB of file-scope statics).
- [x] Hot reload (dev): game logic compiles to a shared lib the host (src/host.c)
      dlopen's and swaps on rebuild, keeping App state across reloads. `./build.sh hot`
      is the whole loop: it builds, runs the game, and watches src/ — every save
      rebuilds the lib and the running game swaps it in (compile errors keep the
      last good code). Release stays the src/mach.c monolith. See ARCHITECTURE.md.
      Limit: code, not struct-layout, changes reload live.

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
- [ ] Viewport-cull entities in render (only ground is culled today; game_render_draw
      draws every entity regardless of the visible grid bbox we already compute).
      The scaling win once belt counts get large, well before the animation math costs.
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
Design target: docs/gdd.typ (v0.2.0). Items below the "value model rework" are
mechanics the GDD calls for that the code hasn't caught up to yet, in priority order.

- [x] Entity system (fat structs + direct arrays)
- [x] Core loop: droppers emit items, conveyors route them, upgraders raise value,
      collectors bank it. world_tick runs the belt sim.
- [x] Belt movement: one item per cell, gap-aware advance (a settle pass so a belt
      with a gap fully advances in one tick; a packed loop correctly jams)
- [x] Directional placement with rotation (R), money HUD, item/arrow rendering
- [x] Upgraders bound "once per upgrader, per item" (u64 touched-mask, 64 cap).
      SUPERSEDED — the design now makes recirculating loops central, which the
      once-per rule made pointless. Replaced by the value model rework below.
- [x] **Value model rework (GDD Milestone 2).** "Diminishing per pass, uncapped roof":
      an ore climbs toward a value ceiling, each pass closing a fixed fraction of the gap
      so a single loop settles and looping forever asymptotes. The once-per touched-mask
      is repurposed — each *distinct* upgrader multiplies the ceiling once, so more/stronger
      upgraders raise the (uncapped) roof. In world.{c,h} (Item.ceiling, item_apply_cell,
      UPGRADER_CEILING_MULT / UPGRADER_CLIMB_DIVISOR). Ore value now renders above its sprite.
      Tuning of the exact curve/constants is deferred (see GDD Open Questions).
- [ ] **Money economy (GDD Milestone 3 — the progression spine).** Give money real,
      competing sinks: grid expansion on a 2^n side-length cadence (2x2 -> 4x4 -> 8x8...,
      cost scaling with area, gated by a playable_extent) + buyable tiers for
      droppers/upgraders/belts. Space vs spend.
- [ ] Rename collector -> furnace throughout (ENTITY_COLLECTOR / TOOL_COLLECTOR /
      Entity_Collector in world.{h,c}, game.{h,c}, render_game.c). GDD naming decision.
- [ ] Machine tiers plumbing: a `tier` field scaling one stat. Dropper/upgrader tiers
      are a value bump today (drop_cooldown, UPGRADER_MULT). Belt-speed tiers need belt
      speed moved off the global sim clock onto a per-entity ticks-per-cell cadence, with
      item interpolation spanning that multi-tick window instead of one-move-per-tick.
- [ ] Special upgraders (caps, value gates, multipliers with conditions)
- [ ] Save/load a layout (full world state: objects + tiers, ore in flight, money,
      unlocked grid size, camera)
- [ ] Open (see GDD Open Questions): one furnace only (single sink, strong spatial
      constraint) vs many placeable; whether furnaces become a 4th upgrade axis

## UI & controls
- [x] Clay adopted for UI (third_party/clay, vendored zlib). Bound to render2d in
      engine/ui/clay_ui.{h,c}: bitmap-font text-measure hook + render-command
      translation (rects, text, borders, scissor). Hot-reload-safe: context lives in
      host App memory, re-pointed each frame. Reach for Clay for all new UI.
- [x] HUD as a Clay panel: money always on, selected tool/facing under it, pause marker.
- [x] F3 debug/info overlay (fps, tick/entity/item counts, hover cell, camera, controls),
      toggled and small so it stays out of the way.
- [x] Engine exposes FPS via engine_fps(); the game owns all HUD drawing (engine no
      longer draws its own overlay).
- [x] Pause / resume the simulation (Space): freezes sim ticks and animation; build and
      pan still work while frozen.
- [x] Rotate the hovered piece in place (R); over an empty tile it rotates the facing the
      next placed piece will use.
- [x] Feed real mouse position/button into clay_ui_begin (from Engine.input).
- [x] Color type + stock palette (engine/render/color.h): Color aliases Vec4, palette
      is modus-vivendi with the theme's own names, helpers (shade/lighten/lerp/alpha)
      moved up from render_game statics. Game rethemed onto it.
- [x] HUD spread to the screen edges via Clay floating panels (attach-to-root):
      status top-left, inspect top-center, F3 debug bottom-left, controls bottom-center.
- [x] Hover inspect panel (WTHIT-style): names the machine under the cursor, facing,
      dropper cooldown, collector banked total, and the ore's value / ceiling.
- [ ] Interactive UI: build clickable UI on top of it — a tool palette, and the
      economy's shop / grid-expansion buttons. Needs "UI consumed this click"
      priority over world placement (Clay_PointerOver before place_at_hover).

## Feel & polish (backlog)
Things to make it play and look right, batched for later sessions. Not urgent.
- [x] Smooth item travel: items slide between cells along the belt instead of
      snapping cell to cell, so they read as real objects being carried
- [x] Belt surface animation: the conveyor top scrolls chevrons in its facing
      direction (real-time, so it runs even when the belt is empty)
- [ ] Sprites for pieces and items, replacing the flat shaded blocks/diamonds
- [x] Item despawn handling. Deleting an entity despawns the ore on its cell; ore that
      hits a dead end (off-grid edge, bare ground, or a dropper's back) tips off and drops
      out over FALL_TICKS with a sink-and-fade effect. See world_despawn, item_begin_fall,
      world_update_falls.
- [x] Verify/fix the facing arrows so they read clearly in the iso projection
- [x] Tune speed and feel: chunky tier-1 baseline — belts 3 cells/s (SIM_TICKS_PER_SEC),
      an item every 2 cells (DROP_PERIOD), chevron scroll matched to belt speed.
      Value curve (ITEM_BASE_VALUE, UPGRADER_MULT) left as-is; revisit with the economy.

## Content
- [ ] Asset loading pipeline
- [ ] Level/tilemap format and loader
- [ ] Data serialization

## Debugging
- [x] Leveled logging (INFO / ERROR / DEBUG)
- [ ] Debug draw: collision bounds, vectors
- [ ] Performance profiler
- [x] Immediate-mode debug UI (the F3 overlay, built on Clay)

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
- [ ] Linux: build and test (toolchain wired: build.sh)
- [ ] Windows: build and test (toolchain wired: build.bat)

> **Note: cross-platform got even simpler with RGFW.** One vendored header
> speaks Win32, X11, and Cocoa, and the renderer is GL 3.3 core, which all
> three platforms ship. Linux/Windows just need a C compiler; verify on real
> hardware when available.
