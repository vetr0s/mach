// Game scene rendering implementation (included into mach.c).

#include "render_game.h"
#include "world/world.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

// Ground tile colors (subtle checker so the grid reads).
#define GROUND_A ((Vec4){0.16f, 0.20f, 0.24f, 1.0f})
#define GROUND_B ((Vec4){0.20f, 0.25f, 0.30f, 1.0f})

// Grid line: a touch darker than the tiles so the diamonds separate cleanly.
#define GROUND_LINE ((Vec4){0.10f, 0.13f, 0.16f, 1.0f})

// Per-piece block heights (elevation units) and base colors.
#define DROPPER_H    0.80f
#define CONVEYOR_H   0.12f
#define UPGRADER_H   0.50f
#define COLLECTOR_H  0.85f

#define DROPPER_COL   ((Vec4){0.30f, 0.62f, 0.36f, 1.0f})
#define CONVEYOR_COL  ((Vec4){0.05f, 0.05f, 0.06f, 1.0f})
#define UPGRADER_COL  ((Vec4){0.55f, 0.36f, 0.78f, 1.0f})
#define COLLECTOR_COL ((Vec4){0.85f, 0.68f, 0.25f, 1.0f})

// Belt surface: dark-gray chevrons that scroll toward the flow direction so the
// black belt reads as running even when it's empty.
#define BELT_CHEVRON_COL  ((Vec4){0.30f, 0.30f, 0.33f, 1.0f})
#define BELT_CHEVRONS     3
#define BELT_SCROLL_SPEED 1.5f   // chevron cycles/sec; kept below belt speed so 3 packed
                                 // chevrons read as a calm crawl, not a fast flicker

// Multiply a color's RGB by f (keeps alpha) for cheap directional shading.
static Vec4 shade(Vec4 c, f32 f) { return (Vec4){c.x * f, c.y * f, c.z * f, c.w}; }

// Lighten a color toward white by t in [0,1].
static Vec4 lighten(Vec4 c, f32 t) {
    return (Vec4){c.x + (1.0f - c.x) * t, c.y + (1.0f - c.y) * t, c.z + (1.0f - c.z) * t, c.w};
}

static f32 minf4(f32 a, f32 b, f32 c, f32 d) {
    f32 m = a; if (b < m) m = b; if (c < m) m = c; if (d < m) m = d; return m;
}
static f32 maxf4(f32 a, f32 b, f32 c, f32 d) {
    f32 m = a; if (b > m) m = b; if (c > m) m = c; if (d > m) m = d; return m;
}

// The four screen-space corners of grid cell (gx,gy) at the given elevation.
static void tile_corners(Renderer *r, const Camera2D *cam, f32 gx, f32 gy, f32 elev,
                         Vec2 *n, Vec2 *e, Vec2 *s, Vec2 *w) {
    f32 sw = (f32)r->width, sh = (f32)r->height;
    *n = iso_to_screen(cam, sw, sh, gx - 0.5f, gy - 0.5f, elev);
    *e = iso_to_screen(cam, sw, sh, gx + 0.5f, gy - 0.5f, elev);
    *s = iso_to_screen(cam, sw, sh, gx + 0.5f, gy + 0.5f, elev);
    *w = iso_to_screen(cam, sw, sh, gx - 0.5f, gy + 0.5f, elev);
}

// Flat diamond on the ground (elev 0).
static void draw_tile(Renderer *r, const Camera2D *cam, f32 gx, f32 gy, Vec4 color) {
    Vec2 n, e, s, w;
    tile_corners(r, cam, gx, gy, 0.0f, &n, &e, &s, &w);
    Vec2 pts[4] = {n, e, s, w};
    r2d_fill_poly(r, pts, 4, color);
}

// Stroke the outline of a ground-level diamond (grid line, hover highlight).
static void draw_tile_edges(Renderer *r, const Camera2D *cam, f32 gx, f32 gy, Vec4 color) {
    Vec2 n, e, s, w;
    tile_corners(r, cam, gx, gy, 0.0f, &n, &e, &s, &w);
    Vec2 pts[4] = {n, e, s, w};
    r2d_poly_outline(r, pts, 4, color);
}

// A shaded box: two visible front faces plus a top diamond, with outlined seams.
// Top is brightest, sides progressively darker, faking directional 3D.
static void draw_block(Renderer *r, const Camera2D *cam, f32 gx, f32 gy, f32 h, Vec4 base) {
    Vec2 tn, te, ts, tw;   // top corners (elevated)
    Vec2 bn, be, bs, bw;   // base corners (ground)
    tile_corners(r, cam, gx, gy, h, &tn, &te, &ts, &tw);
    tile_corners(r, cam, gx, gy, 0.0f, &bn, &be, &bs, &bw);
    (void)bn; (void)be;

    Vec2 left[4]  = {tw, ts, bs, bw};   // S-W face
    Vec2 right[4] = {ts, te, be, bs};   // S-E face
    Vec2 top[4]   = {tn, te, ts, tw};
    r2d_fill_poly(r, left,  4, shade(base, 0.62f));
    r2d_fill_poly(r, right, 4, shade(base, 0.46f));
    r2d_fill_poly(r, top,   4, base);

    Vec4 line = shade(base, 0.30f);
    r2d_poly_outline(r, top,   4, line);
    r2d_poly_outline(r, left,  4, line);
    r2d_poly_outline(r, right, 4, line);
}

// A flat triangle on a piece's top face, pointing the way it moves items. Built
// entirely in grid space (perpendicular in grid, not screen) so it lies flat on the
// tile and reads the same in every facing under the iso projection.
static void draw_arrow(Renderer *r, const Camera2D *cam, f32 gx, f32 gy, f32 elev,
                       Direction dir, Vec4 color) {
    f32 sw = (f32)r->width, sh = (f32)r->height;
    f32 dx = (f32)DIR_DX[dir], dy = (f32)DIR_DY[dir];
    f32 px = -dy, py = dx;                  // perpendicular in grid space
    f32 fwd = 0.34f, back = 0.18f, halfw = 0.22f;

    Vec2 tip = iso_to_screen(cam, sw, sh, gx + dx * fwd, gy + dy * fwd, elev);
    Vec2 b1  = iso_to_screen(cam, sw, sh, gx - dx * back + px * halfw,
                                          gy - dy * back + py * halfw, elev);
    Vec2 b2  = iso_to_screen(cam, sw, sh, gx - dx * back - px * halfw,
                                          gy - dy * back - py * halfw, elev);
    Vec2 pts[3] = {tip, b1, b2};
    r2d_fill_poly(r, pts, 3, color);
    r2d_poly_outline(r, pts, 3, shade(color, 0.45f));
}

// A thick line between two grid-space points, drawn as a filled quad on the belt's
// top face so its width scales with zoom instead of being a hairline.
static void draw_belt_tread(Renderer *r, const Camera2D *cam, f32 ax, f32 ay,
                            f32 bx, f32 by, f32 half_thick, Vec4 color) {
    f32 sw = (f32)r->width, sh = (f32)r->height;
    f32 vx = bx - ax, vy = by - ay;
    f32 len = sqrtf(vx * vx + vy * vy);
    if (len < 1e-5f) return;
    f32 nx = -vy / len * half_thick;   // perpendicular offset in grid units
    f32 ny =  vx / len * half_thick;
    Vec2 p0 = iso_to_screen(cam, sw, sh, ax + nx, ay + ny, CONVEYOR_H);
    Vec2 p1 = iso_to_screen(cam, sw, sh, bx + nx, by + ny, CONVEYOR_H);
    Vec2 p2 = iso_to_screen(cam, sw, sh, bx - nx, by - ny, CONVEYOR_H);
    Vec2 p3 = iso_to_screen(cam, sw, sh, ax - nx, ay - ny, CONVEYOR_H);
    Vec2 pts[4] = {p0, p1, p2, p3};
    r2d_fill_poly(r, pts, 4, color);
}

// Scrolling chevrons on a conveyor's top face. `phase` in [0,1) advances over real
// time, sliding each chevron toward the flow direction, so the belt looks like it's
// running. Also doubles as the conveyor's direction cue.
static void draw_belt_surface(Renderer *r, const Camera2D *cam, f32 gx, f32 gy,
                              Direction dir, f32 phase, Vec4 color) {
    f32 dx = (f32)DIR_DX[dir], dy = (f32)DIR_DY[dir];
    f32 px = -dy, py = dx;         // perpendicular in grid space
    f32 halfw = 0.26f;             // chevron arm spread across the belt
    f32 depth = 0.14f;             // apex-to-tail offset along the flow
    f32 thick = 0.05f;             // half the tread width, in grid units
    // (npt): Cap travel so the tail never crosses the cell's back edge onto the
    // tile behind. The chevron spans [center - depth, center + depth] along flow,
    // so keeping the center within +-(0.5 - depth) keeps the whole thing inside.
    f32 travel = 0.5f - depth;

    for (i32 k = 0; k < BELT_CHEVRONS; k++) {
        f32 u = (f32)k / (f32)BELT_CHEVRONS + phase;
        u -= floorf(u);
        f32 s = (u - 0.5f) * 2.0f * travel;   // chevron center along the flow axis
        f32 cx = gx + dx * s, cy = gy + dy * s;

        // (npt): Fade each chevron in as it enters the back of the cell and out as
        // it leaves the front, so it rises out of the belt instead of popping on top.
        Vec4 c = color;
        c.w *= sinf(3.14159265f * u);

        f32 apx = cx + dx * depth,             apy = cy + dy * depth;
        f32 tlx = cx - dx * depth + px * halfw, tly = cy - dy * depth + py * halfw;
        f32 trx = cx - dx * depth - px * halfw, trgy = cy - dy * depth - py * halfw;
        draw_belt_tread(r, cam, apx, apy, tlx, tly, thick, c);
        draw_belt_tread(r, cam, apx, apy, trx, trgy, thick, c);
    }
}

static i32 popcount_u64(u64 x) {
    i32 c = 0;
    while (x) { c += (i32)(x & 1u); x >>= 1; }
    return c;
}

// Item color reads its upgrade count: pale yellow when fresh, hot gold once it's
// been through several upgraders.
static Vec4 item_color(const Item *it) {
    f32 t = (f32)popcount_u64(it->upgraded_mask) / 6.0f;
    if (t > 1.0f) t = 1.0f;
    Vec4 fresh = {0.92f, 0.88f, 0.55f, 1.0f};
    Vec4 hot   = {1.00f, 0.78f, 0.20f, 1.0f};
    return (Vec4){fresh.x + (hot.x - fresh.x) * t,
                  fresh.y + (hot.y - fresh.y) * t,
                  fresh.z + (hot.z - fresh.z) * t, 1.0f};
}

// Compact money-style value: "25", "1.5K", "3.2M", up to quintillions (an i64 tops
// out at ~9.2 Qi). Keeps the label short as values run away.
static void format_value(i64 v, char *buf, size_t n) {
    static const char *suffix[] = {"", "K", "M", "B", "T", "Qa", "Qi"};
    if (v < 1000) { snprintf(buf, n, "%lld", (long long)v); return; }
    f64 d = (f64)v;
    i32 t = 0;
    while (d >= 1000.0 && t < 6) { d /= 1000.0; t++; }
    snprintf(buf, n, "%.1f%s", d, suffix[t]);
}

// A small diamond floating just above the belt at the item's cell, with the ore's
// current value labeled above it. `alpha` is the fraction into the current sim tick,
// so the item slides from its previous cell to its current one instead of snapping.
static void draw_item(Renderer *r, const Camera2D *cam, const Item *it, f32 alpha) {
    f32 sw = (f32)r->width, sh = (f32)r->height;
    f32 gx = (f32)it->prev_x + ((f32)it->grid_x - (f32)it->prev_x) * alpha;
    f32 gy = (f32)it->prev_y + ((f32)it->grid_y - (f32)it->prev_y) * alpha;
    f32 e = 0.34f, s = 0.18f;
    Vec4 col = item_color(it);

    // Ore that tipped off a dead end: sink it below the belt, shrink and fade it as it
    // drops out. `fall` counts FALL_TICKS -> 0; alpha smooths the drop between ticks.
    if (it->fall > 0) {
        f32 p = ((f32)(FALL_TICKS - it->fall) + alpha) / (f32)FALL_TICKS;
        if (p < 0.0f) p = 0.0f;
        if (p > 1.0f) p = 1.0f;
        e -= p * 0.9f;              // sink through the belt surface
        s *= 1.0f - 0.45f * p;     // shrink a touch
        col.w = 1.0f - p;          // fade to nothing
    }

    Vec2 n  = iso_to_screen(cam, sw, sh, gx,     gy - s, e);
    Vec2 ee = iso_to_screen(cam, sw, sh, gx + s, gy,     e);
    Vec2 ss = iso_to_screen(cam, sw, sh, gx,     gy + s, e);
    Vec2 ww = iso_to_screen(cam, sw, sh, gx - s, gy,     e);
    Vec2 pts[4] = {n, ee, ss, ww};
    r2d_fill_poly(r, pts, 4, col);
    r2d_poly_outline(r, pts, 4, shade(col, 0.45f));

    if (it->fall > 0) return;   // no value label on ore that's dropping out

    // Value label, centered over the diamond and sitting just above its top point.
    char buf[32];
    format_value(it->value, buf, sizeof buf);
    f32 tscale = 1.0f;
    f32 adv = (f32)r->font->advance * tscale;
    f32 gh  = (f32)r->font->glyph_h * tscale;
    Vec2 c  = iso_to_screen(cam, sw, sh, gx, gy, e);
    f32 tx = c.x - adv * (f32)strlen(buf) * 0.5f;
    f32 ty = n.y - gh - 4.0f;
    r2d_text(r, tx + 1.0f, ty + 1.0f, tscale, buf, (Vec4){0.0f, 0.0f, 0.0f, 0.7f});  // shadow
    r2d_text(r, tx, ty, tscale, buf, (Vec4){0.98f, 0.98f, 0.90f, 1.0f});
}

static void draw_entity(Renderer *r, const Camera2D *cam, const Entity *e, f32 belt_phase) {
    f32 gx = (f32)e->grid_x, gy = (f32)e->grid_y;
    switch (e->type) {
    case ENTITY_DROPPER:
        draw_block(r, cam, gx, gy, DROPPER_H, DROPPER_COL);
        draw_arrow(r, cam, gx, gy, DROPPER_H, e->dir, lighten(DROPPER_COL, 0.55f));
        break;
    case ENTITY_CONVEYOR:
        draw_block(r, cam, gx, gy, CONVEYOR_H, CONVEYOR_COL);
        draw_belt_surface(r, cam, gx, gy, e->dir, belt_phase, BELT_CHEVRON_COL);
        break;
    case ENTITY_UPGRADER:
        draw_block(r, cam, gx, gy, UPGRADER_H, UPGRADER_COL);
        draw_arrow(r, cam, gx, gy, UPGRADER_H, e->dir, lighten(UPGRADER_COL, 0.55f));
        break;
    case ENTITY_COLLECTOR:
        draw_block(r, cam, gx, gy, COLLECTOR_H, COLLECTOR_COL);
        break;
    default:
        break;
    }
}

// Depth-sort key for painter's order: far cells (smaller gx+gy) first.
typedef struct { f32 depth; const void *ptr; } DrawItem;
static DrawItem g_entities[MAX_ENTITIES];
static DrawItem g_items[MAX_ITEMS];

static int cmp_draw(const void *a, const void *b) {
    f32 da = ((const DrawItem *)a)->depth, db = ((const DrawItem *)b)->depth;
    return (da < db) ? -1 : (da > db) ? 1 : 0;
}

void game_render_draw(Renderer *r, const Game_State *game) {
    const Camera2D *cam = &game->camera;
    World *w = game->world;
    f32 sw = (f32)r->width, sh = (f32)r->height;

    // Visible ground range: unproject the screen corners, take the grid bbox.
    Vec2 c0 = screen_to_iso(cam, sw, sh, 0.0f, 0.0f);
    Vec2 c1 = screen_to_iso(cam, sw, sh, sw, 0.0f);
    Vec2 c2 = screen_to_iso(cam, sw, sh, 0.0f, sh);
    Vec2 c3 = screen_to_iso(cam, sw, sh, sw, sh);
    i32 gx0 = (i32)minf4(c0.x, c1.x, c2.x, c3.x) - 1;
    i32 gx1 = (i32)maxf4(c0.x, c1.x, c2.x, c3.x) + 1;
    i32 gy0 = (i32)minf4(c0.y, c1.y, c2.y, c3.y) - 1;
    i32 gy1 = (i32)maxf4(c0.y, c1.y, c2.y, c3.y) + 1;
    if (gx0 < 0) gx0 = 0;
    if (gy0 < 0) gy0 = 0;
    if (gx1 >= WORLD_GRID_SIZE) gx1 = WORLD_GRID_SIZE - 1;
    if (gy1 >= WORLD_GRID_SIZE) gy1 = WORLD_GRID_SIZE - 1;

    for (i32 gy = gy0; gy <= gy1; gy++) {
        for (i32 gx = gx0; gx <= gx1; gx++) {
            draw_tile(r, cam, (f32)gx, (f32)gy, ((gx + gy) & 1) ? GROUND_A : GROUND_B);
            draw_tile_edges(r, cam, (f32)gx, (f32)gy, GROUND_LINE);
        }
    }

    // Hover preview: a highlighted tile over the ground, under everything else.
    if (game->hover_valid) {
        Vec4 color;
        if (!game->hover_can_place)                     color = (Vec4){0.9f, 0.3f, 0.3f, 1.0f};
        else if (game->selected_tool == TOOL_COLLECTOR) color = (Vec4){0.85f, 0.7f, 0.3f, 1.0f};
        else                                            color = (Vec4){0.4f, 0.85f, 0.4f, 1.0f};
        draw_tile(r, cam, (f32)game->hover_grid_x, (f32)game->hover_grid_y, color);
        draw_tile_edges(r, cam, (f32)game->hover_grid_x, (f32)game->hover_grid_y,
                        lighten(color, 0.5f));
    }

    if (!w) return;

    // Machines, back-to-front by (gx+gy) so nearer blocks overdraw farther ones.
    i32 ne = 0;
    for (i32 i = 0; i < w->entity_count; i++) {
        const Entity *e = &w->entities[i];
        g_entities[ne].depth = (f32)(e->grid_x + e->grid_y);
        g_entities[ne].ptr = e;
        ne++;
    }
    f32 belt_phase = game->anim_time * BELT_SCROLL_SPEED;
    belt_phase -= floorf(belt_phase);
    qsort(g_entities, (size_t)ne, sizeof(DrawItem), cmp_draw);
    for (i32 i = 0; i < ne; i++) draw_entity(r, cam, (const Entity *)g_entities[i].ptr, belt_phase);

    // Items ride on top of the belts, also depth-sorted among themselves. The
    // leftover sim accumulator gives the fraction into the current tick, which
    // slides each item smoothly from its previous cell to its current one.
    f32 alpha = game->sim_accumulator / SIM_TICK_DT;
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    i32 ni = 0;
    for (i32 i = 0; i < MAX_ITEMS; i++) {
        const Item *it = &w->items[i];
        if (!it->alive) continue;
        g_items[ni].depth = (f32)(it->grid_x + it->grid_y);
        g_items[ni].ptr = it;
        ni++;
    }
    qsort(g_items, (size_t)ni, sizeof(DrawItem), cmp_draw);
    for (i32 i = 0; i < ni; i++) draw_item(r, cam, (const Item *)g_items[i].ptr, alpha);
}
