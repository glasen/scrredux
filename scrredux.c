/*
 * SCR REDUX - a Stunt Car Racer inspired roller-coaster racer in a stadium.
 * SDL3 (window, input, audio) + SDL_GPU (rendering). Flat-shaded polygons, no textures.
 *
 * The car is a real rigid body: 4 spring/damper suspensions, tyre friction with
 * weight transfer, full 3D rotation, solid track deck / barrier collisions.
 *
 * Controls: arrows/WASD drive, SPACE boost, R reset to track,
 *           P/ESC pause menu, F11 fullscreen; keys and options in the Options menu.
 *
 * Debug:  scrredux --sim SECONDS [TRACK]
 *         scrredux --shot FILE.png SECONDS [TRACK] [CAM]
 *
 * Layout: this file = maths, world geometry, physics, AI, car model, game logic.
 *         scenery.h = stadium, arena floor and props, service vehicles.
 *         trackdef.h = tracks as grid pieces (originals and the builder's), league.h = season, Hall of Fame,
 *         saves and replays, menu.h = the menus, editor.h = the track builder.
 *         scr_main.h = HUD, PNG writer, SDL_GPU layer and main().
 */
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <time.h>

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
    float boostrate, shw, shdrop, vtop, vtopb;
} Phys;
static Phys ph;
#define GRAV (ph.grav)
#define MAXSEG 6000
#define TRACK_HW 4.425f                  /* half the original's 384-unit road */
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
static float steer_max(float v, float gl)        /* gl: gravity's help toward the side being steered (banking), m/s^2 */
{
    float d = ph.steer / (1.f + (v / ph.steerfall) * (v / ph.steerfall));
    float lim = atanf(2.67f * fmaxf(ph.steerlat + gl, 5.f) / fmaxf(v * v, 1.f));   /* never steer harder than the tyres can hold: no oversteer snap */
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

/* the text font: Rajdhani Bold as triangles (font_vec.h, made by tools/gen_font.py); cap height 1, y up */
#include "font_vec.h"
/* calls tri() for each of ch's triangles (x0, y0, x1, y1, x2, y2) */
static void glyph_tris(char ch, void (*tri)(void *u, const float *t), void *u)
{
    if (ch < 32 || ch > 126) return;
    for (int k = 0; k < FV_GLYPH[ch - 32].n; k++) tri(u, FV_TRI + 6 * (FV_GLYPH[ch - 32].first + k));
}
static float glyph_adv(char ch) { return ch < 32 || ch > 126 ? 0 : FV_GLYPH[ch - 32].adv; }
/* what may be typed into a name */
static const char NAME_CHARS[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.:-/!+<>?,abcdefghijklmnopqrstuvwxyz'()_=*%";

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
#include "audio.h"

/* ----------------------------------------------------------------- track */
typedef struct { V3 c, T, R, N; float u, curv, vcurv, bin; unsigned char gap, steep; } Seg;   /* bin: sin of the bank into the bend */   /* steep: sheer face, height measured vertically */
/* The eight tracks of the original (scr_tracks.h): each piece fills one cube of a 16x16 grid and is a shape template
 * (straight, diagonal or 45 degree curve) rotated in its cube, with a height profile per edge on top of a base height. */
#include "scr_tracks.h"
#define NTRACKS SCR_NTRACKS
#define SCR_U (2.95f / 128.f)                          /* metres per x/z unit: the car's 2.95 m wheelbase is 128 units */
#include "trackdef.h"

static Seg seg[MAXSEG]; static int nseg; static float ds;
static float jump_v[MAXSEG];                           /* AI: speed to hold here for the jump ahead (its run-up and the bend before it), 0 = none */
static unsigned char jump_b[MAXSEG];                   /* AI: that run-up needs the boost */
static int cur_track = -1;
/* crane drop points, like the original: the car is lowered onto the middle of a section, never onto a bend piece
 * (templates 1, 3, 6, 7) nor onto a section in the track's own list (the tail of each track's road data) */
static const unsigned char SCR_NOREST[SCR_NTRACKS][16] = {
    {1, 34}, {16, 17, 18, 19, 20, 21, 22, 47, 5}, {38}, {16, 17, 21, 22, 32, 33, 34, 35}, {32},
    {4, 13, 38, 51, 52, 53, 54}, {30, 31, 32, 33, 34, 37, 38, 39, 40, 41, 42, 43, 44, 45}, {0}};
static const unsigned char SCR_NNOREST[SCR_NTRACKS] = {2, 9, 1, 8, 1, 7, 14, 0};
/* our cars top out at 250 km/h on boost, the original's flew further: its jumps reshaped to suit (build_track_geometry) */
enum { ED_FILL, ED_LIFT, ED_STEP, ED_FLAT };
static const struct { unsigned char track, sec, kind; float m; } TRACK_EDIT[] = {
    {0, 33, ED_LIFT, 2}, {0, 0, ED_LIFT, 4},
    {1, 15, ED_LIFT, 2}, {1, 17, ED_LIFT, 2}, {1, 19, ED_LIFT, 2}, {1, 21, ED_LIFT, 2},
    {3, 17, ED_FILL, 0}, {3, 22, ED_FILL, 3}, {3, 34, ED_FILL, 10},
    {4, 32, ED_STEP, 8.5f},
    {5, 3, ED_LIFT, 5}, {5, 12, ED_LIFT, 4}, {5, 38, ED_FILL, 0},
    {6, 40, ED_LIFT, 8}, {6, 43, ED_FLAT, 0}};
static const struct { unsigned char track, sec; signed char from; float vd, smax, c, R; } TRACK_HILL[] = {
    {4, 3, -1, 50, 0.5f, 4, 120}, {5, 37, -1, 50, 0.5f, 4, 120}, {6, 40, 44, 56, 0.9f, 4, 150}};
static short seg_sec[MAXSEG]; static int sec_mid[MAXPC], nsec; static unsigned char sec_ok[MAXPC];
static float tr_minx, tr_maxx, tr_minz, tr_maxz;
static float ar_x0, ar_x1, ar_z0, ar_z1;        /* stadium arena bounds */
#define NET_H 8.f                               /* top of the catch net on the arena wall */

static float smooth01(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }
static void smooth_circ(float *a, int passes)
{
    static float tmp[MAXSEG];
    for (int p = 0; p < passes; p++) {
        for (int i = 0; i < nseg; i++) tmp[i] = (a[(i + nseg - 1) % nseg] + 2 * a[i] + a[(i + 1) % nseg]) * 0.25f;
        memcpy(a, tmp, sizeof(float) * nseg);
    }
}

static void plan_jumps(void);
static int planning;                                   /* a scratch car is on the track (plan_jumps): no debug output */

/* the track's cross-sections in driving order from the start line, with the jump edits: x/z in metres, heights in units */
enum { MAXX = MAXPC * 13, PER = 24 };
static float tx_ex[MAXX][2], tx_ez[MAXX][2]; static int tx_ey[MAXX][2], tx_sec[MAXX];
static int track_xsecs(const TrackDef *td)
{
    float (*ex)[2] = tx_ex, (*ez)[2] = tx_ez; int (*ey)[2] = tx_ey, *xsec = tx_sec;
    int np = td->np, start = td->start, orig = td->orig;
    /* cross-sections round the loop from the start line (the end of the start piece); a piece's last one is the next one's first */
    int nx = 0;
    nsec = np;
    for (int q = 0; q < np; q++) {
        int p = (start + 1 + q) % np;
        const TPiece *pc = &td->p[p];
        int t = pc->at & 15, ns = SCR_TPL_NS[t];
        sec_ok[p] = t != 1 && t != 3 && t != 6 && t != 7;
        for (int k = 0; orig >= 0 && k < SCR_NNOREST[orig]; k++) if (SCR_NOREST[orig][k] == p) sec_ok[p] = 0;
        for (int i = 0; i < ns; i++) {
            int xz[2][2];
            tpl_cross(pc->at, i, pc->cx, pc->cz, xz);
            for (int e = 0; e < 2; e++) {
                ex[nx][e] = -(xz[e][0] - 8 * 2048) * SCR_U;     /* mirrored: the original's world is left-handed */
                ez[nx][e] = (xz[e][1] - 8 * 2048) * SCR_U;
                ey[nx][e] = pc->h[e][i];
            }
            xsec[nx++] = p;
        }
    }
    /* the jump edits, m metres each (a sheer face is a step of 640 units or more between cross-sections) */
    const float U = 0.25f * SCR_U;
    for (int e = 0; e < (int)(sizeof TRACK_EDIT / sizeof TRACK_EDIT[0]); e++) {
        if (TRACK_EDIT[e].track != orig) continue;
        int i0 = 0, i, M = (int)(TRACK_EDIT[e].m / U);
        while (i0 < nx && xsec[i0] != TRACK_EDIT[e].sec) i0++;
        switch (TRACK_EDIT[e].kind) {
        case ED_FILL:                    /* a pit filled from this section on, up to its far face's top (sloping down by m to it) */
            for (i = i0; i + 1 < nx && ey[i + 1][0] - ey[i][0] < 640 && ey[i + 1][1] - ey[i][1] < 640; i++) {}
            if (i + 2 >= nx) break;
            for (int ed = 0; ed < 2; ed++) {
                int H = ey[i + 2][ed] > ey[i + 1][ed] ? ey[i + 2][ed] : ey[i + 1][ed];   /* a crest just past the face */
                ey[i + 1][ed] = H;
                for (int k = i0; k <= i; k++) ey[k][ed] = H + M * (i + 1 - k) / (i + 1 - i0);
            }
            break;
        case ED_LIFT:                    /* a ramp's lip raised by m, from nothing at the section start (the lip may be the next one's first) */
            for (i = i0; i + 1 < nx && xsec[i] == xsec[i0] && ey[i + 1][0] - ey[i][0] > -640 && ey[i + 1][1] - ey[i][1] > -640; i++) {}
            for (int k = i0 + 1; k <= i; k++) for (int ed = 0; ed < 2; ed++) ey[k][ed] += M * (k - i0) / (i - i0);
            break;
        case ED_STEP: {                  /* a step down onto a climb made shallower: the climb raised by m, tapering out at the crest */
            for (i = i0; i + 1 < nx && ey[i + 1][0] - ey[i][0] > -640 && ey[i + 1][1] - ey[i][1] > -640; i++) {}
            int kc = i + 1;
            for (int k = i + 1; k < nx && k < i + 40; k++) if (ey[k][0] + ey[k][1] > ey[kc][0] + ey[kc][1]) kc = k;
            for (int k = i + 1; k < kc; k++) for (int ed = 0; ed < 2; ed++) ey[k][ed] += M * (kc - k) / (kc - i - 1);
            break;
        }
        case ED_FLAT:                    /* a tower between two pits taken down to their floor: one gap */
            for (int ed = 0; ed < 2; ed++) {
                int lo = 1 << 30;
                for (int k = i0; k < nx && xsec[k] == xsec[i0]; k++) lo = ey[k][ed] < lo ? ey[k][ed] : lo;
                for (int k = i0; k < nx && xsec[k] == xsec[i0]; k++) ey[k][ed] = lo;
            }
            break;
        }
    }
    /* a drop too deep to land flat after at our speeds becomes a landing hill: a sheer step of c metres at the lip, then a
     * knoll the car follows up to vd m/s, steepening to a straight slope smax and curving out (radius ~R) onto the floor */
    for (int e = 0; e < (int)(sizeof TRACK_HILL / sizeof TRACK_HILL[0]); e++) {
        if (TRACK_HILL[e].track != orig) continue;
        int i0 = 0;
        while (i0 < nx && xsec[i0] != TRACK_HILL[e].sec) i0++;
        int i = i0;
        while (i + 1 < nx && ey[i + 1][0] - ey[i][0] > -640 && ey[i + 1][1] - ey[i][1] > -640) i++;
        const float vd = TRACK_HILL[e].vd, sm = TRACK_HILL[e].smax, c = TRACK_HILL[e].c, D = TRACK_HILL[e].R * sm * sm * 0.5f;
        float H = (ey[i][0] + ey[i][1]) * 0.5f * U, fl = 1e9f, d = 0;
        int kf = i + 1;                                                                /* the hill starts after the lip, or across a pit */
        if (TRACK_HILL[e].from >= 0) while (kf < nx && xsec[kf] != TRACK_HILL[e].from) kf++;
        for (int k = kf; k < nx && k < kf + 80; k++) {                                /* the floor: where the track first stops going down */
            float y = (ey[k][0] + ey[k][1]) * 0.5f * U;
            if (y > fl + 0.5f) break;
            fl = fminf(fl, y);
        }
        float th = ((ey[i][0] + ey[i][1]) - (ey[i - 1][0] + ey[i - 1][1])) * 0.5f * U /
                   hypotf((ex[i][0] + ex[i][1] - ex[i - 1][0] - ex[i - 1][1]) * 0.5f, (ez[i][0] + ez[i][1] - ez[i - 1][0] - ez[i - 1][1]) * 0.5f);
        static float tj[700];                                                          /* the flight from the lip at vd, with the car's drag */
        { float px = 0, py = 0, vx = vd / sqrtf(1 + th * th), vy = vx * th; int n = 0;
          for (int it = 0; it < 6000 && n < 700; it++) { float sp = hypotf(vx, vy), dt = 0.005f; vx -= ph.drag / ph.mass * sp * vx * dt; vy -= (GRAV + ph.drag / ph.mass * sp * vy) * dt;
                            px += vx * dt; py += vy * dt; while (n < 700 && n <= px) tj[n++] = py; }
          while (n < 700) { tj[n] = n ? tj[n - 1] - 10.f : py; n++; } }
        float xk = 699, yk;
        for (int x = 1; x < 699; x++) if (tj[x + 1] - tj[x] < -sm) { xk = (float)x; break; }
        yk = tj[(int)xk] - c;
        for (int k = i + 1; k < nx; k++) {
            d += hypotf((ex[k][0] + ex[k][1] - ex[k - 1][0] - ex[k - 1][1]) * 0.5f, (ez[k][0] + ez[k][1] - ez[k - 1][0] - ez[k - 1][1]) * 0.5f);
            float y = d < xk ? tj[(int)d] + (tj[(int)d + 1] - tj[(int)d]) * (d - (int)d) - c : yk - sm * (d - xk), r = y - (fl - H);
            y = r > D ? y : r > -D ? fl - H + (r + D) * (r + D) / (4 * D) : fl - H;    /* smooth max onto the floor */
            float yo = (ey[k][0] + ey[k][1]) * 0.5f * U - H;
            if (r < -D && yo <= fl - H + 0.05f) break;                                 /* down: the rest is the original's */
            if (k < kf) continue;
            for (int ed = 0; ed < 2; ed++) ey[k][ed] = ey[i][ed] + (int)(y / U);
        }
    }
    return nx;
}

static void build_track_geometry(int idx)
{
    const TrackDef *td = &trk[idx];
    int np = td->np, start = td->start, orig = td->orig;
    cur_track = idx; track_gen++;
    float (*ex)[2] = tx_ex, (*ez)[2] = tx_ez; int (*ey)[2] = tx_ey, *xsec = tx_sec;
    int nx = track_xsecs(td);
    (void)np; (void)start; (void)orig; (void)xsec;
    float ymin = 1e9f;
    for (int i = 0; i < nx; i++) ymin = fminf(ymin, fminf(ey[i][0], ey[i][1]) * 0.25f * SCR_U);
    float lift = fmaxf(0.f, 1.3f - ymin);                  /* roads at ground level sit on their deck slab */
    /* which template edge is on the right */
    V3 d0 = V(ex[1][0] + ex[1][1] - ex[0][0] - ex[0][1], 0, ez[1][0] + ez[1][1] - ez[0][0] - ez[0][1]);
    int re = vdot(vcross(d0, UP), V(ex[0][1] - ex[0][0], 0, ez[0][1] - ez[0][0])) > 0 ? 1 : 0;

    /* plan: centripetal Catmull-Rom through the cross-section centres; heights stay linear between cross-sections */
    static V3 fp[MAXX * PER]; static float fy[MAXX * PER], fb[MAXX * PER], cum[MAXX * PER + 1]; static unsigned char fs[MAXX * PER]; static short fsec[MAXX * PER];
    int M = 0;
    for (int i = 0; i < nx; i++) {
        V3 P[4];
        for (int q = 0; q < 4; q++) { int k = (i - 1 + q + nx) % nx; P[q] = V((ex[k][0] + ex[k][1]) * 0.5f, 0, (ez[k][0] + ez[k][1]) * 0.5f); }
        float t[4] = {0};
        for (int q = 1; q < 4; q++) t[q] = t[q - 1] + sqrtf(fmaxf(vlen(vsub(P[q], P[q - 1])), 1e-3f));
        int j = (i + 1) % nx;
        float yl0 = ey[i][1 - re], yr0 = ey[i][re], yl1 = ey[j][1 - re], yr1 = ey[j][re];
        int steep = fabsf(yl1 - yl0) >= 640 || fabsf(yr1 - yr0) >= 640;
        for (int k = 0; k < PER; k++) {
            float f = (float)k / PER, tt = t[1] + (t[2] - t[1]) * f;
            V3 A1 = vlerp(P[0], P[1], (tt - t[0]) / (t[1] - t[0])), A2 = vlerp(P[1], P[2], (tt - t[1]) / (t[2] - t[1]));
            V3 A3 = vlerp(P[2], P[3], (tt - t[2]) / (t[3] - t[2]));
            V3 B1 = vlerp(A1, A2, (tt - t[0]) / (t[2] - t[0])), B2 = vlerp(A2, A3, (tt - t[1]) / (t[3] - t[1]));
            fp[M] = vlerp(B1, B2, (tt - t[1]) / (t[2] - t[1]));
            /* a sheer face becomes a vertical step at the upper deck's end: the lip and the gap stay the original's */
            float fh = !steep ? f : (yl1 + yr1 < yl0 + yr0) ? clampf(f / 0.08f, 0, 1) : clampf((f - 0.92f) / 0.08f, 0, 1);
            float yl = yl0 + (yl1 - yl0) * fh, yr = yr0 + (yr1 - yr0) * fh;
            fy[M] = (yl + yr) * 0.5f * 0.25f * SCR_U + lift;
            fb[M] = (yl - yr) * 0.25f * SCR_U;              /* left edge minus right edge */
            fs[M] = steep; fsec[M] = (short)xsec[i];
            M++;
        }
    }
    cum[0] = 0;
    for (int i = 0; i < M; i++) cum[i + 1] = cum[i] + vlen(vsub(fp[(i + 1) % M], fp[i]));
    float total = cum[M];
    nseg = (int)(total / 1.0f + 0.5f); if (nseg > MAXSEG) nseg = MAXSEG;
    ds = total / nseg;
    static float bank[MAXSEG]; static unsigned char st[MAXSEG];
    int i = 0;
    for (int k = 0; k < nseg; k++) {
        float tg = k * ds;
        while (i < M - 1 && cum[i + 1] < tg) i++;
        float f = (tg - cum[i]) / (cum[i + 1] - cum[i]);
        int i2 = (i + 1) % M;
        seg[k].c = vlerp(fp[i], fp[i2], f);
        seg[k].c.y = fy[i] + (fy[i2] - fy[i]) * f;
        seg[k].u = tg;
        bank[k] = atan2f(fb[i] + (fb[i2] - fb[i]) * f, 2 * TRACK_HW);
        st[k] = fs[i]; seg_sec[k] = fsec[i];
    }
    for (int k = 0, k0 = 0; k < nseg; k++)
        if (k == nseg - 1 || seg_sec[k + 1] != seg_sec[k]) { sec_mid[seg_sec[k]] = (k0 + k) / 2; k0 = k + 1; }
    /* sharp hollows (the foot of the draw bridge's 30 degree ramps: an 11 m radius) are rounded off to ~80 m, away from sheer faces */
    for (int k = 0; k < nseg; k++) {
        int near = 0;
        for (int o = -6; o <= 6; o++) near |= st[(k + o + nseg) % nseg];
        if (near) continue;
        #define YS(o) seg[((k) + (o) + nseg) % nseg].c.y
        float s1 = (YS(-1) - YS(-4)) / (3 * ds), s2 = (YS(4) - YS(1)) / (3 * ds), d2 = YS(1) - 2 * YS(0) + YS(-1);
        if (s2 - s1 < 0.15f || d2 < YS(2) - 2 * YS(1) + YS(0) || d2 < YS(0) - 2 * YS(-1) + YS(-2)) continue;
        int W = (int)fminf(40.f * (s2 - s1) / ds, 25.f);
        float y0 = YS(-W) + s1 * W * ds;                                   /* where the two straights meet, about here */
        for (int j = -W; j <= W; j++) {
            float y = y0 + s1 * j * ds + (s2 - s1) * sq((j + W) * ds) / (4 * W * ds);
            int q = (k + j + nseg) % nseg; if (y > seg[q].c.y) seg[q].c.y = y;   /* only ever filled in */
        }
        #undef YS
        k += W;
    }
    for (int k = 0; k < nseg; k++) {
        int kn = (k + 1) % nseg;
        seg[k].gap = 0; seg[k].steep = (st[k] || st[kn]) && fabsf(seg[kn].c.y - seg[k].c.y) > 1.5f;
    }
    static float kap[MAXSEG], vk[MAXSEG]; static V3 Th[MAXSEG];
    for (int k = 0; k < nseg; k++) {
        int kp = (k + nseg - 1) % nseg, kn = (k + 1) % nseg;
        if (!seg[k].steep) { if (seg[kn].steep) kn = k; if (seg[kp].steep) kp = k; }
        seg[k].T = vnorm(vsub(seg[kn].c, seg[kp].c));
        Th[k] = vnorm(V(seg[k].T.x, 0, seg[k].T.z));
    }
    for (int k = 0; k < nseg; k++) {
        V3 Rh = vnorm(vcross(Th[k], UP));
        kap[k] = vdot(vsub(Th[(k + 1) % nseg], Th[(k + nseg - 1) % nseg]), Rh) / (2 * ds);
        vk[k] = -(seg[(k + 1) % nseg].T.y - seg[(k + nseg - 1) % nseg].T.y) / (2 * ds);
    }
    smooth_circ(kap, (int)(120 / ds)); smooth_circ(vk, (int)(30 / ds));
    for (int k = 0; k < nseg; k++) {
        seg[k].curv = fabsf(kap[k]); seg[k].vcurv = vk[k]; seg[k].bin = sinf(bank[k]) * (kap[k] >= 0 ? 1.f : -1.f);
        V3 R0 = vnorm(vcross(seg[k].T, UP)), N0 = vcross(R0, seg[k].T);
        float cb = cosf(bank[k]), sb = sinf(bank[k]);
        seg[k].R = vsub(vmul(R0, cb), vmul(N0, sb));
        seg[k].N = vadd(vmul(N0, cb), vmul(R0, sb));
        if (seg[k].steep) { seg[k].R = R0; seg[k].N = UP; }
    }
    tr_minx = tr_minz = 1e9f; tr_maxx = tr_maxz = -1e9f;
    for (int k = 0; k < nseg; k++) {
        tr_minx = fminf(tr_minx, seg[k].c.x); tr_maxx = fmaxf(tr_maxx, seg[k].c.x);
        tr_minz = fminf(tr_minz, seg[k].c.z); tr_maxz = fmaxf(tr_maxz, seg[k].c.z);
    }
    ar_x0 = tr_minx - 90; ar_x1 = tr_maxx + 90; ar_z0 = tr_minz - 90; ar_z1 = tr_maxz + 90;
}

/* flat stadium floor, like the original */
static float ground_h(float x, float z) { (void)x; (void)z; return 0; }

typedef struct { int seg, valid; float u, lat, h; V3 N, R, T; int road; } Q;

static void track_query(V3 p, int hint, int range, Q *q)
{
    float best = 1e30f; int bj = -1, bg = 0; float bu = 0;
    for (int o = -range; o <= range; o++) {
        int j = ((hint + o) % nseg + nseg) % nseg;
        const Seg *s = &seg[j], *s2 = &seg[(j + 1) % nseg];
        V3 ch = vsub(s2->c, s->c), d = vsub(p, s->c);
        if (s->steep) { ch.y = 0; d.y = 0; }
        float cc2 = vdot(ch, ch), al = vdot(d, ch) / cc2;
        /* on the outside of a bend consecutive segments leave thin wedges that neither covers: accept a small
         * overshoot, clamped to the segment end, so a wheel crossing a wedge never loses the deck for a tick */
        if (al < -0.25f || al > 1.25f) continue;
        float alc = clampf(al, 0, 0.9999f);
        V3 e = vsub(p, vlerp(s->c, s2->c, alc));
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
        float p = (-4.6f + k * 2.3f + 1.1f * sinf(u / 41.f + k * 1.7f) + 0.5f * sinf(u / 13.f + k)) * (TRACK_HW / 6.9f);
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
        if (s->steep) {                                     /* sheer step: a red and white end panel like the side skirts */
            uint32_t col = is_edge(k) ? 0x6A6A72 : (((k - 1) / 4) & 1) ? COL_RED : 0xE8E8E8;
            draw_quad(a0, a1, b1, b0, col); draw_quad(a0, b0, b1, a1, col);
            continue;
        }
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
    if (s->steep) return;
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
    if (i % 3 == 0 && !gn && !gp && !s->steep) {
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
            tk[k] = s->c.y + s->R.y * qx[k] * (TRACK_HW - 1.6f) + s->T.y * qz[k] * 1.4f - 1.2f * s->N.y - 0.95f;
    }
    V3 Th = vnorm(V(s->T.x, 0, s->T.z)), Rh = vnorm(vcross(Th, UP));
    float spread = TRACK_HW - 1.6f, bspread = spread + top * 0.14f;
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
    int seg, lap, lapmax, finished, wrecked, active, safe, bst, nwc, hold;   /* lapmax: the furthest lap begun */
    float thr, brk, str, delta, boost, damage, prog, ftime, lapstart, bestlap, lastlap, lapshow;
    float crash, crane, crane_T, hook, dust_t;    /* crash on the ground / crane lowering timers, hook retract timer */
    V3 crane_x;                                   /* where the crane sets the car down */
    int crane_seg;
    float wl[4], wph[4], flip_t, stuck_t, hurt_cd, smash_t;   /* smash_t: after a smash, hits only creak for a while */
    unsigned char hit[16], wbot[4], wcon[4];
    float lr, lp, lrr, lpr, alat_f, along_f;      /* visual body roll/pitch (angle, rate) and filtered accelerations */
    float flame;                                  /* exhaust flame strength (0..1.4), boost only */
    int nholes;                                   /* big hits: holes in the cockpit's top tube this season (see hole_at) */
    uint32_t col;
    float ai_lane; int ai_lset;                   /* AI: the lane it is easing towards its target */
} Car;
/* the original's ten hole spots on the 240-pixel crack: 12 pixels wide, every 24, filled from the right end */
#define NHOLE 10
#define HOLE_W 5.f
static float hole_at(int k) { return (8 + 24 * (NHOLE - 1 - k)) / 2.4f; }   /* k-th hole's start, in % of the crack */

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

enum { ST_MENU, ST_COUNT, ST_RACE, ST_DONE, ST_VIEW, ST_REPLAY, ST_EDIT, ST_PREVIEW };
static Car cars[2];
static int state = ST_MENU, laps = 3, sel_track = 0, result = 0, paused = 0, cockpit = 1;
static float state_t, racetime;
static char msg[64]; static float msg_t;
static int stat_falls, stat_contacts; static float stat_push;
static const float skill_vcap[3] = {56, 62, 69}, skill_alat[3] = {11.0f, 13.5f, 16.0f};

static void set_msg(const char *s, float t) { snprintf(msg, sizeof msg, "%s", s); msg_t = t; }
#include "league.h"
#include "replay.h"
#include "options.h"
#include "controls.h"

/* ---- sound: when the original's samples play (StuntCarRacer.s, Car_Behaviour.cpp), periods and volumes from its tables ---- */
#define AMI_V (30720.f / 72.f)               /* original speed units per m/s: its top speed 0x7800 = our unboosted ~72 m/s */
static int eng_revs, eng_chg, eng_fluct, eng_on;   /* the original's engine.revs model */
static float snd_t10, snd_t50, grounded_cd, hitcar_cd;
static int snd_scrape, snd_dirt;             /* player since the last 10 Hz frame: steel scraping the deck / sliding in the dirt */
static uint32_t snd_rng = 1;
static int snd_rand(void) { snd_rng = snd_rng * 1664525u + 1013904223u; return (int)(snd_rng >> 16); }   /* not frand: the game's sequence stays as it was */
static int snd_live(const Car *c) { return c == &cars[0] && !planning && (state == ST_COUNT || state == ST_RACE || state == ST_DONE); }
static void snd_grounded(const Car *c, float rate)
{
    if (!snd_live(c) || grounded_cd > 0) return;
    snd_play(S_GROUNDED, 400, (int)clampf(28 + (rate - 3.f) * 8, 28, 64)); grounded_cd = 0.5f;
    pad_rumble(clampf((rate - 3.f) * 0.06f, 0.1f, 0.5f), 90);
}
/* UpdateEngineRevs, at the original's ~10 Hz frame rate */
static void engine_revs(int touch, int input, int zs)
{
    int c = touch ? (zs & ~0xf) : (input ? 0x9000 : 0);
    c = (c + 0x580) >> 3;
    c = eng_revs < 192 && c < 256 ? 2 : (c - eng_revs) >> 3;   /* slow climb from a standstill, until the car pulls away (after the crane) */
    if (c >= 0x100) c = 0x100;
    else if (c < (touch ? -0x100 : -0x20)) c = touch ? -0x100 : -0x20;
    eng_chg = c; eng_fluct = snd_rand() & 0xf;
}
/* FramesWheelsEngine, 50 Hz: the revs pick a sample and a period below 256, which is the same as TickOver at 47.3 * (revs + 378) Hz */
static void engine_frame(void)
{
    eng_revs += eng_chg; if (eng_revs < 0) eng_revs = 0;
    int period = 4800000 / (eng_revs + 378), k = 6;
    if (period >= 0x3fff) period = 0x3ffe;
    period |= eng_fluct;
    if (period < 124) period = 124;
    while (period >= 256) { period >>= 1; if (--k < 0) k = 0; }
    snd_engine(k, period);
}
static void snd_tick(float dt)
{
    static int quiet;
    const Car *p = &cars[0];
    grounded_cd -= dt; hitcar_cd -= dt;
    if (state == ST_MENU || state == ST_VIEW || state == ST_EDIT || state == ST_PREVIEW || (state == ST_REPLAY && (rp_paused || rp_speed != 1))) {
        if (!quiet) snd_stop_all(); quiet = 1; eng_on = eng_revs = eng_chg = 0; return; }
    quiet = 0;
    if (p->crane > 0) {                      /* on the crane all is quiet, the engine starts again from a standstill */
        if (eng_on) snd_engine_stop();
        eng_on = eng_revs = eng_chg = 0; snd_scrape = snd_dirt = 0;
    } else {
        if ((snd_t10 += dt) >= 0.1f) {
            snd_t10 -= 0.1f;
            int zs = (int)(fabsf(vdot(p->v, p->F)) * AMI_V), fer = zs >> 8 < 50 ? zs >> 8 : 50;
            if (state == ST_DONE) eng_chg = -8;      /* race over: the revs wind down to nothing */
            else engine_revs(p->nwc > 0, (state == ST_RACE || state == ST_REPLAY) && !p->wrecked && p->crash <= 0 && (p->thr > 0 || p->brk > 0), zs);
            if (snd_scrape && p->crash <= 0 && fer >= 1) snd_play(S_WRECK, (31 - (fer >> 1)) * 4 + 170, 64);
            if (snd_dirt && vlen(p->v) > 1.5f) snd_play(S_OFFROAD, 450 + (snd_rand() & 0x1c), 64);
            snd_scrape = snd_dirt = 0;
        }
        for (snd_t50 += dt; snd_t50 >= 0.02f; snd_t50 -= 0.02f) {
            if (state == ST_DONE && eng_revs <= 0) { if (eng_on) snd_engine_stop(); eng_on = 0; }
            else { engine_frame(); eng_on = 1; }
        }
    }
    if (snd_dump) snd_dump_frames(dt);
}
/* pause: the engine stops, effects play out */
static void snd_paused(void) { snd_engine_stop(); }

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
    float x = c->damage, left = amt;                                    /* the crack crosses holes for free */
    while (left > 0) {
        float next = 1e9f; int in = 0;
        for (int k = 0; k < c->nholes && !in; k++) {
            float a = hole_at(k);
            if (x >= a && x < a + HOLE_W) { x = a + HOLE_W; in = 1; }
            else if (a > x && a < next) next = a;
        }
        if (!in) { float st = fminf(left, next - x); x += st; left -= st; }
    }
    c->damage = x;
    if (snd_live(c)) pad_rumble(0.3f + amt * 0.06f, (int)fminf(120 + amt * 25, 600));   /* hits, crashes, the net: the pad shakes */
    if (getenv("SCR_DEBUG") && amt > 1.5f && c == &cars[0]) fprintf(stderr, "hurt %.1f seg %d u=%.2f v=%.1f nwc=%d vy=%.1f\n", amt, c->seg, seg[c->seg].u, vlen(c->v), c->nwc, c->v.y);
    if (c->damage >= 100) { c->damage = 100; c->wrecked = 1; }
    int smash = amt >= 8.f && loud > 0 && c->smash_t <= 0 && c->nholes < NHOLE;   /* big hit: a hole, then a while with only creaks (the original's 69 frames) */
    if (smash) { c->smash_t = 6.9f; c->nholes++; }
    if (snd_live(c) && loud > 0) {
        if (smash) snd_play(S_SMASH, 280, 64);
        else if (amt >= 0.5f) snd_play(S_CREAK, 238, (int)clampf(amt * 10.f, 28, 64));
    }
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

/* a sheer face going up just behind or at segment k: its segment, else -1 */
static int climb_face(int k)
{
    for (int o = -3; o <= 1; o++) {
        int f = ((k + o) % nseg + nseg) % nseg;
        if (seg[f].steep && seg[(f + 1) % nseg].c.y > seg[f].c.y) return f;
    }
    return -1;
}
/* below the upper deck past a sheer face going up: the face is a wall there. How far P is through it, else 0 */
static float in_climb_wall(V3 P, int f, V3 *n)
{
    const Seg *s = &seg[f], *s2 = &seg[(f + 1) % nseg];
    V3 Th = vnorm(V(s2->c.x - s->c.x, 0, s2->c.z - s->c.z));
    float d = vdot(vsub(P, s->c), Th);
    if (d <= 0 || d > 4.f || P.y > s2->c.y - 0.3f) return 0;
    *n = vmul(Th, -1);
    return d;
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
    int stp = seg[q.seg].steep || seg[(q.seg + 1) % nseg].steep || seg[(q.seg + nseg - 1) % nseg].steep;   /* solid down to the ground */
    int cf = al < ow ? climb_face(q.seg) : -1; V3 wn; float wd = cf >= 0 ? in_climb_wall(P, cf, &wn) : 0;
    if (wd > 0) { h = -wd; topy = 0; stp = 0; }                   /* the wall: pushed straight back out, never up the face */
    if (al > ow || (h < -1.2f && !stp && wd <= 0) || h > topy) { c->hit[idx] = 0; return; }
    float best = topy - h; V3 bn = wd > 0 ? wn : q.N;
    float pb = h + 1.2f; if (pb < best && !stp && wd <= 0) { best = pb; bn = vmul(q.N, -1); }
    float ps = ow - al; if (ps < best && wd <= 0) { best = ps; bn = vmul(q.R, sg); }
    if (seg[(q.seg + nseg - 1) % nseg].gap) { float pst = q.u * ds; if (pst < best) { best = pst; bn = vmul(q.T, -1); } }
    if (seg[(q.seg + 1) % nseg].gap) { float pe = (1 - q.u) * ds; if (pe < best) { best = pe; bn = q.T; } }
    V3 r = vsub(P, c->x), vp = vadd(c->v, vcross(c->w, r));
    float vn = vdot(vp, bn);
    float Fn = 250000.f * best - 7000.f * vn;
    if (Fn < 0) Fn = 0;
    if (Fn > 220000.f) Fn = 220000.f;
    if (stp && Fn > 0.6f * CAR_M * GRAV) Fn = 0.6f * CAR_M * GRAV;   /* a sheer face lifts the car, it does not fire it (the original clamps too) */
    V3 f = vmul(bn, Fn);
    V3 vt = vsub(vp, vmul(bn, vn)); float vts = vlen(vt);
    if (vts > 1e-3f) f = vadd(f, vmul(vt, -0.25f * Fn * tanhf(vts / 0.5f) / vts));
    *F = vadd(*F, f); *Tq = vadd(*Tq, vcross(r, f));
    if (vts > 3.f && Fn > 1500.f && c == &cars[0]) snd_scrape = 1;
    if (vts > 3.f && Fn > 1500.f && frand() < 0.35f)                  /* steel on the deck: sparks */
        for (int k = 0; k < 4; k++)
            spawn(P_SPARK, P, vadd(vadd(vmul(vt, 0.55f + 0.3f * frand()), vmul(bn, 1.5f + 2.f * frand())), vmul(rnd_dir(), 2.f)),
                  0.25f + 0.35f * frand(), 0.05f, frand() < 0.5f ? 0xFFF0A0 : 0xFFB030);
    if (!c->hit[idx] && -vn > 9.0f) car_hurt(c, (-vn - 9.0f) * 0.5f, 1.f);
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
            if (vts > 2.f && c == &cars[0]) snd_scrape = 1;
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

/* where the crane puts the car back: the middle of the section it was last on, or of the nearest one before it allowed */
static int restart_seg(const Car *c)
{
    int s = seg_sec[c->safe], s0;
    for (int n = 0; n < nsec && !sec_ok[s]; n++) s = (s + nsec - 1) % nsec;
    s0 = s;                                    /* and not where a jump ahead needs a run-up: from a standstill it would fall in */
    for (int n = 0; n < nsec && (!sec_ok[s] || jump_v[sec_mid[s]] > 0); n++) s = (s + nsec - 1) % nsec;
    return sec_mid[sec_ok[s] && jump_v[sec_mid[s]] <= 0 ? s : s0];
}

/* the crane picks the car up and lowers it onto the track at segment s, like the original */
static void start_crane(Car *c, int s, float lat, float T)
{
    s = ((s % nseg) + nseg) % nseg;
    if (c->lap > 0 && c->seg < nseg / 4 && s > nseg * 3 / 4) c->lap--;   /* set down behind the start line: that lap again */
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

/* the catch net on the arena wall: it brakes the car at up to ~80 g and hardly pushes back, so a car is caught in it
 * instead of bouncing off. A car that went over the top is outside and stays there. */
static float fence_contact(Car *c, V3 *F, V3 *Tq, int *touch)
{
    V3 P[20], n[20]; float fn[20], sum = 0, worst = 0; int np = 0;
    for (int side = 0; side < 4; side++) {
        float out = side == 0 ? ar_x0 - c->x.x : side == 1 ? c->x.x - ar_x1 : side == 2 ? ar_z0 - c->x.z : c->x.z - ar_z1;
        if (out > 3.f) continue;
        V3 nn = side == 0 ? V(1, 0, 0) : side == 1 ? V(-1, 0, 0) : side == 2 ? V(0, 0, 1) : V(0, 0, -1);
        for (int i = 0; i < 20 && np < 20; i++) {
            V3 lp = i < 16 ? CPTS[i] : vsub(WMOUNT[i - 16], V(0, WREST[i - 16] + WRAD[i - 16], 0));
            V3 p = body_pt(c, lp);
            float pen = -vdot(vsub(p, c->x), nn) + out;
            if (pen <= 0 || p.y >= NET_H) continue;
            float vn = vdot(vadd(c->v, vcross(c->w, vsub(p, c->x))), nn);
            worst = fmaxf(worst, -vn);
            P[np] = p; n[np] = nn; fn[np] = fmaxf(300.f * pen - 8000.f * vn, 0.f); sum += fn[np++];
        }
    }
    if (!np) return 0;
    *touch = 1;
    float k = sum > 800000.f ? 800000.f / sum : 1.f;
    for (int j = 0; j < np; j++) {
        V3 r = vsub(P[j], c->x), vp = vadd(c->v, vcross(c->w, r)), vt = vsub(vp, vmul(n[j], vdot(vp, n[j])));
        float Fn = fn[j] * k, vts = vlen(vt);
        V3 f = vmul(n[j], Fn);
        if (vts > 1e-3f) f = vadd(f, vmul(vt, -0.5f * Fn * tanhf(vts / 0.5f) / vts));
        *F = vadd(*F, f); *Tq = vadd(*Tq, vcross(r, f));
    }
    return worst;
}

static void crash_smoke(const Car *c)
{
    V3 eng = body_pt(c, V(0, FLAMEP[1].y, (FLAMEP[1].z + FLAMEP[2].z) * 0.5f));
    spawn(P_SMOKE, vadd(eng, vmul(rnd_dir(), 0.3f)), V(frand() - 0.5f, 1.5f, frand() - 0.5f), 2.5f + frand(), 0.3f, (frand() < 0.5f) ? 0x4A4A4E : 0x66666A);
    if (frand() < 0.6f) spawn(P_FIRE, vadd(eng, vmul(rnd_dir(), 0.4f)), V(0, 1.f, 0), 0.4f + 0.3f * frand(), 0.35f, frand() < 0.5f ? 0xFF8A18 : 0xFFD040);
}
static void lap_done(Car *c);
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
            if (c->lap > c->lapmax) {          /* a new lap; not the line again after reversing or a crane drop behind it */
                c->lapmax = c->lap;
                if (state == ST_RACE && c->lap >= 2) {
                    c->lastlap = racetime - c->lapstart;
                    if (c->bestlap <= 0 || c->lastlap < c->bestlap) c->bestlap = c->lastlap;
                    c->lapshow = 3.f;
                    lap_done(c);
                }
                c->lapstart = racetime;
                if (laps > 0 && c->lap > laps && !c->finished && state == ST_RACE) { c->finished = 1; c->ftime = racetime; }
            }
        } else if (prev < nseg / 4 && cu > nseg * 3 / 4) c->lap--;
        c->seg = cu; c->prog = (float)c->lap * nseg + cu + q.u;
    }

    if (c->hurt_cd > 0) c->hurt_cd -= dt;
    if (c->smash_t > 0) c->smash_t -= dt;
    V3 Fo = V(0, -CAR_M * GRAV, 0), To = V(0, 0, 0);
    float speed = vdot(c->v, c->F);
    float gl = -GRAV * c->R.y, dmax = steer_max(speed, c->str >= 0 ? gl : -gl);
    float tgt = c->str * dmax, dd = tgt - c->delta, mr = ph.steerrate * dt;
    c->delta += clampf(dd, -mr, mr);

    float Fdrv = 0, brake = c->brk;
    if (c->thr > 0) {
        float P = ph.power, Fm = ph.fmax;
        if (c->bst && c->boost > 0) { P += ph.boostp; Fm += ph.boostf; c->boost -= dt * ph.boostrate; if (c->boost < 0) c->boost = 0; }
        float vtop = c->bst && c->boost > 0 ? ph.vtopb : ph.vtop;      /* the engine cuts out at the top speed, boost or not, like the original */
        Fdrv = c->thr * fminf(Fm, P / fmaxf(speed, 3.f)) * clampf((vtop - speed) / 2.f, 0, 1);
    }
    if (c->brk > 0 && speed < 0.8f && c->thr <= 0 && !c->hold) { Fdrv = -c->brk * 4500.f; brake = 0; }

    int nwc = 0;
    for (int i = 0; i < 4; i++) {
        V3 M = body_pt(c, WMOUNT[i]);
        Q wq; track_query(M, c->seg, 12, &wq);
        float hub = WLF[i]; int contact = 0; float l = WLF[i];
        float over = fabsf(wq.lat) - (TRACK_HW + 0.1f);
        if (wq.valid && over > 0) {
            wq.h += ph.shdrop + 0.5f * over;
            wq.N = vnorm(vadd(wq.N, vmul(wq.R, wq.lat > 0 ? 0.5f : -0.5f)));
        }
        float cosd = wq.valid ? vdot(c->U, wq.N) : 0;
        int wcf = wq.valid ? climb_face(wq.seg) : -1; V3 wn;
        if (wcf >= 0 && in_climb_wall(M, wcf, &wn) > 0) wq.valid = 0;                 /* below the top of a face going up: the wall */
        if (wq.valid && wq.road && over < ph.shw && cosd > 0.15f && wq.h > (seg[wq.seg].steep ? -8.f : -0.6f)) {
            float d = wq.h / cosd;
            l = d - WRAD[i];
            if (l < WLF[i]) { contact = 1; hub = l > 0 ? l : 0; }
        }
        c->wl[i] = hub;
        int was = c->wcon[i]; c->wcon[i] = contact;
        if (!contact) { c->wbot[i] = 0; continue; }
        nwc++;
        V3 N = wq.N, Pc = vsub(M, vmul(c->U, l + WRAD[i]));
        V3 r = vsub(Pc, c->x), vc = vadd(c->v, vcross(c->w, r));
        float rate = -vdot(vc, N);
        if (!was && rate > 4.f) snd_grounded(c, rate);                       /* hard touchdown */
        float comp = WLF[i] - l;
        float kk = i < 2 ? K_F : K_R, cs = sqrtf(kk / 14000.f);
        float Fs = kk * comp + (rate > 0 ? C_COMP : C_REB) * cs * rate;
        if (l < WLMIN[i]) {
            float x = WLMIN[i] - l;                                          /* progressive hydraulic stop */
            Fs += K_BUMP * x + 6.0e6f * x * x + (rate > 0 ? 9000.f * rate : 0.f);
            if (!c->wbot[i] && rate > 2.5f) snd_grounded(c, rate);          /* bottomed out: the original's grounded sound */
            if (!c->wbot[i] && rate > 7.f) car_hurt(c, (rate - 7.f) * 0.6f, 1.f);
            c->wbot[i] = 1;
        } else c->wbot[i] = 0;
        if (Fs < 0) Fs = 0;
        if (Fs > 160000.f) Fs = 160000.f;
        if (seg[wq.seg].steep && Fs > 1.5f * CAR_M * GRAV) Fs = 1.5f * CAR_M * GRAV;
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
    int touch = 0, net = 0;
    float impact = ground_contact(c, &Fo, &To, &touch);
    if (touch && c == &cars[0]) snd_dirt = 1;
    impact = fmaxf(impact, 0.5f * fence_contact(c, &Fo, &To, &net));   /* the net gives: a softer hit */
    if ((touch || net) && c->crash <= 0) {                           /* landed beside the track or caught in the net: crash */
        c->crash = 1e-4f; stat_falls++;
        car_hurt(c, 6.f + 0.5f * impact, 1.f);
        dust_burst(c->x, 36, 3.f + 0.3f * impact);
        if (c == &cars[0]) set_msg("CRASHED!", 2.0f);
        if (getenv("SCR_DEBUG") && !planning) fprintf(stderr, "crash car%d seg %d impact %.1f t=%.2f\n", (int)(c - cars), c->seg, impact, racetime);
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

    if (nwc >= 3 && q.valid && q.road && fabsf(q.lat) < TRACK_HW - 1.5f) c->safe = q.seg;
    if (c->crash > 0) {                                              /* lying in the dirt: smoke, a little fire, then the crane */
        c->crash += dt; c->dust_t -= dt;
        if (c->dust_t <= 0) { c->dust_t = 0.12f; crash_smoke(c); }
        if (c->crash > 3.4f) start_crane(c, restart_seg(c), 0, 2.6f);
        return;
    }
    c->flip_t = c->U.y < 0.25f && vlen(c->v) < 6.f ? c->flip_t + dt : 0;
    c->stuck_t = (state == ST_RACE && vlen(c->v) < 1.0f && c->thr > 0.15f) ? c->stuck_t + dt : 0;
    if (c->flip_t > 2.5f || c->stuck_t > 2.5f || c->x.y < ground_h(c->x.x, c->x.z) - 4.f) {
        stat_falls++; car_hurt(c, 4.f, 1.f);
        if (getenv("SCR_DEBUG") && !planning) fprintf(stderr, "reset car%d seg %d flip=%.1f stuck=%.1f\n", (int)(c - cars), c->seg, c->flip_t, c->stuck_t);
        if (c == &cars[0]) set_msg("RESET", 1.5f);
        start_crane(c, restart_seg(c), 0, 2.6f);
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
            if (-vn > 1.f && hitcar_cd <= 0 && (snd_live(a) || snd_live(b))) { snd_play(S_HITCAR, 238, 56); hitcar_cd = 0.5f; }
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
    const float need = 3.4f, edge = TRACK_HW - 2.2f;                   /* centre-to-centre room for two cars side by side */
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
    for (int k = -(int)(25 / ds); k < gn; k++) { const Seg *sk = &seg[((c->seg + k) % nseg + nseg) % nseg]; if (sk->gap || sk->steep) gapsoon = 1; }
    const Seg *s0 = &seg[c->seg];
    if (gapsoon) lane = clampf(vdot(vsub(c->x, s0->c), s0->R), -1.5f, 1.5f);   /* hold the lane through a jump, well in from the edges */
    if (jump_v[c->seg] > 0) lane = clampf(lane, -2.2f, 2.2f);                 /* a run-up is driven near the middle, whoever is about */
    if (!c->ai_lset) { c->ai_lane = vdot(vsub(c->x, s0->c), s0->R); c->ai_lset = 1; }
    c->ai_lane += clampf(lane - c->ai_lane, -2.f / 240.f, 2.f / 240.f);       /* change lanes at 2 m/s, never with a jerk */
    lane = c->ai_lane;
    V3 Th = vnorm(V(s0->T.x, 0, s0->T.z)), Rt = vnorm(vcross(Th, UP));
    V3 Fh = vnorm(V(c->F.x, 0, c->F.z));
    float e = vdot(vsub(c->x, s0->c), s0->R) - lane;                 /* cross-track error (+ = right of lane) */
    float hdg = atan2f(vdot(Fh, Rt), vdot(Fh, Th));                 /* heading error (+ = pointing right) */
    int jf = (c->seg + (int)((6 + fmaxf(spd, 0) * 0.3f) / ds)) % nseg;
    V3 Tf = vnorm(V(seg[jf].T.x, 0, seg[jf].T.z));
    float kf = vdot(vsub(Tf, Th), Rt) / (((jf - c->seg + nseg) % nseg + 1) * ds);
    float delta = -0.55f * hdg - atanf(2.2f * e / (fmaxf(spd, 0) + 4.f)) + atanf(3.4f * kf);
    float gl = -GRAV * c->R.y, dmax = steer_max(spd, delta >= 0 ? gl : -gl);
    float dlim = 3.2f / (fmaxf(spd, 0) + 5.f);
    c->str = c->nwc == 0 ? 0.f : clampf(clampf(delta, -dlim, dlim) / dmax, -1.f, 1.f);
    float vmax = 0, vmin = 0, vbend = 1e9f;
    int n = (int)((fmaxf(spd, 5) * 2.0f + 20) / ds);
    for (int k = 0; k <= n; k++) {
        int sk = (c->seg + k) % nseg; const Seg *s = &seg[sk];
        float cap = sqrtf(fmaxf(alat + GRAV * s->bin, 3.f) / fmaxf(s->curv, 1e-4f));   /* banking holds the car in */
        vbend = fminf(vbend, fmaxf(cap, jump_v[sk]));                  /* the bend before a jump is taken near the limit */
        vmax = fmaxf(vmax, s->vcurv); vmin = fminf(vmin, s->vcurv);
    }
    float vt = fminf(vcap, vbend), jv = jump_v[c->seg];
    if (jv > 0) vt = jv;                                              /* run-up: flat out, then down to the take-off speed */
    else if (!gapsoon) {                                              /* a little float over crests, none out wide where the deck may twist */
        float fl = fabsf(vdot(vsub(c->x, s0->c), s0->R)) > 1.6f ? 0.95f : 1.1f;
        vt = fminf(fminf(vt, fl * sqrtf(GRAV / fmaxf(vmax, 1e-4f))), sqrtf(26.f / fmaxf(-vmin, 1e-4f)));
    }
    else vt = clampf(vt, 30.f, 34.f);
    if (spd < vt) { c->thr = clampf((vt - spd) * (jv > 0 ? 2.f : 0.5f), 0.2f, 1.f); c->brk = 0; }
    else if (jv > 0 ? spd > vt + 1.f : spd > vt * 1.06f && !gapsoon) { c->thr = 0; c->brk = clampf((spd - vt) * 0.15f, 0.15f, 1.f); }
    else { c->thr = 0.3f; c->brk = 0; }
    if (spd > 8.f) c->thr *= fmaxf(0.25f, 1.f - 0.85f * fabsf(c->str));
    if (lift && jv <= 0) { c->thr = 0; if (spd > 6.f) c->brk = fmaxf(c->brk, 0.25f); }
    if (c->nwc < 3) c->brk = 0;                                       /* no braking in the air or on an unsettled landing */
    else c->brk *= 1.f - 0.7f * fabsf(c->str);                        /* leave the tyres grip to steer with */
    c->bst = jump_b[c->seg] && spd < jv - 1.f;
}

/* ---- AI jump planning: a scratch car tries each jump with the real physics, so bounces off a far face count too ---- */
#define RUN_ALAT 36.f              /* lateral grip the AI uses in the bend before a jump (m/s^2, before banking) */
#define NJ 64
static float bend_hold(int k) { return seg[k].curv > 0.004f ? sqrtf(fmaxf(RUN_ALAT + GRAV * seg[k].bin, 3.f) / seg[k].curv) : 1e9f; }
/* fastest the AI goes at seg k, q segments before a take-off: bends near the limit, no flying off a crest on the way */
static float run_cap(int k, int q)
{
    float v = bend_hold(k);
    if (q > (int)(40 / ds) && seg[k].vcurv > 0.002f) v = fminf(v, 1.2f * sqrtf(GRAV / seg[k].vcurv));
    return v;
}
/* run-up to a take-off at speed vt from seg L: flat out (boost if b) up to a braking curve that meets vt at the lip,
 * back through the last bend before it */
static void run_up(int L, float vt, int b)
{
    int inb = 0;
    for (int q = 0; q < (int)(700 / ds); q++) {
        int si = (L - q + nseg) % nseg;
        if (seg[si].steep) break;
        if (bend_hold(si) < 1e9f) inb = 1;
        else if (inb) break;
        float v = fminf(run_cap(si, q), sqrtf(vt * vt + 2 * 6.f * q * ds));
        if (v >= jump_v[si]) { jump_v[si] = v; jump_b[si] = (unsigned char)b; }
    }
}

/* a scratch car from seg s at speed v0 to seg goal, settled on its wheels: the damage it took, 1000 if it crashed,
 * got stuck or never got there. *vmark: its speed at seg mark */
static float scratch_run(int s, float v0, int mark, int goal, float lane, int boost, float *vmark)
{
    Car t; memset(&t, 0, sizeof t);
    t.active = 1; t.boost = boost ? 100 : 0;
    car_place(&t, s, lane, v0);
    int dm = (mark - s + nseg) % nseg, dg = (goal - s + nseg) % nseg, seen = 0;
    for (int i = 0; i < 240 * 20; i++) {
        ai_drive(&t, lane, skill_vcap[1], skill_alat[1], NULL);
        car_step(&t, 1.f / 240);
        if (t.crash > 0 || t.crane > 0 || t.wrecked) return 1000;
        float d = t.prog - s;
        if (d >= dm && !seen) { seen = 1; if (vmark) *vmark = vdot(t.v, t.F); }
        if (d >= dg && (t.nwc >= 3 || goal == mark)) return t.damage;
    }
    return 1000;
}

/* the scratch car over a chain of n jumps (lips l[], -1 = no run-up of its own), all taken off at v, from seg s
 * (at no more than v0) */
static float try_jumps(const int *l, int n, float v, int b, int s, float v0, int goal, float lane, float *vmark)
{
    memset(jump_v, 0, sizeof(float) * nseg); memset(jump_b, 0, nseg);
    for (int k = 0; k < n; k++) if (l[k] >= 0) run_up(l[k], v, b);
    if (jump_v[s] > 0) v0 = fminf(v0, jump_v[s]);
    return scratch_run(s, fminf(v0, v), l[0], goal, lane, b, vmark);
}

/* a jump starts at a sheer face going down and runs on over the next faces while the road between is no place to
 * land: far below the lip (a pit) or above it (a wall to clear). Jumps with little road between them (stepping
 * stones) form a chain taken at one speed: the one the scratch car got over cleanest, with a little either side to
 * spare, that the run-up can reach (without boost if it can) */
static int badj[NJ], nbadj;                     /* lips of jumps nothing gets over cleanly */
static int plan_gen = -1;                       /* the track_gen the plan is for */
static void plan_jumps(void)
{
    int lip[NJ], end[NJ], back[NJ], nj = 0;
    nbadj = 0;
    memset(jump_v, 0, sizeof(float) * nseg); memset(jump_b, 0, nseg);
    for (int k = 0; k < nseg && nj < NJ; k++) {
        int l = (k + nseg - 1) % nseg;
        if (!seg[k].steep || seg[l].steep || seg[(k + 2) % nseg].c.y > seg[k].c.y) continue;   /* only faces going down */
        int e = k;
        float ylip = seg[k].c.y;
        for (;;) {
            while (seg[(e + 1) % nseg].steep) e = (e + 1) % nseg;
            int m = 1, lim = (int)(300 / ds);
            while (m < lim && !seg[(e + m) % nseg].steep) m++;
            if (m >= lim) break;
            float lo = 1e9f, hi = -1e9f;
            for (int q = 1; q < m; q++) { lo = fminf(lo, seg[(e + q) % nseg].c.y); hi = fmaxf(hi, seg[(e + q) % nseg].c.y); }
            if (!(hi < ylip - 2.f || lo > ylip + 2.f)) break;
            e = (e + m) % nseg;
        }
        int bk = 0;
        while (bk < (int)(700 / ds) && !seg[(l - bk - 1 + nseg) % nseg].steep) bk++;
        lip[nj] = l; end[nj] = e; back[nj] = bk; nj++;
    }
    const int CHAIN = (int)(150 / ds);
    int h0 = 0;
    while (h0 < nj && back[h0] < CHAIN) h0++;
    if (h0 == nj) h0 = 0;
    float cv[NJ]; int cb[NJ], cl[NJ][NJ], cn[NJ], nc = 0;
    int st0 = state; float rt0 = racetime; int np0 = npart, sf0 = stat_falls, ss0 = stat_sparks; uint32_t rs0 = rngs;
    state = ST_RACE; planning = 1;
    for (int j = h0; j < h0 + nj; ) {
        int *l = cl[nc], n = 0, J = j % nj;
        do { l[n++] = back[j % nj] < (int)(40 / ds) ? -1 : lip[j % nj]; j++; } while (j < h0 + nj && back[j % nj] < CHAIN);
        int L = lip[J], E = end[(j - 1) % nj], bk = back[J], nxt = lip[j % nj];
        int dgo = (int)(200 / ds), dnx = (nxt - E + nseg) % nseg - (int)(30 / ds);
        int goal = (E + (nj > 1 && dnx < dgo ? (dnx > 2 ? dnx : 2) : dgo)) % nseg;
        /* fastest take-off: flat out from the start of the last bend, the bend at the limit */
        int q0 = 0;
        while (q0 < bk && bend_hold((L - q0 + nseg) % nseg) >= 1e9f) q0++;
        while (q0 < bk && bend_hold((L - q0 + nseg) % nseg) < 1e9f) q0++;
        q0 = q0 + (int)(10 / ds) < bk ? q0 + (int)(10 / ds) : bk - (int)(5 / ds);
        int rs = (L - q0 + nseg) % nseg;
        float vr[2] = {0, 0};
        for (int b = 0; b < 2; b++) try_jumps(l, 1, 200.f, b, rs, 55.f, L, 0, &vr[b]);
        /* what each take-off speed does */
        enum { V0 = 20, NV = 92 };
        float sc[NV], best = 1e9f; int bv = 0;
        int vtop = (int)fminf(vr[1] + 3, V0 + NV - 1);
        int ts = (L - (bk < CHAIN ? bk - (int)(5 / ds) : CHAIN) + nseg) % nseg;
        for (int v = V0; v < V0 + NV; v++) sc[v - V0] = v <= vtop ? try_jumps(l, n, (float)v, 1, ts, 200.f, goal, 0, NULL) : 1000;
        for (int pass = 0; pass < 2; pass++)                 /* pass 1: the best few again from either side of the deck */
            for (int v = V0 + 1; v < V0 + NV - 1; v++) {
                float r = fmaxf(sc[v - V0], fmaxf(sc[v - V0 - 1], sc[v - V0 + 1]));
                float cost = r + (v > vr[0] - 1.5f ? 3.f : 0.f) + 20.f * fmaxf(v - (vr[1] - 1.5f), 0.f) - 0.05f * v;   /* clean, then fast */
                if (pass == 1 && cost < best + 5.f) {
                    for (int sd = -1; sd <= 1; sd += 2)
                        cost += fmaxf(0.f, try_jumps(l, n, (float)v, 1, ts, 200.f, goal, 2.f * sd, NULL) - r);
                    if (cost < best) { best = cost; bv = v; }
                } else if (pass == 0 && cost < best) { best = cost; bv = v; }
            }
        cv[nc] = (float)bv; cb[nc] = bv > vr[0] - 1.5f; cn[nc] = n;
        if (nbadj < NJ && best >= 500.f) badj[nbadj++] = L;          /* no take-off speed gets over it: the builder says so */
        if (getenv("SCR_DEBUG")) printf("jump seg %d..%d (%d): %.1f m/s (reach %.1f, %.1f on boost; risk %.1f)\n", L, E, n, cv[nc], vr[0], vr[1], best);
        nc++;
    }
    memset(jump_v, 0, sizeof(float) * nseg); memset(jump_b, 0, nseg);
    for (int c = 0; c < nc; c++)
        for (int k = 0; k < cn[c]; k++) if (cl[c][k] >= 0) run_up(cl[c][k], cv[c], cb[c]);
    state = st0; racetime = rt0; npart = np0; stat_falls = sf0; stat_sparks = ss0; rngs = rs0; planning = 0;
    plan_gen = track_gen;
}
/* the plan is only needed once the AI drives: at the start of a race, not when a track is shown */
static void plan_need(void) { if (plan_gen != track_gen) plan_jumps(); }

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
static int autoplay;

static const char *player_name(void) { return sea.pname[0] ? sea.pname : "Player"; }
/* season races: three laps against one opponent; practice and the builder's test drive: alone, laps until ESC */
static void start_race(void)
{
    plan_need();
    int solo = race_mode == RM_PRACTICE || race_mode == RM_TEST;
    if (race_mode == RM_SEASON) laps = 3; else if (solo) laps = 0;
    for (int i = 0; i < 2; i++) {
        Car *c = &cars[i];
        memset(c, 0, sizeof *c);
        if (i && solo) continue;
        c->active = 1; c->boost = 100; c->col = i ? 0x2860D8 : CAR_RED;
        start_crane(c, nseg - 14, solo ? 0.f : i ? 2.0f : -2.0f, 2.4f);
    }
    if (race_mode == RM_SEASON) cars[0].nholes = sea.nholes;
    npart = 0;
    racetime = 0; state = ST_COUNT; state_t = 3.99f; result = 0; msg_t = 0; stat_falls = 0; paused = 0;
    if (race_mode == RM_SEASON) rec_begin(); else rec_on = 0;
}
/* a lap in a season race or in practice: the Hall of Fame's lap record */
static void lap_done(Car *c)
{
    if (planning || (race_mode != RM_SEASON && race_mode != RM_PRACTICE)) return;
    int me = c == &cars[0];
    if (hof_lap(trk[cur_track].name, me ? player_name() : drv_name(opp_drv), c->lastlap) && me) set_msg("NEW LAP RECORD!", 2.5f);
}
/* replay: the recorded cars at the replay clock; crashes smoke again */
static void rp_step(float dt)
{
    if (!rp_paused) rp_t += dt * rp_speed;
    float tmax = (nrp - 1) / (float)RP_HZ;
    rp_t = clampf(rp_t, 0, tmax);
    int k = (int)(rp_t * RP_HZ); if (k >= nrp) k = nrp - 1;
    const RTick *t = &rpb[k];
    for (int i = 0; i < 2; i++) {
        Car *c = &cars[i]; float was = c->crash;
        rec_unpack(c, &t->c[i]);
        if (rp_paused || rp_speed < 0) continue;
        if (c->crash > 0 && was <= 0) dust_burst(c->x, 36, 4.f);
        if (c->crash > 0 && (rp_smoke[i] -= dt) <= 0) { rp_smoke[i] = 0.12f; crash_smoke(c); }
    }
    racetime = t->t;
}
/* the car seen from its driver's seat, -1 for none */
static int fp_car(void)
{
    if (state == ST_COUNT || state == ST_RACE || state == ST_DONE) return cockpit ? 0 : -1;
    if (state == ST_REPLAY) return rp_cam == RC_COCKPIT ? 0 : rp_cam == RC_OPP_COCKPIT && cars[1].active ? 1 : -1;
    return -1;
}
static int cam_car(void) { return state == ST_REPLAY && (rp_cam == RC_OPP_COCKPIT || rp_cam == RC_OPP_CHASE) && cars[1].active ? 1 : 0; }

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

/* the menu: the camera turns round the empty track (preview_cam) */
static float orbit_t;
static void setup_menu(void)
{
    if (cur_track != sel_track) build_track(sel_track);
    memset(cars, 0, sizeof cars);
    npart = 0; paused = 0;
    state = ST_MENU;
}

static void game_tick(float dt)
{
    switch (state) {
    case ST_MENU:
        orbit_t += dt;
        break;
    case ST_COUNT: {
        int before = (int)ceilf(state_t);
        state_t -= dt;
        if ((int)ceilf(state_t) != before && state_t > 0) snd_play(S_CHIME, 150, 40);
        for (int i = 0; i < 2; i++) { cars[i].thr = 0; cars[i].brk = 1; cars[i].hold = 1; car_step(&cars[i], dt); }
        if (state_t <= 0) { state = ST_RACE; racetime = 0; cars[0].hold = cars[1].hold = 0; snd_play(S_CHIME, 75, 40); set_msg("GO!", 1.0f); }
        break; }
    case ST_RACE: {
        racetime += dt;
        Car *p = &cars[0], *o = &cars[1];
        if (keys && !autoplay) {
            PadIn pi = pad_read();
            float tg = ctl_down(CT_RIGHT) - ctl_down(CT_LEFT);
            if (tg == 0) tg = pi.steer;                                       /* a stick steers part way */
            float rate = (tg != 0 && p->str * tg >= 0) ? ph.keyrise : ph.keyret;   /* tap = small correction, hold = more lock */
            float goal = tg != 0 ? tg : 0.f, dstr = goal - p->str, stp = rate * dt;
            p->str += clampf(dstr, -stp, stp);
            p->thr = ctl_down(CT_ACCEL) ? 1.f : pi.thr;
            p->brk = ctl_down(CT_BRAKE) ? 1.f : pi.brk;
            p->bst = ctl_down(CT_BOOST) || pi.bst;
        } else { static float push = -1; if (push < 0) push = getenv("SCR_PUSH") ? (float)atof(getenv("SCR_PUSH")) : 1.f; static float plane = -9; if (plane < -8) plane = getenv("SCR_PLANE") ? (float)atof(getenv("SCR_PLANE")) : -1.f;
               ai_drive(p, plane, 67 * push, 13.f * push, NULL); if (push > 1.f) p->bst = 1; }   /* autopilot ignores the rival: worst case */
        if (o->active) ai_drive(o, 1.8f, opp_vcap, opp_alat, p);
        car_step(p, dt); car_step(o, dt);
        cars_interact(p, o, dt);
        if (p->finished || o->finished || p->wrecked || o->wrecked) {
            state = ST_DONE; state_t = 0;
            if (p->wrecked) result = -2;
            else if (o->wrecked) result = 2;
            else if (p->finished && (!o->finished || p->ftime <= o->ftime)) result = 1;
            else result = -1;
            snd_play(S_CHIME, result > 0 ? 100 : 300, 40);
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
        break; }
    case ST_DONE:
        state_t += dt;
        for (int i = 0; i < 2; i++) { cars[i].thr = 0; cars[i].brk = 0.4f; cars[i].hold = 1; cars[i].str = 0; cars[i].bst = 0; car_step(&cars[i], dt); }
        break;
    case ST_REPLAY:
        rp_step(dt);
        break;
    case ST_EDIT:
        break;
    case ST_PREVIEW:
        orbit_t += dt;
        break;
    }
    if (state == ST_COUNT || state == ST_RACE || state == ST_DONE) rec_tick();
    if (msg_t > 0) msg_t -= dt;
    for (int i = 0; i < 2; i++) if (cars[i].lapshow > 0) cars[i].lapshow -= dt;
    parts_step(dt);
    snd_tick(dt);
}

/* ---------------------------------------------------------------- camera */
static V3 cam_dir, cam_pos_s, cam_f, cam_u;
static void update_camera(const Car *c, float dt, int snap)
{
    if (state == ST_PREVIEW || state == ST_MENU) return;             /* set by preview_cam */
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
    } else if (fp_car() >= 0) {                                       /* races: driver's view only, like the original */
        float m[12]; car_body_rows(c, m);
        cam.pos = V(m[0]*EYE.x + m[1]*EYE.y + m[2]*EYE.z + m[3], m[4]*EYE.x + m[5]*EYE.y + m[6]*EYE.z + m[7], m[8]*EYE.x + m[9]*EYE.y + m[10]*EYE.z + m[11]);
        cam.fwd = vnorm(vsub(cam_f, vmul(cam_u, 0.04f)));
        up_ref = cam_u;
    } else if (state == ST_REPLAY && rp_cam == RC_TV) {               /* replay: trackside cameras, each takes the car past it */
        int k = ((c->seg / 120 + 1) * 120) % nseg;
        const Seg *s = &seg[k];
        V3 Rh = vnorm(V(s->R.x, 0, s->R.z));
        cam.pos = vadd(s->c, vadd(vmul(Rh, ((c->seg / 120) & 1 ? 1.f : -1.f) * (TRACK_HW + 9.f)), V(0, 3.5f, 0)));
        cam.pos.y = fmaxf(cam.pos.y, 2.f);
        cam.fwd = vnorm(vsub(vadd(c->x, V(0, 0.5f, 0)), cam.pos)); up_ref = UP;
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

#include "scr_main.h"
