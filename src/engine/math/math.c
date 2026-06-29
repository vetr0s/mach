// Math implementation (included into mach.c).

#include "math.h"
#include <math.h>

f32 math_min(f32 a, f32 b) { return a < b ? a : b; }
f32 math_max(f32 a, f32 b) { return a > b ? a : b; }
f32 math_lerp(f32 a, f32 b, f32 t) { return a + (b - a) * t; }

f32 math_clamp(f32 v, f32 lo, f32 hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

Vec2 vec2_add(Vec2 a, Vec2 b)  { return (Vec2){a.x + b.x, a.y + b.y}; }
Vec2 vec2_sub(Vec2 a, Vec2 b)  { return (Vec2){a.x - b.x, a.y - b.y}; }
Vec2 vec2_scale(Vec2 v, f32 s) { return (Vec2){v.x * s, v.y * s}; }
f32  vec2_dot(Vec2 a, Vec2 b)  { return a.x * b.x + a.y * b.y; }
f32  vec2_length(Vec2 v)       { return sqrtf(vec2_dot(v, v)); }

Vec2 vec2_normalize(Vec2 v) {
    f32 len = vec2_length(v);
    if (len == 0.0f) return (Vec2){0.0f, 0.0f};
    return vec2_scale(v, 1.0f / len);
}

Vec2 vec2_lerp(Vec2 a, Vec2 b, f32 t) {
    return (Vec2){math_lerp(a.x, b.x, t), math_lerp(a.y, b.y, t)};
}
