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
        .resizable = MACH_FALSE,
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
    game_render_draw(r, &a->game);

    // Game HUD, drawn below the engine's FPS line.
    static const char *tool_names[] = {"None", "Miner", "Storage", "Delete"};
    const char *tool =
        (a->game.selected_tool >= 0 && a->game.selected_tool < (i32)ARRAY_COUNT(tool_names))
            ? tool_names[a->game.selected_tool]
            : "?";

    const Vec4 green = {0.45f, 0.85f, 0.45f, 1.0f};
    const Vec4 grey  = {0.70f, 0.70f, 0.75f, 1.0f};
    char line[64];
    snprintf(line, sizeof(line), "Tool: %s", tool);
    r2d_text(r, 10.0f, 30.0f, 2.0f, line, green);
    r2d_text(r, 10.0f, 50.0f, 2.0f, "1:Miner 2:Storage 3:Delete", grey);
}

void app_shutdown(App *a, Engine *e) {
    (void)e;
    game_shutdown(&a->game);
}
