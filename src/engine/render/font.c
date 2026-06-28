// Font implementation: Proggy Clean rendered to bitmap atlas (included into mach.c).

#include "font.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../../../third_party/stb/stb_truetype.h"

#define FONT_SIZE 8
#define CHARS_PER_ROW 16
#define CHARS_ROWS 8
#define CHAR_SIZE 8

Font* font_create(SDL_Renderer *rend) {
    if (!rend) return NULL;

    Font *font = (Font *)malloc(sizeof(Font));
    if (!font) return NULL;

    // Try to load Proggy Clean TTF
    FILE *f = fopen("assets/fonts/ProggyClean.ttf/ProggyClean.ttf", "rb");
    int use_proggy = 0;

    stbtt_fontinfo info = {0};
    u8 *font_data = NULL;

    if (f) {
        fseek(f, 0, SEEK_END);
        long fsize = ftell(f);
        fseek(f, 0, SEEK_SET);

        font_data = (u8 *)malloc(fsize);
        if (font_data && fread(font_data, 1, fsize, f) == (size_t)fsize) {
            if (stbtt_InitFont(&info, font_data, 0)) {
                use_proggy = 1;
            }
        }
        fclose(f);
    }

    // Create 16x8 grid surface (128 chars, 8x8 each)
    int tex_w = CHARS_PER_ROW * CHAR_SIZE;
    int tex_h = CHARS_ROWS * CHAR_SIZE;
    SDL_Surface *surf = SDL_CreateSurface(tex_w, tex_h, SDL_PIXELFORMAT_RGBA8888);
    if (!surf) {
        if (font_data) free(font_data);
        free(font);
        return NULL;
    }

    // Fill with black background
    u32 black = SDL_MapSurfaceRGB(surf, 0, 0, 0);
    SDL_FillSurfaceRect(surf, NULL, black);

    if (use_proggy) {
        // Render Proggy Clean TTF to grid
        f32 scale = stbtt_ScaleForPixelHeight(&info, FONT_SIZE);
        u32 white = SDL_MapSurfaceRGB(surf, 255, 255, 255);

        for (int i = 0; i < 96; i++) {
            unsigned char ch = (unsigned char)(32 + i);
            int grid_x = (i % CHARS_PER_ROW) * CHAR_SIZE;
            int grid_y = (i / CHARS_PER_ROW) * CHAR_SIZE;

            // Render glyph to bitmap
            u8 *glyph_bitmap = (u8 *)malloc(CHAR_SIZE * CHAR_SIZE);
            if (glyph_bitmap) {
                memset(glyph_bitmap, 0, CHAR_SIZE * CHAR_SIZE);
                stbtt_MakeCodepointBitmap(&info, glyph_bitmap, CHAR_SIZE, CHAR_SIZE,
                                         CHAR_SIZE, scale, scale, ch);

                // Copy to surface pixel-by-pixel using SDL_FillSurfaceRect
                for (int gy = 0; gy < CHAR_SIZE; gy++) {
                    for (int gx = 0; gx < CHAR_SIZE; gx++) {
                        u8 alpha = glyph_bitmap[gy * CHAR_SIZE + gx];
                        if (alpha > 127) {
                            SDL_Rect pixel_rect = {grid_x + gx, grid_y + gy, 1, 1};
                            SDL_FillSurfaceRect(surf, &pixel_rect, white);
                        }
                    }
                }
                free(glyph_bitmap);
            }
        }
        fprintf(stderr, "[FONT] Loaded Proggy Clean at %dpx\n", FONT_SIZE);
    } else {
        // Fallback: simple boxes to verify rendering works
        fprintf(stderr, "[FONT] Proggy Clean not found, using test pattern\n");

        u32 white = SDL_MapSurfaceRGB(surf, 255, 255, 255);

        // Draw test pattern: fill first character to verify rendering
        SDL_Rect test_rect = {0, 0, CHAR_SIZE, CHAR_SIZE};
        SDL_FillSurfaceRect(surf, &test_rect, white);
    }

    font->atlas = SDL_CreateTextureFromSurface(rend, surf);

    if (!font->atlas) {
        fprintf(stderr, "[FONT] Error: failed to create texture from surface\n");
        SDL_DestroySurface(surf);
        if (font_data) free(font_data);
        free(font);
        return NULL;
    }

    fprintf(stderr, "[FONT] Atlas texture created: %dx%d\n", surf->w, surf->h);
    SDL_DestroySurface(surf);

    if (font_data) free(font_data);

    SDL_SetTextureBlendMode(font->atlas, SDL_BLENDMODE_BLEND);
    return font;
}

void font_destroy(Font *font) {
    if (!font) return;
    if (font->atlas) {
        SDL_DestroyTexture(font->atlas);
    }
    free(font);
}

void font_render_text(SDL_Renderer *rend, Font *font, const char *text, i32 x, i32 y, u8 r, u8 g, u8 b) {
    if (!rend || !font || !text) return;

    if (!font->atlas) {
        fprintf(stderr, "[FONT] Error: font->atlas is NULL\n");
        return;
    }

    static int debug_once = 1;
    if (debug_once && text[0]) {
        fprintf(stderr, "[FONT] Render: text='%s' at (%d,%d) RGB(%d,%d,%d) atlas=%p\n", text, x, y, r, g, b, font->atlas);
        debug_once = 0;
    }

    SDL_SetTextureColorMod(font->atlas, r, g, b);

    i32 cur_x = x;
    int char_count = 0;
    for (const char *c = text; *c; c++) {
        u8 ch = (u8)*c;
        if (ch < 32 || ch > 127) continue;

        char_count++;
        i32 char_idx = ch - 32;
        i32 src_x = (char_idx % CHARS_PER_ROW) * CHAR_SIZE;
        i32 src_y = (char_idx / CHARS_PER_ROW) * CHAR_SIZE;

        SDL_FRect src = {(f32)src_x, (f32)src_y, (f32)CHAR_SIZE, (f32)CHAR_SIZE};
        SDL_FRect dst = {(f32)cur_x, (f32)y, (f32)CHAR_SIZE, (f32)CHAR_SIZE};

        int result = SDL_RenderTexture(rend, font->atlas, &src, &dst);
        if (result != 0 && char_count == 1) {
            fprintf(stderr, "[FONT] SDL_RenderTexture error: %s\n", SDL_GetError());
        }

        cur_x += CHAR_SIZE;
    }

    if (char_count > 0 && debug_once == 0) {
        fprintf(stderr, "[FONT] Rendered %d chars\n", char_count);
    }
}
