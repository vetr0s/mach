// Engine implementation. Included into mach.c (not compiled separately).
#include "engine.h"

int engine_init(Engine *e, const char *title, int w, int h) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 0;
    }

    if (!SDL_CreateWindowAndRenderer(title, w, h, 0, &e->window, &e->renderer)) {
        SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        return 0;
    }

    e->running = 1;
    return 1;
}

void engine_run(Engine *e) {
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

        SDL_SetRenderDrawColor(e->renderer, 0x1e, 0x29, 0x3b, 0xff);
        SDL_RenderClear(e->renderer);
        SDL_RenderPresent(e->renderer);
    }
}

void engine_shutdown(Engine *e) {
    if (e->renderer) {
        SDL_DestroyRenderer(e->renderer);
        e->renderer = NULL;
    }
    if (e->window) {
        SDL_DestroyWindow(e->window);
        e->window = NULL;
    }
    SDL_Quit();
}
