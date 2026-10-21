/* config/ai.h - how the computer driver steers, picks its speed and plans jumps */
#pragma once

/* ---- keeping clear of the other car (ai_lane_clear) ---- */
#define AI_RIVAL_BEHIND 6.f          /* m: a rival further behind than this is ignored ... */
#define AI_RIVAL_AHEAD 18.f          /* ... and one further ahead */
#define AI_PASS_ROOM 3.4f            /* m, centre to centre, for two cars side by side */
#define AI_PASS_ROOM_SLACK 0.2f      /* m the gap may fall short of that */
#define AI_EDGE_MARGIN 2.2f          /* m kept from the road edge while passing */
#define AI_PASS_OTHER_SIDE_AHEAD 4.f /* m the rival must be ahead to pass it on the other side */
#define AI_BACK_OFF_AHEAD -1.f       /* m: alongside or behind this: lift off */

/* ---- lane ---- */
#define AI_JUMP_LOOK_BEHIND 25   /* m: a gap or sheer face this close behind ... */
#define AI_JUMP_LOOK_AHEAD 110   /* ... or ahead makes the lane hold */
#define AI_JUMP_LANE_LIMIT 1.5f  /* m from the middle while holding the lane through a jump */
#define AI_RUNUP_LANE_LIMIT 2.2f /* m from the middle on a run-up */
#define AI_LANE_CHANGE_RATE 2.f  /* m/s: lanes change smoothly, never with a jerk */

/* ---- steering ---- */
#define AI_HEADING_GAIN 0.55f          /* against the heading error */
#define AI_CROSSTRACK_GAIN 2.2f        /* arctan of this times the cross-track error over speed */
#define AI_CROSSTRACK_SPEED_OFFSET 4.f /* m/s added to the speed there */
#define AI_CURVATURE_GAIN 3.4f         /* feed-forward from the road curving ahead */
#define AI_CURVE_LOOK_BASE 6.f         /* m ahead the curvature is read, plus ... */
#define AI_CURVE_LOOK_TIME 0.3f        /* ... this many seconds of travel */
#define AI_STEER_LIMIT 3.2f /* rad m/s: the steering angle limit is this over (speed + AI_STEER_LIMIT_SPEED) */
#define AI_STEER_LIMIT_SPEED 5.f

/* ---- speed ---- */
#define AI_SPEED_LOOK_TIME 2.0f     /* s of travel looked ahead for bends, plus ... */
#define AI_SPEED_LOOK_BASE 20.f     /* ... this many metres (at least AI_SPEED_LOOK_MIN_SPEED's worth) */
#define AI_SPEED_LOOK_MIN_SPEED 5.f /* m/s */
#define AI_MIN_BEND_ACCEL 3.f       /* m/s^2: a bend never limits the speed below what this allows */
#define AI_CREST_FLOAT 1.1f         /* the car may float a little over crests ... */
#define AI_CREST_FLOAT_WIDE 0.95f   /* ... less out wide, where the deck may twist, */
#define AI_WIDE_LANE 1.6f           /* m from the middle that counts as wide */
#define AI_DIP_ACCEL 26.f           /* m/s^2: the most it pushes down into a dip */
#define AI_JUMP_ZONE_SPEED_MIN 30.f /* m/s: speed band near a jump that is not a run-up */
#define AI_JUMP_ZONE_SPEED_MAX 34.f
#define AI_THROTTLE_GAIN 0.5f      /* throttle per m/s below the target speed ... */
#define AI_RUNUP_THROTTLE_GAIN 2.f /* ... and on a run-up */
#define AI_THROTTLE_MIN 0.2f
#define AI_OVERSPEED 1.06f     /* factor over the target before braking */
#define AI_RUNUP_OVERSPEED 1.f /* m/s over the take-off speed before braking on a run-up */
#define AI_BRAKE_GAIN 0.15f    /* brake per m/s over */
#define AI_BRAKE_MIN 0.15f
#define AI_CRUISE_THROTTLE 0.3f /* holding the speed */
#define AI_STEER_LIFT_SPEED 8.f /* m/s above which steering takes throttle away ... */
#define AI_STEER_LIFT_MIN 0.25f
#define AI_STEER_LIFT_FACTOR 0.85f
#define AI_LIFT_BRAKE_SPEED 6.f    /* m/s above which a rival alongside gets a brake ... */
#define AI_LIFT_BRAKE 0.25f        /* ... of this strength */
#define AI_STEER_BRAKE_RELIEF 0.7f /* brake lost at full lock, so the tyres keep grip to steer with */
#define AI_BOOST_MARGIN 1.f        /* m/s below the take-off speed that calls for boost */

/* ---- jump planning (ai/jump_planner.h) ---- */
#define NJ 64                           /* jumps planned per track at most */
#define RUN_ALAT 36.f                   /* lateral grip the AI uses in the bend before a jump (m/s^2, before banking) */
#define JUMP_BEND_MIN_CURVATURE 0.004f  /* 1/m: a gentler bend is no bend */
#define JUMP_CREST_MIN_CURVATURE 0.002f /* 1/m: a gentler crest is not a crest */
#define JUMP_CREST_FACTOR 1.2f          /* the run-up may float this much over crests */
#define JUMP_RUNUP_BRAKING 6.f          /* m/s^2: braking curve towards the take-off speed */
#define JUMP_RUNUP_LENGTH 700           /* m searched back from a lip for its run-up */
#define JUMP_CREST_FREE_DISTANCE 40     /* m before the lip from which crests count */
#define JUMP_CHAIN_GAP 150              /* m: jumps closer than this form one chain taken at one speed */
#define JUMP_NO_RUNUP_GAP 40            /* m: a lip this close behind another has no run-up of its own */
#define JUMP_LANDING_SEARCH 300         /* m searched for the next face when finding where a jump ends */
#define JUMP_LANDING_TOLERANCE 2.f      /* m above or below the lip that still counts as level */
#define JUMP_GOAL_DISTANCE 200          /* m after a jump the scratch car must settle by */
#define JUMP_NEXT_MARGIN 30             /* m kept before the next jump */
#define JUMP_SCRATCH_SECONDS 20         /* s the scratch car is given */
#define JUMP_FAILED 1000.f              /* score of a crashed or stuck scratch run */
#define JUMP_MIN_SPEED 20               /* m/s: the take-off speeds tried */
#define JUMP_SPEED_COUNT 92
#define JUMP_UNREACHABLE 500.f         /* best score at or above which no speed gets over the jump */
#define JUMP_SCRATCH_START_SPEED 200.f /* m/s cap on the scratch car's start speed: effectively none */
#define JUMP_LIP_RUN_START_SPEED 55.f  /* m/s the scratch car starts a reach test at */
#define JUMP_RUNUP_TRIM 10             /* m: the reach test starts this far into the last bend, or ... */
#define JUMP_RUNUP_EARLY 5             /* ... this far before the run-up begins */
#define JUMP_REACH_MARGIN 3.f          /* m/s over the boosted reach still tried */
#define JUMP_SAFE_MARGIN 1.5f          /* m/s under the reach that counts as reachable */
#define JUMP_BOOST_PENALTY 3.f         /* cost of a speed that needs the boost */
#define JUMP_OVERREACH_PENALTY 20.f    /* cost per m/s over what even the boost reaches */
#define JUMP_SPEED_BONUS 0.05f         /* cost taken off per m/s: faster is better */
#define JUMP_RETRY_WINDOW 5.f          /* cost within which a speed is tried again from either side */
#define JUMP_SIDE_OFFSET 2.f           /* m off the middle for those tries */
#define JUMP_MIN_GOAL_GAP 2            /* segments: the goal is at least this far past the jump */
