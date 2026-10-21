/* config/physics.h - physics step and the shorthand names for the tunable vehicle parameters */
#pragma once

#define GRAV (phys.gravity)
#define CAR_M (phys.mass)
#define K_F (phys.spring_front)
#define K_R (phys.spring_rear)
#define C_COMP (phys.damper_compression)
#define C_REB (phys.damper_rebound)
#define K_BUMP (phys.bump_stop_spring)
#define MU (phys.tyre_friction)
#define IX (2200.f * phys.mass / 1000.f)
#define IY (2400.f * phys.mass / 1000.f)
#define IZ (900.f * phys.mass / 1000.f)
#define PHYS_DT (1.0f / 240.0f)

/* ---- wheels: finding the road (physics/car_step.h) ---- */
#define ROAD_EDGE_MARGIN 0.1f       /* m past the road's half width before the shoulder starts */
#define SHOULDER_SLOPE 0.5f         /* the shoulder falls away 1:2, and its normal tips outwards by the same */
#define WHEEL_MIN_SURFACE_COS 0.15f /* a wheel only rests on a surface the car is within ~81 degrees of */
#define WHEEL_BELOW_ROAD_MAX -0.6f  /* m: a mount deeper below the deck is under it (no contact) ... */
#define WHEEL_BELOW_STEEP_MAX -8.f  /* ... but beside a sheer face the deck reaches down to the ground */

/* ---- suspension ---- */
#define DAMPER_REFERENCE_SPRING 14000.f /* N/m: damper rates are for this spring, scaled with the square root */
#define BUMP_STOP_QUADRATIC 6.0e6f      /* N/m^2: the bump stop gets harder the further it is pressed */
#define BUMP_STOP_DAMPING 9000.f        /* N s/m while the stop is closing */
#define SPRING_FORCE_MAX 160000.f       /* N, per wheel */
#define SPRING_FORCE_STEEP_G 1.5f       /* beside a sheer face a wheel pushes at most this many car weights */
#define TOUCHDOWN_SOUND_RATE 4.f        /* m/s closing speed that sounds a touchdown */
#define BOTTOM_OUT_SOUND_RATE 2.5f      /* m/s closing speed on the bump stop that sounds "grounded" ... */
#define BOTTOM_OUT_DAMAGE_RATE 7.f      /* ... and above which it damages the car, ... */
#define BOTTOM_OUT_DAMAGE_PER_MS 0.6f   /* ... by this much per m/s over */

/* ---- tyres ---- */
#define STANDSTILL_SPEED 0.8f           /* m/s below which a held brake becomes reverse */
#define STANDSTILL_REVERSE_FORCE 4500.f /* N */
#define DRIVE_MIN_SPEED 3.f             /* m/s: the engine's push is power / speed, but not above this speed's push */
#define DRIVE_CUTOFF_RANGE 2.f          /* m/s over which the push fades out below the top speed */
#define REAR_DRIVE_SHARE 0.5f           /* of the driving force on each rear wheel */
#define BRAKE_SLIP_SCALE 0.25f          /* wheel speed (m/s) at which the brake force saturates: 1 / this */
#define ROLLING_RESISTANCE 0.012f       /* fraction of the tyre load */
#define ROLLING_SLIP_SCALE 3.f          /* wheel speed at which it saturates: 1 / this */
#define TYRE_FORCE_HEIGHT 0.85f         /* tyre forces act this fraction of the way up to the centre of mass */

/* ---- the body ---- */
#define ANGULAR_DRAG 180.f         /* N m per rad/s */
#define AIR_CONTROL_MIN_SPEED 6.f  /* m/s above which the in-air assist works */
#define AIR_CONTROL_DAMPING 4500.f /* N m per rad/s about the right and up axes in flight */
#define AIR_LEVEL_STIFFNESS 9000.f /* torque levelling the wings */
#define AIR_ROLL_DAMPING 3200.f    /* N m per rad/s about the forward axis in flight */
#define GROUND_ASSIST_PARTIAL 0.5f /* strength of the stability aid with fewer than 3 wheels down */
#define MAX_SPIN 14.f              /* rad/s */
#define MAX_SPEED 130.f            /* m/s */

/* ---- visual motion ---- */
#define FLAME_STRENGTH 1.4f         /* exhaust flame while the boost burns */
#define FLAME_RATE 14.f             /* per second it follows */
#define LEAN_FILTER_RATE 16.f       /* per second the lean accelerations follow the forces */
#define LEAN_LATERAL_ACCEL_MAX 30.f /* m/s^2 */
#define LEAN_LONGITUDINAL_ACCEL_MAX 25.f
#define LEAN_ROLL_MAX 0.20f /* rad */
#define LEAN_PITCH_MAX 0.14f
#define LEAN_ROLL_SPRING 140.f
#define LEAN_ROLL_DAMPER 21.f
#define LEAN_PITCH_SPRING 120.f
#define LEAN_PITCH_DAMPER 19.f

/* ---- crashes and recovery ---- */
#define CRASH_DAMAGE_BASE 6.f        /* damage of landing beside the track or in the net ... */
#define CRASH_DAMAGE_PER_IMPACT 0.5f /* ... plus this per m/s of impact */
#define CRASH_SMOKE_INTERVAL 0.12f   /* s between puffs from the wreck */
#define CRASH_CRANE_DELAY 3.4f       /* s lying there before the crane comes */
#define CRANE_LOWER_TIME 2.6f        /* s the crane takes to set a car down */
#define CRANE_HEIGHT 6.5f            /* m above the track the car is picked up from */
#define CRANE_SWAY 0.04f
#define CRANE_SWAY_RATE 2.3f
#define HOOK_RETRACT_TIME 2.0f     /* s the hook takes to wind back up */
#define RESET_TIME 2.5f            /* s flipped or stuck before the crane resets the car */
#define RESET_FLIPPED_UP_Y 0.25f   /* up axis' height below which the car counts as flipped ... */
#define RESET_SLOW_SPEED 6.f       /* ... when also slower than this (m/s) */
#define RESET_STUCK_SPEED 1.0f     /* m/s: stuck with the throttle held ... */
#define RESET_STUCK_THROTTLE 0.15f /* ... above this */
#define RESET_FALL_DEPTH 4.f       /* m below the ground: out of the world */
#define RESET_DAMAGE 4.f

/* ---- contacts (physics/contacts.h) ---- */
#define CONTACT_SLIP_SCALE 0.5f /* sliding speed (m/s) at which contact friction saturates: 1 / this */
#define BODY_SPRING 250000.f    /* N/m of the body points and the deck edge against the road */
#define BODY_DAMPING 7000.f     /* N s/m */
#define BODY_FORCE_MAX 220000.f
#define BODY_STEEP_G 0.6f /* against a sheer face: at most this many car weights, it lifts, not fires */
#define BODY_FRICTION 0.25f
#define BODY_DEPTH 1.2f          /* m under the deck surface where the road's underside is */
#define BODY_SCRAPE_SPEED 3.f    /* m/s sliding and */
#define BODY_SCRAPE_FORCE 1500.f /* N pressing: steel scrapes the deck */
#define BODY_SPARK_CHANCE 0.35f
#define BODY_HIT_SPEED 9.0f  /* m/s closing speed above which a hit damages the car ... */
#define BODY_HIT_DAMAGE 0.5f /* ... by this much per m/s over */
#define EDGE_FORCE_MAX 120000.f
#define EDGE_FRICTION 0.3f
#define EDGE_MAX_PENETRATION 0.35f /* m: deeper is the deck's other side */
#define EDGE_SCRAPE_SPEED 2.f
#define EDGE_SPARK_CHANCE 0.5f
#define GROUND_SPRING 160000.f /* N/m of the ground beside the track */
#define GROUND_DAMPING 6000.f
#define GROUND_FRICTION 0.7f
#define DUST_SPEED 2.f /* m/s sliding that raises dust */
#define DUST_CHANCE 0.04f
#define FENCE_REACH 3.f          /* m from the arena wall where the catch net starts to be felt */
#define FENCE_SPRING 300.f       /* N/m: the net hardly pushes back */
#define FENCE_DAMPING 8000.f     /* N s/m: it brakes instead */
#define FENCE_FORCE_MAX 800000.f /* N in total */
#define FENCE_FRICTION 0.5f
#define CLIMB_WALL_DEPTH 4.f      /* m: how far into a sheer face going up counts as the wall */
#define CLIMB_WALL_CLEARANCE 0.3f /* m under the upper deck */

/* ---- steering limit (physics/steering.h): tan(angle) = wheelbase * lateral acceleration / speed^2 ---- */
#define STEER_WHEELBASE 2.67f  /* m */
#define STEER_MIN_GRIP 5.f     /* m/s^2: never assume less sideways grip than this */
#define STEER_MIN_SPEED_SQ 1.f /* (m/s)^2: the speed squared never counts as less than this */
