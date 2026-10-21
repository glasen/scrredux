/* physics/car_step.h - one physics step of a car, and car-to-car contact */
#pragma once

static void crash_smoke(const Car *c)
{
    V3 eng = body_pt(c, V(0, FLAMEP[1].y, (FLAMEP[1].z + FLAMEP[2].z) * 0.5f));
    spawn(P_SMOKE, vadd(eng, vmul(rnd_dir(), 0.3f)), V(frand() - 0.5f, 1.5f, frand() - 0.5f), 2.5f + frand(), 0.3f,
          (frand() < 0.5f) ? 0x4A4A4E : 0x66666A);

    if (frand() < 0.6f) {
        spawn(P_FIRE, vadd(eng, vmul(rnd_dir(), 0.4f)), V(0, 1.f, 0), 0.4f + 0.3f * frand(), 0.35f,
              frand() < 0.5f ? 0xFF8A18 : 0xFFD040);
    }
}

static void lap_done(Car *c);

static void car_step(Car *c, float dt)
{
    if (!c->active) {
        return;
    }

    if (c->hook > 0) {
        c->hook -= dt;
    }

    if (c->crane > 0) {
        crane_step(c, dt);
        return;
    }

    if (c->wrecked || c->crash > 0) {
        c->thr = 0;
        c->brk = 1;
        c->bst = 0;
        c->str = 0;
    }

    Q q;
    track_query(c->x, c->seg, 30, &q);

    if (q.valid) {
        int prev = c->seg, cu = q.seg;

        if (prev > nseg * 3 / 4 && cu < nseg / 4) {
            c->lap++;

            if (c->lap > c->lapmax) { /* a new lap; not the line again after reversing or a crane drop behind it */
                c->lapmax = c->lap;

                if (state == ST_RACE && c->lap >= 2) {
                    c->lastlap = racetime - c->lapstart;

                    if (c->bestlap <= 0 || c->lastlap < c->bestlap) {
                        c->bestlap = c->lastlap;
                    }

                    c->lapshow = 3.f;
                    lap_done(c);
                }

                c->lapstart = racetime;

                if (laps > 0 && c->lap > laps && !c->finished && state == ST_RACE) {
                    c->finished = 1;
                    c->ftime = racetime;
                }
            }
        } else if (prev < nseg / 4 && cu > nseg * 3 / 4) {
            c->lap--;
        }
        c->seg = cu;
        c->prog = (float)c->lap * nseg + cu + q.u;
    }

    if (c->hurt_cd > 0) {
        c->hurt_cd -= dt;
    }

    if (c->smash_t > 0) {
        c->smash_t -= dt;
    }

    V3 Fo = V(0, -CAR_M * GRAV, 0), To = V(0, 0, 0);
    float speed = vdot(c->v, c->F);
    float gl = -GRAV * c->R.y, dmax = steer_max(speed, c->str >= 0 ? gl : -gl);
    float tgt = c->str * dmax, dd = tgt - c->delta, mr = phys.steer_rate * dt;
    c->delta += clampf(dd, -mr, mr);

    float Fdrv = 0, brake = c->brk;

    if (c->thr > 0) {
        float P = phys.engine_power, Fm = phys.max_drive_force;

        if (c->bst && c->boost > 0) {
            P += phys.boost_power;
            Fm += phys.boost_force_max;
            c->boost -= dt * phys.boost_drain_rate;

            if (c->boost < 0) {
                c->boost = 0;
            }
        }

        float vtop = c->bst && c->boost > 0
                         ? phys.top_speed_boost
                         : phys.top_speed; /* the engine cuts out at the top speed, boost or not, like the original */
        Fdrv = c->thr * fminf(Fm, P / fmaxf(speed, 3.f)) * clampf((vtop - speed) / 2.f, 0, 1);
    }

    if (c->brk > 0 && speed < 0.8f && c->thr <= 0 && !c->hold) {
        Fdrv = -c->brk * 4500.f;
        brake = 0;
    }

    int nwc = 0;

    for (int i = 0; i < 4; i++) {
        V3 M = body_pt(c, WMOUNT[i]);
        Q wq;
        track_query(M, c->seg, 12, &wq);
        float hub = WLF[i];
        int contact = 0;
        float l = WLF[i];
        float over = fabsf(wq.lat) - (TRACK_HW + 0.1f);

        if (wq.valid && over > 0) {
            wq.h += phys.shoulder_drop + 0.5f * over;
            wq.N = vnorm(vadd(wq.N, vmul(wq.R, wq.lat > 0 ? 0.5f : -0.5f)));
        }

        float cosd = wq.valid ? vdot(c->U, wq.N) : 0;
        int wcf = wq.valid ? climb_face(wq.seg) : -1;
        V3 wn;

        if (wcf >= 0 && in_climb_wall(M, wcf, &wn) > 0) {
            wq.valid = 0; /* below the top of a face going up: the wall */
        }

        if (wq.valid && wq.road && over < phys.shoulder_width && cosd > 0.15f &&
            wq.h > (seg[wq.seg].steep ? -8.f : -0.6f)) {
            float d = wq.h / cosd;
            l = d - WRAD[i];

            if (l < WLF[i]) {
                contact = 1;
                hub = l > 0 ? l : 0;
            }
        }

        c->wl[i] = hub;
        int was = c->wcon[i];
        c->wcon[i] = contact;

        if (!contact) {
            c->wbot[i] = 0;
            continue;
        }

        nwc++;
        V3 N = wq.N, Pc = vsub(M, vmul(c->U, l + WRAD[i]));
        V3 r = vsub(Pc, c->x), vc = vadd(c->v, vcross(c->w, r));
        float rate = -vdot(vc, N);

        if (!was && rate > 4.f) {
            snd_grounded(c, rate); /* hard touchdown */
        }

        float comp = WLF[i] - l;
        float kk = i < 2 ? K_F : K_R, cs = sqrtf(kk / 14000.f);
        float Fs = kk * comp + (rate > 0 ? C_COMP : C_REB) * cs * rate;

        if (l < WLMIN[i]) {
            float x = WLMIN[i] - l; /* progressive hydraulic stop */
            Fs += K_BUMP * x + 6.0e6f * x * x + (rate > 0 ? 9000.f * rate : 0.f);

            if (!c->wbot[i] && rate > 2.5f) {
                snd_grounded(c, rate); /* bottomed out: the original's grounded sound */
            }

            if (!c->wbot[i] && rate > 7.f) {
                car_hurt(c, (rate - 7.f) * 0.6f, 1.f);
            }

            c->wbot[i] = 1;
        } else {
            c->wbot[i] = 0;
        }

        if (Fs < 0) {
            Fs = 0;
        }

        if (Fs > 160000.f) {
            Fs = 160000.f;
        }

        if (seg[wq.seg].steep && Fs > 1.5f * CAR_M * GRAV) {
            Fs = 1.5f * CAR_M * GRAV;
        }

        V3 fN = vmul(N, Fs);
        Fo = vadd(Fo, fN);
        To = vadd(To, vcross(r, fN));

        V3 Fw0 = c->F;

        if (i < 2) {
            Fw0 = vadd(vmul(c->F, cosf(c->delta)), vmul(c->R, sinf(c->delta)));
        }

        V3 Fw = vnorm(vsub(Fw0, vmul(N, vdot(Fw0, N)))), Sw = vcross(Fw, N);
        float vlong = vdot(vc, Fw), vlat = vdot(vc, Sw);
        c->wph[i] = fmodf(c->wph[i] + vlong * dt / WRAD[i], 2 * PI_F);
        float Fx = 0;

        if (i >= 2) {
            Fx += Fdrv * 0.5f;
        }

        Fx -= brake * (i < 2 ? phys.brake_force_front : phys.brake_force_rear) * tanhf(vlong * 0.25f);
        Fx -= 0.012f * Fs * tanhf(vlong * 3.f);
        float maxF = MU * (i < 2 ? 1.f : phys.rear_grip_factor) *
                     fminf(Fs, phys.tyre_load_max); /* tyre load saturation: landing spikes don't add grip */
        float Fy = -maxF * tanhf(phys.cornering_stiffness * vlat / (fabsf(vlong) + phys.low_speed_slip_offset));
        float mag = sqrtf(Fx * Fx + Fy * Fy);

        if (mag > maxF && mag > 1e-3f) {
            float k = maxF / mag;
            Fx *= k;
            Fy *= k;
        }

        V3 ft = vadd(vmul(Fw, Fx), vmul(Sw, Fy));
        Fo = vadd(Fo, ft);
        V3 rr =
            vsub(r, vmul(c->U, 0.85f * vdot(r, c->U))); /* tyre forces act near CoM height: no roll-over from grip */
        To = vadd(To, vcross(rr, ft));
    }
    c->nwc = nwc;

    for (int i = 0; i < 16; i++) {
        contact_point(c, i, CPTS[i], &Fo, &To);
    }

    edge_contact(c, &Fo, &To);
    int touch = 0, net = 0;
    float impact = ground_contact(c, &Fo, &To, &touch);

    if (touch && c == &cars[0]) {
        snd_dirt = 1;
    }

    impact = fmaxf(impact, 0.5f * fence_contact(c, &Fo, &To, &net)); /* the net gives: a softer hit */
    if ((touch || net) && c->crash <= 0) { /* landed beside the track or caught in the net: crash */
        c->crash = 1e-4f;
        stat_falls++;
        car_hurt(c, 6.f + 0.5f * impact, 1.f);
        dust_burst(c->x, 36, 3.f + 0.3f * impact);

        if (c == &cars[0]) {
            set_msg("CRASHED!", 2.0f);
        }

        if (getenv("SCR_DEBUG") && !planning) {
            fprintf(stderr, "crash car%d seg %d impact %.1f t=%.2f\n", (int)(c - cars), c->seg, impact, racetime);
        }
    }

    float sp2 = vlen(c->v);
    Fo = vsub(Fo, vmul(c->v, phys.drag * sp2));
    To = vsub(To, vmul(c->w, 180.f));

    if (nwc == 0 && sp2 > 6.f) { /* air-control assist: nose follows the flight path, wings level */
        V3 vn = vmul(c->v, 1.f / sp2);
        V3 ax = vcross(c->F, vn); /* axis taking the nose toward the velocity */
        To = vadd(To, vsub(vmul(ax, phys.air_control_torque),
                           vadd(vmul(c->R, 4500.f * vdot(c->w, c->R)), vmul(c->U, 4500.f * vdot(c->w, c->U)))));
        V3 Ut = vnorm(vsub(UP, vmul(c->F, vdot(UP, c->F))));
        To = vadd(To, vmul(c->F, 9000.f * vdot(vcross(c->U, Ut), c->F) - 3200.f * vdot(c->w, c->F)));
    }

    if (nwc > 0) { /* stability aid while on the ground */
        float g = nwc >= 3 ? 1.f : 0.5f;
        To = vsub(To, vmul(c->F, g * phys.roll_damping * vdot(c->w, c->F)));
        To = vsub(To, vmul(c->U, g * phys.yaw_damping * vdot(c->w, c->U)));
        To = vsub(To, vmul(c->R, g * phys.pitch_damping * vdot(c->w, c->R)));
    }

    { /* exhaust flames: only while the boost burns */
        float tg = (c->bst && c->boost > 0 && c->thr > 0 && !c->wrecked) ? 1.4f : 0.f;
        c->flame += (tg - c->flame) * clampf(14.f * dt, 0, 1);
    }
    { /* body lean: rocks the cab on its springs so cornering/braking/landing are visible */
        float al = vdot(Fo, c->R) / CAR_M, ag = vdot(Fo, c->F) / CAR_M, k = clampf(16.f * dt, 0, 1);
        c->alat_f += (clampf(al, -30, 30) - c->alat_f) * k;
        c->along_f += (clampf(ag, -25, 25) - c->along_f) * k;
        float tr = nwc > 1 ? clampf(-c->alat_f * phys.lean_roll_per_accel, -0.20f, 0.20f) : 0.f;
        float tp = nwc > 1 ? clampf(c->along_f * phys.lean_pitch_per_accel, -0.14f, 0.14f) : 0.f;
        c->lrr += (140.f * (tr - c->lr) - 21.f * c->lrr) * dt;
        c->lr += c->lrr * dt;
        c->lpr += (120.f * (tp - c->lp) - 19.f * c->lpr) * dt;
        c->lp += c->lpr * dt;
        lean_limit(c);
    }
    c->v = vadd(c->v, vmul(Fo, dt / CAR_M));
    V3 tl = V(vdot(To, c->R), vdot(To, c->U), vdot(To, c->F));
    c->w = vadd(c->w, vmul(vadd(vmul(c->R, tl.x / IX), vadd(vmul(c->U, tl.y / IY), vmul(c->F, tl.z / IZ))), dt));
    float wlen = vlen(c->w);

    if (wlen > 14.f) {
        c->w = vmul(c->w, 14.f / wlen);
        wlen = 14.f;
    }

    if (sp2 > 130.f) {
        c->v = vmul(c->v, 130.f / sp2);
    }

    c->x = vadd(c->x, vmul(c->v, dt));

    if (wlen > 1e-6f) {
        V3 ax = vmul(c->w, 1 / wlen);
        float an = wlen * dt;
        c->F = rotv(c->F, ax, an);
        c->U = rotv(c->U, ax, an);
    }

    c->F = vnorm(c->F);
    c->U = vnorm(vsub(c->U, vmul(c->F, vdot(c->U, c->F))));
    c->R = vcross(c->F, c->U);

    if (nwc >= 3 && q.valid && q.road && fabsf(q.lat) < TRACK_HW - 1.5f) {
        c->safe = q.seg;
    }

    if (c->crash > 0) { /* lying in the dirt: smoke, a little fire, then the crane */
        c->crash += dt;
        c->dust_t -= dt;

        if (c->dust_t <= 0) {
            c->dust_t = 0.12f;
            crash_smoke(c);
        }

        if (c->crash > 3.4f) {
            start_crane(c, restart_seg(c), 0, 2.6f);
        }

        return;
    }

    c->flip_t = c->U.y < 0.25f && vlen(c->v) < 6.f ? c->flip_t + dt : 0;
    c->stuck_t = (state == ST_RACE && vlen(c->v) < 1.0f && c->thr > 0.15f) ? c->stuck_t + dt : 0;

    if (c->flip_t > 2.5f || c->stuck_t > 2.5f || c->x.y < ground_h(c->x.x, c->x.z) - 4.f) {
        stat_falls++;
        car_hurt(c, 4.f, 1.f);

        if (getenv("SCR_DEBUG") && !planning) {
            fprintf(stderr, "reset car%d seg %d flip=%.1f stuck=%.1f\n", (int)(c - cars), c->seg, c->flip_t,
                    c->stuck_t);
        }

        if (c == &cars[0]) {
            set_msg("RESET", 1.5f);
        }

        start_crane(c, restart_seg(c), 0, 2.6f);
    }
}

static void apply_ang(Car *a, V3 t, float dt)
{
    a->w = vadd(a->w, vmul(vadd(vmul(a->R, vdot(t, a->R) / IX),
                                vadd(vmul(a->U, vdot(t, a->U) / IY), vmul(a->F, vdot(t, a->F) / IZ))),
                           dt));
}

static void cars_interact(Car *a, Car *b, float dt)
{
    if (!a->active || !b->active || car_out(a) || car_out(b)) {
        return;
    }

    static const float off[5] = {-1.8f, -0.9f, 0.f, 0.9f, 1.8f};

    for (int i = 0; i < 5; i++) {
        for (int j = 0; j < 5; j++) {
            V3 pa = body_pt(a, V(0, 0.1f, off[i])), pb = body_pt(b, V(0, 0.1f, off[j]));
            V3 d = vsub(pb, pa);
            float dist = vlen(d);

            if (dist >= 1.9f || dist < 1e-4f) {
                continue;
            }

            V3 n = vmul(d, 1 / dist);
            V3 ra = vsub(pa, a->x), rb = vsub(pb, b->x);
            V3 va = vadd(a->v, vcross(a->w, ra)), vb = vadd(b->v, vcross(b->w, rb));
            float vn = vdot(vsub(vb, va), n);
            float Fn = 30000.f * (1.9f - dist) - 2000.f * vn;

            if (Fn < 0) {
                continue;
            }

            stat_contacts++;
            stat_push += Fn * dt;
            V3 f = vmul(n, Fn);
            a->v = vsub(a->v, vmul(f, dt / CAR_M));
            b->v = vadd(b->v, vmul(f, dt / CAR_M));
            apply_ang(a, vcross(ra, vmul(f, -1)), dt);
            apply_ang(b, vcross(rb, f), dt);

            if (-vn > 1.f && hitcar_cd <= 0 && (snd_live(a) || snd_live(b))) {
                snd_play(S_HITCAR, 238, 56);
                hitcar_cd = 0.5f;
            }

            if (-vn > 3.f) {
                car_hurt(a, 0.015f * (-vn - 3.f), 0.6f);
                car_hurt(b, 0.015f * (-vn - 3.f), 0.6f);
            }
        }
    }
}
