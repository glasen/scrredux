/* car_model.h - the stunt buggy, loaded from models/stuntcar.scm (written by export_game_model.py in
 * ../stunt_car_racer from the Blender model), and the posing of its independent suspension.
 * Included by sr_main.h. The game does not start without the file.
 *
 * File (little endian): "SCRCAR1\0", u32 version (1 or 2), f32 rest_h, place_h, eye[3], cpts[16][3], flames[8][3],
 * (version 2: flame directions[8][3]),
 * steering wheel centre[3], axis[3], ratio; 4 x wheel {mount[3], radius, lmin, lf, rest, bump, droop,
 * kingpin point[3], kingpin axis[3], hub[3]}; 4 x rig {lower pivots 2x[3], upper pivots 2x[3], lower/upper
 * ball joint[3], coilover lower/upper eye[3], u32 nlinks, nlinks x {inner[3], outer[3]}, u32 n, n x {dz, a, b}};
 * u32 ngroups, ngroups x {char name[8], u32 role, i32 corner, i32 link, u32 first, u32 count};
 * u32 nverts, nverts x Vtx. All in the car frame (+x right, +y up, +z forward, origin = centre of mass).
 */

enum { R_BODY, R_ENGTOP, R_ROOF, R_DRIVER, R_STWHL, R_WHEEL, R_UPRIGHT, R_LARM, R_UARM, R_CBOT, R_CTOP, R_SPRING,
       R_LINK };

typedef struct {
    V3 lb0, lb1, ub0, ub1, L0, U0, elo, ehi;       /* wishbone pivot axes, ball joints, coilover eyes at ride height */
    int nlink; V3 lin[2], lout[2];                  /* links from a fixed inner point to a point on the upright */
    int ntab; float *tab;                           /* wheel travel dz -> lower / upper wishbone angle, sorted by dz */
} Rig;
static Rig rig[4];
static float WDROOP[4];
static V3 KP_P[4], KP_A[4], HUB0[4];
static V3 STW_C, STW_A; static float STW_RATIO;

/* ---- 3x4 affine matrices, rows like car_body_rows() ---- */
static V3 m34_apply(const float m[12], V3 p)
{
    return V(m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3], m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7],
             m[8] * p.x + m[9] * p.y + m[10] * p.z + m[11]);
}
static void m34_mul(const float a[12], const float b[12], float o[12])   /* o = a after b */
{
    float r[12];
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 4; j++)
            r[i * 4 + j] = a[i * 4] * b[j] + a[i * 4 + 1] * b[4 + j] + a[i * 4 + 2] * b[8 + j] + (j == 3 ? a[i * 4 + 3] : 0);
    memcpy(o, r, sizeof r);
}
static void m34_set3(float m[12], const float R[9], V3 p)   /* linear part R about the fixed point p */
{
    for (int i = 0; i < 3; i++) { m[i * 4] = R[i * 3]; m[i * 4 + 1] = R[i * 3 + 1]; m[i * 4 + 2] = R[i * 3 + 2]; }
    V3 q = V(R[0] * p.x + R[1] * p.y + R[2] * p.z, R[3] * p.x + R[4] * p.y + R[5] * p.z, R[6] * p.x + R[7] * p.y + R[8] * p.z);
    m[3] = p.x - q.x; m[7] = p.y - q.y; m[11] = p.z - q.z;
}
static void rot3(V3 ax, float an, float R[9])
{
    ax = vnorm(ax);
    float c = cosf(an), s = sinf(an), t = 1 - c;
    float r[9] = {t * ax.x * ax.x + c, t * ax.x * ax.y - s * ax.z, t * ax.x * ax.z + s * ax.y,
                  t * ax.x * ax.y + s * ax.z, t * ax.y * ax.y + c, t * ax.y * ax.z - s * ax.x,
                  t * ax.x * ax.z - s * ax.y, t * ax.y * ax.z + s * ax.x, t * ax.z * ax.z + c};
    memcpy(R, r, sizeof r);
}
static void m34_rot(V3 p, V3 ax, float an, float m[12]) { float R[9]; rot3(ax, an, R); m34_set3(m, R, p); }
/* about p: turn direction d0 onto d1; with stretch also scale along d0 so the far end lands on p + d1 */
static void m34_aim(V3 p, V3 d0, V3 d1, int stretch, float m[12])
{
    V3 a = vnorm(d0), b = vnorm(d1), ax = vcross(a, b);
    float s = vlen(ax), R[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    if (s > 1e-7f) rot3(ax, atan2f(s, vdot(a, b)), R);
    if (stretch) {
        float k = vlen(d1) / fmaxf(vlen(d0), 1e-6f) - 1, S[9], RS[9], av[3] = {a.x, a.y, a.z};
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) S[i * 3 + j] = (i == j) + k * av[i] * av[j];
        for (int i = 0; i < 3; i++) for (int j = 0; j < 3; j++) RS[i * 3 + j] = R[i * 3] * S[j] + R[i * 3 + 1] * S[3 + j] + R[i * 3 + 2] * S[6 + j];
        memcpy(R, RS, sizeof R);
    }
    m34_set3(m, R, p);
}
static void m34_shift(float m[12], V3 d) { m[3] += d.x; m[7] += d.y; m[11] += d.z; }

/* ---- loading ---- */
typedef struct { const uint8_t *p, *end; int bad; } Rd;
static uint32_t rd_u32(Rd *r) { uint32_t v = 0; if (r->p + 4 > r->end) { r->bad = 1; return 0; } memcpy(&v, r->p, 4); r->p += 4; return v; }
static float rd_f(Rd *r) { float v = 0; if (r->p + 4 > r->end) { r->bad = 1; return 0; } memcpy(&v, r->p, 4); r->p += 4; return v; }
static V3 rd_v3(Rd *r) { float x = rd_f(r), y = rd_f(r), z = rd_f(r); return V(x, y, z); }

static int car_model_load(void)
{
    char path[1024]; size_t sz = 0;
    const char *base = SDL_GetBasePath();
    snprintf(path, sizeof path, "%smodels/stuntcar.scm", base ? base : "");
    uint8_t *buf = SDL_LoadFile(path, &sz);
    if (!buf) buf = SDL_LoadFile("models/stuntcar.scm", &sz);
    if (!buf) { fprintf(stderr, "models/stuntcar.scm not found\n"); return 0; }
    Rd r = {buf, buf + sz, 0};
    uint32_t ver = 0;
    if (sz < 12 || memcmp(buf, "SCRCAR1", 8) || (r.p += 8, ver = rd_u32(&r)) < 1 || ver > 2) { fprintf(stderr, "stuntcar.scm: bad header\n"); SDL_free(buf); return 0; }
    REST_H = rd_f(&r); PLACE_H = rd_f(&r);
    EYE = rd_v3(&r);
    for (int i = 0; i < 16; i++) CPTS[i] = rd_v3(&r);
    for (int i = 0; i < 8; i++) FLAMEP[i] = rd_v3(&r);
    if (ver >= 2) for (int i = 0; i < 8; i++) FLAMED[i] = vnorm(rd_v3(&r));
    STW_C = rd_v3(&r); STW_A = rd_v3(&r); STW_RATIO = rd_f(&r);
    for (int i = 0; i < 4; i++) {
        WMOUNT[i] = rd_v3(&r); WRAD[i] = rd_f(&r); WLMIN[i] = rd_f(&r); WLF[i] = rd_f(&r); WREST[i] = rd_f(&r);
        WBUMP[i] = rd_f(&r); WDROOP[i] = rd_f(&r);
        KP_P[i] = rd_v3(&r); KP_A[i] = rd_v3(&r); HUB0[i] = rd_v3(&r);
    }
    for (int i = 0; i < 4 && !r.bad; i++) {
        Rig *g = &rig[i];
        g->lb0 = rd_v3(&r); g->lb1 = rd_v3(&r); g->ub0 = rd_v3(&r); g->ub1 = rd_v3(&r);
        g->L0 = rd_v3(&r); g->U0 = rd_v3(&r); g->elo = rd_v3(&r); g->ehi = rd_v3(&r);
        g->nlink = (int)rd_u32(&r); if (g->nlink > 2) { r.bad = 1; break; }
        for (int k = 0; k < g->nlink; k++) { g->lin[k] = rd_v3(&r); g->lout[k] = rd_v3(&r); }
        g->ntab = (int)rd_u32(&r);
        if (g->ntab < 2 || g->ntab > 10000) { r.bad = 1; break; }
        g->tab = malloc((size_t)g->ntab * 3 * sizeof(float));
        for (int k = 0; k < g->ntab * 3; k++) g->tab[k] = rd_f(&r);
    }
    uint32_t ng = rd_u32(&r);
    if (r.bad || ng > (uint32_t)(sizeof mdl_g / sizeof mdl_g[0])) { fprintf(stderr, "stuntcar.scm: bad rig\n"); SDL_free(buf); return 0; }
    for (uint32_t i = 0; i < ng; i++) {
        if (r.p + 8 > r.end) { r.bad = 1; break; }
        memcpy(mdl_g[i].name, r.p, 8); mdl_g[i].name[7] = 0; r.p += 8;
        mdl_g[i].role = (int)rd_u32(&r); mdl_g[i].corner = (int)rd_u32(&r); mdl_g[i].link = (int)rd_u32(&r);
        mdl_g[i].first = rd_u32(&r); mdl_g[i].count = rd_u32(&r);
    }
    uint32_t nv = rd_u32(&r);
    if (r.bad || r.p + (size_t)nv * sizeof(Vtx) > r.end) { fprintf(stderr, "stuntcar.scm: truncated\n"); SDL_free(buf); return 0; }
    for (uint32_t i = 0; i < ng; i++)
        if (mdl_g[i].first + mdl_g[i].count > nv || mdl_g[i].corner > 3 || mdl_g[i].link > 1) { fprintf(stderr, "stuntcar.scm: bad group\n"); SDL_free(buf); return 0; }
    mdl_v = malloc((size_t)nv * sizeof(Vtx));
    memcpy(mdl_v, r.p, (size_t)nv * sizeof(Vtx));
    mdl_nv = (int)nv; mdl_ng = (int)ng;
    SDL_free(buf);
    return 1;
}

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
