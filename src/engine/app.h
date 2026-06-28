// Application interface: how the engine drives a game without depending on it.
//
// The engine (core loop) calls through these callbacks and never names a game
// type. The game implements the callbacks and hands the engine an Engine_App
// via main(). `state` is the game's opaque state pointer.

#ifndef APP_H
#define APP_H

#include <SDL3/SDL.h>
#include "base/base.h"
#include "render/gpu.h"

typedef struct {
    void *state;

    void (*init)(void *state, Gpu_Renderer *gpu);
    void (*handle_event)(void *state, const SDL_Event *event);
    void (*update)(void *state, f32 dt);
    void (*render)(void *state, Gpu_Renderer *gpu);
    void (*shutdown)(void *state, Gpu_Renderer *gpu);
} Engine_App;

#endif
