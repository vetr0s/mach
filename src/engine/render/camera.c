// Camera implementation (included into mach.c).

#include "camera.h"

Mat4 camera_view(const Camera *c) {
    return mat4_look_at(c->position, c->target, c->up);
}

Mat4 camera_proj(const Camera *c) {
    if (c->projection == CAMERA_ORTHOGRAPHIC) {
        f32 half_h = c->ortho_size;
        f32 half_w = c->ortho_size * c->aspect;
        return mat4_ortho(-half_w, half_w, -half_h, half_h, c->near_plane, c->far_plane);
    }
    return mat4_perspective(c->fov_y, c->aspect, c->near_plane, c->far_plane);
}

Mat4 camera_view_proj(const Camera *c) {
    return mat4_mul(camera_proj(c), camera_view(c));
}

void camera_screen_ray(const Camera *c, f32 screen_x, f32 screen_y,
                       i32 screen_w, i32 screen_h, Vec3 *out_origin, Vec3 *out_dir) {
    // Pixel -> NDC. Screen Y is top-down; NDC Y is bottom-up, so flip it.
    f32 ndc_x = (2.0f * screen_x / (f32)screen_w) - 1.0f;
    f32 ndc_y = 1.0f - (2.0f * screen_y / (f32)screen_h);

    Mat4 inv = mat4_inverse(camera_view_proj(c));

    // Unproject the near (z=0) and far (z=1) points; the perspective divide is
    // applied inside mat4_mul_point.
    Vec3 near_pt = mat4_mul_point(inv, (Vec3){ndc_x, ndc_y, 0.0f});
    Vec3 far_pt  = mat4_mul_point(inv, (Vec3){ndc_x, ndc_y, 1.0f});

    *out_origin = near_pt;
    *out_dir = vec3_normalize(vec3_sub(far_pt, near_pt));
}
