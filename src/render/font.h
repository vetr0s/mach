// Bitmap font: simple monospace font for rendering text.

#ifndef FONT_H
#define FONT_H

#include "../base/base.h"
#include <SDL3/SDL.h>

// Font metrics
#define FONT_CHAR_WIDTH 5
#define FONT_CHAR_HEIGHT 8
#define FONT_CHARS_PER_ROW 16
#define FONT_NUM_ROWS 8

typedef struct {
    SDL_Texture *texture;
    i32 char_width;
    i32 char_height;
} Font;

// Font API
Font* font_create(SDL_Renderer *rend);
void font_destroy(Font *font);
void font_render_text(SDL_Renderer *rend, Font *font, const char *text, i32 x, i32 y, u8 r, u8 g, u8 b);

#endif
