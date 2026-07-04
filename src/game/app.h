// Game application: its state and the entry points the loop calls each frame.
//
// This is the bridge layer — the one place that knows both the engine API and
// the game internals. The game owns the loop in main() and calls these directly.

#ifndef GAME_APP_H
#define GAME_APP_H

#include <SDL3/SDL.h>
#include "../engine/core/core.h"
#include "../engine/ui/clay_ui.h"
#include "game.h"
#include "render_game.h"

typedef struct {
    Game_State game;
    ClayUI     clay;   // UI layout/draw; lives in host memory so it survives hot reload
} App;

// The window and engine policy the game wants (clear color, Escape behavior,
// frame cap).
Engine_Config game_engine_config(void);

void app_init(App *a, Engine *e);
void app_update(App *a, Engine *e, f32 dt);
void app_render(App *a, Engine *e);
void app_shutdown(App *a, Engine *e);

#endif
