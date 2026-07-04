// Game state and input handling.

#ifndef GAME_H
#define GAME_H

#include "../engine/base/base.h"
#include "../engine/input/input.h"
#include "../engine/render/render2d.h"
#include "world/world.h"

// Fixed simulation rate. The world advances in discrete ticks at this rate,
// decoupled from the render framerate. The renderer reads SIM_TICK_DT to turn the
// leftover accumulator into an interpolation fraction.
//
// This doubles as the global belt speed today: an item advances one cell per tick,
// so belts run at SIM_TICKS_PER_SEC cells/sec. That's deliberately slow (chunky
// tier-1 feel) with headroom to speed up later. When placeable belt tiers land,
// belt speed moves off this clock onto a per-entity ticks-per-cell cadence (and the
// item interpolation has to span that window), leaving this as just the sim heartbeat.
#define SIM_TICKS_PER_SEC 3
#define SIM_TICK_DT       (1.0f / (f32)SIM_TICKS_PER_SEC)

typedef struct {
    Arena arena;          // backs the world; freed whole at shutdown
    World *world;
    i32 selected_tool;
    Direction place_dir;  // facing applied to directional pieces on placement

    Camera2D camera;

    f32 sim_accumulator;  // real seconds carried toward the next fixed sim tick
    f32 anim_time;        // real seconds elapsed, for continuous visual animation
    b32 paused;           // freeze the world: no sim ticks and no animation advance
    b32 show_debug;        // F3-style overlay: everything but money is toggled off by default

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

// Consume this frame's input snapshot: tool keys, rotate, pause, placement
// clicks, camera pan/zoom, and hover tracking. screen_w/h are the current render
// size, used to unproject the mouse onto the grid.
void game_process_input(Game_State *g, const Input *in, f32 screen_w, f32 screen_h, f32 dt);

#endif
