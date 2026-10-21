/* core/vertex_buffer.h - CPU vertex buffers and the immediate-mode triangle emitters */
#pragma once

/* ------------------------------------------------------- vertex buffers */
typedef struct {
    float x, y, z;
    uint8_t r, g, b, a;
} Vtx;

typedef struct {
    Vtx *v;
    int n, cap;
} VB;

static VB sb, db, hb,
    tkb; /* static world, dynamic world, HUD; the track builder's track (its own buffer, rebuilt as it changes) */
static int tkb_gen; /* bumped when tkb is rebuilt */
static VB *cur = &db;

static struct {
    V3 pos, right, up, fwd;
} cam;

static void vb_reserve(VB *b, int add)
{
    if (b->n + add <= b->cap) {
        return;
    }

    int nc = b->cap ? b->cap : 1 << 16;

    while (nc < b->n + add) {
        nc *= 2;
    }

    b->v = realloc(b->v, (size_t)nc * sizeof(Vtx));
    b->cap = nc;
}

static void emit(V3 p, uint32_t c, uint8_t a)
{
    Vtx *v = &cur->v[cur->n++];
    v->x = p.x;
    v->y = p.y;
    v->z = p.z;
    v->r = (c >> 16) & 255;
    v->g = (c >> 8) & 255;
    v->b = c & 255;
    v->a = a;
}

static int g_alpha = 255; /* 255 = lit, 128 = tintable body paint (car generator) */

static void draw_tri(V3 a, V3 b, V3 c, uint32_t col)
{
    vb_reserve(cur, 3);
    emit(a, col, (uint8_t)g_alpha);
    emit(b, col, (uint8_t)g_alpha);
    emit(c, col, (uint8_t)g_alpha);
}

static void draw_quad(V3 a, V3 b, V3 c, V3 d, uint32_t col)
{
    draw_tri(a, b, c, col);
    draw_tri(a, c, d, col);
}

/* per-corner colours, blended across the face */
static void draw_quad_c(V3 a, V3 b, V3 c, V3 d, uint32_t ca, uint32_t cb, uint32_t cc, uint32_t cd)
{
    vb_reserve(cur, 6);
    emit(a, ca, (uint8_t)g_alpha);
    emit(b, cb, (uint8_t)g_alpha);
    emit(c, cc, (uint8_t)g_alpha);
    emit(a, ca, (uint8_t)g_alpha);
    emit(c, cc, (uint8_t)g_alpha);
    emit(d, cd, (uint8_t)g_alpha);
}

static void draw_tri_unlit(V3 a, V3 b, V3 c, uint32_t col)
{
    vb_reserve(cur, 3);
    emit(a, col, 0);
    emit(b, col, 0);
    emit(c, col, 0);
}

static void draw_quad_unlit(V3 a, V3 b, V3 c, V3 d, uint32_t col)
{
    draw_tri_unlit(a, b, c, col);
    draw_tri_unlit(a, c, d, col);
}
