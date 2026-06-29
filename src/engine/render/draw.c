// Draw batteries implementation (Layer 2; included into mach.c).
//
// (npt): The built-in shaders are authored in HLSL and cross-compiled offline
// into shaders_generated.h (SPIR-V for Vulkan, MSL for Metal). These pipelines
// belong to the batteries, not the GPU core — the core stays content-agnostic.

#include "draw.h"
#include "../debug.h"
#include "shaders_generated.h"
#include <stddef.h>

// Uniform block layouts. float4 used throughout to dodge std140/float3 packing.
typedef struct { Mat4 view_proj; } Gpu_FrameVS;     // vertex slot 0 (lit)
typedef struct { Mat4 model;     } Gpu_DrawVS;       // vertex slot 1 (lit)
typedef struct { Vec4 dir; Vec4 ambient; } Gpu_LightFS;  // fragment slot 0 (lit)
typedef struct { Vec4 rgba;      } Gpu_ColorFS;      // fragment slot 1 (lit)
typedef struct { Mat4 proj;      } Gpu_Frame2D;      // vertex slot 0 (overlay)

#define GPU_AMBIENT 0.30f

// --- Pipelines --------------------------------------------------------------

static b32 draw_create_pipeline_3d(Draw *dr) {
    SDL_GPUShader *vs = gpu_make_shader(dr->gpu,
        shader_lit_vert_spv, shader_lit_vert_spv_len,
        shader_lit_vert_msl, shader_lit_vert_msl_len,
        SDL_GPU_SHADERSTAGE_VERTEX, 2, 0);
    SDL_GPUShader *fs = gpu_make_shader(dr->gpu,
        shader_lit_frag_spv, shader_lit_frag_spv_len,
        shader_lit_frag_msl, shader_lit_frag_msl_len,
        SDL_GPU_SHADERSTAGE_FRAGMENT, 2, 0);
    if (!vs || !fs) {
        if (vs) SDL_ReleaseGPUShader(dr->gpu->device, vs);
        if (fs) SDL_ReleaseGPUShader(dr->gpu->device, fs);
        return MACH_FALSE;
    }

    Gpu_VertexAttr attrs[2] = {
        {.location = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = 0},
        {.location = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = offsetof(Vertex3D, normal)},
    };
    Gpu_PipelineDesc desc = {
        .vertex_shader = vs, .fragment_shader = fs,
        .vertex_pitch = sizeof(Vertex3D), .attrs = attrs, .num_attrs = 2,
        // (npt): No culling — cubes are closed (depth resolves overlaps) and the
        // ground plane must show from above regardless of winding.
        .depth_test = MACH_TRUE, .alpha_blend = MACH_FALSE,
        .cull_mode = SDL_GPU_CULLMODE_NONE,
    };
    dr->pipeline_3d = gpu_make_pipeline(dr->gpu, &desc);
    SDL_ReleaseGPUShader(dr->gpu->device, vs);
    SDL_ReleaseGPUShader(dr->gpu->device, fs);
    return dr->pipeline_3d != NULL;
}

static b32 draw_create_pipeline_2d(Draw *dr) {
    SDL_GPUShader *vs = gpu_make_shader(dr->gpu,
        shader_overlay_vert_spv, shader_overlay_vert_spv_len,
        shader_overlay_vert_msl, shader_overlay_vert_msl_len,
        SDL_GPU_SHADERSTAGE_VERTEX, 1, 0);
    SDL_GPUShader *fs = gpu_make_shader(dr->gpu,
        shader_overlay_frag_spv, shader_overlay_frag_spv_len,
        shader_overlay_frag_msl, shader_overlay_frag_msl_len,
        SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 1);
    if (!vs || !fs) {
        if (vs) SDL_ReleaseGPUShader(dr->gpu->device, vs);
        if (fs) SDL_ReleaseGPUShader(dr->gpu->device, fs);
        return MACH_FALSE;
    }

    Gpu_VertexAttr attrs[3] = {
        {.location = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = offsetof(Vertex2D, pos)},
        {.location = 1, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, .offset = offsetof(Vertex2D, uv)},
        {.location = 2, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, .offset = offsetof(Vertex2D, color)},
    };
    Gpu_PipelineDesc desc = {
        .vertex_shader = vs, .fragment_shader = fs,
        .vertex_pitch = sizeof(Vertex2D), .attrs = attrs, .num_attrs = 3,
        .depth_test = MACH_FALSE, .alpha_blend = MACH_TRUE,
        .cull_mode = SDL_GPU_CULLMODE_NONE,
    };
    dr->pipeline_2d = gpu_make_pipeline(dr->gpu, &desc);
    SDL_ReleaseGPUShader(dr->gpu->device, vs);
    SDL_ReleaseGPUShader(dr->gpu->device, fs);
    return dr->pipeline_2d != NULL;
}

// --- Lifecycle --------------------------------------------------------------

b32 draw_init(Draw *dr, Gpu_Device *gpu) {
    dr->gpu = gpu;
    dr->overlay_count = 0;

    if (!draw_create_pipeline_3d(dr) || !draw_create_pipeline_2d(dr)) {
        return MACH_FALSE;
    }

    u32 overlay_bytes = DRAW_MAX_OVERLAY_QUADS * 6 * sizeof(Vertex2D);
    dr->overlay_cpu = (Vertex2D *)SDL_malloc(overlay_bytes);
    dr->overlay_vbuf = gpu_make_vertex_buffer(gpu, overlay_bytes);
    SDL_GPUTransferBufferCreateInfo oti = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = overlay_bytes};
    dr->overlay_transfer = SDL_CreateGPUTransferBuffer(gpu->device, &oti);
    if (!dr->overlay_cpu || !dr->overlay_vbuf || !dr->overlay_transfer) {
        LOG_ERROR("draw_init: overlay buffer allocation failed: %s", SDL_GetError());
        return MACH_FALSE;
    }

    dr->font = font_create(gpu->device);
    if (!dr->font) return MACH_FALSE;

    LOG_INFO("draw batteries ready");
    return MACH_TRUE;
}

void draw_shutdown(Draw *dr) {
    if (!dr->gpu) return;
    SDL_GPUDevice *device = dr->gpu->device;
    if (dr->font) { font_destroy(device, dr->font); dr->font = NULL; }
    if (dr->overlay_cpu) { SDL_free(dr->overlay_cpu); dr->overlay_cpu = NULL; }
    if (dr->overlay_transfer) {
        SDL_ReleaseGPUTransferBuffer(device, dr->overlay_transfer);
        dr->overlay_transfer = NULL;
    }
    if (dr->overlay_vbuf) {
        SDL_ReleaseGPUBuffer(device, dr->overlay_vbuf);
        dr->overlay_vbuf = NULL;
    }
    if (dr->pipeline_2d) {
        SDL_ReleaseGPUGraphicsPipeline(device, dr->pipeline_2d);
        dr->pipeline_2d = NULL;
    }
    if (dr->pipeline_3d) {
        SDL_ReleaseGPUGraphicsPipeline(device, dr->pipeline_3d);
        dr->pipeline_3d = NULL;
    }
}

// --- 3D ---------------------------------------------------------------------

void draw_begin_3d(Draw *dr, Mat4 view_proj, Vec3 light_dir) {
    gpu_apply_pipeline(dr->gpu, dr->pipeline_3d);

    Gpu_FrameVS frame = {view_proj};
    gpu_push_vertex_uniform(dr->gpu, 0, &frame, sizeof(frame));

    Gpu_LightFS light;
    light.dir = (Vec4){light_dir.x, light_dir.y, light_dir.z, 0.0f};
    light.ambient = (Vec4){GPU_AMBIENT, 0.0f, 0.0f, 0.0f};
    gpu_push_fragment_uniform(dr->gpu, 0, &light, sizeof(light));
}

void draw_mesh(Draw *dr, const Mesh *mesh, Mat4 model, Vec4 color) {
    Gpu_DrawVS draw = {model};
    gpu_push_vertex_uniform(dr->gpu, 1, &draw, sizeof(draw));
    Gpu_ColorFS col = {color};
    gpu_push_fragment_uniform(dr->gpu, 1, &col, sizeof(col));

    gpu_bind_vertex_buffer(dr->gpu, mesh->vbuf);
    gpu_bind_index_buffer(dr->gpu, mesh->ibuf);
    gpu_draw_indexed(dr->gpu, (u32)mesh->index_count);
}

// --- 2D overlay -------------------------------------------------------------

// Append one quad (two triangles) in screen space with the given atlas UVs.
static void overlay_quad(Draw *dr, f32 x, f32 y, f32 w, f32 h,
                         f32 u0, f32 v0, f32 u1, f32 v1, Vec4 c) {
    if (dr->overlay_count + 6 > DRAW_MAX_OVERLAY_QUADS * 6) return;
    Vertex2D *v = &dr->overlay_cpu[dr->overlay_count];
    v[0] = (Vertex2D){{x,     y},     {u0, v0}, c};
    v[1] = (Vertex2D){{x,     y + h}, {u0, v1}, c};
    v[2] = (Vertex2D){{x + w, y + h}, {u1, v1}, c};
    v[3] = (Vertex2D){{x,     y},     {u0, v0}, c};
    v[4] = (Vertex2D){{x + w, y + h}, {u1, v1}, c};
    v[5] = (Vertex2D){{x + w, y},     {u1, v0}, c};
    dr->overlay_count += 6;
}

void draw_rect(Draw *dr, f32 x, f32 y, f32 w, f32 h, Vec4 color) {
    f32 u = dr->font->white_u, v = dr->font->white_v;
    overlay_quad(dr, x, y, w, h, u, v, u, v, color);
}

void draw_text(Draw *dr, f32 x, f32 y, f32 scale, const char *text, Vec4 color) {
    const Font *font = dr->font;
    if (!text) return;

    f32 gw = (f32)font->glyph_w * scale;
    f32 gh = (f32)font->glyph_h * scale;
    f32 adv = (f32)font->advance * scale;
    f32 cur_x = x;
    for (const char *c = text; *c; c++) {
        f32 u0, v0, u1, v1;
        if (font_glyph_uv(font, *c, &u0, &v0, &u1, &v1)) {
            overlay_quad(dr, cur_x, y, gw, gh, u0, v0, u1, v1, color);
        }
        cur_x += adv;
    }
}

void draw_flush_overlay(Draw *dr) {
    if (dr->overlay_count > 0) {
        Gpu_Device *gpu = dr->gpu;
        u32 bytes = dr->overlay_count * sizeof(Vertex2D);
        gpu_upload_buffer(gpu, dr->overlay_transfer, dr->overlay_vbuf, dr->overlay_cpu, bytes);

        // Color-only pass that loads (preserves) the rendered scene.
        gpu_begin_pass(gpu, &(Gpu_PassDesc){.clear = MACH_FALSE, .use_depth = MACH_FALSE});
        gpu_apply_pipeline(gpu, dr->pipeline_2d);
        Gpu_Frame2D frame = {mat4_ortho(0.0f, (f32)gpu->width, (f32)gpu->height, 0.0f, -1.0f, 1.0f)};
        gpu_push_vertex_uniform(gpu, 0, &frame, sizeof(frame));
        gpu_bind_vertex_buffer(gpu, dr->overlay_vbuf);
        gpu_bind_fragment_sampler(gpu, dr->font->atlas, dr->font->sampler);
        gpu_draw(gpu, dr->overlay_count);
        gpu_end_pass(gpu);
    }
    dr->overlay_count = 0;  // start fresh next frame, whether or not we drew
}
