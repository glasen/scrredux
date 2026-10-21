/* game/race.h - starting races, laps, replay stepping and camera-car selection */
#pragma once

/* ------------------------------------------------------------ game logic */
static int autoplay;

static const char *player_name(void)
{
    return sea.pname[0] ? sea.pname : "Player";
}

/* season races: three laps against one opponent; practice and the builder's test drive: alone, laps until ESC */
static void start_race(void)
{
    plan_need();
    int solo = race_mode == RM_PRACTICE || race_mode == RM_TEST;

    if (race_mode == RM_SEASON) {
        laps = 3;
    } else if (solo) {
        laps = 0;
    }

    for (int i = 0; i < 2; i++) {
        Car *c = &cars[i];
        memset(c, 0, sizeof *c);

        if (i && solo) {
            continue;
        }

        c->active = 1;
        c->boost = 100;
        c->col = i ? 0x2860D8 : CAR_RED;
        start_crane(c, nseg - 14, solo ? 0.f : i ? 2.0f : -2.0f, 2.4f);
    }

    if (race_mode == RM_SEASON) {
        cars[0].nholes = sea.nholes;
    }

    npart = 0;
    racetime = 0;
    state = ST_COUNT;
    state_t = 3.99f;
    result = 0;
    msg_t = 0;
    stat_falls = 0;
    paused = 0;

    if (race_mode == RM_SEASON) {
        rec_begin();
    } else {
        rec_on = 0;
    }
}

/* a lap in a season race or in practice: the Hall of Fame's lap record */
static void lap_done(Car *c)
{
    if (planning || (race_mode != RM_SEASON && race_mode != RM_PRACTICE)) {
        return;
    }

    int me = c == &cars[0];

    if (hof_lap(trk[cur_track].name, me ? player_name() : drv_name(opp_drv), c->lastlap) && me) {
        set_msg("NEW LAP RECORD!", 2.5f);
    }
}

/* replay: the recorded cars at the replay clock; crashes smoke again */
static void rp_step(float dt)
{
    if (!rp_paused) {
        rp_t += dt * rp_speed;
    }

    float tmax = (nrp - 1) / (float)RP_HZ;
    rp_t = clampf(rp_t, 0, tmax);
    int k = (int)(rp_t * RP_HZ);

    if (k >= nrp) {
        k = nrp - 1;
    }

    const RTick *t = &rpb[k];

    for (int i = 0; i < 2; i++) {
        Car *c = &cars[i];
        float was = c->crash;
        rec_unpack(c, &t->c[i]);

        if (rp_paused || rp_speed < 0) {
            continue;
        }

        if (c->crash > 0 && was <= 0) {
            dust_burst(c->x, 36, 4.f);
        }

        if (c->crash > 0 && (rp_smoke[i] -= dt) <= 0) {
            rp_smoke[i] = 0.12f;
            crash_smoke(c);
        }
    }

    racetime = t->t;
}

/* the car seen from its driver's seat, -1 for none */
static int fp_car(void)
{
    if (state == ST_COUNT || state == ST_RACE || state == ST_DONE) {
        return cockpit ? 0 : -1;
    }

    if (state == ST_REPLAY) {
        return rp_cam == RC_COCKPIT ? 0 : rp_cam == RC_OPP_COCKPIT && cars[1].active ? 1 : -1;
    }

    return -1;
}

static int cam_car(void)
{
    return state == ST_REPLAY && (rp_cam == RC_OPP_COCKPIT || rp_cam == RC_OPP_CHASE) && cars[1].active ? 1 : 0;
}
