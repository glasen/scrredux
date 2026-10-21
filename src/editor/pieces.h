/* editor/pieces.h - piece joining rules and move search */
#pragma once

static int xs_eq(const int a[2][2], const int b[2][2])
{
    for (int e = 0; e < 2; e++) {
        for (int j = 0; j < 2; j++) {
            if (abs(a[e][j] - b[e][j]) > 8) {
                return 0;
            }
        }
    }
    return 1;
}

static void pc_xs(const TPiece *p, int i, int o[2][2])
{
    tpl_cross(p->at, i, p->cx, p->cz, o);
}

static void pc_endxs(const TPiece *p, int o[2][2])
{
    pc_xs(p, pc_ns(p), o);
}

static float xs_ang(const int e[2][2])
{
    return atan2f((float)(e[1][0] - e[0][0]), (float)-(e[1][1] - e[0][1]));
}

static int ed_closed(void)
{
    if (ed.np < 2) {
        return 0;
    }
    int a[2][2], b[2][2];
    pc_endxs(&ed.p[ed.np - 1], a);
    pc_xs(&ed.p[0], 0, b);
    return xs_eq(a, b);
}

/* every piece that joins on at cross-section e, each shape once */
static int cands(const int e[2][2], TPiece *out)
{
    int n = 0;
    for (int ti = 0; ti < 7; ti++) {
        for (int d = 0; d < 2; d++) {
            for (int r = 0; r < 4; r++) {
                int at = TPL_USE[ti] | (d ? 0x10 : 0) | (r << 6), s[2][2], x1[2][2], x2[2][2], dup = 0;
                tpl_cross(at, 0, 0, 0, s);
                int cx = (int)lroundf((e[0][0] - s[0][0]) / 2048.f), cz = (int)lroundf((e[0][1] - s[0][1]) / 2048.f);
                if (cx < 0 || cx > 15 || cz < 0 || cz > 15) {
                    continue;
                }
                tpl_cross(at, 0, cx, cz, s);
                if (!xs_eq(s, e)) {
                    continue;
                }
                TPiece c;
                memset(&c, 0, sizeof c);
                c.cx = (unsigned char)cx;
                c.cz = (unsigned char)cz;
                c.at = (unsigned char)at;
                c.shape = SH_SMOOTH;
                pc_endxs(&c, x1);
                for (int k = 0; k < n && !dup; k++) {
                    pc_endxs(&out[k], x2);
                    dup = out[k].cx == cx && out[k].cz == cz && xs_eq(x1, x2);
                }
                if (!dup) {
                    out[n++] = c;
                }
            }
        }
    }
    return n;
}

/* a heading the road can carry on straight from (along the grid or diagonally) */
static int stable(const int e[2][2])
{
    TPiece c[64];
    int n = cands(e, c);
    for (int k = 0; k < n; k++) {
        if ((c[k].at & 15) == 0 || (c[k].at & 15) == 10) {
            return 1;
        }
    }
    return 0;
}

/* the cubes a piece's road passes over (-1: off the grid) */
static int pc_cubes(const TPiece *p, int *cb)
{
    int n = 0, ns = pc_ns(p), a[2][2], b[2][2];
    pc_xs(p, 0, a);
    for (int i = 0; i < ns; i++) {
        pc_xs(p, i + 1, b);
        int mx = (a[0][0] + a[1][0] + b[0][0] + b[1][0]) / 4, mz = (a[0][1] + a[1][1] + b[0][1] + b[1][1]) / 4;
        int c = mx < 0 || mz < 0 || mx >= 16 * 2048 || mz >= 16 * 2048 ? -1 : (mz / 2048) * 16 + mx / 2048, dup = 0;
        for (int k = 0; k < n; k++) {
            dup |= cb[k] == c;
        }
        if (!dup) {
            cb[n++] = c;
        }
        memcpy(a, b, sizeof a);
    }
    return n;
}

static int pc_clash(const TPiece *a, const TPiece *b)
{
    int ca[16], cb[16], na = pc_cubes(a, ca), nb = pc_cubes(b, cb);
    for (int i = 0; i < na; i++) {
        for (int j = 0; j < nb; j++) {
            if (ca[i] == cb[j]) {
                return 1;
            }
        }
    }
    return 0;
}

static int pc_ongrid(const TPiece *p)
{
    int c[16], n = pc_cubes(p, c);
    for (int i = 0; i < n; i++) {
        if (c[i] < 0) {
            return 0;
        }
    }
    return 1;
}

/* new piece c, the k-th after the open end: clear of every piece but its neighbours */
static int fits(const TPiece *c, int k, int closes)
{
    if (!pc_ongrid(c)) {
        return 0;
    }
    for (int j = 0; j < ed.np; j++) {
        if ((j == ed.np - 1 && k == 0) || (j == 0 && closes)) {
            continue;
        }
        if (pc_clash(c, &ed.p[j])) {
            return 0;
        }
    }
    return 1;
}

static void move_find(const int e[2][2], Move *m, Move *out, int *nm)
{
    int x[2][2];
    if (m->n) {
        pc_endxs(&m->p[m->n - 1], x);
    } else {
        memcpy(x, e, sizeof x);
    }
    if (m->n && stable(x)) {
        float d = xs_ang(x) - xs_ang(e);
        while (d > PI_F) {
            d -= 2 * PI_F;
        }
        while (d <= -PI_F) {
            d += 2 * PI_F;
        }
        m->turn = (int)lroundf(d * 4 / PI_F) * 45;
        for (int j = 0; j < *nm; j++) {
            if (out[j].turn == m->turn) {
                if (m->n < out[j].n) {
                    out[j] = *m;
                }
                return;
            }
        }
        out[(*nm)++] = *m;
        return;
    }
    if (m->n >= 3) {
        return;
    }
    TPiece c[64];
    int n = cands(x, c);
    for (int k = 0; k < n; k++) {
        m->p[m->n++] = c[k];
        move_find(e, m, out, nm);
        m->n--;
    }
}

/* straight on, 45 and 90 degree bends: the pieces it takes to get to the next heading */
static int moves(const int e[2][2], Move *out)
{
    Move m;
    memset(&m, 0, sizeof m);
    int nm = 0;
    move_find(e, &m, out, &nm);
    return nm;
}

static void open_end(int e[2][2])
{
    pc_endxs(&ed.p[ed.np - 1], e);
}

static int move_ok(const Move *m, int *closes)
{
    int s0[2][2], x[2][2];
    pc_xs(&ed.p[0], 0, s0);
    pc_endxs(&m->p[m->n - 1], x);
    *closes = xs_eq(x, s0);
    if (ed.np + m->n > MAXPC) {
        return 0;
    }
    for (int k = 0; k < m->n; k++) {
        if (!fits(&m->p[k], k, *closes && k == m->n - 1)) {
            return 0;
        }
    }
    return 1;
}
