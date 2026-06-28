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
#include "engine/render/mesh.c"
#include "engine/render/camera.c"
#include "engine/render/gpu.c"
#include "engine/render/font.c"

// Game
#include "game/world/world.c"
#include "game/game.c"
#include "game/render_game.c"
#include "game/app.c"

// Core (drives the loop via the Engine_App interface; depends on no game type)
#include "engine/core/core.c"

#include "game/app.h"

// Application entry point. The engine owns the loop and drives the game through
// the Engine_App interface; main just wires the two together.
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    Core_Engine engine = {0};
    if (!core_init(&engine, "mach", SCREEN_WIDTH, SCREEN_HEIGHT)) {
        return 1;
    }

    Engine_App app = game_app();
    core_run(&engine, &app);

    core_shutdown(&engine);
    return 0;
}
