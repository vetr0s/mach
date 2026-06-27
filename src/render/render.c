// Render implementation (included into mach.c).

#include "render.h"
#include <SDL3/SDL.h>

// Convert grid coordinates to screen space using standard isometric projection.
Vec2 grid_to_isometric(i32 grid_x, i32 grid_y, i32 tile_size) {
    Vec2 result;
    result.x = (f32)((grid_x - grid_y) * (tile_size / 2));
    result.y = (f32)((grid_x + grid_y) * (tile_size / 4));
    return result;
}

static void render_rect_iso(SDL_Renderer *rend, i32 grid_x, i32 grid_y, i32 tile_size,
                            u8 r, u8 g, u8 b, i32 offset_x, i32 offset_y) {
    Vec2 pos = grid_to_isometric(grid_x, grid_y, tile_size);

    i32 hw = tile_size / 2;
    i32 qh = tile_size / 4;

    // (npt): Center the diamond on the grid position by offsetting by half the dimensions
    i32 screen_x = (i32)pos.x + offset_x + hw;
    i32 screen_y = (i32)pos.y + offset_y + qh;

    SDL_FPoint points[4] = {
        {(f32)screen_x, (f32)(screen_y - qh)},           // top
        {(f32)(screen_x + hw), (f32)screen_y},           // right
        {(f32)screen_x, (f32)(screen_y + qh)},           // bottom
        {(f32)(screen_x - hw), (f32)screen_y},           // left
    };

    SDL_SetRenderDrawColor(rend, r, g, b, 255);

    for (i32 i = 0; i < 4; i++) {
        SDL_RenderLine(rend, points[i].x, points[i].y, points[(i + 1) % 4].x, points[(i + 1) % 4].y);
    }
}

static void render_miner(SDL_Renderer *rend, Entity_Miner *miner, i32 tile_size, i32 offset_x, i32 offset_y) {
    render_rect_iso(rend, miner->grid_x, miner->grid_y, tile_size, 139, 90, 43, offset_x, offset_y);

    Vec2 pos = grid_to_isometric(miner->grid_x, miner->grid_y, tile_size);
    i32 cx = (i32)pos.x + offset_x + tile_size / 2;
    i32 cy = (i32)pos.y + offset_y + tile_size / 4;

    SDL_SetRenderDrawColor(rend, 200, 200, 200, 255);
    SDL_RenderLine(rend, cx - 3, cy - 2, cx + 3, cy + 2);
    SDL_RenderLine(rend, cx - 3, cy + 2, cx + 3, cy - 2);
}

static void render_storage(SDL_Renderer *rend, Entity_Storage *storage, i32 tile_size, i32 offset_x, i32 offset_y) {
    render_rect_iso(rend, storage->grid_x, storage->grid_y, tile_size, 150, 150, 150, offset_x, offset_y);

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

// Render all entities in the world to the given viewport.
void render_world(UI_Context *ui, World *w, i32 tile_size, i32 offset_x, i32 offset_y) {
    if (!ui || !ui->renderer || !w) return;
    for (i32 i = 0; i < w->entity_count; i++) {
        Entity *e = &w->entities[i];

        if (e->type == ENTITY_MINER) {
            render_miner(ui->renderer, &e->data.miner, tile_size, offset_x, offset_y);
        } else if (e->type == ENTITY_STORAGE) {
            render_storage(ui->renderer, &e->data.storage, tile_size, offset_x, offset_y);
        }
    }
}

// Render a semi-transparent preview at the snapped grid position.
void render_hover_preview(SDL_Renderer *rend, i32 grid_x, i32 grid_y, i32 tile_size, i32 offset_x, i32 offset_y, i32 tool) {
    if (!rend) return;

    Vec2 pos = grid_to_isometric(grid_x, grid_y, tile_size);

    i32 hw = tile_size / 2;
    i32 qh = tile_size / 4;

    // (npt): Use same centering as render_rect_iso so preview matches placed machine
    i32 screen_x = (i32)pos.x + offset_x + hw;
    i32 screen_y = (i32)pos.y + offset_y + qh;

    SDL_FPoint points[4] = {
        {(f32)screen_x, (f32)(screen_y - qh)},
        {(f32)(screen_x + hw), (f32)screen_y},
        {(f32)screen_x, (f32)(screen_y + qh)},
        {(f32)(screen_x - hw), (f32)screen_y},
    };

    u8 r = 100, g = 200, b = 100;
    if (tool == 2) {
        r = 150;
        g = 150;
        b = 200;
    }

    SDL_SetRenderDrawColor(rend, r, g, b, 100);
    for (i32 i = 0; i < 4; i++) {
        SDL_RenderLine(rend, points[i].x, points[i].y, points[(i + 1) % 4].x, points[(i + 1) % 4].y);
    }
}
