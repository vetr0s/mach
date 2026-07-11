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
#include "game/sprites.c"
#include "game/effects.c"
#include "game/world/world.c"
#include "game/game.c"
#include "game/render_game.c"

// Application entry point. The game owns the frame loop and calls into the engine
// through its public API; the engine keeps window lifecycle and frame timing.
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    Mach m = {0};
    if (!mach_init(&m, game_config())) {
        return 1;
    }

    Game_State game = {0};
    game_init(&game, &m);

    MACH_LOG_INFO("entering main loop");
    while (mach_running(&m)) {
        mach_frame_begin(&m);
        game_frame(&game, &m);
        mach_frame_end(&m);
    }
    MACH_LOG_INFO("exited main loop");

    game_shutdown(&game);
    mach_shutdown(&m);
    return 0;
}
