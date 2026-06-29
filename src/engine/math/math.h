// Math: 2D vectors and scalar helpers.
//
// The engine is 2D, so this is a 2D math library. Vec4 stays as the RGBA color
// type. (3D vector/matrix math was removed with the SDL_GPU renderer; it returns
// if and when 3D does.)

#ifndef MATH_H
#define MATH_H

#include "../base/base.h"

typedef struct {
    f32 x, y;
} Vec2;

typedef struct {
    f32 x, y, z, w;
} Vec4;  // also used as RGBA color

// Scalar
f32 math_min(f32 a, f32 b);
f32 math_max(f32 a, f32 b);
f32 math_clamp(f32 v, f32 lo, f32 hi);
f32 math_lerp(f32 a, f32 b, f32 t);

// Vec2
Vec2 vec2_add(Vec2 a, Vec2 b);
Vec2 vec2_sub(Vec2 a, Vec2 b);
Vec2 vec2_scale(Vec2 v, f32 s);
f32  vec2_dot(Vec2 a, Vec2 b);
f32  vec2_length(Vec2 v);
Vec2 vec2_normalize(Vec2 v);
Vec2 vec2_lerp(Vec2 a, Vec2 b, f32 t);

#endif
