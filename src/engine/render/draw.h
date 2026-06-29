// Draw batteries (Layer 2): convenience rendering built on the GPU core.
//
// (npt): This is the "batteries included" layer. It owns the built-in lit 3D
// pipeline and the textured 2D overlay pipeline (plus the bitmap font), and
// exposes high-level draw calls so a game can render meshes and HUD without
// authoring a single shader. A game that wants custom rendering bypasses this
// and drives gpu.h (Layer 1) directly. See ARCHITECTURE.md.

#ifndef DRAW_H
#define DRAW_H

#include "gpu.h"
#include "mesh.h"
#include "font.h"

// Max 2D overlay quads accumulated per frame (text glyphs + rects).
#define DRAW_MAX_OVERLAY_QUADS 8192

// 2D overlay vertex: screen-space position, atlas UV, tint color.
typedef struct {
    Vec2 pos;
    Vec2 uv;
    Vec4 color;
} Vertex2D;

typedef struct {
    Gpu_Device *gpu;   // non-owning: the device these batteries render through

    SDL_GPUGraphicsPipeline *pipeline_3d;  // lit, depth-tested geometry
    SDL_GPUGraphicsPipeline *pipeline_2d;  // textured, alpha-blended overlay
    Font *font;                            // built-in bitmap font/atlas

    // 2D overlay batch. Quads accumulate on the CPU during the frame and are
    // uploaded + drawn by draw_flush_overlay.
    SDL_GPUBuffer         *overlay_vbuf;
    SDL_GPUTransferBuffer *overlay_transfer;
    Vertex2D              *overlay_cpu;
    u32                    overlay_count;   // vertices (6 per quad)
} Draw;

// Lifecycle. draw_init builds the pipelines + font against an initialized device.
b32  draw_init(Draw *dr, Gpu_Device *gpu);
void draw_shutdown(Draw *dr);

// 3D. Call draw_begin_3d once per scene pass to bind the lit pipeline and set the
// camera + light, then draw_mesh per object. Must be inside an active pass.
void draw_begin_3d(Draw *dr, Mat4 view_proj, Vec3 light_dir);
void draw_mesh(Draw *dr, const Mesh *mesh, Mat4 model, Vec4 color);

// 2D overlay. Accumulate any time during a frame; drawn by draw_flush_overlay.
void draw_rect(Draw *dr, f32 x, f32 y, f32 w, f32 h, Vec4 color);
void draw_text(Draw *dr, f32 x, f32 y, f32 scale, const char *text, Vec4 color);

// Draw the accumulated overlay in its own color-only pass, then reset the batch.
// Call after the scene pass has ended and before gpu_end_frame.
void draw_flush_overlay(Draw *dr);

#endif
