#include "mach/engine.h"

#include <SDL3/SDL.h>

namespace mach {

bool Engine::init(const char *title, int width, int height) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    if (!SDL_CreateWindowAndRenderer(title, width, height, 0, &window_,
                                     &renderer_)) {
        SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        return false;
    }

    return true;
}

int Engine::run() {
    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_EVENT_QUIT:
                running = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                    running = false;
                }
                break;
            default:
                break;
            }
        }

        SDL_SetRenderDrawColor(renderer_, 0x1e, 0x29, 0x3b, 0xff);
        SDL_RenderClear(renderer_);
        SDL_RenderPresent(renderer_);
    }

    return 0;
}

void Engine::shutdown() {
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

Engine::~Engine() { shutdown(); }

} // namespace mach
