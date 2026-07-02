// Game application glue (included into mach.c): wires the game state into the
// loop's per-frame entry points.

#include "app.h"
#include "game.h"
#include "render_game.h"
#include <stdio.h>

#define CAMERA_PAN_SPEED 300.0f  // pixels per second

// The game defines its own window here.
Window_Config game_window_config(void) {
    return (Window_Config){
        .title = "mach",
        .width = 1280,
        .height = 720,
        .fullscreen = MACH_FALSE,
        .resizable = MACH_TRUE,
    };
}

void app_init(App *a, Engine *e) {
    (void)e;
    game_init(&a->game);
}

void app_handle_event(App *a, Engine *e, const SDL_Event *ev) {
    // Hover and placement work in the renderer's current pixel space, which may
    // differ from the requested size (fullscreen, or a resized window).
    f32 sw = (f32)e->r2d.width, sh = (f32)e->r2d.height;
    switch (ev->type) {
    case SDL_EVENT_MOUSE_MOTION:
        game_update_hover(&a->game, sw, sh, (i32)ev->motion.x, (i32)ev->motion.y);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (ev->button.button == SDL_BUTTON_LEFT) {
            game_handle_input(&a->game, sw, sh, (i32)ev->button.x, (i32)ev->button.y, 1);
        }
        break;
    case SDL_EVENT_MOUSE_WHEEL: {
        f32 dir = (ev->wheel.direction == SDL_MOUSEWHEEL_NORMAL) ? 1.0f : -1.0f;
        game_camera_zoom(&a->game, ev->wheel.y * 0.1f * dir);
        break;
    }
    case SDL_EVENT_KEY_DOWN:
        game_handle_key(&a->game, ev->key.scancode);
        break;
    default:
        break;
    }
}

void app_update(App *a, f32 dt) {
    // Continuous camera panning from held keys.
    const bool *keys = SDL_GetKeyboardState(NULL);
    f32 px = 0.0f, py = 0.0f;
    if (keys[SDL_SCANCODE_LEFT]  || keys[SDL_SCANCODE_A]) px -= CAMERA_PAN_SPEED * dt;
    if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) px += CAMERA_PAN_SPEED * dt;
    if (keys[SDL_SCANCODE_UP]    || keys[SDL_SCANCODE_W]) py -= CAMERA_PAN_SPEED * dt;
    if (keys[SDL_SCANCODE_DOWN]  || keys[SDL_SCANCODE_S]) py += CAMERA_PAN_SPEED * dt;
    if (px != 0.0f || py != 0.0f) game_camera_pan(&a->game, px, py);

    game_tick(&a->game, dt);
}

void app_render(App *a, Engine *e) {
    Renderer *r = &e->r2d;
    const Game_State *g = &a->game;
    game_render_draw(r, g);

    const Vec4 gold  = {0.90f, 0.78f, 0.35f, 1.0f};
    const Vec4 amber = {1.00f, 0.65f, 0.20f, 1.0f};
    const Vec4 grey  = {0.65f, 0.66f, 0.72f, 1.0f};
    char line[96];

    // Always on: money is the one number worth a permanent, prominent slot. A pause
    // marker sits under it since that's game state, not the toggled debug info.
    i64 money = g->world ? g->world->money : 0;
    snprintf(line, sizeof(line), "$%lld", (long long)money);
    r2d_text(r, 12.0f, 12.0f, 2.0f, line, gold);
    if (g->paused) r2d_text(r, 12.0f, 34.0f, 1.0f, "PAUSED", amber);

    if (!g->show_debug) return;

    // F3 overlay: everything else, small so it stays out of the way. Minecraft-style —
    // there when you want it, gone otherwise.
    static const char *tool_names[] = {"None", "Dropper", "Conveyor", "Upgrader",
                                       "Collector", "Delete"};
    static const char *dir_names[]  = {"N", "E", "S", "W"};
    const char *tool = (g->selected_tool >= 0 && g->selected_tool < (i32)ARRAY_COUNT(tool_names))
                           ? tool_names[g->selected_tool] : "?";
    const char *facing = (g->place_dir >= 0 && g->place_dir < (i32)ARRAY_COUNT(dir_names))
                             ? dir_names[g->place_dir] : "?";
    i32 tick  = g->world ? g->world->tick : 0;
    i32 ents  = g->world ? g->world->entity_count : 0;
    i32 items = g->world ? g->world->item_count : 0;

    f32 y = 52.0f;
    const f32 dy = 12.0f;
    snprintf(line, sizeof(line), "fps %d", engine_fps(e));
    r2d_text(r, 12.0f, y, 1.0f, line, grey); y += dy;
    snprintf(line, sizeof(line), "tool %s   facing %s", tool, facing);
    r2d_text(r, 12.0f, y, 1.0f, line, grey); y += dy;
    snprintf(line, sizeof(line), "tick %d   entities %d   items %d", tick, ents, items);
    r2d_text(r, 12.0f, y, 1.0f, line, grey); y += dy;
    if (g->hover_valid)
        snprintf(line, sizeof(line), "hover %d,%d%s", g->hover_grid_x, g->hover_grid_y,
                 g->hover_can_place ? "" : " (blocked)");
    else
        snprintf(line, sizeof(line), "hover --");
    r2d_text(r, 12.0f, y, 1.0f, line, grey); y += dy;
    snprintf(line, sizeof(line), "cam %.0f,%.0f   zoom %.2f", g->camera.pan.x, g->camera.pan.y, g->camera.zoom);
    r2d_text(r, 12.0f, y, 1.0f, line, grey); y += dy;
    r2d_text(r, 12.0f, y, 1.0f, "1drop 2belt 3upgr 4collect 5del   R rotate   Space pause   F3 info", grey);
}

void app_shutdown(App *a, Engine *e) {
    (void)e;
    game_shutdown(&a->game);
}
