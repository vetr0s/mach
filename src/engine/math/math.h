// Math library: vectors, scalars, and common operations.

#ifndef MATH_H
#define MATH_H

#include "../base/base.h"
#include <math.h>

// Vector types

typedef struct {
    f32 x, y;
} Vec2;

typedef struct {
    f32 x, y, z;
} Vec3;

typedef struct {
    f32 x, y, z, w;
} Vec4;

// 4x4 matrix, column-major (m[col*4 + row]) to match Metal/GLSL conventions.
typedef struct {
    f32 m[16];
} Mat4;

// Scalar math

f32 math_lerp(f32 a, f32 b, f32 t);
f32 math_clamp(f32 v, f32 min, f32 max);
f32 math_min(f32 a, f32 b);
f32 math_max(f32 a, f32 b);
f32 math_abs(f32 v);
f32 math_sqrt(f32 v);
f32 math_radians(f32 degrees);

// Vec2 operations

Vec2 vec2(f32 x, f32 y);
Vec2 vec2_add(Vec2 a, Vec2 b);
Vec2 vec2_sub(Vec2 a, Vec2 b);
Vec2 vec2_scale(Vec2 v, f32 s);
Vec2 vec2_negate(Vec2 v);
f32 vec2_dot(Vec2 a, Vec2 b);
f32 vec2_length(Vec2 v);
f32 vec2_distance(Vec2 a, Vec2 b);
Vec2 vec2_normalize(Vec2 v);
Vec2 vec2_lerp(Vec2 a, Vec2 b, f32 t);
i32 vec2_equal(Vec2 a, Vec2 b);

// Vec3 operations

Vec3 vec3(f32 x, f32 y, f32 z);
Vec3 vec3_add(Vec3 a, Vec3 b);
Vec3 vec3_sub(Vec3 a, Vec3 b);
Vec3 vec3_scale(Vec3 v, f32 s);
Vec3 vec3_cross(Vec3 a, Vec3 b);
f32 vec3_dot(Vec3 a, Vec3 b);
f32 vec3_length(Vec3 v);
Vec3 vec3_normalize(Vec3 v);

// Mat4 operations (column-major). Transform builders assume right-handed world
// space; projections map to a [0,1] depth range to match SDL_GPU/Metal/Vulkan.

Mat4 mat4_identity(void);
Mat4 mat4_mul(Mat4 a, Mat4 b);              // a * b
Vec3 mat4_mul_point(Mat4 a, Vec3 p);        // transform point (w = 1)
Mat4 mat4_translate(Vec3 t);
Mat4 mat4_scale(Vec3 s);
Mat4 mat4_rotate_x(f32 radians);
Mat4 mat4_rotate_y(f32 radians);
Mat4 mat4_rotate_z(f32 radians);
Mat4 mat4_look_at(Vec3 eye, Vec3 center, Vec3 up);
Mat4 mat4_perspective(f32 fov_y, f32 aspect, f32 near_plane, f32 far_plane);
Mat4 mat4_ortho(f32 left, f32 right, f32 bottom, f32 top, f32 near_plane, f32 far_plane);
Mat4 mat4_inverse(Mat4 a);

#endif
