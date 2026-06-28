// Unity build root. Includes every translation unit and defines main().
//
// The whole engine compiles as one translation unit: this file #includes all
// the .c sources directly, so the compiler sees everything at once. See TODO.md
// for the roadmap.

// Engine
#include "engine/base/base.h"
#include "engine/os.h"
#include "engine/ui.h"
#include "engine/debug.h"
#include "engine/math/math.c"
#include "engine/render/image.c"
#include "engine/render/font.c"
#include "engine/render/render.c"
#include "engine/core/core.c"

// Game
#include "game/world/world.c"
#include "game/game.c"

// Application entry point. The engine owns the game loop and game state; main
// just brings the engine up and tears it down.
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    Core_Engine engine = {0};
    if (!core_init(&engine, "mach", SCREEN_WIDTH, SCREEN_HEIGHT)) {
        return 1;
    }

    core_run(&engine);
    core_shutdown(&engine);

    return 0;
}
