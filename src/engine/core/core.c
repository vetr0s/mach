// Core implementation (included into mach.c).
//
// (npt): The engine exposes the frame loop as discrete steps; the game owns the
// loop in main() and calls them. The engine keeps window lifecycle (quit/escape)
// and frame timing/cap, but no longer drives the application.

#include "core.h"
#include "../debug.h"
#include <stdio.h>

// Frame timing.
#define TARGET_FRAME_MS 8     // ~120 FPS soft cap
#define MAX_DT          0.1f  // clamp dt to prevent large simulation jumps

// Background clear color (dark slate), normalized.
#define CLEAR_R (0x1e / 255.0f)
#define CLEAR_G (0x29 / 255.0f)
#define CLEAR_B (0x3b / 255.0f)

// Initialize SDL, create the window, bring up the GPU renderer, and start the
// frame-timing clocks.
b32 engine_init(Engine *e, const char *title, i32 w, i32 h) {
    LOG_INFO("mach v%d.%d.%d starting up",
             MACH_VERSION_MAJOR, MACH_VERSION_MINOR, MACH_VERSION_PATCH);

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        LOG_ERROR("SDL_Init failed: %s", SDL_GetError());
        return MACH_FALSE;
    }

    e->ui.window = SDL_CreateWindow(title, w, h, 0);
    if (!e->ui.window) {
        LOG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return MACH_FALSE;
    }

    if (!gpu_device_init(&e->gpu, e->ui.window)) {
        SDL_DestroyWindow(e->ui.window);
        e->ui.window = NULL;
        SDL_Quit();
        return MACH_FALSE;
    }

    if (!draw_init(&e->draw, &e->gpu)) {
        draw_shutdown(&e->draw);
        gpu_device_shutdown(&e->gpu);
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

// Clean up GPU resources and close the window.
void engine_shutdown(Engine *e) {
    draw_shutdown(&e->draw);
    gpu_device_shutdown(&e->gpu);
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

// Drain the next game-relevant event into `out`. Window lifecycle events
// (quit / Escape) are consumed here and clear `running` instead of reaching the
// game. Returns false when the queue is empty.
b32 engine_poll_event(Engine *e, SDL_Event *out) {
    while (SDL_PollEvent(out)) {
        if (out->type == SDL_EVENT_QUIT) {
            LOG_INFO("quit requested");
            e->running = 0;
        } else if (out->type == SDL_EVENT_KEY_DOWN &&
                   out->key.scancode == SDL_SCANCODE_ESCAPE) {
            LOG_INFO("escape pressed, exiting");
            e->running = 0;
        } else {
            return MACH_TRUE;
        }
    }
    return MACH_FALSE;
}

// Begin the GPU frame and open the scene pass (clear color + depth). Returns
// false when there is no swapchain image (e.g. minimized); the game should skip
// rendering and not call engine_render_end.
b32 engine_render_begin(Engine *e) {
    if (!gpu_begin_frame(&e->gpu)) return MACH_FALSE;
    gpu_begin_pass(&e->gpu, &(Gpu_PassDesc){
        .clear = MACH_TRUE, .clear_r = CLEAR_R, .clear_g = CLEAR_G, .clear_b = CLEAR_B,
        .use_depth = MACH_TRUE,
    });
    return MACH_TRUE;
}

// Finish the GPU frame: draw the engine's debug overlay (FPS, top-left) on top
// of whatever the game rendered, end the scene pass, flush the 2D overlay in its
// own pass, then present.
void engine_render_end(Engine *e) {
    char line[32];
    snprintf(line, sizeof(line), "FPS: %d", e->fps);
    draw_text(&e->draw, 10.0f, 10.0f, 2.0f, line, (Vec4){0.45f, 0.85f, 0.45f, 1.0f});

    gpu_end_pass(&e->gpu);          // end the scene pass
    draw_flush_overlay(&e->draw);   // overlay pass (loads the scene, draws HUD)
    gpu_end_frame(&e->gpu);         // submit
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
