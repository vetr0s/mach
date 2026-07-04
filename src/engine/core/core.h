// Core engine lifecycle and the per-frame steps of the loop.
//
// The game owns the loop (in main()) and calls these steps directly. The engine
// owns window lifecycle — engine_frame_begin drains the event queue, consuming
// quit/Escape/resize and folding everything else into the Input snapshot — and
// keeps the frame timing and soft frame cap.

#ifndef CORE_H
#define CORE_H

#include <SDL3/SDL.h>
#include "../base/base.h"
#include "../input/input.h"
#include "../mem/arena.h"
#include "../render/render2d.h"

// How the game wants the engine set up: the window, plus the policy the engine
// applies each frame. The game fills all of it — the engine has no defaults of
// its own — and passes it to engine_init.
typedef struct {
    const char *title;
    i32  width, height;
    b32  fullscreen;
    b32  resizable;

    Color clear_color;   // frame clear color (see render/color.h for the palette)
    b32   escape_quits;  // Escape closes the window (dev convenience); otherwise
                         // Escape reaches the game through the input snapshot
    i32   target_fps;    // soft frame cap; <= 0 leaves the frame rate uncapped
} Engine_Config;

typedef struct {
    SDL_Window  *window;
    Renderer     r2d;    // 2D renderer (SDL_Renderer + bitmap font)
    Input        input;  // per-frame input snapshot, filled by engine_frame_begin

    // Per-frame scratch: reset at every engine_frame_begin, so anything allocated
    // from it lives exactly one frame (sort buffers, transient strings). The
    // regions are reused, not freed, so steady-state allocation is malloc-free.
    Arena        frame_arena;

    i32          running;

    // Per-frame policy, copied out of Engine_Config at init.
    Color clear_color;
    b32   escape_quits;
    u32   frame_cap_ms;  // 0 = uncapped

    // Frame timing, persisted across loop iterations (the game owns the loop).
    u32 frame_start;       // tick at the current frame's start (for the cap)
    u32 last_frame_time;   // tick at the previous frame's start (for dt)
    u32 fps_timer;         // start of the current 1s FPS sampling window
    i32 frame_count;       // frames seen in the current window
    i32 fps;               // last completed window's frame count
} Engine;

// Lifecycle. The game supplies the configuration.
b32  engine_init(Engine *e, Engine_Config cfg);
void engine_shutdown(Engine *e);

// True until a quit is requested (window close or Escape).
b32  engine_running(Engine *e);

// Per-frame steps, called in order by the game's loop:
//   f32 dt = engine_frame_begin(e);   // drain events into e->input, return delta time
//   ... game update (dt, reads e->input) ...
//   if (engine_render_begin(e)) {     // clear the frame (false => skip)
//       ... game render ...
//       engine_render_end(e);         // present
//   }
//   engine_frame_end(e);              // FPS bookkeeping + frame cap
f32  engine_frame_begin(Engine *e);
b32  engine_render_begin(Engine *e);
void engine_render_end(Engine *e);
void engine_frame_end(Engine *e);

// Frames per second over the last completed 1-second window, for the game to display.
i32  engine_fps(const Engine *e);

#endif
