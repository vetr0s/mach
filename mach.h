// mach.h — a small 2D game engine in one header. C99.
//
// RGFW opens the window and delivers input; on top of that sits mach's own
// OpenGL 3.3 core batch renderer (one shader, one draw stream), a bitmap font,
// stb_image loading, a Clay UI binding, arenas, and the frame loop.
//
// Usage: include this header wherever the engine is used; in exactly ONE
// translation unit define the implementation first:
//
//     #define MACH_IMPLEMENTATION
//     #include "mach.h"
//
// The single-header dependencies must be on the include path (they are vendored
// in third_party/):  RGFW.h  stb_image.h  clay.h
//
// The engine holds no mutable global state: everything lives in the Mach_Engine /
// Mach_Renderer / Mach_Arena structs the caller owns and passes by pointer. That is what
// makes the hot-reload dev loop work; keep it that way.
//
// License: zlib, at the bottom of this file.

#ifndef MACH_H
#define MACH_H

// =============================================================================
// base: fundamental types and version
// =============================================================================

// Fundamental types, type aliases, and base utilities.


#include <stdint.h>
#include <stddef.h>
#include <string.h>

// Semantic versioning: MAJOR.MINOR.PATCH
#define MACH_VERSION_MAJOR 0
#define MACH_VERSION_MINOR 5
#define MACH_VERSION_PATCH 1

// Sized integer aliases. Define MACH_INT_DEFINED before including mach.h if
// your project already typedefs these names (they must match these widths).
#ifndef MACH_INT_DEFINED
#define MACH_INT_DEFINED
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float  f32;
typedef double f64;

typedef size_t    usize;
typedef ptrdiff_t isize;
#endif // MACH_INT_DEFINED

// 32-bit boolean. Used in place of bare `int` for truth values so intent is
// explicit and consistent across the codebase.
typedef i32 b32;
#define MACH_TRUE  1
#define MACH_FALSE 0

// Number of elements in a fixed-size array.
#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

// =============================================================================
// debug: leveled logging and assertions
// =============================================================================

// Debug utilities: leveled logging and assertions.
//
// Logging levels:
//   LOG_INFO  - lifecycle / high-level events (always compiled in)
//   LOG_ERROR - failures and recoverable errors (always compiled in)
//   LOG_DEBUG - verbose per-frame / per-event tracing (debug builds only)
//
// All output goes to stderr with a level tag so logs are greppable.

#include <stdio.h>

#define LOG_INFO(fmt, ...) \
  fprintf(stderr, "[INFO]  " fmt "\n", ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
  fprintf(stderr, "[ERROR] " fmt "\n", ##__VA_ARGS__)

// Break into the debugger, per compiler. gcc has no __builtin_debugbreak, so it
// gets __builtin_trap (kills the process instead of pausing it, but still stops
// exactly at the failed assertion).
#if defined(_MSC_VER)
  #define MACH_DEBUGBREAK() __debugbreak()
#elif defined(__clang__)
  #define MACH_DEBUGBREAK() __builtin_debugbreak()
#else
  #define MACH_DEBUGBREAK() __builtin_trap()
#endif

#ifdef NDEBUG
  #define DEBUG_ASSERT(x) (void)(x)
  #define LOG_DEBUG(fmt, ...) (void)0
#else
  #define DEBUG_ASSERT(x) \
    do { \
      if (!(x)) { \
        LOG_ERROR("assertion failed: %s (%s:%d)", #x, __FILE__, __LINE__); \
        MACH_DEBUGBREAK(); \
      } \
    } while (0)
  #define LOG_DEBUG(fmt, ...) \
    fprintf(stderr, "[DEBUG] " fmt "\n", ##__VA_ARGS__)
#endif

// =============================================================================
// RGFW declarations (implementation compiles below, with MACH_IMPLEMENTATION)
// =============================================================================

// base already defines the u8/i64-style aliases RGFW would otherwise typedef.
#define RGFW_INT_DEFINED
// RGFW_OPENGL must be defined before RGFW.h or the GL context API is compiled out.
#define RGFW_OPENGL
#include "RGFW.h"

// =============================================================================
// mem: arena allocator
// =============================================================================

// Mach_Arena allocator: linear bump allocation over a linked list of malloc'd regions.
//
// Modeled on Tsoding's arena.h (https://github.com/tsoding/arena). An allocation
// bumps a cursor inside the current region; when a region fills, a larger one is
// chained on. Individual allocations are never freed on their own — you reset the
// arena (keep the memory, reuse it) or free it whole. That trades fine-grained
// frees for near-zero bookkeeping and no fragmentation, which fits allocations
// that share a lifetime: a world, a level, per-frame scratch.


typedef struct Mach_Arena_Region Mach_Arena_Region;

// A region's storage is measured in words (uintptr_t), not bytes, so the bump
// cursor stays word-aligned and every allocation comes back aligned for free.
struct Mach_Arena_Region {
    Mach_Arena_Region *next;
    usize count;        // words handed out
    usize capacity;     // words available
    uintptr_t data[];   // region storage (flexible array member)
};

// Zero-initialize to create an empty arena: Mach_Arena a = {0};
typedef struct {
    Mach_Arena_Region *begin;
    Mach_Arena_Region *end;
} Mach_Arena;

// Hand back `size` bytes from the arena, chaining on a new region if the current
// one is full. The result is uintptr_t-aligned. Memory is NOT zeroed. Returns
// NULL only if the backing malloc fails.
void *arena_alloc(Mach_Arena *a, usize size);

// Mark every region empty so its memory is reused by later allocations, without
// returning anything to the OS. Pointers from before the reset become garbage.
void arena_reset(Mach_Arena *a);

// Return every region to the OS and leave the arena empty (safe to reuse).
void arena_free(Mach_Arena *a);

// =============================================================================
// math: 2D vectors and scalar helpers
// =============================================================================

// Math: 2D vectors and scalar helpers.
//
// The engine is 2D, so this is a 2D math library. Mach_Vec4 exists mainly as the
// backing of Mach_Color (render/color.h), which is what code should usually say.
// (3D vector/matrix math was removed with the SDL_GPU renderer; it returns if
// and when 3D does.)


typedef struct {
    f32 x, y;
} Mach_Vec2;

typedef struct {
    f32 x, y, z, w;
} Mach_Vec4;  // also used as RGBA color

// Scalar
f32 math_min(f32 a, f32 b);
f32 math_max(f32 a, f32 b);
f32 math_clamp(f32 v, f32 lo, f32 hi);
f32 math_lerp(f32 a, f32 b, f32 t);

// Mach_Vec2
Mach_Vec2 vec2_add(Mach_Vec2 a, Mach_Vec2 b);
Mach_Vec2 vec2_sub(Mach_Vec2 a, Mach_Vec2 b);
Mach_Vec2 vec2_scale(Mach_Vec2 v, f32 s);
f32  vec2_dot(Mach_Vec2 a, Mach_Vec2 b);
f32  vec2_length(Mach_Vec2 v);
Mach_Vec2 vec2_normalize(Mach_Vec2 v);
Mach_Vec2 vec2_lerp(Mach_Vec2 a, Mach_Vec2 b, f32 t);

// =============================================================================
// color: Mach_Color type, helpers, stock palette (modus-vivendi)
// =============================================================================

// Mach_Color type and the stock palette.
//
// Mach_Color is Mach_Vec4 RGBA in [0,1] — the exact type every r2d call takes — under the
// name that says what it is. The palette is modus-vivendi (Protesilaos Stavrou's
// Emacs theme, https://protesilaos.com/emacs/modus-themes): WCAG-AAA-contrast
// colors designed for a black background, which is exactly what a game HUD wants.
// The names mirror the theme's own (bg-dim, red-warmer, ...), so its docs apply.
//
// A game can lean on these everywhere, or define its own colors with COLOR_HEX
// and ignore the palette entirely. The engine itself has no opinion.


typedef Mach_Vec4 Mach_Color;

// A Mach_Color from 0xRRGGBB, alpha 1. Every palette entry below is one of these, so
// the hex value stays visible and greppable.
#define COLOR_HEX(h) ((Mach_Color){ \
    (f32)(((h) >> 16) & 0xFF) / 255.0f, \
    (f32)(((h) >>  8) & 0xFF) / 255.0f, \
    (f32)(((h)      ) & 0xFF) / 255.0f, \
    1.0f })

// --- Basic values -------------------------------------------------------------

#define COLOR_BG_MAIN      COLOR_HEX(0x000000)
#define COLOR_BG_DIM       COLOR_HEX(0x1e1e1e)
#define COLOR_FG_MAIN      COLOR_HEX(0xffffff)
#define COLOR_FG_DIM       COLOR_HEX(0x989898)
#define COLOR_FG_ALT       COLOR_HEX(0xc6daff)
#define COLOR_BG_ACTIVE    COLOR_HEX(0x535353)
#define COLOR_BG_INACTIVE  COLOR_HEX(0x303030)
#define COLOR_BORDER       COLOR_HEX(0x646464)

#define COLOR_BLACK        COLOR_BG_MAIN
#define COLOR_WHITE        COLOR_FG_MAIN

// --- Common accents -----------------------------------------------------------

#define COLOR_RED              COLOR_HEX(0xff5f59)
#define COLOR_RED_WARMER       COLOR_HEX(0xff6b55)
#define COLOR_RED_COOLER       COLOR_HEX(0xff7f86)
#define COLOR_RED_FAINT        COLOR_HEX(0xff9580)
#define COLOR_RED_INTENSE      COLOR_HEX(0xff5f5f)

#define COLOR_GREEN            COLOR_HEX(0x44bc44)
#define COLOR_GREEN_WARMER     COLOR_HEX(0x70b900)
#define COLOR_GREEN_COOLER     COLOR_HEX(0x00c06f)
#define COLOR_GREEN_FAINT      COLOR_HEX(0x88ca9f)
#define COLOR_GREEN_INTENSE    COLOR_HEX(0x44df44)

#define COLOR_YELLOW           COLOR_HEX(0xd0bc00)
#define COLOR_YELLOW_WARMER    COLOR_HEX(0xfec43f)
#define COLOR_YELLOW_COOLER    COLOR_HEX(0xdfaf7a)
#define COLOR_YELLOW_FAINT     COLOR_HEX(0xd2b580)
#define COLOR_YELLOW_INTENSE   COLOR_HEX(0xefef00)

#define COLOR_BLUE             COLOR_HEX(0x2fafff)
#define COLOR_BLUE_WARMER      COLOR_HEX(0x79a8ff)
#define COLOR_BLUE_COOLER      COLOR_HEX(0x00bcff)
#define COLOR_BLUE_FAINT       COLOR_HEX(0x82b0ec)
#define COLOR_BLUE_INTENSE     COLOR_HEX(0x338fff)

#define COLOR_MAGENTA          COLOR_HEX(0xfeacd0)
#define COLOR_MAGENTA_WARMER   COLOR_HEX(0xf78fe7)
#define COLOR_MAGENTA_COOLER   COLOR_HEX(0xb6a0ff)
#define COLOR_MAGENTA_FAINT    COLOR_HEX(0xcaa6df)
#define COLOR_MAGENTA_INTENSE  COLOR_HEX(0xff66ff)

#define COLOR_CYAN             COLOR_HEX(0x00d3d0)
#define COLOR_CYAN_WARMER      COLOR_HEX(0x4ae2f0)
#define COLOR_CYAN_COOLER      COLOR_HEX(0x6ae4b9)
#define COLOR_CYAN_FAINT       COLOR_HEX(0x9ac8e0)
#define COLOR_CYAN_INTENSE     COLOR_HEX(0x00eff0)

// --- Uncommon accents ---------------------------------------------------------

#define COLOR_RUST    COLOR_HEX(0xdb7b5f)
#define COLOR_GOLD    COLOR_HEX(0xc0965b)
#define COLOR_OLIVE   COLOR_HEX(0x9cbd6f)
#define COLOR_SLATE   COLOR_HEX(0x76afbf)
#define COLOR_INDIGO  COLOR_HEX(0x9099d9)
#define COLOR_MAROON  COLOR_HEX(0xcf7fa7)
#define COLOR_PINK    COLOR_HEX(0xd09dc0)

// --- Accent backgrounds (intense > subtle > nuanced) --------------------------

#define COLOR_BG_RED_INTENSE      COLOR_HEX(0x9d1f1f)
#define COLOR_BG_GREEN_INTENSE    COLOR_HEX(0x2f822f)
#define COLOR_BG_YELLOW_INTENSE   COLOR_HEX(0x7a6100)
#define COLOR_BG_BLUE_INTENSE     COLOR_HEX(0x1640b0)
#define COLOR_BG_MAGENTA_INTENSE  COLOR_HEX(0x7030af)
#define COLOR_BG_CYAN_INTENSE     COLOR_HEX(0x2266ae)

#define COLOR_BG_RED_SUBTLE       COLOR_HEX(0x620f2a)
#define COLOR_BG_GREEN_SUBTLE     COLOR_HEX(0x00422a)
#define COLOR_BG_YELLOW_SUBTLE    COLOR_HEX(0x4a4000)
#define COLOR_BG_BLUE_SUBTLE      COLOR_HEX(0x242679)
#define COLOR_BG_MAGENTA_SUBTLE   COLOR_HEX(0x552f5f)
#define COLOR_BG_CYAN_SUBTLE      COLOR_HEX(0x004065)

#define COLOR_BG_RED_NUANCED      COLOR_HEX(0x3a0c14)
#define COLOR_BG_GREEN_NUANCED    COLOR_HEX(0x092f1f)
#define COLOR_BG_YELLOW_NUANCED   COLOR_HEX(0x381d0f)
#define COLOR_BG_BLUE_NUANCED     COLOR_HEX(0x12154a)
#define COLOR_BG_MAGENTA_NUANCED  COLOR_HEX(0x2f0c3f)
#define COLOR_BG_CYAN_NUANCED     COLOR_HEX(0x042837)

// --- Special purpose ----------------------------------------------------------

#define COLOR_BG_POPUP    COLOR_HEX(0x0c0c0c)
#define COLOR_BG_HOVER    COLOR_HEX(0x45605e)
#define COLOR_BG_HL_LINE  COLOR_HEX(0x2f3849)
#define COLOR_BG_REGION   COLOR_HEX(0x5a5a5a)

// --- Helpers ------------------------------------------------------------------

// Same color, different alpha.
static inline Mach_Color color_alpha(Mach_Color c, f32 a) { c.w = a; return c; }

// Multiply RGB by f (keeps alpha): cheap directional shading.
static inline Mach_Color color_shade(Mach_Color c, f32 f) {
    return (Mach_Color){c.x * f, c.y * f, c.z * f, c.w};
}

// Move RGB toward white by t in [0,1] (keeps alpha).
static inline Mach_Color color_lighten(Mach_Color c, f32 t) {
    return (Mach_Color){c.x + (1.0f - c.x) * t, c.y + (1.0f - c.y) * t,
                   c.z + (1.0f - c.z) * t, c.w};
}

// Componentwise blend from a to b, alpha included.
static inline Mach_Color color_lerp(Mach_Color a, Mach_Color b, f32 t) {
    return (Mach_Color){a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                   a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t};
}

// =============================================================================
// gl: the hand-declared GL 3.3 core surface
// =============================================================================

// Minimal OpenGL 3.3 core surface for the 2D renderer.
//
// The renderer declares exactly the entry points and constants it uses instead
// of pulling in platform GL headers. r2d_init fills the function table through
// RGFW's proc loader once the context exists; the table lives inside the
// Mach_Renderer struct — pointer-passed like all engine state — so a hot-reloaded
// game library draws through the pointers the host loaded.


// Constants the renderer uses (values fixed by the GL spec). Guarded as a block
// in case a platform GL header ends up in the same translation unit.
#ifndef GL_TRIANGLES
#define GL_TRIANGLES            0x0004
#define GL_UNSIGNED_BYTE        0x1401
#define GL_UNSIGNED_SHORT       0x1403
#define GL_FLOAT                0x1406
#define GL_RGBA                 0x1908
#define GL_RGBA8                0x8058
#define GL_TEXTURE_2D           0x0DE1
#define GL_TEXTURE0             0x84C0
#define GL_TEXTURE_MAG_FILTER   0x2800
#define GL_TEXTURE_MIN_FILTER   0x2801
#define GL_TEXTURE_WRAP_S       0x2802
#define GL_TEXTURE_WRAP_T       0x2803
#define GL_NEAREST              0x2600
#define GL_LINEAR               0x2601
#define GL_CLAMP_TO_EDGE        0x812F
#define GL_BLEND                0x0BE2
#define GL_SCISSOR_TEST         0x0C11
#define GL_SRC_ALPHA            0x0302
#define GL_ONE_MINUS_SRC_ALPHA  0x0303
#define GL_COLOR_BUFFER_BIT     0x00004000
#define GL_ARRAY_BUFFER         0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STREAM_DRAW          0x88E0
#define GL_VERTEX_SHADER        0x8B31
#define GL_FRAGMENT_SHADER      0x8B30
#define GL_COMPILE_STATUS       0x8B81
#define GL_LINK_STATUS          0x8B82
#define GL_UNPACK_ALIGNMENT     0x0CF5
#define GL_VERSION              0x1F02
#endif

// The loaded GL entry points, named without the gl prefix: gl->DrawElements(...).
typedef struct Mach_GLApi {
    void (*ActiveTexture)(u32 texture);
    void (*AttachShader)(u32 program, u32 shader);
    void (*BindBuffer)(u32 target, u32 buffer);
    void (*BindTexture)(u32 target, u32 texture);
    void (*BindVertexArray)(u32 array);
    void (*BlendFunc)(u32 sfactor, u32 dfactor);
    void (*BufferData)(u32 target, isize size, const void *data, u32 usage);
    void (*BufferSubData)(u32 target, isize offset, isize size, const void *data);
    void (*Clear)(u32 mask);
    void (*ClearColor)(f32 r, f32 g, f32 b, f32 a);
    void (*CompileShader)(u32 shader);
    u32  (*CreateProgram)(void);
    u32  (*CreateShader)(u32 type);
    void (*DeleteBuffers)(i32 n, const u32 *buffers);
    void (*DeleteProgram)(u32 program);
    void (*DeleteShader)(u32 shader);
    void (*DeleteTextures)(i32 n, const u32 *textures);
    void (*DeleteVertexArrays)(i32 n, const u32 *arrays);
    void (*Disable)(u32 cap);
    void (*DrawElements)(u32 mode, i32 count, u32 type, const void *indices);
    void (*Enable)(u32 cap);
    void (*EnableVertexAttribArray)(u32 index);
    void (*GenBuffers)(i32 n, u32 *buffers);
    void (*GenTextures)(i32 n, u32 *textures);
    void (*GenVertexArrays)(i32 n, u32 *arrays);
    void (*GetProgramInfoLog)(u32 program, i32 max_len, i32 *len, char *log);
    void (*GetProgramiv)(u32 program, u32 pname, i32 *params);
    void (*GetShaderInfoLog)(u32 shader, i32 max_len, i32 *len, char *log);
    void (*GetShaderiv)(u32 shader, u32 pname, i32 *params);
    const u8 *(*GetString)(u32 name);
    i32  (*GetUniformLocation)(u32 program, const char *name);
    void (*LinkProgram)(u32 program);
    void (*PixelStorei)(u32 pname, i32 param);
    void (*Scissor)(i32 x, i32 y, i32 w, i32 h);
    void (*ShaderSource)(u32 shader, i32 count, const char *const *string, const i32 *length);
    void (*TexImage2D)(u32 target, i32 level, i32 internal_format, i32 w, i32 h,
                       i32 border, u32 format, u32 type, const void *pixels);
    void (*TexParameteri)(u32 target, u32 pname, i32 param);
    void (*Uniform1i)(i32 location, i32 v0);
    void (*Uniform2f)(i32 location, f32 v0, f32 v1);
    void (*UseProgram)(u32 program);
    void (*VertexAttribPointer)(u32 index, i32 size, u32 type, u8 normalized,
                                i32 stride, const void *pointer);
    void (*Viewport)(i32 x, i32 y, i32 w, i32 h);
} Mach_GLApi;

// A GPU texture and its pixel size (for sprite sizing and atlas UVs).
typedef struct {
    u32 id;
    f32 w, h;
} Mach_R2D_Texture;

// =============================================================================
// font: 8x8 bitmap font
// =============================================================================

// Bitmap font: 8x8 glyphs baked into a GL texture atlas for 2D text.


struct Mach_Renderer;

typedef struct {
    Mach_R2D_Texture atlas;        // RGBA: white glyph pixels with alpha; tinted per-vertex
    i32 glyph_w, glyph_h;     // glyph cell size in pixels
    i32 advance;              // horizontal step per character
} Mach_Font;

Mach_Font *font_create(struct Mach_Renderer *r);
void  font_destroy(struct Mach_Renderer *r, Mach_Font *font);

// Normalized atlas UVs for an ASCII character. Returns false for glyphs outside
// the printable range.
b32 font_glyph_uv(const Mach_Font *font, char ch, f32 *u0, f32 *v0, f32 *u1, f32 *v1);

// =============================================================================
// image: stb_image loading
// =============================================================================

// Mach_Image loading: PNG, BMP, JPG support via stb_image.


typedef struct {
    u8 *data;        // Raw pixel data (RGBA)
    i32 width;
    i32 height;
    i32 channels;    // Usually 4 (RGBA)
} Mach_Image;

// Load image from file. Returns image with allocated data, or zeroed struct on failure.
Mach_Image image_load(const char *path);

// Free image data.
void image_free(Mach_Image *img);

// =============================================================================
// render2d: the batch renderer
// =============================================================================

// 2D renderer over OpenGL 3.3 core: one batched stream of textured,
// vertex-colored triangles, plus primitives, text, sprites, and an isometric
// camera. See ARCHITECTURE.md for the design.


// Isometric tile footprint in screen pixels at zoom 1 (classic 2:1 diamond), and
// the screen height of one unit of block elevation.
#define ISO_TILE_W 64.0f
#define ISO_TILE_H 32.0f
#define ISO_ELEV   28.0f

// 2D camera over the isometric plane: an iso-space pan point centered on screen,
// and a zoom (pixels-per-iso-unit multiplier).
typedef struct {
    Mach_Vec2 pan;
    f32  zoom;
} Mach_Camera2D;

// One vertex of the batch. Everything — fills, text, sprites — draws through
// the same shader; untextured shapes sample a 1x1 white texture.
typedef struct {
    f32   x, y;   // window points; the shader maps to clip space
    f32   u, v;
    Mach_Color color;
} Mach_R2D_Vertex;

// Batch capacity. A flush costs one glDrawElements; overflowing mid-frame just
// splits the frame into more draws, so these only need to cover the common case.
#define R2D_MAX_VERTS   8192
#define R2D_MAX_INDICES 16384

typedef struct Mach_Renderer {
    RGFW_window *window;
    Mach_GLApi gl;              // loaded GL entry points (see gl.h)

    i32 width, height;         // logical render size (window points)
    i32 fb_width, fb_height;   // framebuffer size in pixels (differs under HiDPI)
    Mach_Font *font;                // bitmap font as a GL texture atlas

    // GL objects, created once at init.
    u32 program;
    u32 vao, vbo, ibo;
    i32 u_screen;              // uniform: logical size, for point -> clip mapping
    Mach_R2D_Texture white;         // 1x1 white, sampled by untextured draws

    // The pending batch: appended by the draw calls, flushed on texture change,
    // scissor change, overflow, or present.
    u32        batch_tex;      // texture the pending vertices sample
    i32        vert_count, index_count;
    Mach_R2D_Vertex verts[R2D_MAX_VERTS];
    u16        indices[R2D_MAX_INDICES];
} Mach_Renderer;

// Lifecycle. The window must already hold a current GL 3.3 core context.
b32  r2d_init(Mach_Renderer *r, RGFW_window *window);
void r2d_shutdown(Mach_Renderer *r);

// Re-read the window and framebuffer size. Call after the window is resized so
// render and input coordinates track the new size.
void r2d_resized(Mach_Renderer *r);

// Frame.
void r2d_begin(Mach_Renderer *r, Mach_Color clear);
void r2d_present(Mach_Renderer *r);

// Screen-space primitives. Colors are RGBA in [0,1]; see color.h for the palette.
void r2d_fill_rect(Mach_Renderer *r, f32 x, f32 y, f32 w, f32 h, Mach_Color color);
void r2d_fill_poly(Mach_Renderer *r, const Mach_Vec2 *pts, i32 n, Mach_Color color);  // convex, <=16 pts
void r2d_poly_outline(Mach_Renderer *r, const Mach_Vec2 *pts, i32 n, Mach_Color color);  // closed loop, <=16 pts
void r2d_text(Mach_Renderer *r, f32 x, f32 y, f32 scale, const char *text, Mach_Color color);

// Clip rect in window points (the UI's scissor). Draws between begin/end are
// clipped; nesting is not supported.
void r2d_clip_begin(Mach_Renderer *r, f32 x, f32 y, f32 w, f32 h);
void r2d_clip_end(Mach_Renderer *r);

// Textures and sprites (for real art later). Tint multiplies the texture; pass
// white for none. A zero id means the load failed.
Mach_R2D_Texture r2d_texture_from_pixels(Mach_Renderer *r, const void *rgba, i32 w, i32 h, b32 nearest);
Mach_R2D_Texture r2d_load_texture(Mach_Renderer *r, const char *path);
void r2d_destroy_texture(Mach_Renderer *r, Mach_R2D_Texture *tex);
void r2d_sprite(Mach_Renderer *r, Mach_R2D_Texture tex, f32 x, f32 y, f32 scale, Mach_Color tint);

// Isometric projection helpers (no Mach_Renderer needed). `elev` is block height in
// units; the inverse solves on the ground plane (elev 0).
Mach_Vec2 iso_to_screen(const Mach_Camera2D *cam, f32 screen_w, f32 screen_h,
                   f32 grid_x, f32 grid_y, f32 elev);
Mach_Vec2 screen_to_iso(const Mach_Camera2D *cam, f32 screen_w, f32 screen_h,
                   f32 screen_x, f32 screen_y);

// =============================================================================
// input: per-frame snapshot
// =============================================================================

// Per-frame input snapshot: keyboard and mouse state the game reads directly,
// instead of draining window events itself.
//
// The engine owns one of these (Mach_Engine.input) and fills it while draining the
// event queue in engine_frame_begin. "down" persists while a key or button is
// held; "pressed"/"released" mark this frame's edges and are cleared at the top
// of the next frame. Read fields directly: in->key_pressed[RGFW_key1].


typedef enum {
    MOUSE_LEFT = 0,
    MOUSE_RIGHT,
    MOUSE_MIDDLE,
    MOUSE_BUTTON_COUNT,
} Mach_Mouse_Button;

typedef struct {
    // Keyboard, indexed by RGFW_key. key_pressed excludes OS key repeats.
    u8 key_down[RGFW_keyLast];
    u8 key_pressed[RGFW_keyLast];

    // Mouse, in render coordinates (window points). wheel is this frame's scroll,
    // positive away from the user.
    Mach_Vec2 mouse;
    Mach_Vec2 mouse_delta;
    f32  wheel;
    u8   mouse_down[MOUSE_BUTTON_COUNT];
    u8   mouse_pressed[MOUSE_BUTTON_COUNT];
    u8   mouse_released[MOUSE_BUTTON_COUNT];

    u8 mouse_seen;  // internal: first motion event seeds mouse without a delta spike
} Mach_Input;

// Clear the per-frame edges (pressed/released/delta/wheel). Held state persists.
void input_frame_begin(Mach_Input *in);

// Fold one RGFW event into the snapshot. Events the snapshot doesn't model are ignored.
void input_handle_event(Mach_Input *in, const RGFW_event *ev);

// =============================================================================
// clay_ui: Clay layout bound to render2d
// =============================================================================

#include "clay.h"

// Clay UI binding: layout by Clay (third_party/clay), drawing by our 2D renderer.
//
// Clay does the UI layout and hands back a list of render commands; clay_ui_render
// walks that list and draws each with r2d. Clay keeps an internal "current context"
// global plus callback pointers, so those are re-pointed every frame in clay_ui_begin
// to survive hot reload (the reloaded library's globals start empty). The context data
// itself lives in a malloc'd block held in host-owned App memory, so it persists across
// a code swap.


typedef struct {
    Clay_Context *ctx;
    void         *memory;   // malloc backing for Clay's arena (survives hot reload)
    b32           ready;
} Mach_ClayUI;

// One-time setup: allocate Clay's arena and initialize it against the renderer's size.
b32  clay_ui_init(Mach_ClayUI *ui, Mach_Renderer *r);
void clay_ui_shutdown(Mach_ClayUI *ui);

// Per-frame. Call clay_ui_begin, declare the layout with CLAY(...) / CLAY_TEXT(...),
// then clay_ui_render to draw it. `mouse`/`mouse_down` feed Clay's pointer state for
// hover/click handling (pass zero/false when there's nothing interactive yet).
void clay_ui_begin(Mach_ClayUI *ui, Mach_Renderer *r, Clay_Vector2 mouse, b32 mouse_down);
void clay_ui_render(Mach_ClayUI *ui, Mach_Renderer *r);

// A Clay_String over a null-terminated C string. The chars are not copied, so they
// must outlive this frame's clay_ui_render call.
static inline Clay_String clay_string(const char *s) {
    return (Clay_String){ .length = (i32)strlen(s), .chars = s };
}

// An engine Mach_Color ([0,1] RGBA) in Clay's 0-255 convention, so the palette in
// render/color.h works for UI declarations too.
static inline Clay_Color clay_color_of(Mach_Color c) {
    return (Clay_Color){ c.x * 255.0f, c.y * 255.0f, c.z * 255.0f, c.w * 255.0f };
}

// =============================================================================
// core: engine lifecycle and the frame loop
// =============================================================================

// Core engine lifecycle and the per-frame steps of the loop.
//
// The game owns the loop (in main()) and calls these steps directly. The engine
// owns window lifecycle — engine_frame_begin drains the event queue, consuming
// quit/Escape/resize and folding everything else into the Mach_Input snapshot — and
// keeps the frame timing and soft frame cap.


// How the game wants the engine set up: the window, plus the policy the engine
// applies each frame. The game fills all of it — the engine has no defaults of
// its own — and passes it to engine_init.
typedef struct {
    const char *title;
    i32  width, height;
    b32  fullscreen;
    b32  resizable;

    Mach_Color clear_color;   // frame clear color (see render/color.h for the palette)
    b32   escape_quits;  // Escape closes the window (dev convenience); otherwise
                         // Escape reaches the game through the input snapshot
    i32   target_fps;    // soft frame cap; <= 0 leaves the frame rate uncapped
} Mach_Engine_Config;

typedef struct {
    RGFW_window *window;
    Mach_Renderer     r2d;    // 2D renderer (GL batch renderer + bitmap font)
    Mach_Input        input;  // per-frame input snapshot, filled by engine_frame_begin

    // Per-frame scratch: reset at every engine_frame_begin, so anything allocated
    // from it lives exactly one frame (sort buffers, transient strings). The
    // regions are reused, not freed, so steady-state allocation is malloc-free.
    Mach_Arena        frame_arena;

    i32          running;

    // Per-frame policy, copied out of Mach_Engine_Config at init.
    Mach_Color clear_color;
    b32   escape_quits;
    u32   frame_cap_ms;  // 0 = uncapped

    // Frame timing, persisted across loop iterations (the game owns the loop).
    u32 frame_start;       // tick at the current frame's start (for the cap)
    u32 last_frame_time;   // tick at the previous frame's start (for dt)
    u32 fps_timer;         // start of the current 1s FPS sampling window
    i32 frame_count;       // frames seen in the current window
    i32 fps;               // last completed window's frame count
} Mach_Engine;

// Lifecycle. The game supplies the configuration.
b32  engine_init(Mach_Engine *e, Mach_Engine_Config cfg);
void engine_shutdown(Mach_Engine *e);

// True until a quit is requested (window close or Escape).
b32  engine_running(Mach_Engine *e);

// Per-frame steps, called in order by the game's loop:
//   f32 dt = engine_frame_begin(e);   // drain events into e->input, return delta time
//   ... game update (dt, reads e->input) ...
//   if (engine_render_begin(e)) {     // clear the frame (false => skip)
//       ... game render ...
//       engine_render_end(e);         // present
//   }
//   engine_frame_end(e);              // FPS bookkeeping + frame cap
f32  engine_frame_begin(Mach_Engine *e);
b32  engine_render_begin(Mach_Engine *e);
void engine_render_end(Mach_Engine *e);
void engine_frame_end(Mach_Engine *e);

// Frames per second over the last completed 1-second window, for the game to display.
i32  engine_fps(const Mach_Engine *e);

// Monotonic milliseconds from an arbitrary origin; wraps every ~49 days, so
// only differences are meaningful.
u32  engine_ticks_ms(void);


#endif // MACH_H



// ///////////////////////////////////////////////////////////////////////////////
//
//                                IMPLEMENTATION
//
// ///////////////////////////////////////////////////////////////////////////////

#if defined(MACH_IMPLEMENTATION) && !defined(MACH_IMPLEMENTATION_INCLUDED)
#define MACH_IMPLEMENTATION_INCLUDED

// =============================================================================
// mem: arena allocator
// =============================================================================

// Mach_Arena allocator implementation (MACH_IMPLEMENTATION).

#include <stdlib.h>

// (npt): Default region size in words. 8K words is 64 KiB on a 64-bit target —
// big enough that most arenas live in one region, small enough to not over-commit.
#define ARENA_REGION_CAPACITY (8 * 1024)

static Mach_Arena_Region *region_new(usize capacity) {
    usize bytes = sizeof(Mach_Arena_Region) + sizeof(uintptr_t) * capacity;
    Mach_Arena_Region *r = (Mach_Arena_Region *)malloc(bytes);
    if (!r) {
        LOG_ERROR("arena: region allocation failed (%zu bytes)", bytes);
        return NULL;
    }
    r->next = NULL;
    r->count = 0;
    r->capacity = capacity;
    return r;
}

void *arena_alloc(Mach_Arena *a, usize size) {
    // (npt): Round the byte request up to whole words so the next allocation
    // starts word-aligned too.
    usize words = (size + sizeof(uintptr_t) - 1) / sizeof(uintptr_t);

    if (a->end == NULL) {
        usize capacity = words > ARENA_REGION_CAPACITY ? words : ARENA_REGION_CAPACITY;
        a->begin = a->end = region_new(capacity);
        if (!a->end) return NULL;
    }

    // After a reset, `end` points at the first region; walk forward over any
    // already-full regions before deciding to allocate a new one.
    while (a->end->count + words > a->end->capacity && a->end->next != NULL) {
        a->end = a->end->next;
    }
    if (a->end->count + words > a->end->capacity) {
        usize capacity = words > ARENA_REGION_CAPACITY ? words : ARENA_REGION_CAPACITY;
        a->end->next = region_new(capacity);
        a->end = a->end->next;
        if (!a->end) return NULL;
    }

    void *result = &a->end->data[a->end->count];
    a->end->count += words;
    return result;
}

void arena_reset(Mach_Arena *a) {
    for (Mach_Arena_Region *r = a->begin; r != NULL; r = r->next) {
        r->count = 0;
    }
    a->end = a->begin;
}

void arena_free(Mach_Arena *a) {
    Mach_Arena_Region *r = a->begin;
    while (r != NULL) {
        Mach_Arena_Region *next = r->next;
        free(r);
        r = next;
    }
    a->begin = NULL;
    a->end = NULL;
}

// =============================================================================
// math
// =============================================================================

// Math implementation (MACH_IMPLEMENTATION).

#include <math.h>

f32 math_min(f32 a, f32 b) { return a < b ? a : b; }
f32 math_max(f32 a, f32 b) { return a > b ? a : b; }
f32 math_lerp(f32 a, f32 b, f32 t) { return a + (b - a) * t; }

f32 math_clamp(f32 v, f32 lo, f32 hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

Mach_Vec2 vec2_add(Mach_Vec2 a, Mach_Vec2 b)  { return (Mach_Vec2){a.x + b.x, a.y + b.y}; }
Mach_Vec2 vec2_sub(Mach_Vec2 a, Mach_Vec2 b)  { return (Mach_Vec2){a.x - b.x, a.y - b.y}; }
Mach_Vec2 vec2_scale(Mach_Vec2 v, f32 s) { return (Mach_Vec2){v.x * s, v.y * s}; }
f32  vec2_dot(Mach_Vec2 a, Mach_Vec2 b)  { return a.x * b.x + a.y * b.y; }
f32  vec2_length(Mach_Vec2 v)       { return sqrtf(vec2_dot(v, v)); }

Mach_Vec2 vec2_normalize(Mach_Vec2 v) {
    f32 len = vec2_length(v);
    if (len == 0.0f) return (Mach_Vec2){0.0f, 0.0f};
    return vec2_scale(v, 1.0f / len);
}

Mach_Vec2 vec2_lerp(Mach_Vec2 a, Mach_Vec2 b, f32 t) {
    return (Mach_Vec2){math_lerp(a.x, b.x, t), math_lerp(a.y, b.y, t)};
}

// =============================================================================
// image (stb_image implementation lives here)
// =============================================================================

// Mach_Image implementation using stb_image (MACH_IMPLEMENTATION).

#include <stdlib.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

Mach_Image image_load(const char *path) {
    Mach_Image img = {0};
    if (!path) return img;

    int w, h, channels;
    u8 *data = stbi_load(path, &w, &h, &channels, 4);  // Force RGBA
    if (!data) return img;

    img.data = data;
    img.width = w;
    img.height = h;
    img.channels = 4;
    return img;
}

void image_free(Mach_Image *img) {
    if (!img || !img->data) return;
    stbi_image_free(img->data);
    img->data = NULL;
    img->width = 0;
    img->height = 0;
}

// =============================================================================
// font
// =============================================================================

// Mach_Font implementation: 8x8 glyphs baked into a GL texture atlas (included into
// mach.c).
//
// Glyphs are stored as a static bit table, then expanded into an RGBA atlas
// texture at startup (white pixels with alpha; transparent elsewhere). Text is
// drawn by batching glyph quads with a per-vertex color for tint.

#include <stdlib.h>
#include <string.h>

#define FONT_FIRST_CHAR 32
#define FONT_LAST_CHAR  126
#define FONT_GLYPH_COUNT (FONT_LAST_CHAR - FONT_FIRST_CHAR + 1)  // 95

#define CELL      8
#define ATLAS_COLS 16
#define ATLAS_ROWS 6
#define ATLAS_W   (ATLAS_COLS * CELL)  // 128
#define ATLAS_H   (ATLAS_ROWS * CELL)  // 48

// 8x8 glyphs indexed by (ascii - FONT_FIRST_CHAR). MSB = leftmost pixel.
static const u8 GLYPHS[FONT_GLYPH_COUNT][CELL] = {
    [' ' - FONT_FIRST_CHAR] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    ['$' - FONT_FIRST_CHAR] = {0x18, 0x3E, 0x58, 0x3C, 0x1A, 0x7C, 0x18, 0x00},
    ['(' - FONT_FIRST_CHAR] = {0x0C, 0x18, 0x30, 0x30, 0x30, 0x18, 0x0C, 0x00},
    [')' - FONT_FIRST_CHAR] = {0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x18, 0x30, 0x00},
    ['+' - FONT_FIRST_CHAR] = {0x00, 0x18, 0x18, 0x7E, 0x18, 0x18, 0x00, 0x00},
    [',' - FONT_FIRST_CHAR] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x30},
    ['-' - FONT_FIRST_CHAR] = {0x00, 0x00, 0x00, 0x7E, 0x00, 0x00, 0x00, 0x00},
    ['.' - FONT_FIRST_CHAR] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00},
    ['%' - FONT_FIRST_CHAR] = {0x62, 0x66, 0x0C, 0x18, 0x30, 0x66, 0x46, 0x00},
    ['/' - FONT_FIRST_CHAR] = {0x06, 0x0C, 0x18, 0x18, 0x30, 0x60, 0xC0, 0x00},
    ['!' - FONT_FIRST_CHAR] = {0x18, 0x18, 0x18, 0x18, 0x18, 0x00, 0x18, 0x00},
    ['0' - FONT_FIRST_CHAR] = {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['1' - FONT_FIRST_CHAR] = {0x18, 0x38, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['2' - FONT_FIRST_CHAR] = {0x3C, 0x66, 0x06, 0x0C, 0x18, 0x30, 0x7E, 0x00},
    ['3' - FONT_FIRST_CHAR] = {0x3C, 0x66, 0x06, 0x1C, 0x06, 0x66, 0x3C, 0x00},
    ['4' - FONT_FIRST_CHAR] = {0x0C, 0x1C, 0x3C, 0x6C, 0x7E, 0x0C, 0x0C, 0x00},
    ['5' - FONT_FIRST_CHAR] = {0x7E, 0x60, 0x7C, 0x06, 0x06, 0x66, 0x3C, 0x00},
    ['6' - FONT_FIRST_CHAR] = {0x3C, 0x66, 0x60, 0x7C, 0x66, 0x66, 0x3C, 0x00},
    ['7' - FONT_FIRST_CHAR] = {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x60, 0x00},
    ['8' - FONT_FIRST_CHAR] = {0x3C, 0x66, 0x66, 0x3C, 0x66, 0x66, 0x3C, 0x00},
    ['9' - FONT_FIRST_CHAR] = {0x3C, 0x66, 0x66, 0x3E, 0x06, 0x66, 0x3C, 0x00},
    [':' - FONT_FIRST_CHAR] = {0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00},
    ['A' - FONT_FIRST_CHAR] = {0x3C, 0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x00},
    ['B' - FONT_FIRST_CHAR] = {0x7C, 0x66, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00},
    ['C' - FONT_FIRST_CHAR] = {0x3C, 0x66, 0x60, 0x60, 0x60, 0x66, 0x3C, 0x00},
    ['D' - FONT_FIRST_CHAR] = {0x78, 0x6C, 0x66, 0x66, 0x66, 0x6C, 0x78, 0x00},
    ['E' - FONT_FIRST_CHAR] = {0x7E, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x7E, 0x00},
    ['F' - FONT_FIRST_CHAR] = {0x7E, 0x60, 0x7C, 0x60, 0x60, 0x60, 0x60, 0x00},
    ['G' - FONT_FIRST_CHAR] = {0x3C, 0x66, 0x60, 0x6E, 0x66, 0x66, 0x3C, 0x00},
    ['H' - FONT_FIRST_CHAR] = {0x66, 0x66, 0x7E, 0x66, 0x66, 0x66, 0x66, 0x00},
    ['I' - FONT_FIRST_CHAR] = {0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['J' - FONT_FIRST_CHAR] = {0x0E, 0x06, 0x06, 0x06, 0x66, 0x66, 0x3C, 0x00},
    ['K' - FONT_FIRST_CHAR] = {0x66, 0x6C, 0x78, 0x70, 0x78, 0x6C, 0x66, 0x00},
    ['L' - FONT_FIRST_CHAR] = {0x60, 0x60, 0x60, 0x60, 0x60, 0x60, 0x7E, 0x00},
    ['M' - FONT_FIRST_CHAR] = {0x63, 0x77, 0x7F, 0x6B, 0x63, 0x63, 0x63, 0x00},
    ['N' - FONT_FIRST_CHAR] = {0x66, 0x76, 0x7E, 0x7E, 0x6E, 0x66, 0x66, 0x00},
    ['O' - FONT_FIRST_CHAR] = {0x3C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['P' - FONT_FIRST_CHAR] = {0x7C, 0x66, 0x7C, 0x60, 0x60, 0x60, 0x60, 0x00},
    ['Q' - FONT_FIRST_CHAR] = {0x3C, 0x66, 0x66, 0x66, 0x6E, 0x7C, 0x06, 0x00},
    ['R' - FONT_FIRST_CHAR] = {0x7C, 0x66, 0x66, 0x7C, 0x78, 0x6C, 0x66, 0x00},
    ['S' - FONT_FIRST_CHAR] = {0x3C, 0x66, 0x60, 0x3C, 0x06, 0x66, 0x3C, 0x00},
    ['T' - FONT_FIRST_CHAR] = {0x7E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
    ['U' - FONT_FIRST_CHAR] = {0x66, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['V' - FONT_FIRST_CHAR] = {0x66, 0x66, 0x66, 0x66, 0x3C, 0x3C, 0x18, 0x00},
    ['W' - FONT_FIRST_CHAR] = {0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x63, 0x00},
    ['X' - FONT_FIRST_CHAR] = {0x66, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00},
    ['Y' - FONT_FIRST_CHAR] = {0x66, 0x66, 0x3C, 0x18, 0x18, 0x18, 0x18, 0x00},
    ['Z' - FONT_FIRST_CHAR] = {0x7E, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00},
    ['a' - FONT_FIRST_CHAR] = {0x00, 0x3C, 0x06, 0x3E, 0x66, 0x66, 0x3C, 0x00},
    ['b' - FONT_FIRST_CHAR] = {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x7C, 0x00},
    ['c' - FONT_FIRST_CHAR] = {0x00, 0x3C, 0x60, 0x60, 0x60, 0x60, 0x3C, 0x00},
    ['d' - FONT_FIRST_CHAR] = {0x06, 0x06, 0x3E, 0x66, 0x66, 0x66, 0x3E, 0x00},
    ['e' - FONT_FIRST_CHAR] = {0x00, 0x3C, 0x66, 0x7E, 0x60, 0x66, 0x3C, 0x00},
    ['f' - FONT_FIRST_CHAR] = {0x1C, 0x30, 0x7E, 0x30, 0x30, 0x30, 0x30, 0x00},
    ['g' - FONT_FIRST_CHAR] = {0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x3C, 0x00},
    ['h' - FONT_FIRST_CHAR] = {0x60, 0x60, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x00},
    ['i' - FONT_FIRST_CHAR] = {0x18, 0x00, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['j' - FONT_FIRST_CHAR] = {0x0C, 0x00, 0x0C, 0x0C, 0x0C, 0x6C, 0x38, 0x00},
    ['k' - FONT_FIRST_CHAR] = {0x60, 0x60, 0x66, 0x6C, 0x78, 0x6C, 0x66, 0x00},
    ['l' - FONT_FIRST_CHAR] = {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x00},
    ['m' - FONT_FIRST_CHAR] = {0x00, 0x7C, 0xA6, 0x92, 0x92, 0x82, 0x82, 0x00},
    ['n' - FONT_FIRST_CHAR] = {0x00, 0x7C, 0x66, 0x66, 0x66, 0x66, 0x66, 0x00},
    ['o' - FONT_FIRST_CHAR] = {0x00, 0x3C, 0x66, 0x66, 0x66, 0x66, 0x3C, 0x00},
    ['p' - FONT_FIRST_CHAR] = {0x00, 0x7C, 0x66, 0x66, 0x7C, 0x60, 0x60, 0x00},
    ['q' - FONT_FIRST_CHAR] = {0x00, 0x3E, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x00},
    ['r' - FONT_FIRST_CHAR] = {0x00, 0x7C, 0x66, 0x60, 0x60, 0x60, 0x60, 0x00},
    ['s' - FONT_FIRST_CHAR] = {0x00, 0x3C, 0x60, 0x3C, 0x06, 0x06, 0x3C, 0x00},
    ['t' - FONT_FIRST_CHAR] = {0x30, 0x7E, 0x30, 0x30, 0x30, 0x30, 0x1C, 0x00},
    ['u' - FONT_FIRST_CHAR] = {0x00, 0x66, 0x66, 0x66, 0x66, 0x66, 0x3E, 0x00},
    ['v' - FONT_FIRST_CHAR] = {0x00, 0x66, 0x66, 0x66, 0x3C, 0x3C, 0x18, 0x00},
    ['w' - FONT_FIRST_CHAR] = {0x00, 0x63, 0x63, 0x6B, 0x7F, 0x37, 0x63, 0x00},
    ['x' - FONT_FIRST_CHAR] = {0x00, 0x66, 0x3C, 0x18, 0x3C, 0x66, 0x66, 0x00},
    ['y' - FONT_FIRST_CHAR] = {0x00, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x3C, 0x00},
    ['z' - FONT_FIRST_CHAR] = {0x00, 0x7E, 0x0C, 0x18, 0x30, 0x60, 0x7E, 0x00},
};

// Expand the bit table into an RGBA pixel buffer (caller frees). Set bits become
// opaque white; everything else is transparent.
static u32 *font_build_pixels(void) {
    u32 *px = (u32 *)calloc(ATLAS_W * ATLAS_H, sizeof(u32));
    if (!px) return NULL;

    for (i32 idx = 0; idx < FONT_GLYPH_COUNT; idx++) {
        i32 cx = (idx % ATLAS_COLS) * CELL;
        i32 cy = (idx / ATLAS_COLS) * CELL;
        for (i32 r = 0; r < CELL; r++) {
            u8 bits = GLYPHS[idx][r];
            for (i32 c = 0; c < CELL; c++) {
                if (bits & (0x80 >> c)) px[(cy + r) * ATLAS_W + (cx + c)] = 0xFFFFFFFFu;
            }
        }
    }
    return px;
}

Mach_Font *font_create(struct Mach_Renderer *r) {
    Mach_Font *font = (Mach_Font *)calloc(1, sizeof(Mach_Font));
    if (!font) return NULL;

    font->glyph_w = CELL;
    font->glyph_h = CELL;
    font->advance = CELL + 1;

    u32 *px = font_build_pixels();
    if (!px) { free(font); return NULL; }

    font->atlas = r2d_texture_from_pixels((Mach_Renderer *)r, px, ATLAS_W, ATLAS_H, MACH_TRUE);
    free(px);
    if (!font->atlas.id) {
        LOG_ERROR("font_create: atlas texture creation failed");
        free(font);
        return NULL;
    }

    LOG_INFO("font atlas created (%dx%d RGBA, %d glyphs)", ATLAS_W, ATLAS_H, FONT_GLYPH_COUNT);
    return font;
}

void font_destroy(struct Mach_Renderer *r, Mach_Font *font) {
    if (!font) return;
    r2d_destroy_texture((Mach_Renderer *)r, &font->atlas);
    free(font);
}

b32 font_glyph_uv(const Mach_Font *font, char ch, f32 *u0, f32 *v0, f32 *u1, f32 *v1) {
    (void)font;
    u8 c = (u8)ch;
    if (c < FONT_FIRST_CHAR || c > FONT_LAST_CHAR) return MACH_FALSE;
    i32 idx = c - FONT_FIRST_CHAR;
    f32 x = (f32)((idx % ATLAS_COLS) * CELL);
    f32 y = (f32)((idx / ATLAS_COLS) * CELL);
    *u0 = x / (f32)ATLAS_W;
    *v0 = y / (f32)ATLAS_H;
    *u1 = (x + (f32)CELL) / (f32)ATLAS_W;
    *v1 = (y + (f32)CELL) / (f32)ATLAS_H;
    return MACH_TRUE;
}

// =============================================================================
// render2d
// =============================================================================

// 2D renderer implementation over OpenGL 3.3 core (MACH_IMPLEMENTATION).
//
// Everything draws through one shader as batched, textured, vertex-colored
// triangles. The draw calls append to a CPU-side batch that flushes to a single
// glDrawElements whenever the texture or scissor changes, the batch fills, or
// the frame presents.


// --- Shader -------------------------------------------------------------------

// Positions arrive in window points; u_screen maps them to clip space (y down).
static const char *R2D_VERT_SRC =
    "#version 330 core\n"
    "layout(location = 0) in vec2 a_pos;\n"
    "layout(location = 1) in vec2 a_uv;\n"
    "layout(location = 2) in vec4 a_color;\n"
    "uniform vec2 u_screen;\n"
    "out vec2 v_uv;\n"
    "out vec4 v_color;\n"
    "void main() {\n"
    "    vec2 ndc = vec2(a_pos.x * 2.0 / u_screen.x - 1.0,\n"
    "                    1.0 - a_pos.y * 2.0 / u_screen.y);\n"
    "    gl_Position = vec4(ndc, 0.0, 1.0);\n"
    "    v_uv = a_uv;\n"
    "    v_color = a_color;\n"
    "}\n";

static const char *R2D_FRAG_SRC =
    "#version 330 core\n"
    "in vec2 v_uv;\n"
    "in vec4 v_color;\n"
    "uniform sampler2D u_tex;\n"
    "out vec4 frag;\n"
    "void main() { frag = texture(u_tex, v_uv) * v_color; }\n";

// --- GL loading ---------------------------------------------------------------

static b32 r2d_gl_load(Mach_GLApi *gl) {
    b32 ok = MACH_TRUE;
#define GL_LOAD(name) do { \
        *(void **)(&gl->name) = (void *)RGFW_getProcAddress_OpenGL("gl" #name); \
        if (!gl->name) { LOG_ERROR("gl load: missing gl%s", #name); ok = MACH_FALSE; } \
    } while (0)
    GL_LOAD(ActiveTexture);
    GL_LOAD(AttachShader);
    GL_LOAD(BindBuffer);
    GL_LOAD(BindTexture);
    GL_LOAD(BindVertexArray);
    GL_LOAD(BlendFunc);
    GL_LOAD(BufferData);
    GL_LOAD(BufferSubData);
    GL_LOAD(Clear);
    GL_LOAD(ClearColor);
    GL_LOAD(CompileShader);
    GL_LOAD(CreateProgram);
    GL_LOAD(CreateShader);
    GL_LOAD(DeleteBuffers);
    GL_LOAD(DeleteProgram);
    GL_LOAD(DeleteShader);
    GL_LOAD(DeleteTextures);
    GL_LOAD(DeleteVertexArrays);
    GL_LOAD(Disable);
    GL_LOAD(DrawElements);
    GL_LOAD(Enable);
    GL_LOAD(EnableVertexAttribArray);
    GL_LOAD(GenBuffers);
    GL_LOAD(GenTextures);
    GL_LOAD(GenVertexArrays);
    GL_LOAD(GetProgramInfoLog);
    GL_LOAD(GetProgramiv);
    GL_LOAD(GetShaderInfoLog);
    GL_LOAD(GetShaderiv);
    GL_LOAD(GetString);
    GL_LOAD(GetUniformLocation);
    GL_LOAD(LinkProgram);
    GL_LOAD(PixelStorei);
    GL_LOAD(Scissor);
    GL_LOAD(ShaderSource);
    GL_LOAD(TexImage2D);
    GL_LOAD(TexParameteri);
    GL_LOAD(Uniform1i);
    GL_LOAD(Uniform2f);
    GL_LOAD(UseProgram);
    GL_LOAD(VertexAttribPointer);
    GL_LOAD(Viewport);
#undef GL_LOAD
    return ok;
}

static u32 r2d_compile_shader(const Mach_GLApi *gl, u32 type, const char *src) {
    u32 shader = gl->CreateShader(type);
    gl->ShaderSource(shader, 1, &src, NULL);
    gl->CompileShader(shader);
    i32 status = 0;
    gl->GetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status) {
        char log[512];
        gl->GetShaderInfoLog(shader, sizeof(log), NULL, log);
        LOG_ERROR("shader compile failed: %s", log);
        gl->DeleteShader(shader);
        return 0;
    }
    return shader;
}

// --- Lifecycle ------------------------------------------------------------------

// Cache the window size (points) and framebuffer size (pixels), and point the
// viewport at the full framebuffer. Render and mouse coordinates share the
// window-point space; GL scales to the HiDPI framebuffer via the viewport.
static void r2d_apply_window_size(Mach_Renderer *r) {
    RGFW_window_getSize(r->window, &r->width, &r->height);
    r->fb_width = r->width;
    r->fb_height = r->height;
    RGFW_window_getSizeInPixels(r->window, &r->fb_width, &r->fb_height);
    r->gl.Viewport(0, 0, r->fb_width, r->fb_height);
}

b32 r2d_init(Mach_Renderer *r, RGFW_window *window) {
    r->window = window;

    if (!r2d_gl_load(&r->gl)) return MACH_FALSE;
    const Mach_GLApi *gl = &r->gl;

    u32 vs = r2d_compile_shader(gl, GL_VERTEX_SHADER, R2D_VERT_SRC);
    u32 fs = r2d_compile_shader(gl, GL_FRAGMENT_SHADER, R2D_FRAG_SRC);
    if (!vs || !fs) return MACH_FALSE;
    r->program = gl->CreateProgram();
    gl->AttachShader(r->program, vs);
    gl->AttachShader(r->program, fs);
    gl->LinkProgram(r->program);
    gl->DeleteShader(vs);
    gl->DeleteShader(fs);
    i32 status = 0;
    gl->GetProgramiv(r->program, GL_LINK_STATUS, &status);
    if (!status) {
        char log[512];
        gl->GetProgramInfoLog(r->program, sizeof(log), NULL, log);
        LOG_ERROR("program link failed: %s", log);
        return MACH_FALSE;
    }
    r->u_screen = gl->GetUniformLocation(r->program, "u_screen");
    gl->UseProgram(r->program);
    gl->Uniform1i(gl->GetUniformLocation(r->program, "u_tex"), 0);

    // One VAO/VBO/IBO for the batch, allocated to capacity once and refilled
    // per flush with BufferSubData.
    gl->GenVertexArrays(1, &r->vao);
    gl->BindVertexArray(r->vao);
    gl->GenBuffers(1, &r->vbo);
    gl->BindBuffer(GL_ARRAY_BUFFER, r->vbo);
    gl->BufferData(GL_ARRAY_BUFFER, (isize)sizeof(r->verts), NULL, GL_STREAM_DRAW);
    gl->GenBuffers(1, &r->ibo);
    gl->BindBuffer(GL_ELEMENT_ARRAY_BUFFER, r->ibo);
    gl->BufferData(GL_ELEMENT_ARRAY_BUFFER, (isize)sizeof(r->indices), NULL, GL_STREAM_DRAW);
    gl->EnableVertexAttribArray(0);
    gl->VertexAttribPointer(0, 2, GL_FLOAT, 0, (i32)sizeof(Mach_R2D_Vertex), (void *)0);
    gl->EnableVertexAttribArray(1);
    gl->VertexAttribPointer(1, 2, GL_FLOAT, 0, (i32)sizeof(Mach_R2D_Vertex),
                            (void *)(2 * sizeof(f32)));
    gl->EnableVertexAttribArray(2);
    gl->VertexAttribPointer(2, 4, GL_FLOAT, 0, (i32)sizeof(Mach_R2D_Vertex),
                            (void *)(4 * sizeof(f32)));

    gl->Enable(GL_BLEND);
    gl->BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl->ActiveTexture(GL_TEXTURE0);

    u32 white_px = 0xFFFFFFFFu;
    r->white = r2d_texture_from_pixels(r, &white_px, 1, 1, MACH_TRUE);
    if (!r->white.id) return MACH_FALSE;
    r->batch_tex = r->white.id;

    r2d_apply_window_size(r);

    r->font = font_create(r);
    if (!r->font) return MACH_FALSE;

    LOG_INFO("2D renderer ready (%dx%d points, %dx%d px, GL %s)", r->width, r->height,
             r->fb_width, r->fb_height, (const char *)gl->GetString(GL_VERSION));
    return MACH_TRUE;
}

void r2d_shutdown(Mach_Renderer *r) {
    const Mach_GLApi *gl = &r->gl;
    if (r->font) { font_destroy(r, r->font); r->font = NULL; }
    if (r->white.id) r2d_destroy_texture(r, &r->white);
    if (r->program) { gl->DeleteProgram(r->program); r->program = 0; }
    if (r->vbo) { gl->DeleteBuffers(1, &r->vbo); r->vbo = 0; }
    if (r->ibo) { gl->DeleteBuffers(1, &r->ibo); r->ibo = 0; }
    if (r->vao) { gl->DeleteVertexArrays(1, &r->vao); r->vao = 0; }
    LOG_INFO("2D renderer shut down");
}

void r2d_resized(Mach_Renderer *r) {
    r2d_apply_window_size(r);
    LOG_DEBUG("window resized to %dx%d", r->width, r->height);
}

// --- Batch ----------------------------------------------------------------------

static void r2d_flush(Mach_Renderer *r) {
    if (r->index_count == 0) return;
    const Mach_GLApi *gl = &r->gl;
    gl->BufferSubData(GL_ARRAY_BUFFER, 0,
                      (isize)((usize)r->vert_count * sizeof(Mach_R2D_Vertex)), r->verts);
    gl->BufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0,
                      (isize)((usize)r->index_count * sizeof(u16)), r->indices);
    gl->BindTexture(GL_TEXTURE_2D, r->batch_tex);
    gl->DrawElements(GL_TRIANGLES, r->index_count, GL_UNSIGNED_SHORT, 0);
    r->vert_count = 0;
    r->index_count = 0;
}

// Reserve batch space for nverts/nindices drawing with `tex`, flushing first if
// the texture changes or the batch would overflow. Returns the base vertex
// index; the caller writes verts at verts[base + i] and absolute indices.
static i32 r2d_reserve(Mach_Renderer *r, u32 tex, i32 nverts, i32 nindices) {
    if (tex != r->batch_tex || r->vert_count + nverts > R2D_MAX_VERTS ||
        r->index_count + nindices > R2D_MAX_INDICES) {
        r2d_flush(r);
        r->batch_tex = tex;
    }
    return r->vert_count;
}

static Mach_R2D_Vertex r2d_vertex(f32 x, f32 y, f32 u, f32 v, Mach_Color c) {
    Mach_R2D_Vertex vert;
    vert.x = x; vert.y = y;
    vert.u = u; vert.v = v;
    vert.color = c;
    return vert;
}

// Append one textured axis-aligned quad.
static void r2d_quad(Mach_Renderer *r, u32 tex, f32 x, f32 y, f32 w, f32 h,
                     f32 u0, f32 v0, f32 u1, f32 v1, Mach_Color c) {
    i32 base = r2d_reserve(r, tex, 4, 6);
    r->verts[base + 0] = r2d_vertex(x,     y,     u0, v0, c);
    r->verts[base + 1] = r2d_vertex(x + w, y,     u1, v0, c);
    r->verts[base + 2] = r2d_vertex(x + w, y + h, u1, v1, c);
    r->verts[base + 3] = r2d_vertex(x,     y + h, u0, v1, c);
    u16 b = (u16)base;
    u16 *ix = r->indices + r->index_count;
    ix[0] = b; ix[1] = (u16)(b + 1); ix[2] = (u16)(b + 2);
    ix[3] = b; ix[4] = (u16)(b + 2); ix[5] = (u16)(b + 3);
    r->vert_count += 4;
    r->index_count += 6;
}

// --- Frame ------------------------------------------------------------------

void r2d_begin(Mach_Renderer *r, Mach_Color clear) {
    const Mach_GLApi *gl = &r->gl;
    gl->ClearColor(clear.x, clear.y, clear.z, clear.w);
    gl->Clear(GL_COLOR_BUFFER_BIT);
    gl->UseProgram(r->program);
    gl->Uniform2f(r->u_screen, (f32)r->width, (f32)r->height);
    gl->BindVertexArray(r->vao);
    gl->BindBuffer(GL_ARRAY_BUFFER, r->vbo);
    r->vert_count = 0;
    r->index_count = 0;
    r->batch_tex = r->white.id;
}

void r2d_present(Mach_Renderer *r) {
    r2d_flush(r);
    RGFW_window_swapBuffers_OpenGL(r->window);
}

// --- Primitives -------------------------------------------------------------

void r2d_fill_rect(Mach_Renderer *r, f32 x, f32 y, f32 w, f32 h, Mach_Color color) {
    r2d_quad(r, r->white.id, x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, color);
}

// Fill a convex polygon as a triangle fan.
void r2d_fill_poly(Mach_Renderer *r, const Mach_Vec2 *pts, i32 n, Mach_Color color) {
    if (n < 3 || n > 16) return;
    i32 base = r2d_reserve(r, r->white.id, n, (n - 2) * 3);
    for (i32 i = 0; i < n; i++) {
        r->verts[base + i] = r2d_vertex(pts[i].x, pts[i].y, 0.0f, 0.0f, color);
    }
    u16 *ix = r->indices + r->index_count;
    for (i32 i = 1; i < n - 1; i++) {
        *ix++ = (u16)base;
        *ix++ = (u16)(base + i);
        *ix++ = (u16)(base + i + 1);
    }
    r->vert_count += n;
    r->index_count += (n - 2) * 3;
}

// Stroke a closed polygon with 1-point-thick edge quads (core GL has no
// reliable line width, so edges are geometry like everything else).
void r2d_poly_outline(Mach_Renderer *r, const Mach_Vec2 *pts, i32 n, Mach_Color color) {
    if (n < 2 || n > 16) return;
    for (i32 i = 0; i < n; i++) {
        Mach_Vec2 p = pts[i];
        Mach_Vec2 q = pts[(i + 1) % n];
        Mach_Vec2 d = vec2_sub(q, p);
        f32 len = vec2_length(d);
        if (len < 0.0001f) continue;
        // Half-thickness normal on each side of the edge.
        f32 nx = -d.y / len * 0.5f;
        f32 ny = d.x / len * 0.5f;
        i32 base = r2d_reserve(r, r->white.id, 4, 6);
        r->verts[base + 0] = r2d_vertex(p.x + nx, p.y + ny, 0.0f, 0.0f, color);
        r->verts[base + 1] = r2d_vertex(q.x + nx, q.y + ny, 0.0f, 0.0f, color);
        r->verts[base + 2] = r2d_vertex(q.x - nx, q.y - ny, 0.0f, 0.0f, color);
        r->verts[base + 3] = r2d_vertex(p.x - nx, p.y - ny, 0.0f, 0.0f, color);
        u16 b = (u16)base;
        u16 *ix = r->indices + r->index_count;
        ix[0] = b; ix[1] = (u16)(b + 1); ix[2] = (u16)(b + 2);
        ix[3] = b; ix[4] = (u16)(b + 2); ix[5] = (u16)(b + 3);
        r->vert_count += 4;
        r->index_count += 6;
    }
}

void r2d_text(Mach_Renderer *r, f32 x, f32 y, f32 scale, const char *text, Mach_Color color) {
    if (!text) return;
    Mach_Font *font = r->font;
    f32 gw = (f32)font->glyph_w * scale;
    f32 gh = (f32)font->glyph_h * scale;
    f32 adv = (f32)font->advance * scale;
    f32 cur_x = x;
    for (const char *c = text; *c; c++) {
        f32 u0, v0, u1, v1;
        if (font_glyph_uv(font, *c, &u0, &v0, &u1, &v1)) {
            r2d_quad(r, font->atlas.id, cur_x, y, gw, gh, u0, v0, u1, v1, color);
        }
        cur_x += adv;
    }
}

// --- Clip ---------------------------------------------------------------------

// glScissor works in framebuffer pixels with a bottom-left origin, so convert
// from window points (y down) and scale for HiDPI.
void r2d_clip_begin(Mach_Renderer *r, f32 x, f32 y, f32 w, f32 h) {
    r2d_flush(r);
    f32 sx = (f32)r->fb_width / (f32)r->width;
    f32 sy = (f32)r->fb_height / (f32)r->height;
    i32 px = (i32)(x * sx);
    i32 py = r->fb_height - (i32)((y + h) * sy);
    i32 pw = (i32)(w * sx);
    i32 ph = (i32)(h * sy);
    if (pw < 0) pw = 0;
    if (ph < 0) ph = 0;
    r->gl.Enable(GL_SCISSOR_TEST);
    r->gl.Scissor(px, py, pw, ph);
}

void r2d_clip_end(Mach_Renderer *r) {
    r2d_flush(r);
    r->gl.Disable(GL_SCISSOR_TEST);
}

// --- Textures and sprites -----------------------------------------------------

Mach_R2D_Texture r2d_texture_from_pixels(Mach_Renderer *r, const void *rgba, i32 w, i32 h, b32 nearest) {
    const Mach_GLApi *gl = &r->gl;
    Mach_R2D_Texture tex = {0};
    gl->GenTextures(1, &tex.id);
    if (!tex.id) {
        LOG_ERROR("r2d_texture_from_pixels: glGenTextures failed");
        return tex;
    }
    tex.w = (f32)w;
    tex.h = (f32)h;
    i32 filter = nearest ? GL_NEAREST : GL_LINEAR;
    gl->BindTexture(GL_TEXTURE_2D, tex.id);
    gl->PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

Mach_R2D_Texture r2d_load_texture(Mach_Renderer *r, const char *path) {
    Mach_R2D_Texture tex = {0};
    Mach_Image img = image_load(path);
    if (!img.data) {
        LOG_ERROR("r2d_load_texture: failed to load %s", path);
        return tex;
    }
    tex = r2d_texture_from_pixels(r, img.data, img.width, img.height, MACH_FALSE);
    image_free(&img);
    return tex;
}

void r2d_destroy_texture(Mach_Renderer *r, Mach_R2D_Texture *tex) {
    if (!tex->id) return;
    r->gl.DeleteTextures(1, &tex->id);
    tex->id = 0;
}

void r2d_sprite(Mach_Renderer *r, Mach_R2D_Texture tex, f32 x, f32 y, f32 scale, Mach_Color tint) {
    if (!tex.id) return;
    r2d_quad(r, tex.id, x, y, tex.w * scale, tex.h * scale, 0.0f, 0.0f, 1.0f, 1.0f, tint);
}

// --- Isometric projection ---------------------------------------------------

Mach_Vec2 iso_to_screen(const Mach_Camera2D *cam, f32 screen_w, f32 screen_h,
                   f32 grid_x, f32 grid_y, f32 elev) {
    f32 iso_x = (grid_x - grid_y) * (ISO_TILE_W * 0.5f);
    f32 iso_y = (grid_x + grid_y) * (ISO_TILE_H * 0.5f) - elev * ISO_ELEV;
    return (Mach_Vec2){
        (iso_x - cam->pan.x) * cam->zoom + screen_w * 0.5f,
        (iso_y - cam->pan.y) * cam->zoom + screen_h * 0.5f,
    };
}

Mach_Vec2 screen_to_iso(const Mach_Camera2D *cam, f32 screen_w, f32 screen_h,
                   f32 screen_x, f32 screen_y) {
    f32 iso_x = (screen_x - screen_w * 0.5f) / cam->zoom + cam->pan.x;
    f32 iso_y = (screen_y - screen_h * 0.5f) / cam->zoom + cam->pan.y;
    // Invert the elev-0 projection: a = gx-gy, b = gx+gy.
    f32 a = iso_x / (ISO_TILE_W * 0.5f);
    f32 b = iso_y / (ISO_TILE_H * 0.5f);
    return (Mach_Vec2){(a + b) * 0.5f, (b - a) * 0.5f};
}

// =============================================================================
// clay_ui (Clay implementation lives here)
// =============================================================================

// Clay UI backend (included once into the unity build). This is the single home of
// Clay's implementation, plus the text-measure hook and the render-command -> r2d
// translation. See clay_ui.h for the hot-reload notes.

// Clay's implementation lives here. It's third-party single-header code, so silence
// the warnings it trips under -Wall -Wextra rather than let them bury ours.
#define CLAY_IMPLEMENTATION
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wcast-qual"
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#include "clay.h"
#pragma clang diagnostic pop

#include <stdlib.h>

static Mach_Color clay_color(Clay_Color c) {
    return (Mach_Color){ c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f };
}

// Clay measures text through this; our bitmap font is a fixed 8x8 glyph advanced by
// font->advance, scaled uniformly. fontSize is treated as the target pixel height.
static Clay_Dimensions clay_measure_text(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
    Mach_Renderer *r = (Mach_Renderer *)userData;
    f32 scale = (f32)config->fontSize / (f32)r->font->glyph_h;
    return (Clay_Dimensions){
        .width  = (f32)text.length * (f32)r->font->advance * scale,
        .height = (f32)r->font->glyph_h * scale,
    };
}

static void clay_on_error(Clay_ErrorData e) {
    LOG_ERROR("clay: %.*s", (int)e.errorText.length, e.errorText.chars);
}

b32 clay_ui_init(Mach_ClayUI *ui, Mach_Renderer *r) {
    if (!ui || !r) return MACH_FALSE;
    uint32_t need = Clay_MinMemorySize();
    ui->memory = malloc(need);
    if (!ui->memory) {
        LOG_ERROR("clay_ui_init: failed to allocate %u bytes", need);
        return MACH_FALSE;
    }
    Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(need, ui->memory);
    Clay_Dimensions dims = { (f32)r->width, (f32)r->height };
    ui->ctx = Clay_Initialize(arena, dims, (Clay_ErrorHandler){ clay_on_error, NULL });
    Clay_SetMeasureTextFunction(clay_measure_text, r);
    ui->ready = MACH_TRUE;
    LOG_INFO("clay ui initialized (%u bytes)", need);
    return MACH_TRUE;
}

void clay_ui_shutdown(Mach_ClayUI *ui) {
    if (!ui) return;
    free(ui->memory);
    ui->memory = NULL;
    ui->ctx = NULL;
    ui->ready = MACH_FALSE;
}

void clay_ui_begin(Mach_ClayUI *ui, Mach_Renderer *r, Clay_Vector2 mouse, b32 mouse_down) {
    if (!ui || !ui->ready) return;
    // Re-point Clay's globals every frame: after a hot reload the new library's copy
    // of Clay starts with an empty context and a dangling measure-fn pointer.
    Clay_SetCurrentContext(ui->ctx);
    Clay_SetMeasureTextFunction(clay_measure_text, r);
    Clay_SetLayoutDimensions((Clay_Dimensions){ (f32)r->width, (f32)r->height });
    Clay_SetPointerState(mouse, mouse_down);
    Clay_BeginLayout();
}

void clay_ui_render(Mach_ClayUI *ui, Mach_Renderer *r) {
    if (!ui || !ui->ready) return;
    // deltaTime is only used by Clay's animation/transition features, which we don't
    // use yet, so 0 is fine.
    Clay_RenderCommandArray cmds = Clay_EndLayout(0.0f);
    for (i32 i = 0; i < cmds.length; i++) {
        Clay_RenderCommand *cmd = Clay_RenderCommandArray_Get(&cmds, i);
        Clay_BoundingBox b = cmd->boundingBox;
        switch (cmd->commandType) {
        case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
            r2d_fill_rect(r, b.x, b.y, b.width, b.height,
                          clay_color(cmd->renderData.rectangle.backgroundColor));
            break;
        case CLAY_RENDER_COMMAND_TYPE_TEXT: {
            Clay_TextRenderData *t = &cmd->renderData.text;
            char buf[256];
            i32 n = t->stringContents.length;
            if (n > (i32)sizeof(buf) - 1) n = (i32)sizeof(buf) - 1;
            memcpy(buf, t->stringContents.chars, (size_t)n);
            buf[n] = '\0';
            f32 scale = (f32)t->fontSize / (f32)r->font->glyph_h;
            r2d_text(r, b.x, b.y, scale, buf, clay_color(t->textColor));
        } break;
        case CLAY_RENDER_COMMAND_TYPE_BORDER: {
            Clay_BorderRenderData *bd = &cmd->renderData.border;
            Mach_Vec4 col = clay_color(bd->color);
            if (bd->width.top)    r2d_fill_rect(r, b.x, b.y, b.width, (f32)bd->width.top, col);
            if (bd->width.bottom) r2d_fill_rect(r, b.x, b.y + b.height - (f32)bd->width.bottom, b.width, (f32)bd->width.bottom, col);
            if (bd->width.left)   r2d_fill_rect(r, b.x, b.y, (f32)bd->width.left, b.height, col);
            if (bd->width.right)  r2d_fill_rect(r, b.x + b.width - (f32)bd->width.right, b.y, (f32)bd->width.right, b.height, col);
        } break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
            r2d_clip_begin(r, b.x, b.y, b.width, b.height);
            break;
        case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
            r2d_clip_end(r);
            break;
        default:
            break;  // images, custom, overlays: unused by our HUD for now
        }
    }
}

// =============================================================================
// core (RGFW implementation lives here)
// =============================================================================

// Core implementation (MACH_IMPLEMENTATION).
//
// The engine exposes the frame loop as discrete steps; the game owns the loop in
// main() and calls them. The engine keeps window lifecycle and frame timing.
//
// RGFW's implementation compiles here — this file is the single home of the
// windowing layer, the way clay_ui.c owns Clay and image.c owns stb_image.
// Everything else includes engine/rgfw.h for declarations only.

#include <stdio.h>
#include <time.h>

// RGFW is third-party single-header code, so silence the warnings it trips
// under -Wall -Wextra rather than let them bury ours.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-parameter"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wsign-compare"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wcast-qual"
#pragma clang diagnostic ignored "-Wpedantic"
#pragma clang diagnostic ignored "-Wunused-macros"
#pragma clang diagnostic ignored "-Wdouble-promotion"
#pragma clang diagnostic ignored "-Wimplicit-int-conversion"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#define RGFW_IMPLEMENTATION
#include "RGFW.h"
#pragma clang diagnostic pop

// Clamp dt to prevent large simulation jumps after a stall.
#define MAX_DT 0.1f

// (npt): The Win32 branches lean on RGFW's implementation include above already
// having pulled in windows.h; QPC/Sleep are core kernel32 so WIN32_LEAN_AND_MEAN
// doesn't hide them. RGFW also calls timeBeginPeriod(1), which makes Sleep
// 1ms-granular — good enough for the soft frame cap.
u32 engine_ticks_ms(void) {
#if defined(_WIN32)
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (u32)(count.QuadPart * 1000 / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u32)((u64)ts.tv_sec * 1000u + (u64)ts.tv_nsec / 1000000u);
#endif
}

static void engine_sleep_ms(u32 ms) {
#if defined(_WIN32)
    Sleep(ms);
#else
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000u);
    ts.tv_nsec = (long)((ms % 1000u) * 1000000L);
    nanosleep(&ts, NULL);
#endif
}

// Initialize RGFW, create the window from the game's config with a GL 3.3 core
// context, bring up the 2D renderer, and start the frame-timing clocks.
b32 engine_init(Mach_Engine *e, Mach_Engine_Config cfg) {
    LOG_INFO("mach v%d.%d.%d starting up",
             MACH_VERSION_MAJOR, MACH_VERSION_MINOR, MACH_VERSION_PATCH);

    if (RGFW_init("mach", RGFW_initOpenGL) < 0) {
        LOG_ERROR("RGFW_init failed");
        return MACH_FALSE;
    }

    // RGFW keeps the hints pointer, so they live in static storage. Written once
    // here at init (host side only); nothing reads or writes them afterward.
    static RGFW_glHints gl_hints;
    gl_hints = *RGFW_getGlobalHints_OpenGL();
    gl_hints.major = 3;
    gl_hints.minor = 3;
    gl_hints.profile = RGFW_glCore;
    RGFW_setGlobalHints_OpenGL(&gl_hints);

    RGFW_windowFlags flags = RGFW_windowCenter | RGFW_windowOpenGL;
    if (cfg.fullscreen) flags |= RGFW_windowFullscreen;
    if (!cfg.resizable) flags |= RGFW_windowNoResize;
    e->window = RGFW_createWindow(cfg.title, 0, 0, cfg.width, cfg.height, flags);
    if (!e->window) {
        LOG_ERROR("RGFW_createWindow failed");
        RGFW_deinit();
        return MACH_FALSE;
    }

    // Our loop has its own frame cap, so disable vsync and let it govern.
    RGFW_window_swapInterval_OpenGL(e->window, 0);

    if (!r2d_init(&e->r2d, e->window)) {
        RGFW_window_close(e->window);
        e->window = NULL;
        RGFW_deinit();
        return MACH_FALSE;
    }

    e->clear_color = cfg.clear_color;
    e->escape_quits = cfg.escape_quits;
    e->frame_cap_ms = cfg.target_fps > 0 ? 1000u / (u32)cfg.target_fps : 0;

    u32 now = engine_ticks_ms();
    e->running = 1;
    e->frame_start = now;
    e->last_frame_time = now;
    e->fps_timer = now;
    e->frame_count = 0;
    e->fps = 0;
    return MACH_TRUE;
}

// Clean up renderer resources and close the window.
void engine_shutdown(Mach_Engine *e) {
    arena_free(&e->frame_arena);
    r2d_shutdown(&e->r2d);
    if (e->window) {
        RGFW_window_close(e->window);
        e->window = NULL;
    }
    RGFW_deinit();
    LOG_INFO("shutdown complete");
}

b32 engine_running(Mach_Engine *e) {
    return e->running;
}

// Start a frame: compute delta time since the previous frame (clamped), then
// drain the event queue. Window lifecycle events (quit, Escape, resize) are
// consumed here; everything else folds into e->input for the game to read.
f32 engine_frame_begin(Mach_Engine *e) {
    e->frame_start = engine_ticks_ms();
    f32 dt = (f32)(e->frame_start - e->last_frame_time) / 1000.0f;
    if (dt > MAX_DT) dt = MAX_DT;
    e->last_frame_time = e->frame_start;

    arena_reset(&e->frame_arena);
    input_frame_begin(&e->input);
    RGFW_event ev;
    while (RGFW_window_checkEvent(e->window, &ev)) {
        if (ev.type == RGFW_windowClose) {
            LOG_INFO("quit requested");
            e->running = 0;
        } else if (e->escape_quits && ev.type == RGFW_keyPressed &&
                   ev.key.value == RGFW_keyEscape) {
            LOG_INFO("escape pressed, exiting");
            e->running = 0;
        } else if (ev.type == RGFW_windowResized) {
            r2d_resized(&e->r2d);
        } else {
            input_handle_event(&e->input, &ev);
        }
    }
    return dt;
}

// Begin the frame: clear to the game's background color. Always succeeds, but
// keeps the bool shape of the loop.
b32 engine_render_begin(Mach_Engine *e) {
    r2d_begin(&e->r2d, e->clear_color);
    return MACH_TRUE;
}

// Finish the frame: present whatever the game rendered. The engine no longer draws
// its own overlay — FPS is exposed via engine_fps() for the game to display however
// it likes.
void engine_render_end(Mach_Engine *e) {
    r2d_present(&e->r2d);
}

// Frames per second over the last completed 1-second sampling window.
i32 engine_fps(const Mach_Engine *e) { return e ? e->fps : 0; }

// End-of-frame bookkeeping: update the 1s FPS sample and sleep to honor the soft
// frame cap.
void engine_frame_end(Mach_Engine *e) {
    e->frame_count++;
    u32 now = engine_ticks_ms();
    if (now - e->fps_timer >= 1000) {
        e->fps = e->frame_count;
        e->frame_count = 0;
        e->fps_timer = now;
    }

    u32 frame_time = engine_ticks_ms() - e->frame_start;
    if (e->frame_cap_ms && frame_time < e->frame_cap_ms) {
        engine_sleep_ms(e->frame_cap_ms - frame_time);
    }
}

// =============================================================================
// input
// =============================================================================

// Mach_Input snapshot implementation (MACH_IMPLEMENTATION).


// Map an RGFW button to Mach_Mouse_Button, or -1 for buttons we don't model.
static i32 mouse_button_index(u8 rgfw_button) {
    switch (rgfw_button) {
    case RGFW_mouseLeft:   return MOUSE_LEFT;
    case RGFW_mouseRight:  return MOUSE_RIGHT;
    case RGFW_mouseMiddle: return MOUSE_MIDDLE;
    default:               return -1;
    }
}

void input_frame_begin(Mach_Input *in) {
    memset(in->key_pressed, 0, sizeof(in->key_pressed));
    memset(in->mouse_pressed, 0, sizeof(in->mouse_pressed));
    memset(in->mouse_released, 0, sizeof(in->mouse_released));
    in->mouse_delta = (Mach_Vec2){0.0f, 0.0f};
    in->wheel = 0.0f;
}

void input_handle_event(Mach_Input *in, const RGFW_event *ev) {
    switch (ev->type) {
    case RGFW_keyPressed:
        if (!ev->key.repeat) in->key_pressed[ev->key.value] = 1;
        in->key_down[ev->key.value] = 1;
        break;
    case RGFW_keyReleased:
        in->key_down[ev->key.value] = 0;
        break;
    case RGFW_mouseMotion: {
        // RGFW reports absolute positions; the delta is ours to accumulate.
        Mach_Vec2 m = {(f32)ev->mouse.x, (f32)ev->mouse.y};
        if (in->mouse_seen) {
            in->mouse_delta.x += m.x - in->mouse.x;
            in->mouse_delta.y += m.y - in->mouse.y;
        }
        in->mouse = m;
        in->mouse_seen = 1;
    } break;
    case RGFW_mouseButtonPressed:
    case RGFW_mouseButtonReleased: {
        i32 b = mouse_button_index(ev->button.value);
        if (b < 0) break;
        if (ev->type == RGFW_mouseButtonPressed) {
            in->mouse_down[b] = 1;
            in->mouse_pressed[b] = 1;
        } else {
            in->mouse_down[b] = 0;
            in->mouse_released[b] = 1;
        }
    } break;
    case RGFW_mouseScroll:
        in->wheel += ev->delta.y;
        break;
    default:
        break;
    }
}


#endif // MACH_IMPLEMENTATION (once)

/*
zlib License

Copyright (c) 2026 Nathan Tebbs

This software is provided 'as-is', without any express or implied warranty. In
no event will the authors be held liable for any damages arising from the use
of this software.

Permission is granted to anyone to use this software for any purpose, including
commercial applications, and to alter it and redistribute it freely, subject to
the following restrictions:

1. The origin of this software must not be misrepresented; you must not claim
   that you wrote the original software. If you use this software in a product,
   an acknowledgment in the product documentation would be appreciated but is
   not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
*/
