// Game scene rendering implementation (included into mach.c).

#include "render_game.h"
#include "world/world.h"
#include <stdlib.h>

// Block height in elevation units.
#define BLOCK_H 0.8f

// Ground tile colors (subtle checker so the grid reads).
#define GROUND_A ((Vec4){0.16f, 0.20f, 0.24f, 1.0f})
#define GROUND_B ((Vec4){0.20f, 0.25f, 0.30f, 1.0f})

// Multiply a color's RGB by f (keeps alpha) — cheap directional shading.
static Vec4 shade(Vec4 c, f32 f) { return (Vec4){c.x * f, c.y * f, c.z * f, c.w}; }

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

// A shaded box: two visible front faces (the S-W and S-E sides) plus a top
// diamond. Top is brightest, sides progressively darker — fakes directional 3D.
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
}

// Depth-sorted draw list for machines (painter's algorithm: far cells first).
typedef struct { f32 depth; const Entity *e; } DrawItem;
static DrawItem g_items[MAX_ENTITIES];

static int cmp_item(const void *a, const void *b) {
    f32 da = ((const DrawItem *)a)->depth, db = ((const DrawItem *)b)->depth;
    return (da < db) ? -1 : (da > db) ? 1 : 0;
}

static void draw_entity(Renderer *r, const Camera2D *cam, const Entity *e) {
    if (e->type == ENTITY_MINER) {
        const Entity_Miner *m = &e->data.miner;
        draw_block(r, cam, (f32)m->grid_x, (f32)m->grid_y, BLOCK_H,
                   (Vec4){0.55f, 0.35f, 0.17f, 1.0f});
    } else if (e->type == ENTITY_STORAGE) {
        const Entity_Storage *s = &e->data.storage;
        draw_block(r, cam, (f32)s->grid_x, (f32)s->grid_y, BLOCK_H,
                   (Vec4){0.6f, 0.6f, 0.62f, 1.0f});
        // Ore fill: an orange block stacked taller by the stored fraction.
        if (s->ore_capacity > 0 && s->ore_stored > 0) {
            f32 frac = (f32)s->ore_stored / (f32)s->ore_capacity;
            if (frac > 1.0f) frac = 1.0f;
            draw_block(r, cam, (f32)s->grid_x, (f32)s->grid_y, BLOCK_H + 0.4f * frac,
                       (Vec4){0.85f, 0.45f, 0.2f, 1.0f});
        }
    }
}

void game_render_draw(Renderer *r, const Game_State *game) {
    const Camera2D *cam = &game->camera;
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
        }
    }

    // Hover preview: a highlighted tile, drawn over the ground, under machines.
    if (game->hover_valid) {
        Vec4 color;
        if (!game->hover_can_place)                   color = (Vec4){0.9f, 0.3f, 0.3f, 1.0f};
        else if (game->selected_tool == TOOL_STORAGE) color = (Vec4){0.5f, 0.6f, 0.9f, 1.0f};
        else                                          color = (Vec4){0.4f, 0.85f, 0.4f, 1.0f};
        draw_tile(r, cam, (f32)game->hover_grid_x, (f32)game->hover_grid_y, color);
    }

    // Machines, sorted back-to-front by (gx+gy) so nearer blocks overdraw.
    World *w = game->world;
    if (w) {
        i32 count = 0;
        for (i32 i = 0; i < w->entity_count; i++) {
            const Entity *e = &w->entities[i];
            i32 ex, ey;
            entity_grid_pos(e, &ex, &ey);
            g_items[count].depth = (f32)(ex + ey);
            g_items[count].e = e;
            count++;
        }
        qsort(g_items, (size_t)count, sizeof(DrawItem), cmp_item);
        for (i32 i = 0; i < count; i++) draw_entity(r, cam, g_items[i].e);
    }
}
