// GPU core (Layer 1): SDL_GPU device, frame lifecycle, render passes, and
// generic resource + draw primitives.
//
// (npt): This layer is deliberately game- and content-agnostic. It knows about
// devices, pipelines, buffers, passes, and draws — but nothing about "lit
// geometry," "overlays," "sprites," or fonts. The batteries layer (draw.{h,c})
// and the game compose these primitives into actual rendering. See ARCHITECTURE.md.

#ifndef GPU_H
#define GPU_H

#include <SDL3/SDL.h>
#include "../base/base.h"
#include "../math/math.h"

typedef struct {
    SDL_GPUDevice *device;
    SDL_Window    *window;

    // The single bytecode format chosen at init (MSL on Metal, SPIR-V on
    // Vulkan); selects which baked blob + entrypoint each shader loads.
    SDL_GPUShaderFormat shader_format;

    SDL_GPUTexture       *depth_texture;
    SDL_GPUTextureFormat  depth_format;
    u32 width, height;

    // Per-frame state, valid between gpu_begin_frame and gpu_end_frame.
    SDL_GPUCommandBuffer *cmd;
    SDL_GPUTexture       *swapchain;
    SDL_GPURenderPass    *pass;
} Gpu_Device;

// One vertex attribute in an interleaved buffer (single buffer slot 0).
typedef struct {
    u32 location;
    SDL_GPUVertexElementFormat format;
    u32 offset;
} Gpu_VertexAttr;

// Everything needed to build a graphics pipeline. The color target is the
// swapchain and the depth target is the device's depth texture; both are filled
// in by gpu_make_pipeline.
typedef struct {
    SDL_GPUShader *vertex_shader;
    SDL_GPUShader *fragment_shader;
    u32 vertex_pitch;                 // stride of one interleaved vertex
    const Gpu_VertexAttr *attrs;
    u32 num_attrs;
    b32 depth_test;                   // depth test + write against the depth target
    b32 alpha_blend;                  // standard src-alpha blending on the color target
    SDL_GPUCullMode cull_mode;
} Gpu_PipelineDesc;

// A render pass over the swapchain (and optionally the depth target).
typedef struct {
    f32 clear_r, clear_g, clear_b;
    b32 clear;                        // color load op: true = CLEAR, false = LOAD
    b32 use_depth;                    // attach + clear the depth target
} Gpu_PassDesc;

// Lifecycle.
b32  gpu_device_init(Gpu_Device *d, SDL_Window *window);
void gpu_device_shutdown(Gpu_Device *d);

// Resource creation. Shaders take both baked blobs; the device's chosen format
// selects which one is used (entrypoint "main0" for MSL, "main" for SPIR-V).
SDL_GPUShader *gpu_make_shader(Gpu_Device *d,
                               const unsigned char *spv, unsigned long spv_len,
                               const unsigned char *msl, unsigned long msl_len,
                               SDL_GPUShaderStage stage,
                               u32 num_uniform_buffers, u32 num_samplers);
SDL_GPUGraphicsPipeline *gpu_make_pipeline(Gpu_Device *d, const Gpu_PipelineDesc *desc);
SDL_GPUBuffer *gpu_make_vertex_buffer(Gpu_Device *d, u32 size);

// Frame. gpu_begin_frame acquires the command buffer + swapchain image; it
// returns false when there is no image this frame (e.g. minimized) — skip
// rendering and do not call gpu_end_frame.
b32  gpu_begin_frame(Gpu_Device *d);
void gpu_end_frame(Gpu_Device *d);

// Render passes, within a frame. Begin one, issue draws, end it. A frame may
// contain several (e.g. a depth-tested scene pass then a color-only overlay).
void gpu_begin_pass(Gpu_Device *d, const Gpu_PassDesc *desc);
void gpu_end_pass(Gpu_Device *d);

// Draw submission, within a pass.
void gpu_apply_pipeline(Gpu_Device *d, SDL_GPUGraphicsPipeline *pipeline);
void gpu_push_vertex_uniform(Gpu_Device *d, u32 slot, const void *data, u32 size);
void gpu_push_fragment_uniform(Gpu_Device *d, u32 slot, const void *data, u32 size);
void gpu_bind_vertex_buffer(Gpu_Device *d, SDL_GPUBuffer *vbuf);
void gpu_bind_index_buffer(Gpu_Device *d, SDL_GPUBuffer *ibuf);   // 16-bit indices
void gpu_bind_fragment_sampler(Gpu_Device *d, SDL_GPUTexture *tex, SDL_GPUSampler *sampler);
void gpu_draw_indexed(Gpu_Device *d, u32 index_count);
void gpu_draw(Gpu_Device *d, u32 vertex_count);

// Upload CPU bytes into a vertex buffer via a transfer buffer. Call within a
// frame but outside any render pass (it opens its own copy pass).
void gpu_upload_buffer(Gpu_Device *d, SDL_GPUTransferBuffer *transfer,
                       SDL_GPUBuffer *dst, const void *src, u32 size);

#endif
