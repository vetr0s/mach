// Game state and input handling.

#ifndef GAME_H
#define GAME_H

#include "mach.h"
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
    Mach_Arena arena;          // backs the world; freed whole at shutdown
    World *world;
    Mach_ClayUI clay;          // HUD layout/draw; in host memory so it survives hot reload
    i32 selected_tool;
    Direction place_dir;  // facing applied to directional pieces on placement

    Mach_Camera2D camera;

    f32 sim_accumulator;  // real seconds carried toward the next fixed sim tick
    f32 anim_time;        // real seconds elapsed, for continuous visual animation
    b32 paused;           // freeze the world: no sim ticks and no animation advance
    b32 show_debug;        // backtick-toggled overlay: everything but money is off by default

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

// The four functions the loop (main or the hot-reload host) calls. They are the
// game's whole surface, and the hot-reload seam, so they keep external linkage.
Mach_Config game_config(void);                    // window + engine policy
void game_init(Game_State *g, Mach *m);
void game_frame(Game_State *g, Mach *m);          // input + sim + draw, one frame
void game_shutdown(Game_State *g);

// Internals of game_frame, split for readability.
void game_tick(Game_State *g, f32 dt);

// Consume this frame's input snapshot: tool keys, rotate, pause, placement
// clicks, camera pan/zoom, and hover tracking. screen_w/h are the current render
// size, used to unproject the mouse onto the grid.
void game_process_input(Game_State *g, const Mach_Input *in, f32 screen_w, f32 screen_h, f32 dt);

// Compact money-style value: "25", "1.5K", "3.2M", up to quintillions (an i64
// tops out at ~9.2 Qi). Keeps labels short as values run away.
void game_format_value(i64 v, char *buf, usize n);

#endif
