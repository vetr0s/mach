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

// Scalar math

f32 math_lerp(f32 a, f32 b, f32 t);
f32 math_clamp(f32 v, f32 min, f32 max);
f32 math_min(f32 a, f32 b);
f32 math_max(f32 a, f32 b);
f32 math_abs(f32 v);
f32 math_sqrt(f32 v);

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

#endif
