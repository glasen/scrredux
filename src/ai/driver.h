/* ai/driver.h - the computer driver: lane choice and speed control */
#pragma once

/* keep clear of the other car: never steer into it, tuck in behind when there is no room to pass */
static float ai_lane_clear(const Car *c, const Car *o, float lane, float *lift)
{
    *lift = 0;
    if (!o || !o->active || car_out(o)) return lane;
    const Seg *s0 = &seg[c->seg];
    V3 d = vsub(o->x, c->x);
    float dlong = vdot(d, s0->T), mylat = vdot(vsub(c->x, s0->c), s0->R), olat = mylat + vdot(d, s0->R);
    if (dlong < -6.f || dlong > 18.f) return lane;                    /* far behind or well ahead */
    const float need = 3.4f, edge = TRACK_HW - 2.2f;                   /* centre-to-centre room for two cars side by side */
    if (fabsf(lane - olat) >= need) return lane;
    float side = mylat >= olat ? 1.f : -1.f;                           /* stay on the side we are already on */
    float alt = clampf(olat + side * need, -edge, edge);
    if (fabsf(alt - olat) >= need - 0.2f) return alt;
    float other = clampf(olat - side * need, -edge, edge);             /* no room on our side */
    if (dlong > 4.f && fabsf(other - olat) >= need - 0.2f) return other;   /* pass on the other side */
    if (dlong > -1.f) *lift = 1;                                       /* alongside or behind: back off */
    return alt;
}

static void ai_drive(Car *c, float lane, float vcap, float alat, const Car *rival)
{
    float spd = vdot(c->v, c->F), lift = 0;
    lane = ai_lane_clear(c, rival, lane, &lift);
    int gapsoon = 0, gn = (int)(110 / ds);
    for (int k = -(int)(25 / ds); k < gn; k++) { const Seg *sk = &seg[((c->seg + k) % nseg + nseg) % nseg]; if (sk->gap || sk->steep) gapsoon = 1; }
    const Seg *s0 = &seg[c->seg];
    if (gapsoon) lane = clampf(vdot(vsub(c->x, s0->c), s0->R), -1.5f, 1.5f);   /* hold the lane through a jump, well in from the edges */
    if (jump_v[c->seg] > 0) lane = clampf(lane, -2.2f, 2.2f);                 /* a run-up is driven near the middle, whoever is about */
    if (!c->ai_lset) { c->ai_lane = vdot(vsub(c->x, s0->c), s0->R); c->ai_lset = 1; }
    c->ai_lane += clampf(lane - c->ai_lane, -2.f / 240.f, 2.f / 240.f);       /* change lanes at 2 m/s, never with a jerk */
    lane = c->ai_lane;
    V3 Th = vnorm(V(s0->T.x, 0, s0->T.z)), Rt = vnorm(vcross(Th, UP));
    V3 Fh = vnorm(V(c->F.x, 0, c->F.z));
    float e = vdot(vsub(c->x, s0->c), s0->R) - lane;                 /* cross-track error (+ = right of lane) */
    float hdg = atan2f(vdot(Fh, Rt), vdot(Fh, Th));                 /* heading error (+ = pointing right) */
    int jf = (c->seg + (int)((6 + fmaxf(spd, 0) * 0.3f) / ds)) % nseg;
    V3 Tf = vnorm(V(seg[jf].T.x, 0, seg[jf].T.z));
    float kf = vdot(vsub(Tf, Th), Rt) / (((jf - c->seg + nseg) % nseg + 1) * ds);
    float delta = -0.55f * hdg - atanf(2.2f * e / (fmaxf(spd, 0) + 4.f)) + atanf(3.4f * kf);
    float gl = -GRAV * c->R.y, dmax = steer_max(spd, delta >= 0 ? gl : -gl);
    float dlim = 3.2f / (fmaxf(spd, 0) + 5.f);
    c->str = c->nwc == 0 ? 0.f : clampf(clampf(delta, -dlim, dlim) / dmax, -1.f, 1.f);
    float vmax = 0, vmin = 0, vbend = 1e9f;
    int n = (int)((fmaxf(spd, 5) * 2.0f + 20) / ds);
    for (int k = 0; k <= n; k++) {
        int sk = (c->seg + k) % nseg; const Seg *s = &seg[sk];
        float cap = sqrtf(fmaxf(alat + GRAV * s->bin, 3.f) / fmaxf(s->curv, 1e-4f));   /* banking holds the car in */
        vbend = fminf(vbend, fmaxf(cap, jump_v[sk]));                  /* the bend before a jump is taken near the limit */
        vmax = fmaxf(vmax, s->vcurv); vmin = fminf(vmin, s->vcurv);
    }
    float vt = fminf(vcap, vbend), jv = jump_v[c->seg];
    if (jv > 0) vt = jv;                                              /* run-up: flat out, then down to the take-off speed */
    else if (!gapsoon) {                                              /* a little float over crests, none out wide where the deck may twist */
        float fl = fabsf(vdot(vsub(c->x, s0->c), s0->R)) > 1.6f ? 0.95f : 1.1f;
        vt = fminf(fminf(vt, fl * sqrtf(GRAV / fmaxf(vmax, 1e-4f))), sqrtf(26.f / fmaxf(-vmin, 1e-4f)));
    }
    else vt = clampf(vt, 30.f, 34.f);
    if (spd < vt) { c->thr = clampf((vt - spd) * (jv > 0 ? 2.f : 0.5f), 0.2f, 1.f); c->brk = 0; }
    else if (jv > 0 ? spd > vt + 1.f : spd > vt * 1.06f && !gapsoon) { c->thr = 0; c->brk = clampf((spd - vt) * 0.15f, 0.15f, 1.f); }
    else { c->thr = 0.3f; c->brk = 0; }
    if (spd > 8.f) c->thr *= fmaxf(0.25f, 1.f - 0.85f * fabsf(c->str));
    if (lift && jv <= 0) { c->thr = 0; if (spd > 6.f) c->brk = fmaxf(c->brk, 0.25f); }
    if (c->nwc < 3) c->brk = 0;                                       /* no braking in the air or on an unsettled landing */
    else c->brk *= 1.f - 0.7f * fabsf(c->str);                        /* leave the tyres grip to steer with */
    c->bst = jump_b[c->seg] && spd < jv - 1.f;
}
