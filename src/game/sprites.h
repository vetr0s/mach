// Sprite art, baked into the executable.
//
// nob.c turns assets/sprites/*.png into a generated header of encoded PNG bytes
// (build/generated/assets_sprites.h). At startup we decode those bytes and upload
// them as textures. Nothing is read from disk at runtime: the game ships as one
// self-contained binary per platform, and that is a constraint, not an accident.
//
// Art is optional. A sprite that does not exist yields a zeroed texture, and the
// renderer falls back to drawing the procedural shaded block it always drew. So
// the game runs with no art at all, and each new .png replaces one more block.

#ifndef SPRITES_H
#define SPRITES_H

#include "mach.h"

#define MAX_SPRITES 32

typedef struct {
    const char *name; // file stem: "ore.png" -> "ore"
    Mach_R2D_Texture tex;
} Sprite;

typedef struct {
    Sprite items[MAX_SPRITES];
    i32 count;
    Mach_Renderer *r; // the renderer the textures live on, kept so unload needs no args
} Sprites;

// Decode and upload every embedded sprite. Safe to call with no art present.
void sprites_load(Sprites *s, Mach_Renderer *r);

// Release the GL textures. game_shutdown's signature is part of the hot-reload
// seam and carries no Mach*, hence the renderer stashed above.
void sprites_unload(Sprites *s);

// Look up by file stem. Returns a zeroed texture (id 0) when the sprite is
// absent, which every draw path treats as "no art, use the block".
Mach_R2D_Texture sprites_get(const Sprites *s, const char *name);

#endif // SPRITES_H
