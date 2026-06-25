// Core engine implementation. Included into mach.c (not compiled separately).

#include "core.h"

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
    while (e->running) {
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
            default:
                break;
            }
        }

        SDL_SetRenderDrawColor(e->ui.renderer, 0x1e, 0x29, 0x3b, 0xff);
        SDL_RenderClear(e->ui.renderer);
        SDL_RenderPresent(e->ui.renderer);
    }
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
