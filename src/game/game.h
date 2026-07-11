// Game state and input handling.

#ifndef GAME_H
#define GAME_H

#include "mach.h"
#include "effects.h"
#include "sprites.h"
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
#define SIM_TICK_DT (1.0f / (f32)SIM_TICKS_PER_SEC)

// Which screen the app is showing. game_frame dispatches on this: the menu and the
// running factory are separate top-level modes so neither has to know about the
// other. New screens (settings, a load picker) slot in here.
typedef enum {
    SCREEN_MENU = 0, // title screen: new game, load, quit (menu.c)
    SCREEN_PLAYING,  // the running factory: sim + HUD (the rest of game.c)
    SCREEN_PAUSED,   // pause overlay over the frozen game: resume or main menu (menu.c)
} App_Screen;

typedef struct {
    Mach_Arena arena; // backs the world; freed whole at shutdown
    World *world;
    Mach_ClayUI clay; // HUD layout/draw; in host memory so it survives hot reload
    Sprites sprites;  // baked-in art; empty until PNGs land in assets/sprites
    Effects effects;  // transient visuals fed by world events; real-time, sim-independent
    App_Screen screen;
    i32 selected_tool;
    i32 selected_tier;   // tier bought when placing a dropper/upgrader (1..MAX_TIER)
    Direction place_dir; // facing applied to directional pieces on placement

    Mach_Camera2D camera;

    f32 sim_accumulator; // real seconds carried toward the next fixed sim tick
    f32 anim_time;       // real seconds elapsed, for continuous visual animation
    b32 paused;          // freeze the world: no sim ticks and no animation advance
    b32 show_debug;      // backtick-toggled overlay: everything but money is off by default

    // Hover: the grid cell currently under the mouse.
    i32 hover_grid_x;
    i32 hover_grid_y;
    b32 hover_valid; // the mouse projected onto the ground plane
    b32 hover_can_place;

    b32 pointer_over_ui; // set by the HUD when the cursor is over the shop panel, so a
                         // click on the panel doesn't also place a tile in the world
} Game_State;

// Tool IDs for Game_State.selected_tool.
typedef enum {
    TOOL_NONE = 0,
    TOOL_DROPPER,
    TOOL_CONVEYOR,
    TOOL_UPGRADER,
    TOOL_FURNACE,
    TOOL_DELETE,
    TOOL_COUNT,
} Tool;

// The four functions the loop (main or the hot-reload host) calls. They are the
// game's whole surface, and the hot-reload seam, so they keep external linkage.
Mach_Config game_config(void); // window + engine policy
void game_init(Game_State *g, Mach *m);
void game_frame(Game_State *g, Mach *m); // input + sim + draw, one frame
void game_shutdown(Game_State *g);

// Internals of game_frame, split for readability.
void game_tick(Game_State *g, f32 dt);

// Reset to a fresh new game: clear the world and lay the starter state. Used by the
// menu's "New Game" and at first launch.
void game_new(Game_State *g);

// Persistence. One save slot, a versioned binary blob (see save.c). game_save writes
// the full world + economy + camera state; game_load replaces the current game with a
// saved one. Both return false on failure (and game_load leaves the game untouched on
// a bad or missing file). save_exists reports whether a save is present, for the
// menu's "Continue"/"Load" affordance.
#define SAVE_PATH "mach_save.dat"
b32 game_save(const Game_State *g, const char *path);
b32 game_load(Game_State *g, const char *path);
b32 save_exists(const char *path);

// Consume this frame's input snapshot: tool keys, rotate, pause, placement
// clicks, camera pan/zoom, and hover tracking. screen_w/h are the current render
// size, used to unproject the mouse onto the grid.
void game_process_input(Game_State *g, const Mach_Input *in, f32 screen_w, f32 screen_h, f32 dt);

// Compact money-style value: "25", "1.5K", "3.2M", up to quintillions (an i64
// tops out at ~9.2 Qi). Keeps labels short as values run away.
void game_format_value(i64 v, char *buf, usize n);

#endif
