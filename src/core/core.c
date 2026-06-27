// Core engine implementation. Included into mach.c (not compiled separately).

#include "core.h"
#include "../game/game.h"
#include "../render/render.h"

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

void core_run(Core_Engine *e) {
    Game_State game = {0};
    game_init(&game);

    const i32 TICKS_PER_SECOND = 60;
    const i32 TICK_MS = 1000 / TICKS_PER_SECOND;

    while (e->running) {
        i32 frame_start = SDL_GetTicks();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                e->running = 0;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                    e->running = 0;
                }
                break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN: {
                i32 button = 0;
                if (event.button.button == SDL_BUTTON_LEFT) button = 1;
                else if (event.button.button == SDL_BUTTON_MIDDLE) button = 2;
                else if (event.button.button == SDL_BUTTON_RIGHT) button = 3;

                if (button != 0) {
                    game_handle_input(&game, event.button.x, event.button.y, button);
                }
                break;
            }
            default:
                break;
            }
        }

        // Tick the simulation
        game_tick(&game);

        // Render
        SDL_SetRenderDrawColor(e->ui.renderer, 0x1e, 0x29, 0x3b, 0xff);
        SDL_RenderClear(e->ui.renderer);

        render_world(&e->ui, game.world, game.tile_size, game.view_offset_x, game.view_offset_y);

        SDL_RenderPresent(e->ui.renderer);

        // Frame rate limiting
        i32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < TICK_MS) {
            SDL_Delay(TICK_MS - frame_time);
        }
    }

    game_shutdown(&game);
}

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
