// Game-side scene rendering: draws the factory world as 2D isometric tiles and
// shaded blocks.
//
// This lives in the game layer (not the engine) so the engine renderer stays
// generic. It composes the engine's 2D primitives into the game's look.

#ifndef RENDER_GAME_H
#define RENDER_GAME_H

#include "mach.h"
#include "game.h"

// Draw the world (ground tiles, machines, hover preview) for the given frame.
// `scratch` backs this frame's sort buffers; pass the engine's frame arena.
void game_render_draw(Mach_Renderer *r, const Game_State *game, Mach_Arena *scratch);

// Draw the HUD (Clay floating panels: status, inspect, debug, controls) over the
// world. Non-const g: Clay's per-frame context lives in g->clay.
void game_render_hud(Game_State *g, Mach *m);

#endif
