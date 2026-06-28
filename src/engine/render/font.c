// Font implementation: bitmap font rendering (included into mach.c).

#include "font.h"
#include <stdlib.h>
#include <string.h>

// Create a simple monospace bitmap font by drawing characters to a surface.
// Character grid: 16 chars wide × 8 rows (128 total ASCII chars 32-127).
// Each char is 8×8 pixels on a white-on-black surface.

static void draw_bitmap_char(SDL_Surface *surf, int char_idx, u8 *bitmap) {
    int grid_x = (char_idx % 16) * 8;
    int grid_y = (char_idx / 16) * 8;

    u32 white = SDL_MapSurfaceRGB(surf, 255, 255, 255);

    for (int row = 0; row < 8; row++) {
        u8 bits = bitmap[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                SDL_Rect rect = {grid_x + col, grid_y + row, 1, 1};
                SDL_FillSurfaceRect(surf, &rect, white);
            }
        }
    }
}

Font* font_create(SDL_Renderer *rend) {
    if (!rend) return NULL;

    Font *font = (Font *)malloc(sizeof(Font));
    if (!font) return NULL;

    // Create 16×8 grid surface for 128 characters (32-127 ASCII)
    int tex_w = 16 * 8;
    int tex_h = 8 * 8;
    SDL_Surface *surf = SDL_CreateSurface(tex_w, tex_h, SDL_PIXELFORMAT_RGBA8888);
    if (!surf) {
        free(font);
        return NULL;
    }

    // Fill with black background
    u32 black = SDL_MapSurfaceRGB(surf, 0, 0, 0);
    SDL_FillSurfaceRect(surf, NULL, black);

    // Draw each ASCII character (32-127) as a simple bitmap pattern.
    // Each character is defined as 8 bytes (8 rows × 8 bits).

    for (int i = 0; i < 96; i++) {
        char ch = (char)(32 + i);
        u8 bitmap[8] = {0};

        // Generate a simple bitmap for each character
        switch (ch) {
        case ' ': memset(bitmap, 0x00, 8); break;
        case '0': {
            u8 b[] = {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case '1': {
            u8 b[] = {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case '2': {
            u8 b[] = {0x3C, 0x66, 0x06, 0x0C, 0x18, 0x30, 0x7E, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case '3': {
            u8 b[] = {0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case '4': {
            u8 b[] = {0x0C, 0x1C, 0x3C, 0x6C, 0x7E, 0x0C, 0x0C, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case '5': {
            u8 b[] = {0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case '6': {
            u8 b[] = {0x3C, 0x66, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case '7': {
            u8 b[] = {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x60, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case '8': {
            u8 b[] = {0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case '9': {
            u8 b[] = {0x3C, 0x66, 0x66, 0x3E, 0x06, 0x66, 0x3C, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case ':': {
            u8 b[] = {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'F': {
            u8 b[] = {0x7E, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x60, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'M': {
            u8 b[] = {0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'P': {
            u8 b[] = {0x7C, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x60, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'S': {
            u8 b[] = {0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'T': {
            u8 b[] = {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'a': {
            u8 b[] = {0x00, 0x3C, 0x06, 0x3E, 0x66, 0x66, 0x3C, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'e': {
            u8 b[] = {0x00, 0x3C, 0x66, 0x7E, 0x60, 0x66, 0x3C, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'o': {
            u8 b[] = {0x00, 0x3C, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'r': {
            u8 b[] = {0x00, 0x7C, 0x66, 0x60, 0x60, 0x60, 0x60, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'A': {
            u8 b[] = {0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'B': {
            u8 b[] = {0x7C, 0x66, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'C': {
            u8 b[] = {0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'D': {
            u8 b[] = {0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'E': {
            u8 b[] = {0x7E, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x7E, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'G': {
            u8 b[] = {0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3C, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'H': {
            u8 b[] = {0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x66, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'I': {
            u8 b[] = {0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'J': {
            u8 b[] = {0x0E, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3C, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'K': {
            u8 b[] = {0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'L': {
            u8 b[] = {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'N': {
            u8 b[] = {0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'O': {
            u8 b[] = {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'Q': {
            u8 b[] = {0x3C, 0x66, 0x66, 0x66, 0x6E, 0x7C, 0x06, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'R': {
            u8 b[] = {0x7C, 0x66, 0x66, 0x7C, 0x78, 0x6C, 0x66, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'U': {
            u8 b[] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'V': {
            u8 b[] = {0x66, 0x66, 0x66, 0x66, 0x3C, 0x3C, 0x18, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'W': {
            u8 b[] = {0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x63, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'X': {
            u8 b[] = {0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'Y': {
            u8 b[] = {0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x18, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'Z': {
            u8 b[] = {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'b': {
            u8 b[] = {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'c': {
            u8 b[] = {0x00, 0x3C, 0x60, 0x60, 0x60, 0x60, 0x3C, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'd': {
            u8 b[] = {0x06, 0x06, 0x3E, 0x66, 0x66, 0x66, 0x3E, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'f': {
            u8 b[] = {0x1C, 0x30, 0x7E, 0x30, 0x30, 0x30, 0x30, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'g': {
            u8 b[] = {0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x3C, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'h': {
            u8 b[] = {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'i': {
            u8 b[] = {0x18, 0x00, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'j': {
            u8 b[] = {0x0C, 0x00, 0x0C, 0x0C, 0x0C, 0x6C, 0x38, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'k': {
            u8 b[] = {0x60, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'l': {
            u8 b[] = {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'm': {
            u8 b[] = {0x00, 0x7C, 0xA6, 0x92, 0x92, 0x82, 0x82, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'n': {
            u8 b[] = {0x00, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'p': {
            u8 b[] = {0x00, 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'q': {
            u8 b[] = {0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 's': {
            u8 b[] = {0x00, 0x3C, 0x60, 0x3C, 0x06, 0x06, 0x3C, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 't': {
            u8 b[] = {0x30, 0x7E, 0x30, 0x30, 0x30, 0x30, 0x1C, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'u': {
            u8 b[] = {0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'v': {
            u8 b[] = {0x00, 0x66, 0x66, 0x66, 0x3C, 0x3C, 0x18, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'w': {
            u8 b[] = {0x00, 0x63, 0x63, 0x6B, 0x7F, 0x37, 0x63, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'x': {
            u8 b[] = {0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'y': {
            u8 b[] = {0x00, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x3C, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        case 'z': {
            u8 b[] = {0x00, 0x7E, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        default: {
            u8 b[] = {0xFF, 0x81, 0x81, 0x81, 0x81, 0x81, 0xFF, 0x00};
            memcpy(bitmap, b, 8);
        } break;
        }

        draw_bitmap_char(surf, i, bitmap);
    }

    font->atlas = SDL_CreateTextureFromSurface(rend, surf);
    SDL_DestroySurface(surf);

    if (!font->atlas) {
        free(font);
        return NULL;
    }

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

    SDL_SetTextureColorMod(font->atlas, r, g, b);

    i32 cur_x = x;
    for (const char *c = text; *c; c++) {
        u8 ch = (u8)*c;
        if (ch < 32 || ch > 127) continue;

        i32 char_idx = ch - 32;
        i32 src_x = (char_idx % 16) * 8;
        i32 src_y = (char_idx / 16) * 8;

        SDL_FRect src = {(f32)src_x, (f32)src_y, 8.0f, 8.0f};
        SDL_FRect dst = {(f32)cur_x, (f32)y, 8.0f, 8.0f};

        SDL_RenderTexture(rend, font->atlas, &src, &dst);
        cur_x += 8;
    }
}
