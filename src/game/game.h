// Game state and input handling.

#ifndef GAME_H
#define GAME_H

#include "../engine/base/base.h"
#include "../engine/render/render2d.h"
#include "world/world.h"
#include <SDL3/SDL.h>

// Fixed simulation rate. The world advances in discrete ticks at this rate,
// decoupled from the render framerate. The renderer reads SIM_TICK_DT to turn the
// leftover accumulator into an interpolation fraction.
#define SIM_TICKS_PER_SEC 10
#define SIM_TICK_DT       (1.0f / (f32)SIM_TICKS_PER_SEC)

typedef struct {
    Arena arena;          // backs the world; freed whole at shutdown
    World *world;
    i32 selected_tool;
    Direction place_dir;  // facing applied to directional pieces on placement

    Camera2D camera;

    f32 sim_accumulator;  // real seconds carried toward the next fixed sim tick

    // Hover: the grid cell currently under the mouse.
    i32 hover_grid_x;
    i32 hover_grid_y;
    b32 hover_valid;      // the mouse projected onto the ground plane
    b32 hover_can_place;
} Game_State;

// Tool IDs for Game_State.selected_tool.
typedef enum {
    TOOL_NONE = 0,
    TOOL_DROPPER,
    TOOL_CONVEYOR,
    TOOL_UPGRADER,
    TOOL_COLLECTOR,
    TOOL_DELETE,
    TOOL_COUNT,
} Tool;

void game_init(Game_State *g);
void game_tick(Game_State *g, f32 dt);
void game_shutdown(Game_State *g);
void game_update_hover(Game_State *g, f32 screen_w, f32 screen_h, i32 mouse_x, i32 mouse_y);
void game_handle_input(Game_State *g, f32 screen_w, f32 screen_h, i32 mouse_x, i32 mouse_y, i32 button);
void game_handle_key(Game_State *g, SDL_Scancode scancode);
void game_camera_pan(Game_State *g, f32 dx, f32 dy);
void game_camera_zoom(Game_State *g, f32 zoom_delta);

#endif
