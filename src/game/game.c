// Game implementation (included into mach.c).

#include "game.h"
#include "render_game.h"
#include <math.h>
#include <stdio.h>

// Camera zoom limits (pixels-per-iso-unit multiplier).
#define ZOOM_MIN 0.5f
#define ZOOM_MAX 5.0f

// Camera pan speed from held keys, in screen pixels per second.
#define CAMERA_PAN_SPEED 300.0f

// SIM_TICKS_PER_SEC / SIM_TICK_DT live in game.h so the renderer can read them.

// Center the 2D iso camera on grid cell (cx, cy) at a default zoom.
static void setup_camera(Mach_Camera2D *c, f32 cx, f32 cy) {
    c->pan.x = (cx - cy) * (MACH_ISO_TILE_W * 0.5f);
    c->pan.y = (cx + cy) * (MACH_ISO_TILE_H * 0.5f);
    c->zoom = 2.0f;
}

// The window and engine policy the game wants.
Mach_Config game_config(void) {
    return (Mach_Config){
        .title = "mach",
        .width = 1280,
        .height = 720,
        .clear_color = MACH_COLOR_BG_MAIN, // modus-vivendi: true black beyond the grid
        .escape_quits = MACH_TRUE,         // dev convenience, for now
        .target_fps = 144,
    };
}

// Initialize game state with a world and a short starter chain so there's a value
// loop running on first launch: dropper -> conveyor -> upgrader -> conveyor ->
// collector, all facing east.
void game_init(Game_State *g, Mach *m) {
    g->arena = (Mach_Arena){0};
    g->world = world_create(&g->arena);
    mach_clay_ui_init(&g->clay, &m->r2d);
    sprites_load(&g->sprites, &m->r2d);
    effects_clear(&g->effects);
    g->selected_tool = TOOL_NONE;
    g->place_dir = DIR_E;
    g->hover_grid_x = 0;
    g->hover_grid_y = 0;
    g->hover_valid = MACH_FALSE;
    g->hover_can_place = MACH_FALSE;
    g->sim_accumulator = 0.0f;
    g->anim_time = 0.0f;
    g->paused = MACH_FALSE;
    g->show_debug = MACH_FALSE;

    setup_camera(&g->camera, 7.0f, 5.0f);

    if (g->world) {
        world_spawn_dropper(g->world, 5, 5, DIR_E);
        world_spawn_conveyor(g->world, 6, 5, DIR_E);
        world_spawn_upgrader(g->world, 7, 5, DIR_E);
        world_spawn_conveyor(g->world, 8, 5, DIR_E);
        world_spawn_collector(g->world, 9, 5);
    }

    MACH_LOG_INFO("game initialized (value-loop sim)");
}

// Update the hovered grid cell by inverse-projecting the mouse onto the ground
// plane. Grid cell (x,y) is the tile centered at iso coordinate (x, y).
static void update_hover(Game_State *g, f32 screen_w, f32 screen_h, f32 mouse_x, f32 mouse_y) {
    Mach_Vec2 grid = mach_screen_to_iso(&g->camera, screen_w, screen_h, mouse_x, mouse_y);
    g->hover_grid_x = (i32)floorf(grid.x + 0.5f);
    g->hover_grid_y = (i32)floorf(grid.y + 0.5f);
    g->hover_valid = (g->hover_grid_x >= 0 && g->hover_grid_x < WORLD_GRID_SIZE &&
                      g->hover_grid_y >= 0 && g->hover_grid_y < WORLD_GRID_SIZE);
    g->hover_can_place =
        g->hover_valid && world_can_place_at(g->world, g->hover_grid_x, g->hover_grid_y);
}

// Effect durations, in real seconds (not sim ticks): how long each transient runs.
#define BANK_EFFECT_SECS 0.6f
#define FALL_EFFECT_SECS 0.45f

// Turn the sim events from this frame's ticks into real-time effects, then clear the
// queue. This is the one-way seam: the sim says what happened, the renderer owns how
// (and for how long) it looks. Adding a new bit of juice later is a new event type
// and a case here, never a change to world.c.
static void game_sync_effects(Game_State *g) {
    World *w = g->world;
    for (i32 i = 0; i < w->event_count; i++) {
        const World_Event *ev = &w->events[i];
        Effect *fx = effects_add(&g->effects);
        if (!fx)
            break; // pool full this frame; the rest are dropped sparks
        fx->from_x = (f32)ev->from_x;
        fx->from_y = (f32)ev->from_y;
        fx->to_x = (f32)ev->to_x;
        fx->to_y = (f32)ev->to_y;
        fx->value = ev->value;
        switch (ev->type) {
        case WORLD_EVENT_BANKED:
            fx->type = EFFECT_BANK;
            fx->lifetime = BANK_EFFECT_SECS;
            break;
        case WORLD_EVENT_FELL:
            fx->type = EFFECT_FALL;
            fx->lifetime = FALL_EFFECT_SECS;
            break;
        }
    }
    w->event_count = 0;
}

// Advance the simulation. Rendering runs every frame, but the world steps at a
// fixed rate: we bank real elapsed time and run world_tick in fixed slices, so a
// miner produces the same ore per second regardless of framerate. dt is already
// clamped engine-side, so the accumulator can't run away after a long stall.
void game_tick(Game_State *g, f32 dt) {
    if (!g || !g->world)
        return;
    if (g->paused)
        return; // frozen: no sim ticks, no animation advance

    g->anim_time += dt;
    g->sim_accumulator += dt;
    while (g->sim_accumulator >= SIM_TICK_DT) {
        world_tick(g->world);
        g->sim_accumulator -= SIM_TICK_DT;
    }

    // Drain the events those ticks emitted into real-time effects, then age them.
    // Both run on real dt and freeze with the sim on pause (this function returns
    // early above), so a paused world holds its effects mid-flight.
    game_sync_effects(g);
    effects_update(&g->effects, dt);
}

// One whole game frame: consume input, advance the sim, draw the world and HUD.
// Runs between mach_frame_begin and mach_frame_end.
void game_frame(Game_State *g, Mach *m) {
    // Input works in the renderer's current pixel space, which may differ from
    // the requested size (fullscreen, or a resized window).
    game_process_input(g, &m->input, (f32)m->r2d.width, (f32)m->r2d.height, m->dt);
    game_tick(g, m->dt);
    game_render_draw(&m->r2d, g, &m->frame_arena);
    game_render_hud(g, m);
}

// Clean up game state and free resources.
void game_shutdown(Game_State *g) {
    if (!g)
        return;
    sprites_unload(&g->sprites);
    mach_clay_ui_shutdown(&g->clay);
    if (g->world) {
        MACH_LOG_INFO("world torn down (%d entities, tick %d)", g->world->entity_count,
                      g->world->tick);
        g->world = NULL;
    }
    mach_arena_free(&g->arena);
    MACH_LOG_INFO("game shut down");
}

// Place (or delete) with the selected tool at the hovered cell.
static void place_at_hover(Game_State *g) {
    if (!g->hover_valid)
        return;

    i32 hx = g->hover_grid_x, hy = g->hover_grid_y;
    switch (g->selected_tool) {
    case TOOL_DROPPER:
        world_spawn_dropper(g->world, hx, hy, g->place_dir);
        break;
    case TOOL_CONVEYOR:
        world_spawn_conveyor(g->world, hx, hy, g->place_dir);
        break;
    case TOOL_UPGRADER:
        world_spawn_upgrader(g->world, hx, hy, g->place_dir);
        break;
    case TOOL_COLLECTOR:
        world_spawn_collector(g->world, hx, hy);
        break;
    case TOOL_DELETE: {
        i32 entity_id = world_get_entity_at(g->world, hx, hy);
        if (entity_id != 0)
            world_despawn(g->world, entity_id);
        break;
    }
    default:
        break;
    }
}

// Toggle the given tool: selecting the active tool again clears it.
static void toggle_tool(Game_State *g, Tool tool) {
    g->selected_tool = (g->selected_tool == (i32)tool) ? TOOL_NONE : (i32)tool;
    MACH_LOG_DEBUG("selected tool: %d", g->selected_tool);
}

// Pan the camera. (dx, dy) is screen-space motion in pixels; divide by zoom to
// move the iso-space focus point by the same on-screen amount at any zoom.
static void camera_pan(Game_State *g, f32 dx, f32 dy) {
    g->camera.pan.x += dx / g->camera.zoom;
    g->camera.pan.y += dy / g->camera.zoom;
}

// Zoom the camera. Positive delta zooms in (larger tiles).
static void camera_zoom(Game_State *g, f32 zoom_delta) {
    g->camera.zoom *= (1.0f + zoom_delta);
    g->camera.zoom = mach_clamp(g->camera.zoom, ZOOM_MIN, ZOOM_MAX);
}

// All of the game's input, one place: read this frame's snapshot and apply it.
void game_process_input(Game_State *g, const Mach_Input *in, f32 screen_w, f32 screen_h, f32 dt) {
    if (!g || !g->world || !in)
        return;

    if (in->key_pressed[RGFW_key1])
        toggle_tool(g, TOOL_DROPPER);
    if (in->key_pressed[RGFW_key2])
        toggle_tool(g, TOOL_CONVEYOR);
    if (in->key_pressed[RGFW_key3])
        toggle_tool(g, TOOL_UPGRADER);
    if (in->key_pressed[RGFW_key4])
        toggle_tool(g, TOOL_COLLECTOR);
    if (in->key_pressed[RGFW_key5])
        toggle_tool(g, TOOL_DELETE);
    if (in->key_pressed[RGFW_keySpace]) {
        g->paused = !g->paused;
        MACH_LOG_DEBUG("simulation %s", g->paused ? "paused" : "resumed");
    }
    if (in->key_pressed[RGFW_keyBacktick])
        g->show_debug = !g->show_debug;

    // Camera: continuous pan from held keys, zoom from the wheel.
    f32 px = 0.0f, py = 0.0f;
    if (in->key_down[RGFW_keyLeft] || in->key_down[RGFW_keyA])
        px -= CAMERA_PAN_SPEED * dt;
    if (in->key_down[RGFW_keyRight] || in->key_down[RGFW_keyD])
        px += CAMERA_PAN_SPEED * dt;
    if (in->key_down[RGFW_keyUp] || in->key_down[RGFW_keyW])
        py -= CAMERA_PAN_SPEED * dt;
    if (in->key_down[RGFW_keyDown] || in->key_down[RGFW_keyS])
        py += CAMERA_PAN_SPEED * dt;
    if (px != 0.0f || py != 0.0f)
        camera_pan(g, px, py);
    if (in->wheel != 0.0f)
        camera_zoom(g, in->wheel * 0.1f);

    // Hover follows the mouse every frame, after any camera motion, so panning or
    // zooming under a still cursor keeps the highlighted cell accurate.
    update_hover(g, screen_w, screen_h, in->mouse.x, in->mouse.y);

    if (in->key_pressed[RGFW_keyR]) {
        // Rotate the piece under the cursor in place; with no piece there (or a
        // collector, which has no facing), rotate the facing for the next placement.
        i32 id =
            g->hover_valid ? world_get_entity_at(g->world, g->hover_grid_x, g->hover_grid_y) : 0;
        if (id != 0 && world_rotate_entity(g->world, id)) {
            MACH_LOG_DEBUG("rotated entity %d at (%d,%d)", id, g->hover_grid_x, g->hover_grid_y);
        } else {
            g->place_dir = (Direction)((g->place_dir + 1) % DIR_COUNT);
            MACH_LOG_DEBUG("place direction: %d", g->place_dir);
        }
    }

    if (in->mouse_pressed[MACH_MOUSE_LEFT])
        place_at_hover(g);
}

void game_format_value(i64 v, char *buf, usize n) {
    static const char *suffix[] = {"", "K", "M", "B", "T", "Qa", "Qi"};
    if (v < 1000) {
        snprintf(buf, n, "%lld", (long long)v);
        return;
    }
    f64 d = (f64)v;
    i32 t = 0;
    while (d >= 1000.0 && t < 6) {
        d /= 1000.0;
        t++;
    }
    snprintf(buf, n, "%.1f%s", d, suffix[t]);
}
