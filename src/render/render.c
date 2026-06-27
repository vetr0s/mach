// Render implementation. Included into mach.c (not compiled separately).

#include "render.h"
#include <SDL3/SDL.h>

Vec2 grid_to_isometric(i32 grid_x, i32 grid_y, i32 tile_size) {
    // Standard isometric projection
    // Isometric x = (grid_x - grid_y) * (tile_size / 2)
    // Isometric y = (grid_x + grid_y) * (tile_size / 4)
    Vec2 result;
    result.x = (f32)((grid_x - grid_y) * (tile_size / 2));
    result.y = (f32)((grid_x + grid_y) * (tile_size / 4));
    return result;
}

static void render_rect_iso(SDL_Renderer *rend, i32 grid_x, i32 grid_y, i32 tile_size,
                            u8 r, u8 g, u8 b, i32 offset_x, i32 offset_y) {
    Vec2 pos = grid_to_isometric(grid_x, grid_y, tile_size);

    // Isometric diamond: half tile_size width, quarter tile_size height
    i32 hw = tile_size / 2;  // half width
    i32 qh = tile_size / 4;  // quarter height

    // Adjust for view offset
    i32 screen_x = (i32)pos.x + offset_x;
    i32 screen_y = (i32)pos.y + offset_y;

    // Draw a diamond (isometric square)
    SDL_FPoint points[4] = {
        {(f32)(screen_x + hw), (f32)screen_y},           // top
        {(f32)(screen_x + hw * 2), (f32)(screen_y + qh)},  // right
        {(f32)(screen_x + hw), (f32)(screen_y + qh * 2)},   // bottom
        {(f32)screen_x, (f32)(screen_y + qh)},          // left
    };

    SDL_SetRenderDrawColor(rend, r, g, b, 255);

    // Draw edges
    for (i32 i = 0; i < 4; i++) {
        SDL_RenderLine(rend, points[i].x, points[i].y, points[(i + 1) % 4].x, points[(i + 1) % 4].y);
    }

    // Fill with slight transparency by drawing a filled quad
    // For now, just draw the outline
}

static void render_miner(SDL_Renderer *rend, Entity_Miner *miner, i32 tile_size, i32 offset_x, i32 offset_y) {
    // Draw miner as brown diamond
    render_rect_iso(rend, miner->grid_x, miner->grid_y, tile_size, 139, 90, 43, offset_x, offset_y);

    // Draw a small pickaxe icon in the center
    Vec2 pos = grid_to_isometric(miner->grid_x, miner->grid_y, tile_size);
    i32 cx = (i32)pos.x + offset_x + tile_size / 2;
    i32 cy = (i32)pos.y + offset_y + tile_size / 4;

    SDL_SetRenderDrawColor(rend, 200, 200, 200, 255);
    SDL_RenderLine(rend, cx - 3, cy - 2, cx + 3, cy + 2);
    SDL_RenderLine(rend, cx - 3, cy + 2, cx + 3, cy - 2);
}

static void render_storage(SDL_Renderer *rend, Entity_Storage *storage, i32 tile_size, i32 offset_x, i32 offset_y) {
    // Draw storage as gray diamond
    render_rect_iso(rend, storage->grid_x, storage->grid_y, tile_size, 150, 150, 150, offset_x, offset_y);

    // Draw ore stored amount as a bar
    Vec2 pos = grid_to_isometric(storage->grid_x, storage->grid_y, tile_size);
    i32 cx = (i32)pos.x + offset_x + tile_size / 2;
    i32 cy = (i32)pos.y + offset_y + tile_size / 4;

    SDL_SetRenderDrawColor(rend, 200, 100, 50, 255);
    i32 fill_height = (storage->ore_stored * tile_size / 4) / storage->ore_capacity;
    SDL_RenderLine(rend, cx - 2, cy, cx + 2, cy);
    if (fill_height > 0) {
        SDL_RenderLine(rend, cx - 2, cy - fill_height, cx + 2, cy - fill_height);
    }
}

void render_world(UI_Context *ui, World *w, i32 tile_size, i32 offset_x, i32 offset_y) {
    if (!ui || !ui->renderer || !w) return;

    // Draw all entities
    for (i32 i = 0; i < w->entity_count; i++) {
        Entity *e = &w->entities[i];

        if (e->type == ENTITY_MINER) {
            render_miner(ui->renderer, &e->data.miner, tile_size, offset_x, offset_y);
        } else if (e->type == ENTITY_STORAGE) {
            render_storage(ui->renderer, &e->data.storage, tile_size, offset_x, offset_y);
        }
    }
}
