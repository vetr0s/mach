// Game application glue (included into mach.c).
//
// Bundles the game's state + render resources and adapts them to the engine's
// Engine_App callback interface. This is the only place that knows about both
// the engine interface and the game internals.

#include "app.h"
#include "game.h"
#include "render_game.h"
#include <stdio.h>

#define CAMERA_PAN_SPEED 300.0f  // pixels per second

typedef struct {
    Game_State  game;
    Game_Render render;
} App;

static App g_app;

static void app_init(void *s, Gpu_Renderer *gpu) {
    App *a = (App *)s;
    game_init(&a->game);
    game_render_init(&a->render, gpu->device);
}

static void app_handle_event(void *s, const SDL_Event *e) {
    App *a = (App *)s;
    switch (e->type) {
    case SDL_EVENT_MOUSE_MOTION:
        game_update_hover(&a->game, (i32)e->motion.x, (i32)e->motion.y);
        break;
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (e->button.button == SDL_BUTTON_LEFT) {
            game_handle_input(&a->game, (i32)e->button.x, (i32)e->button.y, 1);
        }
        break;
    case SDL_EVENT_MOUSE_WHEEL: {
        f32 dir = (e->wheel.direction == SDL_MOUSEWHEEL_NORMAL) ? 1.0f : -1.0f;
        game_camera_zoom(&a->game, e->wheel.y * 0.1f * dir);
        break;
    }
    case SDL_EVENT_KEY_DOWN:
        game_handle_key(&a->game, e->key.scancode);
        break;
    default:
        break;
    }
}

static void app_update(void *s, f32 dt) {
    App *a = (App *)s;

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

static void app_render(void *s, Gpu_Renderer *gpu) {
    App *a = (App *)s;
    game_render_draw(gpu, &a->render, &a->game);

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
    gpu_draw_text(gpu, 10.0f, 30.0f, 2.0f, line, green);
    gpu_draw_text(gpu, 10.0f, 50.0f, 2.0f, "1:Miner 2:Storage 3:Delete", grey);
}

static void app_shutdown(void *s, Gpu_Renderer *gpu) {
    App *a = (App *)s;
    game_render_shutdown(&a->render, gpu->device);
    game_shutdown(&a->game);
}

Engine_App game_app(void) {
    Engine_App app = {0};
    app.state = &g_app;
    app.init = app_init;
    app.handle_event = app_handle_event;
    app.update = app_update;
    app.render = app_render;
    app.shutdown = app_shutdown;
    return app;
}
