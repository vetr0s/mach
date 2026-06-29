// GPU renderer implementation (included into mach.c).

#include "gpu.h"
#include "../debug.h"
#include "shaders_generated.h"
#include <string.h>
#include <stddef.h>

// --- Shaders ----------------------------------------------------------------
//
// Shaders are authored once in HLSL (src/engine/render/shaders/*.hlsl) and
// cross-compiled offline by scripts/shaders.sh into shaders_generated.h: a
// SPIR-V blob (Vulkan: Linux/Windows) and an MSL blob (Metal: macOS) per stage.
// gpu_load_shader picks the blob matching the device's chosen format.
//
// Uniform block layouts. float4 used throughout to dodge std140/float3 packing.
typedef struct { Mat4 view_proj; } Gpu_FrameVS;
typedef struct { Mat4 model;     } Gpu_DrawVS;
typedef struct { Vec4 dir; Vec4 ambient; } Gpu_LightFS;
typedef struct { Vec4 rgba;      } Gpu_ColorFS;

#define GPU_AMBIENT 0.30f

// Create a shader from its baked blobs, selecting the one matching the device's
// format. spirv-cross renames the entrypoint to "main0" (Metal reserves "main");
// the SPIR-V path keeps glslang's "main".
static SDL_GPUShader *gpu_load_shader(Gpu_Renderer *gpu,
                                      const unsigned char *spv, unsigned long spv_len,
                                      const unsigned char *msl, unsigned long msl_len,
                                      SDL_GPUShaderStage stage,
                                      u32 num_uniform_buffers, u32 num_samplers) {
    SDL_GPUShaderCreateInfo info = {0};
    if (gpu->shader_format == SDL_GPU_SHADERFORMAT_MSL) {
        info.code = msl;
        info.code_size = msl_len;
        info.format = SDL_GPU_SHADERFORMAT_MSL;
        info.entrypoint = "main0";
    } else {
        info.code = spv;
        info.code_size = spv_len;
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        info.entrypoint = "main";
    }
    info.stage = stage;
    info.num_uniform_buffers = num_uniform_buffers;
    info.num_samplers = num_samplers;
    SDL_GPUShader *s = SDL_CreateGPUShader(gpu->device, &info);
    if (!s) LOG_ERROR("shader create failed: %s", SDL_GetError());
    return s;
}

static b32 gpu_create_pipeline_3d(Gpu_Renderer *gpu) {
    SDL_GPUShader *vs = gpu_load_shader(gpu,
        shader_lit_vert_spv, shader_lit_vert_spv_len,
        shader_lit_vert_msl, shader_lit_vert_msl_len,
        SDL_GPU_SHADERSTAGE_VERTEX, 2, 0);
    SDL_GPUShader *fs = gpu_load_shader(gpu,
        shader_lit_frag_spv, shader_lit_frag_spv_len,
        shader_lit_frag_msl, shader_lit_frag_msl_len,
        SDL_GPU_SHADERSTAGE_FRAGMENT, 2, 0);
    if (!vs || !fs) {
        if (vs) SDL_ReleaseGPUShader(gpu->device, vs);
        if (fs) SDL_ReleaseGPUShader(gpu->device, fs);
        return MACH_FALSE;
    }

    SDL_GPUVertexBufferDescription vbdesc = {
        .slot = 0, .pitch = sizeof(Vertex3D),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX, .instance_step_rate = 0,
    };
    SDL_GPUVertexAttribute attrs[2] = {
        {.location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = 0},
        {.location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(Vertex3D, normal)},
    };
    SDL_GPUColorTargetDescription color = {
        .format = SDL_GetGPUSwapchainTextureFormat(gpu->device, gpu->window),
    };

    SDL_GPUGraphicsPipelineCreateInfo info = {0};
    info.vertex_shader = vs;
    info.fragment_shader = fs;
    info.vertex_input_state.vertex_buffer_descriptions = &vbdesc;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_attributes = attrs;
    info.vertex_input_state.num_vertex_attributes = 2;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    // No culling: cubes are closed (depth test resolves overlaps) and the ground
    // plane must be visible from above regardless of its winding.
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    info.rasterizer_state.enable_depth_clip = true;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.enable_depth_write = true;
    info.target_info.color_target_descriptions = &color;
    info.target_info.num_color_targets = 1;
    info.target_info.depth_stencil_format = gpu->depth_format;
    info.target_info.has_depth_stencil_target = true;

    gpu->pipeline_3d = SDL_CreateGPUGraphicsPipeline(gpu->device, &info);
    SDL_ReleaseGPUShader(gpu->device, vs);
    SDL_ReleaseGPUShader(gpu->device, fs);

    if (!gpu->pipeline_3d) {
        LOG_ERROR("pipeline_3d creation failed: %s", SDL_GetError());
        return MACH_FALSE;
    }
    return MACH_TRUE;
}

// 2D overlay pipeline (overlay.*.hlsl): screen-space quads sampling the R8 font
// atlas. The atlas's white texel lets the same shader draw solid rects.
typedef struct { Mat4 proj; } Gpu_Frame2D;

static b32 gpu_create_pipeline_2d(Gpu_Renderer *gpu) {
    SDL_GPUShader *vs = gpu_load_shader(gpu,
        shader_overlay_vert_spv, shader_overlay_vert_spv_len,
        shader_overlay_vert_msl, shader_overlay_vert_msl_len,
        SDL_GPU_SHADERSTAGE_VERTEX, 1, 0);
    SDL_GPUShader *fs = gpu_load_shader(gpu,
        shader_overlay_frag_spv, shader_overlay_frag_spv_len,
        shader_overlay_frag_msl, shader_overlay_frag_msl_len,
        SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 1);
    if (!vs || !fs) {
        if (vs) SDL_ReleaseGPUShader(gpu->device, vs);
        if (fs) SDL_ReleaseGPUShader(gpu->device, fs);
        return MACH_FALSE;
    }

    SDL_GPUVertexBufferDescription vbdesc = {
        .slot = 0, .pitch = sizeof(Vertex2D),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX, .instance_step_rate = 0,
    };
    SDL_GPUVertexAttribute attrs[3] = {
        {.location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = offsetof(Vertex2D, pos)},
        {.location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = offsetof(Vertex2D, uv)},
        {.location = 2, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, .offset = offsetof(Vertex2D, color)},
    };

    SDL_GPUColorTargetDescription color = {0};
    color.format = SDL_GetGPUSwapchainTextureFormat(gpu->device, gpu->window);
    color.blend_state.enable_blend = true;
    color.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    color.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    color.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    color.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

    SDL_GPUGraphicsPipelineCreateInfo info = {0};
    info.vertex_shader = vs;
    info.fragment_shader = fs;
    info.vertex_input_state.vertex_buffer_descriptions = &vbdesc;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_attributes = attrs;
    info.vertex_input_state.num_vertex_attributes = 3;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    // The overlay draws in its own color-only pass, so no depth target.
    info.target_info.color_target_descriptions = &color;
    info.target_info.num_color_targets = 1;

    gpu->pipeline_2d = SDL_CreateGPUGraphicsPipeline(gpu->device, &info);
    SDL_ReleaseGPUShader(gpu->device, vs);
    SDL_ReleaseGPUShader(gpu->device, fs);
    if (!gpu->pipeline_2d) {
        LOG_ERROR("pipeline_2d creation failed: %s", SDL_GetError());
        return MACH_FALSE;
    }
    return MACH_TRUE;
}

// Pick a supported depth format (spec guarantees one of D32/D24, not both).
static SDL_GPUTextureFormat gpu_choose_depth_format(SDL_GPUDevice *device) {
    if (SDL_GPUTextureSupportsFormat(device, SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
            SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) {
        return SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    }
    return SDL_GPU_TEXTUREFORMAT_D24_UNORM;
}

static SDL_GPUTexture *gpu_create_depth_texture(Gpu_Renderer *gpu) {
    SDL_GPUTextureCreateInfo info = {0};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = gpu->depth_format;
    info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    info.width = gpu->width;
    info.height = gpu->height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    return SDL_CreateGPUTexture(gpu->device, &info);
}

b32 gpu_init(Gpu_Renderer *gpu, SDL_Window *window) {
    gpu->window = window;

    // Request every format we bake; SDL picks a backend and we load the blob to
    // match. SPIR-V covers Vulkan (Linux/Windows), MSL covers Metal (macOS).
    SDL_GPUShaderFormat formats = SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL;
    gpu->device = SDL_CreateGPUDevice(formats, /*debug=*/true, NULL);
    if (!gpu->device) {
        LOG_ERROR("SDL_CreateGPUDevice failed: %s", SDL_GetError());
        return MACH_FALSE;
    }

    // Narrow to the single format this backend accepts.
    SDL_GPUShaderFormat available = SDL_GetGPUShaderFormats(gpu->device);
    if (available & SDL_GPU_SHADERFORMAT_MSL) {
        gpu->shader_format = SDL_GPU_SHADERFORMAT_MSL;
    } else if (available & SDL_GPU_SHADERFORMAT_SPIRV) {
        gpu->shader_format = SDL_GPU_SHADERFORMAT_SPIRV;
    } else {
        LOG_ERROR("no baked shader format supported by GPU backend");
        SDL_DestroyGPUDevice(gpu->device);
        gpu->device = NULL;
        return MACH_FALSE;
    }
    LOG_INFO("GPU device created (driver: %s, shaders: %s)",
             SDL_GetGPUDeviceDriver(gpu->device),
             gpu->shader_format == SDL_GPU_SHADERFORMAT_MSL ? "MSL" : "SPIR-V");

    if (!SDL_ClaimWindowForGPUDevice(gpu->device, window)) {
        LOG_ERROR("SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        SDL_DestroyGPUDevice(gpu->device);
        gpu->device = NULL;
        return MACH_FALSE;
    }

    i32 w, h;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    gpu->width = (u32)w;
    gpu->height = (u32)h;

    gpu->depth_format = gpu_choose_depth_format(gpu->device);
    gpu->depth_texture = gpu_create_depth_texture(gpu);
    if (!gpu->depth_texture) {
        LOG_ERROR("depth texture creation failed: %s", SDL_GetError());
        SDL_ReleaseWindowFromGPUDevice(gpu->device, window);
        SDL_DestroyGPUDevice(gpu->device);
        gpu->device = NULL;
        return MACH_FALSE;
    }

    if (!gpu_create_pipeline_3d(gpu) || !gpu_create_pipeline_2d(gpu)) {
        gpu_shutdown(gpu);
        return MACH_FALSE;
    }

    // Allocate the dynamic 2D overlay buffers.
    u32 overlay_bytes = GPU_MAX_OVERLAY_QUADS * 6 * sizeof(Vertex2D);
    gpu->overlay_cpu = (Vertex2D *)SDL_malloc(overlay_bytes);
    SDL_GPUBufferCreateInfo obi = {.usage = SDL_GPU_BUFFERUSAGE_VERTEX, .size = overlay_bytes};
    gpu->overlay_vbuf = SDL_CreateGPUBuffer(gpu->device, &obi);
    SDL_GPUTransferBufferCreateInfo oti = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = overlay_bytes};
    gpu->overlay_transfer = SDL_CreateGPUTransferBuffer(gpu->device, &oti);
    if (!gpu->overlay_cpu || !gpu->overlay_vbuf || !gpu->overlay_transfer) {
        LOG_ERROR("overlay buffer allocation failed: %s", SDL_GetError());
        gpu_shutdown(gpu);
        return MACH_FALSE;
    }

    gpu->font = font_create(gpu->device);
    if (!gpu->font) {
        gpu_shutdown(gpu);
        return MACH_FALSE;
    }

    LOG_INFO("GPU renderer ready (%ux%u, depth %s)", gpu->width, gpu->height,
             gpu->depth_format == SDL_GPU_TEXTUREFORMAT_D32_FLOAT ? "D32F" : "D24");
    return MACH_TRUE;
}

void gpu_shutdown(Gpu_Renderer *gpu) {
    if (!gpu->device) return;
    if (gpu->font) { font_destroy(gpu->device, gpu->font); gpu->font = NULL; }
    if (gpu->overlay_cpu) { SDL_free(gpu->overlay_cpu); gpu->overlay_cpu = NULL; }
    if (gpu->overlay_transfer) {
        SDL_ReleaseGPUTransferBuffer(gpu->device, gpu->overlay_transfer);
        gpu->overlay_transfer = NULL;
    }
    if (gpu->overlay_vbuf) {
        SDL_ReleaseGPUBuffer(gpu->device, gpu->overlay_vbuf);
        gpu->overlay_vbuf = NULL;
    }
    if (gpu->pipeline_2d) {
        SDL_ReleaseGPUGraphicsPipeline(gpu->device, gpu->pipeline_2d);
        gpu->pipeline_2d = NULL;
    }
    if (gpu->pipeline_3d) {
        SDL_ReleaseGPUGraphicsPipeline(gpu->device, gpu->pipeline_3d);
        gpu->pipeline_3d = NULL;
    }
    if (gpu->depth_texture) {
        SDL_ReleaseGPUTexture(gpu->device, gpu->depth_texture);
        gpu->depth_texture = NULL;
    }
    if (gpu->window) {
        SDL_ReleaseWindowFromGPUDevice(gpu->device, gpu->window);
    }
    SDL_DestroyGPUDevice(gpu->device);
    gpu->device = NULL;
    LOG_INFO("GPU renderer shut down");
}

b32 gpu_begin_frame(Gpu_Renderer *gpu, f32 clear_r, f32 clear_g, f32 clear_b) {
    gpu->cmd = SDL_AcquireGPUCommandBuffer(gpu->device);
    if (!gpu->cmd) {
        LOG_ERROR("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return MACH_FALSE;
    }

    gpu->swapchain = NULL;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(gpu->cmd, gpu->window,
                                               &gpu->swapchain, NULL, NULL)) {
        LOG_ERROR("SDL_WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        SDL_SubmitGPUCommandBuffer(gpu->cmd);
        gpu->cmd = NULL;
        return MACH_FALSE;
    }

    // No swapchain image (e.g. minimized): nothing to draw, but the command
    // buffer must still be submitted.
    if (!gpu->swapchain) {
        SDL_SubmitGPUCommandBuffer(gpu->cmd);
        gpu->cmd = NULL;
        return MACH_FALSE;
    }

    gpu->overlay_count = 0;  // start a fresh overlay batch for this frame

    SDL_GPUColorTargetInfo color = {0};
    color.texture = gpu->swapchain;
    color.clear_color = (SDL_FColor){clear_r, clear_g, clear_b, 1.0f};
    color.load_op = SDL_GPU_LOADOP_CLEAR;
    color.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPUDepthStencilTargetInfo depth = {0};
    depth.texture = gpu->depth_texture;
    depth.clear_depth = 1.0f;
    depth.load_op = SDL_GPU_LOADOP_CLEAR;
    depth.store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;

    gpu->pass = SDL_BeginGPURenderPass(gpu->cmd, &color, 1, &depth);
    return MACH_TRUE;
}

// Draw the accumulated 2D overlay in a final color-only pass, then submit.
static void gpu_flush_overlay(Gpu_Renderer *gpu) {
    if (gpu->overlay_count == 0) return;

    // Upload this frame's overlay vertices (outside any render pass).
    u32 bytes = gpu->overlay_count * sizeof(Vertex2D);
    void *map = SDL_MapGPUTransferBuffer(gpu->device, gpu->overlay_transfer, true);
    memcpy(map, gpu->overlay_cpu, bytes);
    SDL_UnmapGPUTransferBuffer(gpu->device, gpu->overlay_transfer);

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(gpu->cmd);
    SDL_GPUTransferBufferLocation src = {.transfer_buffer = gpu->overlay_transfer, .offset = 0};
    SDL_GPUBufferRegion dst = {.buffer = gpu->overlay_vbuf, .offset = 0, .size = bytes};
    SDL_UploadToGPUBuffer(copy, &src, &dst, true);
    SDL_EndGPUCopyPass(copy);

    // Color-only pass that loads (preserves) the rendered scene.
    SDL_GPUColorTargetInfo color = {0};
    color.texture = gpu->swapchain;
    color.load_op = SDL_GPU_LOADOP_LOAD;
    color.store_op = SDL_GPU_STOREOP_STORE;
    gpu->pass = SDL_BeginGPURenderPass(gpu->cmd, &color, 1, NULL);

    SDL_BindGPUGraphicsPipeline(gpu->pass, gpu->pipeline_2d);
    Gpu_Frame2D frame = {mat4_ortho(0.0f, (f32)gpu->width, (f32)gpu->height, 0.0f, -1.0f, 1.0f)};
    SDL_PushGPUVertexUniformData(gpu->cmd, 0, &frame, sizeof(frame));
    SDL_GPUBufferBinding vb = {.buffer = gpu->overlay_vbuf, .offset = 0};
    SDL_BindGPUVertexBuffers(gpu->pass, 0, &vb, 1);
    SDL_GPUTextureSamplerBinding tex = {.texture = gpu->font->atlas, .sampler = gpu->font->sampler};
    SDL_BindGPUFragmentSamplers(gpu->pass, 0, &tex, 1);
    SDL_DrawGPUPrimitives(gpu->pass, gpu->overlay_count, 1, 0, 0);
}

void gpu_end_frame(Gpu_Renderer *gpu) {
    if (gpu->pass) {
        SDL_EndGPURenderPass(gpu->pass);
        gpu->pass = NULL;
    }
    gpu_flush_overlay(gpu);
    if (gpu->pass) {
        SDL_EndGPURenderPass(gpu->pass);
        gpu->pass = NULL;
    }
    if (gpu->cmd) {
        SDL_SubmitGPUCommandBuffer(gpu->cmd);
        gpu->cmd = NULL;
    }
    gpu->swapchain = NULL;
}

// Bind the lit pipeline and set per-frame camera + light uniforms.
void gpu_begin_3d(Gpu_Renderer *gpu, Mat4 view_proj, Vec3 light_dir) {
    SDL_BindGPUGraphicsPipeline(gpu->pass, gpu->pipeline_3d);

    Gpu_FrameVS frame = {view_proj};
    SDL_PushGPUVertexUniformData(gpu->cmd, 0, &frame, sizeof(frame));

    Gpu_LightFS light;
    light.dir = (Vec4){light_dir.x, light_dir.y, light_dir.z, 0.0f};
    light.ambient = (Vec4){GPU_AMBIENT, 0.0f, 0.0f, 0.0f};
    SDL_PushGPUFragmentUniformData(gpu->cmd, 0, &light, sizeof(light));
}

// Draw a mesh with a model transform and flat color. Call after gpu_begin_3d.
void gpu_draw_mesh(Gpu_Renderer *gpu, const Mesh *mesh, Mat4 model, Vec4 color) {
    Gpu_DrawVS draw = {model};
    SDL_PushGPUVertexUniformData(gpu->cmd, 1, &draw, sizeof(draw));
    Gpu_ColorFS col = {color};
    SDL_PushGPUFragmentUniformData(gpu->cmd, 1, &col, sizeof(col));

    SDL_GPUBufferBinding vb = {.buffer = mesh->vbuf, .offset = 0};
    SDL_BindGPUVertexBuffers(gpu->pass, 0, &vb, 1);
    SDL_GPUBufferBinding ib = {.buffer = mesh->ibuf, .offset = 0};
    SDL_BindGPUIndexBuffer(gpu->pass, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);
    SDL_DrawGPUIndexedPrimitives(gpu->pass, (u32)mesh->index_count, 1, 0, 0, 0);
}

// --- 2D overlay -------------------------------------------------------------

// Append one quad (two triangles) in screen space with the given atlas UVs.
static void overlay_quad(Gpu_Renderer *gpu, f32 x, f32 y, f32 w, f32 h,
                         f32 u0, f32 v0, f32 u1, f32 v1, Vec4 c) {
    if (gpu->overlay_count + 6 > GPU_MAX_OVERLAY_QUADS * 6) return;
    Vertex2D *v = &gpu->overlay_cpu[gpu->overlay_count];
    v[0] = (Vertex2D){{x,     y},     {u0, v0}, c};
    v[1] = (Vertex2D){{x,     y + h}, {u0, v1}, c};
    v[2] = (Vertex2D){{x + w, y + h}, {u1, v1}, c};
    v[3] = (Vertex2D){{x,     y},     {u0, v0}, c};
    v[4] = (Vertex2D){{x + w, y + h}, {u1, v1}, c};
    v[5] = (Vertex2D){{x + w, y},     {u1, v0}, c};
    gpu->overlay_count += 6;
}

void gpu_draw_rect(Gpu_Renderer *gpu, f32 x, f32 y, f32 w, f32 h, Vec4 color) {
    f32 u = gpu->font->white_u, v = gpu->font->white_v;
    overlay_quad(gpu, x, y, w, h, u, v, u, v, color);
}

void gpu_draw_text(Gpu_Renderer *gpu, f32 x, f32 y, f32 scale, const char *text, Vec4 color) {
    const Font *font = gpu->font;
    if (!text) return;

    f32 gw = (f32)font->glyph_w * scale;
    f32 gh = (f32)font->glyph_h * scale;
    f32 adv = (f32)font->advance * scale;
    f32 cur_x = x;
    for (const char *c = text; *c; c++) {
        f32 u0, v0, u1, v1;
        if (font_glyph_uv(font, *c, &u0, &v0, &u1, &v1)) {
            overlay_quad(gpu, cur_x, y, gw, gh, u0, v0, u1, v1, color);
        }
        cur_x += adv;
    }
}
