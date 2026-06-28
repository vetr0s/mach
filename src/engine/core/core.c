// Core implementation (included into mach.c).

#include "core.h"
#include "../debug.h"
#include "../../game/game.h"
#include "../render/render.h"
#include <stdio.h>

// Frame timing.
#define TARGET_FRAME_MS 8      // ~120 FPS soft cap
#define MAX_DT          0.1f   // Clamp dt to prevent large simulation jumps
#define CAMERA_PAN_SPEED 300.0f // pixels per second

// Initialize SDL, create window and renderer.
b32 core_init(Core_Engine *e, const char *title, i32 w, i32 h) {
    LOG_INFO("mach v%d.%d.%d starting up",
             MACH_VERSION_MAJOR, MACH_VERSION_MINOR, MACH_VERSION_PATCH);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return MACH_FALSE;
    }

    if (!SDL_CreateWindowAndRenderer(title, w, h, 0, &e->ui.window, &e->ui.renderer)) {
        LOG_ERROR("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        SDL_Quit();
        return MACH_FALSE;
    }

    e->running = 1;
    LOG_INFO("window created (%dx%d), renderer ready", w, h);
    return MACH_TRUE;
}

// Poll and dispatch all pending SDL events for this frame.
static void core_process_events(Core_Engine *e, Game_State *game) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            LOG_INFO("quit requested");
            e->running = 0;
            break;
        case SDL_EVENT_KEY_DOWN:
            if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                LOG_INFO("escape pressed, exiting");
                e->running = 0;
            } else {
                game_handle_key(game, event.key.scancode);
            }
            break;
        case SDL_EVENT_MOUSE_MOTION:
            game_update_hover(game, (i32)event.motion.x, (i32)event.motion.y);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                game_handle_input(game, (i32)event.button.x, (i32)event.button.y, 1);
            }
            break;
        case SDL_EVENT_MOUSE_WHEEL: {
            f32 dir = (event.wheel.direction == SDL_MOUSEWHEEL_NORMAL) ? 1.0f : -1.0f;
            game_camera_zoom(game, event.wheel.y * 0.1f * dir);
            break;
        }
        default:
            break;
        }
    }
}

// Apply continuous (held-key) camera panning for this frame.
static void core_process_camera_input(Game_State *game, f32 dt) {
    const bool *keys = SDL_GetKeyboardState(NULL);

    f32 pan_x = 0.0f, pan_y = 0.0f;
    if (keys[SDL_SCANCODE_LEFT]  || keys[SDL_SCANCODE_A]) pan_x -= CAMERA_PAN_SPEED * dt;
    if (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) pan_x += CAMERA_PAN_SPEED * dt;
    if (keys[SDL_SCANCODE_UP]    || keys[SDL_SCANCODE_W]) pan_y -= CAMERA_PAN_SPEED * dt;
    if (keys[SDL_SCANCODE_DOWN]  || keys[SDL_SCANCODE_S]) pan_y += CAMERA_PAN_SPEED * dt;

    if (pan_x != 0.0f || pan_y != 0.0f) {
        game_camera_pan(game, pan_x, pan_y);
    }
}

// Render the debug HUD overlay (FPS, current tool, key hints).
static void core_render_hud(Core_Engine *e, Game_State *game, Font *font, i32 fps) {
    if (!font) return;

    static const char *tool_names[] = {"None", "Miner", "Storage", "Delete"};
    const char *tool_name =
        (game->selected_tool >= 0 && game->selected_tool < (i32)ARRAY_COUNT(tool_names))
            ? tool_names[game->selected_tool]
            : "?";

    char line[64];
    snprintf(line, sizeof(line), "FPS: %d", fps);
    font_render_text(e->ui.renderer, font, line, 10, 10, 100, 200, 100);

    snprintf(line, sizeof(line), "Tool: %s", tool_name);
    font_render_text(e->ui.renderer, font, line, 10, 20, 100, 200, 100);

    font_render_text(e->ui.renderer, font, "1:Miner 2:Storage 3:Delete", 10, 35, 150, 150, 150);
}

// Main game loop. Runs until e->running is false.
void core_run(Core_Engine *e) {
    Game_State game = {0};
    game_init(&game);

    Font *font = font_create(e->ui.renderer);
    if (font) {
        LOG_INFO("debug font ready");
    } else {
        LOG_ERROR("font_create failed; HUD text disabled");
    }

    i32 frame_count = 0;
    i32 fps = 0;
    u32 fps_timer = SDL_GetTicks();
    u32 last_frame_time = SDL_GetTicks();

    LOG_INFO("entering main loop");
    while (e->running) {
        u32 frame_start = SDL_GetTicks();
        f32 dt = (f32)(frame_start - last_frame_time) / 1000.0f;
        if (dt > MAX_DT) dt = MAX_DT;
        last_frame_time = frame_start;

        core_process_camera_input(&game, dt);
        core_process_events(e, &game);
        game_tick(&game, dt);

        SDL_SetRenderDrawColor(e->ui.renderer, 0x1e, 0x29, 0x3b, 0xff);
        SDL_RenderClear(e->ui.renderer);

        i32 cam_x = (i32)game.camera_x;
        i32 cam_y = (i32)game.camera_y;
        render_grid(e->ui.renderer, game.tile_size, cam_x, cam_y, game.zoom);
        render_world(e->ui.renderer, game.world, game.tile_size, cam_x, cam_y, game.zoom);
        render_hover_preview(e->ui.renderer, game.hover_grid_x, game.hover_grid_y,
                             game.tile_size, cam_x, cam_y, game.zoom,
                             game.selected_tool, game.hover_can_place);

        core_render_hud(e, &game, font, fps);

        SDL_RenderPresent(e->ui.renderer);

        frame_count++;
        if (SDL_GetTicks() - fps_timer >= 1000) {
            fps = frame_count;
            frame_count = 0;
            fps_timer = SDL_GetTicks();
            LOG_DEBUG("fps=%d entities=%d", fps, game.world ? game.world->entity_count : 0);
        }

        u32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < TARGET_FRAME_MS) {
            SDL_Delay(TARGET_FRAME_MS - frame_time);
        }
    }
    LOG_INFO("exited main loop");

    if (font) font_destroy(font);
    game_shutdown(&game);
}

// Clean up SDL resources and close window.
void core_shutdown(Core_Engine *e) {
    if (e->ui.renderer) {
        SDL_DestroyRenderer(e->ui.renderer);
        e->ui.renderer = NULL;
    }
    if (e->ui.window) {
        SDL_DestroyWindow(e->ui.window);
        e->ui.window = NULL;
    }
    SDL_Quit();
    LOG_INFO("shutdown complete");
}
