// Game scene rendering implementation (included into mach.c).

#include "render_game.h"
#include "world/world.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

// The look is the engine's stock modus-vivendi palette (engine/render/color.h):
// a near-black ground, machines in the theme's accent colors.

// Ground tile colors (subtle checker so the grid reads).
#define GROUND_A COLOR_BG_DIM
#define GROUND_B COLOR_HEX(0x252525)   // between bg-dim and bg-inactive: a quiet checker

// Grid line: darker than the tiles so the diamonds separate cleanly.
#define GROUND_LINE COLOR_HEX(0x0f0f0f)

// Per-piece block heights (elevation units) and base colors.
#define DROPPER_H    0.80f
#define CONVEYOR_H   0.12f
#define UPGRADER_H   0.50f
#define COLLECTOR_H  0.85f

#define DROPPER_COL   COLOR_GREEN
#define CONVEYOR_COL  COLOR_BG_POPUP
#define UPGRADER_COL  COLOR_MAGENTA_COOLER
#define COLLECTOR_COL COLOR_YELLOW_WARMER

// Belt surface: gray chevrons that scroll toward the flow direction so the
// black belt reads as running even when it's empty.
#define BELT_CHEVRON_COL  COLOR_BG_ACTIVE
#define BELT_CHEVRONS     3
#define BELT_SCROLL_SPEED 1.5f   // chevron cycles/sec; kept below belt speed so 3 packed
                                 // chevrons read as a calm crawl, not a fast flicker

static f32 minf4(f32 a, f32 b, f32 c, f32 d) {
    f32 m = a; if (b < m) m = b; if (c < m) m = c; if (d < m) m = d; return m;
}
static f32 maxf4(f32 a, f32 b, f32 c, f32 d) {
    f32 m = a; if (b > m) m = b; if (c > m) m = c; if (d > m) m = d; return m;
}

// The four screen-space corners of grid cell (gx,gy) at the given elevation.
static void tile_corners(Mach_Renderer *r, const Mach_Camera2D *cam, f32 gx, f32 gy, f32 elev,
                         Mach_Vec2 *n, Mach_Vec2 *e, Mach_Vec2 *s, Mach_Vec2 *w) {
    f32 sw = (f32)r->width, sh = (f32)r->height;
    *n = iso_to_screen(cam, sw, sh, gx - 0.5f, gy - 0.5f, elev);
    *e = iso_to_screen(cam, sw, sh, gx + 0.5f, gy - 0.5f, elev);
    *s = iso_to_screen(cam, sw, sh, gx + 0.5f, gy + 0.5f, elev);
    *w = iso_to_screen(cam, sw, sh, gx - 0.5f, gy + 0.5f, elev);
}

// Flat diamond on the ground (elev 0).
static void draw_tile(Mach_Renderer *r, const Mach_Camera2D *cam, f32 gx, f32 gy, Mach_Color color) {
    Mach_Vec2 n, e, s, w;
    tile_corners(r, cam, gx, gy, 0.0f, &n, &e, &s, &w);
    Mach_Vec2 pts[4] = {n, e, s, w};
    r2d_fill_poly(r, pts, 4, color);
}

// Stroke the outline of a ground-level diamond (grid line, hover highlight).
static void draw_tile_edges(Mach_Renderer *r, const Mach_Camera2D *cam, f32 gx, f32 gy, Mach_Color color) {
    Mach_Vec2 n, e, s, w;
    tile_corners(r, cam, gx, gy, 0.0f, &n, &e, &s, &w);
    Mach_Vec2 pts[4] = {n, e, s, w};
    r2d_poly_outline(r, pts, 4, color);
}

// A shaded box: two visible front faces plus a top diamond, with outlined seams.
// Top is brightest, sides progressively darker, faking directional 3D.
static void draw_block(Mach_Renderer *r, const Mach_Camera2D *cam, f32 gx, f32 gy, f32 h, Mach_Color base) {
    Mach_Vec2 tn, te, ts, tw;   // top corners (elevated)
    Mach_Vec2 bn, be, bs, bw;   // base corners (ground)
    tile_corners(r, cam, gx, gy, h, &tn, &te, &ts, &tw);
    tile_corners(r, cam, gx, gy, 0.0f, &bn, &be, &bs, &bw);
    (void)bn; (void)be;

    Mach_Vec2 left[4]  = {tw, ts, bs, bw};   // S-W face
    Mach_Vec2 right[4] = {ts, te, be, bs};   // S-E face
    Mach_Vec2 top[4]   = {tn, te, ts, tw};
    r2d_fill_poly(r, left,  4, color_shade(base, 0.62f));
    r2d_fill_poly(r, right, 4, color_shade(base, 0.46f));
    r2d_fill_poly(r, top,   4, base);

    Mach_Color line = color_shade(base, 0.30f);
    r2d_poly_outline(r, top,   4, line);
    r2d_poly_outline(r, left,  4, line);
    r2d_poly_outline(r, right, 4, line);
}

// A flat triangle on a piece's top face, pointing the way it moves items. Built
// entirely in grid space (perpendicular in grid, not screen) so it lies flat on the
// tile and reads the same in every facing under the iso projection.
static void draw_arrow(Mach_Renderer *r, const Mach_Camera2D *cam, f32 gx, f32 gy, f32 elev,
                       Direction dir, Mach_Color color) {
    f32 sw = (f32)r->width, sh = (f32)r->height;
    f32 dx = (f32)DIR_DX[dir], dy = (f32)DIR_DY[dir];
    f32 px = -dy, py = dx;                  // perpendicular in grid space
    f32 fwd = 0.34f, back = 0.18f, halfw = 0.22f;

    Mach_Vec2 tip = iso_to_screen(cam, sw, sh, gx + dx * fwd, gy + dy * fwd, elev);
    Mach_Vec2 b1  = iso_to_screen(cam, sw, sh, gx - dx * back + px * halfw,
                                          gy - dy * back + py * halfw, elev);
    Mach_Vec2 b2  = iso_to_screen(cam, sw, sh, gx - dx * back - px * halfw,
                                          gy - dy * back - py * halfw, elev);
    Mach_Vec2 pts[3] = {tip, b1, b2};
    r2d_fill_poly(r, pts, 3, color);
    r2d_poly_outline(r, pts, 3, color_shade(color, 0.45f));
}

// A thick line between two grid-space points, drawn as a filled quad on the belt's
// top face so its width scales with zoom instead of being a hairline.
static void draw_belt_tread(Mach_Renderer *r, const Mach_Camera2D *cam, f32 ax, f32 ay,
                            f32 bx, f32 by, f32 half_thick, Mach_Color color) {
    f32 sw = (f32)r->width, sh = (f32)r->height;
    f32 vx = bx - ax, vy = by - ay;
    f32 len = sqrtf(vx * vx + vy * vy);
    if (len < 1e-5f) return;
    f32 nx = -vy / len * half_thick;   // perpendicular offset in grid units
    f32 ny =  vx / len * half_thick;
    Mach_Vec2 p0 = iso_to_screen(cam, sw, sh, ax + nx, ay + ny, CONVEYOR_H);
    Mach_Vec2 p1 = iso_to_screen(cam, sw, sh, bx + nx, by + ny, CONVEYOR_H);
    Mach_Vec2 p2 = iso_to_screen(cam, sw, sh, bx - nx, by - ny, CONVEYOR_H);
    Mach_Vec2 p3 = iso_to_screen(cam, sw, sh, ax - nx, ay - ny, CONVEYOR_H);
    Mach_Vec2 pts[4] = {p0, p1, p2, p3};
    r2d_fill_poly(r, pts, 4, color);
}

// Scrolling chevrons on a conveyor's top face. `phase` in [0,1) advances over real
// time, sliding each chevron toward the flow direction, so the belt looks like it's
// running. Also doubles as the conveyor's direction cue.
static void draw_belt_surface(Mach_Renderer *r, const Mach_Camera2D *cam, f32 gx, f32 gy,
                              Direction dir, f32 phase, Mach_Color color) {
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
        Mach_Color c = color;
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

// Item color reads its upgrade count: pale tan when fresh, hot gold once it's
// been through several upgraders.
static Mach_Color item_color(const Item *it) {
    f32 t = (f32)popcount_u64(it->upgraded_mask) / 6.0f;
    if (t > 1.0f) t = 1.0f;
    return color_lerp(COLOR_YELLOW_FAINT, COLOR_YELLOW_WARMER, t);
}

// A small diamond floating just above the belt at the item's cell, with the ore's
// current value labeled above it. `alpha` is the fraction into the current sim tick,
// so the item slides from its previous cell to its current one instead of snapping.
static void draw_item(Mach_Renderer *r, const Mach_Camera2D *cam, const Item *it, f32 alpha) {
    f32 sw = (f32)r->width, sh = (f32)r->height;
    f32 gx = (f32)it->prev_x + ((f32)it->grid_x - (f32)it->prev_x) * alpha;
    f32 gy = (f32)it->prev_y + ((f32)it->grid_y - (f32)it->prev_y) * alpha;
    f32 e = 0.34f, s = 0.18f;
    Mach_Color col = item_color(it);

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

    Mach_Vec2 n  = iso_to_screen(cam, sw, sh, gx,     gy - s, e);
    Mach_Vec2 ee = iso_to_screen(cam, sw, sh, gx + s, gy,     e);
    Mach_Vec2 ss = iso_to_screen(cam, sw, sh, gx,     gy + s, e);
    Mach_Vec2 ww = iso_to_screen(cam, sw, sh, gx - s, gy,     e);
    Mach_Vec2 pts[4] = {n, ee, ss, ww};
    r2d_fill_poly(r, pts, 4, col);
    r2d_poly_outline(r, pts, 4, color_shade(col, 0.45f));

    if (it->fall > 0) return;   // no value label on ore that's dropping out

    // Value label, centered over the diamond and sitting just above its top point.
    char buf[32];
    game_format_value(it->value, buf, sizeof buf);
    f32 tscale = 1.0f;
    f32 adv = (f32)r->font->advance * tscale;
    f32 gh  = (f32)r->font->glyph_h * tscale;
    Mach_Vec2 c  = iso_to_screen(cam, sw, sh, gx, gy, e);
    f32 tx = c.x - adv * (f32)strlen(buf) * 0.5f;
    f32 ty = n.y - gh - 4.0f;
    r2d_text(r, tx + 1.0f, ty + 1.0f, tscale, buf, color_alpha(COLOR_BG_MAIN, 0.7f));  // shadow
    r2d_text(r, tx, ty, tscale, buf, COLOR_FG_MAIN);
}

static void draw_entity(Mach_Renderer *r, const Mach_Camera2D *cam, const Entity *e, f32 belt_phase) {
    f32 gx = (f32)e->grid_x, gy = (f32)e->grid_y;
    switch (e->type) {
    case ENTITY_DROPPER:
        draw_block(r, cam, gx, gy, DROPPER_H, DROPPER_COL);
        draw_arrow(r, cam, gx, gy, DROPPER_H, e->dir, color_lighten(DROPPER_COL, 0.55f));
        break;
    case ENTITY_CONVEYOR:
        draw_block(r, cam, gx, gy, CONVEYOR_H, CONVEYOR_COL);
        draw_belt_surface(r, cam, gx, gy, e->dir, belt_phase, BELT_CHEVRON_COL);
        break;
    case ENTITY_UPGRADER:
        draw_block(r, cam, gx, gy, UPGRADER_H, UPGRADER_COL);
        draw_arrow(r, cam, gx, gy, UPGRADER_H, e->dir, color_lighten(UPGRADER_COL, 0.55f));
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

static int cmp_draw(const void *a, const void *b) {
    f32 da = ((const DrawItem *)a)->depth, db = ((const DrawItem *)b)->depth;
    return (da < db) ? -1 : (da > db) ? 1 : 0;
}

void game_render_draw(Mach_Renderer *r, const Game_State *game, Mach_Arena *scratch) {
    const Mach_Camera2D *cam = &game->camera;
    World *w = game->world;
    f32 sw = (f32)r->width, sh = (f32)r->height;

    // Visible ground range: unproject the screen corners, take the grid bbox.
    Mach_Vec2 c0 = screen_to_iso(cam, sw, sh, 0.0f, 0.0f);
    Mach_Vec2 c1 = screen_to_iso(cam, sw, sh, sw, 0.0f);
    Mach_Vec2 c2 = screen_to_iso(cam, sw, sh, 0.0f, sh);
    Mach_Vec2 c3 = screen_to_iso(cam, sw, sh, sw, sh);
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
        Mach_Color color;
        if (!game->hover_can_place)                     color = COLOR_RED;
        else if (game->selected_tool == TOOL_COLLECTOR) color = COLOR_YELLOW_WARMER;
        else                                            color = COLOR_GREEN;
        draw_tile(r, cam, (f32)game->hover_grid_x, (f32)game->hover_grid_y, color);
        draw_tile_edges(r, cam, (f32)game->hover_grid_x, (f32)game->hover_grid_y,
                        color_lighten(color, 0.5f));
    }

    if (!w) return;

    // Machines, back-to-front by (gx+gy) so nearer blocks overdraw farther ones.
    // Sort buffers come from the frame arena: sized to what's alive, gone next frame.
    DrawItem *ents = (DrawItem *)arena_alloc(scratch, (usize)w->entity_count * sizeof(DrawItem));
    if (w->entity_count > 0 && !ents) return;
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
    for (i32 i = 0; i < ne; i++) draw_entity(r, cam, (const Entity *)ents[i].ptr, belt_phase);

    // Items ride on top of the belts, also depth-sorted among themselves. The
    // leftover sim accumulator gives the fraction into the current tick, which
    // slides each item smoothly from its previous cell to its current one.
    f32 alpha = game->sim_accumulator / SIM_TICK_DT;
    if (alpha < 0.0f) alpha = 0.0f;
    if (alpha > 1.0f) alpha = 1.0f;

    DrawItem *items = (DrawItem *)arena_alloc(scratch, (usize)w->item_count * sizeof(DrawItem));
    if (w->item_count > 0 && !items) return;
    i32 ni = 0;
    for (i32 i = 0; i < MAX_ITEMS && ni < w->item_count; i++) {
        const Item *it = &w->items[i];
        if (!it->alive) continue;
        items[ni].depth = (f32)(it->grid_x + it->grid_y);
        items[ni].ptr = it;
        ni++;
    }
    qsort(items, (size_t)ni, sizeof(DrawItem), cmp_draw);
    for (i32 i = 0; i < ni; i++) draw_item(r, cam, (const Item *)items[i].ptr, alpha);
}

// ---------------------------------------------------------------------------
// HUD: Clay floating panels pinned to the screen edges.
// ---------------------------------------------------------------------------

// HUD colors, from the engine palette (clay_color_of converts to Clay's 0-255).
#define HUD_PANEL   clay_color_of(color_alpha(COLOR_BG_DIM, 0.86f))
#define HUD_GOLD    clay_color_of(COLOR_YELLOW_WARMER)
#define HUD_GREEN   clay_color_of(COLOR_GREEN)
#define HUD_AMBER   clay_color_of(COLOR_YELLOW)
#define HUD_GREY    clay_color_of(COLOR_FG_DIM)
#define HUD_WHITE   clay_color_of(COLOR_FG_MAIN)
#define HUD_PURPLE  clay_color_of(COLOR_MAGENTA_COOLER)

// A floating HUD panel pinned to a screen corner/edge: the same attach point on
// the element and the root, nudged inward by (ox, oy). Follow with a block of
// CLAY_TEXT children.
#define HUD_PANEL_AT(name, attach, ox, oy) \
    CLAY(CLAY_ID(name), { \
        .layout = { .layoutDirection = CLAY_TOP_TO_BOTTOM, \
                    .padding = CLAY_PADDING_ALL(10), .childGap = 4, \
                    .sizing = { .width = CLAY_SIZING_FIT(0), .height = CLAY_SIZING_FIT(0) } }, \
        .backgroundColor = HUD_PANEL, \
        .cornerRadius = CLAY_CORNER_RADIUS(6), \
        .floating = { .attachTo = CLAY_ATTACH_TO_ROOT, \
                      .attachPoints = { .element = attach, .parent = attach }, \
                      .offset = { ox, oy } } })

void game_render_hud(Game_State *g, Mach *m) {
    Mach_Renderer *r = &m->r2d;

    static const char *tool_names[] = {"None", "Dropper", "Conveyor", "Upgrader",
                                       "Collector", "Delete"};
    static const char *dir_names[]  = {"N", "E", "S", "W"};
    static const char *entity_names[] = {"", "Dropper", "Conveyor", "Upgrader", "Collector"};
    const char *tool = (g->selected_tool >= 0 && g->selected_tool < (i32)ARRAY_COUNT(tool_names))
                           ? tool_names[g->selected_tool] : "?";
    const char *facing = (g->place_dir >= 0 && g->place_dir < (i32)ARRAY_COUNT(dir_names))
                             ? dir_names[g->place_dir] : "?";

    // HUD strings. These buffers must outlive clay_ui_render (Clay keeps the char
    // pointers, not copies), so they stay in scope for the whole function.
    char money_s[32], tool_s[48], fps_s[24], counts_s[64], hover_s[48], cam_s[48];
    char val_a[24], val_b[24], banked_s[24];
    char insp_sub_s[32], insp_extra_s[48], insp_item_s[64];
    snprintf(money_s, sizeof(money_s), "$%lld", (long long)(g->world ? g->world->money : 0));
    snprintf(tool_s, sizeof(tool_s), "%s   facing %s", tool, facing);
    snprintf(fps_s, sizeof(fps_s), "fps %d", m->fps);
    snprintf(counts_s, sizeof(counts_s), "tick %d   entities %d   items %d",
             g->world ? g->world->tick : 0, g->world ? g->world->entity_count : 0,
             g->world ? g->world->item_count : 0);
    if (g->hover_valid)
        snprintf(hover_s, sizeof(hover_s), "hover %d,%d%s", g->hover_grid_x, g->hover_grid_y,
                 g->hover_can_place ? "" : " (blocked)");
    else
        snprintf(hover_s, sizeof(hover_s), "hover --");
    snprintf(cam_s, sizeof(cam_s), "cam %.0f,%.0f   zoom %.2f",
             g->camera.pan.x, g->camera.pan.y, g->camera.zoom);

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
        case ENTITY_COLLECTOR:
            insp_title_col = HUD_GOLD;
            game_format_value(ins->data.collector.banked, banked_s, sizeof banked_s);
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
    clay_ui_begin(&g->clay, r, (Clay_Vector2){in->mouse.x, in->mouse.y},
                  in->mouse_down[MOUSE_LEFT]);

    // Top-left: the always-on status.
    HUD_PANEL_AT("hud-status", CLAY_ATTACH_POINT_LEFT_TOP, 10, 10) {
        CLAY_TEXT(clay_string(money_s), CLAY_TEXT_CONFIG({ .fontSize = 16, .textColor = HUD_GOLD }));
        CLAY_TEXT(clay_string(tool_s),  CLAY_TEXT_CONFIG({ .fontSize = 8,  .textColor = HUD_GREEN }));
        if (g->paused)
            CLAY_TEXT(CLAY_STRING("PAUSED"), CLAY_TEXT_CONFIG({ .fontSize = 8, .textColor = HUD_AMBER }));
    }

    // Top-center: what the cursor is pointing at.
    if (ins || ore) {
        HUD_PANEL_AT("hud-inspect", CLAY_ATTACH_POINT_CENTER_TOP, 0, 10) {
            CLAY_TEXT(clay_string(insp_title), CLAY_TEXT_CONFIG({ .fontSize = 16, .textColor = insp_title_col }));
            if (insp_sub_s[0])
                CLAY_TEXT(clay_string(insp_sub_s), CLAY_TEXT_CONFIG({ .fontSize = 8, .textColor = HUD_GREY }));
            if (insp_extra_s[0])
                CLAY_TEXT(clay_string(insp_extra_s), CLAY_TEXT_CONFIG({ .fontSize = 8, .textColor = HUD_GOLD }));
            if (insp_item_s[0])
                CLAY_TEXT(clay_string(insp_item_s), CLAY_TEXT_CONFIG({ .fontSize = 8, .textColor = HUD_WHITE }));
        }
    }

    // Bottom-left: the F3 diagnostics.
    if (g->show_debug) {
        HUD_PANEL_AT("hud-debug", CLAY_ATTACH_POINT_LEFT_BOTTOM, 10, -10) {
            CLAY_TEXT(clay_string(fps_s),    CLAY_TEXT_CONFIG({ .fontSize = 8, .textColor = HUD_GREY }));
            CLAY_TEXT(clay_string(counts_s), CLAY_TEXT_CONFIG({ .fontSize = 8, .textColor = HUD_GREY }));
            CLAY_TEXT(clay_string(hover_s),  CLAY_TEXT_CONFIG({ .fontSize = 8, .textColor = HUD_GREY }));
            CLAY_TEXT(clay_string(cam_s),    CLAY_TEXT_CONFIG({ .fontSize = 8, .textColor = HUD_GREY }));
        }
    }

    // Bottom-center: the controls, on screen until a real menu exists to teach them.
    HUD_PANEL_AT("hud-controls", CLAY_ATTACH_POINT_CENTER_BOTTOM, 0, -10) {
        CLAY_TEXT(CLAY_STRING("(1)drop (2)belt (3)upgr (4)collect (5)del (R)rotate (Space)pause (F3)info"),
                  CLAY_TEXT_CONFIG({ .fontSize = 8, .textColor = HUD_GREY }));
    }

    clay_ui_render(&g->clay, r);
}
