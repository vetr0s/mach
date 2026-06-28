// Camera: view/projection matrices and mouse-pick rays.
//
// The camera is projection-agnostic. Isometric is just an orthographic camera
// positioned at a particular angle (configured by the game), so the engine is
// not tied to any single look.

#ifndef CAMERA_H
#define CAMERA_H

#include "../base/base.h"
#include "../math/math.h"

typedef enum {
    CAMERA_PERSPECTIVE,
    CAMERA_ORTHOGRAPHIC,
} Camera_Projection;

typedef struct {
    Vec3 position;
    Vec3 target;
    Vec3 up;

    Camera_Projection projection;
    f32 fov_y;       // vertical field of view in radians (perspective)
    f32 ortho_size;  // half-height of the view volume in world units (orthographic)
    f32 near_plane;
    f32 far_plane;
    f32 aspect;      // width / height
} Camera;

Mat4 camera_view(const Camera *c);
Mat4 camera_proj(const Camera *c);
Mat4 camera_view_proj(const Camera *c);

// Unproject a screen pixel (top-left origin, +Y down) into a world-space ray.
void camera_screen_ray(const Camera *c, f32 screen_x, f32 screen_y,
                       i32 screen_w, i32 screen_h, Vec3 *out_origin, Vec3 *out_dir);

#endif
