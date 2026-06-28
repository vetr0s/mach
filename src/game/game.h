// Game state and input handling.

#ifndef GAME_H
#define GAME_H

#include "../base/base.h"
#include "../world/world.h"
#include <SDL3/SDL.h>

typedef struct {
    World *world;
    i32 selected_tool;  // 0 = none, 1 = miner, 2 = storage
    i32 tile_size;
    i32 view_offset_x;
    i32 view_offset_y;
    i32 hover_grid_x;   // Current mouse position in grid coords
    i32 hover_grid_y;
} Game_State;

void game_init(Game_State *g);
void game_tick(Game_State *g);
void game_shutdown(Game_State *g);
void game_update_hover(Game_State *g, i32 mouse_x, i32 mouse_y);
void game_handle_input(Game_State *g, i32 mouse_x, i32 mouse_y, i32 button);
void game_handle_key(Game_State *g, SDL_Scancode scancode);

#endif
