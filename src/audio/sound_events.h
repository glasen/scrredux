/* audio/sound_events.h - when the original's samples play */
#pragma once

/* ---- sound: when the original's samples play (StuntCarRacer.s, Car_Behaviour.cpp), periods and volumes from its tables ---- */
/* what the sound triggers keep between frames */
static struct {
    int engine_revs, engine_change, engine_fluct, engine_on; /* the original's engine.revs model */
    float timer_10hz, timer_50hz;                            /* the original updates sounds at 10 and 50 Hz */
    float grounded_cooldown, hitcar_cooldown;
    int scraping, in_dirt; /* player since the last 10 Hz frame: steel scraping the deck / sliding in the dirt */
    uint32_t rng;
} sfx = {.rng = 1};

static int snd_rand(void)
{
    sfx.rng = sfx.rng * 1664525u + 1013904223u;
    return (int)(sfx.rng >> 16);
} /* not frand: the game's sequence stays as it was */

static int snd_live(const Car *c)
{
    return c == &cars[0] && !planning && (state == ST_COUNT || state == ST_RACE || state == ST_DONE);
}

static void snd_grounded(const Car *c, float rate)
{
    if (!snd_live(c) || sfx.grounded_cooldown > 0) {
        return;
    }

    snd_play(S_GROUNDED, 400, (int)clampf(28 + (rate - 3.f) * 8, 28, 64));
    sfx.grounded_cooldown = 0.5f;
    pad_rumble(clampf((rate - 3.f) * 0.06f, 0.1f, 0.5f), 90);
}

/* UpdateEngineRevs, at the original's ~10 Hz frame rate */
static void engine_revs(int touch, int input, int zs)
{
    int c = touch ? (zs & ~0xf) : (input ? 0x9000 : 0);
    c = (c + 0x580) >> 3;
    c = sfx.engine_revs < 192 && c < 256
            ? 2
            : (c - sfx.engine_revs) >> 3; /* slow climb from a standstill, until the car pulls away (after the crane) */
    c = SDL_min(c, 0x100);
    c = SDL_max(c, touch ? -0x100 : -0x20);
    sfx.engine_change = c;
    sfx.engine_fluct = snd_rand() & 0xf;
}

/* FramesWheelsEngine, 50 Hz: the revs pick a sample and a period below 256, which is the same as TickOver at 47.3 * (revs + 378) Hz */
static void engine_frame(void)
{
    sfx.engine_revs += sfx.engine_change;

    if (sfx.engine_revs < 0) {
        sfx.engine_revs = 0;
    }

    int period = 4800000 / (sfx.engine_revs + 378), k = 6;

    if (period >= 0x3fff) {
        period = 0x3ffe;
    }

    period |= sfx.engine_fluct;

    if (period < 124) {
        period = 124;
    }

    while (period >= 256) {
        period >>= 1;

        if (--k < 0) {
            k = 0;
        }
    }

    snd_engine(k, period);
}

static void snd_tick(float dt)
{
    static int quiet;
    const Car *p = &cars[0];
    sfx.grounded_cooldown -= dt;
    sfx.hitcar_cooldown -= dt;
    if (state == ST_MENU || state == ST_VIEW || state == ST_EDIT || state == ST_PREVIEW ||
        (state == ST_REPLAY && (rp_paused || rp_speed != 1))) {
        if (!quiet) {
            snd_stop_all();
        }

        quiet = 1;
        sfx.engine_on = sfx.engine_revs = sfx.engine_change = 0;
        return;
    }
    quiet = 0;

    if (p->crane_time > 0) { /* on the crane all is quiet, the engine starts again from a standstill */
        if (sfx.engine_on) {
            snd_engine_stop();
        }

        sfx.engine_on = sfx.engine_revs = sfx.engine_change = 0;
        sfx.scraping = sfx.in_dirt = 0;
    } else {
        if ((sfx.timer_10hz += dt) >= 0.1f) {
            sfx.timer_10hz -= 0.1f;
            int zs = (int)(fabsf(vdot(p->v, p->F)) * AMI_V), fer = zs >> 8 < 50 ? zs >> 8 : 50;

            if (state == ST_DONE) {
                sfx.engine_change = -8; /* race over: the revs wind down to nothing */
            } else {
                engine_revs(p->wheels_touching > 0,
                            (state == ST_RACE || state == ST_REPLAY) && !p->wrecked && p->crash_time <= 0 &&
                                (p->throttle > 0 || p->brake > 0),
                            zs);
            }

            if (sfx.scraping && p->crash_time <= 0 && fer >= 1) {
                snd_play(S_WRECK, (31 - (fer >> 1)) * 4 + 170, 64);
            }

            if (sfx.in_dirt && vlen(p->v) > 1.5f) {
                snd_play(S_OFFROAD, 450 + (snd_rand() & 0x1c), 64);
            }

            sfx.scraping = sfx.in_dirt = 0;
        }

        for (sfx.timer_50hz += dt; sfx.timer_50hz >= 0.02f; sfx.timer_50hz -= 0.02f) {
            if (state == ST_DONE && sfx.engine_revs <= 0) {
                if (sfx.engine_on) {
                    snd_engine_stop();
                }

                sfx.engine_on = 0;
            } else {
                engine_frame();
                sfx.engine_on = 1;
            }
        }
    }

    if (mixer.dump) {
        snd_dump_frames(dt);
    }
}

/* pause: the engine stops, effects play out */
static void snd_paused(void)
{
    snd_engine_stop();
}
