// Game implementation (included into mach.c).

#include "game.h"

// (npt): Inverse isometric projection accounting for camera position and zoom.
static i32 screen_to_grid(i32 screen_x, i32 screen_y, i32 tile_size, f32 camera_x, f32 camera_y, f32 zoom, i32 *out_gx, i32 *out_gy) {
    f32 half_tile = (f32)tile_size / 2.0f;
    f32 quarter_tile = (f32)tile_size / 4.0f;

    // Invert the render transform: screen -> isometric world -> grid
    // Render adds hw and qh to center the diamond, so subtract them back
    f32 adjusted_x = ((f32)screen_x - 640.0f) / zoom + camera_x - half_tile;
    f32 adjusted_y = ((f32)screen_y - 360.0f) / zoom + camera_y - quarter_tile;

    f32 gx_f = (adjusted_x / half_tile + adjusted_y / quarter_tile) / 2.0f;
    f32 gy_f = (adjusted_y / quarter_tile - adjusted_x / half_tile) / 2.0f;

    i32 gx = (i32)(gx_f + 0.5f);
    i32 gy = (i32)(gy_f + 0.5f);

    *out_gx = gx;
    *out_gy = gy;

    return 1;
}

// Initialize game state with an empty world and spawn test entities for development.
void game_init(Game_State *g) {
    g->world = world_create();
    g->selected_tool = 0;
    g->tile_size = 32;
    g->camera_x = 0.0f;
    g->camera_y = 0.0f;
    g->zoom = 1.0f;
    g->hover_grid_x = 0;
    g->hover_grid_y = 0;

    if (g->world) {
        world_spawn_miner(g->world, 5, 5);
        world_spawn_storage(g->world, 6, 5);
    }
}

// Update hover grid position based on mouse screen coordinates.
void game_update_hover(Game_State *g, i32 mouse_x, i32 mouse_y) {
    if (!g) return;
    screen_to_grid(mouse_x, mouse_y, g->tile_size, g->camera_x, g->camera_y, g->zoom,
                   &g->hover_grid_x, &g->hover_grid_y);
    g->hover_can_place = world_can_place_at(g->world, g->hover_grid_x, g->hover_grid_y);
}

// Advance game simulation. dt is delta time in seconds since last frame.
void game_tick(Game_State *g, f32 dt) {
    if (!g || !g->world) return;
    (void)dt;
    world_tick(g->world);
}

// Clean up game state and free resources.
void game_shutdown(Game_State *g) {
    if (g && g->world) {
        world_destroy(g->world);
        g->world = NULL;
    }
}

// Handle mouse input for machine placement and deletion.
void game_handle_input(Game_State *g, i32 mouse_x, i32 mouse_y, i32 button) {
    if (!g || !g->world) return;

    game_update_hover(g, mouse_x, mouse_y);

    if (button == 1) {
        if (g->selected_tool == 1) {
            world_spawn_miner(g->world, g->hover_grid_x, g->hover_grid_y);
        } else if (g->selected_tool == 2) {
            world_spawn_storage(g->world, g->hover_grid_x, g->hover_grid_y);
        } else if (g->selected_tool == 3) {
            i32 entity_id = world_get_entity_at(g->world, g->hover_grid_x, g->hover_grid_y);
            if (entity_id != 0) {
                world_despawn(g->world, entity_id);
            }
        }
    }
}

// Handle keyboard input for tool selection and actions.
void game_handle_key(Game_State *g, SDL_Scancode scancode) {
    if (!g) return;

    switch (scancode) {
    case SDL_SCANCODE_1:
        g->selected_tool = (g->selected_tool == 1) ? 0 : 1;
        break;
    case SDL_SCANCODE_2:
        g->selected_tool = (g->selected_tool == 2) ? 0 : 2;
        break;
    case SDL_SCANCODE_3:
        g->selected_tool = (g->selected_tool == 3) ? 0 : 3;
        break;
    default:
        break;
    }
}

// Handle key release for camera panning.
void game_handle_key_up(Game_State *g, SDL_Scancode scancode) {
    (void)g;
    (void)scancode;
}

// Pan camera by offset (in screen space).
void game_camera_pan(Game_State *g, f32 dx, f32 dy) {
    if (!g) return;
    g->camera_x += dx;
    g->camera_y += dy;
}

// Zoom camera (positive = zoom in, negative = zoom out).
void game_camera_zoom(Game_State *g, f32 zoom_delta) {
    if (!g) return;
    g->zoom += zoom_delta;
    if (g->zoom < 0.1f) g->zoom = 0.1f;
    if (g->zoom > 5.0f) g->zoom = 5.0f;
}
