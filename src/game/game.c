// Game implementation (included into mach.c).

#include "game.h"

// (npt): Approximate inverse isometric projection. Placement may be off by 1 grid cell;
// refinement needed for exact snapping to grid.
static i32 screen_to_grid(i32 screen_x, i32 screen_y, i32 tile_size, i32 offset_x, i32 offset_y, i32 *out_gx, i32 *out_gy) {
    i32 adjusted_x = screen_x - offset_x;
    i32 adjusted_y = screen_y - offset_y;

    i32 gx = (adjusted_x / (tile_size / 2) + adjusted_y / (tile_size / 4)) / 2;
    i32 gy = (adjusted_y / (tile_size / 4) - adjusted_x / (tile_size / 2)) / 2;

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

    // Spawn a test miner and storage to verify rendering
    if (g->world) {
        world_spawn_miner(g->world, 5, 5);
        world_spawn_storage(g->world, 6, 5);
    }
}

// Advance game simulation by one tick.
void game_tick(Game_State *g) {
    if (!g || !g->world) return;
    world_tick(g->world);
}

// Clean up game state and free resources.
void game_shutdown(Game_State *g) {
    if (g && g->world) {
        world_destroy(g->world);
        g->world = NULL;
    }
}

// Handle mouse input for machine placement and deletion. Button: 1=place miner, 2=place storage, 3=delete.
void game_handle_input(Game_State *g, i32 mouse_x, i32 mouse_y, i32 button) {
    if (!g || !g->world) return;

    // Button 1 = place miner, Button 2 = place storage, Button 3 = erase
    if (button == 1 || button == 2) {
        i32 gx, gy;
        screen_to_grid(mouse_x, mouse_y, g->tile_size, g->view_offset_x, g->view_offset_y, &gx, &gy);

        if (button == 1) {
            world_spawn_miner(g->world, gx, gy);
        } else if (button == 2) {
            world_spawn_storage(g->world, gx, gy);
        }
    } else if (button == 3) {
        i32 gx, gy;
        screen_to_grid(mouse_x, mouse_y, g->tile_size, g->view_offset_x, g->view_offset_y, &gx, &gy);

        i32 entity_id = world_get_entity_at(g->world, gx, gy);
        if (entity_id != 0) {
            world_despawn(g->world, entity_id);
        }
    }
}
