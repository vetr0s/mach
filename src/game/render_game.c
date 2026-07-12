// Game scene rendering implementation (included into mach.c).

#include "render_game.h"
#include "world/world.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

// The look is the engine's stock modus-vivendi palette (engine/render/color.h):
// a near-black ground, machines in the theme's accent colors.

// Ground tile colors (subtle checker so the grid reads).
#define GROUND_A MACH_COLOR_BG_DIM
#define GROUND_B MACH_COLOR_HEX(0x252525) // between bg-dim and bg-inactive: a quiet checker

// Grid line: darker than the tiles so the diamonds separate cleanly.
#define GROUND_LINE MACH_COLOR_HEX(0x0f0f0f)

// Per-piece block heights (elevation units) and base colors.
#define DROPPER_H 0.80f
#define CONVEYOR_H 0.12f
#define UPGRADER_H 0.50f
#define SPLITTER_H 0.22f // a belt surface, raised just enough to read as its own machine
// The furnace is a shallow open bin, not a tower: a low rim about conveyor height
// with a recessed mouth, so ore drops into a sink instead of colliding with a tall
// block. FURNACE_MOUTH is the opening's radius in grid units (cell half is 0.5).
#define FURNACE_WALL_H 0.16f
#define FURNACE_MOUTH 0.34f

// Horizontal spread of a bank "+value" label at zoom 1, in pixels, times its jitter
// in [-1,1]: keeps simultaneous nearby payouts from stacking on the same spot.
#define BANK_LABEL_SPREAD_PX 22.0f

#define DROPPER_COL MACH_COLOR_GREEN
#define CONVEYOR_COL MACH_COLOR_BG_POPUP
#define UPGRADER_COL MACH_COLOR_MAGENTA_COOLER
#define FURNACE_COL MACH_COLOR_YELLOW_WARMER
#define SPLITTER_COL MACH_COLOR_CYAN

// How far each of a splitter's two arrows is nudged from the cell center toward its own
// output edge, in grid units, so the pair reads as two exits rather than one smear.
#define SPLITTER_ARROW_OFFSET 0.14f

// Belt surface: gray chevrons that scroll toward the flow direction so the
// black belt reads as running even when it's empty.
#define BELT_CHEVRON_COL MACH_COLOR_BG_ACTIVE
#define BELT_CHEVRONS 3
#define BELT_SCROLL_SPEED                                                                          \
    1.5f // chevron cycles/sec; kept below belt speed so 3 packed
         // chevrons read as a calm crawl, not a fast flicker

static f32 minf4(f32 a, f32 b, f32 c, f32 d) {
    f32 m = a;
    if (b < m)
        m = b;
    if (c < m)
        m = c;
    if (d < m)
        m = d;
    return m;
}
static f32 maxf4(f32 a, f32 b, f32 c, f32 d) {
    f32 m = a;
    if (b > m)
        m = b;
    if (c > m)
        m = c;
    if (d > m)
        m = d;
    return m;
}

// The four screen-space corners of grid cell (gx,gy) at the given elevation.
static void tile_corners(Mach_Renderer *r, const Mach_Camera2D *cam, f32 gx, f32 gy, f32 elev,
                         Mach_Vec2 *n, Mach_Vec2 *e, Mach_Vec2 *s, Mach_Vec2 *w) {
    f32 sw = (f32)r->width, sh = (f32)r->height;
    *n = mach_iso_to_screen(cam, sw, sh, gx - 0.5f, gy - 0.5f, elev);
    *e = mach_iso_to_screen(cam, sw, sh, gx + 0.5f, gy - 0.5f, elev);
    *s = mach_iso_to_screen(cam, sw, sh, gx + 0.5f, gy + 0.5f, elev);
    *w = mach_iso_to_screen(cam, sw, sh, gx - 0.5f, gy + 0.5f, elev);
}

// Flat diamond on the ground (elev 0).
static void draw_tile(Mach_Renderer *r, const Mach_Camera2D *cam, f32 gx, f32 gy,
                      Mach_Color color) {
    Mach_Vec2 n, e, s, w;
    tile_corners(r, cam, gx, gy, 0.0f, &n, &e, &s, &w);
    Mach_Vec2 pts[4] = {n, e, s, w};
    mach_r2d_fill_poly(r, pts, 4, color);
}

// Stroke the outline of a ground-level diamond (grid line, hover highlight).
static void draw_tile_edges(Mach_Renderer *r, const Mach_Camera2D *cam, f32 gx, f32 gy,
                            Mach_Color color) {
    Mach_Vec2 n, e, s, w;
    tile_corners(r, cam, gx, gy, 0.0f, &n, &e, &s, &w);
    Mach_Vec2 pts[4] = {n, e, s, w};
    mach_r2d_poly_outline(r, pts, 4, color);
}

// A shaded box: two visible front faces plus a top diamond, with outlined seams.
// Top is brightest, sides progressively darker, faking directional 3D.
static void draw_block(Mach_Renderer *r, const Mach_Camera2D *cam, f32 gx, f32 gy, f32 h,
                       Mach_Color base) {
    Mach_Vec2 tn, te, ts, tw; // top corners (elevated)
    Mach_Vec2 bn, be, bs, bw; // base corners (ground)
    tile_corners(r, cam, gx, gy, h, &tn, &te, &ts, &tw);
    tile_corners(r, cam, gx, gy, 0.0f, &bn, &be, &bs, &bw);
    (void)bn;
    (void)be;

    Mach_Vec2 left[4] = {tw, ts, bs, bw};  // S-W face
    Mach_Vec2 right[4] = {ts, te, be, bs}; // S-E face
    Mach_Vec2 top[4] = {tn, te, ts, tw};
    mach_r2d_fill_poly(r, left, 4, mach_color_shade(base, 0.62f));
    mach_r2d_fill_poly(r, right, 4, mach_color_shade(base, 0.46f));
    mach_r2d_fill_poly(r, top, 4, base);

    Mach_Color line = mach_color_shade(base, 0.30f);
    mach_r2d_poly_outline(r, top, 4, line);
    mach_r2d_poly_outline(r, left, 4, line);
    mach_r2d_poly_outline(r, right, 4, line);
}

// A flat triangle on a piece's top face, pointing the way it moves items. Built
// entirely in grid space (perpendicular in grid, not screen) so it lies flat on the
// tile and reads the same in every facing under the iso projection.
static void draw_arrow(Mach_Renderer *r, const Mach_Camera2D *cam, f32 gx, f32 gy, f32 elev,
                       Direction dir, Mach_Color color) {
    f32 sw = (f32)r->width, sh = (f32)r->height;
    f32 dx = (f32)DIR_DX[dir], dy = (f32)DIR_DY[dir];
    f32 px = -dy, py = dx; // perpendicular in grid space
    f32 fwd = 0.34f, back = 0.18f, halfw = 0.22f;

    Mach_Vec2 tip = mach_iso_to_screen(cam, sw, sh, gx + dx * fwd, gy + dy * fwd, elev);
    Mach_Vec2 b1 = mach_iso_to_screen(cam, sw, sh, gx - dx * back + px * halfw,
                                      gy - dy * back + py * halfw, elev);
    Mach_Vec2 b2 = mach_iso_to_screen(cam, sw, sh, gx - dx * back - px * halfw,
                                      gy - dy * back - py * halfw, elev);
    Mach_Vec2 pts[3] = {tip, b1, b2};
    mach_r2d_fill_poly(r, pts, 3, color);
    mach_r2d_poly_outline(r, pts, 3, mach_color_shade(color, 0.45f));
}

// A thick line between two grid-space points, drawn as a filled quad on the belt's
// top face so its width scales with zoom instead of being a hairline.
static void draw_belt_tread(Mach_Renderer *r, const Mach_Camera2D *cam, f32 ax, f32 ay, f32 bx,
                            f32 by, f32 half_thick, Mach_Color color) {
    f32 sw = (f32)r->width, sh = (f32)r->height;
    f32 vx = bx - ax, vy = by - ay;
    f32 len = sqrtf(vx * vx + vy * vy);
    if (len < 1e-5f)
        return;
    f32 nx = -vy / len * half_thick; // perpendicular offset in grid units
    f32 ny = vx / len * half_thick;
    Mach_Vec2 p0 = mach_iso_to_screen(cam, sw, sh, ax + nx, ay + ny, CONVEYOR_H);
    Mach_Vec2 p1 = mach_iso_to_screen(cam, sw, sh, bx + nx, by + ny, CONVEYOR_H);
    Mach_Vec2 p2 = mach_iso_to_screen(cam, sw, sh, bx - nx, by - ny, CONVEYOR_H);
    Mach_Vec2 p3 = mach_iso_to_screen(cam, sw, sh, ax - nx, ay - ny, CONVEYOR_H);
    Mach_Vec2 pts[4] = {p0, p1, p2, p3};
    mach_r2d_fill_poly(r, pts, 4, color);
}

// Scrolling chevrons on a conveyor's top face. `phase` in [0,1) advances over real
// time, sliding each chevron toward the flow direction, so the belt looks like it's
// running. Also doubles as the conveyor's direction cue.
static void draw_belt_surface(Mach_Renderer *r, const Mach_Camera2D *cam, f32 gx, f32 gy,
                              Direction dir, f32 phase, Mach_Color color) {
    f32 dx = (f32)DIR_DX[dir], dy = (f32)DIR_DY[dir];
    f32 px = -dy, py = dx; // perpendicular in grid space
    f32 halfw = 0.26f;     // chevron arm spread across the belt
    f32 depth = 0.14f;     // apex-to-tail offset along the flow
    f32 thick = 0.05f;     // half the tread width, in grid units
    // (npt): Cap travel so the tail never crosses the cell's back edge onto the
    // tile behind. The chevron spans [center - depth, center + depth] along flow,
    // so keeping the center within +-(0.5 - depth) keeps the whole thing inside.
    f32 travel = 0.5f - depth;

    for (i32 k = 0; k < BELT_CHEVRONS; k++) {
        f32 u = (f32)k / (f32)BELT_CHEVRONS + phase;
        u -= floorf(u);
        f32 s = (u - 0.5f) * 2.0f * travel; // chevron center along the flow axis
        f32 cx = gx + dx * s, cy = gy + dy * s;

        // (npt): Fade each chevron in as it enters the back of the cell and out as
        // it leaves the front, so it rises out of the belt instead of popping on top.
        Mach_Color c = color;
        c.w *= sinf(3.14159265f * u);

        f32 apx = cx + dx * depth, apy = cy + dy * depth;
        f32 tlx = cx - dx * depth + px * halfw, tly = cy - dy * depth + py * halfw;
        f32 trx = cx - dx * depth - px * halfw, trgy = cy - dy * depth - py * halfw;
        draw_belt_tread(r, cam, apx, apy, tlx, tly, thick, c);
        draw_belt_tread(r, cam, apx, apy, trx, trgy, thick, c);
    }
}

static i32 popcount_u64(u64 x) {
    i32 c = 0;
    while (x) {
        c += (i32)(x & 1u);
        x >>= 1;
    }
    return c;
}

// Item color reads its upgrade count: pale tan when fresh, hot gold once it's
// been through several upgraders.
static Mach_Color item_color(const Item *it) {
    f32 t = (f32)popcount_u64(it->upgraded_mask) / 6.0f;
    if (t > 1.0f)
        t = 1.0f;
    return mach_color_lerp(MACH_COLOR_YELLOW_FAINT, MACH_COLOR_YELLOW_WARMER, t);
}

// Draw the ore glyph (the sprite if assets/sprites/ore.png exists, else the
// procedural diamond) at a grid position with the given elevation, half-size and
// color. Returns the diamond's top screen point, where callers hang a label. Shared
// by the live belt ore and the transient effects, so the same thing always looks
// the same whether it is riding a belt or dropping off a dead end.
static Mach_Vec2 draw_ore(Mach_Renderer *r, const Mach_Camera2D *cam, const Sprites *sprites,
                          f32 gx, f32 gy, f32 elev, f32 s, Mach_Color col) {
    f32 sw = (f32)r->width, sh = (f32)r->height;
    Mach_Vec2 n = mach_iso_to_screen(cam, sw, sh, gx, gy - s, elev);
    Mach_R2D_Texture ore = sprites_get(sprites, "ore");
    if (ore.id) {
        Mach_Vec2 c = mach_iso_to_screen(cam, sw, sh, gx, gy, elev);
        f32 sc = cam->zoom;
        mach_r2d_sprite(r, ore, c.x - (f32)ore.w * sc * 0.5f, c.y - (f32)ore.h * sc * 0.5f, sc,
                        col);
    } else {
        Mach_Vec2 ee = mach_iso_to_screen(cam, sw, sh, gx + s, gy, elev);
        Mach_Vec2 ss = mach_iso_to_screen(cam, sw, sh, gx, gy + s, elev);
        Mach_Vec2 ww = mach_iso_to_screen(cam, sw, sh, gx - s, gy, elev);
        Mach_Vec2 pts[4] = {n, ee, ss, ww};
        mach_r2d_fill_poly(r, pts, 4, col);
        mach_r2d_poly_outline(r, pts, 4, mach_color_shade(col, 0.45f));
    }
    return n;
}

// The world-space text scale for value labels at the current zoom. The labels live
// in the scene (over the ore), not in the HUD, so they have to scale with the camera
// or they read as huge when zoomed out and tiny when zoomed in. Tuned so the default
// zoom (2.0) gives scale 1.0, clamped so it stays legible at both ends.
static f32 label_scale(const Mach_Camera2D *cam) {
    return mach_clamp(cam->zoom * 0.5f, 0.6f, 3.0f);
}

// A value tag centered on screen-x `sx` with its top at `sy`, drawn at text scale
// `sc`, drop-shadowed and faded by `a` in [0,1]. Shared by the live ore label and the
// effect labels, so a number over an ore reads the same whether it's riding, banking,
// or lost.
static void draw_value_tag(Mach_Renderer *r, f32 sx, f32 sy, const char *s, f32 sc, f32 a,
                           Mach_Color col) {
    f32 tx = sx - (f32)r->font->advance * sc * (f32)strlen(s) * 0.5f;
    col.w = a;
    mach_r2d_text(r, tx + sc, sy + sc, sc, s, mach_color_alpha(MACH_COLOR_BG_MAIN, 0.7f * a));
    mach_r2d_text(r, tx, sy, sc, s, col);
}

// A small ore glyph floating just above the belt at the item's cell, with its
// current value labeled above it. `alpha` is the fraction into the current sim tick,
// so the item slides from its previous cell to its current one instead of snapping.
// Only live belt ore is drawn here; an ore that banks or tips off a dead end is gone
// from the sim that tick and is animated by draw_effects instead.
static void draw_item(Mach_Renderer *r, const Mach_Camera2D *cam, const Sprites *sprites,
                      const Item *it, f32 alpha) {
    f32 sw = (f32)r->width, sh = (f32)r->height;
    f32 gx = (f32)it->prev_x + ((f32)it->grid_x - (f32)it->prev_x) * alpha;
    f32 gy = (f32)it->prev_y + ((f32)it->grid_y - (f32)it->prev_y) * alpha;
    f32 e = 0.34f, s = 0.18f;

    Mach_Vec2 n = draw_ore(r, cam, sprites, gx, gy, e, s, item_color(it));

    // Value label, centered over the diamond and sitting just above its top point.
    char buf[32];
    game_format_value(it->value, buf, sizeof buf);
    Mach_Vec2 c = mach_iso_to_screen(cam, sw, sh, gx, gy, e);
    f32 sc = label_scale(cam);
    draw_value_tag(r, c.x, n.y - (f32)r->font->glyph_h * sc - 4.0f, buf, sc, 1.0f,
                   MACH_COLOR_FG_MAIN);
}

// The transient visuals the sim can't hold: an ore banking, an ore tipping off a
// dead end. Each is driven by its own real-time age, so it plays smoothly and lives
// independently of the item it came from (which is already gone from the sim). This
// is where the "prettify" work lands later; the skeleton keeps it to what the old
// code already did (the drop-out) plus the banking that was invisible before.
static void draw_effects(Mach_Renderer *r, const Mach_Camera2D *cam, const Sprites *sprites,
                         const Effects *fx) {
    f32 sw = (f32)r->width, sh = (f32)r->height;
    for (i32 i = 0; i < fx->count; i++) {
        const Effect *e = &fx->items[i];
        f32 p = e->lifetime > 0.0f ? e->age / e->lifetime : 1.0f;
        if (p > 1.0f)
            p = 1.0f;

        if (e->type == EFFECT_FALL) {
            // Ore rides on toward the dead-end cell, sinking through the belt and fading.
            // It still shows its value on the way out, so a wasted ore reads as a loss.
            f32 gx = e->from_x + (e->to_x - e->from_x) * p;
            f32 gy = e->from_y + (e->to_y - e->from_y) * p;
            Mach_Color col = MACH_COLOR_YELLOW_WARMER;
            col.w = 1.0f - p;
            Mach_Vec2 n = draw_ore(r, cam, sprites, gx, gy, 0.34f - p * 0.9f,
                                   0.18f * (1.0f - 0.45f * p), col);
            char buf[32];
            game_format_value(e->value, buf, sizeof buf);
            f32 sc = label_scale(cam);
            draw_value_tag(r, n.x, n.y - (f32)r->font->glyph_h * sc - 4.0f, buf, sc, 1.0f - p,
                           MACH_COLOR_FG_MAIN);
            continue;
        }

        // EFFECT_BANK, in three beats: the ore slides to the furnace and STOPS over the
        // mouth (no more horizontal motion), settling onto the rim; it then fades out in
        // place; and only once it's gone (processed) does the "+value" rise. The sim
        // already banked the money the tick the ore arrived; this is just the show, so
        // the order reads as the furnace consuming the ore and then paying out.
        f32 arrive = 0.4f;   // sliding in, done by here
        f32 fade_end = 0.7f; // ore fully faded by here; payout runs after
        if (p < fade_end) {
            f32 s = p < arrive ? p / arrive : 1.0f; // horizontal progress; pins at 1 (stops)
            f32 gx = e->from_x + (e->to_x - e->from_x) * s;
            f32 gy = e->from_y + (e->to_y - e->from_y) * s;
            f32 elev = 0.34f + s * (FURNACE_WALL_H - 0.34f);                // settle onto the rim
            f32 f = p < arrive ? 0.0f : (p - arrive) / (fade_end - arrive); // fade in place
            Mach_Color col = MACH_COLOR_YELLOW_WARMER;
            col.w = 1.0f - f;
            draw_ore(r, cam, sprites, gx, gy, elev, 0.18f * (1.0f - 0.3f * f), col);
        } else {
            f32 q = (p - fade_end) / (1.0f - fade_end); // 0..1 over the payout beat
            char buf[33];
            buf[0] = '+';
            game_format_value(e->value, buf + 1, sizeof buf - 1);
            Mach_Vec2 c =
                mach_iso_to_screen(cam, sw, sh, e->to_x, e->to_y, FURNACE_WALL_H + 0.2f + q * 0.7f);
            f32 sc = label_scale(cam);
            // Spread the label horizontally by its jitter (scaled with zoom) so
            // simultaneous, nearby payouts stay separately readable.
            draw_value_tag(r, c.x + e->jitter * BANK_LABEL_SPREAD_PX * sc, c.y, buf, sc, 1.0f - q,
                           MACH_COLOR_GREEN);
        }
    }
}

// A furnace is a shallow open bin, not a tower: a short block only about conveyor
// height, with a darker recessed mouth inset into its top so the perimeter reads as a
// raised wall. Ore drops into the mouth (see the bank effect), so it reads as a sink.
static void draw_furnace(Mach_Renderer *r, const Mach_Camera2D *cam, f32 gx, f32 gy) {
    draw_block(r, cam, gx, gy, FURNACE_WALL_H, FURNACE_COL);

    f32 sw = (f32)r->width, sh = (f32)r->height;
    f32 m = FURNACE_MOUTH;
    Mach_Vec2 mn = mach_iso_to_screen(cam, sw, sh, gx, gy - m, FURNACE_WALL_H);
    Mach_Vec2 me = mach_iso_to_screen(cam, sw, sh, gx + m, gy, FURNACE_WALL_H);
    Mach_Vec2 ms = mach_iso_to_screen(cam, sw, sh, gx, gy + m, FURNACE_WALL_H);
    Mach_Vec2 mw = mach_iso_to_screen(cam, sw, sh, gx - m, gy, FURNACE_WALL_H);
    Mach_Vec2 mouth[4] = {mn, me, ms, mw};
    mach_r2d_fill_poly(r, mouth, 4, mach_color_shade(FURNACE_COL, 0.30f)); // dark interior
    mach_r2d_poly_outline(r, mouth, 4, mach_color_shade(FURNACE_COL, 0.22f));
}

// A splitter is a belt surface with two outputs. Both are drawn as arrows on its top
// face, each nudged toward the edge it leaves by: the one the next ore will take is lit,
// the other is dimmed. That makes the alternation readable at a glance, so you can see
// which way the next piece of ore will peel off without opening the inspect panel.
static void draw_splitter(Mach_Renderer *r, const Mach_Camera2D *cam, const Entity *e) {
    f32 gx = (f32)e->grid_x, gy = (f32)e->grid_y;
    draw_block(r, cam, gx, gy, SPLITTER_H, SPLITTER_COL);

    Direction primary = e->dir;
    Direction branch = world_splitter_out_dir(e);
    b32 branch_next = e->data.splitter.flip;

    Mach_Color lit = mach_color_lighten(SPLITTER_COL, 0.55f);
    Mach_Color dim = mach_color_shade(SPLITTER_COL, 0.50f);

    f32 po = SPLITTER_ARROW_OFFSET;
    draw_arrow(r, cam, gx + (f32)DIR_DX[primary] * po, gy + (f32)DIR_DY[primary] * po, SPLITTER_H,
               primary, branch_next ? dim : lit);
    draw_arrow(r, cam, gx + (f32)DIR_DX[branch] * po, gy + (f32)DIR_DY[branch] * po, SPLITTER_H,
               branch, branch_next ? lit : dim);
}

static void draw_entity(Mach_Renderer *r, const Mach_Camera2D *cam, const Entity *e,
                        f32 belt_phase) {
    f32 gx = (f32)e->grid_x, gy = (f32)e->grid_y;
    switch (e->type) {
    case ENTITY_DROPPER:
        draw_block(r, cam, gx, gy, DROPPER_H, DROPPER_COL);
        draw_arrow(r, cam, gx, gy, DROPPER_H, e->dir, mach_color_lighten(DROPPER_COL, 0.55f));
        break;
    case ENTITY_CONVEYOR:
        draw_block(r, cam, gx, gy, CONVEYOR_H, CONVEYOR_COL);
        draw_belt_surface(r, cam, gx, gy, e->dir, belt_phase, BELT_CHEVRON_COL);
        break;
    case ENTITY_UPGRADER:
        draw_block(r, cam, gx, gy, UPGRADER_H, UPGRADER_COL);
        draw_arrow(r, cam, gx, gy, UPGRADER_H, e->dir, mach_color_lighten(UPGRADER_COL, 0.55f));
        break;
    case ENTITY_SPLITTER:
        draw_splitter(r, cam, e);
        break;
    case ENTITY_FURNACE:
        draw_furnace(r, cam, gx, gy);
        break;
    default:
        break;
    }
}

// Depth-sort key for painter's order: far cells (smaller gx+gy) first.
typedef struct {
    f32 depth;
    const void *ptr;
} DrawItem;

static int cmp_draw(const void *a, const void *b) {
    f32 da = ((const DrawItem *)a)->depth, db = ((const DrawItem *)b)->depth;
    return (da < db) ? -1 : (da > db) ? 1 : 0;
}

void game_render_draw(Mach_Renderer *r, const Game_State *game, Mach_Arena *scratch) {
    const Mach_Camera2D *cam = &game->camera;
    World *w = game->world;
    f32 sw = (f32)r->width, sh = (f32)r->height;

    // Visible ground range: unproject the screen corners, take the grid bbox.
    Mach_Vec2 c0 = mach_screen_to_iso(cam, sw, sh, 0.0f, 0.0f);
    Mach_Vec2 c1 = mach_screen_to_iso(cam, sw, sh, sw, 0.0f);
    Mach_Vec2 c2 = mach_screen_to_iso(cam, sw, sh, 0.0f, sh);
    Mach_Vec2 c3 = mach_screen_to_iso(cam, sw, sh, sw, sh);
    i32 gx0 = (i32)minf4(c0.x, c1.x, c2.x, c3.x) - 1;
    i32 gx1 = (i32)maxf4(c0.x, c1.x, c2.x, c3.x) + 1;
    i32 gy0 = (i32)minf4(c0.y, c1.y, c2.y, c3.y) - 1;
    i32 gy1 = (i32)maxf4(c0.y, c1.y, c2.y, c3.y) + 1;
    if (gx0 < 0)
        gx0 = 0;
    if (gy0 < 0)
        gy0 = 0;
    if (gx1 >= WORLD_GRID_SIZE)
        gx1 = WORLD_GRID_SIZE - 1;
    if (gy1 >= WORLD_GRID_SIZE)
        gy1 = WORLD_GRID_SIZE - 1;

    // Only the unlocked region gets a ground checker; locked cells stay the black
    // clear color so the boundary reads as "not yours yet". Clip the visible bbox to
    // the region [lo, hi) on both axes.
    i32 plo, phi;
    world_playable_bounds(w, &plo, &phi);
    i32 rx0 = gx0 > plo ? gx0 : plo, rx1 = gx1 < phi - 1 ? gx1 : phi - 1;
    i32 ry0 = gy0 > plo ? gy0 : plo, ry1 = gy1 < phi - 1 ? gy1 : phi - 1;
    for (i32 gy = ry0; gy <= ry1; gy++) {
        for (i32 gx = rx0; gx <= rx1; gx++) {
            draw_tile(r, cam, (f32)gx, (f32)gy, ((gx + gy) & 1) ? GROUND_A : GROUND_B);
            draw_tile_edges(r, cam, (f32)gx, (f32)gy, GROUND_LINE);
        }
    }

    // A bright outline around the whole unlocked square. Cells [plo, phi-1] span grid
    // corners [plo-0.5, phi-0.5], which are this square's edges.
    f32 blo = (f32)plo - 0.5f, bhi = (f32)phi - 0.5f;
    Mach_Vec2 border[4] = {
        mach_iso_to_screen(cam, sw, sh, blo, blo, 0.0f),
        mach_iso_to_screen(cam, sw, sh, bhi, blo, 0.0f),
        mach_iso_to_screen(cam, sw, sh, bhi, bhi, 0.0f),
        mach_iso_to_screen(cam, sw, sh, blo, bhi, 0.0f),
    };
    mach_r2d_poly_outline(r, border, 4, mach_color_alpha(MACH_COLOR_FG_MAIN, 0.55f));

    // Hover preview: a highlighted tile over the ground, under everything else.
    if (game->hover_valid) {
        Mach_Color color;
        if (!game->hover_can_place)
            color = MACH_COLOR_RED;
        else if (game->selected_tool == TOOL_FURNACE)
            color = MACH_COLOR_YELLOW_WARMER;
        else
            color = MACH_COLOR_GREEN;
        draw_tile(r, cam, (f32)game->hover_grid_x, (f32)game->hover_grid_y, color);
        draw_tile_edges(r, cam, (f32)game->hover_grid_x, (f32)game->hover_grid_y,
                        mach_color_lighten(color, 0.5f));
    }

    if (!w)
        return;

    // Machines, back-to-front by (gx+gy) so nearer blocks overdraw farther ones.
    // Sort buffers come from the frame arena: sized to what's alive, gone next frame.
    DrawItem *ents =
        (DrawItem *)mach_arena_alloc(scratch, (usize)w->entity_count * sizeof(DrawItem));
    if (w->entity_count > 0 && !ents)
        return;
    i32 ne = 0;
    for (i32 i = 0; i < w->entity_count; i++) {
        const Entity *e = &w->entities[i];
        ents[ne].depth = (f32)(e->grid_x + e->grid_y);
        ents[ne].ptr = e;
        ne++;
    }
    f32 belt_phase = game->anim_time * BELT_SCROLL_SPEED;
    belt_phase -= floorf(belt_phase);
    qsort(ents, (size_t)ne, sizeof(DrawItem), cmp_draw);
    for (i32 i = 0; i < ne; i++)
        draw_entity(r, cam, (const Entity *)ents[i].ptr, belt_phase);

    // Items ride on top of the belts, also depth-sorted among themselves. The
    // leftover sim accumulator gives the fraction into the current tick, which
    // slides each item smoothly from its previous cell to its current one.
    f32 alpha = game->sim_accumulator / SIM_TICK_DT;
    if (alpha < 0.0f)
        alpha = 0.0f;
    if (alpha > 1.0f)
        alpha = 1.0f;

    DrawItem *items =
        (DrawItem *)mach_arena_alloc(scratch, (usize)w->item_count * sizeof(DrawItem));
    if (w->item_count > 0 && !items)
        return;
    i32 ni = 0;
    for (i32 i = 0; i < MAX_ITEMS && ni < w->item_count; i++) {
        const Item *it = &w->items[i];
        if (!it->alive)
            continue;
        items[ni].depth = (f32)(it->grid_x + it->grid_y);
        items[ni].ptr = it;
        ni++;
    }
    qsort(items, (size_t)ni, sizeof(DrawItem), cmp_draw);
    for (i32 i = 0; i < ni; i++)
        draw_item(r, cam, &game->sprites, (const Item *)items[i].ptr, alpha);

    // Transient effects on top of the world. Not depth-sorted with entities yet:
    // they are brief sparks, and correct occlusion is a prettify concern, not skeleton.
    draw_effects(r, cam, &game->sprites, &game->effects);
}

// ---------------------------------------------------------------------------
// HUD: Clay floating panels pinned to the screen edges.
// ---------------------------------------------------------------------------

// HUD colors, from the engine palette (mach_clay_color_of converts to Clay's 0-255).
#define HUD_PANEL mach_clay_color_of(mach_color_alpha(MACH_COLOR_BG_DIM, 0.86f))
#define HUD_GOLD mach_clay_color_of(MACH_COLOR_YELLOW_WARMER)
#define HUD_GREEN mach_clay_color_of(MACH_COLOR_GREEN)
#define HUD_AMBER mach_clay_color_of(MACH_COLOR_YELLOW)
#define HUD_GREY mach_clay_color_of(MACH_COLOR_FG_DIM)
#define HUD_WHITE mach_clay_color_of(MACH_COLOR_FG_MAIN)
#define HUD_PURPLE mach_clay_color_of(MACH_COLOR_MAGENTA_COOLER)
#define HUD_CYAN mach_clay_color_of(MACH_COLOR_CYAN)

// Shop button backgrounds: resting, hovered, and selected.
#define HUD_BTN mach_clay_color_of(MACH_COLOR_BG_ACTIVE)
#define HUD_BTN_HOVER mach_clay_color_of(MACH_COLOR_BG_HOVER)
#define HUD_BTN_ACTIVE mach_clay_color_of(MACH_COLOR_BG_BLUE_SUBTLE)

// A clickable full-width row: its id is used for hit-testing after layout; the
// background brightens on hover and stays lit when `active`.
#define SHOP_BTN(id, active)                                                                       \
    CLAY(CLAY_ID(id),                                                                              \
         {.layout = {.padding = CLAY_PADDING_ALL(5), .sizing = {.width = CLAY_SIZING_GROW(0)}},    \
          .backgroundColor =                                                                       \
              (active) ? HUD_BTN_ACTIVE : (Clay_Hovered() ? HUD_BTN_HOVER : HUD_BTN),              \
          .cornerRadius = CLAY_CORNER_RADIUS(4)})

// A floating HUD panel pinned to a screen corner/edge: the same attach point on
// the element and the root, nudged inward by (ox, oy). Follow with a block of
// CLAY_TEXT children.
#define HUD_PANEL_AT(name, attach, ox, oy)                                                         \
    CLAY(CLAY_ID(name),                                                                            \
         {.layout = {.layoutDirection = CLAY_TOP_TO_BOTTOM,                                        \
                     .padding = CLAY_PADDING_ALL(10),                                              \
                     .childGap = 4,                                                                \
                     .sizing = {.width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0)}},       \
          .backgroundColor = HUD_PANEL,                                                            \
          .cornerRadius = CLAY_CORNER_RADIUS(6),                                                   \
          .floating = {.attachTo = CLAY_ATTACH_TO_ROOT,                                            \
                       .attachPoints = {.element = attach, .parent = attach},                      \
                       .offset = {ox, oy}}})

void game_render_hud(Game_State *g, Mach *m) {
    Mach_Renderer *r = &m->r2d;

    static const char *tool_names[] = {"None",    "Dropper", "Conveyor", "Upgrader",
                                       "Furnace", "Delete",  "Splitter"};
    static const char *dir_names[] = {"N", "E", "S", "W"};
    static const char *entity_names[] = {"",         "Dropper", "Conveyor",
                                         "Upgrader", "Furnace", "Splitter"};
    const char *tool =
        (g->selected_tool >= 0 && g->selected_tool < (i32)MACH_ARRAY_COUNT(tool_names))
            ? tool_names[g->selected_tool]
            : "?";
    const char *facing = (g->place_dir >= 0 && g->place_dir < (i32)MACH_ARRAY_COUNT(dir_names))
                             ? dir_names[g->place_dir]
                             : "?";

    // HUD strings. These buffers must outlive mach_clay_ui_render (Clay keeps the char
    // pointers, not copies), so they stay in scope for the whole function.
    char money_s[32], tool_s[48], fps_s[40], counts_s[64], hover_s[48], cam_s[48];
    char val_a[24], val_b[24], banked_s[24];
    char insp_sub_s[32], insp_extra_s[48], insp_item_s[64];
    char drop_s[28], conv_s[28], upg_s[28], furn_s[28], split_s[28], tier_s[24], expand_s[40];
    snprintf(money_s, sizeof(money_s), "$%lld", (long long)(g->world ? g->world->money : 0));
    snprintf(tool_s, sizeof(tool_s), "%s   facing %s", tool, facing);
    // fps reads flat under vsync, so show frame_ms (the real work per frame) and its
    // peak beside it: that's the headroom number, and where a hitch actually shows.
    snprintf(fps_s, sizeof(fps_s), "fps %d   %.1f/%.1fms", m->fps, m->frame_ms, m->frame_ms_peak);
    snprintf(counts_s, sizeof(counts_s), "tick %d   entities %d   items %d",
             g->world ? g->world->tick : 0, g->world ? g->world->entity_count : 0,
             g->world ? g->world->item_count : 0);
    if (g->hover_valid)
        snprintf(hover_s, sizeof(hover_s), "hover %d,%d%s", g->hover_grid_x, g->hover_grid_y,
                 g->hover_can_place ? "" : " (blocked)");
    else
        snprintf(hover_s, sizeof(hover_s), "hover --");
    snprintf(cam_s, sizeof(cam_s), "cam %.0f,%.0f   zoom %.2f", g->camera.pan.x, g->camera.pan.y,
             g->camera.zoom);

    // Shop labels: dropper/upgrader prices track the selected tier; conveyor/furnace
    // are flat. Expansion shows the next region's price, or "Max" at the cap.
    i32 tier = g->selected_tier;
    snprintf(drop_s, sizeof drop_s, "Dropper T%d  $%lld", tier,
             (long long)world_entity_cost(ENTITY_DROPPER, tier));
    snprintf(conv_s, sizeof conv_s, "Conveyor  $%lld",
             (long long)world_entity_cost(ENTITY_CONVEYOR, 1));
    snprintf(upg_s, sizeof upg_s, "Upgrader T%d  $%lld", tier,
             (long long)world_entity_cost(ENTITY_UPGRADER, tier));
    snprintf(furn_s, sizeof furn_s, "Furnace  $%lld",
             (long long)world_entity_cost(ENTITY_FURNACE, 1));
    snprintf(split_s, sizeof split_s, "Splitter  $%lld",
             (long long)world_entity_cost(ENTITY_SPLITTER, 1));
    snprintf(tier_s, sizeof tier_s, "Tier %d  (click +)", tier);
    i64 expand_cost = g->world ? world_expand_cost(g->world) : 0;
    if (expand_cost > 0)
        snprintf(expand_s, sizeof expand_s, "Expand  $%lld", (long long)expand_cost);
    else
        snprintf(expand_s, sizeof expand_s, "Max size");

    // Inspect: describe whatever sits under the cursor (machine and/or ore).
    const Entity *ins = NULL;
    const Item *ore = NULL;
    if (g->world && g->hover_valid) {
        ins = world_get_entity(g->world,
                               world_get_entity_at(g->world, g->hover_grid_x, g->hover_grid_y));
        ore = world_get_item_at(g->world, g->hover_grid_x, g->hover_grid_y);
    }
    insp_sub_s[0] = insp_extra_s[0] = insp_item_s[0] = '\0';
    const char *insp_title = "Ore";
    Clay_Color insp_title_col = HUD_GOLD;
    if (ins) {
        insp_title = entity_names[ins->type];
        switch (ins->type) {
        case ENTITY_DROPPER:
            insp_title_col = HUD_GREEN;
            snprintf(insp_sub_s, sizeof insp_sub_s, "facing %s", dir_names[ins->dir]);
            snprintf(insp_extra_s, sizeof insp_extra_s, "next drop in %dt",
                     ins->data.dropper.drop_cooldown);
            break;
        case ENTITY_CONVEYOR:
            insp_title_col = HUD_WHITE;
            snprintf(insp_sub_s, sizeof insp_sub_s, "facing %s", dir_names[ins->dir]);
            break;
        case ENTITY_UPGRADER:
            insp_title_col = HUD_PURPLE;
            snprintf(insp_sub_s, sizeof insp_sub_s, "facing %s", dir_names[ins->dir]);
            break;
        case ENTITY_SPLITTER: {
            insp_title_col = HUD_CYAN;
            Direction br = world_splitter_out_dir(ins);
            snprintf(insp_sub_s, sizeof insp_sub_s, "out %s / %s  (T turns)", dir_names[ins->dir],
                     dir_names[br]);
            snprintf(insp_extra_s, sizeof insp_extra_s, "next ore goes %s",
                     dir_names[ins->data.splitter.flip ? br : ins->dir]);
            break;
        }
        case ENTITY_FURNACE:
            insp_title_col = HUD_GOLD;
            game_format_value(ins->data.furnace.banked, banked_s, sizeof banked_s);
            snprintf(insp_extra_s, sizeof insp_extra_s, "banked $%s", banked_s);
            break;
        default:
            break;
        }
    }
    if (ore) {
        game_format_value(ore->value, val_a, sizeof val_a);
        game_format_value(ore->ceiling, val_b, sizeof val_b);
        snprintf(insp_item_s, sizeof insp_item_s, "ore $%s / max $%s", val_a, val_b);
    }

    // Floating panels around the screen edges. Font sizes are multiples of the 8px
    // bitmap glyph (16 -> 2x, 8 -> 1x) so the text stays crisp.
    const Mach_Input *in = &m->input;
    mach_clay_ui_begin(&g->clay, r, (Clay_Vector2){in->mouse.x, in->mouse.y},
                       in->mouse_down[MACH_MOUSE_LEFT]);

    // Top-left: the always-on status.
    HUD_PANEL_AT("hud-status", CLAY_ATTACH_POINT_LEFT_TOP, 10, 10) {
        CLAY_TEXT(mach_clay_string(money_s),
                  CLAY_TEXT_CONFIG({.fontSize = 16, .textColor = HUD_GOLD}));
        CLAY_TEXT(mach_clay_string(tool_s),
                  CLAY_TEXT_CONFIG({.fontSize = 8, .textColor = HUD_GREEN}));
        if (g->paused)
            CLAY_TEXT(CLAY_STRING("PAUSED"),
                      CLAY_TEXT_CONFIG({.fontSize = 8, .textColor = HUD_AMBER}));
    }

    // Top-center: what the cursor is pointing at.
    if (ins || ore) {
        HUD_PANEL_AT("hud-inspect", CLAY_ATTACH_POINT_CENTER_TOP, 0, 10) {
            CLAY_TEXT(mach_clay_string(insp_title),
                      CLAY_TEXT_CONFIG({.fontSize = 16, .textColor = insp_title_col}));
            if (insp_sub_s[0])
                CLAY_TEXT(mach_clay_string(insp_sub_s),
                          CLAY_TEXT_CONFIG({.fontSize = 8, .textColor = HUD_GREY}));
            if (insp_extra_s[0])
                CLAY_TEXT(mach_clay_string(insp_extra_s),
                          CLAY_TEXT_CONFIG({.fontSize = 8, .textColor = HUD_GOLD}));
            if (insp_item_s[0])
                CLAY_TEXT(mach_clay_string(insp_item_s),
                          CLAY_TEXT_CONFIG({.fontSize = 8, .textColor = HUD_WHITE}));
        }
    }

    // Bottom-left: the backtick-toggled diagnostics.
    if (g->show_debug) {
        HUD_PANEL_AT("hud-debug", CLAY_ATTACH_POINT_LEFT_BOTTOM, 10, -10) {
            CLAY_TEXT(mach_clay_string(fps_s),
                      CLAY_TEXT_CONFIG({.fontSize = 8, .textColor = HUD_GREY}));
            CLAY_TEXT(mach_clay_string(counts_s),
                      CLAY_TEXT_CONFIG({.fontSize = 8, .textColor = HUD_GREY}));
            CLAY_TEXT(mach_clay_string(hover_s),
                      CLAY_TEXT_CONFIG({.fontSize = 8, .textColor = HUD_GREY}));
            CLAY_TEXT(mach_clay_string(cam_s),
                      CLAY_TEXT_CONFIG({.fontSize = 8, .textColor = HUD_GREY}));
        }
    }

    // Bottom-center: the controls, on screen until a real menu exists to teach them.
    HUD_PANEL_AT("hud-controls", CLAY_ATTACH_POINT_CENTER_BOTTOM, 0, -10) {
        CLAY_TEXT(CLAY_STRING("(1)drop (2)belt (3)upgr (4)furnace (5)del (6)split (R)rotate "
                              "(T)branch (Space)pause (`)info"),
                  CLAY_TEXT_CONFIG({.fontSize = 8, .textColor = HUD_GREY}));
    }

    // Right side: the shop. Click a tool to select it, the tier row to cycle the tier
    // bought, or Expand to enlarge the region. Clicks are handled after layout below.
    HUD_PANEL_AT("shop", CLAY_ATTACH_POINT_RIGHT_TOP, -10, 10) {
        CLAY_TEXT(CLAY_STRING("BUILD"), CLAY_TEXT_CONFIG({.fontSize = 8, .textColor = HUD_GREY}));
        SHOP_BTN("shop-drop", g->selected_tool == TOOL_DROPPER) {
            CLAY_TEXT(mach_clay_string(drop_s),
                      CLAY_TEXT_CONFIG({.fontSize = 8, .textColor = HUD_GREEN}));
        }
        SHOP_BTN("shop-conv", g->selected_tool == TOOL_CONVEYOR) {
            CLAY_TEXT(mach_clay_string(conv_s),
                      CLAY_TEXT_CONFIG({.fontSize = 8, .textColor = HUD_WHITE}));
        }
        SHOP_BTN("shop-split", g->selected_tool == TOOL_SPLITTER) {
            CLAY_TEXT(mach_clay_string(split_s),
                      CLAY_TEXT_CONFIG({.fontSize = 8, .textColor = HUD_CYAN}));
        }
        SHOP_BTN("shop-upg", g->selected_tool == TOOL_UPGRADER) {
            CLAY_TEXT(mach_clay_string(upg_s),
                      CLAY_TEXT_CONFIG({.fontSize = 8, .textColor = HUD_PURPLE}));
        }
        SHOP_BTN("shop-furn", g->selected_tool == TOOL_FURNACE) {
            CLAY_TEXT(mach_clay_string(furn_s),
                      CLAY_TEXT_CONFIG({.fontSize = 8, .textColor = HUD_GOLD}));
        }
        SHOP_BTN("shop-del", g->selected_tool == TOOL_DELETE) {
            CLAY_TEXT(CLAY_STRING("Delete"),
                      CLAY_TEXT_CONFIG({.fontSize = 8, .textColor = HUD_GREY}));
        }
        SHOP_BTN("shop-tier", MACH_FALSE) {
            CLAY_TEXT(mach_clay_string(tier_s),
                      CLAY_TEXT_CONFIG({.fontSize = 8, .textColor = HUD_AMBER}));
        }
        SHOP_BTN("shop-expand", MACH_FALSE) {
            CLAY_TEXT(mach_clay_string(expand_s),
                      CLAY_TEXT_CONFIG({.fontSize = 8, .textColor = HUD_WHITE}));
        }
    }

    mach_clay_ui_render(&g->clay, r);

    // Handle shop clicks against the finished layout (Clay_PointerOver reads this
    // frame's layout at this frame's pointer). Selecting a tool toggles it, matching
    // the number keys; the tier row cycles 1..MAX_TIER; Expand buys the next region.
    b32 clicked = in->mouse_pressed[MACH_MOUSE_LEFT];
    if (clicked) {
        if (Clay_PointerOver(Clay_GetElementId(CLAY_STRING("shop-drop"))))
            g->selected_tool = (g->selected_tool == TOOL_DROPPER) ? TOOL_NONE : TOOL_DROPPER;
        else if (Clay_PointerOver(Clay_GetElementId(CLAY_STRING("shop-conv"))))
            g->selected_tool = (g->selected_tool == TOOL_CONVEYOR) ? TOOL_NONE : TOOL_CONVEYOR;
        else if (Clay_PointerOver(Clay_GetElementId(CLAY_STRING("shop-split"))))
            g->selected_tool = (g->selected_tool == TOOL_SPLITTER) ? TOOL_NONE : TOOL_SPLITTER;
        else if (Clay_PointerOver(Clay_GetElementId(CLAY_STRING("shop-upg"))))
            g->selected_tool = (g->selected_tool == TOOL_UPGRADER) ? TOOL_NONE : TOOL_UPGRADER;
        else if (Clay_PointerOver(Clay_GetElementId(CLAY_STRING("shop-furn"))))
            g->selected_tool = (g->selected_tool == TOOL_FURNACE) ? TOOL_NONE : TOOL_FURNACE;
        else if (Clay_PointerOver(Clay_GetElementId(CLAY_STRING("shop-del"))))
            g->selected_tool = (g->selected_tool == TOOL_DELETE) ? TOOL_NONE : TOOL_DELETE;
        else if (Clay_PointerOver(Clay_GetElementId(CLAY_STRING("shop-tier"))))
            g->selected_tier = (g->selected_tier >= MAX_TIER) ? 1 : g->selected_tier + 1;
        else if (Clay_PointerOver(Clay_GetElementId(CLAY_STRING("shop-expand"))))
            world_expand(g->world);
    }
    // Remember for next frame's input: a click over the shop must not place a tile.
    g->pointer_over_ui = Clay_PointerOver(Clay_GetElementId(CLAY_STRING("shop")));
}
