// TrueType font rendering via stb_truetype.

#ifndef FONT_TRUETYPE_H
#define FONT_TRUETYPE_H

#include "../base/base.h"
#include <SDL3/SDL.h>

typedef struct {
    u8 *font_data;           // Loaded TTF file data
    void *font_info;         // stbtt_fontinfo (opaque)
    f32 scale;               // Pixel scale for current size
} FontTrueType;

// Load a TrueType font file.
FontTrueType* font_truetype_load(const char *path, f32 font_size);

// Free font resources.
void font_truetype_free(FontTrueType *font);

// Render text to renderer at (x, y) with color.
void font_truetype_render_text(SDL_Renderer *rend, FontTrueType *font,
                               const char *text, i32 x, i32 y,
                               u8 r, u8 g, u8 b);

#endif
