// Core implementation (included into mach.c).
//
// The engine exposes the frame loop as discrete steps; the game owns the loop in
// main() and calls them. The engine keeps window lifecycle and frame timing.

#include "core.h"
#include "../debug.h"
#include <stdio.h>

// Clamp dt to prevent large simulation jumps after a stall.
#define MAX_DT 0.1f

// Initialize SDL, create the window from the game's config, bring up the 2D
// renderer, and start the frame-timing clocks.
b32 engine_init(Engine *e, Engine_Config cfg) {
    LOG_INFO("mach v%d.%d.%d starting up",
             MACH_VERSION_MAJOR, MACH_VERSION_MINOR, MACH_VERSION_PATCH);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return MACH_FALSE;
    }

    SDL_WindowFlags flags = 0;
    if (cfg.fullscreen) flags |= SDL_WINDOW_FULLSCREEN;
    if (cfg.resizable)  flags |= SDL_WINDOW_RESIZABLE;
    e->window = SDL_CreateWindow(cfg.title, cfg.width, cfg.height, flags);
    if (!e->window) {
        LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return MACH_FALSE;
    }

    if (!r2d_init(&e->r2d, e->window)) {
        SDL_DestroyWindow(e->window);
        e->window = NULL;
        SDL_Quit();
        return MACH_FALSE;
    }

    e->clear_color = cfg.clear_color;
    e->escape_quits = cfg.escape_quits;
    e->frame_cap_ms = cfg.target_fps > 0 ? 1000u / (u32)cfg.target_fps : 0;

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
    arena_free(&e->frame_arena);
    r2d_shutdown(&e->r2d);
    if (e->window) {
        SDL_DestroyWindow(e->window);
        e->window = NULL;
    }
    SDL_Quit();
    LOG_INFO("shutdown complete");
}

b32 engine_running(Engine *e) {
    return e->running;
}

// Start a frame: compute delta time since the previous frame (clamped), then
// drain the event queue. Window lifecycle events (quit, Escape, resize) are
// consumed here; everything else folds into e->input for the game to read.
f32 engine_frame_begin(Engine *e) {
    e->frame_start = SDL_GetTicks();
    f32 dt = (f32)(e->frame_start - e->last_frame_time) / 1000.0f;
    if (dt > MAX_DT) dt = MAX_DT;
    e->last_frame_time = e->frame_start;

    arena_reset(&e->frame_arena);
    input_frame_begin(&e->input);
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_EVENT_QUIT) {
            LOG_INFO("quit requested");
            e->running = 0;
        } else if (e->escape_quits && ev.type == SDL_EVENT_KEY_DOWN &&
                   ev.key.scancode == SDL_SCANCODE_ESCAPE) {
            LOG_INFO("escape pressed, exiting");
            e->running = 0;
        } else if (ev.type == SDL_EVENT_WINDOW_RESIZED) {
            r2d_resized(&e->r2d);
        } else {
            input_handle_event(&e->input, &ev);
        }
    }
    return dt;
}

// Begin the frame: clear to the game's background color. Always succeeds
// (SDL_Renderer handles a minimized window gracefully), but keeps the bool
// shape of the loop.
b32 engine_render_begin(Engine *e) {
    r2d_begin(&e->r2d, e->clear_color);
    return MACH_TRUE;
}

// Finish the frame: present whatever the game rendered. The engine no longer draws
// its own overlay — FPS is exposed via engine_fps() for the game to display however
// it likes.
void engine_render_end(Engine *e) {
    r2d_present(&e->r2d);
}

// Frames per second over the last completed 1-second sampling window.
i32 engine_fps(const Engine *e) { return e ? e->fps : 0; }

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
    if (e->frame_cap_ms && frame_time < e->frame_cap_ms) {
        SDL_Delay(e->frame_cap_ms - frame_time);
    }
}
