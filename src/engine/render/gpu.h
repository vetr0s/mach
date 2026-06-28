// GPU renderer: SDL_GPU device, frame lifecycle, and draw API.
//
// This is the engine's rendering backend. It is deliberately game-agnostic:
// it knows about meshes, cameras, and 2D overlays, but nothing about
// isometric grids or factory machines. Higher layers compose these.

#ifndef GPU_H
#define GPU_H

#include <SDL3/SDL.h>
#include "../base/base.h"
#include "../math/math.h"
#include "mesh.h"
#include "font.h"

// Max 2D overlay quads accumulated per frame (text glyphs + rects).
#define GPU_MAX_OVERLAY_QUADS 8192

// 2D overlay vertex: screen-space position, atlas UV, tint color.
typedef struct {
    Vec2 pos;
    Vec2 uv;
    Vec4 color;
} Vertex2D;

typedef struct {
    SDL_GPUDevice *device;
    SDL_Window    *window;

    SDL_GPUTexture       *depth_texture;
    SDL_GPUTextureFormat  depth_format;
    u32 width, height;

    SDL_GPUGraphicsPipeline *pipeline_3d;  // lit, depth-tested geometry
    SDL_GPUGraphicsPipeline *pipeline_2d;  // textured, alpha-blended overlay

    Font *font;  // built-in bitmap font/atlas used by the 2D overlay

    // 2D overlay batch. Quads accumulate on the CPU during the frame and are
    // uploaded + drawn in a final color pass by gpu_end_frame.
    SDL_GPUBuffer         *overlay_vbuf;
    SDL_GPUTransferBuffer *overlay_transfer;
    Vertex2D              *overlay_cpu;
    u32                    overlay_count;   // vertices (6 per quad)

    // Per-frame state, valid between gpu_begin_frame and gpu_end_frame.
    SDL_GPUCommandBuffer *cmd;
    SDL_GPUTexture       *swapchain;
    SDL_GPURenderPass    *pass;
} Gpu_Renderer;

// Lifecycle.
b32  gpu_init(Gpu_Renderer *gpu, SDL_Window *window);
void gpu_shutdown(Gpu_Renderer *gpu);

// Frame. gpu_begin_frame returns false when there is no swapchain image this
// frame (e.g. the window is minimized); skip drawing and do not call end_frame.
b32  gpu_begin_frame(Gpu_Renderer *gpu, f32 clear_r, f32 clear_g, f32 clear_b);
void gpu_end_frame(Gpu_Renderer *gpu);

// 3D drawing. Call gpu_begin_3d once per frame to bind the lit pipeline and set
// the camera + light, then gpu_draw_mesh per object. Must be inside a frame.
void gpu_begin_3d(Gpu_Renderer *gpu, Mat4 view_proj, Vec3 light_dir);
void gpu_draw_mesh(Gpu_Renderer *gpu, const Mesh *mesh, Mat4 model, Vec4 color);

// 2D overlay. Call any time during a frame; quads are drawn on top of the scene
// when the frame ends. Uses the renderer's built-in font.
void gpu_draw_rect(Gpu_Renderer *gpu, f32 x, f32 y, f32 w, f32 h, Vec4 color);
void gpu_draw_text(Gpu_Renderer *gpu, f32 x, f32 y, f32 scale, const char *text, Vec4 color);

#endif
