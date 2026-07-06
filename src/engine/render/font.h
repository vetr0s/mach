// Bitmap font: 8x8 glyphs baked into a GL texture atlas for 2D text.

#ifndef FONT_H
#define FONT_H

#include "../base/base.h"
#include "gl.h"

struct Renderer;

typedef struct {
    R2D_Texture atlas;        // RGBA: white glyph pixels with alpha; tinted per-vertex
    i32 glyph_w, glyph_h;     // glyph cell size in pixels
    i32 advance;              // horizontal step per character
} Font;

Font *font_create(struct Renderer *r);
void  font_destroy(struct Renderer *r, Font *font);

// Normalized atlas UVs for an ASCII character. Returns false for glyphs outside
// the printable range.
b32 font_glyph_uv(const Font *font, char ch, f32 *u0, f32 *v0, f32 *u1, f32 *v1);

#endif
