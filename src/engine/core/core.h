// Core engine lifecycle and the building blocks of the frame loop.
//
// (npt): The engine no longer owns the loop. The game drives it from main() and
// calls these per-frame steps directly (raylib-style). The engine still owns
// window lifecycle: engine_poll_event intercepts quit/escape and clears
// `running`, and the engine keeps frame timing + the soft frame cap.

#ifndef CORE_H
#define CORE_H

#include <SDL3/SDL.h>
#include "../base/base.h"
#include "../os.h"
#include "../ui.h"
#include "../render/render2d.h"

typedef struct {
    UI_Context   ui;
    Renderer     r2d;    // 2D renderer (SDL_Renderer + bitmap font)
    i32          running;

    // (npt): Frame timing. These were locals in the old engine-owned loop; now
    // that the game owns the loop, they persist on the engine across iterations.
    u32 frame_start;       // tick at the current frame's start (for the cap)
    u32 last_frame_time;   // tick at the previous frame's start (for dt)
    u32 fps_timer;         // start of the current 1s FPS sampling window
    i32 frame_count;       // frames seen in the current window
    i32 fps;               // last completed window's frame count
} Engine;

// Lifecycle.
b32  engine_init(Engine *e, const char *title, i32 w, i32 h);
void engine_shutdown(Engine *e);

// True until a quit is requested (window close or Escape).
b32  engine_running(Engine *e);

// Per-frame steps, called in order by the game's loop:
//   f32 dt = engine_frame_begin(e);          // compute + return delta time
//   while (engine_poll_event(e, &ev)) {...}  // drain game-relevant events
//   ... game update (dt) ...
//   if (engine_render_begin(e)) {            // clear the frame (false => skip)
//       ... game render ...
//       engine_render_end(e);                // engine overlay + present
//   }
//   engine_frame_end(e);                     // FPS bookkeeping + frame cap
f32  engine_frame_begin(Engine *e);
b32  engine_poll_event(Engine *e, SDL_Event *out);
b32  engine_render_begin(Engine *e);
void engine_render_end(Engine *e);
void engine_frame_end(Engine *e);

#endif
