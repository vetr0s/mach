// Core implementation (included into mach.c).

#include "core.h"
#include "../../game/game.h"
#include "../render/render.h"
#include <stdio.h>

// Initialize SDL, create window and renderer.
int core_init(Core_Engine *e, const char *title, i32 w, i32 h) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 0;
    }

    if (!SDL_CreateWindowAndRenderer(title, w, h, 0, &e->ui.window, &e->ui.renderer)) {
        SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        return 0;
    }

    e->running = 1;
    return 1;
}

// Main game loop. Runs until core->running is false.
void core_run(Core_Engine *e) {
    Game_State game = {0};
    game_init(&game);

    Font *font = font_create(e->ui.renderer);

    // (npt): Variable timestep with soft cap at 120 FPS to balance performance and CPU usage
    const u32 TARGET_FRAME_MS = 8;  // ~120 FPS soft cap
    const f32 MAX_DT = 0.1f;        // Clamp dt to prevent large jumps

    i32 frame_count = 0;
    i32 fps = 0;
    u32 fps_timer = SDL_GetTicks();
    u32 last_frame_time = SDL_GetTicks();

    // Camera panning state
    f32 pan_speed = 300.0f;  // pixels per second

    while (e->running) {
        u32 frame_start = SDL_GetTicks();
        f32 dt = (f32)(frame_start - last_frame_time) / 1000.0f;
        if (dt > MAX_DT) dt = MAX_DT;
        last_frame_time = frame_start;

        // Update keyboard state for continuous input
        const u8 *key_state = SDL_GetKeyboardState(NULL);

        // Camera panning with arrow keys and WASD
        f32 pan_x = 0.0f, pan_y = 0.0f;
        if (key_state[SDL_SCANCODE_LEFT] || key_state[SDL_SCANCODE_A]) pan_x -= pan_speed * dt;
        if (key_state[SDL_SCANCODE_RIGHT] || key_state[SDL_SCANCODE_D]) pan_x += pan_speed * dt;
        if (key_state[SDL_SCANCODE_UP] || key_state[SDL_SCANCODE_W]) pan_y -= pan_speed * dt;
        if (key_state[SDL_SCANCODE_DOWN] || key_state[SDL_SCANCODE_S]) pan_y += pan_speed * dt;
        if (pan_x != 0.0f || pan_y != 0.0f) {
            game_camera_pan(&game, pan_x, pan_y);
        }

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                e->running = 0;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                    e->running = 0;
                } else {
                    game_handle_key(&game, event.key.scancode);
                }
                break;
            case SDL_EVENT_MOUSE_MOTION:
                game_update_hover(&game, event.motion.x, event.motion.y);
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    game_handle_input(&game, event.button.x, event.button.y, 1);
                }
                break;
            }
            case SDL_EVENT_MOUSE_WHEEL: {
                f32 zoom_delta = (event.wheel.direction == SDL_MOUSEWHEEL_NORMAL) ? event.wheel.y * 0.1f : event.wheel.y * -0.1f;
                game_camera_zoom(&game, zoom_delta);
                break;
            }
            default:
                break;
            }
        }

        game_tick(&game, dt);

        SDL_SetRenderDrawColor(e->ui.renderer, 0x1e, 0x29, 0x3b, 0xff);
        SDL_RenderClear(e->ui.renderer);

        render_grid(e->ui.renderer, game.tile_size, (i32)game.camera_x, (i32)game.camera_y, game.zoom, 1280, 720);
        render_world(&e->ui, game.world, game.tile_size, (i32)game.camera_x, (i32)game.camera_y, game.zoom);
        render_hover_preview(e->ui.renderer, game.hover_grid_x, game.hover_grid_y, game.tile_size,
                            (i32)game.camera_x, (i32)game.camera_y, game.zoom, game.selected_tool, game.hover_can_place);

        // Debug display with font
        if (font) {
            char fps_text[32];
            snprintf(fps_text, sizeof(fps_text), "FPS: %d", fps);
            font_render_text(e->ui.renderer, font, fps_text, 10, 10, 100, 200, 100);

            const char *tool_names[] = {"None", "Miner", "Storage", "Delete"};
            const char *tool_name = (game.selected_tool >= 0 && game.selected_tool <= 3)
                                   ? tool_names[game.selected_tool]
                                   : "?";
            char tool_text[32];
            snprintf(tool_text, sizeof(tool_text), "Tool: %s", tool_name);
            font_render_text(e->ui.renderer, font, tool_text, 10, 20, 100, 200, 100);

            font_render_text(e->ui.renderer, font, "1:Miner 2:Storage 3:Delete", 10, 35, 150, 150, 150);
        }

        SDL_RenderPresent(e->ui.renderer);

        frame_count++;
        u32 elapsed = SDL_GetTicks() - fps_timer;
        if (elapsed >= 1000) {
            fps = frame_count;
            frame_count = 0;
            fps_timer = SDL_GetTicks();
        }

        // Soft frame cap at 120 FPS
        u32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < TARGET_FRAME_MS) {
            SDL_Delay(TARGET_FRAME_MS - frame_time);
        }
    }

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
}
