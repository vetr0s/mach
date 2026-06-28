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

static void render_rect_iso(SDL_Renderer *rend, i32 grid_x, i32 grid_y, i32 tile_size,
                            u8 r, u8 g, u8 b, i32 camera_x, i32 camera_y, f32 zoom) {
    Vec2 pos = grid_to_isometric(grid_x, grid_y, tile_size);

    i32 hw = tile_size / 2;
    i32 qh = tile_size / 4;

    // (npt): Convert world-space to screen-space: (world - camera) * zoom + screen_center
    // Camera represents top-left corner of viewport in world space
    i32 screen_x = (i32)(((f32)pos.x + (f32)hw - (f32)camera_x) * zoom) + 640;
    i32 screen_y = (i32)(((f32)pos.y + (f32)qh - (f32)camera_y) * zoom) + 360;

    // (npt): Scale diamond dimensions by zoom
    i32 hw_z = (i32)((f32)hw * zoom);
    i32 qh_z = (i32)((f32)qh * zoom);

    SDL_FPoint points[4] = {
        {(f32)screen_x, (f32)(screen_y - qh_z)},
        {(f32)(screen_x + hw_z), (f32)screen_y},
        {(f32)screen_x, (f32)(screen_y + qh_z)},
        {(f32)(screen_x - hw_z), (f32)screen_y},
    };

    SDL_SetRenderDrawColor(rend, r, g, b, 255);

    for (i32 i = 0; i < 4; i++) {
        SDL_RenderLine(rend, points[i].x, points[i].y, points[(i + 1) % 4].x, points[(i + 1) % 4].y);
    }
}

static void render_miner(SDL_Renderer *rend, Entity_Miner *miner, i32 tile_size, i32 camera_x, i32 camera_y, f32 zoom) {
    render_rect_iso(rend, miner->grid_x, miner->grid_y, tile_size, 139, 90, 43, camera_x, camera_y, zoom);

    Vec2 pos = grid_to_isometric(miner->grid_x, miner->grid_y, tile_size);
    i32 hw = tile_size / 2;
    i32 qh = tile_size / 4;
    i32 cx = (i32)(((f32)pos.x + (f32)hw - (f32)camera_x) * zoom) + 640;
    i32 cy = (i32)(((f32)pos.y + (f32)qh - (f32)camera_y) * zoom) + 360;

    SDL_SetRenderDrawColor(rend, 200, 200, 200, 255);
    SDL_RenderLine(rend, cx - 3, cy - 2, cx + 3, cy + 2);
    SDL_RenderLine(rend, cx - 3, cy + 2, cx + 3, cy - 2);
}

static void render_storage(SDL_Renderer *rend, Entity_Storage *storage, i32 tile_size, i32 camera_x, i32 camera_y, f32 zoom) {
    render_rect_iso(rend, storage->grid_x, storage->grid_y, tile_size, 150, 150, 150, camera_x, camera_y, zoom);

    Vec2 pos = grid_to_isometric(storage->grid_x, storage->grid_y, tile_size);
    i32 hw = tile_size / 2;
    i32 qh = tile_size / 4;
    i32 cx = (i32)(((f32)pos.x + (f32)hw - (f32)camera_x) * zoom) + 640;
    i32 cy = (i32)(((f32)pos.y + (f32)qh - (f32)camera_y) * zoom) + 360;

    SDL_SetRenderDrawColor(rend, 200, 100, 50, 255);
    i32 fill_height = (storage->ore_stored * tile_size / 4) / storage->ore_capacity;
    SDL_RenderLine(rend, cx - 2, cy, cx + 2, cy);
    if (fill_height > 0) {
        SDL_RenderLine(rend, cx - 2, cy - fill_height, cx + 2, cy - fill_height);
    }
}

// Render all entities in the world to the given viewport.
void render_world(UI_Context *ui, World *w, i32 tile_size, i32 camera_x, i32 camera_y, f32 zoom) {
    if (!ui || !ui->renderer || !w) return;
    for (i32 i = 0; i < w->entity_count; i++) {
        Entity *e = &w->entities[i];

        if (e->type == ENTITY_MINER) {
            render_miner(ui->renderer, &e->data.miner, tile_size, camera_x, camera_y, zoom);
        } else if (e->type == ENTITY_STORAGE) {
            render_storage(ui->renderer, &e->data.storage, tile_size, camera_x, camera_y, zoom);
        }
    }
}

// Render the 256x256 placeable grid.
void render_grid(SDL_Renderer *rend, i32 tile_size, i32 camera_x, i32 camera_y, f32 zoom, i32 screen_w, i32 screen_h) {
    if (!rend) return;

    SDL_SetRenderDrawColor(rend, 60, 80, 100, 80);

    i32 hw = tile_size / 2;
    i32 qh = tile_size / 4;

    // Draw grid lines for the 256x256 area
    // Only draw tiles that are visible on screen to avoid lag
    for (i32 grid_y = 0; grid_y < 256; grid_y++) {
        for (i32 grid_x = 0; grid_x < 256; grid_x++) {
            Vec2 pos = grid_to_isometric(grid_x, grid_y, tile_size);
            i32 screen_x = (i32)(((f32)pos.x + (f32)hw - (f32)camera_x) * zoom) + 640;
            i32 screen_y = (i32)(((f32)pos.y + (f32)qh - (f32)camera_y) * zoom) + 360;

            i32 hw_z = (i32)((f32)hw * zoom);
            i32 qh_z = (i32)((f32)qh * zoom);

            // Only draw if roughly on screen
            if (screen_x < -hw_z * 2 || screen_x > screen_w + hw_z * 2 ||
                screen_y < -qh_z * 2 || screen_y > screen_h + qh_z * 2) {
                continue;
            }

            SDL_FPoint points[4] = {
                {(f32)screen_x, (f32)(screen_y - qh_z)},
                {(f32)(screen_x + hw_z), (f32)screen_y},
                {(f32)screen_x, (f32)(screen_y + qh_z)},
                {(f32)(screen_x - hw_z), (f32)screen_y},
            };

            for (i32 i = 0; i < 4; i++) {
                SDL_RenderLine(rend, points[i].x, points[i].y, points[(i + 1) % 4].x, points[(i + 1) % 4].y);
            }
        }
    }
}

// Render a semi-transparent preview at the snapped grid position.
void render_hover_preview(SDL_Renderer *rend, i32 grid_x, i32 grid_y, i32 tile_size, i32 camera_x, i32 camera_y, f32 zoom, i32 tool, int can_place) {
    if (!rend) return;

    Vec2 pos = grid_to_isometric(grid_x, grid_y, tile_size);

    i32 hw = tile_size / 2;
    i32 qh = tile_size / 4;

    // (npt): Apply camera transform matching render_rect_iso
    i32 screen_x = (i32)(((f32)pos.x + (f32)hw - (f32)camera_x) * zoom) + 640;
    i32 screen_y = (i32)(((f32)pos.y + (f32)qh - (f32)camera_y) * zoom) + 360;

    i32 hw_z = (i32)((f32)hw * zoom);
    i32 qh_z = (i32)((f32)qh * zoom);

    SDL_FPoint points[4] = {
        {(f32)screen_x, (f32)(screen_y - qh_z)},
        {(f32)(screen_x + hw_z), (f32)screen_y},
        {(f32)screen_x, (f32)(screen_y + qh_z)},
        {(f32)(screen_x - hw_z), (f32)screen_y},
    };

    u8 r, g, b;
    if (!can_place) {
        r = 255;
        g = 100;
        b = 100;
    } else if (tool == 2) {
        r = 150;
        g = 150;
        b = 200;
    } else {
        r = 100;
        g = 200;
        b = 100;
    }

    SDL_SetRenderDrawColor(rend, r, g, b, 100);
    for (i32 i = 0; i < 4; i++) {
        SDL_RenderLine(rend, points[i].x, points[i].y, points[(i + 1) % 4].x, points[(i + 1) % 4].y);
    }
}

// (npt): Draw a simple digit using 7-segment style lines (very basic).
static void draw_digit(SDL_Renderer *rend, i32 x, i32 y, i32 digit) {
    // Draw simple vertical bars for each digit (0-9)
    // Just draw bars representing the digit value visually
    i32 bar_height = 8;
    for (i32 i = 0; i < digit % 10; i++) {
        render_rect_filled(rend, x + i * 3, y, 2, bar_height, 150, 200, 100, 255);
    }
}

// Debug text display: FPS and current tool binds.
void render_debug_text(SDL_Renderer *rend, i32 fps, i32 tool, i32 screen_w, i32 screen_h) {
    (void)screen_w;
    (void)screen_h;

    if (!rend) return;

    i32 x = 10, y = 10;

    // Background panel
    render_rect_filled(rend, x, y, 180, 100, 0, 0, 0, 220);
    render_rect_outline(rend, x, y, 180, 100, 100, 200, 100);

    // FPS display - draw tens and ones digits
    i32 fps_tens = fps / 10;
    i32 fps_ones = fps % 10;

    draw_digit(rend, x + 15, y + 12, fps_tens);
    draw_digit(rend, x + 50, y + 12, fps_ones);

    // Tool indicator: 3 colored blocks
    u8 tool_colors[4][3] = {
        {100, 100, 100},  // 0 = none (gray)
        {100, 255, 100},  // 1 = miner (green)
        {150, 150, 255},  // 2 = storage (blue)
        {255, 100, 100},  // 3 = delete (red)
    };

    for (i32 i = 0; i < 3; i++) {
        u8 r = tool_colors[i + 1][0];
        u8 g = tool_colors[i + 1][1];
        u8 b = tool_colors[i + 1][2];

        i32 size = (i + 1 == tool) ? 40 : 32;
        i32 offset = (i + 1 == tool) ? -4 : 0;
        i32 thick = (i + 1 == tool) ? 3 : 1;

        render_rect_filled(rend, x + 20 + i * 48 + offset, y + 50 + offset, size, size, r, g, b, 255);

        // Thicker outline for selected tool
        for (i32 j = 0; j < thick; j++) {
            render_rect_outline(rend, x + 20 + i * 48 + offset - j, y + 50 + offset - j,
                               size + j * 2, size + j * 2, r, g, b);
        }
    }
}
