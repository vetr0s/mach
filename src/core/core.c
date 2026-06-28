// Core implementation (included into mach.c).

#include "core.h"
#include "../game/game.h"
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

    const i32 TICKS_PER_SECOND = 60;
    const i32 TICK_MS = 1000 / TICKS_PER_SECOND;

    i32 frame_count = 0;
    i32 fps = 0;
    u32 fps_timer = SDL_GetTicks();

    while (e->running) {
        u32 frame_start = SDL_GetTicks();

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
            default:
                break;
            }
        }

        game_tick(&game);

        SDL_SetRenderDrawColor(e->ui.renderer, 0x1e, 0x29, 0x3b, 0xff);
        SDL_RenderClear(e->ui.renderer);

        render_world(&e->ui, game.world, game.tile_size, game.view_offset_x, game.view_offset_y);
        render_hover_preview(e->ui.renderer, game.hover_grid_x, game.hover_grid_y, game.tile_size,
                            game.view_offset_x, game.view_offset_y, game.selected_tool);

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

        u32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < TICK_MS) {
            SDL_Delay(TICK_MS - frame_time);
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
