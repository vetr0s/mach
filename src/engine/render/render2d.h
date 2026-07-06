// 2D renderer over OpenGL 3.3 core: one batched stream of textured,
// vertex-colored triangles, plus primitives, text, sprites, and an isometric
// camera. See ARCHITECTURE.md for the design.

#ifndef RENDER2D_H
#define RENDER2D_H

#include "../rgfw.h"
#include "../base/base.h"
#include "../math/math.h"
#include "color.h"
#include "gl.h"
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

// One vertex of the batch. Everything — fills, text, sprites — draws through
// the same shader; untextured shapes sample a 1x1 white texture.
typedef struct {
    f32   x, y;   // window points; the shader maps to clip space
    f32   u, v;
    Color color;
} R2D_Vertex;

// Batch capacity. A flush costs one glDrawElements; overflowing mid-frame just
// splits the frame into more draws, so these only need to cover the common case.
#define R2D_MAX_VERTS   8192
#define R2D_MAX_INDICES 16384

typedef struct Renderer {
    RGFW_window *window;
    GLApi gl;              // loaded GL entry points (see gl.h)

    i32 width, height;         // logical render size (window points)
    i32 fb_width, fb_height;   // framebuffer size in pixels (differs under HiDPI)
    Font *font;                // bitmap font as a GL texture atlas

    // GL objects, created once at init.
    u32 program;
    u32 vao, vbo, ibo;
    i32 u_screen;              // uniform: logical size, for point -> clip mapping
    R2D_Texture white;         // 1x1 white, sampled by untextured draws

    // The pending batch: appended by the draw calls, flushed on texture change,
    // scissor change, overflow, or present.
    u32        batch_tex;      // texture the pending vertices sample
    i32        vert_count, index_count;
    R2D_Vertex verts[R2D_MAX_VERTS];
    u16        indices[R2D_MAX_INDICES];
} Renderer;

// Lifecycle. The window must already hold a current GL 3.3 core context.
b32  r2d_init(Renderer *r, RGFW_window *window);
void r2d_shutdown(Renderer *r);

// Re-read the window and framebuffer size. Call after the window is resized so
// render and input coordinates track the new size.
void r2d_resized(Renderer *r);

// Frame.
void r2d_begin(Renderer *r, Color clear);
void r2d_present(Renderer *r);

// Screen-space primitives. Colors are RGBA in [0,1]; see color.h for the palette.
void r2d_fill_rect(Renderer *r, f32 x, f32 y, f32 w, f32 h, Color color);
void r2d_fill_poly(Renderer *r, const Vec2 *pts, i32 n, Color color);  // convex, <=16 pts
void r2d_poly_outline(Renderer *r, const Vec2 *pts, i32 n, Color color);  // closed loop, <=16 pts
void r2d_text(Renderer *r, f32 x, f32 y, f32 scale, const char *text, Color color);

// Clip rect in window points (the UI's scissor). Draws between begin/end are
// clipped; nesting is not supported.
void r2d_clip_begin(Renderer *r, f32 x, f32 y, f32 w, f32 h);
void r2d_clip_end(Renderer *r);

// Textures and sprites (for real art later). Tint multiplies the texture; pass
// white for none. A zero id means the load failed.
R2D_Texture r2d_texture_from_pixels(Renderer *r, const void *rgba, i32 w, i32 h, b32 nearest);
R2D_Texture r2d_load_texture(Renderer *r, const char *path);
void r2d_destroy_texture(Renderer *r, R2D_Texture *tex);
void r2d_sprite(Renderer *r, R2D_Texture tex, f32 x, f32 y, f32 scale, Color tint);

// Isometric projection helpers (no Renderer needed). `elev` is block height in
// units; the inverse solves on the ground plane (elev 0).
Vec2 iso_to_screen(const Camera2D *cam, f32 screen_w, f32 screen_h,
                   f32 grid_x, f32 grid_y, f32 elev);
Vec2 screen_to_iso(const Camera2D *cam, f32 screen_w, f32 screen_h,
                   f32 screen_x, f32 screen_y);

#endif
