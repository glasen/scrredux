/* car/car.h - the Car type and the car's geometry loaded from the model */
#pragma once

/* ------------------------------------------------------------------ cars */

typedef struct {
    V3 x, v, R, U, F, w;
    int seg, lap, lapmax, finished, wrecked, active, safe, bst, nwc, hold; /* lapmax: the furthest lap begun */
    float thr, brk, str, delta, boost, damage, prog, ftime, lapstart, bestlap, lastlap, lapshow;
    float crash, crane, crane_T, hook, dust_t; /* crash on the ground / crane lowering timers, hook retract timer */
    V3 crane_x;                                /* where the crane sets the car down */
    int crane_seg;
    float wl[4], wph[4], flip_t, stuck_t, hurt_cd, smash_t; /* smash_t: after a smash, hits only creak for a while */
    unsigned char hit[16], wbot[4], wcon[4];
    float lr, lp, lrr, lpr, alat_f, along_f; /* visual body roll/pitch (angle, rate) and filtered accelerations */
    float flame;                             /* exhaust flame strength (0..1.4), boost only */
    int nholes;                              /* big hits: holes in the cockpit's top tube this season (see hole_at) */
    uint32_t col;
    float ai_lane;
    int ai_lset; /* AI: the lane it is easing towards its target */
} Car;
/* the original's ten hole spots on the 240-pixel crack: 12 pixels wide, every 24, filled from the right end */
static float hole_at(int k)
{
    return (8 + 24 * (NHOLE - 1 - k)) / 2.4f;
} /* k-th hole's start, in % of the crack */

/* car geometry relative to the centre of mass (FL, FR, RL, RR), read from models/stuntcar.scm (car/model_loader.h) */
static V3 WMOUNT[4];
static float WRAD[4];
static float WLF[4];            /* suspension length at full droop (mount to hub) */
static float WLMIN[4];          /* length where the bump stop engages */
static float WREST[4];          /* length at the model's ride height */
static float REST_H, PLACE_H;   /* centre-of-mass height at rest / when dropped onto the track */
static V3 FLAMEP[8], FLAMED[8]; /* exhaust pipe tips and axes */
static V3 CPTS[16];             /* body points that collide with the deck */
/* eye position of the driver */
static V3 EYE;
