// Font implementation: simple monospace font using rectangles (included into mach.c).

#include "font.h"
#include "render.h"

// (npt): Simple text rendering using filled rectangles.
// Each character is drawn as small blocks, good enough for HUD text.

Font* font_create(SDL_Renderer *rend) {
    (void)rend;
    Font *font = (Font *)malloc(sizeof(Font));
    if (!font) return NULL;

    font->char_width = 6;
    font->char_height = 8;
    font->texture = NULL;

    return font;
}

void font_destroy(Font *font) {
    if (!font) return;
    free(font);
}

// Draw a single character using filled rectangles
static void draw_char(SDL_Renderer *rend, char ch, i32 x, i32 y, u8 r, u8 g, u8 b) {
    switch (ch) {
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
        // Draw digit as vertical bars (0-9)
        {
            int digit = ch - '0';
            for (int i = 0; i < digit; i++) {
                render_rect_filled(rend, x + i * 2, y, 1, 6, r, g, b, 255);
            }
        }
        break;
    case ':':
        render_rect_filled(rend, x + 2, y + 1, 1, 1, r, g, b, 255);
        render_rect_filled(rend, x + 2, y + 5, 1, 1, r, g, b, 255);
        break;
    case ' ':
        break;
    case 'F': case 'P': case 'S':
        render_rect_filled(rend, x + 0, y + 0, 4, 1, r, g, b, 255);
        render_rect_filled(rend, x + 0, y + 1, 1, 6, r, g, b, 255);
        if (ch == 'F') {
            render_rect_filled(rend, x + 0, y + 3, 3, 1, r, g, b, 255);
        } else if (ch == 'P') {
            render_rect_filled(rend, x + 0, y + 3, 3, 1, r, g, b, 255);
            render_rect_filled(rend, x + 3, y + 1, 1, 2, r, g, b, 255);
        } else {
            render_rect_filled(rend, x + 0, y + 3, 4, 1, r, g, b, 255);
            render_rect_filled(rend, x + 3, y + 4, 1, 3, r, g, b, 255);
        }
        break;
    case 'M': case 'T': case 'o': case 'r':
        render_rect_filled(rend, x + 0, y + 0, 4, 1, r, g, b, 255);
        render_rect_filled(rend, x + 2, y + 1, 1, 5, r, g, b, 255);
        break;
    default:
        render_rect_filled(rend, x + 0, y + 0, 4, 1, r, g, b, 255);
        render_rect_filled(rend, x + 0, y + 7, 4, 1, r, g, b, 255);
        render_rect_filled(rend, x + 0, y + 0, 1, 8, r, g, b, 255);
        render_rect_filled(rend, x + 3, y + 0, 1, 8, r, g, b, 255);
    }
}

void font_render_text(SDL_Renderer *rend, Font *font, const char *text, i32 x, i32 y, u8 r, u8 g, u8 b) {
    if (!rend || !font || !text) return;

    i32 cur_x = x;
    for (const char *c = text; *c; c++) {
        draw_char(rend, *c, cur_x, y, r, g, b);
        cur_x += font->char_width;
    }
}
