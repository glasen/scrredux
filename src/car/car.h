/* car/car.h - the Car type and the car's geometry loaded from the model */
#pragma once

/* ------------------------------------------------------------------ cars */

typedef struct {
    /* rigid body: position (x) and velocity (v) of the centre of mass, axes right/up/forward, angular velocity */
    V3 x, v, R, U, F, w;

    /* race progress */
    int seg; /* road segment under the car */
    int lap;
    int furthest_lap; /* the furthest lap begun */
    int finished, wrecked, active;
    int held_at_start; /* countdown: brakes held, the car does not reverse */
    int safe_segment;  /* last segment the car was safely on, for crane recovery */
    float progress;    /* laps * segments + position: ordering of the two cars */
    float finish_time, lap_start_time, best_lap, last_lap;
    float lap_banner_time; /* seconds left showing the lap time */

    /* driver input and resulting state */
    float throttle, brake;
    float steer_input; /* -1 .. 1 */
    float steer_angle; /* rad */
    float boost;       /* tank, 0 .. 100 */
    int boosting;
    float damage;   /* 0 .. 100, wrecked at 100 */
    int hole_count; /* big hits: holes in the cockpit's top tube this season (see hole_at) */

    /* crane recovery */
    float crash_time;     /* seconds since a crash on the ground */
    float crane_time;     /* seconds of lowering so far */
    float crane_duration; /* total lowering time */
    float hook_time;      /* hook retract timer */
    V3 crane_target;      /* where the crane sets the car down */
    int crane_segment;

    /* wheels and suspension, front left, front right, rear left, rear right */
    float suspension_len[4];
    float wheel_phase[4]; /* rotation angle of each wheel */
    int wheels_touching;
    unsigned char point_hit[16]; /* body collision points already in contact (one crack per impact) */
    unsigned char wheel_bottomed[4], wheel_contact[4];

    /* timers */
    float dust_timer, flipped_time, stuck_time, hurt_cooldown;
    float smash_time; /* after a smash, hits only creak for a while */

    /* visual body motion */
    float lean_roll, lean_pitch; /* angles */
    float lean_roll_rate, lean_pitch_rate;
    float lateral_accel, longitudinal_accel; /* filtered */
    float flame;                             /* exhaust flame strength (0..1.4), boost only */

    uint32_t col;
    float ai_lane;
    int ai_lane_set; /* AI: the lane it is easing towards its target */
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
