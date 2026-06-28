// GPU mesh: vertex/index buffers plus procedural primitive generators.

#ifndef MESH_H
#define MESH_H

#include <SDL3/SDL.h>
#include "../base/base.h"
#include "../math/math.h"

// Interleaved vertex: position + normal. Per-instance color is a draw uniform.
typedef struct {
    Vec3 position;
    Vec3 normal;
} Vertex3D;

typedef struct {
    SDL_GPUBuffer *vbuf;
    SDL_GPUBuffer *ibuf;
    i32 index_count;
} Mesh;

// Upload a mesh to the GPU (16-bit indices). Returns a zeroed Mesh on failure.
Mesh mesh_create(SDL_GPUDevice *device, const Vertex3D *verts, i32 vcount,
                 const u16 *indices, i32 icount);
void mesh_destroy(SDL_GPUDevice *device, Mesh *mesh);

// Primitives, centered on the origin, 1 unit in size.
Mesh mesh_cube(SDL_GPUDevice *device);   // [-0.5,0.5]^3, per-face normals
Mesh mesh_plane(SDL_GPUDevice *device);  // XZ unit quad, normal +Y

#endif
