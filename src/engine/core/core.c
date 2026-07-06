// Core implementation (included into mach.c).
//
// The engine exposes the frame loop as discrete steps; the game owns the loop in
// main() and calls them. The engine keeps window lifecycle and frame timing.
//
// RGFW's implementation compiles here — this file is the single home of the
// windowing layer, the way clay_ui.c owns Clay and image.c owns stb_image.
// Everything else includes engine/rgfw.h for declarations only.

#include "core.h"
#include "../debug.h"
#include <stdio.h>
#include <time.h>

// RGFW is third-party single-header code, so silence the warnings it trips
// under -Wall -Wextra rather than let them bury ours.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wcast-qual"
#pragma clang diagnostic ignored "-Wpedantic"
#pragma clang diagnostic ignored "-Wunused-macros"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#define RGFW_IMPLEMENTATION
#include "../../../third_party/rgfw/RGFW.h"
#pragma clang diagnostic pop

// Clamp dt to prevent large simulation jumps after a stall.
#define MAX_DT 0.1f

u32 engine_ticks_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u32)((u64)ts.tv_sec * 1000u + (u64)ts.tv_nsec / 1000000u);
}

static void engine_sleep_ms(u32 ms) {
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000u);
    ts.tv_nsec = (long)((ms % 1000u) * 1000000L);
    nanosleep(&ts, NULL);
}

// Initialize RGFW, create the window from the game's config with a GL 3.3 core
// context, bring up the 2D renderer, and start the frame-timing clocks.
b32 engine_init(Engine *e, Engine_Config cfg) {
    LOG_INFO("mach v%d.%d.%d starting up",
             MACH_VERSION_MAJOR, MACH_VERSION_MINOR, MACH_VERSION_PATCH);

    if (RGFW_init("mach", RGFW_initOpenGL) < 0) {
        LOG_ERROR("RGFW_init failed");
        return MACH_FALSE;
    }

    // RGFW keeps the hints pointer, so they live in static storage. Written once
    // here at init (host side only); nothing reads or writes them afterward.
    static RGFW_glHints gl_hints;
    gl_hints = *RGFW_getGlobalHints_OpenGL();
    gl_hints.major = 3;
    gl_hints.minor = 3;
    gl_hints.profile = RGFW_glCore;
    RGFW_setGlobalHints_OpenGL(&gl_hints);

    RGFW_windowFlags flags = RGFW_windowCenter | RGFW_windowOpenGL;
    if (cfg.fullscreen) flags |= RGFW_windowFullscreen;
    if (!cfg.resizable) flags |= RGFW_windowNoResize;
    e->window = RGFW_createWindow(cfg.title, 0, 0, cfg.width, cfg.height, flags);
    if (!e->window) {
        LOG_ERROR("RGFW_createWindow failed");
        RGFW_deinit();
        return MACH_FALSE;
    }

    // Our loop has its own frame cap, so disable vsync and let it govern.
    RGFW_window_swapInterval_OpenGL(e->window, 0);

    if (!r2d_init(&e->r2d, e->window)) {
        RGFW_window_close(e->window);
        e->window = NULL;
        RGFW_deinit();
        return MACH_FALSE;
    }

    e->clear_color = cfg.clear_color;
    e->escape_quits = cfg.escape_quits;
    e->frame_cap_ms = cfg.target_fps > 0 ? 1000u / (u32)cfg.target_fps : 0;

    u32 now = engine_ticks_ms();
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
        RGFW_window_close(e->window);
        e->window = NULL;
    }
    RGFW_deinit();
    LOG_INFO("shutdown complete");
}

b32 engine_running(Engine *e) {
    return e->running;
}

// Start a frame: compute delta time since the previous frame (clamped), then
// drain the event queue. Window lifecycle events (quit, Escape, resize) are
// consumed here; everything else folds into e->input for the game to read.
f32 engine_frame_begin(Engine *e) {
    e->frame_start = engine_ticks_ms();
    f32 dt = (f32)(e->frame_start - e->last_frame_time) / 1000.0f;
    if (dt > MAX_DT) dt = MAX_DT;
    e->last_frame_time = e->frame_start;

    arena_reset(&e->frame_arena);
    input_frame_begin(&e->input);
    RGFW_event ev;
    while (RGFW_window_checkEvent(e->window, &ev)) {
        if (ev.type == RGFW_windowClose) {
            LOG_INFO("quit requested");
            e->running = 0;
        } else if (e->escape_quits && ev.type == RGFW_keyPressed &&
                   ev.key.value == RGFW_keyEscape) {
            LOG_INFO("escape pressed, exiting");
            e->running = 0;
        } else if (ev.type == RGFW_windowResized) {
            r2d_resized(&e->r2d);
        } else {
            input_handle_event(&e->input, &ev);
        }
    }
    return dt;
}

// Begin the frame: clear to the game's background color. Always succeeds, but
// keeps the bool shape of the loop.
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
    u32 now = engine_ticks_ms();
    if (now - e->fps_timer >= 1000) {
        e->fps = e->frame_count;
        e->frame_count = 0;
        e->fps_timer = now;
    }

    u32 frame_time = engine_ticks_ms() - e->frame_start;
    if (e->frame_cap_ms && frame_time < e->frame_cap_ms) {
        engine_sleep_ms(e->frame_cap_ms - frame_time);
    }
}
