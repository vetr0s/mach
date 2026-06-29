// Unity build root. Includes every translation unit and defines main().
//
// The whole engine compiles as one translation unit: this file #includes all the
// .c sources directly, so the compiler sees everything at once. The game owns the
// loop in main() and drives the engine through its public API. See ARCHITECTURE.md
// for the design and TODO.md for the roadmap.

// Engine
#include "engine/base/base.h"
#include "engine/os.h"
#include "engine/ui.h"
#include "engine/debug.h"
#include "engine/math/math.c"
#include "engine/render/image.c"
#include "engine/render/mesh.c"
#include "engine/render/camera.c"
#include "engine/render/gpu.c"
#include "engine/render/font.c"
#include "engine/render/draw.c"
#include "engine/core/core.c"

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
    if (!engine_init(&engine, "mach", SCREEN_WIDTH, SCREEN_HEIGHT)) {
        return 1;
    }

    App app = {0};
    app_init(&app, &engine);

    LOG_INFO("entering main loop");
    while (engine_running(&engine)) {
        f32 dt = engine_frame_begin(&engine);

        SDL_Event ev;
        while (engine_poll_event(&engine, &ev)) {
            app_handle_event(&app, &ev);
        }

        app_update(&app, dt);

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
