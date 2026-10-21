/* editor/picking.h - what is under the mouse */
#pragma once

/* ---- the mouse: what is under it ---- */

static float ray_tri(V3 o, V3 d, V3 a, V3 b, V3 c)
{
    V3 e1 = vsub(b, a), e2 = vsub(c, a), p = vcross(d, e2);
    float det = vdot(e1, p);

    if (fabsf(det) < 1e-6f) {
        return -1;
    }

    V3 t = vsub(o, a), q = vcross(t, e1);
    float u = vdot(t, p) / det, v = vdot(d, q) / det, s = vdot(e2, q) / det;
    return u < 0 || v < 0 || u + v > 1 || s <= 0 ? -1 : s;
}

static float ray_piece(V3 o, V3 d, const TPiece *pc, int p)
{
    float best = -1;
    V3 a[2], b[2];
    piece_world(pc, p, 0, a);

    for (int i = 1; i <= pc_ns(pc); i++, memcpy(a, b, sizeof a)) {
        piece_world(pc, p, i, b);
        float t1 = ray_tri(o, d, a[0], a[1], b[1]), t2 = ray_tri(o, d, a[0], b[1], b[0]);

        if (t1 > 0 && (best < 0 || t1 < best)) {
            best = t1;
        }

        if (t2 > 0 && (best < 0 || t2 < best)) {
            best = t2;
        }
    }

    return best;
}

static Pick ed_pick(float hx, float hy)
{
    Pick k = {0, -1, 0};

    if (hx < 0) {
        return k;
    }

    float asp = HW / VH, best = 1e9f;
    V3 o = cam.pos, d = vnorm(vadd(cam.fwd, vadd(vmul(cam.right, (hx / HW * 2 - 1) * asp / FOCAL_Y),
                                                 vmul(cam.up, (1 - hy / VH * 2) / FOCAL_Y))));

    for (int p = 0; p < editor.track.np; p++) {
        float t = ray_piece(o, d, &editor.track.p[p], p);

        if (t > 0 && t < best) {
            best = t;
            k.kind = 1;
            k.p = p;
        }
    }

    Move mv[8];
    int n = ed_ghosts(mv);

    for (int g = 0; g < n; g++) {
        for (int j = 0; j < mv[g].n; j++) {
            float t = ray_piece(o, d, &mv[g].p[j], -1);

            if (t > 0 && t < best) {
                best = t;
                k.kind = 2;
                k.turn = mv[g].turn;
            }
        }
    }

    return k;
}
