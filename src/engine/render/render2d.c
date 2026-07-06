// 2D renderer implementation over OpenGL 3.3 core (included into mach.c).
//
// Everything draws through one shader as batched, textured, vertex-colored
// triangles. The draw calls append to a CPU-side batch that flushes to a single
// glDrawElements whenever the texture or scissor changes, the batch fills, or
// the frame presents.

#include "render2d.h"
#include "image.h"
#include "../debug.h"

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

static b32 r2d_gl_load(GLApi *gl) {
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

static u32 r2d_compile_shader(const GLApi *gl, u32 type, const char *src) {
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
static void r2d_apply_window_size(Renderer *r) {
    RGFW_window_getSize(r->window, &r->width, &r->height);
    r->fb_width = r->width;
    r->fb_height = r->height;
    RGFW_window_getSizeInPixels(r->window, &r->fb_width, &r->fb_height);
    r->gl.Viewport(0, 0, r->fb_width, r->fb_height);
}

b32 r2d_init(Renderer *r, RGFW_window *window) {
    r->window = window;

    if (!r2d_gl_load(&r->gl)) return MACH_FALSE;
    const GLApi *gl = &r->gl;

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
    gl->VertexAttribPointer(0, 2, GL_FLOAT, 0, (i32)sizeof(R2D_Vertex), (void *)0);
    gl->EnableVertexAttribArray(1);
    gl->VertexAttribPointer(1, 2, GL_FLOAT, 0, (i32)sizeof(R2D_Vertex),
                            (void *)(2 * sizeof(f32)));
    gl->EnableVertexAttribArray(2);
    gl->VertexAttribPointer(2, 4, GL_FLOAT, 0, (i32)sizeof(R2D_Vertex),
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

void r2d_shutdown(Renderer *r) {
    const GLApi *gl = &r->gl;
    if (r->font) { font_destroy(r, r->font); r->font = NULL; }
    if (r->white.id) r2d_destroy_texture(r, &r->white);
    if (r->program) { gl->DeleteProgram(r->program); r->program = 0; }
    if (r->vbo) { gl->DeleteBuffers(1, &r->vbo); r->vbo = 0; }
    if (r->ibo) { gl->DeleteBuffers(1, &r->ibo); r->ibo = 0; }
    if (r->vao) { gl->DeleteVertexArrays(1, &r->vao); r->vao = 0; }
    LOG_INFO("2D renderer shut down");
}

void r2d_resized(Renderer *r) {
    r2d_apply_window_size(r);
    LOG_DEBUG("window resized to %dx%d", r->width, r->height);
}

// --- Batch ----------------------------------------------------------------------

static void r2d_flush(Renderer *r) {
    if (r->index_count == 0) return;
    const GLApi *gl = &r->gl;
    gl->BufferSubData(GL_ARRAY_BUFFER, 0,
                      (isize)((usize)r->vert_count * sizeof(R2D_Vertex)), r->verts);
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
static i32 r2d_reserve(Renderer *r, u32 tex, i32 nverts, i32 nindices) {
    if (tex != r->batch_tex || r->vert_count + nverts > R2D_MAX_VERTS ||
        r->index_count + nindices > R2D_MAX_INDICES) {
        r2d_flush(r);
        r->batch_tex = tex;
    }
    return r->vert_count;
}

static R2D_Vertex r2d_vertex(f32 x, f32 y, f32 u, f32 v, Color c) {
    R2D_Vertex vert;
    vert.x = x; vert.y = y;
    vert.u = u; vert.v = v;
    vert.color = c;
    return vert;
}

// Append one textured axis-aligned quad.
static void r2d_quad(Renderer *r, u32 tex, f32 x, f32 y, f32 w, f32 h,
                     f32 u0, f32 v0, f32 u1, f32 v1, Color c) {
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

void r2d_begin(Renderer *r, Color clear) {
    const GLApi *gl = &r->gl;
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

void r2d_present(Renderer *r) {
    r2d_flush(r);
    RGFW_window_swapBuffers_OpenGL(r->window);
}

// --- Primitives -------------------------------------------------------------

void r2d_fill_rect(Renderer *r, f32 x, f32 y, f32 w, f32 h, Color color) {
    r2d_quad(r, r->white.id, x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, color);
}

// Fill a convex polygon as a triangle fan.
void r2d_fill_poly(Renderer *r, const Vec2 *pts, i32 n, Color color) {
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
void r2d_poly_outline(Renderer *r, const Vec2 *pts, i32 n, Color color) {
    if (n < 2 || n > 16) return;
    for (i32 i = 0; i < n; i++) {
        Vec2 p = pts[i];
        Vec2 q = pts[(i + 1) % n];
        Vec2 d = vec2_sub(q, p);
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

void r2d_text(Renderer *r, f32 x, f32 y, f32 scale, const char *text, Color color) {
    if (!text) return;
    Font *font = r->font;
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
void r2d_clip_begin(Renderer *r, f32 x, f32 y, f32 w, f32 h) {
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

void r2d_clip_end(Renderer *r) {
    r2d_flush(r);
    r->gl.Disable(GL_SCISSOR_TEST);
}

// --- Textures and sprites -----------------------------------------------------

R2D_Texture r2d_texture_from_pixels(Renderer *r, const void *rgba, i32 w, i32 h, b32 nearest) {
    const GLApi *gl = &r->gl;
    R2D_Texture tex = {0};
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

R2D_Texture r2d_load_texture(Renderer *r, const char *path) {
    R2D_Texture tex = {0};
    Image img = image_load(path);
    if (!img.data) {
        LOG_ERROR("r2d_load_texture: failed to load %s", path);
        return tex;
    }
    tex = r2d_texture_from_pixels(r, img.data, img.width, img.height, MACH_FALSE);
    image_free(&img);
    return tex;
}

void r2d_destroy_texture(Renderer *r, R2D_Texture *tex) {
    if (!tex->id) return;
    r->gl.DeleteTextures(1, &tex->id);
    tex->id = 0;
}

void r2d_sprite(Renderer *r, R2D_Texture tex, f32 x, f32 y, f32 scale, Color tint) {
    if (!tex.id) return;
    r2d_quad(r, tex.id, x, y, tex.w * scale, tex.h * scale, 0.0f, 0.0f, 1.0f, 1.0f, tint);
}

// --- Isometric projection ---------------------------------------------------

Vec2 iso_to_screen(const Camera2D *cam, f32 screen_w, f32 screen_h,
                   f32 grid_x, f32 grid_y, f32 elev) {
    f32 iso_x = (grid_x - grid_y) * (ISO_TILE_W * 0.5f);
    f32 iso_y = (grid_x + grid_y) * (ISO_TILE_H * 0.5f) - elev * ISO_ELEV;
    return (Vec2){
        (iso_x - cam->pan.x) * cam->zoom + screen_w * 0.5f,
        (iso_y - cam->pan.y) * cam->zoom + screen_h * 0.5f,
    };
}

Vec2 screen_to_iso(const Camera2D *cam, f32 screen_w, f32 screen_h,
                   f32 screen_x, f32 screen_y) {
    f32 iso_x = (screen_x - screen_w * 0.5f) / cam->zoom + cam->pan.x;
    f32 iso_y = (screen_y - screen_h * 0.5f) / cam->zoom + cam->pan.y;
    // Invert the elev-0 projection: a = gx-gy, b = gx+gy.
    f32 a = iso_x / (ISO_TILE_W * 0.5f);
    f32 b = iso_y / (ISO_TILE_H * 0.5f);
    return (Vec2){(a + b) * 0.5f, (b - a) * 0.5f};
}
