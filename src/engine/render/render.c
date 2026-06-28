// Render implementation (included into mach.c).

#include "render.h"
#include "../ui.h"
#include <SDL3/SDL.h>

// Convert grid coordinates to screen space using standard isometric projection.
Vec2 grid_to_isometric(i32 grid_x, i32 grid_y, i32 tile_size) {
    Vec2 result;
    result.x = (f32)((grid_x - grid_y) * (tile_size / 2));
    result.y = (f32)((grid_x + grid_y) * (tile_size / 4));
    return result;
}

// Drawing primitives

void render_rect_filled(SDL_Renderer *rend, i32 x, i32 y, i32 w, i32 h, u8 r, u8 g, u8 b, u8 a) {
    if (!rend) return;
    SDL_SetRenderDrawColor(rend, r, g, b, a);
    SDL_FRect rect = {(f32)x, (f32)y, (f32)w, (f32)h};
    SDL_RenderFillRect(rend, &rect);
}

void render_rect_outline(SDL_Renderer *rend, i32 x, i32 y, i32 w, i32 h, u8 r, u8 g, u8 b) {
    if (!rend) return;
    SDL_SetRenderDrawColor(rend, r, g, b, 255);
    SDL_FRect rect = {(f32)x, (f32)y, (f32)w, (f32)h};
    SDL_RenderRect(rend, &rect);
}

void render_circle_filled(SDL_Renderer *rend, i32 cx, i32 cy, i32 radius, u8 r, u8 g, u8 b, u8 a) {
    if (!rend || radius <= 0) return;
    SDL_SetRenderDrawColor(rend, r, g, b, a);

    for (i32 y = -radius; y <= radius; y++) {
        for (i32 x = -radius; x <= radius; x++) {
            if (x * x + y * y <= radius * radius) {
                SDL_RenderPoint(rend, (f32)(cx + x), (f32)(cy + y));
            }
        }
    }
}

void render_line(SDL_Renderer *rend, i32 x1, i32 y1, i32 x2, i32 y2, u8 r, u8 g, u8 b) {
    if (!rend) return;
    SDL_SetRenderDrawColor(rend, r, g, b, 255);
    SDL_RenderLine(rend, (f32)x1, (f32)y1, (f32)x2, (f32)y2);
}

// --- Isometric projection helpers -------------------------------------------
//
// World-space to screen-space: (world - camera) * zoom + screen_center.
// The camera represents the world-space point anchored at the screen center.

// Center pixel of a grid cell's diamond.
static SDL_FPoint iso_center(i32 grid_x, i32 grid_y, i32 tile_size,
                             i32 camera_x, i32 camera_y, f32 zoom) {
    Vec2 pos = grid_to_isometric(grid_x, grid_y, tile_size);
    i32 hw = tile_size / 2;
    i32 qh = tile_size / 4;
    SDL_FPoint c;
    c.x = ((pos.x + (f32)hw - (f32)camera_x) * zoom) + (f32)SCREEN_CENTER_X;
    c.y = ((pos.y + (f32)qh - (f32)camera_y) * zoom) + (f32)SCREEN_CENTER_Y;
    return c;
}

// The four corners of a grid cell's isometric diamond, in screen space.
static void iso_diamond(i32 grid_x, i32 grid_y, i32 tile_size,
                        i32 camera_x, i32 camera_y, f32 zoom, SDL_FPoint out[4]) {
    SDL_FPoint c = iso_center(grid_x, grid_y, tile_size, camera_x, camera_y, zoom);
    f32 hw_z = (f32)(tile_size / 2) * zoom;
    f32 qh_z = (f32)(tile_size / 4) * zoom;

    out[0] = (SDL_FPoint){c.x,        c.y - qh_z};  // top
    out[1] = (SDL_FPoint){c.x + hw_z, c.y};         // right
    out[2] = (SDL_FPoint){c.x,        c.y + qh_z};  // bottom
    out[3] = (SDL_FPoint){c.x - hw_z, c.y};         // left
}

// Draw a diamond outline from its four corners in the current draw color.
static void draw_diamond(SDL_Renderer *rend, const SDL_FPoint pts[4]) {
    for (i32 i = 0; i < 4; i++) {
        SDL_RenderLine(rend, pts[i].x, pts[i].y, pts[(i + 1) % 4].x, pts[(i + 1) % 4].y);
    }
}

// --- Entity rendering -------------------------------------------------------

static void render_miner(SDL_Renderer *rend, Entity_Miner *miner, i32 tile_size,
                         i32 camera_x, i32 camera_y, f32 zoom) {
    SDL_FPoint pts[4];
    iso_diamond(miner->grid_x, miner->grid_y, tile_size, camera_x, camera_y, zoom, pts);
    SDL_SetRenderDrawColor(rend, 139, 90, 43, 255);
    draw_diamond(rend, pts);

    SDL_FPoint c = iso_center(miner->grid_x, miner->grid_y, tile_size, camera_x, camera_y, zoom);
    SDL_SetRenderDrawColor(rend, 200, 200, 200, 255);
    SDL_RenderLine(rend, c.x - 3, c.y - 2, c.x + 3, c.y + 2);
    SDL_RenderLine(rend, c.x - 3, c.y + 2, c.x + 3, c.y - 2);
}

static void render_storage(SDL_Renderer *rend, Entity_Storage *storage, i32 tile_size,
                           i32 camera_x, i32 camera_y, f32 zoom) {
    SDL_FPoint pts[4];
    iso_diamond(storage->grid_x, storage->grid_y, tile_size, camera_x, camera_y, zoom, pts);
    SDL_SetRenderDrawColor(rend, 150, 150, 150, 255);
    draw_diamond(rend, pts);

    SDL_FPoint c = iso_center(storage->grid_x, storage->grid_y, tile_size, camera_x, camera_y, zoom);
    SDL_SetRenderDrawColor(rend, 200, 100, 50, 255);
    SDL_RenderLine(rend, c.x - 2, c.y, c.x + 2, c.y);

    if (storage->ore_capacity > 0) {
        i32 fill_height = (storage->ore_stored * (tile_size / 4)) / storage->ore_capacity;
        if (fill_height > 0) {
            SDL_RenderLine(rend, c.x - 2, c.y - (f32)fill_height, c.x + 2, c.y - (f32)fill_height);
        }
    }
}

// Render all entities in the world.
void render_world(SDL_Renderer *rend, World *w, i32 tile_size, i32 camera_x, i32 camera_y, f32 zoom) {
    if (!rend || !w) return;
    for (i32 i = 0; i < w->entity_count; i++) {
        Entity *e = &w->entities[i];
        switch (e->type) {
        case ENTITY_MINER:
            render_miner(rend, &e->data.miner, tile_size, camera_x, camera_y, zoom);
            break;
        case ENTITY_STORAGE:
            render_storage(rend, &e->data.storage, tile_size, camera_x, camera_y, zoom);
            break;
        default:
            break;
        }
    }
}

// Render the placeable grid, culling cells outside the viewport.
void render_grid(SDL_Renderer *rend, i32 tile_size, i32 camera_x, i32 camera_y, f32 zoom) {
    if (!rend) return;

    SDL_SetRenderDrawColor(rend, 60, 80, 100, 80);

    f32 hw_z = (f32)(tile_size / 2) * zoom;
    f32 qh_z = (f32)(tile_size / 4) * zoom;

    for (i32 grid_y = 0; grid_y < WORLD_GRID_SIZE; grid_y++) {
        for (i32 grid_x = 0; grid_x < WORLD_GRID_SIZE; grid_x++) {
            SDL_FPoint c = iso_center(grid_x, grid_y, tile_size, camera_x, camera_y, zoom);

            // Skip cells comfortably off-screen to avoid wasted draw calls.
            if (c.x < -hw_z * 2 || c.x > (f32)SCREEN_WIDTH + hw_z * 2 ||
                c.y < -qh_z * 2 || c.y > (f32)SCREEN_HEIGHT + qh_z * 2) {
                continue;
            }

            SDL_FPoint pts[4];
            iso_diamond(grid_x, grid_y, tile_size, camera_x, camera_y, zoom, pts);
            draw_diamond(rend, pts);
        }
    }
}

// Render a semi-transparent placement preview at the snapped grid position.
void render_hover_preview(SDL_Renderer *rend, i32 grid_x, i32 grid_y, i32 tile_size,
                          i32 camera_x, i32 camera_y, f32 zoom, i32 tool, b32 can_place) {
    if (!rend) return;

    SDL_FPoint pts[4];
    iso_diamond(grid_x, grid_y, tile_size, camera_x, camera_y, zoom, pts);

    // Tool id 2 == storage (see game.h Tool enum). The engine layer stays
    // game-agnostic, so the id is passed in rather than the enum imported.
    u8 r, g, b;
    if (!can_place) {
        r = 255; g = 100; b = 100;  // invalid: red
    } else if (tool == 2) {
        r = 150; g = 150; b = 200;  // storage: blue
    } else {
        r = 100; g = 200; b = 100;  // valid: green
    }

    SDL_SetRenderDrawColor(rend, r, g, b, 100);
    draw_diamond(rend, pts);
}
