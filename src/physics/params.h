/* physics/params.h - the tunable vehicle parameters */
#pragma once

/* ---- tunable vehicle physics (menu F2, saved to physics.cfg) ---- */
typedef struct {
    float mass;                  /* kg */
    float gravity;               /* m/s^2 */
    float engine_power;          /* W */
    float max_drive_force;       /* N, traction limit at low speed */
    float boost_power;           /* W added by the boost */
    float boost_force_max;       /* N added to the traction limit by the boost */
    float boost_drain_rate;      /* boost tank (100) drained per second */
    float brake_force_front;     /* N */
    float brake_force_rear;      /* N */
    float drag;                  /* aerodynamic drag factor */
    float tyre_friction;         /* grip coefficient of the tyres */
    float rear_grip_factor;      /* rear tyre grip relative to the front */
    float steer_lateral_grip;    /* m/s^2 of sideways grip the steering limit assumes */
    float key_steer_rise;        /* steering input rate while a key is held, per second */
    float key_steer_return;      /* steering input rate when it returns to centre, per second */
    float cornering_stiffness;   /* how fast sideways slip saturates the tyre force (higher = sharper turn-in) */
    float low_speed_slip_offset; /* m/s added to the forward speed in the slip ratio, so rest does not divide by 0 */
    float tyre_load_max;         /* N, tyre load that still adds grip: landing spikes do not */
    float max_steer_angle;       /* rad at standstill */
    float steer_falloff_speed;   /* m/s at which the steering angle has halved */
    float steer_rate;            /* rad/s the wheels turn at */
    float spring_front;          /* N/m */
    float spring_rear;           /* N/m */
    float damper_compression;    /* N s/m while compressing */
    float damper_rebound;        /* N s/m while extending */
    float bump_stop_spring;      /* N/m of the progressive bump stops */
    float roll_damping;          /* angular damping about the car's forward axis */
    float yaw_damping;           /* about its up axis */
    float pitch_damping;         /* about its right axis */
    float air_control_torque;    /* torque of the in-air pitch control */
    float lean_roll_per_accel;   /* visual body roll per m/s^2 of sideways acceleration */
    float lean_pitch_per_accel;  /* visual body pitch per m/s^2 of forward acceleration */
    float shoulder_width;        /* m past the deck edge where wheels still find an invisible shoulder */
    float shoulder_drop;         /* m the shoulder lies below the edge; it then falls 1:2 outwards */
    float top_speed;             /* m/s, engine cut-off */
    float top_speed_boost;       /* m/s, engine cut-off on boost */
} Phys;

static Phys phys; /* the live values, set by phys_defaults() */
