// Save/load. The persistence interface is declared in game.h (game_save, game_load,
// save_exists, SAVE_PATH); this header is where save.c's own internals live if it
// needs any. Kept separate from game.h so the format work is self-contained.
//
// SKELETON STUB: the functions in save.c return false / do nothing until the
// serialization work lands. They must not be filled in until the economy's World and
// Entity layout is settled, since the save format has to cover the new fields
// (playable region, per-entity tiers, money).

#ifndef SAVE_H
#define SAVE_H

#include "game.h"

#endif // SAVE_H
