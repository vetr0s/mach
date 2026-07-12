# mach (the game): roadmap

Game work only. Engine work lives in the mach.h repo's TODO; this repo carries
the engine as a committed header at `src/mach.h`.

## NOTES

- REPO SPLIT (2026-07-06): the engine moved to its own repo, **mach.h**, with
  its own versioning (starting 0.1.0). This repo carries a copy at `src/mach.h`
  (moved out of `third_party/` 2026-07-08, it's our own library, not a third
  party); update by copying a new release in and fixing what its changelog says
  changed. Engine bugs found here get proven against the copy here, then move
  upstream.

- RESOLVED (2026-07-06): the void linux build errors. Two separate problems:
  - `Font` collided with X11's `typedef XID Font`. Every engine name is now
    `Mach_`/`mach_`/`MACH_`-prefixed, so no engine name can shadow an OS-header
    name on any platform. `scripts/check_namespace.sh` compile-checks the
    engine + game declarations against faked X11/Win32 names.
  - `GL/glx.h` not found is just missing dev headers. `./nob` preflights
    the X11/GLX headers on Linux and prints per-distro install commands; on void:
    `sudo xbps-install -S libX11-devel libXrandr-devel libXcursor-devel libXext-devel libXi-devel libglvnd-devel`.
  Verified: the game builds and runs on the linux box with those installed.

- RE: gdd, think city skylines mixed with miners haven and a pinch of factorio.
  The design doc (docs/gdd.typ, v0.5.0) is authoritative and now describes the
  value-loop builder in full; the Gameplay section below tracks catching the code
  up to it.

## Gameplay (value-loop belt builder)
Design target: docs/gdd.typ (v0.5.0). Items below the "value model rework" are
mechanics the GDD calls for that the code hasn't caught up to yet, in priority order.

- [x] Entity system (fat structs + direct arrays)
- [x] Fixed simulation timestep: the world steps at a constant rate (3/s in
      game.c), decoupled from the render framerate
- [x] Core loop: droppers emit items, conveyors route them, upgraders raise value,
      furnaces bank it. world_tick runs the belt sim.
- [x] Belt movement: one item per cell, gap-aware advance (a settle pass so a belt
      with a gap fully advances in one tick; a packed loop correctly jams)
- [x] Directional placement with rotation (R), money HUD, item/arrow rendering
- [x] Upgraders bound "once per upgrader, per item" (u64 touched-mask, 64 cap).
      SUPERSEDED: the design now makes recirculating loops central, which the
      once-per rule made pointless. Replaced by the value model rework below.
- [x] **Value model rework (GDD Milestone 2).** "Diminishing per pass, uncapped roof":
      an ore climbs toward a value ceiling, each pass closing a fixed fraction of the gap
      so a single loop settles and looping forever asymptotes. The once-per touched-mask
      is repurposed: each *distinct* upgrader multiplies the ceiling once, so more/stronger
      upgraders raise the (uncapped) roof. In world.{c,h} (Item.ceiling, item_apply_cell,
      UPGRADER_CEILING_MULT / UPGRADER_CLIMB_DIVISOR). Ore value now renders above its sprite.
      Tuning of the exact curve/constants is deferred (see GDD Open Questions).
- [x] **Money economy (GDD Milestone 3, the progression spine).** Money is a spendable
      balance now. Grid expansion (World.playable_side, a centered 2^n square; world_expand
      doubles it for an area-scaled price; placement confined to the region). Buyable tiers
      for droppers/upgraders (Entity.tier scales ore value / ceiling lift; cost scales with
      tier) via world_try_place. Shop HUD (right panel: tool + tier + expand buttons, with
      click-consumption so a UI click doesn't place a tile). Belt-speed tiers still deferred
      (need the parked movement-cadence rework). Left to tune: the cost/tier curve by feel.
- [x] Rename collector -> furnace throughout (ENTITY_FURNACE / TOOL_FURNACE /
      Entity_Furnace, world_spawn_furnace, and the UI strings). GDD naming decision.
      The furnace also became a shallow walled bin visually, not a tall block.
- [ ] Machine tiers plumbing: a `tier` field scaling one stat. Dropper/upgrader tiers
      are a value bump today (ITEM_BASE_VALUE * tier, UPGRADER_CEILING_MULT +
      UPGRADER_TIER_STEP). Belt-speed tiers need belt
      speed moved off the global sim clock onto a per-entity ticks-per-cell cadence, with
      item interpolation spanning that multi-tick window instead of one-move-per-tick.
- [x] **Splitter (GDD Milestone 3.5, the missing half of the loop).** A belt surface with two
      outputs that alternates between them. Before this, a recirculating loop, the GDD's
      signature move, was physically unbuildable: every object has a single facing, so ore
      that entered a cycle rode it forever and packed the cycle into a permanent jam. A
      headless probe confirmed it: an 8-cell loop with an upgrader on it banked $0 over 2000
      ticks and deadlocked with 8 stranded ore. The splitter is the tap that drains a loop.
      ENTITY_SPLITTER / TOOL_SPLITTER (key 6), `Entity_Splitter{branch, flip}` where `branch`
      is quarter-turns clockwise from the facing (1..3, so the two outputs can never be the
      same cell and rotating the piece carries the branch with it). T turns just the branch.
      It prefers the output its alternation points at but takes the other if that one is
      backed up, because strict alternation deadlocks a loop exactly when it fills. Save
      format is v2 (v1 files still load: they predate the type, so they cannot contain one).
      Measured: the same loop now banks steadily, holds 3-4 ore on 8 cells instead of packing
      to 8, and beats an equivalent straight line ($31.9k vs $25.0k over 3000 ticks).
- [ ] Special upgraders (caps, value gates, multipliers with conditions). A value-gated
      splitter (peel off only once the ore has stopped climbing) is the obvious next one now
      that the round-robin splitter proves the two-output plumbing.
- [x] Save/load a layout (full world state: objects + tiers, ore in flight, money,
      unlocked grid size, camera). One slot, versioned binary blob in save.c, written
      field by field for cross-platform stability; grids rebuilt on load. The menu's
      Continue loads; K / L are the in-game shortcuts. Bad/missing files fail cleanly.
      Format is at v3 (the widened upgrader bitmaps); v1 and v2 files still load.
- [ ] Open (see GDD Open Questions): one furnace only (single sink, strong spatial
      constraint) vs many placeable; whether furnaces become a 4th upgrade axis

## Rendering (game-side, in render_game.c)
- [x] Isometric look: viewport-culled ground checker, shaded blocks (top + two
      side faces), depth-sorted painter's order by gx+gy, edge outlines
- [x] Placement validation: red/green hover preview
- [x] Facing arrows that read clearly in the iso projection
- [x] Viewport-cull entities and items in render. Culled in SCREEN space, not on the
      ground bbox: a block's height and an ore's value label hang outside the cell by an
      amount measured in pixels, not cells, so a cell-counted margin is right at one zoom
      and pops machines at every other. The backtick overlay reads drawn/total so a bad
      margin is visible. A headless probe asserts the property that matters: never cull
      something visible.
- [x] Sprite pipeline: nob bakes assets/sprites/*.png into a generated header, the game
      decodes them at startup (mach.h v0.1.4's mach_r2d_texture_from_memory) and uploads
      with nearest filtering. Nothing is read from disk at runtime, so the single static
      binary per platform stays self-contained. `./nob hot` re-bakes on save, so art
      hot-reloads like code. A missing sprite falls back to the procedural block, so art
      lands one piece at a time. See assets/sprites/README.md.
- [ ] Real sprite art: `ore` is wired to a sprite when present. Dropper, belt, upgrader,
      and furnace still draw as blocks; each is a sprites_get call plus a fallback branch
      in render_game.c, following draw_item.

## UI & controls
- [x] Clay drives the HUD (Clay is embedded in mach.h; the binding is the engine's
      mach_clay_ui_* section). Reach for Clay for all new UI.
- [x] Main menu (title screen): App_Screen SCREEN_MENU/SCREEN_PLAYING/SCREEN_PAUSED,
      game_frame dispatches. New Game (game_new), Continue (save_exists + game_load),
      Quit (m->running = false). menu.c, launches here.
- [x] Pause menu: Escape (escape_quits now off) freezes the game and pops a scrim +
      panel over it, Resume / Main Menu. Escape again resumes. In menu.c.
- [x] HUD spread to the screen edges via Clay floating panels: status top-left,
      inspect top-center, debug bottom-left (backtick), controls bottom-center.
- [x] Hover inspect panel (WTHIT-style): names the machine under the cursor, facing,
      dropper cooldown, furnace banked total, and the ore's value / ceiling.
- [x] Pause / resume the simulation (Space): freezes sim ticks and animation; build and
      pan still work while frozen.
- [x] Rotate the hovered piece in place (R); over an empty tile it rotates the facing the
      next placed piece will use.
- [x] Debug/info overlay on the backtick key (fps, tick/entity/item counts, hover cell,
      camera, controls), toggled and small so it stays out of the way.
- [x] Interactive UI: the right-hand shop panel (tools, tiers, grid expansion), clicks
      consumed by the UI before world placement (pointer_over_ui, set from Clay_PointerOver
      over every HUD panel, checked before place_at_hover).

## Feel & polish (backlog)
Things to make it play and look right, batched for later sessions. Not urgent.
- [x] Smooth item travel: items slide between cells along the belt instead of
      snapping cell to cell, so they read as real objects being carried
- [x] Belt surface animation: the conveyor top scrolls chevrons in its facing
      direction (real-time, so it runs even when the belt is empty)
- [ ] Sprites for pieces and items, replacing the flat shaded blocks/diamonds
- [x] Item despawn handling. Deleting an entity despawns the ore on its cell; ore that
      hits a dead end (off-grid edge, bare ground, or a dropper's back) tips off and is
      killed that tick. The sim emits a World_Event (FELL / BANKED) and the renderer turns
      it into a real-time Effect with the sink-and-fade: visual timing lives in effects.c,
      never in the sim. See world_despawn / item_kill (world.c), game_sync_effects (game.c).
- [x] Tune speed and feel: chunky tier-1 baseline, belts 3 cells/s (SIM_TICKS_PER_SEC),
      an item every 2 cells (DROP_PERIOD), chevron scroll matched to belt speed.
      Value curve (ITEM_BASE_VALUE, UPGRADER_CEILING_MULT, UPGRADER_TIER_STEP,
      UPGRADER_CLIMB_DIVISOR) left as-is; revisit with the economy. NOTE: the drop cadence
      was off by one until the audit batch (droppers fired every 3 ticks, not 2), so every
      number in the economy was tuned against 2/3 of the real income. It all wants a
      playtest pass.

## Content
- [x] Asset embedding: build-time bake of assets/sprites/*.png into the executable.
      Runtime file loading is off the table; the binary ships alone.
- [ ] Level/tilemap format and loader
- [x] Data serialization (save.c: versioned binary reader/writer; feeds save/load)

## Docs
- [x] Reconcile the GDD, README and TODO with the code (docs/VERSION -> 0.5.0). The GDD
      had drifted: it cited deleted symbols (FALL_TICKS, item_begin_fall), claimed sprites
      shipped when assets/sprites/ is empty, described a 2x2 starting region that is really
      4x4, sold belt tiers as a third live upgrade axis, and listed four object types after
      the splitter made five. The stack description (SDL3) had already been fixed earlier.

## Dev tooling
- [x] Hot reload: game logic compiles to a shared lib the host (src/host.c)
      dlopen's and swaps on rebuild, keeping Game_State across reloads.
      `./nob hot` is the whole loop. The reload seam is the four game_*
      functions. Limit: code, not struct-layout, changes reload live.
- [ ] Level editor (future)
- [ ] Sprite / scene editors (future, if sprites earn them)

## Platform
- [x] macOS: building and running
- [x] Linux: building and running on real hardware. Deps preflight + namespace guard
      are in place; see NOTES.
- [x] Windows: building and running on real hardware (nob.c's cl.exe path, /MT so the
      exe needs no Visual C++ redistributable)
