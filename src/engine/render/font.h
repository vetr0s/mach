// Bitmap font: TrueType rendered with immediate drawing.

#ifndef FONT_H
#define FONT_H

#include "../base/base.h"
#include <SDL3/SDL.h>

typedef struct Font Font;

// Font API
Font* font_create(SDL_Renderer *rend);
void font_destroy(Font *font);
void font_render_text(SDL_Renderer *rend, Font *font, const char *text, i32 x, i32 y, u8 r, u8 g, u8 b);

#endif
