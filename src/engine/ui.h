#ifndef UI_H
#define UI_H

#include <SDL3/SDL.h>
#include "base/base.h"

// UI layer: window management and rendering context.

// Window dimensions. Single source of truth for screen size; the window is not
// resizable yet, so these double as compile-time constants for projection math.
#define SCREEN_WIDTH  1280
#define SCREEN_HEIGHT 720

// Screen-space center, where the world origin is anchored during projection.
#define SCREEN_CENTER_X (SCREEN_WIDTH / 2)
#define SCREEN_CENTER_Y (SCREEN_HEIGHT / 2)

typedef struct {
    SDL_Window *window;
} UI_Context;

#endif
