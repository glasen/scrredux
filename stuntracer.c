/*
 * STUNT RACER - a Stunt Car Racer inspired roller-coaster racer in a stadium.
 * SDL3 (window, input, audio) + SDL_GPU (rendering). Flat-shaded polygons, no textures.
 *
 * The car is a real rigid body: 4 spring/damper suspensions, tyre friction with
 * weight transfer, full 3D rotation, solid track deck / barrier collisions.
 *
 * Controls: arrows/WASD drive, SPACE boost, C camera, R reset to track,
 *           P pause, F11 fullscreen, ESC menu/quit.
 *
 * Debug:  stuntracer --sim SECONDS [TRACK]
 *         stuntracer --shot FILE.png SECONDS [TRACK] [CAM]
 *
 * Layout: this file = maths, world geometry, physics, AI, car model, game logic.
 *         scenery.h = stadium, arena floor and props, service vehicles.
 *         sr_main.h = HUD, PNG writer, SDL_GPU layer and main().
 */
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define FOCAL_Y 2.2222f           /* 1/tan(fovy/2) */
#define NEAR_Z 0.25f
#define FAR_Z 6000.0f
#define FOG0 300.0f
#define FOG1 3500.0f
/* ---- tunable vehicle physics (menu F2, saved to physics.cfg) ---- */
typedef struct {
    float mass, grav, power, fmax, boostp, boostf, brkf, brkr, drag, mu, corner, lowspd, loadmax;
    float steer, steerfall, steerrate, kf, kr, ccomp, creb, kbump;
    float sroll, syaw, spitch, air, leanr, leanp, rgrip, steerlat, keyrise, keyret;
} Phys;
static Phys ph;
#define GRAV (ph.grav)
#define MAXSEG 6000
#define TRACK_HW 6.9f
#define PI_F 3.14159265f
#define FOGCOL 0xB8CCE0u
#define ZENCOL 0x3060B8u
#define CAR_RED 0xC81C14u           /* player car paint (tinted x1.25 by the shader) */
#define VH 270.0f                 /* HUD virtual height */

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
static float steer_max(float v)
{
    float d = ph.steer / (1.f + (v / ph.steerfall) * (v / ph.steerfall));
    float lim = atanf(2.67f * ph.steerlat / fmaxf(v * v, 1.f));   /* never steer harder than the tyres can hold: no oversteer snap */
    return fminf(d, lim);
}

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

/* 5x7 bitmap font (HUD and lettering in the world) */
static const char FONT_CHARS[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.:-/!+<>?,";
static const uint8_t FONT[][7] = {
 {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
 {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},{0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E},
 {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
 {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},{0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
 {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},{0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
 {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
 {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},{0x1E,0x11,0x11,0x11,0x11,0x11,0x1E},
 {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
 {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},{0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
 {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},{0x07,0x02,0x02,0x02,0x02,0x12,0x0C},
 {0x11,0x12,0x14,0x18,0x14,0x12,0x11},{0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
 {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},{0x11,0x19,0x15,0x13,0x11,0x11,0x11},
 {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},{0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
 {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
 {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},{0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
 {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},{0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
 {0x11,0x11,0x11,0x15,0x15,0x1B,0x11},{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
 {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
 {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C},{0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00},
 {0x00,0x00,0x00,0x1F,0x00,0x00,0x00},{0x01,0x01,0x02,0x04,0x08,0x10,0x10},
 {0x04,0x04,0x04,0x04,0x04,0x00,0x04},{0x00,0x04,0x04,0x1F,0x04,0x04,0x00},
 {0x02,0x04,0x08,0x10,0x08,0x04,0x02},{0x08,0x04,0x02,0x01,0x02,0x04,0x08},
 {0x0E,0x11,0x01,0x02,0x04,0x00,0x04},{0x00,0x00,0x00,0x00,0x0C,0x04,0x08},
};

/* ------------------------------------------------------- vertex buffers */
typedef struct { float x, y, z; uint8_t r, g, b, a; } Vtx;
typedef struct { Vtx *v; int n, cap; } VB;
static VB sb, db, hb;            /* static world, dynamic world, HUD */
static VB *cur = &db;
static struct { V3 pos, right, up, fwd; } cam;

static void vb_reserve(VB *b, int add)
{
    if (b->n + add <= b->cap) return;
    int nc = b->cap ? b->cap : 1 << 16;
    while (nc < b->n + add) nc *= 2;
    b->v = realloc(b->v, (size_t)nc * sizeof(Vtx)); b->cap = nc;
}
static void emit(V3 p, uint32_t c, uint8_t a)
{
    Vtx *v = &cur->v[cur->n++];
    v->x = p.x; v->y = p.y; v->z = p.z;
    v->r = (c >> 16) & 255; v->g = (c >> 8) & 255; v->b = c & 255; v->a = a;
}
static int g_alpha = 255;                         /* 255 = lit, 128 = tintable body paint (car generator) */
static void draw_tri(V3 a, V3 b, V3 c, uint32_t col)
{
    vb_reserve(cur, 3); emit(a, col, (uint8_t)g_alpha); emit(b, col, (uint8_t)g_alpha); emit(c, col, (uint8_t)g_alpha);
}
static void draw_quad(V3 a, V3 b, V3 c, V3 d, uint32_t col) { draw_tri(a, b, c, col); draw_tri(a, c, d, col); }
/* per-corner colours, blended across the face */
static void draw_quad_c(V3 a, V3 b, V3 c, V3 d, uint32_t ca, uint32_t cb, uint32_t cc, uint32_t cd)
{
    vb_reserve(cur, 6);
    emit(a, ca, (uint8_t)g_alpha); emit(b, cb, (uint8_t)g_alpha); emit(c, cc, (uint8_t)g_alpha);
    emit(a, ca, (uint8_t)g_alpha); emit(c, cc, (uint8_t)g_alpha); emit(d, cd, (uint8_t)g_alpha);
}
static void draw_tri_unlit(V3 a, V3 b, V3 c, uint32_t col)
{
    vb_reserve(cur, 3); emit(a, col, 0); emit(b, col, 0); emit(c, col, 0);
}
static void draw_quad_unlit(V3 a, V3 b, V3 c, V3 d, uint32_t col) { draw_tri_unlit(a, b, c, col); draw_tri_unlit(a, c, d, col); }
static uint32_t mixcol(uint32_t a, uint32_t b, float t)
{
    int r = (int)(((a >> 16) & 255) * (1 - t) + ((b >> 16) & 255) * t);
    int g = (int)(((a >> 8) & 255) * (1 - t) + ((b >> 8) & 255) * t);
    int bl = (int)((a & 255) * (1 - t) + (b & 255) * t);
    return (uint32_t)(r << 16 | g << 8 | bl);
}
static uint32_t scalecol(uint32_t c, float s)
{
    int r = (int)clampf(((c >> 16) & 255) * s, 0, 255), g = (int)clampf(((c >> 8) & 255) * s, 0, 255), b = (int)clampf((c & 255) * s, 0, 255);
    return (uint32_t)(r << 16 | g << 8 | b);
}

static void obox(V3 c, V3 ax, V3 ay, V3 az, float hx, float hy, float hz, uint32_t col)
{
    V3 X = vmul(ax, hx), Y = vmul(ay, hy), Z = vmul(az, hz), v[8];
    for (int i = 0; i < 8; i++) {
        float sx = (i & 1) ? 1.f : -1.f, sy = (i & 2) ? 1.f : -1.f, sz = (i & 4) ? 1.f : -1.f;
        v[i] = vadd(c, vadd(vmul(X, sx), vadd(vmul(Y, sy), vmul(Z, sz))));
    }
    static const int f[6][4] = {{0,1,3,2},{4,5,7,6},{0,4,5,1},{2,3,7,6},{0,2,6,4},{1,5,7,3}};
    for (int i = 0; i < 6; i++) draw_quad(v[f[i][0]], v[f[i][1]], v[f[i][2]], v[f[i][3]], col);
}
/* tapered cylinder between two points; caps: 1 = start, 2 = end */
static void cyl(V3 a, V3 b, float r0, float r1, int n, uint32_t col, int caps)
{
    V3 ax = vsub(b, a); float L = vlen(ax);
    if (L < 1e-6f) return;
    ax = vmul(ax, 1 / L);
    V3 t = fabsf(ax.y) < 0.9f ? UP : V(1, 0, 0);
    V3 u = vnorm(vcross(ax, t)), w = vcross(ax, u), pa[32], pb[32];
    if (n > 32) n = 32;
    for (int k = 0; k < n; k++) {
        float an = 2 * PI_F * k / n;
        V3 d = vadd(vmul(u, cosf(an)), vmul(w, sinf(an)));
        pa[k] = vadd(a, vmul(d, r0)); pb[k] = vadd(b, vmul(d, r1));
    }
    for (int k = 0; k < n; k++) {
        int k2 = (k + 1) % n;
        draw_quad(pa[k], pa[k2], pb[k2], pb[k], col);
        if (caps & 1) draw_tri(a, pa[k2], pa[k], col);
        if (caps & 2) draw_tri(b, pb[k], pb[k2], col);
    }
}

/* ----------------------------------------------------------------- audio */
static volatile float a_freq, a_vol, a_noise, a_beepf, a_beept, a_thump;
static void synth(int16_t *o, int n)
{
    static float ph, ph2, bph, th; static uint32_t r = 1;
    for (int i = 0; i < n; i++) {
        float f = a_freq;
        ph += f / 22050.f; if (ph >= 1) ph -= 1;
        ph2 += f * 0.5f / 22050.f; if (ph2 >= 1) ph2 -= 1;
        float s = (ph * 2 - 1) * 0.45f + (ph < 0.5f ? 0.2f : -0.2f) + (ph2 < 0.5f ? 0.2f : -0.2f);
        s *= a_vol * 0.4f;
        r = r * 1664525u + 1013904223u;
        float nz = ((r >> 16) & 0x7fff) / 16384.f - 1;
        s += nz * a_noise * 0.5f; a_noise *= 0.9998f;
        if (a_thump > 0.001f) { th += 55.f / 22050.f; if (th >= 1) th -= 1; s += (th < 0.5f ? 1.f : -1.f) * a_thump * 0.6f; a_thump *= 0.9993f; }
        if (a_beept > 0) { bph += a_beepf / 22050.f; if (bph >= 1) bph -= 1; s += bph < 0.5f ? 0.18f : -0.18f; a_beept -= 1.f / 22050.f; }
        s = clampf(s, -1, 1);
        o[i] = (int16_t)(s * 18000);
    }
}
static void SDLCALL audio_cb(void *ud, SDL_AudioStream *st, int add, int total)
{
    (void)ud; (void)total;
    int n = add / 2; int16_t buf[1024];
    while (n > 0) { int m = n > 1024 ? 1024 : n; synth(buf, m); SDL_PutAudioStreamData(st, buf, m * 2); n -= m; }
}
static void beep(float f, float t) { a_beepf = f; a_beept = t; }

/* ----------------------------------------------------------------- track */
typedef struct { V3 c, T, R, N; float u, curv, vcurv; unsigned char gap; } Seg;
enum { F_HUMP, F_PLATEAU };
typedef struct { int type; float s, w, h, w2, w3; } Feat;     /* hump: bell centred at s, half width w. plateau: rise over w from s, hold w2, fall over w3 */
typedef struct {
    const char *name; int n; float p[16][2]; float bankf; int nfeat; Feat feat[12]; int ngap; float gap[3][2]; float kick;
} TrackDef;
#define BASE_Y 7.0f
#define HUMP(s, w, h) {F_HUMP, s, w, h, 0, 0}

/* plans are smooth closed splines through the control points: left and right bends, through the infield.
 * The profile is flat apart from the listed features (arc length in metres from the start line). */
static const TrackDef tracks[3] = {
 { "LITTLE RAMP", 12,
   {{0,0},{260,0},{400,30},{440,130},{380,210},{290,190},{230,120},{150,120},{100,200},{10,250},{-80,200},{-90,80}},
   0.35f, 4, {HUMP(560, 30, 4.f), HUMP(765, 30, 5.f), HUMP(940, 30, 5.f), HUMP(1140, 36, 6.f)}, 1, {{175, 24}}, 3.0f },
 { "HUMP BACK", 12,
   {{0,0},{60,0},{150,0},{300,0},{400,0},{490,60},{490,150},{370,210},{250,165},{120,205},{-10,180},{-70,100}},
   0.5f, 5, {HUMP(225, 100, 24.f), HUMP(560, 28, 6.f), HUMP(810, 30, 7.f), HUMP(940, 34, 8.f), HUMP(1075, 34, 9.f)}, 0, {{0,0}}, 1.2f },
 { "ROLLER COASTER", 14,
   {{0,0},{360,0},{480,50},{500,160},{420,210},{330,185},{255,250},{255,320},{130,365},{-20,335},{-60,240},{30,170},{-30,115},{-110,70}},
   0.45f, 5, {{F_PLATEAU, 80, 90, 26.f, 60, 80}, HUMP(560, 28, 4.f), HUMP(850, 40, 8.f),
              HUMP(1190, 90, 20.f), HUMP(1430, 50, 10.f)}, 0, {{0,0}}, 1.2f },
};
#define NTRACKS 3

static Seg seg[MAXSEG]; static int nseg; static float ds;
static int cur_track = -1;
static float tr_minx, tr_maxx, tr_minz, tr_maxz;
static float ar_x0, ar_x1, ar_z0, ar_z1;        /* stadium arena bounds */

static float smooth01(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }
static void smooth_circ(float *a, int passes)
{
    static float tmp[MAXSEG];
    for (int p = 0; p < passes; p++) {
        for (int i = 0; i < nseg; i++) tmp[i] = (a[(i + nseg - 1) % nseg] + 2 * a[i] + a[(i + 1) % nseg]) * 0.25f;
        memcpy(a, tmp, sizeof(float) * nseg);
    }
}

static void build_track_geometry(int idx)
{
    const TrackDef *td = &tracks[idx];
    cur_track = idx;
    /* plan: closed centripetal Catmull-Rom spline through the control points, sampled densely */
    enum { PER = 300, MAXV = 16 * PER };
    static V3 fp[MAXV]; static float cum[MAXV + 1];
    int n = td->n, M = 0;
    for (int i = 0; i < n; i++) {
        V3 P[4];
        for (int q = 0; q < 4; q++) { int k = (i - 1 + q + n) % n; P[q] = V(td->p[k][0], 0, td->p[k][1]); }
        float t[4] = {0};
        for (int q = 1; q < 4; q++) t[q] = t[q - 1] + sqrtf(fmaxf(vlen(vsub(P[q], P[q - 1])), 1e-3f));
        for (int k = 0; k < PER; k++) {
            float tt = t[1] + (t[2] - t[1]) * k / PER;
            V3 A1 = vlerp(P[0], P[1], (tt - t[0]) / (t[1] - t[0])), A2 = vlerp(P[1], P[2], (tt - t[1]) / (t[2] - t[1]));
            V3 A3 = vlerp(P[2], P[3], (tt - t[2]) / (t[3] - t[2]));
            V3 B1 = vlerp(A1, A2, (tt - t[0]) / (t[2] - t[0])), B2 = vlerp(A2, A3, (tt - t[1]) / (t[3] - t[1]));
            fp[M++] = vlerp(B1, B2, (tt - t[1]) / (t[2] - t[1]));
        }
    }
    cum[0] = 0;
    for (int i = 0; i < M; i++) cum[i + 1] = cum[i] + vlen(vsub(fp[(i + 1) % M], fp[i]));
    float total = cum[M];
    nseg = (int)(total / 1.0f + 0.5f); if (nseg > MAXSEG) nseg = MAXSEG;
    ds = total / nseg;
    int i = 0;
    for (int k = 0; k < nseg; k++) {
        float tg = k * ds;
        while (i < M - 1 && cum[i + 1] < tg) i++;
        float f = (tg - cum[i]) / (cum[i + 1] - cum[i]);
        seg[k].c = vlerp(fp[i], fp[(i + 1) % M], f);
        seg[k].u = tg;
        float h = BASE_Y;                                   /* flat profile plus the listed features */
        for (int q = 0; q < td->nfeat; q++) {
            const Feat *ft = &td->feat[q];
            float d = tg - ft->s;
            if (ft->type == F_HUMP) { if (fabsf(d) < ft->w) h += ft->h * 0.5f * (1.f + cosf(PI_F * d / ft->w)); }
            else if (d > 0 && d < ft->w + ft->w2 + ft->w3) {
                if (d < ft->w) h += ft->h * smooth01(d / ft->w);
                else if (d < ft->w + ft->w2) h += ft->h;
                else h += ft->h * (1.f - smooth01((d - ft->w - ft->w2) / ft->w3));
            }
        }
        seg[k].c.y = h;
    }
    for (int k = 0; k < nseg; k++) {
        seg[k].gap = 0;
        for (int g = 0; g < td->ngap; g++)
            if (seg[k].u >= td->gap[g][0] && seg[k].u < td->gap[g][0] + td->gap[g][1]) seg[k].gap = 1;
    }
    /* kicker ramp before each gap, lower landing after it */
    static float dy[MAXSEG];
    memset(dy, 0, sizeof(float) * nseg);
    int RL = (int)((td->kick > 2.f ? 38 : 30) / ds), DL = (int)(90 / ds);
    for (int k = 0; k < nseg; k++) {
        if (!seg[k].gap || seg[(k + nseg - 1) % nseg].gap) continue;
        int k1 = k;
        while (seg[(k1 + 1) % nseg].gap) k1++;
        const float H = td->kick, D = 0.5f;
        for (int m = 1; m <= RL; m++) { float t = (float)(RL - m + 1) / RL; dy[((k - m) % nseg + nseg) % nseg] += H * t * t; }
        for (int m = 0; k + m <= k1; m++) dy[(k + m) % nseg] += H - (H + D) * (float)m / (k1 - k + 1);
        for (int m = 1; m <= DL; m++) dy[(k1 + m) % nseg] += -D * (1.f - (float)m / DL);
    }
    for (int k = 0; k < nseg; k++) seg[k].c.y += dy[k];
    static float kap[MAXSEG], bank[MAXSEG], vk[MAXSEG]; static V3 Th[MAXSEG];
    for (int k = 0; k < nseg; k++) {
        seg[k].T = vnorm(vsub(seg[(k + 1) % nseg].c, seg[(k + nseg - 1) % nseg].c));
        Th[k] = vnorm(V(seg[k].T.x, 0, seg[k].T.z));
    }
    for (int k = 0; k < nseg; k++) {
        V3 Rh = vnorm(vcross(Th[k], UP));
        kap[k] = vdot(vsub(Th[(k + 1) % nseg], Th[(k + nseg - 1) % nseg]), Rh) / (2 * ds);
        vk[k] = -(seg[(k + 1) % nseg].T.y - seg[(k + nseg - 1) % nseg].T.y) / (2 * ds);
    }
    smooth_circ(kap, (int)(120 / ds)); smooth_circ(vk, (int)(30 / ds));
    for (int k = 0; k < nseg; k++) {
        bank[k] = clampf(atanf(td->bankf * 40.0f * kap[k]), -0.45f, 0.45f);
        seg[k].curv = fabsf(kap[k]); seg[k].vcurv = vk[k];
    }
    smooth_circ(bank, (int)(48 / ds));
    for (int k = 0; k < nseg; k++) {
        V3 R0 = vnorm(vcross(seg[k].T, UP)), N0 = vcross(R0, seg[k].T);
        float cb = cosf(bank[k]), sb = sinf(bank[k]);
        seg[k].R = vsub(vmul(R0, cb), vmul(N0, sb));
        seg[k].N = vadd(vmul(N0, cb), vmul(R0, sb));
    }
    tr_minx = tr_minz = 1e9f; tr_maxx = tr_maxz = -1e9f;
    for (int k = 0; k < nseg; k++) {
        tr_minx = fminf(tr_minx, seg[k].c.x); tr_maxx = fmaxf(tr_maxx, seg[k].c.x);
        tr_minz = fminf(tr_minz, seg[k].c.z); tr_maxz = fmaxf(tr_maxz, seg[k].c.z);
    }
    ar_x0 = tr_minx - 90; ar_x1 = tr_maxx + 90; ar_z0 = tr_minz - 90; ar_z1 = tr_maxz + 90;
}

/* gentle hills inside the arena, flat at the perimeter wall */
static float ground_h(float x, float z)
{
    float d = fminf(fminf(x - ar_x0, ar_x1 - x), fminf(z - ar_z0, ar_z1 - z));
    if (d <= 0) return 0;
    float n = 0.5f * sinf(x * 0.021f + 1.3f) * cosf(z * 0.017f) + 0.3f * sinf(x * 0.047f + z * 0.039f) + 0.2f * cosf(z * 0.071f - x * 0.053f);
    return smooth01(d / 90.f) * (1.6f + 1.5f * n);
}

typedef struct { int seg, valid; float u, lat, h; V3 N, R, T; int road; } Q;

static void track_query(V3 p, int hint, int range, Q *q)
{
    float best = 1e30f; int bj = -1, bg = 0; float bu = 0;
    for (int o = -range; o <= range; o++) {
        int j = ((hint + o) % nseg + nseg) % nseg;
        const Seg *s = &seg[j], *s2 = &seg[(j + 1) % nseg];
        V3 ch = vsub(s2->c, s->c), d = vsub(p, s->c);
        float cc2 = vdot(ch, ch), al = vdot(d, ch) / cc2;
        /* on the outside of a bend consecutive segments leave thin wedges that neither covers: accept a small
         * overshoot, clamped to the segment end, so a wheel crossing a wedge never loses the deck for a tick */
        if (al < -0.25f || al > 1.25f) continue;
        float alc = clampf(al, 0, 0.9999f);
        V3 e = vsub(p, vadd(s->c, vmul(ch, alc)));
        float h = vdot(e, s->N), lt = vdot(e, s->R), score = h * h + lt * lt + sq(al - alc) * cc2;
        if (score < best) { best = score; bj = j; bu = alc; bg = 1; }
    }
    if (!bg) { q->valid = 0; q->seg = hint; q->road = 0; return; }
    const Seg *s = &seg[bj], *s2 = &seg[(bj + 1) % nseg];
    V3 cc = vlerp(s->c, s2->c, bu);
    q->N = vnorm(vlerp(s->N, s2->N, bu)); q->R = vnorm(vlerp(s->R, s2->R, bu));
    q->T = vnorm(vcross(q->N, q->R));
    V3 e = vsub(p, cc);
    q->h = vdot(e, q->N); q->lat = vdot(e, q->R);
    q->seg = bj; q->u = bu; q->road = !s->gap; q->valid = 1;
}

/* ------------------------------------------------- static world geometry */
static inline V3 rp(int i, float d, float h)
{
    const Seg *s = &seg[((i % nseg) + nseg) % nseg];
    return vadd(s->c, vadd(vmul(s->R, d), vmul(s->N, h)));
}

#define NSTR 30                   /* strips across the deck: a thin white line at each edge, 28 deck strips between */
#define EDGE_W 0.22f
#define SLAB 6                    /* precast deck slab length in segments */
#define COL_YEL 0xF0C820
#define COL_RED 0xC82424
static float strip_off(int k)
{
    if (k <= 0) return -TRACK_HW;
    if (k >= NSTR) return TRACK_HW;
    return -TRACK_HW + EDGE_W + (k - 1) * (2 * (TRACK_HW - EDGE_W)) / (NSTR - 2);
}
/* weathered concrete at deck row i, lateral offset d: blotchy wear, rubber streaks from the tyres (heavier in the
 * bends), grime along the edges */
static uint32_t deck_col(int i, float d)
{
    const Seg *sg = &seg[((i % nseg) + nseg) % nseg];
    float u = i * ds;
    float s = 0.88f + 0.15f * (0.6f * vnoise(u / 9.f, d / 4.f, 1) + 0.4f * vnoise(u / 2.3f, d / 1.3f, 2));
    float rub = 0;
    for (int k = 0; k < 5; k++) {
        float p = -4.6f + k * 2.3f + 1.1f * sinf(u / 41.f + k * 1.7f) + 0.5f * sinf(u / 13.f + k);
        rub += expf(-sq((d - p) / 0.5f)) * (0.3f + 0.7f * vnoise(u / 7.f, k * 3.1f, 3));
    }
    float bend = clampf(sg->curv * 70.f, 0, 1);
    s *= 1.f - fminf(rub, 1.f) * (0.06f + 0.14f * bend);
    float e = TRACK_HW - fabsf(d);
    if (e < 1.2f) s *= 0.93f + 0.07f * e / 1.2f;
    return scalecol(0x9E998E, s);
}
static uint32_t edge_col(int i, float d) { return scalecol(0xECEBE4, 0.9f + 0.1f * vnoise(i * ds / 3.f, d, 4)); }
static int is_edge(int k) { return k == 0 || k == NSTR - 1; }
static uint32_t start_col(int i, int k) { return (((k - 1) / 2 + i) & 1) ? 0xF2F2F2 : 0x161616; }
/* repair patches: a few slightly darker rectangles with sharp edges */
static float patch_f(int i, int k)
{
    uint32_t h = hash2(i / 4 + 7, (k - 1) / 5 + 3);
    return (h % 19 == 0) ? 0.86f + 0.04f * ((h >> 8) & 3) : 1.f;
}

static void build_seg_mesh(int i)
{
    int j = (i + 1) % nseg, gp = seg[(i + nseg - 1) % nseg].gap, gn = seg[j].gap;
    const Seg *s = &seg[i];
    if (s->gap) return;
    const float hw = TRACK_HW, ow = TRACK_HW;
    V3 a[NSTR + 1], b[NSTR + 1];
    uint32_t ca[NSTR + 1], cb[NSTR + 1];
    for (int k = 0; k <= NSTR; k++) {
        a[k] = rp(i, strip_off(k), 0); b[k] = rp(j, strip_off(k), 0);
        ca[k] = deck_col(i, strip_off(k)); cb[k] = deck_col(i + 1, strip_off(k));
    }
    /* a slab joint (dark gap with a steel strip) at the start of every slab */
    float tj = (i % SLAB == 0) ? 0.10f : 0.f;
    for (int k = 0; k < NSTR; k++) {
        V3 a0 = a[k], a1 = a[k + 1], b0 = b[k], b1 = b[k + 1];
        if (i < 4 && !is_edge(k)) { draw_quad(a0, a1, b1, b0, start_col(i, k)); continue; }
        if (tj > 0) {
            V3 m0 = vlerp(a0, b0, tj), m1 = vlerp(a1, b1, tj);
            draw_quad(a0, a1, m1, m0, is_edge(k) ? 0x9A9A96 : 0x6C6964);
            a0 = m0; a1 = m1;
        }
        if (is_edge(k)) {
            float d0 = strip_off(k), d1 = strip_off(k + 1);
            draw_quad_c(a0, a1, b1, b0, edge_col(i, d0), edge_col(i, d1), edge_col(i + 1, d1), edge_col(i + 1, d0));
        } else {
            float f = patch_f(i, k);
            draw_quad_c(a0, a1, b1, b0, scalecol(ca[k], f), scalecol(ca[k + 1], f), scalecol(cb[k + 1], f), scalecol(cb[k], f));
        }
    }
    /* open deck edge, no barriers (like the original): steel lip over red and white side skirts */
    for (int sd = -1; sd <= 1; sd += 2) {
        uint32_t hz = ((i / 4) & 1) ? COL_RED : 0xE8E8E8;
        draw_quad(rp(i, sd * hw, 0), rp(j, sd * hw, 0), rp(j, sd * hw, -0.16f), rp(i, sd * hw, -0.16f), 0x6A6A72);
        draw_quad(rp(i, sd * hw, -0.16f), rp(j, sd * hw, -0.16f), rp(j, sd * hw, -1.2f), rp(i, sd * hw, -1.2f), hz);
    }
    draw_quad(rp(i, -hw, -1.2f), rp(j, -hw, -1.2f), rp(j, hw, -1.2f), rp(i, hw, -1.2f), 0x2C2C34);
    if (gp || gn) {
        int ci = gp ? i : j;
        draw_quad(rp(ci, -ow, -1.2f), rp(ci, ow, -1.2f), rp(ci, ow, 0), rp(ci, -ow, 0), 0x4A4A56);
    }
    /* hanging steel truss below the deck */
    if (i % 3 == 0 && !gn && !gp) {
        int j2 = (i + 3) % nseg;
        for (int sd = -1; sd <= 1; sd += 2) {
            V3 t0 = rp(i, sd * (hw + 0.2f), -1.2f);
            V3 b0 = rp(i, sd * (hw - 0.8f), -2.6f), b1 = rp(j2, sd * (hw - 0.8f), -2.6f);
            V3 t1 = rp(j2, sd * (hw + 0.2f), -1.2f);
            cyl(b0, b1, 0.10f, 0.10f, 6, 0x6C6C78, 0);
            cyl(t0, b0, 0.06f, 0.06f, 5, 0x888894, 0);
            if ((i / 3) & 1) cyl(t0, b1, 0.05f, 0.05f, 5, 0x888894, 0); else cyl(b0, t1, 0.05f, 0.05f, 5, 0x888894, 0);
        }
    }
}

static void build_tower(int i)
{
    const Seg *s = &seg[i];
    float top = s->c.y - 1.4f;
    if (top < 3.0f) return;
    float tk[4];                                    /* column tops follow the real underside of the deck (bank, slope) */
    {
        static const float qx[4] = {-1, 1, 1, -1}, qz[4] = {-1, -1, 1, 1};
        for (int k = 0; k < 4; k++)
            tk[k] = s->c.y + s->R.y * qx[k] * 4.4f + s->T.y * qz[k] * 1.4f - 1.2f * s->N.y - 0.95f;
    }
    V3 Th = vnorm(V(s->T.x, 0, s->T.z)), Rh = vnorm(vcross(Th, UP));
    float spread = 4.4f, bspread = 4.4f + top * 0.14f;
    V3 topc[4], botc[4];
    static const float sx[4] = {-1, 1, 1, -1}, sz[4] = {-1, -1, 1, 1};
    for (int k = 0; k < 4; k++) {
        V3 off_t = vadd(vmul(Rh, sx[k] * spread), vmul(Th, sz[k] * 1.4f));
        V3 off_b = vadd(vmul(Rh, sx[k] * bspread), vmul(Th, sz[k] * (1.4f + top * 0.05f)));
        topc[k] = V(s->c.x + off_t.x, tk[k], s->c.z + off_t.z);
        botc[k] = V(s->c.x + off_b.x, ground_h(s->c.x + off_b.x, s->c.z + off_b.z) - 0.3f, s->c.z + off_b.z);
    }
    for (int k = 0; k < 4; k++) cyl(botc[k], topc[k], 0.28f, 0.2f, 8, 0xC8C8D0, 3);
    int levels = (int)(top / 4.0f) + 1;
    for (int l = 0; l <= levels; l++) {
        float t0 = (float)l / levels, t1 = (float)(l + 1) / levels;
        V3 p0[4], p1[4];
        for (int k = 0; k < 4; k++) { p0[k] = vlerp(botc[k], topc[k], t0); p1[k] = vlerp(botc[k], topc[k], t1); }
        uint32_t bc = (l & 1) ? COL_RED : 0xE8E8E8;
        for (int k = 0; k < 4; k++) {
            int k2 = (k + 1) % 4;
            cyl(p0[k], p0[k2], 0.1f, 0.1f, 6, bc, 0);
            if (l < levels) cyl((l & 1) ? p0[k] : p0[k2], (l & 1) ? p1[k2] : p1[k], 0.07f, 0.07f, 5, 0x9C9CA8, 0);
        }
    }
    cyl(vadd(topc[0], V(0, 0.3f, 0)), vadd(topc[1], V(0, 0.3f, 0)), 0.2f, 0.2f, 6, 0x6E6E7A, 3);
    cyl(vadd(topc[3], V(0, 0.3f, 0)), vadd(topc[2], V(0, 0.3f, 0)), 0.2f, 0.2f, 6, 0x6E6E7A, 3);
}



/* ---- stadium ---- */
static void flag(V3 base, float h, uint32_t col);
/* circus big top like the ones around the original's stadium: striped wall, striped cone roof, flag on the king pole */
static void big_top(float x, float z, float R, float wallh, float peak)
{
    enum { NS = 28 };
    float g = ground_h(x, z);
    for (int k = 0; k < NS; k++) {
        float a0 = k * 2 * PI_F / NS, a1 = (k + 1) * 2 * PI_F / NS;
        V3 b0 = V(x + cosf(a0) * R, g, z + sinf(a0) * R), b1 = V(x + cosf(a1) * R, g, z + sinf(a1) * R);
        V3 t0 = vadd(b0, V(0, wallh, 0)), t1 = vadd(b1, V(0, wallh, 0));
        uint32_t c = (k & 1) ? 0xF4F4F0 : 0xC81E18;
        draw_quad(b0, b1, t1, t0, c);
        V3 e0 = V(x + cosf(a0) * R * 1.06f, g + wallh - 0.6f, z + sinf(a0) * R * 1.06f), e1 = V(x + cosf(a1) * R * 1.06f, g + wallh - 0.6f, z + sinf(a1) * R * 1.06f);
        draw_quad(t0, t1, e1, e0, c);
        draw_tri(e0, e1, V(x, g + peak, z), c);                                       /* roof gore */
    }
    cyl(V(x, g + peak - 0.5f, z), V(x, g + peak + 7, z), 0.25f, 0.18f, 6, 0xD8D8E0, 3);
    draw_tri(V(x, g + peak + 7, z), V(x, g + peak + 5.4f, z), V(x + 4.5f, g + peak + 6.2f, z), 0xF0D020);
}
static void flag(V3 base, float h, uint32_t col)
{
    cyl(base, vadd(base, V(0, h, 0)), 0.09f, 0.07f, 5, 0xD8D8E0, 3);
    V3 t = vadd(base, V(0, h, 0));
    draw_tri(t, vadd(t, V(0, -1.6f, 0)), vadd(t, V(2.6f, -0.7f, 0.4f)), col);
}
/* real 3D faceted mountains in a ring around the arena (static, lit, fogged) */
static void build_mountains(void)
{
    float cx = (ar_x0 + ar_x1) / 2, cz = (ar_z0 + ar_z1) / 2;
    float rad = 0.5f * sqrtf(sq(ar_x1 - ar_x0) + sq(ar_z1 - ar_z0));
    rngs = 9001 + cur_track;
    enum { NM = 52, NR = 7, NA = 16 };
    for (int m = 0; m < NM; m++) {
        float an = (m + frand() * 0.8f) * 2 * PI_F / NM;
        float dist = rad + 1500 + frand() * 1300;
        float R = 500 + frand() * 600, H = 110 + frand() * 240;
        if (m % 5 == 0) H *= 1.5f;
        float px = cx + cosf(an) * dist, pz = cz + sinf(an) * dist;
        static V3 pt[NR + 1][NA];
        float hz[NR + 1];
        for (int r = 0; r <= NR; r++) {
            float t = (float)r / NR;
            hz[r] = H * (1.f - powf(1.f - t, 1.7f));
            for (int a = 0; a < NA; a++) {
                float aa = 2 * PI_F * a / NA;
                float j1 = (float)(hash2(m * 131 + r, a * 17 + 3) & 255) / 255.f, j2 = (float)(hash2(m * 71 + a, r * 29 + 9) & 255) / 255.f;
                float rr = R * (1.f - t) * (1.f + 0.22f * (j1 - 0.5f)) * (1.f + 0.25f * sinf(aa * 3 + m));
                float hh = hz[r] * (1.f + 0.16f * (j2 - 0.5f) * (r > 0 && r < NR));
                pt[r][a] = V(px + cosf(aa) * rr, hh, pz + sinf(aa) * rr);
            }
        }
        for (int r = 0; r < NR; r++)
            for (int a = 0; a < NA; a++) {
                int a2 = (a + 1) % NA;
                float hf = (hz[r] + hz[r + 1]) * 0.5f / H;
                uint32_t col = hf > 0.74f ? 0xF2F2FA : hf > 0.5f ? 0x8E8C96 : hf > 0.22f ? 0x6E6A64 : 0x4C5E46;
                col = scalecol(col, 0.88f + 0.24f * (float)(hash2(m * 7 + r, a * 3) & 255) / 255.f);
                draw_quad(pt[r][a], pt[r][a2], pt[r + 1][a2], pt[r + 1][a], col);
            }
    }
}

#include "scenery.h"

static void build_static(void)
{
    cur = &sb; sb.n = 0;
    plan_arena();
    build_floor();
    build_stadium();
    /* big tops at the corners, behind the floodlights */
    big_top(ar_x0 - 70, ar_z0 - 70, 34, 9, 30); big_top(ar_x1 + 70, ar_z0 - 70, 30, 8, 26);
    big_top(ar_x1 + 70, ar_z1 + 70, 36, 9, 32); big_top(ar_x0 - 70, ar_z1 + 70, 28, 8, 24);
    build_mountains();
    build_outside();
    for (int i = 0; i < nseg; i++) build_seg_mesh(i);
    for (int i = 10; i < nseg; i += 30) build_tower(i);
    build_infield();
    cur = &db;
}

static void build_track(int idx)
{
    build_track_geometry(idx);
    build_static();
}

/* ------------------------------------------------------------------ cars */
#define CAR_M (ph.mass)
#define K_F (ph.kf)
#define K_R (ph.kr)
#define C_COMP (ph.ccomp)
#define C_REB (ph.creb)
#define K_BUMP (ph.kbump)
#define MU (ph.mu)
#define IX (2200.f * ph.mass / 1000.f)
#define IY (2400.f * ph.mass / 1000.f)
#define IZ (900.f * ph.mass / 1000.f)

typedef struct {
    V3 x, v, R, U, F, w;
    int seg, lap, finished, wrecked, active, safe, bst, nwc, hold;
    float thr, brk, str, delta, boost, damage, prog, ftime, lapstart, bestlap, lastlap, lapshow;
    float crash, crane, crane_T, hook, dust_t;    /* crash on the ground / crane lowering timers, hook retract timer */
    V3 crane_x;                                   /* where the crane sets the car down */
    int crane_seg;
    float wl[4], wph[4], flip_t, stuck_t, hurt_cd;
    unsigned char hit[16], wbot[4];
    float lr, lp, lrr, lpr, alat_f, along_f;      /* visual body roll/pitch (angle, rate) and filtered accelerations */
    float flame;                                  /* exhaust flame strength (0..1.4), boost only */
    uint32_t col;
} Car;

/* car geometry relative to the centre of mass (FL, FR, RL, RR), read from models/stuntcar.scm (car_model.h) */
static V3 WMOUNT[4];
static float WRAD[4];
static float WLF[4];                                     /* suspension length at full droop (mount to hub) */
static float WLMIN[4];                                   /* length where the bump stop engages */
static float WREST[4];                                   /* length at the model's ride height */
static float REST_H, PLACE_H;                            /* centre-of-mass height at rest / when dropped onto the track */
static V3 FLAMEP[8], FLAMED[8];                          /* exhaust pipe tips and axes */
static V3 CPTS[16];                                      /* body points that collide with the deck */
/* eye position of the driver */
static V3 EYE;

enum { ST_MENU, ST_COUNT, ST_RACE, ST_DONE, ST_VIEW };
static Car cars[2];
static int state = ST_MENU, laps = 3, skill = 1, sel_track = 0, menu_row = 0, result = 0, paused = 0, cockpit = 1;
static float state_t, racetime;
static char msg[64]; static float msg_t;
static int stat_falls, stat_contacts; static float stat_push;
static const char *skill_name[3] = {"ROOKIE", "PRO", "ACE"};
static const float skill_vcap[3] = {36, 40, 44}, skill_alat[3] = {9.0f, 11.0f, 13.0f};

static void set_msg(const char *s, float t) { snprintf(msg, sizeof msg, "%s", s); msg_t = t; }

static V3 body_pt(const Car *c, V3 l)
{
    return vadd(c->x, vadd(vmul(c->R, l.x), vadd(vmul(c->U, l.y), vmul(c->F, l.z))));
}

static void car_place(Car *c, int s, float lat, float speed)
{
    s = ((s % nseg) + nseg) % nseg;
    c->F = seg[s].T; c->U = seg[s].N; c->R = vcross(c->F, c->U);
    c->x = vadd(vadd(seg[s].c, vmul(seg[s].R, lat)), vmul(seg[s].N, PLACE_H));
    c->v = vmul(seg[s].T, speed); c->w = V(0, 0, 0);
    c->seg = s; c->safe = s; c->delta = 0; c->flip_t = c->stuck_t = 0;
    for (int i = 0; i < 4; i++) c->wl[i] = WLF[i];
}

static void car_hurt(Car *c, float amt, float loud)
{
    if (c->wrecked || amt <= 0) return;
    if (c->hurt_cd > 0) amt *= 0.12f; else c->hurt_cd = 0.3f;
    c->damage += amt;
    if (getenv("SR_DEBUG") && amt > 1.5f && c == &cars[0]) fprintf(stderr, "hurt %.1f seg %d u=%.2f v=%.1f nwc=%d vy=%.1f\n", amt, c->seg, seg[c->seg].u, vlen(c->v), c->nwc, c->v.y);
    if (c == &cars[0] && state == ST_RACE) {
        a_noise = fmaxf(a_noise, clampf(amt / 8.f, 0.2f, 1.f) * loud);
        a_thump = fmaxf(a_thump, clampf(amt / 12.f, 0.1f, 0.8f) * loud);
    }
    if (c->damage >= 100) { c->damage = 100; c->wrecked = 1; }
}

/* ---- particles: sparks, dust, smoke, fire (world space, drawn as unlit camera-facing shapes) ---- */
enum { P_SPARK, P_DUST, P_SMOKE, P_FIRE };
typedef struct { V3 p, v; float t, life, size; uint32_t col; int kind; } Part;
#define MAXPART 1500
static Part parts[MAXPART]; static int npart;
static int stat_sparks;
static void spawn(int kind, V3 p, V3 v, float life, float size, uint32_t col)
{
    if (kind == P_SPARK) stat_sparks++;
    if (npart >= MAXPART) return;
    Part *q = &parts[npart++];
    q->p = p; q->v = v; q->t = 0; q->life = life; q->size = size; q->col = col; q->kind = kind;
}
static V3 rnd_dir(void) { return vnorm(V(frand() - 0.5f, frand() - 0.5f, frand() - 0.5f)); }
static void dust_burst(V3 p, int n, float speed)
{
    for (int k = 0; k < n; k++) {
        V3 d = rnd_dir(); d.y = fabsf(d.y) * 0.6f + 0.2f;
        spawn(P_DUST, vadd(p, vmul(d, 0.8f)), vmul(d, speed * (0.3f + frand())), 1.2f + frand() * 1.2f, 0.25f + frand() * 0.35f,
              (k & 1) ? 0xA58A62 : 0x8E7552);
    }
}
static void parts_step(float dt)
{
    for (int i = 0; i < npart; i++) {
        Part *q = &parts[i];
        q->t += dt;
        if (q->t >= q->life) { parts[i--] = parts[--npart]; continue; }
        if (q->kind == P_SPARK) {
            q->v.y -= GRAV * dt;
            if (q->p.y < ground_h(q->p.x, q->p.z)) { parts[i--] = parts[--npart]; continue; }
        } else {
            float drag = q->kind == P_DUST ? 1.6f : 0.9f;
            q->v = vmul(q->v, expf(-drag * dt));
            q->v.y += (q->kind == P_DUST ? 0.35f : q->kind == P_SMOKE ? 1.4f : 2.5f) * dt;
            q->size += (q->kind == P_DUST ? 0.55f : q->kind == P_SMOKE ? 0.6f : -0.3f) * dt;
        }
        q->p = vadd(q->p, vmul(q->v, dt));
    }
}

/* penalty contact of one body point against the solid deck / barriers */
static void contact_point(Car *c, int idx, V3 lp, V3 *F, V3 *Tq)
{
    V3 P = body_pt(c, lp);
    Q q; track_query(P, c->seg, 12, &q);
    if (!q.valid || !q.road) { c->hit[idx] = 0; return; }
    const float ow = TRACK_HW;
    float al = fabsf(q.lat), h = q.h, sg = q.lat > 0 ? 1.f : -1.f;
    float topy = 0.0f;
    if (al > ow || h < -1.2f || h > topy) { c->hit[idx] = 0; return; }
    float best = topy - h; V3 bn = q.N;
    float pb = h + 1.2f; if (pb < best) { best = pb; bn = vmul(q.N, -1); }
    float ps = ow - al; if (ps < best) { best = ps; bn = vmul(q.R, sg); }
    if (seg[(q.seg + nseg - 1) % nseg].gap) { float pst = q.u * ds; if (pst < best) { best = pst; bn = vmul(q.T, -1); } }
    if (seg[(q.seg + 1) % nseg].gap) { float pe = (1 - q.u) * ds; if (pe < best) { best = pe; bn = q.T; } }
    V3 r = vsub(P, c->x), vp = vadd(c->v, vcross(c->w, r));
    float vn = vdot(vp, bn);
    float Fn = 250000.f * best - 7000.f * vn;
    if (Fn < 0) Fn = 0;
    if (Fn > 220000.f) Fn = 220000.f;
    V3 f = vmul(bn, Fn);
    V3 vt = vsub(vp, vmul(bn, vn)); float vts = vlen(vt);
    if (vts > 1e-3f) f = vadd(f, vmul(vt, -0.25f * Fn * tanhf(vts / 0.5f) / vts));
    *F = vadd(*F, f); *Tq = vadd(*Tq, vcross(r, f));
    if (vts > 3.f && Fn > 1500.f && frand() < 0.35f)                  /* steel on the deck: sparks */
        for (int k = 0; k < 4; k++)
            spawn(P_SPARK, P, vadd(vadd(vmul(vt, 0.55f + 0.3f * frand()), vmul(bn, 1.5f + 2.f * frand())), vmul(rnd_dir(), 2.f)),
                  0.25f + 0.35f * frand(), 0.05f, frand() < 0.5f ? 0xFFF0A0 : 0xFFB030);
    if (!c->hit[idx] && -vn > 6.0f) car_hurt(c, (-vn - 6.0f) * 0.9f, 1.f);
    c->hit[idx] = 1;
}

/* underside of the car (car frame): tub in the middle, narrower frame rails front and rear */
static void belly_at(float z, float *y, float *hw)
{
    if (z > 0.6f) { *y = -0.50f; *hw = 0.30f; }
    else if (z < -1.1f) { *y = -0.53f; *hw = 0.32f; }
    else { *y = -0.313f; *hw = 0.63f; }
}
/* the open deck edge against the underside: when the car hangs over the edge it rides and scrapes on the corner */
static void edge_contact(Car *c, V3 *F, V3 *Tq)
{
    for (float z = -1.65f; z <= 2.1f; z += 0.25f) {
        float by, bhw; belly_at(z, &by, &bhw);
        V3 B = body_pt(c, V(0, by, z));
        Q q; track_query(B, c->seg, 12, &q);
        if (!q.valid || !q.road) continue;
        for (int sd = -1; sd <= 1; sd += 2) {
            V3 E = vadd(vsub(vsub(B, vmul(q.R, q.lat)), vmul(q.N, q.h)), vmul(q.R, sd * TRACK_HW));   /* deck corner here */
            V3 d = vsub(E, c->x), l = V(vdot(d, c->R), vdot(d, c->U), vdot(d, c->F));
            float ey, ehw; belly_at(l.z, &ey, &ehw);
            float pen = l.y - ey;
            if (fabsf(l.x) > ehw || pen <= 0 || pen > 0.35f) continue;
            V3 r = d, vp = vadd(c->v, vcross(c->w, r));
            float vn = vdot(vp, c->U);
            float Fn = fmaxf(250000.f * pen - 7000.f * vn, 0.f);
            if (Fn > 120000.f) Fn = 120000.f;
            V3 f = vmul(c->U, Fn), vt = vsub(vp, vmul(c->U, vn)); float vts = vlen(vt);
            if (vts > 1e-3f) f = vadd(f, vmul(vt, -0.3f * Fn * tanhf(vts / 0.5f) / vts));
            *F = vadd(*F, f); *Tq = vadd(*Tq, vcross(r, f));
            if (vts > 2.f && frand() < 0.5f)                                          /* frame grinding on the edge: sparks */
                for (int k = 0; k < 4; k++) {                                         /* some fly high and wide: seen from the cockpit */
                    V3 sv = (k & 1)
                        ? vadd(vadd(vmul(c->v, 1.f), vmul(c->F, 3.f + 4.f * frand())),                     /* sprayed forward and up */
                               vadd(vmul(q.R, sd * (1.5f + 3.f * frand())), vmul(q.N, 3.f + 4.f * frand())))
                        : vadd(vadd(vmul(vt, 0.5f + 0.3f * frand()), vmul(q.R, sd * (1.f + 2.f * frand()))),
                               vadd(vmul(q.N, 1.f + 2.f * frand()), vmul(rnd_dir(), 1.5f)));
                    spawn(P_SPARK, E, sv, 0.35f + 0.45f * frand(), 0.05f, frand() < 0.5f ? 0xFFF0A0 : 0xFFB030);
                }
        }
    }
}

static void car_body_rows(const Car *c, float m[12]);
static float WBUMP[4];
/* the visual lean may not push a mount further down than its wheel's remaining bump travel (else the tyre sinks into the road) */
static void lean_limit(Car *c)
{
    if (WBUMP[0] <= 0) return;
    float m[12], k = 1;
    car_body_rows(c, m);
    for (int i = 0; i < 4; i++) {
        if (c->wl[i] >= WLF[i] - 1e-3f) continue;                     /* wheel in the air */
        V3 w = WMOUNT[i], lm = V(m[0] * w.x + m[1] * w.y + m[2] * w.z + m[3], m[4] * w.x + m[5] * w.y + m[6] * w.z + m[7],
                                m[8] * w.x + m[9] * w.y + m[10] * w.z + m[11]);
        float drop = -vdot(c->U, vsub(lm, body_pt(c, w))), room = fmaxf(WBUMP[i] - (WREST[i] - c->wl[i]), 0.f);
        if (drop > room) k = fminf(k, room / drop);
    }
    if (k < 1) { c->lr *= k; c->lp *= k; c->lrr *= k; c->lpr *= k; }
}

static int car_out(const Car *c) { return c->crash > 0 || c->crane > 0; }

/* the crane picks the car up and lowers it onto the track at segment s, like the original */
static void start_crane(Car *c, int s, float lat, float T)
{
    car_place(c, s, lat, 0);
    c->crane_x = c->x; c->crane_seg = c->seg; c->crane = 1e-4f; c->crane_T = T; c->crash = 0; c->hook = 0;
    c->lr = c->lp = c->lrr = c->lpr = 0;
}
static void crane_step(Car *c, float dt)
{
    c->crane += dt;
    float f = smooth01(c->crane / c->crane_T), sway = 0.04f * sinf(c->crane * 2.3f) * (1 - f);
    const Seg *s = &seg[c->crane_seg];
    c->x = vadd(c->crane_x, vmul(s->N, 6.5f * (1 - f)));
    c->F = vnorm(vadd(s->T, vmul(s->R, sway))); c->U = s->N; c->R = vcross(c->F, c->U);
    c->v = V(0, 0, 0); c->w = V(0, 0, 0);
    for (int i = 0; i < 4; i++) c->wl[i] = WLF[i];
    c->thr = 0; c->brk = 1; c->bst = 0; c->str = 0; c->delta = 0;
    if (c->crane >= c->crane_T) { c->crane = 0; c->hook = 2.0f; }     /* released: the hook goes back up */
}

/* body and wheel points against the ground beside the track; returns the hardest impact speed */
static float ground_contact(Car *c, V3 *F, V3 *Tq, int *touch)
{
    float worst = 0;
    for (int i = 0; i < 20; i++) {
        V3 lp = i < 16 ? CPTS[i] : vsub(WMOUNT[i - 16], V(0, WREST[i - 16] + WRAD[i - 16], 0));
        V3 P = body_pt(c, lp);
        float g = ground_h(P.x, P.z), pen = g - P.y;
        if (pen <= 0) continue;
        *touch = 1;
        V3 r = vsub(P, c->x), vp = vadd(c->v, vcross(c->w, r));
        float vn = vp.y; worst = fmaxf(worst, -vn);
        float Fn = fmaxf(160000.f * pen - 6000.f * vn, 0.f);
        V3 f = V(0, Fn, 0), vt = V(vp.x, 0, vp.z); float vts = vlen(vt);
        if (vts > 1e-3f) f = vadd(f, vmul(vt, -0.7f * Fn * tanhf(vts / 0.5f) / vts));
        *F = vadd(*F, f); *Tq = vadd(*Tq, vcross(r, f));
        if (vts > 2.f && frand() < 0.04f) spawn(P_DUST, P, vadd(vmul(vt, 0.2f), V(0, 1.2f, 0)), 1.0f + frand(), 0.25f + frand() * 0.3f, (i & 1) ? 0xB49A72 : 0x9C8460);
    }
    return worst;
}

static void car_step(Car *c, float dt)
{
    if (!c->active) return;
    if (c->hook > 0) c->hook -= dt;
    if (c->crane > 0) { crane_step(c, dt); return; }
    if (c->wrecked || c->crash > 0) { c->thr = 0; c->brk = 1; c->bst = 0; c->str = 0; }
    Q q; track_query(c->x, c->seg, 30, &q);
    if (q.valid) {
        int prev = c->seg, cu = q.seg;
        if (prev > nseg * 3 / 4 && cu < nseg / 4) {
            c->lap++;
            if (state == ST_RACE && c->lap >= 2) {
                c->lastlap = racetime - c->lapstart;
                if (c->bestlap <= 0 || c->lastlap < c->bestlap) c->bestlap = c->lastlap;
                c->lapshow = 3.f;
            }
            c->lapstart = racetime;
            if (c->lap > laps && !c->finished) { c->finished = 1; c->ftime = racetime; }
        } else if (prev < nseg / 4 && cu > nseg * 3 / 4) c->lap--;
        c->seg = cu; c->prog = (float)c->lap * nseg + cu + q.u;
    }

    if (c->hurt_cd > 0) c->hurt_cd -= dt;
    V3 Fo = V(0, -CAR_M * GRAV, 0), To = V(0, 0, 0);
    float speed = vdot(c->v, c->F);
    float dmax = steer_max(speed);
    float tgt = c->str * dmax, dd = tgt - c->delta, mr = ph.steerrate * dt;
    c->delta += clampf(dd, -mr, mr);

    float Fdrv = 0, brake = c->brk;
    if (c->thr > 0) {
        float P = ph.power, Fm = ph.fmax;
        if (c->bst && c->boost > 0) { P += ph.boostp; Fm += ph.boostf; c->boost -= dt * 14.f; if (c->boost < 0) c->boost = 0; }
        Fdrv = c->thr * fminf(Fm, P / fmaxf(speed, 3.f));
    }
    if (c->brk > 0 && speed < 0.8f && c->thr <= 0 && !c->hold) { Fdrv = -c->brk * 4500.f; brake = 0; }

    int nwc = 0;
    for (int i = 0; i < 4; i++) {
        V3 M = body_pt(c, WMOUNT[i]);
        Q wq; track_query(M, c->seg, 12, &wq);
        float hub = WLF[i]; int contact = 0; float l = WLF[i];
        float cosd = wq.valid ? vdot(c->U, wq.N) : 0;
        if (wq.valid && wq.road && fabsf(wq.lat) < TRACK_HW + 0.1f && cosd > 0.15f && wq.h > -0.6f) {
            float d = wq.h / cosd;
            l = d - WRAD[i];
            if (l < WLF[i]) { contact = 1; hub = l > 0 ? l : 0; }
        }
        c->wl[i] = hub;
        if (!contact) { c->wbot[i] = 0; continue; }
        nwc++;
        V3 N = wq.N, Pc = vsub(M, vmul(c->U, l + WRAD[i]));
        V3 r = vsub(Pc, c->x), vc = vadd(c->v, vcross(c->w, r));
        float rate = -vdot(vc, N);
        float comp = WLF[i] - l;
        float kk = i < 2 ? K_F : K_R, cs = sqrtf(kk / 14000.f);
        float Fs = kk * comp + (rate > 0 ? C_COMP : C_REB) * cs * rate;
        if (l < WLMIN[i]) {
            float x = WLMIN[i] - l;                                          /* progressive hydraulic stop */
            Fs += K_BUMP * x + 6.0e6f * x * x + (rate > 0 ? 9000.f * rate : 0.f);
            if (!c->wbot[i] && rate > 4.5f) car_hurt(c, (rate - 4.5f) * 1.2f, 1.f);
            c->wbot[i] = 1;
        } else c->wbot[i] = 0;
        if (Fs < 0) Fs = 0;
        if (Fs > 160000.f) Fs = 160000.f;
        V3 fN = vmul(N, Fs);
        Fo = vadd(Fo, fN); To = vadd(To, vcross(r, fN));

        V3 Fw0 = c->F;
        if (i < 2) Fw0 = vadd(vmul(c->F, cosf(c->delta)), vmul(c->R, sinf(c->delta)));
        V3 Fw = vnorm(vsub(Fw0, vmul(N, vdot(Fw0, N)))), Sw = vcross(Fw, N);
        float vlong = vdot(vc, Fw), vlat = vdot(vc, Sw);
        c->wph[i] = fmodf(c->wph[i] + vlong * dt / WRAD[i], 2 * PI_F);
        float Fx = 0;
        if (i >= 2) Fx += Fdrv * 0.5f;
        Fx -= brake * (i < 2 ? ph.brkf : ph.brkr) * tanhf(vlong * 0.25f);
        Fx -= 0.012f * Fs * tanhf(vlong * 3.f);
        float maxF = MU * (i < 2 ? 1.f : ph.rgrip) * fminf(Fs, ph.loadmax);   /* tyre load saturation: landing spikes don't add grip */
        float Fy = -maxF * tanhf(ph.corner * vlat / (fabsf(vlong) + ph.lowspd));
        float mag = sqrtf(Fx * Fx + Fy * Fy);
        if (mag > maxF && mag > 1e-3f) { float k = maxF / mag; Fx *= k; Fy *= k; }
        V3 ft = vadd(vmul(Fw, Fx), vmul(Sw, Fy));
        Fo = vadd(Fo, ft);
        V3 rr = vsub(r, vmul(c->U, 0.85f * vdot(r, c->U)));      /* tyre forces act near CoM height: no roll-over from grip */
        To = vadd(To, vcross(rr, ft));
    }
    c->nwc = nwc;
    for (int i = 0; i < 16; i++) contact_point(c, i, CPTS[i], &Fo, &To);
    edge_contact(c, &Fo, &To);
    int touch = 0;
    float impact = ground_contact(c, &Fo, &To, &touch);
    if (touch && c->crash <= 0) {                                    /* landed beside the track: crash */
        c->crash = 1e-4f; stat_falls++;
        car_hurt(c, 6.f + 0.5f * impact, 1.f);
        dust_burst(c->x, 36, 3.f + 0.3f * impact);
        if (c == &cars[0]) set_msg("CRASHED!", 2.0f);
        if (getenv("SR_DEBUG")) fprintf(stderr, "crash car%d seg %d impact %.1f t=%.2f\n", (int)(c - cars), c->seg, impact, racetime);
    }

    float sp2 = vlen(c->v);
    Fo = vsub(Fo, vmul(c->v, ph.drag * sp2));
    To = vsub(To, vmul(c->w, 180.f));
    if (nwc == 0 && sp2 > 6.f) {         /* air-control assist: nose follows the flight path, wings level */
        V3 vn = vmul(c->v, 1.f / sp2);
        V3 ax = vcross(c->F, vn);                                   /* axis taking the nose toward the velocity */
        To = vadd(To, vsub(vmul(ax, ph.air), vadd(vmul(c->R, 4500.f * vdot(c->w, c->R)), vmul(c->U, 4500.f * vdot(c->w, c->U)))));
        V3 Ut = vnorm(vsub(UP, vmul(c->F, vdot(UP, c->F))));
        To = vadd(To, vmul(c->F, 9000.f * vdot(vcross(c->U, Ut), c->F) - 3200.f * vdot(c->w, c->F)));
    }
    if (nwc > 0) {                      /* stability aid while on the ground */
        float g = nwc >= 3 ? 1.f : 0.5f;
        To = vsub(To, vmul(c->F, g * ph.sroll * vdot(c->w, c->F)));
        To = vsub(To, vmul(c->U, g * ph.syaw * vdot(c->w, c->U)));
        To = vsub(To, vmul(c->R, g * ph.spitch * vdot(c->w, c->R)));
    }

    {   /* exhaust flames: only while the boost burns */
        float tg = (c->bst && c->boost > 0 && c->thr > 0 && !c->wrecked) ? 1.4f : 0.f;
        c->flame += (tg - c->flame) * clampf(14.f * dt, 0, 1);
    }
    {   /* body lean: rocks the cab on its springs so cornering/braking/landing are visible */
        float al = vdot(Fo, c->R) / CAR_M, ag = vdot(Fo, c->F) / CAR_M, k = clampf(16.f * dt, 0, 1);
        c->alat_f += (clampf(al, -30, 30) - c->alat_f) * k; c->along_f += (clampf(ag, -25, 25) - c->along_f) * k;
        float tr = nwc > 1 ? clampf(-c->alat_f * ph.leanr, -0.20f, 0.20f) : 0.f;
        float tp = nwc > 1 ? clampf(c->along_f * ph.leanp, -0.14f, 0.14f) : 0.f;
        c->lrr += (140.f * (tr - c->lr) - 21.f * c->lrr) * dt; c->lr += c->lrr * dt;
        c->lpr += (120.f * (tp - c->lp) - 19.f * c->lpr) * dt; c->lp += c->lpr * dt;
        lean_limit(c);
    }
    c->v = vadd(c->v, vmul(Fo, dt / CAR_M));
    V3 tl = V(vdot(To, c->R), vdot(To, c->U), vdot(To, c->F));
    c->w = vadd(c->w, vmul(vadd(vmul(c->R, tl.x / IX), vadd(vmul(c->U, tl.y / IY), vmul(c->F, tl.z / IZ))), dt));
    float wlen = vlen(c->w);
    if (wlen > 14.f) { c->w = vmul(c->w, 14.f / wlen); wlen = 14.f; }
    if (sp2 > 130.f) c->v = vmul(c->v, 130.f / sp2);
    c->x = vadd(c->x, vmul(c->v, dt));
    if (wlen > 1e-6f) {
        V3 ax = vmul(c->w, 1 / wlen); float an = wlen * dt;
        c->F = rotv(c->F, ax, an); c->U = rotv(c->U, ax, an);
    }
    c->F = vnorm(c->F);
    c->U = vnorm(vsub(c->U, vmul(c->F, vdot(c->U, c->F))));
    c->R = vcross(c->F, c->U);

    if (nwc >= 3 && q.valid && q.road && fabsf(q.lat) < TRACK_HW - 1.5f) {
        int ok = 1, look = (int)(110 / ds);
        for (int k = -5; k < look && ok; k++) if (seg[((q.seg + k) % nseg + nseg) % nseg].gap) ok = 0;
        if (ok) c->safe = q.seg;
    }
    if (c->crash > 0) {                                              /* lying in the dirt: smoke, a little fire, then the crane */
        c->crash += dt; c->dust_t -= dt;
        if (c->dust_t <= 0) {
            c->dust_t = 0.12f;
            V3 eng = body_pt(c, V(0, FLAMEP[1].y, (FLAMEP[1].z + FLAMEP[2].z) * 0.5f));
            spawn(P_SMOKE, vadd(eng, vmul(rnd_dir(), 0.3f)), V(frand() - 0.5f, 1.5f, frand() - 0.5f), 2.5f + frand(), 0.3f, (frand() < 0.5f) ? 0x4A4A4E : 0x66666A);
            if (frand() < 0.6f) spawn(P_FIRE, vadd(eng, vmul(rnd_dir(), 0.4f)), V(0, 1.f, 0), 0.4f + 0.3f * frand(), 0.35f, frand() < 0.5f ? 0xFF8A18 : 0xFFD040);
        }
        if (c->crash > 3.4f) start_crane(c, c->safe, 0, 2.6f);
        return;
    }
    c->flip_t = c->U.y < 0.25f && vlen(c->v) < 6.f ? c->flip_t + dt : 0;
    c->stuck_t = (state == ST_RACE && vlen(c->v) < 1.0f && c->thr > 0.15f) ? c->stuck_t + dt : 0;
    if (c->flip_t > 2.5f || c->stuck_t > 2.5f || c->x.y < ground_h(c->x.x, c->x.z) - 4.f) {
        stat_falls++; car_hurt(c, 4.f, 1.f);
        if (getenv("SR_DEBUG")) fprintf(stderr, "reset car%d seg %d flip=%.1f stuck=%.1f\n", (int)(c - cars), c->seg, c->flip_t, c->stuck_t);
        if (c == &cars[0]) set_msg("RESET", 1.5f);
        start_crane(c, c->safe, 0, 2.6f);
    }
}

static void apply_ang(Car *a, V3 t, float dt)
{
    a->w = vadd(a->w, vmul(vadd(vmul(a->R, vdot(t, a->R) / IX), vadd(vmul(a->U, vdot(t, a->U) / IY), vmul(a->F, vdot(t, a->F) / IZ))), dt));
}
static void cars_interact(Car *a, Car *b, float dt)
{
    if (!a->active || !b->active || car_out(a) || car_out(b)) return;
    static const float off[5] = {-1.8f, -0.9f, 0.f, 0.9f, 1.8f};
    for (int i = 0; i < 5; i++)
        for (int j = 0; j < 5; j++) {
            V3 pa = body_pt(a, V(0, 0.1f, off[i])), pb = body_pt(b, V(0, 0.1f, off[j]));
            V3 d = vsub(pb, pa); float dist = vlen(d);
            if (dist >= 1.9f || dist < 1e-4f) continue;
            V3 n = vmul(d, 1 / dist);
            V3 ra = vsub(pa, a->x), rb = vsub(pb, b->x);
            V3 va = vadd(a->v, vcross(a->w, ra)), vb = vadd(b->v, vcross(b->w, rb));
            float vn = vdot(vsub(vb, va), n);
            float Fn = 30000.f * (1.9f - dist) - 2000.f * vn;
            if (Fn < 0) continue;
            stat_contacts++; stat_push += Fn * dt;
            V3 f = vmul(n, Fn);
            a->v = vsub(a->v, vmul(f, dt / CAR_M)); b->v = vadd(b->v, vmul(f, dt / CAR_M));
            apply_ang(a, vcross(ra, vmul(f, -1)), dt); apply_ang(b, vcross(rb, f), dt);
            if (-vn > 3.f) { car_hurt(a, 0.015f * (-vn - 3.f), 0.6f); car_hurt(b, 0.015f * (-vn - 3.f), 0.6f); }
        }
}

/* keep clear of the other car: never steer into it, tuck in behind when there is no room to pass */
static float ai_lane_clear(const Car *c, const Car *o, float lane, float *lift)
{
    *lift = 0;
    if (!o || !o->active || car_out(o)) return lane;
    const Seg *s0 = &seg[c->seg];
    V3 d = vsub(o->x, c->x);
    float dlong = vdot(d, s0->T), mylat = vdot(vsub(c->x, s0->c), s0->R), olat = mylat + vdot(d, s0->R);
    if (dlong < -6.f || dlong > 18.f) return lane;                    /* far behind or well ahead */
    const float need = 3.4f, edge = TRACK_HW - 1.7f;                   /* centre-to-centre room for two cars side by side */
    if (fabsf(lane - olat) >= need) return lane;
    float side = mylat >= olat ? 1.f : -1.f;                           /* stay on the side we are already on */
    float alt = clampf(olat + side * need, -edge, edge);
    if (fabsf(alt - olat) >= need - 0.2f) return alt;
    float other = clampf(olat - side * need, -edge, edge);             /* no room on our side */
    if (dlong > 4.f && fabsf(other - olat) >= need - 0.2f) return other;   /* pass on the other side */
    if (dlong > -1.f) *lift = 1;                                       /* alongside or behind: back off */
    return alt;
}

static void ai_drive(Car *c, float lane, float vcap, float alat, const Car *rival)
{
    float spd = vdot(c->v, c->F), lift = 0;
    lane = ai_lane_clear(c, rival, lane, &lift);
    int gapsoon = 0, gn = (int)(110 / ds);
    for (int k = -(int)(25 / ds); k < gn; k++) if (seg[((c->seg + k) % nseg + nseg) % nseg].gap) gapsoon = 1;
    const Seg *s0 = &seg[c->seg];
    if (gapsoon) lane = clampf(vdot(vsub(c->x, s0->c), s0->R), -2.5f, 2.5f);   /* hold the lane through a jump */
    V3 Th = vnorm(V(s0->T.x, 0, s0->T.z)), Rt = vnorm(vcross(Th, UP));
    V3 Fh = vnorm(V(c->F.x, 0, c->F.z));
    float e = vdot(vsub(c->x, s0->c), s0->R) - lane;                 /* cross-track error (+ = right of lane) */
    float hdg = atan2f(vdot(Fh, Rt), vdot(Fh, Th));                 /* heading error (+ = pointing right) */
    int jf = (c->seg + (int)((6 + fmaxf(spd, 0) * 0.3f) / ds)) % nseg;
    V3 Tf = vnorm(V(seg[jf].T.x, 0, seg[jf].T.z));
    float kf = vdot(vsub(Tf, Th), Rt) / (((jf - c->seg + nseg) % nseg + 1) * ds);
    float delta = -0.55f * hdg - atanf(2.2f * e / (fmaxf(spd, 0) + 4.f)) + atanf(3.4f * kf);
    float dmax = steer_max(spd);
    float dlim = 3.2f / (fmaxf(spd, 0) + 5.f);
    c->str = c->nwc == 0 ? 0.f : clampf(clampf(delta, -dlim, dlim) / dmax, -1.f, 1.f);
    float cmax = 0, vmax = 0, vmin = 0;
    int n = (int)((fmaxf(spd, 5) * 2.0f + 20) / ds);
    for (int k = 0; k <= n; k++) {
        const Seg *s = &seg[(c->seg + k) % nseg];
        cmax = fmaxf(cmax, s->curv); vmax = fmaxf(vmax, s->vcurv); vmin = fminf(vmin, s->vcurv);
    }
    float vt = fminf(vcap, sqrtf(alat / fmaxf(cmax, 1e-4f)));
    if (!gapsoon) vt = fminf(fminf(vt, 1.1f * sqrtf(GRAV / fmaxf(vmax, 1e-4f))), sqrtf(26.f / fmaxf(-vmin, 1e-4f)));
    else vt = clampf(vt, 30.f, 34.f);
    if (spd < vt) { c->thr = clampf((vt - spd) * 0.5f, 0.2f, 1.f); c->brk = 0; }
    else if (spd > vt * 1.06f && !gapsoon) { c->thr = 0; c->brk = clampf((spd - vt) * 0.15f, 0.15f, 1.f); }
    else { c->thr = 0.3f; c->brk = 0; }
    if (spd > 8.f) c->thr *= fmaxf(0.25f, 1.f - 0.85f * fabsf(c->str));
    if (lift) { c->thr = 0; if (spd > 6.f) c->brk = fmaxf(c->brk, 0.25f); }
    c->bst = 0;
}

/* ----------------------------------------------------------- car pose */
/* body frame including the visual roll/pitch lean */
static void body_frame(const Car *c, V3 *R, V3 *U, V3 *F)
{
    float cp = cosf(c->lp), sp = sinf(c->lp), cr = cosf(c->lr), sr = sinf(c->lr);
    V3 Fv = vadd(vmul(c->F, cp), vmul(c->U, sp)), Uv = vsub(vmul(c->U, cp), vmul(c->F, sp));
    *U = vadd(vmul(Uv, cr), vmul(c->R, sr)); *R = vsub(vmul(c->R, cr), vmul(Uv, sr)); *F = Fv;
}
/* model -> world matrix rows (3 x vec4) for the body: leans about a low roll centre */
static void car_body_rows(const Car *c, float m[12])
{
    V3 R, U, F; body_frame(c, &R, &U, &F);
    const V3 pv = {0.f, -0.45f, 0.f};
    V3 t = vadd(c->x, vsub(vadd(vmul(c->R, pv.x), vadd(vmul(c->U, pv.y), vmul(c->F, pv.z))),
                           vadd(vmul(R, pv.x), vadd(vmul(U, pv.y), vmul(F, pv.z)))));
    float r[12] = {R.x, U.x, F.x, t.x, R.y, U.y, F.y, t.y, R.z, U.z, F.z, t.z};
    memcpy(m, r, sizeof r);
}
static V3 car_hub(const Car *c, int i) { return vsub(body_pt(c, WMOUNT[i]), vmul(c->U, c->wl[i])); }
/* sky dome and mountains follow the camera */
static void draw_sky(void)
{
    static const float el[] = {-90, -3, 0, 4, 9, 16, 26, 40, 58, 90};
    enum { NB = 9, NS = 32 };
    const float R = 5800;
    for (int b = 0; b < NB; b++) {
        float t = b < 2 ? 0.f : clampf((el[b] - 2) / 62.f, 0, 1);
        uint32_t col = mixcol(FOGCOL, ZENCOL, sqrtf(t));
        float e0 = el[b] * PI_F / 180, e1 = el[b + 1] * PI_F / 180;
        for (int s = 0; s < NS; s++) {
            float a0 = s * 2 * PI_F / NS, a1 = (s + 1) * 2 * PI_F / NS;
            V3 p00 = vadd(cam.pos, V(R*cosf(e0)*cosf(a0), R*sinf(e0), R*cosf(e0)*sinf(a0)));
            V3 p01 = vadd(cam.pos, V(R*cosf(e0)*cosf(a1), R*sinf(e0), R*cosf(e0)*sinf(a1)));
            V3 p10 = vadd(cam.pos, V(R*cosf(e1)*cosf(a0), R*sinf(e1), R*cosf(e1)*sinf(a0)));
            V3 p11 = vadd(cam.pos, V(R*cosf(e1)*cosf(a1), R*sinf(e1), R*cosf(e1)*sinf(a1)));
            draw_tri_unlit(p00, p01, p11, col); draw_tri_unlit(p00, p11, p10, col);
        }
    }
    /* fair-weather cumulus behind the mountains: clusters of flat puffs facing the camera, white with grey undersides.
     * Every puff sits on its own depth layer (neighbouring clusters too), so overlapping puffs never z-fight. */
    for (int c = 0; c < 22; c++) {
        uint32_t h = hash2(c * 97 + 11, 5);
        float az = c * 2 * PI_F / 22 + (float)(h & 255) / 255.f * 0.25f, el = (4.f + (float)((h >> 8) & 255) / 255.f * 16.f) * PI_F / 180;
        V3 dir = V(cosf(el) * cosf(az), sinf(el), cosf(el) * sinf(az));
        V3 rt = vnorm(V(-sinf(az), 0, cosf(az))), up = UP;
        float w = (160 + (float)((h >> 16) & 255) * 1.2f) * 1.4f;
        for (int k = 0; k < 7; k++) {
            uint32_t hk = hash2(c * 13 + k, 77);
            float dist = 5000.f + (c & 1) * 300.f - k * 40.f, sc = dist / 5000.f;
            V3 ctr = vadd(cam.pos, vmul(dir, dist));
            float ox = ((float)(hk & 255) / 255.f - 0.5f) * w * 1.6f * sc, oy = (float)((hk >> 8) & 255) / 255.f * w * 0.28f * sc;
            float r = w * sc * (0.30f + 0.25f * (float)((hk >> 16) & 255) / 255.f);
            V3 pc = vadd(ctr, vadd(vmul(rt, ox), vmul(up, oy)));
            for (int q = 0; q < 10; q++) {
                float a0 = q * 2 * PI_F / 10, a1 = (q + 1) * 2 * PI_F / 10;
                V3 e0 = vadd(pc, vadd(vmul(rt, cosf(a0) * r), vmul(up, fmaxf(sinf(a0), -0.35f) * r * 0.62f)));
                V3 e1 = vadd(pc, vadd(vmul(rt, cosf(a1) * r), vmul(up, fmaxf(sinf(a1), -0.35f) * r * 0.62f)));
                draw_tri_unlit(pc, e0, e1, sinf(a0) + sinf(a1) < -0.2f ? 0xD6DCE6 : 0xFAFAFC);
            }
        }
    }
}

/* ------------------------------------------------------------ game logic */
static const bool *keys;
static int autoplay;

static void start_race(void)
{
    for (int i = 0; i < 2; i++) {
        Car *c = &cars[i];
        memset(c, 0, sizeof *c);
        c->active = 1; c->boost = 100; c->col = i ? 0x2860D8 : CAR_RED;
        start_crane(c, nseg - 14, i ? 2.0f : -2.0f, 2.4f);
    }
    npart = 0;
    racetime = 0; state = ST_COUNT; state_t = 3.99f; result = 0; msg_t = 0; stat_falls = 0;
}

/* ---- model viewer: the car alone on a plain floor, orbit camera ---- */
static float view_yaw = 0.6f, view_pitch = 0.22f, view_dist = 7.5f;
static int view_auto = 1, view_hide = 0, view_col = 0, view_flame = 0, view_anim = 1;
static V3 view_target = {0, 0.55f, 0};
static const uint32_t PALETTE[] = {CAR_RED, 0x2860D8, 0xF07818, 0x30A050, 0xE8D030, 0xF0F0F0, 0x303034, 0x9040C0};
static void start_view(void)
{
    memset(&cars[0], 0, sizeof(Car)); memset(&cars[1], 0, sizeof(Car));
    Car *c = &cars[0];
    c->active = 1; c->col = PALETTE[view_col % 8];
    c->x = V(0, REST_H, 0); c->F = V(0, 0, 1); c->U = V(0, 1, 0); c->R = vcross(c->F, c->U);
    for (int i = 0; i < 4; i++) c->wl[i] = WREST[i];
    state = ST_VIEW;
}
static void view_key(SDL_Keycode k)
{
    if (k == SDLK_SPACE) view_auto = !view_auto;
    else if (k == SDLK_N) { view_col = (view_col + 1) % 8; cars[0].col = PALETTE[view_col]; }
    else if (k == SDLK_R) view_hide ^= 1;
        else if (k == SDLK_E) view_hide ^= 4;
    else if (k == SDLK_F) view_flame = !view_flame;
    else if (k == SDLK_Z) view_anim = !view_anim;
    else if (k == SDLK_HOME) { view_yaw = 0.6f; view_pitch = 0.22f; view_dist = 7.5f; view_target = V(0, 0.55f, 0); }
}

static void setup_menu(void)
{
    if (cur_track != sel_track) build_track(sel_track);
    memset(&cars[0], 0, sizeof(Car)); memset(&cars[1], 0, sizeof(Car));
    cars[0].active = 1; cars[0].col = CAR_RED; cars[0].boost = 100;
    car_place(&cars[0], 40, 0, 25);
    npart = 0;
    state = ST_MENU;
}

static void game_tick(float dt)
{
    switch (state) {
    case ST_MENU:
        ai_drive(&cars[0], 0, 42, 8, NULL);
        car_step(&cars[0], dt);
        if (cars[0].damage > 40) { cars[0].damage = 0; cars[0].wrecked = 0; }
        a_vol = 0;
        break;
    case ST_COUNT: {
        int before = (int)ceilf(state_t);
        state_t -= dt;
        if ((int)ceilf(state_t) != before && state_t > 0) beep(440, 0.15f);
        for (int i = 0; i < 2; i++) { cars[i].thr = 0; cars[i].brk = 1; cars[i].hold = 1; car_step(&cars[i], dt); }
        if (state_t <= 0) { state = ST_RACE; racetime = 0; cars[0].hold = cars[1].hold = 0; beep(880, 0.5f); set_msg("GO!", 1.0f); }
        a_vol = 0.3f; a_freq = 40;
        break; }
    case ST_RACE: {
        racetime += dt;
        Car *p = &cars[0], *o = &cars[1];
        if (keys && !autoplay) {
            float tg = (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) - (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]);
            float rate = (tg != 0 && p->str * tg >= 0) ? ph.keyrise : ph.keyret;   /* tap = small correction, hold = more lock */
            float goal = tg != 0 ? tg : 0.f, dstr = goal - p->str, stp = rate * dt;
            p->str += clampf(dstr, -stp, stp);
            p->thr = (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) ? 1.f : 0.f;
            p->brk = (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]) ? 1.f : 0.f;
            p->bst = keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_LSHIFT];
        } else { static float push = -1; if (push < 0) push = getenv("SR_PUSH") ? (float)atof(getenv("SR_PUSH")) : 1.f; static float plane = -9; if (plane < -8) plane = getenv("SR_PLANE") ? (float)atof(getenv("SR_PLANE")) : -1.f;
               ai_drive(p, plane, 43 * push, 10.5f * push, NULL); if (push > 1.f) p->bst = 1; }   /* autopilot ignores the rival: worst case */
        ai_drive(o, 1.8f, skill_vcap[skill], skill_alat[skill], p);
        car_step(p, dt); car_step(o, dt);
        cars_interact(p, o, dt);
        float sp = vdot(p->v, p->F);
        a_freq = 38 + fabsf(sp) * 2.2f + p->thr * 14; a_vol = car_out(p) ? 0.15f : 0.55f + 0.45f * p->thr;
        if (p->finished || o->finished || p->wrecked || o->wrecked) {
            state = ST_DONE; state_t = 0;
            if (p->wrecked) result = -2;
            else if (o->wrecked) result = 2;
            else if (p->finished && (!o->finished || p->ftime <= o->ftime)) result = 1;
            else result = -1;
            beep(result > 0 ? 660 : 220, 0.8f);
        }
        break; }
    case ST_VIEW: {
        Car *c = &cars[0];
        if (view_auto) view_yaw += 0.35f * dt;
        if (keys) {
            float kx = (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) - (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]);
            float ky = (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) - (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]);
            float kz = (keys[SDL_SCANCODE_EQUALS] || keys[SDL_SCANCODE_KP_PLUS]) - (keys[SDL_SCANCODE_MINUS] || keys[SDL_SCANCODE_KP_MINUS]);
            view_yaw += kx * 1.6f * dt; view_pitch += ky * 0.9f * dt; view_dist *= 1.f - kz * 0.8f * dt;
            if (kx != 0) view_auto = 0;
        }
        view_pitch = clampf(view_pitch, -0.15f, 1.45f); view_dist = clampf(view_dist, 0.9f, 22.f);
        state_t += dt;
        if (view_anim) {
            c->delta = 0.30f * sinf(state_t * 0.9f);                           /* steering demo */
            for (int i = 0; i < 4; i++) c->wph[i] += dt * 1.8f;                /* wheels turn slowly */
            for (int i = 0; i < 4; i++) {                                      /* suspension demo: each wheel works through its travel */
                float sn = sinf(state_t * 1.1f + i * 1.7f), tr = WREST[i] - WLMIN[i];
                c->wl[i] = WREST[i] - sn * (sn > 0 ? tr : WLF[i] - WREST[i]) * 0.9f;
            }
        } else {                                                               /* stopped: straight ahead at ride height */
            c->delta = 0;
            for (int i = 0; i < 4; i++) c->wl[i] = WREST[i];
        }
        c->flame = view_flame ? 1.f : 0.f;
        a_vol = 0;
        break; }
    case ST_DONE:
        state_t += dt;
        for (int i = 0; i < 2; i++) { cars[i].thr = 0; cars[i].brk = 0.4f; cars[i].hold = 1; cars[i].str = 0; cars[i].bst = 0; car_step(&cars[i], dt); }
        a_vol = 0;
        break;
    }
    if (msg_t > 0) msg_t -= dt;
    for (int i = 0; i < 2; i++) if (cars[i].lapshow > 0) cars[i].lapshow -= dt;
    parts_step(dt);
}

/* ---------------------------------------------------------------- camera */
static V3 cam_dir, cam_pos_s, cam_f, cam_u;
static void update_camera(const Car *c, float dt, int snap)
{
    if (state == ST_VIEW) {
        V3 tg = view_target;
        cam.pos = vadd(tg, vmul(V(cosf(view_pitch) * sinf(view_yaw), sinf(view_pitch), cosf(view_pitch) * cosf(view_yaw)), view_dist));
        cam.fwd = vnorm(vsub(tg, cam.pos)); cam.right = vnorm(vcross(cam.fwd, UP)); cam.up = vcross(cam.right, cam.fwd);
        return;
    }
    V3 up_ref, Rv, Uv, Fv;
    body_frame(c, &Rv, &Uv, &Fv);
    if (snap) { cam_f = Fv; cam_u = Uv; }
    float ks = 1.f - expf(-40.f * dt);
    cam_f = vnorm(vlerp(cam_f, Fv, ks)); cam_u = vnorm(vlerp(cam_u, Uv, ks));
    cam_u = vnorm(vsub(cam_u, vmul(cam_f, vdot(cam_u, cam_f))));
    if (c->crash > 0 && state != ST_MENU) {                           /* crash: look at the wreck from outside */
        float a = atan2f(c->F.z, c->F.x) + 2.2f + 0.25f * c->crash;
        cam.pos = vadd(c->x, V(cosf(a) * 14.f, 6.f, sinf(a) * 14.f));
        cam.pos.y = fmaxf(cam.pos.y, ground_h(cam.pos.x, cam.pos.z) + 3.f);
        cam.fwd = vnorm(vsub(c->x, cam.pos)); up_ref = UP;
    } else if (cockpit && state != ST_MENU && state != ST_VIEW) {     /* races: driver's view only, like the original */
        float m[12]; car_body_rows(c, m);
        cam.pos = V(m[0]*EYE.x + m[1]*EYE.y + m[2]*EYE.z + m[3], m[4]*EYE.x + m[5]*EYE.y + m[6]*EYE.z + m[7], m[8]*EYE.x + m[9]*EYE.y + m[10]*EYE.z + m[11]);
        cam.fwd = vnorm(vsub(cam_f, vmul(cam_u, 0.04f)));
        up_ref = cam_u;
    } else {
        V3 fh = vnorm(V(c->F.x, c->F.y * 0.3f, c->F.z));
        if (snap) cam_dir = fh;
        cam_dir = vnorm(vlerp(cam_dir, fh, 1.f - expf(-5.f * dt)));
        V3 des = vadd(vsub(c->x, vmul(cam_dir, 10.0f)), V(0, 3.6f, 0));
        if (snap) cam_pos_s = des;
        cam_pos_s = vlerp(cam_pos_s, des, 1.f - expf(-9.f * dt));
        cam.pos = cam_pos_s;
        V3 target = vadd(c->x, vadd(vmul(c->U, 0.6f), vmul(cam_dir, 3.f)));
        cam.fwd = vnorm(vsub(target, cam.pos));
        up_ref = vnorm(vlerp(UP, cam_u, 0.3f));
    }
    cam.right = vnorm(vcross(cam.fwd, up_ref));
    cam.up = vcross(cam.right, cam.fwd);
}

#include "sr_main.h"
