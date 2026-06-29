// GPU core implementation (Layer 1; included into mach.c).

#include "gpu.h"
#include "../debug.h"
#include <string.h>

// --- Resources --------------------------------------------------------------

// Create a shader from its baked blobs, selecting the one matching the device's
// format. spirv-cross renames the entrypoint to "main0" (Metal reserves "main");
// the SPIR-V path keeps glslang's "main".
SDL_GPUShader *gpu_make_shader(Gpu_Device *d,
                               const unsigned char *spv, unsigned long spv_len,
                               const unsigned char *msl, unsigned long msl_len,
                               SDL_GPUShaderStage stage,
                               u32 num_uniform_buffers, u32 num_samplers) {
    SDL_GPUShaderCreateInfo info = {0};
    if (d->shader_format == SDL_GPU_SHADERFORMAT_MSL) {
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
    SDL_GPUShader *s = SDL_CreateGPUShader(d->device, &info);
    if (!s) LOG_ERROR("shader create failed: %s", SDL_GetError());
    return s;
}

SDL_GPUGraphicsPipeline *gpu_make_pipeline(Gpu_Device *d, const Gpu_PipelineDesc *desc) {
    SDL_GPUVertexBufferDescription vbdesc = {
        .slot = 0, .pitch = desc->vertex_pitch,
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX, .instance_step_rate = 0,
    };

    // Translate our compact attribute list into SDL's (all on buffer slot 0).
    SDL_GPUVertexAttribute attrs[16];
    u32 n = desc->num_attrs < 16 ? desc->num_attrs : 16;
    for (u32 i = 0; i < n; i++) {
        attrs[i] = (SDL_GPUVertexAttribute){
            .location = desc->attrs[i].location, .buffer_slot = 0,
            .format = desc->attrs[i].format, .offset = desc->attrs[i].offset,
        };
    }

    SDL_GPUColorTargetDescription color = {0};
    color.format = SDL_GetGPUSwapchainTextureFormat(d->device, d->window);
    if (desc->alpha_blend) {
        color.blend_state.enable_blend = true;
        color.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        color.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        color.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        color.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        color.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        color.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    }

    SDL_GPUGraphicsPipelineCreateInfo info = {0};
    info.vertex_shader = desc->vertex_shader;
    info.fragment_shader = desc->fragment_shader;
    info.vertex_input_state.vertex_buffer_descriptions = &vbdesc;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_attributes = attrs;
    info.vertex_input_state.num_vertex_attributes = n;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode = desc->cull_mode;
    info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    info.rasterizer_state.enable_depth_clip = true;
    info.target_info.color_target_descriptions = &color;
    info.target_info.num_color_targets = 1;
    if (desc->depth_test) {
        info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        info.depth_stencil_state.enable_depth_test = true;
        info.depth_stencil_state.enable_depth_write = true;
        info.target_info.depth_stencil_format = d->depth_format;
        info.target_info.has_depth_stencil_target = true;
    }

    SDL_GPUGraphicsPipeline *p = SDL_CreateGPUGraphicsPipeline(d->device, &info);
    if (!p) LOG_ERROR("pipeline creation failed: %s", SDL_GetError());
    return p;
}

SDL_GPUBuffer *gpu_make_vertex_buffer(Gpu_Device *d, u32 size) {
    SDL_GPUBufferCreateInfo bi = {.usage = SDL_GPU_BUFFERUSAGE_VERTEX, .size = size};
    return SDL_CreateGPUBuffer(d->device, &bi);
}

// --- Device lifecycle -------------------------------------------------------

// Pick a supported depth format (spec guarantees one of D32/D24, not both).
static SDL_GPUTextureFormat gpu_choose_depth_format(SDL_GPUDevice *device) {
    if (SDL_GPUTextureSupportsFormat(device, SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
            SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) {
        return SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    }
    return SDL_GPU_TEXTUREFORMAT_D24_UNORM;
}

static SDL_GPUTexture *gpu_create_depth_texture(Gpu_Device *d) {
    SDL_GPUTextureCreateInfo info = {0};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = d->depth_format;
    info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    info.width = d->width;
    info.height = d->height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    return SDL_CreateGPUTexture(d->device, &info);
}

b32 gpu_device_init(Gpu_Device *d, SDL_Window *window) {
    d->window = window;

    // Request every format we bake; SDL picks a backend and shaders load the
    // matching blob. SPIR-V covers Vulkan (Linux/Windows), MSL covers Metal.
    SDL_GPUShaderFormat formats = SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL;
    d->device = SDL_CreateGPUDevice(formats, /*debug=*/true, NULL);
    if (!d->device) {
        LOG_ERROR("SDL_CreateGPUDevice failed: %s", SDL_GetError());
        return MACH_FALSE;
    }

    // Narrow to the single format this backend accepts.
    SDL_GPUShaderFormat available = SDL_GetGPUShaderFormats(d->device);
    if (available & SDL_GPU_SHADERFORMAT_MSL) {
        d->shader_format = SDL_GPU_SHADERFORMAT_MSL;
    } else if (available & SDL_GPU_SHADERFORMAT_SPIRV) {
        d->shader_format = SDL_GPU_SHADERFORMAT_SPIRV;
    } else {
        LOG_ERROR("no baked shader format supported by GPU backend");
        SDL_DestroyGPUDevice(d->device);
        d->device = NULL;
        return MACH_FALSE;
    }
    LOG_INFO("GPU device created (driver: %s, shaders: %s)",
             SDL_GetGPUDeviceDriver(d->device),
             d->shader_format == SDL_GPU_SHADERFORMAT_MSL ? "MSL" : "SPIR-V");

    if (!SDL_ClaimWindowForGPUDevice(d->device, window)) {
        LOG_ERROR("SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        SDL_DestroyGPUDevice(d->device);
        d->device = NULL;
        return MACH_FALSE;
    }

    // Decouple present rate from display refresh so the loop's own frame cap
    // governs. MAILBOX is uncapped without tearing; IMMEDIATE is uncapped with
    // tearing; VSYNC is the guaranteed fallback.
    SDL_GPUPresentMode present = SDL_GPU_PRESENTMODE_VSYNC;
    if (SDL_WindowSupportsGPUPresentMode(d->device, window, SDL_GPU_PRESENTMODE_MAILBOX)) {
        present = SDL_GPU_PRESENTMODE_MAILBOX;
    } else if (SDL_WindowSupportsGPUPresentMode(d->device, window, SDL_GPU_PRESENTMODE_IMMEDIATE)) {
        present = SDL_GPU_PRESENTMODE_IMMEDIATE;
    }
    if (!SDL_SetGPUSwapchainParameters(d->device, window,
            SDL_GPU_SWAPCHAINCOMPOSITION_SDR, present)) {
        LOG_ERROR("SDL_SetGPUSwapchainParameters failed: %s", SDL_GetError());
    }
    LOG_INFO("present mode: %s",
             present == SDL_GPU_PRESENTMODE_MAILBOX    ? "mailbox" :
             present == SDL_GPU_PRESENTMODE_IMMEDIATE  ? "immediate" : "vsync");

    i32 w, h;
    SDL_GetWindowSizeInPixels(window, &w, &h);
    d->width = (u32)w;
    d->height = (u32)h;

    d->depth_format = gpu_choose_depth_format(d->device);
    d->depth_texture = gpu_create_depth_texture(d);
    if (!d->depth_texture) {
        LOG_ERROR("depth texture creation failed: %s", SDL_GetError());
        SDL_ReleaseWindowFromGPUDevice(d->device, window);
        SDL_DestroyGPUDevice(d->device);
        d->device = NULL;
        return MACH_FALSE;
    }

    LOG_INFO("GPU device ready (%ux%u, depth %s)", d->width, d->height,
             d->depth_format == SDL_GPU_TEXTUREFORMAT_D32_FLOAT ? "D32F" : "D24");
    return MACH_TRUE;
}

void gpu_device_shutdown(Gpu_Device *d) {
    if (!d->device) return;
    if (d->depth_texture) {
        SDL_ReleaseGPUTexture(d->device, d->depth_texture);
        d->depth_texture = NULL;
    }
    if (d->window) {
        SDL_ReleaseWindowFromGPUDevice(d->device, d->window);
    }
    SDL_DestroyGPUDevice(d->device);
    d->device = NULL;
    LOG_INFO("GPU device shut down");
}

// --- Frame and passes -------------------------------------------------------

b32 gpu_begin_frame(Gpu_Device *d) {
    d->cmd = SDL_AcquireGPUCommandBuffer(d->device);
    if (!d->cmd) {
        LOG_ERROR("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return MACH_FALSE;
    }

    d->swapchain = NULL;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(d->cmd, d->window,
                                               &d->swapchain, NULL, NULL)) {
        LOG_ERROR("SDL_WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        SDL_SubmitGPUCommandBuffer(d->cmd);
        d->cmd = NULL;
        return MACH_FALSE;
    }

    // No swapchain image (e.g. minimized): nothing to draw, but the command
    // buffer must still be submitted.
    if (!d->swapchain) {
        SDL_SubmitGPUCommandBuffer(d->cmd);
        d->cmd = NULL;
        return MACH_FALSE;
    }
    return MACH_TRUE;
}

void gpu_end_frame(Gpu_Device *d) {
    if (d->pass) {  // (npt): defensive — a balanced caller has already ended it
        SDL_EndGPURenderPass(d->pass);
        d->pass = NULL;
    }
    if (d->cmd) {
        SDL_SubmitGPUCommandBuffer(d->cmd);
        d->cmd = NULL;
    }
    d->swapchain = NULL;
}

void gpu_begin_pass(Gpu_Device *d, const Gpu_PassDesc *desc) {
    SDL_GPUColorTargetInfo color = {0};
    color.texture = d->swapchain;
    color.load_op = desc->clear ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
    color.store_op = SDL_GPU_STOREOP_STORE;
    if (desc->clear) {
        color.clear_color = (SDL_FColor){desc->clear_r, desc->clear_g, desc->clear_b, 1.0f};
    }

    if (desc->use_depth) {
        SDL_GPUDepthStencilTargetInfo depth = {0};
        depth.texture = d->depth_texture;
        depth.clear_depth = 1.0f;
        depth.load_op = SDL_GPU_LOADOP_CLEAR;
        depth.store_op = SDL_GPU_STOREOP_DONT_CARE;
        depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        d->pass = SDL_BeginGPURenderPass(d->cmd, &color, 1, &depth);
    } else {
        d->pass = SDL_BeginGPURenderPass(d->cmd, &color, 1, NULL);
    }
}

void gpu_end_pass(Gpu_Device *d) {
    if (d->pass) {
        SDL_EndGPURenderPass(d->pass);
        d->pass = NULL;
    }
}

// --- Draw submission --------------------------------------------------------

void gpu_apply_pipeline(Gpu_Device *d, SDL_GPUGraphicsPipeline *pipeline) {
    SDL_BindGPUGraphicsPipeline(d->pass, pipeline);
}

void gpu_push_vertex_uniform(Gpu_Device *d, u32 slot, const void *data, u32 size) {
    SDL_PushGPUVertexUniformData(d->cmd, slot, data, size);
}

void gpu_push_fragment_uniform(Gpu_Device *d, u32 slot, const void *data, u32 size) {
    SDL_PushGPUFragmentUniformData(d->cmd, slot, data, size);
}

void gpu_bind_vertex_buffer(Gpu_Device *d, SDL_GPUBuffer *vbuf) {
    SDL_GPUBufferBinding vb = {.buffer = vbuf, .offset = 0};
    SDL_BindGPUVertexBuffers(d->pass, 0, &vb, 1);
}

void gpu_bind_index_buffer(Gpu_Device *d, SDL_GPUBuffer *ibuf) {
    SDL_GPUBufferBinding ib = {.buffer = ibuf, .offset = 0};
    SDL_BindGPUIndexBuffer(d->pass, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);
}

void gpu_bind_fragment_sampler(Gpu_Device *d, SDL_GPUTexture *tex, SDL_GPUSampler *sampler) {
    SDL_GPUTextureSamplerBinding b = {.texture = tex, .sampler = sampler};
    SDL_BindGPUFragmentSamplers(d->pass, 0, &b, 1);
}

void gpu_draw_indexed(Gpu_Device *d, u32 index_count) {
    SDL_DrawGPUIndexedPrimitives(d->pass, index_count, 1, 0, 0, 0);
}

void gpu_draw(Gpu_Device *d, u32 vertex_count) {
    SDL_DrawGPUPrimitives(d->pass, vertex_count, 1, 0, 0);
}

void gpu_upload_buffer(Gpu_Device *d, SDL_GPUTransferBuffer *transfer,
                       SDL_GPUBuffer *dst, const void *src, u32 size) {
    void *map = SDL_MapGPUTransferBuffer(d->device, transfer, true);
    memcpy(map, src, size);
    SDL_UnmapGPUTransferBuffer(d->device, transfer);

    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(d->cmd);
    SDL_GPUTransferBufferLocation s = {.transfer_buffer = transfer, .offset = 0};
    SDL_GPUBufferRegion region = {.buffer = dst, .offset = 0, .size = size};
    SDL_UploadToGPUBuffer(copy, &s, &region, true);
    SDL_EndGPUCopyPass(copy);
}
