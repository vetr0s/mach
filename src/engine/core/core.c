// Core implementation (included into mach.c).
//
// The core owns the window, the GPU renderer, and the frame loop. It drives the
// application through the Engine_App callback interface and never depends on any
// game type.

#include "core.h"
#include "../debug.h"
#include <stdio.h>

// Frame timing.
#define TARGET_FRAME_MS 8     // ~120 FPS soft cap
#define MAX_DT          0.1f  // Clamp dt to prevent large simulation jumps

// Background clear color (dark slate), normalized.
#define CLEAR_R (0x1e / 255.0f)
#define CLEAR_G (0x29 / 255.0f)
#define CLEAR_B (0x3b / 255.0f)

// Initialize SDL, create window, and bring up the GPU renderer.
b32 core_init(Core_Engine *e, const char *title, i32 w, i32 h) {
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

    if (!gpu_init(&e->gpu, e->ui.window)) {
        SDL_DestroyWindow(e->ui.window);
        e->ui.window = NULL;
        SDL_Quit();
        return MACH_FALSE;
    }

    e->running = 1;
    return MACH_TRUE;
}

// Main game loop. Runs until e->running is false.
void core_run(Core_Engine *e, Engine_App *app) {
    app->init(app->state, &e->gpu);

    i32 frame_count = 0;
    i32 fps = 0;
    u32 fps_timer = SDL_GetTicks();
    u32 last_frame_time = SDL_GetTicks();

    LOG_INFO("entering main loop");
    while (e->running) {
        u32 frame_start = SDL_GetTicks();
        f32 dt = (f32)(frame_start - last_frame_time) / 1000.0f;
        if (dt > MAX_DT) dt = MAX_DT;
        last_frame_time = frame_start;

        // Engine owns window lifecycle (quit / escape); the rest goes to the app.
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                LOG_INFO("quit requested");
                e->running = 0;
            } else if (event.type == SDL_EVENT_KEY_DOWN &&
                       event.key.scancode == SDL_SCANCODE_ESCAPE) {
                LOG_INFO("escape pressed, exiting");
                e->running = 0;
            } else {
                app->handle_event(app->state, &event);
            }
        }

        app->update(app->state, dt);

        if (gpu_begin_frame(&e->gpu, CLEAR_R, CLEAR_G, CLEAR_B)) {
            app->render(app->state, &e->gpu);

            // Engine debug overlay: FPS (top-left). The app draws its own HUD below.
            char line[32];
            snprintf(line, sizeof(line), "FPS: %d", fps);
            gpu_draw_text(&e->gpu, 10.0f, 10.0f, 2.0f, line, (Vec4){0.45f, 0.85f, 0.45f, 1.0f});

            gpu_end_frame(&e->gpu);
        }

        frame_count++;
        if (SDL_GetTicks() - fps_timer >= 1000) {
            fps = frame_count;
            frame_count = 0;
            fps_timer = SDL_GetTicks();
        }

        u32 frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < TARGET_FRAME_MS) {
            SDL_Delay(TARGET_FRAME_MS - frame_time);
        }
    }
    LOG_INFO("exited main loop");

    app->shutdown(app->state, &e->gpu);
}

// Clean up GPU resources and close window.
void core_shutdown(Core_Engine *e) {
    gpu_shutdown(&e->gpu);
    if (e->ui.window) {
        SDL_DestroyWindow(e->ui.window);
        e->ui.window = NULL;
    }
    SDL_Quit();
    LOG_INFO("shutdown complete");
}
