/* track/build_geometry.h - turning a track definition into road segments */
#pragma once

/* the track's cross-sections in driving order from the start line, with the jump edits: x/z in metres, heights in units */
enum { MAXX = MAXPC * 13 + 1, PER = 24 };

static float tx_ex[MAXX][2], tx_ez[MAXX][2];
static int tx_ey[MAXX][2], tx_sec[MAXX];

static int track_xsecs(const TrackDef *td)
{
    float (*ex)[2] = tx_ex, (*ez)[2] = tx_ez;
    int (*ey)[2] = tx_ey, *xsec = tx_sec;
    int np = td->np, start = td->start, orig = td->orig;
    /* cross-sections round the loop from the start line (the end of the start piece); a piece's last one is the next one's first */
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

    /* the jump edits, m metres each (a sheer face is a step of 640 units or more between cross-sections) */
    const float U = 0.25f * SCR_U;

    for (int e = 0; e < (int)(sizeof TRACK_EDIT / sizeof TRACK_EDIT[0]); e++) {
        if (TRACK_EDIT[e].track != orig) {
            continue;
        }

        int i0 = 0, i, M = (int)(TRACK_EDIT[e].m / U);

        while (i0 < nx && xsec[i0] != TRACK_EDIT[e].sec) {
            i0++;
        }

        switch (TRACK_EDIT[e].kind) {
            case ED_FILL: /* a pit filled from this section on, up to its far face's top (sloping down by m to it) */
                for (i = i0; i + 1 < nx && ey[i + 1][0] - ey[i][0] < 640 && ey[i + 1][1] - ey[i][1] < 640; i++) {
                }

                if (i + 2 >= nx) {
                    break;
                }

                for (int ed = 0; ed < 2; ed++) {
                    int H =
                        ey[i + 2][ed] > ey[i + 1][ed] ? ey[i + 2][ed] : ey[i + 1][ed]; /* a crest just past the face */
                    ey[i + 1][ed] = H;

                    for (int k = i0; k <= i; k++) {
                        ey[k][ed] = H + M * (i + 1 - k) / (i + 1 - i0);
                    }
                }

                break;
            case ED_LIFT: /* a ramp's lip raised by m, from nothing at the section start (the lip may be the next one's first) */
                for (i = i0; i + 1 < nx && xsec[i] == xsec[i0] && ey[i + 1][0] - ey[i][0] > -640 &&
                             ey[i + 1][1] - ey[i][1] > -640;
                     i++) {
                }

                for (int k = i0 + 1; k <= i; k++) {
                    for (int ed = 0; ed < 2; ed++) {
                        ey[k][ed] += M * (k - i0) / (i - i0);
                    }
                }

                break;
            case ED_STEP: { /* a step down onto a climb made shallower: the climb raised by m, tapering out at the crest */
                for (i = i0; i + 1 < nx && ey[i + 1][0] - ey[i][0] > -640 && ey[i + 1][1] - ey[i][1] > -640; i++) {
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

                break;
            }
            case ED_FLAT: /* a tower between two pits taken down to their floor: one gap */
                for (int ed = 0; ed < 2; ed++) {
                    int lo = 1 << 30;

                    for (int k = i0; k < nx && xsec[k] == xsec[i0]; k++) {
                        lo = ey[k][ed] < lo ? ey[k][ed] : lo;
                    }

                    for (int k = i0; k < nx && xsec[k] == xsec[i0]; k++) {
                        ey[k][ed] = lo;
                    }
                }

                break;
        }
    }
    /* a drop too deep to land flat after at our speeds becomes a landing hill: a sheer step of c metres at the lip, then a
     * knoll the car follows up to vd m/s, steepening to a straight slope smax and curving out (radius ~R) onto the floor */
    for (int e = 0; e < (int)(sizeof TRACK_HILL / sizeof TRACK_HILL[0]); e++) {
        if (TRACK_HILL[e].track != orig) {
            continue;
        }

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
        static float tj[700]; /* the flight from the lip at vd, with the car's drag */
        {
            float px = 0, py = 0, vx = vd / sqrtf(1 + th * th), vy = vx * th;
            int n = 0;

            for (int it = 0; it < 6000 && n < 700; it++) {
                float sp = hypotf(vx, vy), dt = 0.005f;
                vx -= ph.drag / ph.mass * sp * vx * dt;
                vy -= (GRAV + ph.drag / ph.mass * sp * vy) * dt;
                px += vx * dt;
                py += vy * dt;

                while (n < 700 && n <= px) {
                    tj[n++] = py;
                }
            }

            while (n < 700) {
                tj[n] = n ? tj[n - 1] - 10.f : py;
                n++;
            }
        }
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

    return nx;
}

static void build_track_geometry(int idx)
{
    const TrackDef *td = &trk[idx];
    int np = td->np, start = td->start, orig = td->orig;
    cur_track = idx;
    track_gen++;
    float (*ex)[2] = tx_ex, (*ez)[2] = tx_ez;
    int (*ey)[2] = tx_ey, *xsec = tx_sec;
    int nx = track_xsecs(td);
    (void)np;
    (void)start;
    (void)orig;
    (void)xsec;
    float ymin = 1e9f;

    for (int i = 0; i < nx; i++) {
        ymin = fminf(ymin, fminf(ey[i][0], ey[i][1]) * 0.25f * SCR_U);
    }

    float lift = fmaxf(0.f, 1.3f - ymin); /* roads at ground level sit on their deck slab */
    tr_lift = lift;
    /* which template edge is on the right */
    V3 d0 = V(ex[1][0] + ex[1][1] - ex[0][0] - ex[0][1], 0, ez[1][0] + ez[1][1] - ez[0][0] - ez[0][1]);
    int re = vdot(vcross(d0, UP), V(ex[0][1] - ex[0][0], 0, ez[0][1] - ez[0][0])) > 0 ? 1 : 0;

    /* plan: centripetal Catmull-Rom through the cross-section centres; heights stay linear between cross-sections */
    static V3 fp[MAXX * PER];
    static float fy[MAXX * PER], fb[MAXX * PER], cum[MAXX * PER + 1];
    static unsigned char fs[MAXX * PER], fw[MAXX * PER];
    static short fsec[MAXX * PER];
    int M = 0;

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
            fp[M] = vlerp(B1, B2, (tt - t[1]) / (t[2] - t[1]));
            /* a sheer face becomes a vertical step at the upper deck's end: the lip and the gap stay the original's */
            float fh = !steep                    ? f
                       : (yl1 + yr1 < yl0 + yr0) ? clampf(f / 0.08f, 0, 1)
                                                 : clampf((f - 0.92f) / 0.08f, 0, 1);
            float yl = yl0 + (yl1 - yl0) * fh, yr = yr0 + (yr1 - yr0) * fh;
            fy[M] = (yl + yr) * 0.5f * 0.25f * SCR_U + lift;
            fb[M] = (yl - yr) * 0.25f * SCR_U; /* left edge minus right edge */
            fs[M] = steep;
            fsec[M] = (short)xsec[i];
            fw[M] = trk_open && i == nx - 1; /* the open track's gap */
            M++;
        }
    }

    cum[0] = 0;

    for (int i = 0; i < M; i++) {
        cum[i + 1] = cum[i] + vlen(vsub(fp[(i + 1) % M], fp[i]));
    }

    float total = cum[M];
    nseg = (int)(total / 1.0f + 0.5f);

    if (nseg > MAXSEG) {
        nseg = MAXSEG;
    }

    ds = total / nseg;
    static float bank[MAXSEG];
    static unsigned char st[MAXSEG], gk[MAXSEG];
    int i = 0;

    for (int k = 0; k < nseg; k++) {
        float tg = k * ds;

        while (i < M - 1 && cum[i + 1] < tg) {
            i++;
        }

        float f = (tg - cum[i]) / (cum[i + 1] - cum[i]);
        int i2 = (i + 1) % M;
        seg[k].c = vlerp(fp[i], fp[i2], f);
        seg[k].c.y = fy[i] + (fy[i2] - fy[i]) * f;
        seg[k].u = tg;
        bank[k] = atan2f(fb[i] + (fb[i2] - fb[i]) * f, 2 * TRACK_HW);
        st[k] = fs[i];
        seg_sec[k] = fsec[i];
        gk[k] = fw[i];
    }

    for (int k = 0, k0 = 0; k < nseg; k++) {
        if (k == nseg - 1 || seg_sec[k + 1] != seg_sec[k]) {
            sec_mid[seg_sec[k]] = (k0 + k) / 2;
            k0 = k + 1;
        }
    }

    /* sharp hollows (the foot of the draw bridge's 30 degree ramps: an 11 m radius) are rounded off to ~80 m, away from sheer faces */
    for (int k = 0; k < nseg; k++) {
        int near = 0;

        for (int o = -6; o <= 6; o++) {
            near |= st[(k + o + nseg) % nseg];
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

    for (int k = 0; k < nseg; k++) {
        int kn = (k + 1) % nseg;
        seg[k].gap = gk[k];
        seg[k].steep = (st[k] || st[kn]) && fabsf(seg[kn].c.y - seg[k].c.y) > 1.5f;
    }

    static float kap[MAXSEG], vk[MAXSEG];
    static V3 Th[MAXSEG];

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
        seg[k].bin = sinf(bank[k]) * (kap[k] >= 0 ? 1.f : -1.f);
        V3 R0 = vnorm(vcross(seg[k].T, UP)), N0 = vcross(R0, seg[k].T);
        float cb = cosf(bank[k]), sb = sinf(bank[k]);
        seg[k].R = vsub(vmul(R0, cb), vmul(N0, sb));
        seg[k].N = vadd(vmul(N0, cb), vmul(R0, sb));

        if (seg[k].steep) {
            seg[k].R = R0;
            seg[k].N = UP;
        }
    }

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
