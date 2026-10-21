/* editor/world.h - pieces in the world */
#pragma once

/* ---- the pieces in the world ---- */
static float ed_wy(int h)
{
    return h * 0.25f * SCR_U + tr_lift;
}

/* cross-section i of a piece, both edges, in metres; p < 0: a ghost at the open end's height */
static void piece_world(const TPiece *pc, int p, int i, V3 o[2])
{
    int a[2][2];
    pc_xs(pc, i, a);

    for (int e = 0; e < 2; e++) {
        int h = p < 0 ? ed_endh[e] : i < pc_ns(pc) ? pc->h[e][i] : *joint_ptr(p + 1, e);
        o[e] = V(-(a[e][0] - 8 * 2048) * SCR_U, ed_wy(h), (a[e][1] - 8 * 2048) * SCR_U);
    }
}

static V3 piece_mid(const TPiece *pc, int p)
{
    V3 o[2];
    piece_world(pc, p, pc_ns(pc) / 2, o);
    return vmul(vadd(o[0], o[1]), 0.5f);
}

static V3 joint_world(int j)
{
    V3 o[2];

    if (j < ed.np) {
        piece_world(&ed.p[j], j, 0, o);
    } else {
        piece_world(&ed.p[ed.np - 1], ed.np - 1, pc_ns(&ed.p[ed.np - 1]), o);
    }
    return vmul(vadd(o[0], o[1]), 0.5f);
}

/* the ghosts: what can join on at the open end */
static int ed_ghosts(Move *mv)
{
    if (!ed.np || ed_closed()) {
        return 0;
    }

    int e[2][2], cl, n = 0;
    open_end(e);
    Move all[8];
    int na = moves(e, all);

    for (int k = 0; k < na; k++) {
        if (move_ok(&all[k], &cl)) {
            mv[n++] = all[k];
        }
    }

    return n;
}
