// Game state and input handling.

#ifndef GAME_H
#define GAME_H

#include "../engine/base/base.h"
#include "world/world.h"
#include <SDL3/SDL.h>

typedef struct {
    World *world;
    i32 selected_tool;
    i32 tile_size;
    // Camera
    f32 camera_x;       // Camera position in screen space
    f32 camera_y;
    f32 zoom;           // Zoom level (1.0 = normal, 2.0 = 2x magnified)
    // Input
    i32 hover_grid_x;
    i32 hover_grid_y;
    b32 hover_can_place;
} Game_State;

// Tool IDs for Game_State.selected_tool.
typedef enum {
    TOOL_NONE = 0,
    TOOL_MINER,
    TOOL_STORAGE,
    TOOL_DELETE,
    TOOL_COUNT,
} Tool;

void game_init(Game_State *g);
void game_tick(Game_State *g, f32 dt);
void game_shutdown(Game_State *g);
void game_update_hover(Game_State *g, i32 mouse_x, i32 mouse_y);
void game_handle_input(Game_State *g, i32 mouse_x, i32 mouse_y, i32 button);
void game_handle_key(Game_State *g, SDL_Scancode scancode);
void game_camera_pan(Game_State *g, f32 dx, f32 dy);
void game_camera_zoom(Game_State *g, f32 zoom_delta);

#endif
