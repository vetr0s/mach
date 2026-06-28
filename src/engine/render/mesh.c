// Mesh implementation (included into mach.c).

#include "mesh.h"
#include "../debug.h"
#include <string.h>

Mesh mesh_create(SDL_GPUDevice *device, const Vertex3D *verts, i32 vcount,
                 const u16 *indices, i32 icount) {
    Mesh mesh = {0};
    mesh.index_count = icount;

    u32 vbytes = (u32)vcount * sizeof(Vertex3D);
    u32 ibytes = (u32)icount * sizeof(u16);

    SDL_GPUBufferCreateInfo vci = {.usage = SDL_GPU_BUFFERUSAGE_VERTEX, .size = vbytes};
    SDL_GPUBufferCreateInfo ici = {.usage = SDL_GPU_BUFFERUSAGE_INDEX, .size = ibytes};
    mesh.vbuf = SDL_CreateGPUBuffer(device, &vci);
    mesh.ibuf = SDL_CreateGPUBuffer(device, &ici);
    if (!mesh.vbuf || !mesh.ibuf) {
        LOG_ERROR("mesh_create: buffer creation failed: %s", SDL_GetError());
        mesh_destroy(device, &mesh);
        return (Mesh){0};
    }

    // Stage vertex + index data in one transfer buffer, then copy to GPU.
    SDL_GPUTransferBufferCreateInfo tci = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD, .size = vbytes + ibytes};
    SDL_GPUTransferBuffer *tbuf = SDL_CreateGPUTransferBuffer(device, &tci);
    if (!tbuf) {
        LOG_ERROR("mesh_create: transfer buffer failed: %s", SDL_GetError());
        mesh_destroy(device, &mesh);
        return (Mesh){0};
    }

    u8 *map = (u8 *)SDL_MapGPUTransferBuffer(device, tbuf, false);
    memcpy(map, verts, vbytes);
    memcpy(map + vbytes, indices, ibytes);
    SDL_UnmapGPUTransferBuffer(device, tbuf);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTransferBufferLocation vsrc = {.transfer_buffer = tbuf, .offset = 0};
    SDL_GPUBufferRegion vdst = {.buffer = mesh.vbuf, .offset = 0, .size = vbytes};
    SDL_UploadToGPUBuffer(copy, &vsrc, &vdst, false);

    SDL_GPUTransferBufferLocation isrc = {.transfer_buffer = tbuf, .offset = vbytes};
    SDL_GPUBufferRegion idst = {.buffer = mesh.ibuf, .offset = 0, .size = ibytes};
    SDL_UploadToGPUBuffer(copy, &isrc, &idst, false);

    SDL_EndGPUCopyPass(copy);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(device, tbuf);

    return mesh;
}

void mesh_destroy(SDL_GPUDevice *device, Mesh *mesh) {
    if (mesh->vbuf) { SDL_ReleaseGPUBuffer(device, mesh->vbuf); mesh->vbuf = NULL; }
    if (mesh->ibuf) { SDL_ReleaseGPUBuffer(device, mesh->ibuf); mesh->ibuf = NULL; }
    mesh->index_count = 0;
}

Mesh mesh_cube(SDL_GPUDevice *device) {
    const f32 h = 0.5f;
    // 24 vertices: 4 per face with that face's outward normal. CCW when viewed
    // from outside (front faces are counter-clockwise).
    const Vertex3D v[24] = {
        // +X
        {{ h,-h,-h},{1,0,0}}, {{ h,-h, h},{1,0,0}}, {{ h, h, h},{1,0,0}}, {{ h, h,-h},{1,0,0}},
        // -X
        {{-h,-h, h},{-1,0,0}}, {{-h,-h,-h},{-1,0,0}}, {{-h, h,-h},{-1,0,0}}, {{-h, h, h},{-1,0,0}},
        // +Y
        {{-h, h,-h},{0,1,0}}, {{ h, h,-h},{0,1,0}}, {{ h, h, h},{0,1,0}}, {{-h, h, h},{0,1,0}},
        // -Y
        {{-h,-h, h},{0,-1,0}}, {{ h,-h, h},{0,-1,0}}, {{ h,-h,-h},{0,-1,0}}, {{-h,-h,-h},{0,-1,0}},
        // +Z
        {{-h,-h, h},{0,0,1}}, {{ h,-h, h},{0,0,1}}, {{ h, h, h},{0,0,1}}, {{-h, h, h},{0,0,1}},
        // -Z
        {{ h,-h,-h},{0,0,-1}}, {{-h,-h,-h},{0,0,-1}}, {{-h, h,-h},{0,0,-1}}, {{ h, h,-h},{0,0,-1}},
    };
    u16 idx[36];
    for (i32 f = 0; f < 6; f++) {
        u16 b = (u16)(f * 4);
        idx[f*6+0] = b;     idx[f*6+1] = b + 1; idx[f*6+2] = b + 2;
        idx[f*6+3] = b;     idx[f*6+4] = b + 2; idx[f*6+5] = b + 3;
    }
    return mesh_create(device, v, 24, idx, 36);
}

Mesh mesh_plane(SDL_GPUDevice *device) {
    const f32 h = 0.5f;
    const Vertex3D v[4] = {
        {{-h, 0, -h}, {0, 1, 0}},
        {{ h, 0, -h}, {0, 1, 0}},
        {{ h, 0,  h}, {0, 1, 0}},
        {{-h, 0,  h}, {0, 1, 0}},
    };
    const u16 idx[6] = {0, 1, 2, 0, 2, 3};
    return mesh_create(device, v, 4, idx, 6);
}
