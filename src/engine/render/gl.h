// Minimal OpenGL 3.3 core surface for the 2D renderer.
//
// The renderer declares exactly the entry points and constants it uses instead
// of pulling in platform GL headers. r2d_init fills the function table through
// RGFW's proc loader once the context exists; the table lives inside the
// Renderer struct — pointer-passed like all engine state — so a hot-reloaded
// game library draws through the pointers the host loaded.

#ifndef GL_H
#define GL_H

#include "../base/base.h"

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
typedef struct GLApi {
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
} GLApi;

// A GPU texture and its pixel size (for sprite sizing and atlas UVs).
typedef struct {
    u32 id;
    f32 w, h;
} R2D_Texture;

#endif
