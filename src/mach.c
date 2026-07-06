// Unity build root. Includes everything and defines main().
//
// The whole game compiles as one translation unit: the engine comes in as
// mach.h (this file defines its implementation), then this file #includes the
// game .c sources directly, so the compiler sees everything at once. The game
// owns the loop in main() and drives the engine through its public API. See
// ARCHITECTURE.md for the design and TODO.md for the roadmap.

// Engine
#define MACH_IMPLEMENTATION
#include "mach.h"

// Game
#include "game/world/world.c"
#include "game/game.c"
#include "game/render_game.c"
#include "game/app.c"

// Application entry point. The game owns the frame loop and calls into the engine
// through its public API; the engine keeps window lifecycle and frame timing.
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    Engine engine = {0};
    if (!engine_init(&engine, game_engine_config())) {
        return 1;
    }

    App app = {0};
    app_init(&app, &engine);

    LOG_INFO("entering main loop");
    while (engine_running(&engine)) {
        f32 dt = engine_frame_begin(&engine);

        app_update(&app, &engine, dt);

        if (engine_render_begin(&engine)) {
            app_render(&app, &engine);
            engine_render_end(&engine);
        }

        engine_frame_end(&engine);
    }
    LOG_INFO("exited main loop");

    app_shutdown(&app, &engine);
    engine_shutdown(&engine);
    return 0;
}
