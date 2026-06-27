// Math implementation (included into mach.c).

#include "math.h"

// Scalar math

f32 math_lerp(f32 a, f32 b, f32 t) {
    return a + (b - a) * t;
}

f32 math_clamp(f32 v, f32 min_val, f32 max_val) {
    if (v < min_val) return min_val;
    if (v > max_val) return max_val;
    return v;
}

f32 math_min(f32 a, f32 b) {
    return a < b ? a : b;
}

f32 math_max(f32 a, f32 b) {
    return a > b ? a : b;
}

f32 math_abs(f32 v) {
    return v < 0 ? -v : v;
}

f32 math_sqrt(f32 v) {
    return sqrtf(v);
}

// Vec2 operations

Vec2 vec2(f32 x, f32 y) {
    return (Vec2){x, y};
}

Vec2 vec2_add(Vec2 a, Vec2 b) {
    return (Vec2){a.x + b.x, a.y + b.y};
}

Vec2 vec2_sub(Vec2 a, Vec2 b) {
    return (Vec2){a.x - b.x, a.y - b.y};
}

Vec2 vec2_scale(Vec2 v, f32 s) {
    return (Vec2){v.x * s, v.y * s};
}

Vec2 vec2_negate(Vec2 v) {
    return (Vec2){-v.x, -v.y};
}

f32 vec2_dot(Vec2 a, Vec2 b) {
    return a.x * b.x + a.y * b.y;
}

f32 vec2_length(Vec2 v) {
    return math_sqrt(vec2_dot(v, v));
}

f32 vec2_distance(Vec2 a, Vec2 b) {
    return vec2_length(vec2_sub(b, a));
}

Vec2 vec2_normalize(Vec2 v) {
    f32 len = vec2_length(v);
    if (len == 0.0f) return (Vec2){0, 0};
    return vec2_scale(v, 1.0f / len);
}

Vec2 vec2_lerp(Vec2 a, Vec2 b, f32 t) {
    return (Vec2){
        math_lerp(a.x, b.x, t),
        math_lerp(a.y, b.y, t)
    };
}

i32 vec2_equal(Vec2 a, Vec2 b) {
    return a.x == b.x && a.y == b.y;
}

// Vec3 operations

Vec3 vec3(f32 x, f32 y, f32 z) {
    return (Vec3){x, y, z};
}

Vec3 vec3_add(Vec3 a, Vec3 b) {
    return (Vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 vec3_sub(Vec3 a, Vec3 b) {
    return (Vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 vec3_scale(Vec3 v, f32 s) {
    return (Vec3){v.x * s, v.y * s, v.z * s};
}

Vec3 vec3_cross(Vec3 a, Vec3 b) {
    return (Vec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

f32 vec3_dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

f32 vec3_length(Vec3 v) {
    return math_sqrt(vec3_dot(v, v));
}

Vec3 vec3_normalize(Vec3 v) {
    f32 len = vec3_length(v);
    if (len == 0.0f) return (Vec3){0, 0, 0};
    return vec3_scale(v, 1.0f / len);
}
