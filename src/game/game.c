// Game implementation (included into mach.c).

#include "game.h"

// (npt): Inverse isometric projection with proper mathematical solution.
// Given screen coords, find the grid cell. Uses exact math from solving the system:
//   screen_x = (grid_x - grid_y) * (tile_size / 2)
//   screen_y = (grid_x + grid_y) * (tile_size / 4)
static i32 screen_to_grid(i32 screen_x, i32 screen_y, i32 tile_size, i32 offset_x, i32 offset_y, i32 *out_gx, i32 *out_gy) {
    f32 adjusted_x = (f32)(screen_x - offset_x);
    f32 adjusted_y = (f32)(screen_y - offset_y);

    f32 half_tile = (f32)tile_size / 2.0f;
    f32 quarter_tile = (f32)tile_size / 4.0f;

    // (npt): Inverse formulas derived from the forward projection equations
    f32 gx_f = (adjusted_x / half_tile + adjusted_y / quarter_tile) / 2.0f;
    f32 gy_f = (adjusted_y / quarter_tile - adjusted_x / half_tile) / 2.0f;

    // (npt): Round to nearest grid cell
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
    g->view_offset_x = 640;
    g->view_offset_y = 100;
    g->hover_grid_x = 0;
    g->hover_grid_y = 0;

    // Spawn a test miner and storage to verify rendering
    if (g->world) {
        world_spawn_miner(g->world, 5, 5);
        world_spawn_storage(g->world, 6, 5);
    }
}

// Update hover grid position based on mouse screen coordinates.
void game_update_hover(Game_State *g, i32 mouse_x, i32 mouse_y) {
    if (!g) return;
    screen_to_grid(mouse_x, mouse_y, g->tile_size, g->view_offset_x, g->view_offset_y,
                   &g->hover_grid_x, &g->hover_grid_y);
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

    // Left click: place selected tool or delete if in delete mode
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
