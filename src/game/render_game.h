// Game-side 3D scene rendering: draws the factory world as lit geometry.
//
// This lives in the game layer (not the engine) so the engine renderer stays
// generic. It owns the meshes used to represent the world.

#ifndef RENDER_GAME_H
#define RENDER_GAME_H

#include "../engine/render/draw.h"
#include "game.h"

typedef struct {
    Mesh cube;
    Mesh plane;
} Game_Render;

void game_render_init(Game_Render *gr, SDL_GPUDevice *device);
void game_render_shutdown(Game_Render *gr, SDL_GPUDevice *device);

// Draw the world (ground, machines, hover preview) for the given frame. Must be
// called inside the engine's scene pass (between engine_render_begin/_end).
void game_render_draw(Draw *dr, Game_Render *gr, const Game_State *game);

#endif
