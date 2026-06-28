// TrueType font implementation using stb_truetype (included into mach.c).

#include "font_truetype.h"
#include <stdlib.h>
#include <stdio.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "../../../third_party/stb/stb_truetype.h"

FontTrueType* font_truetype_load(const char *path, f32 font_size) {
    if (!path) return NULL;

    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    u8 *font_data = (u8 *)malloc(size);
    if (!font_data) {
        fclose(f);
        return NULL;
    }

    if (fread(font_data, 1, size, f) != (size_t)size) {
        free(font_data);
        fclose(f);
        return NULL;
    }
    fclose(f);

    stbtt_fontinfo *info = (stbtt_fontinfo *)malloc(sizeof(stbtt_fontinfo));
    if (!info) {
        free(font_data);
        return NULL;
    }

    if (!stbtt_InitFont(info, font_data, 0)) {
        free(info);
        free(font_data);
        return NULL;
    }

    FontTrueType *font = (FontTrueType *)malloc(sizeof(FontTrueType));
    if (!font) {
        free(info);
        free(font_data);
        return NULL;
    }

    font->font_data = font_data;
    font->font_info = info;
    font->scale = stbtt_ScaleForPixelHeight((stbtt_fontinfo *)info, font_size);

    return font;
}

void font_truetype_free(FontTrueType *font) {
    if (!font) return;
    if (font->font_info) free(font->font_info);
    if (font->font_data) free(font->font_data);
    free(font);
}

void font_truetype_render_text(SDL_Renderer *rend, FontTrueType *font,
                               const char *text, i32 x, i32 y,
                               u8 r, u8 g, u8 b) {
    if (!rend || !font || !text) return;

    stbtt_fontinfo *info = (stbtt_fontinfo *)font->font_info;
    i32 cursor_x = x;
    i32 cursor_y = y;

    SDL_SetRenderDrawColor(rend, r, g, b, 255);

    for (const char *p = text; *p; p++) {
        int c0 = *p;
        if (c0 == '\n') {
            cursor_x = x;
            cursor_y += (i32)(20 * font->scale);  // Rough line height
            continue;
        }

        int c1 = (p[1] ? p[1] : 0);
        int advance, lsb;
        stbtt_GetCodepointHMetrics(info, c0, &advance, &lsb);

        int x0, y0, x1, y1;
        stbtt_GetCodepointBox(info, c0, &x0, &y0, &x1, &y1);

        i32 glyph_width = (i32)((x1 - x0) * font->scale);
        i32 glyph_height = (i32)((y1 - y0) * font->scale);

        if (glyph_width > 0 && glyph_height > 0) {
            u8 *bitmap = (u8 *)malloc(glyph_width * glyph_height);
            if (bitmap) {
                stbtt_MakeCodepointBitmap(info, bitmap, glyph_width, glyph_height,
                                         glyph_width, font->scale, font->scale, c0);

                // Render bitmap to screen (simple per-pixel drawing)
                for (i32 py = 0; py < glyph_height; py++) {
                    for (i32 px = 0; px < glyph_width; px++) {
                        u8 alpha = bitmap[py * glyph_width + px];
                        if (alpha > 127) {
                            SDL_RenderPoint(rend, (f32)(cursor_x + px), (f32)(cursor_y + py));
                        }
                    }
                }
                free(bitmap);
            }
        }

        cursor_x += (i32)(advance * font->scale);
        if (c1) {
            int kern = stbtt_GetCodepointKernAdvance(info, c0, c1);
            cursor_x += (i32)(kern * font->scale);
        }
    }
}
