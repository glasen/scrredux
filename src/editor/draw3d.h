/* editor/draw3d.h - ghosts, selection and joints in the world */
#pragma once

/* ---- drawing in the world: ghosts, the selection, the joints that move ---- */
static void ed_fence(const TPiece *pc, int p, float h0, float h1, uint32_t col)   /* a low fence along both edges */
{
    V3 a[2], b[2];
    piece_world(pc, p, 0, a);
    for (int i = 1; i <= pc_ns(pc); i++, memcpy(a, b, sizeof a)) {
        piece_world(pc, p, i, b);
        for (int e = 0; e < 2; e++) draw_quad_unlit(vadd(a[e], V(0, h0, 0)), vadd(b[e], V(0, h0, 0)), vadd(b[e], V(0, h1, 0)), vadd(a[e], V(0, h1, 0)), col);
    }
}
static void ed_deck(const TPiece *pc, int p, float lift, uint32_t col)   /* a flat copy of the road over it */
{
    V3 a[2], b[2], l = V(0, lift, 0);
    piece_world(pc, p, 0, a);
    for (int i = 1; i <= pc_ns(pc); i++, memcpy(a, b, sizeof a)) {
        piece_world(pc, p, i, b);
        draw_quad_unlit(vadd(a[0], l), vadd(a[1], l), vadd(b[1], l), vadd(b[0], l), col);
    }
}
static void ed_pole(V3 p, float h, uint32_t col)
{
    for (int k = 0; k < 2; k++) {
        V3 s = k ? V(0.35f, 0, 0) : V(0, 0, 0.35f);
        draw_quad_unlit(vsub(p, s), vadd(p, s), vadd(vadd(p, s), V(0, h, 0)), vadd(vsub(p, s), V(0, h, 0)), col);
    }
    V3 t = vadd(p, V(0, h, 0)), r = vmul(cam.right, 1.2f);
    draw_tri_unlit(vadd(t, V(0, 1.8f, 0)), vadd(t, r), vsub(t, r), col);
    draw_tri_unlit(vsub(t, V(0, 1.8f, 0)), vadd(t, r), vsub(t, r), col);
}
static Pick ed_hov;
static void ed_draw3d(void)
{
    if (ed_mode == EM_FLY || !ed.np) return;
    ed_hov = fc_look ? (Pick){0, -1, 0} : ed_pick(ed_hx, ed_hy);
    Move mv[8]; int n = ed_ghosts(mv);
    for (int g = 0; g < n; g++) {
        int hot = ed_hov.kind == 2 && ed_hov.turn == mv[g].turn;
        for (int j = 0; j < mv[g].n; j++) {
            ed_deck(&mv[g].p[j], -1, 0.2f, hot ? 0xF0D060 : 0x78B4E8);
            ed_fence(&mv[g].p[j], -1, 0.2f, hot ? 1.6f : 1.0f, hot ? 0xFFF0A0 : 0x3C78C8);
        }
    }
    for (int k = 0; k < ed_nbad; k++) if (ed_bad[k] < ed.np) ed_fence(&ed.p[ed_bad[k]], ed_bad[k], 0.1f, 1.4f, 0xFF3030);
    if (ed_hov.kind == 1 && (ed_hov.p < ed_lo() || ed_hov.p > ed_hi())) ed_fence(&ed.p[ed_hov.p], ed_hov.p, 0.1f, 0.9f, 0xF0F0F0);
    for (int p = ed_lo(); p <= ed_hi(); p++) ed_fence(&ed.p[p], p, 0.1f, 1.6f, 0x30E0FF);
    int jl[MAXPC + 1], nj = ed_joints(jl);                        /* the joints R/F move: poles at the run's ends */
    for (int k = 0; k < nj; k += nj > 1 ? nj - 1 : 1) ed_pole(joint_world(jl[k]), 7, 0xFFD040);
    if (!ed_closed()) {                                         /* the start line (a closed track has it on the road) */
        V3 o[2]; piece_world(&ed.p[ed.start], ed.start, pc_ns(&ed.p[ed.start]), o);
        V3 s = vmul(vsub(o[1], o[0]), 1.f / 8), f = vmul(vnorm(vcross(vsub(o[1], o[0]), UP)), 1.2f), y = V(0, 0.12f, 0);
        for (int k = 0; k < 8; k++) for (int r = 0; r < 2; r++) {
            V3 a = vadd(vadd(o[0], vmul(s, (float)k)), vadd(vmul(f, (float)r), y));
            draw_quad_unlit(a, vadd(a, s), vadd(vadd(a, s), f), vadd(a, f), (k + r) & 1 ? 0x101010 : 0xF8F8F8);
        }
    }
}
