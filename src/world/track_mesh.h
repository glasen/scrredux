/* world/track_mesh.h - deck mesh and support towers */
#pragma once

/* ------------------------------------------------- static world geometry */
static inline V3 rp(int i, float d, float h)
{
    const Seg *s = &seg[((i % nseg) + nseg) % nseg];
    return vadd(s->c, vadd(vmul(s->R, d), vmul(s->N, h)));
}

static float strip_off(int k)
{
    if (k <= 0) {
        return -TRACK_HW;
    }

    if (k >= NSTR) {
        return TRACK_HW;
    }

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

    if (e < 1.2f) {
        s *= 0.93f + 0.07f * e / 1.2f;
    }

    return scalecol(0x9E998E, s);
}

static uint32_t edge_col(int i, float d)
{
    return scalecol(0xECEBE4, 0.9f + 0.1f * vnoise(i * ds / 3.f, d, 4));
}

static int is_edge(int k)
{
    return k == 0 || k == NSTR - 1;
}

static uint32_t start_col(int i, int k)
{
    return (((k - 1) / 2 + i) & 1) ? 0xF2F2F2 : 0x161616;
}

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

    if (s->gap) {
        return;
    }

    const float hw = TRACK_HW, ow = TRACK_HW;
    V3 a[NSTR + 1], b[NSTR + 1];
    uint32_t ca[NSTR + 1], cb[NSTR + 1];

    for (int k = 0; k <= NSTR; k++) {
        a[k] = rp(i, strip_off(k), 0);
        b[k] = rp(j, strip_off(k), 0);
        ca[k] = deck_col(i, strip_off(k));
        cb[k] = deck_col(i + 1, strip_off(k));
    }

    /* a slab joint (dark gap with a steel strip) at the start of every slab */
    float tj = (i % SLAB == 0) ? 0.10f : 0.f;

    for (int k = 0; k < NSTR; k++) {
        V3 a0 = a[k], a1 = a[k + 1], b0 = b[k], b1 = b[k + 1];

        if (s->steep) { /* sheer step: a red and white end panel like the side skirts */
            uint32_t col = is_edge(k) ? 0x6A6A72 : (((k - 1) / 4) & 1) ? COL_RED : 0xE8E8E8;
            draw_quad(a0, a1, b1, b0, col);
            draw_quad(a0, b0, b1, a1, col);
            continue;
        }

        if (i < 4 && !trk_open && !is_edge(k)) {
            draw_quad(a0, a1, b1, b0, start_col(i, k));
            continue;
        } /* an open track's first segments are not its start */
        if (tj > 0) {
            V3 m0 = vlerp(a0, b0, tj), m1 = vlerp(a1, b1, tj);
            draw_quad(a0, a1, m1, m0, is_edge(k) ? 0x9A9A96 : 0x6C6964);
            a0 = m0;
            a1 = m1;
        }

        if (is_edge(k)) {
            float d0 = strip_off(k), d1 = strip_off(k + 1);
            draw_quad_c(a0, a1, b1, b0, edge_col(i, d0), edge_col(i, d1), edge_col(i + 1, d1), edge_col(i + 1, d0));
        } else {
            float f = patch_f(i, k);
            draw_quad_c(a0, a1, b1, b0, scalecol(ca[k], f), scalecol(ca[k + 1], f), scalecol(cb[k + 1], f),
                        scalecol(cb[k], f));
        }
    }

    if (s->steep) {
        return;
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
    if (i % 3 == 0 && !gn && !gp && !s->steep) {
        int j2 = (i + 3) % nseg;

        for (int sd = -1; sd <= 1; sd += 2) {
            V3 t0 = rp(i, sd * (hw + 0.2f), -1.2f);
            V3 b0 = rp(i, sd * (hw - 0.8f), -2.6f), b1 = rp(j2, sd * (hw - 0.8f), -2.6f);
            V3 t1 = rp(j2, sd * (hw + 0.2f), -1.2f);
            cyl(b0, b1, 0.10f, 0.10f, 6, 0x6C6C78, 0);
            cyl(t0, b0, 0.06f, 0.06f, 5, 0x888894, 0);

            if ((i / 3) & 1) {
                cyl(t0, b1, 0.05f, 0.05f, 5, 0x888894, 0);
            } else {
                cyl(b0, t1, 0.05f, 0.05f, 5, 0x888894, 0);
            }
        }
    }
}

static void build_tower(int i)
{
    const Seg *s = &seg[i];
    float top = s->c.y - 1.4f;

    if (top < 3.0f) {
        return;
    }

    float tk[4]; /* column tops follow the real underside of the deck (bank, slope) */
    {
        static const float qx[4] = {-1, 1, 1, -1}, qz[4] = {-1, -1, 1, 1};

        for (int k = 0; k < 4; k++) {
            tk[k] = s->c.y + s->R.y * qx[k] * (TRACK_HW - 1.6f) + s->T.y * qz[k] * 1.4f - 1.2f * s->N.y - 0.95f;
        }
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

    for (int k = 0; k < 4; k++) {
        cyl(botc[k], topc[k], 0.28f, 0.2f, 8, 0xC8C8D0, 3);
    }

    int levels = (int)(top / 4.0f) + 1;

    for (int l = 0; l <= levels; l++) {
        float t0 = (float)l / levels, t1 = (float)(l + 1) / levels;
        V3 p0[4], p1[4];

        for (int k = 0; k < 4; k++) {
            p0[k] = vlerp(botc[k], topc[k], t0);
            p1[k] = vlerp(botc[k], topc[k], t1);
        }

        uint32_t bc = (l & 1) ? COL_RED : 0xE8E8E8;

        for (int k = 0; k < 4; k++) {
            int k2 = (k + 1) % 4;
            cyl(p0[k], p0[k2], 0.1f, 0.1f, 6, bc, 0);

            if (l < levels) {
                cyl((l & 1) ? p0[k] : p0[k2], (l & 1) ? p1[k2] : p1[k], 0.07f, 0.07f, 5, 0x9C9CA8, 0);
            }
        }
    }

    cyl(vadd(topc[0], V(0, 0.3f, 0)), vadd(topc[1], V(0, 0.3f, 0)), 0.2f, 0.2f, 6, 0x6E6E7A, 3);
    cyl(vadd(topc[3], V(0, 0.3f, 0)), vadd(topc[2], V(0, 0.3f, 0)), 0.2f, 0.2f, 6, 0x6E6E7A, 3);
}
