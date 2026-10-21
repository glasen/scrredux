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
                cars[i].thr = 0;
                cars[i].brk = 1;
                cars[i].hold = 1;
                car_step(&cars[i], dt);
            }

            if (state_t <= 0) {
                state = ST_RACE;
                racetime = 0;
                cars[0].hold = cars[1].hold = 0;
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

                float rate = (tg != 0 && p->str * tg >= 0) ? ph.keyrise
                                                           : ph.keyret; /* tap = small correction, hold = more lock */
                float goal = tg != 0 ? tg : 0.f, dstr = goal - p->str, stp = rate * dt;
                p->str += clampf(dstr, -stp, stp);
                p->thr = ctl_down(CT_ACCEL) ? 1.f : pi.thr;
                p->brk = ctl_down(CT_BRAKE) ? 1.f : pi.brk;
                p->bst = ctl_down(CT_BOOST) || pi.bst;
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
                    p->bst = 1;
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
                } else if (p->finished && (!o->finished || p->ftime <= o->ftime)) {
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

            if (view_auto) {
                view_yaw += 0.35f * dt;
            }

            if (keys) {
                float kx = (keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D]) -
                           (keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A]);
                float ky =
                    (keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W]) - (keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S]);
                float kz = (keys[SDL_SCANCODE_EQUALS] || keys[SDL_SCANCODE_KP_PLUS]) -
                           (keys[SDL_SCANCODE_MINUS] || keys[SDL_SCANCODE_KP_MINUS]);
                view_yaw += kx * 1.6f * dt;
                view_pitch += ky * 0.9f * dt;
                view_dist *= 1.f - kz * 0.8f * dt;

                if (kx != 0) {
                    view_auto = 0;
                }
            }

            view_pitch = clampf(view_pitch, -0.15f, 1.45f);
            view_dist = clampf(view_dist, 0.9f, 22.f);
            state_t += dt;

            if (view_anim) {
                c->delta = 0.30f * sinf(state_t * 0.9f); /* steering demo */
                for (int i = 0; i < 4; i++) {
                    c->wph[i] += dt * 1.8f; /* wheels turn slowly */
                }

                for (int i = 0; i < 4; i++) { /* suspension demo: each wheel works through its travel */
                    float sn = sinf(state_t * 1.1f + i * 1.7f), tr = WREST[i] - WLMIN[i];
                    c->wl[i] = WREST[i] - sn * (sn > 0 ? tr : WLF[i] - WREST[i]) * 0.9f;
                }
            } else { /* stopped: straight ahead at ride height */
                c->delta = 0;

                for (int i = 0; i < 4; i++) {
                    c->wl[i] = WREST[i];
                }
            }

            c->flame = view_flame ? 1.f : 0.f;
            break;
        }
        case ST_DONE:
            state_t += dt;

            for (int i = 0; i < 2; i++) {
                cars[i].thr = 0;
                cars[i].brk = 0.4f;
                cars[i].hold = 1;
                cars[i].str = 0;
                cars[i].bst = 0;
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
        if (cars[i].lapshow > 0) {
            cars[i].lapshow -= dt;
        }
    }

    parts_step(dt);
    snd_tick(dt);
}
