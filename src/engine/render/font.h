// Bitmap font: 8x8 glyphs baked into a GPU atlas texture for the 2D overlay.

#ifndef FONT_H
#define FONT_H

#include <SDL3/SDL.h>
#include "../base/base.h"

typedef struct {
    SDL_GPUTexture *atlas;    // R8: glyph shapes plus one solid white texel
    SDL_GPUSampler *sampler;  // nearest, clamp-to-edge

    i32 glyph_w, glyph_h;     // glyph cell size in pixels
    i32 advance;              // horizontal step per character
    f32 atlas_w, atlas_h;     // atlas dimensions (for UV math)
    f32 white_u, white_v;     // UV of the solid white texel (for filled rects)
} Font;

Font *font_create(SDL_GPUDevice *device);
void  font_destroy(SDL_GPUDevice *device, Font *font);

// Atlas-space UV rect for an ASCII character. Returns false for glyphs outside
// the printable range.
b32 font_glyph_uv(const Font *font, char ch, f32 *u0, f32 *v0, f32 *u1, f32 *v1);

#endif
