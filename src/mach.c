// Unity build root. Includes all engine code and defines main().

#include "base/base.h"
#include "os/os.h"
#include "ui/ui.h"
#include "core/core.c"
#include "game/game.c"
#include "debug/debug.h"

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    Core_Engine engine = {0};
    if (!core_init(&engine, "mach", 1280, 720)) {
        return 1;
    }

    Game_State game = {0};
    game_init(&game);

    core_run(&engine);

    game_shutdown(&game);
    core_shutdown(&engine);

    return 0;
}
