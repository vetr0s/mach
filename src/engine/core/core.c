// Core implementation (included into mach.c).
//
// The engine exposes the frame loop as discrete steps; the game owns the loop in
// main() and calls them. The engine keeps window lifecycle and frame timing.

#include "core.h"
#include "../debug.h"
#include <stdio.h>

// Frame timing.
#define TARGET_FRAME_MS 8     // ~120 FPS soft cap
#define MAX_DT          0.1f  // clamp dt to prevent large simulation jumps

// Background clear color (dark slate), 8-bit.
#define CLEAR_R 0x1e
#define CLEAR_G 0x29
#define CLEAR_B 0x3b

// Initialize SDL, create the window from the game's config, bring up the 2D
// renderer, and start the frame-timing clocks.
b32 engine_init(Engine *e, Window_Config cfg) {
    LOG_INFO("mach v%d.%d.%d starting up",
             MACH_VERSION_MAJOR, MACH_VERSION_MINOR, MACH_VERSION_PATCH);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return MACH_FALSE;
    }

    SDL_WindowFlags flags = 0;
    if (cfg.fullscreen) flags |= SDL_WINDOW_FULLSCREEN;
    if (cfg.resizable)  flags |= SDL_WINDOW_RESIZABLE;
    e->ui.window = SDL_CreateWindow(cfg.title, cfg.width, cfg.height, flags);
    if (!e->ui.window) {
        LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return MACH_FALSE;
    }

    if (!r2d_init(&e->r2d, e->ui.window)) {
        SDL_DestroyWindow(e->ui.window);
        e->ui.window = NULL;
        SDL_Quit();
        return MACH_FALSE;
    }

    u32 now = SDL_GetTicks();
    e->running = 1;
    e->frame_start = now;
    e->last_frame_time = now;
    e->fps_timer = now;
    e->frame_count = 0;
    e->fps = 0;
    return MACH_TRUE;
}

// Clean up renderer resources and close the window.
void engine_shutdown(Engine *e) {
    r2d_shutdown(&e->r2d);
    if (e->ui.window) {
        SDL_DestroyWindow(e->ui.window);
        e->ui.window = NULL;
    }
    SDL_Quit();
    LOG_INFO("shutdown complete");
}

b32 engine_running(Engine *e) {
    return e->running;
}

// Start a frame: compute delta time since the previous frame (clamped).
f32 engine_frame_begin(Engine *e) {
    e->frame_start = SDL_GetTicks();
    f32 dt = (f32)(e->frame_start - e->last_frame_time) / 1000.0f;
    if (dt > MAX_DT) dt = MAX_DT;
    e->last_frame_time = e->frame_start;
    return dt;
}

// Drain the next game-relevant event into `out`. Window lifecycle events (quit,
// Escape, resize) are consumed here instead of reaching the game. Returns false
// when the queue is empty.
b32 engine_poll_event(Engine *e, SDL_Event *out) {
    while (SDL_PollEvent(out)) {
        if (out->type == SDL_EVENT_QUIT) {
            LOG_INFO("quit requested");
            e->running = 0;
        } else if (out->type == SDL_EVENT_KEY_DOWN &&
                   out->key.scancode == SDL_SCANCODE_ESCAPE) {
            LOG_INFO("escape pressed, exiting");
            e->running = 0;
        } else if (out->type == SDL_EVENT_WINDOW_RESIZED) {
            r2d_resized(&e->r2d);
        } else {
            return MACH_TRUE;
        }
    }
    return MACH_FALSE;
}

// Begin the frame: clear to the background color. Always succeeds (SDL_Renderer
// handles a minimized window gracefully), but keeps the bool shape of the loop.
b32 engine_render_begin(Engine *e) {
    r2d_begin(&e->r2d, CLEAR_R, CLEAR_G, CLEAR_B);
    return MACH_TRUE;
}

// Finish the frame: draw the engine's debug overlay (FPS, top-left) on top of
// whatever the game rendered, then present.
void engine_render_end(Engine *e) {
    char line[32];
    snprintf(line, sizeof(line), "FPS: %d", e->fps);
    r2d_text(&e->r2d, 10.0f, 10.0f, 2.0f, line, (Vec4){0.45f, 0.85f, 0.45f, 1.0f});
    r2d_present(&e->r2d);
}

// End-of-frame bookkeeping: update the 1s FPS sample and sleep to honor the soft
// frame cap.
void engine_frame_end(Engine *e) {
    e->frame_count++;
    u32 now = SDL_GetTicks();
    if (now - e->fps_timer >= 1000) {
        e->fps = e->frame_count;
        e->frame_count = 0;
        e->fps_timer = now;
    }

    u32 frame_time = SDL_GetTicks() - e->frame_start;
    if (frame_time < TARGET_FRAME_MS) {
        SDL_Delay(TARGET_FRAME_MS - frame_time);
    }
}
