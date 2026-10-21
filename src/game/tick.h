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

static void game_tick(float dt)
{
    switch (state) {
        case ST_MENU:
            orbit_t += dt;
            break;
        case ST_COUNT: {
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

            break;
        }
        case ST_RACE: {
            racetime += dt;
            Car *p = &cars[0], *o = &cars[1];

            if (keys && !autoplay) {
                PadIn pi = pad_read();
                float tg = ctl_down(CT_RIGHT) - ctl_down(CT_LEFT);

                if (tg == 0) {
                    tg = pi.steer; /* a stick steers part way */
                }

                float rate = (tg != 0 && p->steer_input * tg >= 0)
                                 ? phys.key_steer_rise
                                 : phys.key_steer_return; /* tap = small correction, hold = more lock */
                float goal = tg != 0 ? tg : 0.f, dstr = goal - p->steer_input, stp = rate * dt;
                p->steer_input += clampf(dstr, -stp, stp);
                p->throttle = ctl_down(CT_ACCEL) ? 1.f : pi.throttle;
                p->brake = ctl_down(CT_BRAKE) ? 1.f : pi.brake;
                p->boosting = ctl_down(CT_BOOST) || pi.boosting;
            } else {
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
            } /* autopilot ignores the rival: worst case */
            if (o->active) {
                ai_drive(o, 1.8f, opp_vcap, opp_alat, p);
            }

            car_step(p, dt);
            car_step(o, dt);
            cars_interact(p, o, dt);

            if (p->finished || o->finished || p->wrecked || o->wrecked) {
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
            break;
        }
        case ST_VIEW: {
            Car *c = &cars[0];

            if (viewer.auto_orbit) {
                viewer.yaw += 0.35f * dt;
            }

            if (keys) {
                float kx = (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) -
                           (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]);
                float ky =
                    (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) - (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]);
                float kz = (keys[SDL_SCANCODE_EQUALS] || keys[SDL_SCANCODE_KP_PLUS]) -
                           (keys[SDL_SCANCODE_MINUS] || keys[SDL_SCANCODE_KP_MINUS]);
                viewer.yaw += kx * 1.6f * dt;
                viewer.pitch += ky * 0.9f * dt;
                viewer.distance *= 1.f - kz * 0.8f * dt;

                if (kx != 0) {
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
                    float sn = sinf(state_t * 1.1f + i * 1.7f), tr = WREST[i] - WLMIN[i];
                    c->suspension_len[i] = WREST[i] - sn * (sn > 0 ? tr : WLF[i] - WREST[i]) * 0.9f;
                }
            } else { /* stopped: straight ahead at ride height */
                c->steer_angle = 0;

                for (int i = 0; i < 4; i++) {
                    c->suspension_len[i] = WREST[i];
                }
            }

            c->flame = viewer.flame ? 1.f : 0.f;
            break;
        }
        case ST_DONE:
            state_t += dt;

            for (int i = 0; i < 2; i++) {
                cars[i].throttle = 0;
                cars[i].brake = 0.4f;
                cars[i].held_at_start = 1;
                cars[i].steer_input = 0;
                cars[i].boosting = 0;
                car_step(&cars[i], dt);
            }

            break;
        case ST_REPLAY:
            rp_step(dt);
            break;
        case ST_EDIT:
            ed_tick(dt);
            break;
        case ST_PREVIEW:
            orbit_t += dt;
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
