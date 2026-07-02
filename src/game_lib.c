// Hot-reloadable game module (dev builds only).
//
// Compiled to a shared library that the dev host (host.c) dlopen's and reloads
// whenever it changes on disk. It carries its own copy of the engine, which is
// safe because the engine holds no mutable global state: everything lives in the
// Engine / Renderer / Arena / App structs, all owned by the host and passed in by
// pointer. So the host's engine copy and this one operate on the same data.
//
// The reload interface is the six functions app.c exports (game_window_config,
// app_init, app_handle_event, app_update, app_render, app_shutdown). They keep
// external linkage and default visibility, so the host resolves them via dlsym.
//
// The release build does NOT use this file; src/mach.c stays a single static
// monolith. See build.sh and ARCHITECTURE.md.

// Engine (own copy; state is all pointer-passed, see above).
#include "engine/base/base.h"
#include "engine/ui.h"
#include "engine/debug.h"
#include "engine/mem/arena.c"
#include "engine/math/math.c"
#include "engine/render/image.c"
#include "engine/render/font.c"
#include "engine/render/render2d.c"
#include "engine/ui/clay_ui.c"
#include "engine/core/core.c"

// Game (app.c defines the six exported entry points).
#include "game/world/world.c"
#include "game/game.c"
#include "game/render_game.c"
#include "game/app.c"
