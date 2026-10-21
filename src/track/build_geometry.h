/* track/build_geometry.h - turning a track definition into road segments */
#pragma once

/* the track's cross-sections in driving order from the start line, with the jump edits: x/z in metres, heights in units */
enum { MAXX = MAXPC * 13 + 1, PER = 24 };

static float tx_ex[MAXX][2], tx_ez[MAXX][2];
static int tx_ey[MAXX][2], tx_sec[MAXX];

/* the cross-sections of the pieces round the loop from the start line (the end of the start piece); a piece's last
 * one is the next one's first. Also marks which sections the crane may set a car down on. Returns their number */
static int xsecs_from_pieces(const TrackDef *td)
{
    float (*ex)[2] = tx_ex, (*ez)[2] = tx_ez;
    int (*ey)[2] = tx_ey, *xsec = tx_sec;
    int np = td->np, start = td->start, orig = td->orig;
    int nx = 0;
    nsec = np;

    for (int q = 0; q < np; q++) {
        int p = (start + 1 + q) % np;
        const TPiece *pc = &td->p[p];
        int t = pc->at & 15, ns = SCR_TPL_NS[t];
        sec_ok[p] = t != 1 && t != 3 && t != 6 && t != 7;

        for (int k = 0; orig >= 0 && k < SCR_NNOREST[orig]; k++) {
            if (SCR_NOREST[orig][k] == p) {
                sec_ok[p] = 0;
            }
        }

        for (int i = 0; i < ns; i++) {
            int xz[2][2];
            tpl_cross(pc->at, i, pc->cx, pc->cz, xz);

            for (int e = 0; e < 2; e++) {
                ex[nx][e] = -(xz[e][0] - 8 * 2048) * SCR_U; /* mirrored: the original's world is left-handed */
                ez[nx][e] = (xz[e][1] - 8 * 2048) * SCR_U;
                ey[nx][e] = pc->h[e][i];
            }

            xsec[nx++] = p;
        }
    }

    if (trk_open) { /* the builder's open track: its open end too (start is its last piece) */
        const TPiece *pc = &td->p[start];
        int xz[2][2];
        tpl_cross(pc->at, SCR_TPL_NS[pc->at & 15], pc->cx, pc->cz, xz);

        for (int e = 0; e < 2; e++) {
            ex[nx][e] = -(xz[e][0] - 8 * 2048) * SCR_U;
            ez[nx][e] = (xz[e][1] - 8 * 2048) * SCR_U;
            ey[nx][e] = trk_endh[e];
        }

        xsec[nx++] = start;
    }

    return nx;
}

/* a step between two cross-sections of 640 height units or more is a sheer face */
static int is_sheer_step_down(int k)
{
    return tx_ey[k + 1][0] - tx_ey[k][0] <= -640 || tx_ey[k + 1][1] - tx_ey[k][1] <= -640;
}

/* a pit filled from this section on, up to its far face's top (sloping down by M units to it) */
static void edit_fill(int i0, int nx, int M)
{
    int (*ey)[2] = tx_ey, i;

    for (i = i0; i + 1 < nx && ey[i + 1][0] - ey[i][0] < 640 && ey[i + 1][1] - ey[i][1] < 640; i++) {
    }

    if (i + 2 >= nx) {
        return;
    }

    for (int ed = 0; ed < 2; ed++) {
        int H = ey[i + 2][ed] > ey[i + 1][ed] ? ey[i + 2][ed] : ey[i + 1][ed]; /* a crest just past the face */
        ey[i + 1][ed] = H;

        for (int k = i0; k <= i; k++) {
            ey[k][ed] = H + M * (i + 1 - k) / (i + 1 - i0);
        }
    }
}

/* a ramp's lip raised by M units, from nothing at the section start (the lip may be the next one's first) */
static void edit_lift(int i0, int nx, int M)
{
    int (*ey)[2] = tx_ey, *xsec = tx_sec, i;

    for (i = i0; i + 1 < nx && xsec[i] == xsec[i0] && !is_sheer_step_down(i); i++) {
    }

    for (int k = i0 + 1; k <= i; k++) {
        for (int ed = 0; ed < 2; ed++) {
            ey[k][ed] += M * (k - i0) / (i - i0);
        }
    }
}

/* a step down onto a climb made shallower: the climb raised by M units, tapering out at the crest */
static void edit_step(int i0, int nx, int M)
{
    int (*ey)[2] = tx_ey, i;

    for (i = i0; i + 1 < nx && !is_sheer_step_down(i); i++) {
    }

    int kc = i + 1;

    for (int k = i + 1; k < nx && k < i + 40; k++) {
        if (ey[k][0] + ey[k][1] > ey[kc][0] + ey[kc][1]) {
            kc = k;
        }
    }

    for (int k = i + 1; k < kc; k++) {
        for (int ed = 0; ed < 2; ed++) {
            ey[k][ed] += M * (kc - k) / (kc - i - 1);
        }
    }
}

/* a tower between two pits taken down to their floor: one gap */
static void edit_flatten(int i0, int nx)
{
    int (*ey)[2] = tx_ey, *xsec = tx_sec;

    for (int ed = 0; ed < 2; ed++) {
        int lo = 1 << 30;

        for (int k = i0; k < nx && xsec[k] == xsec[i0]; k++) {
            lo = ey[k][ed] < lo ? ey[k][ed] : lo;
        }

        for (int k = i0; k < nx && xsec[k] == xsec[i0]; k++) {
            ey[k][ed] = lo;
        }
    }
}

/* the jump edits of an original track (TRACK_EDIT), each in metres */
static void apply_jump_edits(int orig, int nx)
{
    const float U = 0.25f * SCR_U; /* metres per height unit */

    for (int e = 0; e < (int)(sizeof TRACK_EDIT / sizeof TRACK_EDIT[0]); e++) {
        if (TRACK_EDIT[e].track != orig) {
            continue;
        }

        int i0 = 0, M = (int)(TRACK_EDIT[e].m / U);

        while (i0 < nx && tx_sec[i0] != TRACK_EDIT[e].sec) {
            i0++;
        }

        switch (TRACK_EDIT[e].kind) {
            case ED_FILL:
                edit_fill(i0, nx, M);
                break;
            case ED_LIFT:
                edit_lift(i0, nx, M);
                break;
            case ED_STEP:
                edit_step(i0, nx, M);
                break;
            case ED_FLAT:
                edit_flatten(i0, nx);
                break;
        }
    }
}

/* the flight from a lip at speed vd along a slope th, with the car's drag: tj[x] is the height at x metres on */
static void landing_flight(float vd, float th, float *tj)
{
    enum { N = 700 };

    float px = 0, py = 0, vx = vd / sqrtf(1 + th * th), vy = vx * th;
    int n = 0;

    for (int it = 0; it < 6000 && n < N; it++) {
        float sp = hypotf(vx, vy), dt = 0.005f;
        vx -= phys.drag / phys.mass * sp * vx * dt;
        vy -= (GRAV + phys.drag / phys.mass * sp * vy) * dt;
        px += vx * dt;
        py += vy * dt;

        while (n < N && n <= px) {
            tj[n++] = py;
        }
    }

    while (n < N) {
        tj[n] = n ? tj[n - 1] - 10.f : py;
        n++;
    }
}

/* a drop too deep to land flat after at our speeds becomes a landing hill (TRACK_HILL[e]): a sheer step of c metres at
 * the lip, then a knoll the car follows up to vd m/s, steepening to a straight slope smax and curving out (radius ~R)
 * onto the floor */
static void apply_landing_hill(int e, int nx)
{
    float (*ex)[2] = tx_ex, (*ez)[2] = tx_ez;
    int (*ey)[2] = tx_ey, *xsec = tx_sec;
    const float U = 0.25f * SCR_U;
    int i0 = 0;

    while (i0 < nx && xsec[i0] != TRACK_HILL[e].sec) {
        i0++;
    }

    int i = i0;

    while (i + 1 < nx && ey[i + 1][0] - ey[i][0] > -640 && ey[i + 1][1] - ey[i][1] > -640) {
        i++;
    }

    const float vd = TRACK_HILL[e].vd, sm = TRACK_HILL[e].smax, c = TRACK_HILL[e].c,
                D = TRACK_HILL[e].R * sm * sm * 0.5f;
    float H = (ey[i][0] + ey[i][1]) * 0.5f * U, fl = 1e9f, d = 0;
    int kf = i + 1; /* the hill starts after the lip, or across a pit */

    if (TRACK_HILL[e].from >= 0) {
        while (kf < nx && xsec[kf] != TRACK_HILL[e].from) {
            kf++;
        }
    }

    for (int k = kf; k < nx && k < kf + 80; k++) { /* the floor: where the track first stops going down */
        float y = (ey[k][0] + ey[k][1]) * 0.5f * U;

        if (y > fl + 0.5f) {
            break;
        }

        fl = fminf(fl, y);
    }

    float th = ((ey[i][0] + ey[i][1]) - (ey[i - 1][0] + ey[i - 1][1])) * 0.5f * U /
               hypotf((ex[i][0] + ex[i][1] - ex[i - 1][0] - ex[i - 1][1]) * 0.5f,
                      (ez[i][0] + ez[i][1] - ez[i - 1][0] - ez[i - 1][1]) * 0.5f);
    static float tj[700];
    landing_flight(vd, th, tj);
    float xk = 699, yk;

    for (int x = 1; x < 699; x++) {
        if (tj[x + 1] - tj[x] < -sm) {
            xk = (float)x;
            break;
        }
    }

    yk = tj[(int)xk] - c;

    for (int k = i + 1; k < nx; k++) {
        d += hypotf((ex[k][0] + ex[k][1] - ex[k - 1][0] - ex[k - 1][1]) * 0.5f,
                    (ez[k][0] + ez[k][1] - ez[k - 1][0] - ez[k - 1][1]) * 0.5f);
        float y = d < xk ? tj[(int)d] + (tj[(int)d + 1] - tj[(int)d]) * (d - (int)d) - c : yk - sm * (d - xk),
              r = y - (fl - H);
        y = r > D ? y : r > -D ? fl - H + (r + D) * (r + D) / (4 * D) : fl - H; /* smooth max onto the floor */
        float yo = (ey[k][0] + ey[k][1]) * 0.5f * U - H;

        if (r < -D && yo <= fl - H + 0.05f) {
            break; /* down: the rest is the original's */
        }

        if (k < kf) {
            continue;
        }

        for (int ed = 0; ed < 2; ed++) {
            ey[k][ed] = ey[i][ed] + (int)(y / U);
        }
    }
}

/* the track's cross-sections in driving order from the start line, with the jump edits */
static int track_xsecs(const TrackDef *td)
{
    int orig = td->orig;
    int nx = xsecs_from_pieces(td);
    apply_jump_edits(orig, nx);

    for (int e = 0; e < (int)(sizeof TRACK_HILL / sizeof TRACK_HILL[0]); e++) {
        if (TRACK_HILL[e].track == orig) {
            apply_landing_hill(e, nx);
        }
    }

    return nx;
}

/* the smooth centre line through the cross-sections: points every 1/PER of a piece section (the "centre line") with
 * heights, banking, and whether it is a sheer face or the open track's gap */
static V3 cl_pos[MAXX * PER];
static float cl_height[MAXX * PER], cl_bank[MAXX * PER], cl_distance[MAXX * PER + 1];
static unsigned char cl_steep[MAXX * PER], cl_gap[MAXX * PER];
static short cl_section[MAXX * PER];

/* per segment, before the final shaping */
static float seg_bank[MAXSEG];
static unsigned char seg_steep_raw[MAXSEG], seg_gap_raw[MAXSEG];

/* the centre line: centripetal Catmull-Rom through the cross-section centres, heights linear between cross-sections.
 * right_edge is which template edge is on the right; lift raises roads that would sit at ground level. Returns the
 * number of centre-line points */
static int plan_centre_line(int nx, int right_edge, float lift)
{
    float (*ex)[2] = tx_ex, (*ez)[2] = tx_ez;
    int (*ey)[2] = tx_ey, *xsec = tx_sec;
    int re = right_edge, M = 0;

    for (int i = 0; i < nx; i++) {
        V3 P[4];

        for (int q = 0; q < 4; q++) {
            int k = i - 1 + q;
            k = trk_open ? (k < 0     ? 0
                            : k >= nx ? nx - 1
                                      : k)
                         : (k + nx) % nx; /* an open track's ends: no neighbour round the gap */
            P[q] = V((ex[k][0] + ex[k][1]) * 0.5f, 0, (ez[k][0] + ez[k][1]) * 0.5f);
        }

        float t[4] = {0};

        for (int q = 1; q < 4; q++) {
            t[q] = t[q - 1] + sqrtf(fmaxf(vlen(vsub(P[q], P[q - 1])), 1e-3f));
        }

        int j = (i + 1) % nx;
        float yl0 = ey[i][1 - re], yr0 = ey[i][re], yl1 = ey[j][1 - re], yr1 = ey[j][re];
        int steep = fabsf(yl1 - yl0) >= 640 || fabsf(yr1 - yr0) >= 640;

        for (int k = 0; k < PER; k++) {
            float f = (float)k / PER, tt = t[1] + (t[2] - t[1]) * f;
            V3 A1 = vlerp(P[0], P[1], (tt - t[0]) / (t[1] - t[0])), A2 = vlerp(P[1], P[2], (tt - t[1]) / (t[2] - t[1]));
            V3 A3 = vlerp(P[2], P[3], (tt - t[2]) / (t[3] - t[2]));
            V3 B1 = vlerp(A1, A2, (tt - t[0]) / (t[2] - t[0])), B2 = vlerp(A2, A3, (tt - t[1]) / (t[3] - t[1]));
            cl_pos[M] = vlerp(B1, B2, (tt - t[1]) / (t[2] - t[1]));
            /* a sheer face becomes a vertical step at the upper deck's end: the lip and the gap stay the original's */
            float fh = !steep                    ? f
                       : (yl1 + yr1 < yl0 + yr0) ? clampf(f / 0.08f, 0, 1)
                                                 : clampf((f - 0.92f) / 0.08f, 0, 1);
            float yl = yl0 + (yl1 - yl0) * fh, yr = yr0 + (yr1 - yr0) * fh;
            cl_height[M] = (yl + yr) * 0.5f * 0.25f * SCR_U + lift;
            cl_bank[M] = (yl - yr) * 0.25f * SCR_U; /* left edge minus right edge */
            cl_steep[M] = steep;
            cl_section[M] = (short)xsec[i];
            cl_gap[M] = trk_open && i == nx - 1; /* the open track's gap */
            M++;
        }
    }

    return M;
}

/* evenly spaced road segments (about 1 m) along the centre line: position, banking, and the section each belongs to;
 * sets nseg and ds */
static void resample_segments(int M)
{
    cl_distance[0] = 0;

    for (int i = 0; i < M; i++) {
        cl_distance[i + 1] = cl_distance[i] + vlen(vsub(cl_pos[(i + 1) % M], cl_pos[i]));
    }

    float total = cl_distance[M];
    nseg = (int)(total / 1.0f + 0.5f);

    if (nseg > MAXSEG) {
        nseg = MAXSEG;
    }

    ds = total / nseg;
    int i = 0;

    for (int k = 0; k < nseg; k++) {
        float tg = k * ds;

        while (i < M - 1 && cl_distance[i + 1] < tg) {
            i++;
        }

        float f = (tg - cl_distance[i]) / (cl_distance[i + 1] - cl_distance[i]);
        int i2 = (i + 1) % M;
        seg[k].c = vlerp(cl_pos[i], cl_pos[i2], f);
        seg[k].c.y = cl_height[i] + (cl_height[i2] - cl_height[i]) * f;
        seg[k].u = tg;
        seg_bank[k] = atan2f(cl_bank[i] + (cl_bank[i2] - cl_bank[i]) * f, 2 * TRACK_HW);
        seg_steep_raw[k] = cl_steep[i];
        seg_sec[k] = cl_section[i];
        seg_gap_raw[k] = cl_gap[i];
    }

    for (int k = 0, k0 = 0; k < nseg; k++) {
        if (k == nseg - 1 || seg_sec[k + 1] != seg_sec[k]) {
            sec_mid[seg_sec[k]] = (k0 + k) / 2;
            k0 = k + 1;
        }
    }
}

/* sharp hollows (the foot of the draw bridge's 30 degree ramps: an 11 m radius) are rounded off to ~80 m, away from
 * sheer faces */
static void round_off_hollows(void)
{
    for (int k = 0; k < nseg; k++) {
        int near = 0;

        for (int o = -6; o <= 6; o++) {
            near |= seg_steep_raw[(k + o + nseg) % nseg];
        }

        if (near) {
            continue;
        }

#define YS(o) seg[((k) + (o) + nseg) % nseg].c.y
        float s1 = (YS(-1) - YS(-4)) / (3 * ds), s2 = (YS(4) - YS(1)) / (3 * ds), d2 = YS(1) - 2 * YS(0) + YS(-1);

        if (s2 - s1 < 0.15f || d2 < YS(2) - 2 * YS(1) + YS(0) || d2 < YS(0) - 2 * YS(-1) + YS(-2)) {
            continue;
        }

        int W = (int)fminf(40.f * (s2 - s1) / ds, 25.f);
        float y0 = YS(-W) + s1 * W * ds; /* where the two straights meet, about here */

        for (int j = -W; j <= W; j++) {
            float y = y0 + s1 * j * ds + (s2 - s1) * sq((j + W) * ds) / (4 * W * ds);
            int q = (k + j + nseg) % nseg;

            if (y > seg[q].c.y) {
                seg[q].c.y = y; /* only ever filled in */
            }
        }
#undef YS
        k += W;
    }
}

/* gap and sheer flags, the tangent of each segment, its curvature (bends) and vertical curvature (crests), and its
 * frame turned by the banking */
static void finish_segment_frames(void)
{
    static float kap[MAXSEG], vk[MAXSEG];
    static V3 Th[MAXSEG];

    for (int k = 0; k < nseg; k++) {
        int kn = (k + 1) % nseg;
        seg[k].gap = seg_gap_raw[k];
        seg[k].steep = (seg_steep_raw[k] || seg_steep_raw[kn]) && fabsf(seg[kn].c.y - seg[k].c.y) > 1.5f;
    }

    for (int k = 0; k < nseg; k++) {
        int kp = (k + nseg - 1) % nseg, kn = (k + 1) % nseg;

        if (!seg[k].steep) {
            if (seg[kn].steep) {
                kn = k;
            }

            if (seg[kp].steep) {
                kp = k;
            }
        }

        seg[k].T = vnorm(vsub(seg[kn].c, seg[kp].c));
        Th[k] = vnorm(V(seg[k].T.x, 0, seg[k].T.z));
    }

    for (int k = 0; k < nseg; k++) {
        V3 Rh = vnorm(vcross(Th[k], UP));
        kap[k] = vdot(vsub(Th[(k + 1) % nseg], Th[(k + nseg - 1) % nseg]), Rh) / (2 * ds);
        vk[k] = -(seg[(k + 1) % nseg].T.y - seg[(k + nseg - 1) % nseg].T.y) / (2 * ds);
    }

    smooth_circ(kap, (int)(120 / ds));
    smooth_circ(vk, (int)(30 / ds));

    for (int k = 0; k < nseg; k++) {
        seg[k].curv = fabsf(kap[k]);
        seg[k].vcurv = vk[k];
        seg[k].bin = sinf(seg_bank[k]) * (kap[k] >= 0 ? 1.f : -1.f);
        V3 R0 = vnorm(vcross(seg[k].T, UP)), N0 = vcross(R0, seg[k].T);
        float cb = cosf(seg_bank[k]), sb = sinf(seg_bank[k]);
        seg[k].R = vsub(vmul(R0, cb), vmul(N0, sb));
        seg[k].N = vadd(vmul(N0, cb), vmul(R0, sb));

        if (seg[k].steep) {
            seg[k].R = R0;
            seg[k].N = UP;
        }
    }
}

/* the extent of the track and the arena round it */
static void compute_bounds(void)
{
    tr_minx = tr_minz = 1e9f;
    tr_maxx = tr_maxz = -1e9f;

    for (int k = 0; k < nseg; k++) {
        tr_minx = fminf(tr_minx, seg[k].c.x);
        tr_maxx = fmaxf(tr_maxx, seg[k].c.x);
        tr_minz = fminf(tr_minz, seg[k].c.z);
        tr_maxz = fmaxf(tr_maxz, seg[k].c.z);
    }

    ar_x0 = tr_minx - 90;
    ar_x1 = tr_maxx + 90;
    ar_z0 = tr_minz - 90;
    ar_z1 = tr_maxz + 90;
}

static void build_track_geometry(int idx)
{
    cur_track = idx;
    track_gen++;
    int nx = track_xsecs(&trk[idx]);
    float (*ex)[2] = tx_ex, (*ez)[2] = tx_ez;
    int (*ey)[2] = tx_ey;
    float ymin = 1e9f;

    for (int i = 0; i < nx; i++) {
        ymin = fminf(ymin, fminf(ey[i][0], ey[i][1]) * 0.25f * SCR_U);
    }

    float lift = fmaxf(0.f, 1.3f - ymin); /* roads at ground level sit on their deck slab */
    tr_lift = lift;
    /* which template edge is on the right */
    V3 d0 = V(ex[1][0] + ex[1][1] - ex[0][0] - ex[0][1], 0, ez[1][0] + ez[1][1] - ez[0][0] - ez[0][1]);
    int right_edge = vdot(vcross(d0, UP), V(ex[0][1] - ex[0][0], 0, ez[0][1] - ez[0][0])) > 0 ? 1 : 0;

    resample_segments(plan_centre_line(nx, right_edge, lift));
    round_off_hollows();
    finish_segment_frames();
    compute_bounds();
}
