/* car/car_state.h - car placement and damage */
#pragma once

static V3 body_pt(const Car *c, V3 l)
{
    return vadd(c->x, vadd(vmul(c->R, l.x), vadd(vmul(c->U, l.y), vmul(c->F, l.z))));
}

static void car_place(Car *c, int s, float lat, float speed)
{
    s = ((s % nseg) + nseg) % nseg;
    c->F = seg[s].T;
    c->U = seg[s].N;
    c->R = vcross(c->F, c->U);
    c->x = vadd(vadd(seg[s].c, vmul(seg[s].R, lat)), vmul(seg[s].N, PLACE_H));
    c->v = vmul(seg[s].T, speed);
    c->w = V(0, 0, 0);
    c->seg = s;
    c->safe = s;
    c->delta = 0;
    c->flip_t = c->stuck_t = 0;

    for (int i = 0; i < 4; i++) {
        c->wl[i] = WLF[i];
    }
}

static void car_hurt(Car *c, float amt, float loud)
{
    if (c->wrecked || amt <= 0) {
        return;
    }

    if (c->hurt_cd > 0) {
        amt *= 0.12f;
    } else {
        c->hurt_cd = 0.3f;
    }
    float x = c->damage, left = amt; /* the crack crosses holes for free */
    while (left > 0) {
        float next = 1e9f;
        int in = 0;

        for (int k = 0; k < c->nholes && !in; k++) {
            float a = hole_at(k);

            if (x >= a && x < a + HOLE_W) {
                x = a + HOLE_W;
                in = 1;
            } else if (a > x && a < next) {
                next = a;
            }
        }

        if (!in) {
            float st = fminf(left, next - x);
            x += st;
            left -= st;
        }
    }
    c->damage = x;

    if (snd_live(c)) {
        pad_rumble(0.3f + amt * 0.06f, (int)fminf(120 + amt * 25, 600)); /* hits, crashes, the net: the pad shakes */
    }

    if (getenv("SCR_DEBUG") && amt > 1.5f && c == &cars[0]) {
        fprintf(stderr, "hurt %.1f seg %d u=%.2f v=%.1f nwc=%d vy=%.1f\n", amt, c->seg, seg[c->seg].u, vlen(c->v),
                c->nwc, c->v.y);
    }

    if (c->damage >= 100) {
        c->damage = 100;
        c->wrecked = 1;
    }

    int smash = amt >= 8.f && loud > 0 && c->smash_t <= 0 &&
                c->nholes < NHOLE; /* big hit: a hole, then a while with only creaks (the original's 69 frames) */
    if (smash) {
        c->smash_t = 6.9f;
        c->nholes++;
    }

    if (snd_live(c) && loud > 0) {
        if (smash) {
            snd_play(S_SMASH, 280, 64);
        } else if (amt >= 0.5f) {
            snd_play(S_CREAK, 238, (int)clampf(amt * 10.f, 28, 64));
        }
    }
}
