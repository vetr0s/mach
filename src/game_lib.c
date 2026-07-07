// Hot-reloadable game module (dev builds only).
//
// Compiled to a shared library that the dev host (host.c) dlopen's and reloads
// whenever it changes on disk. It carries its own copy of the engine, which is
// safe because the engine holds no mutable global state: everything lives in the
// Mach / Game_State structs, all owned by the host and passed in by pointer. So
// the host's engine copy and this one operate on the same data.
//
// The reload interface is the four functions game.c exports (game_config,
// game_init, game_frame, game_shutdown). They keep external linkage and default
// visibility, so the host resolves them via dlsym.
//
// The release build does NOT use this file; src/mach.c stays a single static
// monolith. See build.sh and ARCHITECTURE.md.

// Engine (own copy; state is all pointer-passed, see above).
#define MACH_IMPLEMENTATION
#include "mach.h"

// Game (game.c defines the four exported entry points).
#include "game/world/world.c"
#include "game/game.c"
#include "game/render_game.c"
