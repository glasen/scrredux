/* ai/jump_planner.h - AI jump planning: a scratch car tries each jump with the real physics */
#pragma once

/* ---- AI jump planning: a scratch car tries each jump with the real physics, so bounces off a far face count too ---- */
static float bend_hold(int k)
{
    return seg[k].curv > 0.004f ? sqrtf(fmaxf(RUN_ALAT + GRAV * seg[k].bin, 3.f) / seg[k].curv) : 1e9f;
}

/* fastest the AI goes at seg k, q segments before a take-off: bends near the limit, no flying off a crest on the way */
static float run_cap(int k, int q)
{
    float v = bend_hold(k);

    if (q > (int)(40 / ds) && seg[k].vcurv > 0.002f) {
        v = fminf(v, 1.2f * sqrtf(GRAV / seg[k].vcurv));
    }

    return v;
}

/* run-up to a take-off at speed vt from seg L: flat out (boost if b) up to a braking curve that meets vt at the lip,
 * back through the last bend before it */
static void run_up(int L, float vt, int b)
{
    int inb = 0;

    for (int q = 0; q < (int)(700 / ds); q++) {
        int si = (L - q + nseg) % nseg;

        if (seg[si].steep) {
            break;
        }

        if (bend_hold(si) < 1e9f) {
            inb = 1;
        } else if (inb) {
            break;
        }
        float v = fminf(run_cap(si, q), sqrtf(vt * vt + 2 * 6.f * q * ds));

        if (v >= jump_v[si]) {
            jump_v[si] = v;
            jump_b[si] = (unsigned char)b;
        }
    }
}

/* a scratch car from seg s at speed v0 to seg goal, settled on its wheels: the damage it took, 1000 if it crashed,
 * got stuck or never got there. *vmark: its speed at seg mark */
static float scratch_run(int s, float v0, int mark, int goal, float lane, int boost, float *vmark)
{
    Car t;
    memset(&t, 0, sizeof t);
    t.active = 1;
    t.boost = boost ? 100 : 0;
    car_place(&t, s, lane, v0);
    int dm = (mark - s + nseg) % nseg, dg = (goal - s + nseg) % nseg, seen = 0;

    for (int i = 0; i < 240 * 20; i++) {
        ai_drive(&t, lane, skill_vcap[1], skill_alat[1], NULL);
        car_step(&t, 1.f / 240);

        if (t.crash > 0 || t.crane > 0 || t.wrecked) {
            return 1000;
        }

        float d = t.prog - s;

        if (d >= dm && !seen) {
            seen = 1;

            if (vmark) {
                *vmark = vdot(t.v, t.F);
            }
        }

        if (d >= dg && (t.nwc >= 3 || goal == mark)) {
            return t.damage;
        }
    }

    return 1000;
}

/* the scratch car over a chain of n jumps (lips l[], -1 = no run-up of its own), all taken off at v, from seg s
 * (at no more than v0) */
static float try_jumps(const int *l, int n, float v, int b, int s, float v0, int goal, float lane, float *vmark)
{
    memset(jump_v, 0, sizeof(float) * nseg);
    memset(jump_b, 0, nseg);

    for (int k = 0; k < n; k++) {
        if (l[k] >= 0) {
            run_up(l[k], v, b);
        }
    }

    if (jump_v[s] > 0) {
        v0 = fminf(v0, jump_v[s]);
    }

    return scratch_run(s, fminf(v0, v), l[0], goal, lane, b, vmark);
}

/* a jump starts at a sheer face going down and runs on over the next faces while the road between is no place to
 * land: far below the lip (a pit) or above it (a wall to clear). Jumps with little road between them (stepping
 * stones) form a chain taken at one speed: the one the scratch car got over cleanest, with a little either side to
 * spare, that the run-up can reach (without boost if it can) */
static int badj[NJ], nbadj; /* lips of jumps nothing gets over cleanly */
static int plan_gen = -1;   /* the track_gen the plan is for */

static void plan_jumps(void)
{
    int lip[NJ], end[NJ], back[NJ], nj = 0;
    nbadj = 0;
    memset(jump_v, 0, sizeof(float) * nseg);
    memset(jump_b, 0, nseg);

    for (int k = 0; k < nseg && nj < NJ; k++) {
        int l = (k + nseg - 1) % nseg;

        if (!seg[k].steep || seg[l].steep || seg[(k + 2) % nseg].c.y > seg[k].c.y) {
            continue; /* only faces going down */
        }

        int e = k;
        float ylip = seg[k].c.y;

        for (;;) {
            while (seg[(e + 1) % nseg].steep) {
                e = (e + 1) % nseg;
            }

            int m = 1, lim = (int)(300 / ds);

            while (m < lim && !seg[(e + m) % nseg].steep) {
                m++;
            }

            if (m >= lim) {
                break;
            }

            float lo = 1e9f, hi = -1e9f;

            for (int q = 1; q < m; q++) {
                lo = fminf(lo, seg[(e + q) % nseg].c.y);
                hi = fmaxf(hi, seg[(e + q) % nseg].c.y);
            }

            if (!(hi < ylip - 2.f || lo > ylip + 2.f)) {
                break;
            }

            e = (e + m) % nseg;
        }

        int bk = 0;

        while (bk < (int)(700 / ds) && !seg[(l - bk - 1 + nseg) % nseg].steep) {
            bk++;
        }

        lip[nj] = l;
        end[nj] = e;
        back[nj] = bk;
        nj++;
    }

    const int CHAIN = (int)(150 / ds);
    int h0 = 0;

    while (h0 < nj && back[h0] < CHAIN) {
        h0++;
    }

    if (h0 == nj) {
        h0 = 0;
    }

    float cv[NJ];
    int cb[NJ], cl[NJ][NJ], cn[NJ], nc = 0;
    int st0 = state;
    float rt0 = racetime;
    int np0 = npart, sf0 = stat_falls, ss0 = stat_sparks;
    uint32_t rs0 = rngs;
    state = ST_RACE;
    planning = 1;

    for (int j = h0; j < h0 + nj;) {
        int *l = cl[nc], n = 0, J = j % nj;

        do {
            l[n++] = back[j % nj] < (int)(40 / ds) ? -1 : lip[j % nj];
            j++;
        } while (j < h0 + nj && back[j % nj] < CHAIN);
        int L = lip[J], E = end[(j - 1) % nj], bk = back[J], nxt = lip[j % nj];
        int dgo = (int)(200 / ds), dnx = (nxt - E + nseg) % nseg - (int)(30 / ds);
        int goal = (E + (nj > 1 && dnx < dgo ? (dnx > 2 ? dnx : 2) : dgo)) % nseg;
        /* fastest take-off: flat out from the start of the last bend, the bend at the limit */
        int q0 = 0;

        while (q0 < bk && bend_hold((L - q0 + nseg) % nseg) >= 1e9f) {
            q0++;
        }

        while (q0 < bk && bend_hold((L - q0 + nseg) % nseg) < 1e9f) {
            q0++;
        }

        q0 = q0 + (int)(10 / ds) < bk ? q0 + (int)(10 / ds) : bk - (int)(5 / ds);
        int rs = (L - q0 + nseg) % nseg;
        float vr[2] = {0, 0};

        for (int b = 0; b < 2; b++) {
            try_jumps(l, 1, 200.f, b, rs, 55.f, L, 0, &vr[b]);
        }

        /* what each take-off speed does */
        enum { V0 = 20, NV = 92 };

        float sc[NV], best = 1e9f;
        int bv = 0;
        int vtop = (int)fminf(vr[1] + 3, V0 + NV - 1);
        int ts = (L - (bk < CHAIN ? bk - (int)(5 / ds) : CHAIN) + nseg) % nseg;

        for (int v = V0; v < V0 + NV; v++) {
            sc[v - V0] = v <= vtop ? try_jumps(l, n, (float)v, 1, ts, 200.f, goal, 0, NULL) : 1000;
        }

        for (int pass = 0; pass < 2; pass++) { /* pass 1: the best few again from either side of the deck */
            for (int v = V0 + 1; v < V0 + NV - 1; v++) {
                float r = fmaxf(sc[v - V0], fmaxf(sc[v - V0 - 1], sc[v - V0 + 1]));
                float cost = r + (v > vr[0] - 1.5f ? 3.f : 0.f) + 20.f * fmaxf(v - (vr[1] - 1.5f), 0.f) -
                             0.05f * v; /* clean, then fast */
                if (pass == 1 && cost < best + 5.f) {
                    for (int sd = -1; sd <= 1; sd += 2) {
                        cost += fmaxf(0.f, try_jumps(l, n, (float)v, 1, ts, 200.f, goal, 2.f * sd, NULL) - r);
                    }

                    if (cost < best) {
                        best = cost;
                        bv = v;
                    }
                } else if (pass == 0 && cost < best) {
                    best = cost;
                    bv = v;
                }
            }
        }
        cv[nc] = (float)bv;
        cb[nc] = bv > vr[0] - 1.5f;
        cn[nc] = n;

        if (nbadj < NJ && best >= 500.f) {
            badj[nbadj++] = L; /* no take-off speed gets over it: the builder says so */
        }

        if (getenv("SCR_DEBUG")) {
            printf("jump seg %d..%d (%d): %.1f m/s (reach %.1f, %.1f on boost; risk %.1f)\n", L, E, n, cv[nc], vr[0],
                   vr[1], best);
        }

        nc++;
    }
    memset(jump_v, 0, sizeof(float) * nseg);
    memset(jump_b, 0, nseg);

    for (int c = 0; c < nc; c++) {
        for (int k = 0; k < cn[c]; k++) {
            if (cl[c][k] >= 0) {
                run_up(cl[c][k], cv[c], cb[c]);
            }
        }
    }

    state = st0;
    racetime = rt0;
    npart = np0;
    stat_falls = sf0;
    stat_sparks = ss0;
    rngs = rs0;
    planning = 0;
    plan_gen = track_gen;
}

/* the plan is only needed once the AI drives: at the start of a race, not when a track is shown */
static void plan_need(void)
{
    if (plan_gen != track_gen) {
        plan_jumps();
    }
}
