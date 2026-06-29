// Bitmap font: 8x8 glyphs baked into an SDL_Texture atlas for 2D text.

#ifndef FONT_H
#define FONT_H

#include <SDL3/SDL.h>
#include "../base/base.h"

typedef struct {
    SDL_Texture *atlas;       // RGBA: white glyph pixels with alpha; tinted via color mod

    i32 glyph_w, glyph_h;     // glyph cell size in pixels
    i32 advance;              // horizontal step per character
    f32 atlas_w, atlas_h;     // atlas dimensions (pixels)
} Font;

Font *font_create(SDL_Renderer *renderer);
void  font_destroy(Font *font);

// Pixel-space source rect for an ASCII character in the atlas. Returns false for
// glyphs outside the printable range.
b32 font_glyph_src(const Font *font, char ch, SDL_FRect *out);

#endif
