# sprites

Drop `.png` files here. `nob` bakes them into the executable at build time
(`build/generated/assets_sprites.h`), and the game decodes them at startup. The
game never reads this directory at runtime: it ships as one static, self-contained
binary per platform, and that is a hard constraint.

The file stem is the sprite's name. `ore.png` is looked up as `sprites_get(&g->sprites, "ore")`.

Art is optional and incremental. A sprite that isn't here yields a zeroed texture,
and the renderer draws the procedural shaded block it always drew. Adding one PNG
replaces one block; the game runs fine with none.

## Authoring

The engine's isometric footprint, from `MACH_ISO_TILE_W` / `MACH_ISO_TILE_H` /
`MACH_ISO_ELEV` in `src/mach.h`:

| thing | size |
|---|---|
| Ground tile (the 2:1 diamond) | 64x32 px |
| One unit of block elevation | 28 px of vertical body |

`assets/sprite_template.png` is an empty 64x32 canvas with the diamond outlined.
Open it in Aseprite as a starting layer, draw on top, delete the guide.

Textures upload with nearest-neighbour filtering, so pixels stay crisp. Author
against the engine's modus-vivendi palette (`MACH_COLOR_*`) rather than picking
colors freehand: the procedural ground and the value tint on ore still use it,
and the two need to sit together while the art lands piece by piece.

## Names the renderer looks for

| name | replaces | status |
|---|---|---|
| `ore` | the ore diamond | wired |

Dropper, belt, upgrader, and furnace still draw as blocks. Wiring each one is a
`sprites_get` call and a fallback branch in `render_game.c`, following `draw_item`.

## Hot reload

`./nob hot` watches this directory. Save a PNG in Aseprite and the running game
picks up the new art the same way it picks up a code change.
