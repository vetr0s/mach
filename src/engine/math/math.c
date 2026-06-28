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

f32 math_radians(f32 degrees) {
    return degrees * (3.14159265358979323846f / 180.0f);
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

// Mat4 operations (column-major: element at row r, col c is m[c*4 + r]).

Mat4 mat4_identity(void) {
    Mat4 r = {{0}};
    r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
    return r;
}

Mat4 mat4_mul(Mat4 a, Mat4 b) {
    Mat4 r;
    for (i32 col = 0; col < 4; col++) {
        for (i32 row = 0; row < 4; row++) {
            f32 sum = 0.0f;
            for (i32 k = 0; k < 4; k++) {
                sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
            r.m[col * 4 + row] = sum;
        }
    }
    return r;
}

// Transform a point (w = 1), applying the perspective divide.
Vec3 mat4_mul_point(Mat4 a, Vec3 p) {
    f32 x = a.m[0] * p.x + a.m[4] * p.y + a.m[8]  * p.z + a.m[12];
    f32 y = a.m[1] * p.x + a.m[5] * p.y + a.m[9]  * p.z + a.m[13];
    f32 z = a.m[2] * p.x + a.m[6] * p.y + a.m[10] * p.z + a.m[14];
    f32 w = a.m[3] * p.x + a.m[7] * p.y + a.m[11] * p.z + a.m[15];
    if (w != 0.0f && w != 1.0f) {
        f32 inv = 1.0f / w;
        x *= inv; y *= inv; z *= inv;
    }
    return (Vec3){x, y, z};
}

Mat4 mat4_translate(Vec3 t) {
    Mat4 r = mat4_identity();
    r.m[12] = t.x;
    r.m[13] = t.y;
    r.m[14] = t.z;
    return r;
}

Mat4 mat4_scale(Vec3 s) {
    Mat4 r = {{0}};
    r.m[0] = s.x;
    r.m[5] = s.y;
    r.m[10] = s.z;
    r.m[15] = 1.0f;
    return r;
}

Mat4 mat4_rotate_x(f32 radians) {
    Mat4 r = mat4_identity();
    f32 c = cosf(radians), s = sinf(radians);
    r.m[5] = c;  r.m[9] = -s;
    r.m[6] = s;  r.m[10] = c;
    return r;
}

Mat4 mat4_rotate_y(f32 radians) {
    Mat4 r = mat4_identity();
    f32 c = cosf(radians), s = sinf(radians);
    r.m[0] = c;  r.m[8] = s;
    r.m[2] = -s; r.m[10] = c;
    return r;
}

Mat4 mat4_rotate_z(f32 radians) {
    Mat4 r = mat4_identity();
    f32 c = cosf(radians), s = sinf(radians);
    r.m[0] = c;  r.m[4] = -s;
    r.m[1] = s;  r.m[5] = c;
    return r;
}

// Right-handed view matrix looking from eye toward center.
Mat4 mat4_look_at(Vec3 eye, Vec3 center, Vec3 up) {
    Vec3 f = vec3_normalize(vec3_sub(center, eye));
    Vec3 s = vec3_normalize(vec3_cross(f, up));
    Vec3 u = vec3_cross(s, f);

    Mat4 r = mat4_identity();
    r.m[0] = s.x;  r.m[4] = s.y;  r.m[8]  = s.z;   r.m[12] = -vec3_dot(s, eye);
    r.m[1] = u.x;  r.m[5] = u.y;  r.m[9]  = u.z;   r.m[13] = -vec3_dot(u, eye);
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;  r.m[14] = vec3_dot(f, eye);
    return r;
}

// Right-handed perspective with a [0,1] depth range (Metal/Vulkan/D3D style).
Mat4 mat4_perspective(f32 fov_y, f32 aspect, f32 near_plane, f32 far_plane) {
    f32 f = 1.0f / tanf(fov_y * 0.5f);
    Mat4 r = {{0}};
    r.m[0] = f / aspect;
    r.m[5] = f;
    r.m[10] = far_plane / (near_plane - far_plane);
    r.m[11] = -1.0f;
    r.m[14] = (near_plane * far_plane) / (near_plane - far_plane);
    return r;
}

// Right-handed orthographic with a [0,1] depth range.
Mat4 mat4_ortho(f32 left, f32 right, f32 bottom, f32 top, f32 near_plane, f32 far_plane) {
    Mat4 r = mat4_identity();
    r.m[0] = 2.0f / (right - left);
    r.m[5] = 2.0f / (top - bottom);
    r.m[10] = 1.0f / (near_plane - far_plane);
    r.m[12] = -(right + left) / (right - left);
    r.m[13] = -(top + bottom) / (top - bottom);
    r.m[14] = near_plane / (near_plane - far_plane);
    return r;
}

// General 4x4 inverse via cofactor expansion. Returns identity if singular.
Mat4 mat4_inverse(Mat4 a) {
    const f32 *m = a.m;
    f32 inv[16];

    inv[0]  =  m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] + m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10];
    inv[4]  = -m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] - m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10];
    inv[8]  =  m[4]*m[9]*m[15]  - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] + m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9];
    inv[12] = -m[4]*m[9]*m[14]  + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] - m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9];
    inv[1]  = -m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] - m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10];
    inv[5]  =  m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] + m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10];
    inv[9]  = -m[0]*m[9]*m[15]  + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] - m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9];
    inv[13] =  m[0]*m[9]*m[14]  - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] + m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9];
    inv[2]  =  m[1]*m[6]*m[15]  - m[1]*m[7]*m[14]  - m[5]*m[2]*m[15] + m[5]*m[3]*m[14] + m[13]*m[2]*m[7]  - m[13]*m[3]*m[6];
    inv[6]  = -m[0]*m[6]*m[15]  + m[0]*m[7]*m[14]  + m[4]*m[2]*m[15] - m[4]*m[3]*m[14] - m[12]*m[2]*m[7]  + m[12]*m[3]*m[6];
    inv[10] =  m[0]*m[5]*m[15]  - m[0]*m[7]*m[13]  - m[4]*m[1]*m[15] + m[4]*m[3]*m[13] + m[12]*m[1]*m[7]  - m[12]*m[3]*m[5];
    inv[14] = -m[0]*m[5]*m[14]  + m[0]*m[6]*m[13]  + m[4]*m[1]*m[14] - m[4]*m[2]*m[13] - m[12]*m[1]*m[6]  + m[12]*m[2]*m[5];
    inv[3]  = -m[1]*m[6]*m[11]  + m[1]*m[7]*m[10]  + m[5]*m[2]*m[11] - m[5]*m[3]*m[10] - m[9]*m[2]*m[7]   + m[9]*m[3]*m[6];
    inv[7]  =  m[0]*m[6]*m[11]  - m[0]*m[7]*m[10]  - m[4]*m[2]*m[11] + m[4]*m[3]*m[10] + m[8]*m[2]*m[7]   - m[8]*m[3]*m[6];
    inv[11] = -m[0]*m[5]*m[11]  + m[0]*m[7]*m[9]   + m[4]*m[1]*m[11] - m[4]*m[3]*m[9]  - m[8]*m[1]*m[7]   + m[8]*m[3]*m[5];
    inv[15] =  m[0]*m[5]*m[10]  - m[0]*m[6]*m[9]   - m[4]*m[1]*m[10] + m[4]*m[2]*m[9]  + m[8]*m[1]*m[6]   - m[8]*m[2]*m[5];

    f32 det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12];
    if (det == 0.0f) return mat4_identity();

    f32 inv_det = 1.0f / det;
    Mat4 r;
    for (i32 i = 0; i < 16; i++) r.m[i] = inv[i] * inv_det;
    return r;
}
