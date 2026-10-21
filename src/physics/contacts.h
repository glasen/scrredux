/* physics/contacts.h - wheel, body, edge, ground and fence contacts and recovery cranes */
#pragma once

/* a sheer face going up just behind or at segment k: its segment, else -1 */
static int climb_face(int k)
{
    for (int o = -3; o <= 1; o++) {
        int f = ((k + o) % nseg + nseg) % nseg;

        if (seg[f].steep && seg[(f + 1) % nseg].c.y > seg[f].c.y) {
            return f;
        }
    }

    return -1;
}

/* below the upper deck past a sheer face going up: the face is a wall there. How far P is through it, else 0 */
static float in_climb_wall(V3 P, int f, V3 *n)
{
    const Seg *s = &seg[f], *s2 = &seg[(f + 1) % nseg];
    V3 Th = vnorm(V(s2->c.x - s->c.x, 0, s2->c.z - s->c.z));
    float d = vdot(vsub(P, s->c), Th);

    if (d <= 0 || d > CLIMB_WALL_DEPTH || P.y > s2->c.y - CLIMB_WALL_CLEARANCE) {
        return 0;
    }

    *n = vmul(Th, -1);
    return d;
}

/* penalty contact of one body point against the solid deck / barriers */
static void contact_point(Car *c, int idx, V3 lp, V3 *F, V3 *Tq)
{
    V3 P = body_pt(c, lp);
    Q q;
    track_query(P, c->seg, 12, &q);

    if (!q.valid || !q.road) {
        c->point_hit[idx] = 0;
        return;
    }

    const float ow = TRACK_HW;
    float al = fabsf(q.lat), h = q.h, sg = q.lat > 0 ? 1.f : -1.f;
    float topy = 0.0f;
    int stp = seg[q.seg].steep || seg[(q.seg + 1) % nseg].steep ||
              seg[(q.seg + nseg - 1) % nseg].steep; /* solid down to the ground */
    int cf = al < ow ? climb_face(q.seg) : -1;
    V3 wn;
    float wd = cf >= 0 ? in_climb_wall(P, cf, &wn) : 0;

    if (wd > 0) {
        h = -wd;
        topy = 0;
        stp = 0;
    } /* the wall: pushed straight back out, never up the face */
    if (al > ow || (h < -BODY_DEPTH && !stp && wd <= 0) || h > topy) {
        c->point_hit[idx] = 0;
        return;
    }

    float best = topy - h;
    V3 bn = wd > 0 ? wn : q.N;
    float pb = h + BODY_DEPTH;

    if (pb < best && !stp && wd <= 0) {
        best = pb;
        bn = vmul(q.N, -1);
    }

    float ps = ow - al;

    if (ps < best && wd <= 0) {
        best = ps;
        bn = vmul(q.R, sg);
    }

    if (seg[(q.seg + nseg - 1) % nseg].gap) {
        float pst = q.u * ds;

        if (pst < best) {
            best = pst;
            bn = vmul(q.T, -1);
        }
    }

    if (seg[(q.seg + 1) % nseg].gap) {
        float pe = (1 - q.u) * ds;

        if (pe < best) {
            best = pe;
            bn = q.T;
        }
    }

    V3 r = vsub(P, c->x), vp = vadd(c->v, vcross(c->w, r));
    float vn = vdot(vp, bn);
    float Fn = BODY_SPRING * best - BODY_DAMPING * vn;

    if (Fn < 0) {
        Fn = 0;
    }

    if (Fn > BODY_FORCE_MAX) {
        Fn = BODY_FORCE_MAX;
    }

    if (stp && Fn > BODY_STEEP_G * CAR_M * GRAV) {
        Fn =
            BODY_STEEP_G * CAR_M * GRAV; /* a sheer face lifts the car, it does not fire it (the original clamps too) */
    }

    V3 f = vmul(bn, Fn);
    V3 vt = vsub(vp, vmul(bn, vn));
    float vts = vlen(vt);

    if (vts > 1e-3f) {
        f = vadd(f, vmul(vt, -BODY_FRICTION * Fn * tanhf(vts / CONTACT_SLIP_SCALE) / vts));
    }

    *F = vadd(*F, f);
    *Tq = vadd(*Tq, vcross(r, f));
    if (vts > BODY_SCRAPE_SPEED && Fn > BODY_SCRAPE_FORCE && c == &cars[0]) {
        sfx.scraping = 1;
    }

    if (vts > BODY_SCRAPE_SPEED && Fn > BODY_SCRAPE_FORCE &&
        frand() < BODY_SPARK_CHANCE) { /* steel on the deck: sparks */
        for (int k = 0; k < 4; k++) {
            spawn(P_SPARK, P,
                  vadd(vadd(vmul(vt, 0.55f + 0.3f * frand()), vmul(bn, 1.5f + 2.f * frand())), vmul(rnd_dir(), 2.f)),
                  0.25f + 0.35f * frand(), 0.05f, frand() < 0.5f ? 0xFFF0A0 : 0xFFB030);
        }
    }

    if (!c->point_hit[idx] && -vn > BODY_HIT_SPEED) {
        car_hurt(c, (-vn - BODY_HIT_SPEED) * BODY_HIT_DAMAGE, 1.f);
    }

    c->point_hit[idx] = 1;
}

/* underside of the car (car frame): tub in the middle, narrower frame rails front and rear */
static void belly_at(float z, float *y, float *hw)
{
    if (z > 0.6f) {
        *y = -0.50f;
        *hw = 0.30f;
    } else if (z < -1.1f) {
        *y = -0.53f;
        *hw = 0.32f;
    } else {
        *y = -0.313f;
        *hw = 0.63f;
    }
}

/* the open deck edge against the underside: when the car hangs over the edge it rides and scrapes on the corner */
static void edge_contact(Car *c, V3 *F, V3 *Tq)
{
    for (float z = -1.65f; z <= 2.1f; z += 0.25f) {
        float by, bhw;
        belly_at(z, &by, &bhw);
        V3 B = body_pt(c, V(0, by, z));
        Q q;
        track_query(B, c->seg, 12, &q);

        if (!q.valid || !q.road) {
            continue;
        }

        for (int sd = -1; sd <= 1; sd += 2) {
            V3 E =
                vadd(vsub(vsub(B, vmul(q.R, q.lat)), vmul(q.N, q.h)), vmul(q.R, sd * TRACK_HW)); /* deck corner here */
            V3 d = vsub(E, c->x), l = V(vdot(d, c->R), vdot(d, c->U), vdot(d, c->F));
            float ey, ehw;
            belly_at(l.z, &ey, &ehw);
            float pen = l.y - ey;

            if (fabsf(l.x) > ehw || pen <= 0 || pen > EDGE_MAX_PENETRATION) {
                continue;
            }

            V3 r = d, vp = vadd(c->v, vcross(c->w, r));
            float vn = vdot(vp, c->U);
            float Fn = fmaxf(BODY_SPRING * pen - BODY_DAMPING * vn, 0.f);

            if (Fn > EDGE_FORCE_MAX) {
                Fn = EDGE_FORCE_MAX;
            }

            V3 f = vmul(c->U, Fn), vt = vsub(vp, vmul(c->U, vn));
            float vts = vlen(vt);

            if (vts > 1e-3f) {
                f = vadd(f, vmul(vt, -EDGE_FRICTION * Fn * tanhf(vts / CONTACT_SLIP_SCALE) / vts));
            }

            *F = vadd(*F, f);
            *Tq = vadd(*Tq, vcross(r, f));
            if (vts > EDGE_SCRAPE_SPEED && c == &cars[0]) {
                sfx.scraping = 1;
            }

            if (vts > EDGE_SCRAPE_SPEED && frand() < EDGE_SPARK_CHANCE) { /* frame grinding on the edge: sparks */
                for (int k = 0; k < 4; k++) { /* some fly high and wide: seen from the cockpit */
                    V3 sv =
                        (k & 1)
                            ? vadd(vadd(vmul(c->v, 1.f), vmul(c->F, 3.f + 4.f * frand())), /* sprayed forward and up */
                                   vadd(vmul(q.R, sd * (1.5f + 3.f * frand())), vmul(q.N, 3.f + 4.f * frand())))
                            : vadd(vadd(vmul(vt, 0.5f + 0.3f * frand()), vmul(q.R, sd * (1.f + 2.f * frand()))),
                                   vadd(vmul(q.N, 1.f + 2.f * frand()), vmul(rnd_dir(), 1.5f)));
                    spawn(P_SPARK, E, sv, 0.35f + 0.45f * frand(), 0.05f, frand() < 0.5f ? 0xFFF0A0 : 0xFFB030);
                }
            }
        }
    }
}

static void car_body_rows(const Car *c, float m[12]);
static float WBUMP[4];

/* the visual lean may not push a mount further down than its wheel's remaining bump travel (else the tyre sinks into the road) */
static void lean_limit(Car *c)
{
    if (WBUMP[0] <= 0) {
        return;
    }

    float m[12], k = 1;
    car_body_rows(c, m);

    for (int i = 0; i < 4; i++) {
        if (c->suspension_len[i] >= WLF[i] - 1e-3f) {
            continue; /* wheel in the air */
        }

        V3 w = WMOUNT[i],
           lm = V(m[0] * w.x + m[1] * w.y + m[2] * w.z + m[3], m[4] * w.x + m[5] * w.y + m[6] * w.z + m[7],
                  m[8] * w.x + m[9] * w.y + m[10] * w.z + m[11]);
        float drop = -vdot(c->U, vsub(lm, body_pt(c, w))),
              room = fmaxf(WBUMP[i] - (WREST[i] - c->suspension_len[i]), 0.f);

        if (drop > room) {
            k = fminf(k, room / drop);
        }
    }

    if (k < 1) {
        c->lean_roll *= k;
        c->lean_pitch *= k;
        c->lean_roll_rate *= k;
        c->lean_pitch_rate *= k;
    }
}

static int car_out(const Car *c)
{
    return c->crash_time > 0 || c->crane_time > 0;
}

/* where the crane puts the car back: the middle of the section it was last on, or of the nearest one before it allowed */
static int restart_seg(const Car *c)
{
    int s = seg_sec[c->safe_segment], s0;

    for (int n = 0; n < nsec && !sec_ok[s]; n++) {
        s = (s + nsec - 1) % nsec;
    }

    s0 = s; /* and not where a jump ahead needs a run-up: from a standstill it would fall in */
    for (int n = 0; n < nsec && (!sec_ok[s] || jump_v[sec_mid[s]] > 0); n++) {
        s = (s + nsec - 1) % nsec;
    }

    return sec_mid[sec_ok[s] && jump_v[sec_mid[s]] <= 0 ? s : s0];
}

/* the crane picks the car up and lowers it onto the track at segment s, like the original */
static void start_crane(Car *c, int s, float lat, float T)
{
    s = ((s % nseg) + nseg) % nseg;

    if (c->lap > 0 && c->seg < nseg / 4 && s > nseg * 3 / 4) {
        c->lap--; /* set down behind the start line: that lap again */
    }

    car_place(c, s, lat, 0);
    c->crane_target = c->x;
    c->crane_segment = c->seg;
    c->crane_time = 1e-4f;
    c->crane_duration = T;
    c->crash_time = 0;
    c->hook_time = 0;
    c->lean_roll = c->lean_pitch = c->lean_roll_rate = c->lean_pitch_rate = 0;
}

static void crane_step(Car *c, float dt)
{
    c->crane_time += dt;
    float f = smooth01(c->crane_time / c->crane_duration),
          sway = CRANE_SWAY * sinf(c->crane_time * CRANE_SWAY_RATE) * (1 - f);
    const Seg *s = &seg[c->crane_segment];
    c->x = vadd(c->crane_target, vmul(s->N, CRANE_HEIGHT * (1 - f)));
    c->F = vnorm(vadd(s->T, vmul(s->R, sway)));
    c->U = s->N;
    c->R = vcross(c->F, c->U);
    c->v = V(0, 0, 0);
    c->w = V(0, 0, 0);

    for (int i = 0; i < 4; i++) {
        c->suspension_len[i] = WLF[i];
    }

    c->throttle = 0;
    c->brake = 1;
    c->boosting = 0;
    c->steer_input = 0;
    c->steer_angle = 0;

    if (c->crane_time >= c->crane_duration) {
        c->crane_time = 0;
        c->hook_time = HOOK_RETRACT_TIME;
    } /* released: the hook goes back up */
}

/* body and wheel points against the ground beside the track; returns the hardest impact speed */
static float ground_contact(Car *c, V3 *F, V3 *Tq, int *touch)
{
    float worst = 0;

    for (int i = 0; i < 20; i++) {
        V3 lp = i < 16 ? CPTS[i] : vsub(WMOUNT[i - 16], V(0, WREST[i - 16] + WRAD[i - 16], 0));
        V3 P = body_pt(c, lp);
        float g = ground_h(P.x, P.z), pen = g - P.y;

        if (pen <= 0) {
            continue;
        }

        *touch = 1;
        V3 r = vsub(P, c->x), vp = vadd(c->v, vcross(c->w, r));
        float vn = vp.y;
        worst = fmaxf(worst, -vn);
        float Fn = fmaxf(GROUND_SPRING * pen - GROUND_DAMPING * vn, 0.f);
        V3 f = V(0, Fn, 0), vt = V(vp.x, 0, vp.z);
        float vts = vlen(vt);

        if (vts > 1e-3f) {
            f = vadd(f, vmul(vt, -GROUND_FRICTION * Fn * tanhf(vts / CONTACT_SLIP_SCALE) / vts));
        }

        *F = vadd(*F, f);
        *Tq = vadd(*Tq, vcross(r, f));
        if (vts > DUST_SPEED && frand() < DUST_CHANCE) {
            spawn(P_DUST, P, vadd(vmul(vt, 0.2f), V(0, 1.2f, 0)), 1.0f + frand(), 0.25f + frand() * 0.3f,
                  (i & 1) ? 0xB49A72 : 0x9C8460);
        }
    }

    return worst;
}

/* the catch net on the arena wall: it brakes the car at up to ~80 g and hardly pushes back, so a car is caught in it
 * instead of bouncing off. A car that went over the top is outside and stays there. */
static float fence_contact(Car *c, V3 *F, V3 *Tq, int *touch)
{
    V3 P[20], n[20];
    float fn[20], sum = 0, worst = 0;
    int np = 0;

    for (int side = 0; side < 4; side++) {
        float out = side == 0   ? ar_x0 - c->x.x
                    : side == 1 ? c->x.x - ar_x1
                    : side == 2 ? ar_z0 - c->x.z
                                : c->x.z - ar_z1;

        if (out > FENCE_REACH) {
            continue;
        }

        V3 nn = side == 0 ? V(1, 0, 0) : side == 1 ? V(-1, 0, 0) : side == 2 ? V(0, 0, 1) : V(0, 0, -1);

        for (int i = 0; i < 20 && np < 20; i++) {
            V3 lp = i < 16 ? CPTS[i] : vsub(WMOUNT[i - 16], V(0, WREST[i - 16] + WRAD[i - 16], 0));
            V3 p = body_pt(c, lp);
            float pen = -vdot(vsub(p, c->x), nn) + out;

            if (pen <= 0 || p.y >= NET_H) {
                continue;
            }

            float vn = vdot(vadd(c->v, vcross(c->w, vsub(p, c->x))), nn);
            worst = fmaxf(worst, -vn);
            P[np] = p;
            n[np] = nn;
            fn[np] = fmaxf(FENCE_SPRING * pen - FENCE_DAMPING * vn, 0.f);
            sum += fn[np++];
        }
    }

    if (!np) {
        return 0;
    }

    *touch = 1;
    float k = sum > FENCE_FORCE_MAX ? FENCE_FORCE_MAX / sum : 1.f;

    for (int j = 0; j < np; j++) {
        V3 r = vsub(P[j], c->x), vp = vadd(c->v, vcross(c->w, r)), vt = vsub(vp, vmul(n[j], vdot(vp, n[j])));
        float Fn = fn[j] * k, vts = vlen(vt);
        V3 f = vmul(n[j], Fn);

        if (vts > 1e-3f) {
            f = vadd(f, vmul(vt, -FENCE_FRICTION * Fn * tanhf(vts / CONTACT_SLIP_SCALE) / vts));
        }

        *F = vadd(*F, f);
        *Tq = vadd(*Tq, vcross(r, f));
    }

    return worst;
}
