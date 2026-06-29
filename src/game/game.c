// Game implementation (included into mach.c).

#include "game.h"
#include "../engine/debug.h"
#include <math.h>

// Camera zoom limits (pixels-per-iso-unit multiplier).
#define ZOOM_MIN 0.5f
#define ZOOM_MAX 5.0f

// Center the 2D iso camera on grid cell (cx, cy) at a default zoom.
static void setup_camera(Camera2D *c, f32 cx, f32 cy) {
    c->pan.x = (cx - cy) * (ISO_TILE_W * 0.5f);
    c->pan.y = (cx + cy) * (ISO_TILE_H * 0.5f);
    c->zoom = 2.0f;
}

// Initialize game state with an empty world and spawn test entities for development.
void game_init(Game_State *g) {
    g->world = world_create();
    g->selected_tool = TOOL_NONE;
    g->hover_grid_x = 0;
    g->hover_grid_y = 0;
    g->hover_valid = MACH_FALSE;
    g->hover_can_place = MACH_FALSE;

    setup_camera(&g->camera, 5.0f, 5.0f);

    if (g->world) {
        world_spawn_miner(g->world, 5, 5);
        world_spawn_storage(g->world, 6, 5);
    }

    LOG_INFO("game initialized (2D iso camera)");
}

// Update the hovered grid cell by inverse-projecting the mouse onto the ground
// plane. Grid cell (x,y) is the tile centered at iso coordinate (x, y).
void game_update_hover(Game_State *g, f32 screen_w, f32 screen_h, i32 mouse_x, i32 mouse_y) {
    if (!g) return;

    Vec2 grid = screen_to_iso(&g->camera, screen_w, screen_h, (f32)mouse_x, (f32)mouse_y);
    g->hover_grid_x = (i32)floorf(grid.x + 0.5f);
    g->hover_grid_y = (i32)floorf(grid.y + 0.5f);
    g->hover_valid = (g->hover_grid_x >= 0 && g->hover_grid_x < WORLD_GRID_SIZE &&
                      g->hover_grid_y >= 0 && g->hover_grid_y < WORLD_GRID_SIZE);
    g->hover_can_place = g->hover_valid &&
                         world_can_place_at(g->world, g->hover_grid_x, g->hover_grid_y);
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
void game_handle_input(Game_State *g, f32 screen_w, f32 screen_h, i32 mouse_x, i32 mouse_y, i32 button) {
    if (!g || !g->world) return;

    game_update_hover(g, screen_w, screen_h, mouse_x, mouse_y);
    if (button != 1 || !g->hover_valid) return;

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

// Pan the camera. (dx, dy) is screen-space motion in pixels; divide by zoom to
// move the iso-space focus point by the same on-screen amount at any zoom.
void game_camera_pan(Game_State *g, f32 dx, f32 dy) {
    if (!g) return;
    g->camera.pan.x += dx / g->camera.zoom;
    g->camera.pan.y += dy / g->camera.zoom;
}

// Zoom the camera. Positive delta zooms in (larger tiles).
void game_camera_zoom(Game_State *g, f32 zoom_delta) {
    if (!g) return;
    g->camera.zoom *= (1.0f + zoom_delta);
    g->camera.zoom = math_clamp(g->camera.zoom, ZOOM_MIN, ZOOM_MAX);
}
