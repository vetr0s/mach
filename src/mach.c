// Unity build root. Includes all engine code and defines main().

#include "engine.c"

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    Engine e = {0};
    if (!engine_init(&e, "mach", 1280, 720)) {
        return 1;
    }
    engine_run(&e);
    engine_shutdown(&e);

    return 0;
}
