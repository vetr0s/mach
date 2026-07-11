#include "save.h"

#include <stdio.h>

// SKELETON STUBS. Real implementation is sequenced after the economy lands, so the
// format can cover playable_side, per-entity tiers, and money in one versioned blob.

b32 game_save(const Game_State *g, const char *path) {
    (void)g;
    (void)path;
    return MACH_FALSE;
}

b32 game_load(Game_State *g, const char *path) {
    (void)g;
    (void)path;
    return MACH_FALSE;
}

b32 save_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f)
        return MACH_FALSE;
    fclose(f);
    return MACH_TRUE;
}
