// 2D renderer over SDL_Renderer: frame, primitives, text, sprites, and an
// isometric camera. See ARCHITECTURE.md for the design.

#ifndef RENDER2D_H
#define RENDER2D_H

#include <SDL3/SDL.h>
#include "../base/base.h"
#include "../math/math.h"
#include "font.h"

// Isometric tile footprint in screen pixels at zoom 1 (classic 2:1 diamond), and
// the screen height of one unit of block elevation.
#define ISO_TILE_W 64.0f
#define ISO_TILE_H 32.0f
#define ISO_ELEV   28.0f

// 2D camera over the isometric plane: an iso-space pan point centered on screen,
// and a zoom (pixels-per-iso-unit multiplier).
typedef struct {
    Vec2 pan;
    f32  zoom;
} Camera2D;

typedef struct {
    SDL_Renderer *sdl;
    SDL_Window   *window;
    i32 width, height;   // logical render size (window points)
    Font *font;          // bitmap font as an SDL_Texture atlas
} Renderer;

// Lifecycle.
b32  r2d_init(Renderer *r, SDL_Window *window);
void r2d_shutdown(Renderer *r);

// Re-read the window size and update the logical presentation. Call after the
// window is resized so render and input coordinates track the new size.
void r2d_resized(Renderer *r);

// Frame.
void r2d_begin(Renderer *r, u8 clear_r, u8 clear_g, u8 clear_b);
void r2d_present(Renderer *r);

// Screen-space primitives. Colors are Vec4 RGBA in [0,1].
void r2d_fill_rect(Renderer *r, f32 x, f32 y, f32 w, f32 h, Vec4 color);
void r2d_fill_poly(Renderer *r, const Vec2 *pts, i32 n, Vec4 color);  // convex, <=16 pts
void r2d_poly_outline(Renderer *r, const Vec2 *pts, i32 n, Vec4 color);  // closed loop, <=16 pts
void r2d_text(Renderer *r, f32 x, f32 y, f32 scale, const char *text, Vec4 color);

// Sprites (for real art later). Tint multiplies the texture; pass white for none.
SDL_Texture *r2d_load_texture(Renderer *r, const char *path);
void r2d_sprite(Renderer *r, SDL_Texture *tex, f32 x, f32 y, f32 scale, Vec4 tint);

// Isometric projection helpers (no Renderer needed). `elev` is block height in
// units; the inverse solves on the ground plane (elev 0).
Vec2 iso_to_screen(const Camera2D *cam, f32 screen_w, f32 screen_h,
                   f32 grid_x, f32 grid_y, f32 elev);
Vec2 screen_to_iso(const Camera2D *cam, f32 screen_w, f32 screen_h,
                   f32 screen_x, f32 screen_y);

#endif
