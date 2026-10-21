/* editor/heights.h - heights and banking at the joints */
#pragma once

/* ---- heights: joint j is where piece j starts (j = np: the open end) ---- */
static int ed_left_edge(void)
{
    int a[2][2], b[2][2];
    pc_xs(&ed.p[0], 0, a);
    pc_xs(&ed.p[0], 1, b);
    float mx = (a[0][0] + a[1][0]) * 0.5f, mz = (a[0][1] + a[1][1]) * 0.5f;
    float dx = (b[0][0] + b[1][0]) * 0.5f - mx, dz = (b[0][1] + b[1][1]) * 0.5f - mz;
    return ((a[0][0] - mx) * -dz + (a[0][1] - mz) * dx) > 0 ? 0 : 1;
}

static int *joint_ptr(int j, int e)
{
    if (j >= ed.np) {
        return ed_closed() ? &ed.p[0].h[e][0] : &ed_endh[e];
    }

    return &ed.p[j].h[e][0];
}

static void regen(int p)
{
    TPiece *pc = &ed.p[p];

    if (pc->shape == SH_RAW) {
        return;
    }

    int ns = pc_ns(pc);

    for (int e = 0; e < 2; e++) {
        float hs = (float)pc->h[e][0], he = (float)*joint_ptr(p + 1, e), A = pc->amp * U_PER_M;

        for (int i = 1; i < ns; i++) {
            float f = (float)i / ns, g = (float)i / (ns - 1), h;

            switch (pc->shape) {
                case SH_LINEAR:
                    h = hs + (he - hs) * f;
                    break;
                case SH_HUMP:
                    h = hs + (he - hs) * smooth01(f) + A * sinf(PI_F * f);
                    break;
                case SH_KICKER:
                    h = hs + A * g * g;
                    break; /* a lip at the end, the drop to the next piece */
                case SH_STEP:
                    h = he;
                    break; /* a sheer face straight after the start */
                case SH_PIT:
                    h = fminf(hs, he) - A;
                    break;
                default:
                    h = hs + (he - hs) * smooth01(f);
                    break;
            }

            pc->h[e][i] = (int)lroundf(clampf(h, 0, 250 * U_PER_M));
        }
    }
}

static void joint_get(int j, float *c, float *b)
{
    int L = ed_left_edge(), hl = *joint_ptr(j, L), hr = *joint_ptr(j, 1 - L);
    *c = (hl + hr) * 0.5f / U_PER_M;
    *b = (hl - hr) / U_PER_M;
}

static void joint_set(int j, float c, float b)
{
    ed_nbad = 0;
    int L = ed_left_edge(), cl = ed_closed();
    c = clampf(c, fabsf(b) / 2, 250);
    b = clampf(b, -6, 6);
    *joint_ptr(j, L) = (int)lroundf((c + b / 2) * U_PER_M);
    *joint_ptr(j, 1 - L) = (int)lroundf((c - b / 2) * U_PER_M);
    if (j < ed.np) {
        regen(j);
    } else if (cl) {
        regen(0);
    }
    int pj = j - 1 < 0 && cl ? ed.np - 1 : j - 1;

    if (pj >= 0) {
        regen(pj);
    }

    ed_dirty = 1;
}

/* the joints the height and banking keys move: one piece's end, or a run's every joint from its first piece's start to
 * its last one's end (the pieces either side become the ramps) */
static int ed_joints(int *j)
{
    int a = ed_lo(), b = ed_hi(), n = 0;

    if (a == b) {
        j[0] = a + 1;
        return 1;
    }

    for (int k = a; k <= b + 1; k++) {
        if (!(k == ed.np && a == 0 && ed_closed())) {
            j[n++] = k; /* a whole closed loop: its end is its start */
        }
    }

    return n;
}

static void ed_tweak_joints(int kind, float dh, float db)
{
    int jl[MAXPC + 1], nj = ed_joints(jl);
    ed_push(kind);

    for (int k = 0; k < nj; k++) {
        float c, bk;
        joint_get(jl[k], &c, &bk);
        joint_set(jl[k], c + dh, bk + db);
    }
}
