/* car/model_pose.h - posing the independent suspension and steering */
#pragma once

/* ---- posing ---- */
typedef struct { float wheel[12], up[12], la[12], ua[12], cb[12], ct[12], sp[12], link[2][12]; } CornerPose;

static void corner_pose(const Car *c, int i, const float body[12], CornerPose *P)
{
    const Rig *g = &rig[i];
    /* the visual body lean lifts or drops the mount; the wheel stays on the road, so the suspension takes it up */
    V3 lm = m34_apply(body, WMOUNT[i]), pm = body_pt(c, WMOUNT[i]);
    float dz = clampf(WREST[i] - c->wl[i] - vdot(c->U, vsub(lm, pm)), -WDROOP[i], WBUMP[i]);
    const float *t = g->tab; int lo = 0, hi = g->ntab - 1;
    while (hi - lo > 1) { int mid = (lo + hi) / 2; if (t[mid * 3] <= dz) lo = mid; else hi = mid; }
    float f = t[hi * 3] > t[lo * 3] ? clampf((dz - t[lo * 3]) / (t[hi * 3] - t[lo * 3]), 0, 1) : 0;
    float a = t[lo * 3 + 1] + (t[hi * 3 + 1] - t[lo * 3 + 1]) * f, b = t[lo * 3 + 2] + (t[hi * 3 + 2] - t[lo * 3 + 2]) * f;
    m34_rot(g->lb0, vsub(g->lb1, g->lb0), a, P->la);
    m34_rot(g->ub0, vsub(g->ub1, g->ub0), b, P->ua);
    V3 L = m34_apply(P->la, g->L0), U = m34_apply(P->ua, g->U0);
    m34_aim(g->L0, vsub(g->U0, g->L0), vsub(U, L), 0, P->up);                 /* upright follows its two ball joints */
    m34_shift(P->up, vsub(L, g->L0));
    if (i < 2) { float st[12]; m34_rot(KP_P[i], KP_A[i], c->delta, st); m34_mul(P->up, st, P->up); }   /* steer about the kingpin */
    float sp[12]; m34_rot(HUB0[i], V(1, 0, 0), c->wph[i], sp); m34_mul(P->up, sp, P->wheel);
    V3 low = m34_apply(P->la, g->elo), d0 = vsub(g->elo, g->ehi), d1 = vsub(low, g->ehi);
    m34_aim(g->ehi, d0, d1, 0, P->ct);                                       /* damper body swings about its top eye */
    memcpy(P->cb, P->ct, sizeof P->ct); m34_shift(P->cb, vsub(low, m34_apply(P->ct, g->elo)));   /* shaft rides on the arm */
    m34_aim(g->ehi, d0, d1, 1, P->sp);                                       /* spring stretches between the two */
    for (int k = 0; k < g->nlink; k++) {
        V3 o = m34_apply(P->up, g->lout[k]);
        m34_aim(g->lin[k], vsub(g->lout[k], g->lin[k]), vsub(o, g->lin[k]), 1, P->link[k]);
    }
}

/* world rows of model group g on car c; 0 = do not draw */
static int car_group_rows(const Car *c, int g, float m[12], int first_person, int hide)
{
    const MGroup *G = &mdl_g[g];
    float body[12]; car_body_rows(c, body);
    switch (G->role) {
    case R_BODY: memcpy(m, body, sizeof body); return 1;
    case R_ENGTOP: if (hide & 4) return 0; memcpy(m, body, sizeof body); return 1;
    case R_ROOF: if (hide & 1) return 0; memcpy(m, body, sizeof body); return 1;
    case R_DRIVER: if (first_person) return 0; memcpy(m, body, sizeof body); return 1;
    case R_STWHL: { float l[12]; m34_rot(STW_C, STW_A, -STW_RATIO * c->delta, l); m34_mul(body, l, m); return 1; }
    }
    if (G->corner < 0) return 0;
    CornerPose P; corner_pose(c, G->corner, body, &P);
    const float *l;
    switch (G->role) {
    case R_WHEEL: l = P.wheel; break;
    case R_UPRIGHT: l = P.up; break;
    case R_LARM: l = P.la; break;
    case R_UARM: l = P.ua; break;
    case R_CBOT: l = P.cb; break;
    case R_CTOP: l = P.ct; break;
    case R_SPRING: l = P.sp; break;
    case R_LINK: if (G->link < 0 || G->link >= rig[G->corner].nlink) return 0; l = P.link[G->link]; break;
    default: return 0;
    }
    m34_mul(body, l, m);
    return 1;
}
