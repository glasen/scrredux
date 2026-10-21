/* audio/sound_events.h - when the original's samples play */
#pragma once

/* ---- sound: when the original's samples play (StuntCarRacer.s, Car_Behaviour.cpp), periods and volumes from its tables ---- */
static int eng_revs, eng_chg, eng_fluct, eng_on;   /* the original's engine.revs model */
static float snd_t10, snd_t50, grounded_cd, hitcar_cd;
static int snd_scrape, snd_dirt;             /* player since the last 10 Hz frame: steel scraping the deck / sliding in the dirt */
static uint32_t snd_rng = 1;
static int snd_rand(void) { snd_rng = snd_rng * 1664525u + 1013904223u; return (int)(snd_rng >> 16); }   /* not frand: the game's sequence stays as it was */
static int snd_live(const Car *c) { return c == &cars[0] && !planning && (state == ST_COUNT || state == ST_RACE || state == ST_DONE); }
static void snd_grounded(const Car *c, float rate)
{
    if (!snd_live(c) || grounded_cd > 0) return;
    snd_play(S_GROUNDED, 400, (int)clampf(28 + (rate - 3.f) * 8, 28, 64)); grounded_cd = 0.5f;
    pad_rumble(clampf((rate - 3.f) * 0.06f, 0.1f, 0.5f), 90);
}
/* UpdateEngineRevs, at the original's ~10 Hz frame rate */
static void engine_revs(int touch, int input, int zs)
{
    int c = touch ? (zs & ~0xf) : (input ? 0x9000 : 0);
    c = (c + 0x580) >> 3;
    c = eng_revs < 192 && c < 256 ? 2 : (c - eng_revs) >> 3;   /* slow climb from a standstill, until the car pulls away (after the crane) */
    if (c >= 0x100) c = 0x100;
    else if (c < (touch ? -0x100 : -0x20)) c = touch ? -0x100 : -0x20;
    eng_chg = c; eng_fluct = snd_rand() & 0xf;
}
/* FramesWheelsEngine, 50 Hz: the revs pick a sample and a period below 256, which is the same as TickOver at 47.3 * (revs + 378) Hz */
static void engine_frame(void)
{
    eng_revs += eng_chg; if (eng_revs < 0) eng_revs = 0;
    int period = 4800000 / (eng_revs + 378), k = 6;
    if (period >= 0x3fff) period = 0x3ffe;
    period |= eng_fluct;
    if (period < 124) period = 124;
    while (period >= 256) { period >>= 1; if (--k < 0) k = 0; }
    snd_engine(k, period);
}
static void snd_tick(float dt)
{
    static int quiet;
    const Car *p = &cars[0];
    grounded_cd -= dt; hitcar_cd -= dt;
    if (state == ST_MENU || state == ST_VIEW || state == ST_EDIT || state == ST_PREVIEW || (state == ST_REPLAY && (rp_paused || rp_speed != 1))) {
        if (!quiet) { snd_stop_all(); } quiet = 1; eng_on = eng_revs = eng_chg = 0; return; }
    quiet = 0;
    if (p->crane > 0) {                      /* on the crane all is quiet, the engine starts again from a standstill */
        if (eng_on) snd_engine_stop();
        eng_on = eng_revs = eng_chg = 0; snd_scrape = snd_dirt = 0;
    } else {
        if ((snd_t10 += dt) >= 0.1f) {
            snd_t10 -= 0.1f;
            int zs = (int)(fabsf(vdot(p->v, p->F)) * AMI_V), fer = zs >> 8 < 50 ? zs >> 8 : 50;
            if (state == ST_DONE) eng_chg = -8;      /* race over: the revs wind down to nothing */
            else engine_revs(p->nwc > 0, (state == ST_RACE || state == ST_REPLAY) && !p->wrecked && p->crash <= 0 && (p->thr > 0 || p->brk > 0), zs);
            if (snd_scrape && p->crash <= 0 && fer >= 1) snd_play(S_WRECK, (31 - (fer >> 1)) * 4 + 170, 64);
            if (snd_dirt && vlen(p->v) > 1.5f) snd_play(S_OFFROAD, 450 + (snd_rand() & 0x1c), 64);
            snd_scrape = snd_dirt = 0;
        }
        for (snd_t50 += dt; snd_t50 >= 0.02f; snd_t50 -= 0.02f) {
            if (state == ST_DONE && eng_revs <= 0) { if (eng_on) snd_engine_stop(); eng_on = 0; }
            else { engine_frame(); eng_on = 1; }
        }
    }
    if (snd_dump) snd_dump_frames(dt);
}
/* pause: the engine stops, effects play out */
static void snd_paused(void) { snd_engine_stop(); }
