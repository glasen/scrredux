/* game/tick.h - one game tick */
#pragma once

static void setup_menu(void)
{
    if (cur_track != sel_track) {
        build_track(sel_track);
    }

    memset(cars, 0, sizeof cars);
    npart = 0;
    paused = 0;
    state = ST_MENU;
}

static void ed_tick(float dt);

/* three, two, one: the cars wait on the line with the brakes on */
static void tick_countdown(float dt)
{
    int before = (int)ceilf(state_t);
    state_t -= dt;

    if ((int)ceilf(state_t) != before && state_t > 0) {
        snd_play(S_CHIME, 150, 40);
    }

    for (int i = 0; i < 2; i++) {
        cars[i].throttle = 0;
        cars[i].brake = 1;
        cars[i].held_at_start = 1;
        car_step(&cars[i], dt);
    }

    if (state_t <= 0) {
        state = ST_RACE;
        racetime = 0;
        cars[0].held_at_start = cars[1].held_at_start = 0;
        snd_play(S_CHIME, 75, 40);
        set_msg("GO!", 1.0f);
    }
}

/* the player's keys and pad drive the car */
static void player_input(Car *p, float dt)
{
    PadIn pad = pad_read();
    float steer_goal = ctl_down(CT_RIGHT) - ctl_down(CT_LEFT);

    if (steer_goal == 0) {
        steer_goal = pad.steer; /* a stick steers part way */
    }

    /* tap = small correction, hold = more lock */
    float rate = (steer_goal != 0 && p->steer_input * steer_goal >= 0) ? phys.key_steer_rise : phys.key_steer_return;
    float max_change = rate * dt;
    p->steer_input += clampf(steer_goal - p->steer_input, -max_change, max_change);
    p->throttle = ctl_down(CT_ACCEL) ? 1.f : pad.throttle;
    p->brake = ctl_down(CT_BRAKE) ? 1.f : pad.brake;
    p->boosting = ctl_down(CT_BOOST) || pad.boosting;
}

/* the autopilot drives the player's car (--sim, SCR_AUTO); it ignores the rival: worst case. SCR_PUSH and SCR_PLANE
 * tune it for experiments */
static void autopilot_input(Car *p)
{
    static float push = -1;

    if (push < 0) {
        push = getenv("SCR_PUSH") ? (float)atof(getenv("SCR_PUSH")) : 1.f;
    }

    static float plane = -9;

    if (plane < -8) {
        plane = getenv("SCR_PLANE") ? (float)atof(getenv("SCR_PLANE")) : -1.f;
    }

    ai_drive(p, plane, 67 * push, 13.f * push, NULL);

    if (push > 1.f) {
        p->boosting = 1;
    }
}

/* the race ends when a car finishes or is wrecked: who won decides `result` (negative: the player lost) */
static void race_check_finished(const Car *p, const Car *o)
{
    if (!(p->finished || o->finished || p->wrecked || o->wrecked)) {
        return;
    }

    state = ST_DONE;
    state_t = 0;

    if (p->wrecked) {
        result = -2;
    } else if (o->wrecked) {
        result = 2;
    } else if (p->finished && (!o->finished || p->finish_time <= o->finish_time)) {
        result = 1;
    } else {
        result = -1;
    }

    snd_play(S_CHIME, result > 0 ? 100 : 300, 40);
}

static void tick_race(float dt)
{
    racetime += dt;
    Car *p = &cars[0], *o = &cars[1];

    if (keys && !autoplay) {
        player_input(p, dt);
    } else {
        autopilot_input(p);
    }

    if (o->active) {
        ai_drive(o, 1.8f, opp_vcap, opp_alat, p);
    }

    car_step(p, dt);
    car_step(o, dt);
    cars_interact(p, o, dt);
    race_check_finished(p, o);
}

/* model viewer: orbit camera from the keys, and the car demonstrating its steering and suspension */
static void tick_model_viewer(float dt)
{
    Car *c = &cars[0];

    if (viewer.auto_orbit) {
        viewer.yaw += 0.35f * dt;
    }

    if (keys) {
        float turn =
            (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) - (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]);
        float tilt =
            (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) - (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]);
        float zoom = (keys[SDL_SCANCODE_EQUALS] || keys[SDL_SCANCODE_KP_PLUS]) -
                     (keys[SDL_SCANCODE_MINUS] || keys[SDL_SCANCODE_KP_MINUS]);
        viewer.yaw += turn * 1.6f * dt;
        viewer.pitch += tilt * 0.9f * dt;
        viewer.distance *= 1.f - zoom * 0.8f * dt;

        if (turn != 0) {
            viewer.auto_orbit = 0;
        }
    }

    viewer.pitch = clampf(viewer.pitch, -0.15f, 1.45f);
    viewer.distance = clampf(viewer.distance, 0.9f, 22.f);
    state_t += dt;

    if (viewer.animate) {
        c->steer_angle = 0.30f * sinf(state_t * 0.9f); /* steering demo */

        for (int i = 0; i < 4; i++) {
            c->wheel_phase[i] += dt * 1.8f; /* wheels turn slowly */
        }

        for (int i = 0; i < 4; i++) { /* suspension demo: each wheel works through its travel */
            float wave = sinf(state_t * 1.1f + i * 1.7f), bump_travel = WREST[i] - WLMIN[i];
            c->suspension_len[i] = WREST[i] - wave * (wave > 0 ? bump_travel : WLF[i] - WREST[i]) * 0.9f;
        }
    } else { /* stopped: straight ahead at ride height */
        c->steer_angle = 0;

        for (int i = 0; i < 4; i++) {
            c->suspension_len[i] = WREST[i];
        }
    }

    c->flame = viewer.flame ? 1.f : 0.f;
}

/* after the finish: the cars roll to a stop */
static void tick_finished(float dt)
{
    state_t += dt;

    for (int i = 0; i < 2; i++) {
        cars[i].throttle = 0;
        cars[i].brake = 0.4f;
        cars[i].held_at_start = 1;
        cars[i].steer_input = 0;
        cars[i].boosting = 0;
        car_step(&cars[i], dt);
    }
}

static void game_tick(float dt)
{
    switch (state) {
        case ST_MENU:
        case ST_PREVIEW:
            orbit_t += dt;
            break;
        case ST_COUNT:
            tick_countdown(dt);
            break;
        case ST_RACE:
            tick_race(dt);
            break;
        case ST_VIEW:
            tick_model_viewer(dt);
            break;
        case ST_DONE:
            tick_finished(dt);
            break;
        case ST_REPLAY:
            rp_step(dt);
            break;
        case ST_EDIT:
            ed_tick(dt);
            break;
    }

    if (state == ST_COUNT || state == ST_RACE || state == ST_DONE) {
        rec_tick();
    }

    if (msg_t > 0) {
        msg_t -= dt;
    }

    for (int i = 0; i < 2; i++) {
        if (cars[i].lap_banner_time > 0) {
            cars[i].lap_banner_time -= dt;
        }
    }

    parts_step(dt);
    snd_tick(dt);
}
