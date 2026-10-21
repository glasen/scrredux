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

/* force and torque (about the centre of mass) collected from the contacts during one step */
typedef struct {
    V3 force, torque;
} Loads;

/* counts a lap when the car crosses the start line, back or forth, and tracks its progress */
static void car_update_laps(Car *c, const Q *q)
{
    int prev = c->seg, cu = q->seg;

    if (prev > nseg * 3 / 4 && cu < nseg / 4) {
        c->lap++;

        /* a new lap; not the line again after reversing or a crane drop behind it */
        if (c->lap > c->furthest_lap) {
            c->furthest_lap = c->lap;

            if (state == ST_RACE && c->lap >= 2) {
                c->last_lap = racetime - c->lap_start_time;

                if (c->best_lap <= 0 || c->last_lap < c->best_lap) {
                    c->best_lap = c->last_lap;
                }

                c->lap_banner_time = 3.f;
                lap_done(c);
            }

            c->lap_start_time = racetime;

            if (laps > 0 && c->lap > laps && !c->finished && state == ST_RACE) {
                c->finished = 1;
                c->finish_time = racetime;
            }
        }
    } else if (prev < nseg / 4 && cu > nseg * 3 / 4) {
        c->lap--;
    }

    c->seg = cu;
    c->progress = (float)c->lap * nseg + cu + q->u;
}

/* turns the front wheels towards the steering input, as far as the tyres can hold at this speed */
static void car_steer(Car *c, float dt, float speed)
{
    float gravity_help = -GRAV * c->R.y; /* banking: gravity's help towards the side being steered */
    float limit = steer_max(speed, c->steer_input >= 0 ? gravity_help : -gravity_help);
    float target = c->steer_input * limit, change = target - c->steer_angle, max_change = phys.steer_rate * dt;
    c->steer_angle += clampf(change, -max_change, max_change);
}

/* the engine's push along the car (negative: the reverse the original gives at a standstill); the brake that is left
 * for the wheels to apply goes to *brake */
static float car_drive_force(Car *c, float dt, float speed, float *brake)
{
    float drive = 0;
    *brake = c->brake;

    if (c->throttle > 0) {
        float power = phys.engine_power, force_max = phys.max_drive_force;

        if (c->boosting && c->boost > 0) {
            power += phys.boost_power;
            force_max += phys.boost_force_max;
            c->boost -= dt * phys.boost_drain_rate;

            if (c->boost < 0) {
                c->boost = 0;
            }
        }

        /* the engine cuts out at the top speed, boost or not, like the original */
        float top = c->boosting && c->boost > 0 ? phys.top_speed_boost : phys.top_speed;
        drive = c->throttle * fminf(force_max, power / fmaxf(speed, DRIVE_MIN_SPEED)) *
                clampf((top - speed) / DRIVE_CUTOFF_RANGE, 0, 1);
    }

    if (c->brake > 0 && speed < STANDSTILL_SPEED && c->throttle <= 0 && !c->held_at_start) {
        drive = -c->brake * STANDSTILL_REVERSE_FORCE;
        *brake = 0;
    }

    return drive;
}

/* finds where wheel i meets the road. Sets the suspension length (the wheel hub's distance from its mount) and
 * whether it touches; returns the contact, with *l the unclamped mount-to-road length and *wq the road there */
static int wheel_probe(Car *c, int i, V3 mount, Q *wq, float *l)
{
    float hub = WLF[i];
    int contact = 0;
    *l = WLF[i];
    track_query(mount, c->seg, 12, wq);
    float over = fabsf(wq->lat) - (TRACK_HW + ROAD_EDGE_MARGIN);

    if (wq->valid && over > 0) { /* past the edge: the invisible shoulder, sloping away */
        wq->h += phys.shoulder_drop + SHOULDER_SLOPE * over;
        wq->N = vnorm(vadd(wq->N, vmul(wq->R, wq->lat > 0 ? SHOULDER_SLOPE : -SHOULDER_SLOPE)));
    }

    float cosd = wq->valid ? vdot(c->U, wq->N) : 0;
    int wall = wq->valid ? climb_face(wq->seg) : -1;
    V3 wall_normal;

    if (wall >= 0 && in_climb_wall(mount, wall, &wall_normal) > 0) {
        wq->valid = 0; /* below the top of a face going up: the wall */
    }

    if (wq->valid && wq->road && over < phys.shoulder_width && cosd > WHEEL_MIN_SURFACE_COS &&
        wq->h > (seg[wq->seg].steep ? WHEEL_BELOW_STEEP_MAX : WHEEL_BELOW_ROAD_MAX)) {
        float d = wq->h / cosd;
        *l = d - WRAD[i];

        if (*l < WLF[i]) {
            contact = 1;
            hub = *l > 0 ? *l : 0;
        }
    }

    c->suspension_len[i] = hub;
    return contact;
}

/* the spring/damper force of wheel i at length l, closing at `rate` m/s: progressive hydraulic bump stop at the end of
 * the travel, never pulling, limited so a sheer face cannot throw the car */
static float wheel_spring_force(Car *c, int i, float l, float rate, const Q *wq)
{
    float compression = WLF[i] - l;
    float spring = i < 2 ? K_F : K_R, damping_scale = sqrtf(spring / DAMPER_REFERENCE_SPRING);
    float force = spring * compression + (rate > 0 ? C_COMP : C_REB) * damping_scale * rate;

    if (l < WLMIN[i]) {
        float x = WLMIN[i] - l;
        force += K_BUMP * x + BUMP_STOP_QUADRATIC * x * x + (rate > 0 ? BUMP_STOP_DAMPING * rate : 0.f);

        if (!c->wheel_bottomed[i] && rate > BOTTOM_OUT_SOUND_RATE) {
            snd_grounded(c, rate); /* bottomed out: the original's grounded sound */
        }

        if (!c->wheel_bottomed[i] && rate > BOTTOM_OUT_DAMAGE_RATE) {
            car_hurt(c, (rate - BOTTOM_OUT_DAMAGE_RATE) * BOTTOM_OUT_DAMAGE_PER_MS, 1.f);
        }

        c->wheel_bottomed[i] = 1;
    } else {
        c->wheel_bottomed[i] = 0;
    }

    force = clampf(force, 0, SPRING_FORCE_MAX);

    if (seg[wq->seg].steep && force > SPRING_FORCE_STEEP_G * CAR_M * GRAV) {
        force = SPRING_FORCE_STEEP_G * CAR_M * GRAV;
    }

    return force;
}

/* the tyre's friction force on the road plane: drive and brake along the wheel, grip across it, together inside the
 * traction circle. normal_force is the spring force pressing the tyre down, r the contact point from the centre of
 * mass, v_contact the velocity there */
static void wheel_tyre_force(Car *c, int i, float dt, V3 N, V3 r, V3 v_contact, float normal_force, float drive,
                             float brake, Loads *ld)
{
    V3 wheel_dir = c->F;

    if (i < 2) {
        wheel_dir = vadd(vmul(c->F, cosf(c->steer_angle)), vmul(c->R, sinf(c->steer_angle)));
    }

    V3 along = vnorm(vsub(wheel_dir, vmul(N, vdot(wheel_dir, N)))), across = vcross(along, N);
    float v_along = vdot(v_contact, along), v_across = vdot(v_contact, across);
    c->wheel_phase[i] = fmodf(c->wheel_phase[i] + v_along * dt / WRAD[i], 2 * PI_F);
    float fx = 0;

    if (i >= 2) {
        fx += drive * REAR_DRIVE_SHARE; /* rear wheel drive */
    }

    fx -= brake * (i < 2 ? phys.brake_force_front : phys.brake_force_rear) * tanhf(v_along * BRAKE_SLIP_SCALE);
    fx -= ROLLING_RESISTANCE * normal_force * tanhf(v_along * ROLLING_SLIP_SCALE); /* rolling resistance */
    float grip = MU * (i < 2 ? 1.f : phys.rear_grip_factor) *
                 fminf(normal_force, phys.tyre_load_max); /* tyre load saturation: landing spikes don't add grip */
    float fy = -grip * tanhf(phys.cornering_stiffness * v_across / (fabsf(v_along) + phys.low_speed_slip_offset));
    float magnitude = sqrtf(fx * fx + fy * fy);

    if (magnitude > grip && magnitude > 1e-3f) {
        float k = grip / magnitude;
        fx *= k;
        fy *= k;
    }

    V3 friction = vadd(vmul(along, fx), vmul(across, fy));
    ld->force = vadd(ld->force, friction);
    /* tyre forces act near CoM height: no roll-over from grip */
    V3 arm = vsub(r, vmul(c->U, TYRE_FORCE_HEIGHT * vdot(r, c->U)));
    ld->torque = vadd(ld->torque, vcross(arm, friction));
}

/* all four wheels: suspension and tyre forces for those on the road; returns how many touch it */
static int wheels_step(Car *c, float dt, float drive, float brake, Loads *ld)
{
    int touching = 0;

    for (int i = 0; i < 4; i++) {
        V3 mount = body_pt(c, WMOUNT[i]);
        Q wq;
        float l;
        int was = c->wheel_contact[i];
        int contact = wheel_probe(c, i, mount, &wq, &l);
        c->wheel_contact[i] = contact;

        if (!contact) {
            c->wheel_bottomed[i] = 0;
            continue;
        }

        touching++;
        V3 N = wq.N, contact_point_world = vsub(mount, vmul(c->U, l + WRAD[i]));
        V3 r = vsub(contact_point_world, c->x), v_contact = vadd(c->v, vcross(c->w, r));
        float rate = -vdot(v_contact, N);

        if (!was && rate > TOUCHDOWN_SOUND_RATE) {
            snd_grounded(c, rate); /* hard touchdown */
        }

        float spring_force = wheel_spring_force(c, i, l, rate, &wq);
        V3 spring = vmul(N, spring_force);
        ld->force = vadd(ld->force, spring);
        ld->torque = vadd(ld->torque, vcross(r, spring));
        wheel_tyre_force(c, i, dt, N, r, v_contact, spring_force, drive, brake, ld);
    }

    return touching;
}

/* landing beside the track or caught in the catch net is a crash */
static void car_ground_and_net(Car *c, Loads *ld)
{
    int touch = 0, net = 0;
    float impact = ground_contact(c, &ld->force, &ld->torque, &touch);

    if (touch && c == &cars[0]) {
        sfx.in_dirt = 1;
    }

    impact = fmaxf(impact, 0.5f * fence_contact(c, &ld->force, &ld->torque, &net)); /* the net gives: a softer hit */

    if ((touch || net) && c->crash_time <= 0) {
        c->crash_time = 1e-4f;
        stats.falls++;
        car_hurt(c, CRASH_DAMAGE_BASE + CRASH_DAMAGE_PER_IMPACT * impact, 1.f);
        dust_burst(c->x, 36, 3.f + 0.3f * impact);

        if (c == &cars[0]) {
            set_msg("CRASHED!", 2.0f);
        }

        if (getenv("SCR_DEBUG") && !planning) {
            fprintf(stderr, "crash car%d seg %d impact %.1f t=%.2f\n", (int)(c - cars), c->seg, impact, racetime);
        }
    }
}

/* air drag, angular damping, the air-control assist in flight and the stability aid on the ground */
static void car_body_forces(Car *c, int wheels_touching, float speed, Loads *ld)
{
    ld->force = vsub(ld->force, vmul(c->v, phys.drag * speed));
    ld->torque = vsub(ld->torque, vmul(c->w, ANGULAR_DRAG));

    if (wheels_touching == 0 &&
        speed > AIR_CONTROL_MIN_SPEED) { /* air-control assist: nose follows the flight path, wings level */
        V3 vn = vmul(c->v, 1.f / speed);
        V3 ax = vcross(c->F, vn); /* axis taking the nose toward the velocity */
        ld->torque = vadd(ld->torque, vsub(vmul(ax, phys.air_control_torque),
                                           vadd(vmul(c->R, AIR_CONTROL_DAMPING * vdot(c->w, c->R)),
                                                vmul(c->U, AIR_CONTROL_DAMPING * vdot(c->w, c->U)))));
        V3 level_up = vnorm(vsub(UP, vmul(c->F, vdot(UP, c->F))));
        ld->torque = vadd(ld->torque, vmul(c->F, AIR_LEVEL_STIFFNESS * vdot(vcross(c->U, level_up), c->F) -
                                                     AIR_ROLL_DAMPING * vdot(c->w, c->F)));
    }

    if (wheels_touching > 0) { /* stability aid while on the ground */
        float g = wheels_touching >= 3 ? 1.f : GROUND_ASSIST_PARTIAL;
        ld->torque = vsub(ld->torque, vmul(c->F, g * phys.roll_damping * vdot(c->w, c->F)));
        ld->torque = vsub(ld->torque, vmul(c->U, g * phys.yaw_damping * vdot(c->w, c->U)));
        ld->torque = vsub(ld->torque, vmul(c->R, g * phys.pitch_damping * vdot(c->w, c->R)));
    }
}

/* exhaust flames and the body lean: purely visual, but driven by the forces */
static void car_visual_motion(Car *c, int wheels_touching, V3 force, float dt)
{
    /* exhaust flames: only while the boost burns */
    float flame_target = (c->boosting && c->boost > 0 && c->throttle > 0 && !c->wrecked) ? FLAME_STRENGTH : 0.f;
    c->flame += (flame_target - c->flame) * clampf(FLAME_RATE * dt, 0, 1);

    /* body lean: rocks the cab on its springs so cornering/braking/landing are visible */
    float accel_side = vdot(force, c->R) / CAR_M, accel_fwd = vdot(force, c->F) / CAR_M;
    float k = clampf(LEAN_FILTER_RATE * dt, 0, 1);
    c->lateral_accel += (clampf(accel_side, -LEAN_LATERAL_ACCEL_MAX, LEAN_LATERAL_ACCEL_MAX) - c->lateral_accel) * k;
    c->longitudinal_accel +=
        (clampf(accel_fwd, -LEAN_LONGITUDINAL_ACCEL_MAX, LEAN_LONGITUDINAL_ACCEL_MAX) - c->longitudinal_accel) * k;
    float roll_target =
        wheels_touching > 1 ? clampf(-c->lateral_accel * phys.lean_roll_per_accel, -LEAN_ROLL_MAX, LEAN_ROLL_MAX) : 0.f;
    float pitch_target =
        wheels_touching > 1 ? clampf(c->longitudinal_accel * phys.lean_pitch_per_accel, -LEAN_PITCH_MAX, LEAN_PITCH_MAX)
                            : 0.f;
    c->lean_roll_rate += (LEAN_ROLL_SPRING * (roll_target - c->lean_roll) - LEAN_ROLL_DAMPER * c->lean_roll_rate) * dt;
    c->lean_roll += c->lean_roll_rate * dt;
    c->lean_pitch_rate +=
        (LEAN_PITCH_SPRING * (pitch_target - c->lean_pitch) - LEAN_PITCH_DAMPER * c->lean_pitch_rate) * dt;
    c->lean_pitch += c->lean_pitch_rate * dt;
    lean_limit(c);
}

/* applies the loads for dt: velocity, spin and then position and orientation (kept orthonormal); speed_before is the
 * speed the loads were computed at, used for the speed limit */
static void car_integrate(Car *c, const Loads *ld, float speed_before, float dt)
{
    c->v = vadd(c->v, vmul(ld->force, dt / CAR_M));
    V3 local_torque = V(vdot(ld->torque, c->R), vdot(ld->torque, c->U), vdot(ld->torque, c->F));
    c->w = vadd(c->w, vmul(vadd(vmul(c->R, local_torque.x / IX),
                                vadd(vmul(c->U, local_torque.y / IY), vmul(c->F, local_torque.z / IZ))),
                           dt));
    float spin = vlen(c->w);

    if (spin > MAX_SPIN) {
        c->w = vmul(c->w, MAX_SPIN / spin);
        spin = MAX_SPIN;
    }

    if (speed_before > MAX_SPEED) {
        c->v = vmul(c->v, MAX_SPEED / speed_before);
    }

    c->x = vadd(c->x, vmul(c->v, dt));

    if (spin > 1e-6f) {
        V3 axis = vmul(c->w, 1 / spin);
        float angle = spin * dt;
        c->F = rotv(c->F, axis, angle);
        c->U = rotv(c->U, axis, angle);
    }

    c->F = vnorm(c->F);
    c->U = vnorm(vsub(c->U, vmul(c->F, vdot(c->U, c->F))));
    c->R = vcross(c->F, c->U);
}

/* after a crash: smoke and a little fire, then the crane. Returns 1 while the car lies there */
static int car_crashed_step(Car *c, float dt)
{
    if (c->crash_time <= 0) {
        return 0;
    }

    c->crash_time += dt;
    c->dust_timer -= dt;

    if (c->dust_timer <= 0) {
        c->dust_timer = CRASH_SMOKE_INTERVAL;
        crash_smoke(c);
    }

    if (c->crash_time > CRASH_CRANE_DELAY) {
        start_crane(c, restart_seg(c), 0, CRANE_LOWER_TIME);
    }

    return 1;
}

/* a car that lies flipped or stuck for 2.5 s, or has fallen out of the world, goes back to the track by crane */
static void car_check_reset(Car *c, float dt)
{
    c->flipped_time = c->U.y < RESET_FLIPPED_UP_Y && vlen(c->v) < RESET_SLOW_SPEED ? c->flipped_time + dt : 0;
    c->stuck_time = (state == ST_RACE && vlen(c->v) < RESET_STUCK_SPEED && c->throttle > RESET_STUCK_THROTTLE)
                        ? c->stuck_time + dt
                        : 0;

    if (c->flipped_time > RESET_TIME || c->stuck_time > RESET_TIME ||
        c->x.y < ground_h(c->x.x, c->x.z) - RESET_FALL_DEPTH) {
        stats.falls++;
        car_hurt(c, RESET_DAMAGE, 1.f);

        if (getenv("SCR_DEBUG") && !planning) {
            fprintf(stderr, "reset car%d seg %d flip=%.1f stuck=%.1f\n", (int)(c - cars), c->seg, c->flipped_time,
                    c->stuck_time);
        }

        if (c == &cars[0]) {
            set_msg("RESET", 1.5f);
        }

        start_crane(c, restart_seg(c), 0, CRANE_LOWER_TIME);
    }
}

/* one physics step of a car */
static void car_step(Car *c, float dt)
{
    if (!c->active) {
        return;
    }

    if (c->hook_time > 0) {
        c->hook_time -= dt;
    }

    if (c->crane_time > 0) {
        crane_step(c, dt);
        return;
    }

    if (c->wrecked || c->crash_time > 0) {
        c->throttle = 0;
        c->brake = 1;
        c->boosting = 0;
        c->steer_input = 0;
    }

    Q road;
    track_query(c->x, c->seg, 30, &road);

    if (road.valid) {
        car_update_laps(c, &road);
    }

    if (c->hurt_cooldown > 0) {
        c->hurt_cooldown -= dt;
    }

    if (c->smash_time > 0) {
        c->smash_time -= dt;
    }

    Loads ld = {V(0, -CAR_M * GRAV, 0), V(0, 0, 0)};
    float forward_speed = vdot(c->v, c->F);
    car_steer(c, dt, forward_speed);
    float brake;
    float drive = car_drive_force(c, dt, forward_speed, &brake);
    int wheels_touching = wheels_step(c, dt, drive, brake, &ld);
    c->wheels_touching = wheels_touching;

    for (int i = 0; i < 16; i++) {
        contact_point(c, i, CPTS[i], &ld.force, &ld.torque);
    }

    edge_contact(c, &ld.force, &ld.torque);
    car_ground_and_net(c, &ld);
    float speed = vlen(c->v);
    car_body_forces(c, wheels_touching, speed, &ld);
    car_visual_motion(c, wheels_touching, ld.force, dt);
    car_integrate(c, &ld, speed, dt);

    if (wheels_touching >= 3 && road.valid && road.road && fabsf(road.lat) < TRACK_HW - 1.5f) {
        c->safe_segment = road.seg;
    }

    if (car_crashed_step(c, dt)) {
        return;
    }

    car_check_reset(c, dt);
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

            stats.contacts++;
            stats.push += Fn * dt;
            V3 f = vmul(n, Fn);
            a->v = vsub(a->v, vmul(f, dt / CAR_M));
            b->v = vadd(b->v, vmul(f, dt / CAR_M));
            apply_ang(a, vcross(ra, vmul(f, -1)), dt);
            apply_ang(b, vcross(rb, f), dt);

            if (-vn > 1.f && sfx.hitcar_cooldown <= 0 && (snd_live(a) || snd_live(b))) {
                snd_play(S_HITCAR, 238, 56);
                sfx.hitcar_cooldown = 0.5f;
            }

            if (-vn > 3.f) {
                car_hurt(a, 0.015f * (-vn - 3.f), 0.6f);
                car_hurt(b, 0.015f * (-vn - 3.f), 0.6f);
            }
        }
    }
}
