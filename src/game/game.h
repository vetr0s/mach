#ifndef GAME_H
#define GAME_H

#include "../base/base.h"
#include "../core/core.h"

// Game-specific logic.

typedef struct {
    // Game state goes here as we develop.
} Game_State;

void game_init(Game_State *g);
void game_update(Game_State *g);
void game_shutdown(Game_State *g);

#endif
