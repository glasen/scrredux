/* ai/driver.h - the computer driver: lane choice and speed control */
#pragma once

/* keep clear of the other car: never steer into it, tuck in behind when there is no room to pass */
static float ai_lane_clear(const Car *c, const Car *o, float lane, float *lift)
{
    *lift = 0;
    if (!o || !o->active || car_out(o)) {
        return lane;
    }

    const Seg *s0 = &seg[c->seg];
    V3 d = vsub(o->x, c->x);
    float dlong = vdot(d, s0->T), mylat = vdot(vsub(c->x, s0->c), s0->R), olat = mylat + vdot(d, s0->R);

    if (dlong < -AI_RIVAL_BEHIND || dlong > AI_RIVAL_AHEAD) {
        return lane; /* far behind or well ahead */
    }

    const float need = AI_PASS_ROOM,
                edge = TRACK_HW - AI_EDGE_MARGIN; /* centre-to-centre room for two cars side by side */
    if (fabsf(lane - olat) >= need) {
        return lane;
    }

    float side = mylat >= olat ? 1.f : -1.f; /* stay on the side we are already on */
    float alt = clampf(olat + side * need, -edge, edge);

    if (fabsf(alt - olat) >= need - AI_PASS_ROOM_SLACK) {
        return alt;
    }

    float other = clampf(olat - side * need, -edge, edge); /* no room on our side */
    if (dlong > AI_PASS_OTHER_SIDE_AHEAD && fabsf(other - olat) >= need - AI_PASS_ROOM_SLACK) {
        return other; /* pass on the other side */
    }

    if (dlong > AI_BACK_OFF_AHEAD) {
        *lift = 1; /* alongside or behind: back off */
    }

    return alt;
}

static void ai_drive(Car *c, float lane, float vcap, float alat, const Car *rival)
{
    float spd = vdot(c->v, c->F), lift = 0;
    lane = ai_lane_clear(c, rival, lane, &lift);
    int gapsoon = 0, gn = (int)(AI_JUMP_LOOK_AHEAD / ds);

    for (int k = -(int)(AI_JUMP_LOOK_BEHIND / ds); k < gn; k++) {
        const Seg *sk = &seg[((c->seg + k) % nseg + nseg) % nseg];

        if (sk->gap || sk->steep) {
            gapsoon = 1;
        }
    }

    const Seg *s0 = &seg[c->seg];

    if (gapsoon) {
        lane = clampf(vdot(vsub(c->x, s0->c), s0->R), -AI_JUMP_LANE_LIMIT,
                      AI_JUMP_LANE_LIMIT); /* hold the lane through a jump, well in from the edges */
    }

    if (jump_v[c->seg] > 0) {
        lane = clampf(lane, -AI_RUNUP_LANE_LIMIT,
                      AI_RUNUP_LANE_LIMIT); /* a run-up is driven near the middle, whoever is about */
    }

    if (!c->ai_lane_set) {
        c->ai_lane = vdot(vsub(c->x, s0->c), s0->R);
        c->ai_lane_set = 1;
    }

    c->ai_lane += clampf(lane - c->ai_lane, -AI_LANE_CHANGE_RATE / 240.f,
                         AI_LANE_CHANGE_RATE / 240.f); /* change lanes at 2 m/s, never with a jerk */
    lane = c->ai_lane;
    V3 Th = vnorm(V(s0->T.x, 0, s0->T.z)), Rt = vnorm(vcross(Th, UP));
    V3 Fh = vnorm(V(c->F.x, 0, c->F.z));
    float e = vdot(vsub(c->x, s0->c), s0->R) - lane; /* cross-track error (+ = right of lane) */
    float hdg = atan2f(vdot(Fh, Rt), vdot(Fh, Th));  /* heading error (+ = pointing right) */
    int jf = (c->seg + (int)((AI_CURVE_LOOK_BASE + fmaxf(spd, 0) * AI_CURVE_LOOK_TIME) / ds)) % nseg;
    V3 Tf = vnorm(V(seg[jf].T.x, 0, seg[jf].T.z));
    float kf = vdot(vsub(Tf, Th), Rt) / (((jf - c->seg + nseg) % nseg + 1) * ds);
    float delta = -AI_HEADING_GAIN * hdg -
                  atanf(AI_CROSSTRACK_GAIN * e / (fmaxf(spd, 0) + AI_CROSSTRACK_SPEED_OFFSET)) +
                  atanf(AI_CURVATURE_GAIN * kf);
    float gl = -GRAV * c->R.y, dmax = steer_max(spd, delta >= 0 ? gl : -gl);
    float dlim = AI_STEER_LIMIT / (fmaxf(spd, 0) + AI_STEER_LIMIT_SPEED);
    c->steer_input = c->wheels_touching == 0 ? 0.f : clampf(clampf(delta, -dlim, dlim) / dmax, -1.f, 1.f);
    float vmax = 0, vmin = 0, vbend = 1e9f;
    int n = (int)((fmaxf(spd, AI_SPEED_LOOK_MIN_SPEED) * AI_SPEED_LOOK_TIME + AI_SPEED_LOOK_BASE) / ds);

    for (int k = 0; k <= n; k++) {
        int sk = (c->seg + k) % nseg;
        const Seg *s = &seg[sk];
        float cap = sqrtf(fmaxf(alat + GRAV * s->bin, AI_MIN_BEND_ACCEL) /
                          fmaxf(s->curv, 1e-4f));     /* banking holds the car in */
        vbend = fminf(vbend, fmaxf(cap, jump_v[sk])); /* the bend before a jump is taken near the limit */
        vmax = fmaxf(vmax, s->vcurv);
        vmin = fminf(vmin, s->vcurv);
    }

    float vt = fminf(vcap, vbend), jv = jump_v[c->seg];

    if (jv > 0) {
        vt = jv;           /* run-up: flat out, then down to the take-off speed */
    } else if (!gapsoon) { /* a little float over crests, none out wide where the deck may twist */
        float fl = fabsf(vdot(vsub(c->x, s0->c), s0->R)) > AI_WIDE_LANE ? AI_CREST_FLOAT_WIDE : AI_CREST_FLOAT;
        vt = fminf(fminf(vt, fl * sqrtf(GRAV / fmaxf(vmax, 1e-4f))), sqrtf(AI_DIP_ACCEL / fmaxf(-vmin, 1e-4f)));
    } else {
        vt = clampf(vt, AI_JUMP_ZONE_SPEED_MIN, AI_JUMP_ZONE_SPEED_MAX);
    }

    if (spd < vt) {
        c->throttle = clampf((vt - spd) * (jv > 0 ? AI_RUNUP_THROTTLE_GAIN : AI_THROTTLE_GAIN), AI_THROTTLE_MIN, 1.f);
        c->brake = 0;
    } else if (jv > 0 ? spd > vt + AI_RUNUP_OVERSPEED : spd > vt * AI_OVERSPEED && !gapsoon) {
        c->throttle = 0;
        c->brake = clampf((spd - vt) * AI_BRAKE_GAIN, AI_BRAKE_MIN, 1.f);
    } else {
        c->throttle = AI_CRUISE_THROTTLE;
        c->brake = 0;
    }

    if (spd > AI_STEER_LIFT_SPEED) {
        c->throttle *= fmaxf(AI_STEER_LIFT_MIN, 1.f - AI_STEER_LIFT_FACTOR * fabsf(c->steer_input));
    }

    if (lift && jv <= 0) {
        c->throttle = 0;

        if (spd > AI_LIFT_BRAKE_SPEED) {
            c->brake = fmaxf(c->brake, AI_LIFT_BRAKE);
        }
    }

    if (c->wheels_touching < 3) {
        c->brake = 0; /* no braking in the air or on an unsettled landing */
    } else {
        c->brake *= 1.f - AI_STEER_BRAKE_RELIEF * fabsf(c->steer_input); /* leave the tyres grip to steer with */
    }
    c->boosting = jump_b[c->seg] && spd < jv - AI_BOOST_MARGIN;
}
