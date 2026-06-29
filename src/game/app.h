// Game application: its state plus render resources, and the entry points the
// game's own loop calls each frame.
//
// (npt): No engine-side callback vtable. The game owns the loop in main() and
// invokes these directly. This header is the bridge layer, so it's the one place
// allowed to know both the engine API and the game internals.

#ifndef GAME_APP_H
#define GAME_APP_H

#include <SDL3/SDL.h>
#include "../engine/core/core.h"
#include "game.h"
#include "render_game.h"

typedef struct {
    Game_State  game;
    Game_Render render;
} App;

void app_init(App *a, Engine *e);
void app_handle_event(App *a, const SDL_Event *ev);
void app_update(App *a, f32 dt);
void app_render(App *a, Engine *e);
void app_shutdown(App *a, Engine *e);

#endif
