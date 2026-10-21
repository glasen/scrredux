/* core/math.h - vectors, small maths helpers, noise and the game random generator */
#pragma once

#define PI_F 3.14159265f

/* ------------------------------------------------------------------ math */
typedef struct { float x, y, z; } V3;
static inline V3 V(float x, float y, float z) { V3 r = {x, y, z}; return r; }
static inline V3 vadd(V3 a, V3 b) { return V(a.x+b.x, a.y+b.y, a.z+b.z); }
static inline V3 vsub(V3 a, V3 b) { return V(a.x-b.x, a.y-b.y, a.z-b.z); }
static inline V3 vmul(V3 a, float s) { return V(a.x*s, a.y*s, a.z*s); }
static inline float vdot(V3 a, V3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static inline V3 vcross(V3 a, V3 b) { return V(a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x); }
static inline float vlen(V3 a) { return sqrtf(vdot(a, a)); }
static inline V3 vnorm(V3 a) { float l = vlen(a); return l > 1e-9f ? vmul(a, 1/l) : V(0, 0, 0); }
static inline V3 vlerp(V3 a, V3 b, float t) { return vadd(a, vmul(vsub(b, a), t)); }
static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float sq(float x) { return x * x; }
static const V3 UP = {0, 1, 0};
static V3 rotv(V3 v, V3 ax, float a)
{
    float c = cosf(a), s = sinf(a);
    return vadd(vadd(vmul(v, c), vmul(vcross(ax, v), s)), vmul(ax, vdot(ax, v) * (1 - c)));
}
static uint32_t rngs = 12345;
static float frand(void) { rngs = rngs * 1664525u + 1013904223u; return (rngs >> 8) / 16777216.0f; }
static uint32_t hash2(int a, int b)
{
    uint32_t h = (uint32_t)a * 73856093u ^ (uint32_t)b * 19349663u;
    h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15; return h;
}
/* smooth value noise in [0,1] */
static float vnoise(float x, float y, int seed)
{
    int ix = (int)floorf(x), iy = (int)floorf(y);
    float fx = x - ix, fy = y - iy;
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
    #define VN(a, b) ((float)(hash2((a) * 7 + seed * 1013, (b) + seed * 389) & 1023) / 1023.f)
    float a = VN(ix, iy), b = VN(ix + 1, iy), c = VN(ix, iy + 1), d = VN(ix + 1, iy + 1);
    #undef VN
    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}
