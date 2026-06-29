#ifndef UI_H
#define UI_H

#include <SDL3/SDL.h>
#include "base/base.h"

// UI layer: window configuration and the window handle.

// How the game wants its window created. The game fills this; the engine creates
// the window from it (see engine_init).
typedef struct {
    const char *title;
    i32  width, height;
    b32  fullscreen;
    b32  resizable;
} Window_Config;

typedef struct {
    SDL_Window *window;
} UI_Context;

#endif
