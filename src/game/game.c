// Game implementation (included into mach.c).

#include "game.h"
#include "../engine/debug.h"
#include <math.h>

// Camera zoom limits (pixels-per-iso-unit multiplier).
#define ZOOM_MIN 0.5f
#define ZOOM_MAX 5.0f

// SIM_TICKS_PER_SEC / SIM_TICK_DT live in game.h so the renderer can read them.

// Center the 2D iso camera on grid cell (cx, cy) at a default zoom.
static void setup_camera(Camera2D *c, f32 cx, f32 cy) {
    c->pan.x = (cx - cy) * (ISO_TILE_W * 0.5f);
    c->pan.y = (cx + cy) * (ISO_TILE_H * 0.5f);
    c->zoom = 2.0f;
}

// Initialize game state with a world and a short starter chain so there's a value
// loop running on first launch: dropper -> conveyor -> upgrader -> conveyor ->
// collector, all facing east.
void game_init(Game_State *g) {
    g->arena = (Arena){0};
    g->world = world_create(&g->arena);
    g->selected_tool = TOOL_NONE;
    g->place_dir = DIR_E;
    g->hover_grid_x = 0;
    g->hover_grid_y = 0;
    g->hover_valid = MACH_FALSE;
    g->hover_can_place = MACH_FALSE;
    g->sim_accumulator = 0.0f;

    setup_camera(&g->camera, 7.0f, 5.0f);

    if (g->world) {
        world_spawn_dropper(g->world, 5, 5, DIR_E);
        world_spawn_conveyor(g->world, 6, 5, DIR_E);
        world_spawn_upgrader(g->world, 7, 5, DIR_E);
        world_spawn_conveyor(g->world, 8, 5, DIR_E);
        world_spawn_collector(g->world, 9, 5);
    }

    LOG_INFO("game initialized (value-loop sim)");
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

// Advance the simulation. Rendering runs every frame, but the world steps at a
// fixed rate: we bank real elapsed time and run world_tick in fixed slices, so a
// miner produces the same ore per second regardless of framerate. dt is already
// clamped engine-side, so the accumulator can't run away after a long stall.
void game_tick(Game_State *g, f32 dt) {
    if (!g || !g->world) return;

    g->sim_accumulator += dt;
    while (g->sim_accumulator >= SIM_TICK_DT) {
        world_tick(g->world);
        g->sim_accumulator -= SIM_TICK_DT;
    }
}

// Clean up game state and free resources.
void game_shutdown(Game_State *g) {
    if (!g) return;
    if (g->world) {
        LOG_INFO("world torn down (%d entities, tick %d)", g->world->entity_count, g->world->tick);
        g->world = NULL;
    }
    arena_free(&g->arena);
    LOG_INFO("game shut down");
}

// Handle mouse input for machine placement and deletion.
void game_handle_input(Game_State *g, f32 screen_w, f32 screen_h, i32 mouse_x, i32 mouse_y, i32 button) {
    if (!g || !g->world) return;

    game_update_hover(g, screen_w, screen_h, mouse_x, mouse_y);
    if (button != 1 || !g->hover_valid) return;

    i32 hx = g->hover_grid_x, hy = g->hover_grid_y;
    switch (g->selected_tool) {
    case TOOL_DROPPER:   world_spawn_dropper(g->world, hx, hy, g->place_dir);   break;
    case TOOL_CONVEYOR:  world_spawn_conveyor(g->world, hx, hy, g->place_dir);  break;
    case TOOL_UPGRADER:  world_spawn_upgrader(g->world, hx, hy, g->place_dir);  break;
    case TOOL_COLLECTOR: world_spawn_collector(g->world, hx, hy);               break;
    case TOOL_DELETE: {
        i32 entity_id = world_get_entity_at(g->world, hx, hy);
        if (entity_id != 0) world_despawn(g->world, entity_id);
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

// Handle keyboard input for tool selection, rotation, and actions.
void game_handle_key(Game_State *g, SDL_Scancode scancode) {
    if (!g) return;

    switch (scancode) {
    case SDL_SCANCODE_1: toggle_tool(g, TOOL_DROPPER);   break;
    case SDL_SCANCODE_2: toggle_tool(g, TOOL_CONVEYOR);  break;
    case SDL_SCANCODE_3: toggle_tool(g, TOOL_UPGRADER);  break;
    case SDL_SCANCODE_4: toggle_tool(g, TOOL_COLLECTOR); break;
    case SDL_SCANCODE_5: toggle_tool(g, TOOL_DELETE);    break;
    case SDL_SCANCODE_R:
        g->place_dir = (Direction)((g->place_dir + 1) % DIR_COUNT);
        LOG_DEBUG("place direction: %d", g->place_dir);
        break;
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
