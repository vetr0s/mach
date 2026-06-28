// Game implementation (included into mach.c).

#include "game.h"
#include "../engine/ui.h"
#include "../engine/debug.h"

// Camera zoom limits.
#define ZOOM_MIN 0.1f
#define ZOOM_MAX 5.0f

// Inverse isometric projection: screen pixel -> grid cell, accounting for camera
// position and zoom. Mirrors the forward transform in render.c.
static void screen_to_grid(i32 screen_x, i32 screen_y, i32 tile_size,
                           f32 camera_x, f32 camera_y, f32 zoom,
                           i32 *out_gx, i32 *out_gy) {
    f32 half_tile = (f32)tile_size / 2.0f;
    f32 quarter_tile = (f32)tile_size / 4.0f;

    // Undo the render transform: screen -> isometric world -> grid. Render adds
    // half/quarter tile to center the diamond, so subtract them back out.
    f32 adjusted_x = ((f32)screen_x - (f32)SCREEN_CENTER_X) / zoom + camera_x - half_tile;
    f32 adjusted_y = ((f32)screen_y - (f32)SCREEN_CENTER_Y) / zoom + camera_y - quarter_tile;

    f32 gx_f = (adjusted_x / half_tile + adjusted_y / quarter_tile) / 2.0f;
    f32 gy_f = (adjusted_y / quarter_tile - adjusted_x / half_tile) / 2.0f;

    *out_gx = (i32)(gx_f + 0.5f);
    *out_gy = (i32)(gy_f + 0.5f);
}

// Initialize game state with an empty world and spawn test entities for development.
void game_init(Game_State *g) {
    g->world = world_create();
    g->selected_tool = TOOL_NONE;
    g->tile_size = 32;
    g->camera_x = 0.0f;
    g->camera_y = 0.0f;
    g->zoom = 1.0f;
    g->hover_grid_x = 0;
    g->hover_grid_y = 0;
    g->hover_can_place = MACH_FALSE;

    if (g->world) {
        world_spawn_miner(g->world, 5, 5);
        world_spawn_storage(g->world, 6, 5);
    }

    LOG_INFO("game initialized (tile_size %d)", g->tile_size);
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
    LOG_INFO("game shut down");
}

// Handle mouse input for machine placement and deletion.
void game_handle_input(Game_State *g, i32 mouse_x, i32 mouse_y, i32 button) {
    if (!g || !g->world) return;

    game_update_hover(g, mouse_x, mouse_y);

    if (button != 1) return;

    switch (g->selected_tool) {
    case TOOL_MINER:
        world_spawn_miner(g->world, g->hover_grid_x, g->hover_grid_y);
        break;
    case TOOL_STORAGE:
        world_spawn_storage(g->world, g->hover_grid_x, g->hover_grid_y);
        break;
    case TOOL_DELETE: {
        i32 entity_id = world_get_entity_at(g->world, g->hover_grid_x, g->hover_grid_y);
        if (entity_id != 0) {
            world_despawn(g->world, entity_id);
        }
        break;
    }
    default:
        break;
    }
}

// Toggle the given tool: selecting the active tool again clears it.
static void toggle_tool(Game_State *g, Tool tool) {
    g->selected_tool = (g->selected_tool == (i32)tool) ? TOOL_NONE : (i32)tool;
    LOG_DEBUG("selected tool: %d", g->selected_tool);
}

// Handle keyboard input for tool selection and actions.
void game_handle_key(Game_State *g, SDL_Scancode scancode) {
    if (!g) return;

    switch (scancode) {
    case SDL_SCANCODE_1: toggle_tool(g, TOOL_MINER);   break;
    case SDL_SCANCODE_2: toggle_tool(g, TOOL_STORAGE); break;
    case SDL_SCANCODE_3: toggle_tool(g, TOOL_DELETE);  break;
    default: break;
    }
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
    if (g->zoom < ZOOM_MIN) g->zoom = ZOOM_MIN;
    if (g->zoom > ZOOM_MAX) g->zoom = ZOOM_MAX;
}
