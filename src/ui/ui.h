#ifndef UI_H
#define UI_H

#include <SDL3/SDL.h>

// UI layer: window management, event handling, rendering.

typedef struct {
    SDL_Window   *window;
    SDL_Renderer *renderer;
} UI_Context;

#endif
