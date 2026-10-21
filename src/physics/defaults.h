/* physics/defaults.h - default values of the vehicle parameters */
#pragma once

/* ------------------------------------------------------- vehicle physics constants */
static void phys_defaults(void)
{
    phys.mass = 1000.f;
    phys.gravity = 13.f;
    phys.engine_power = 337000.f;
    phys.max_drive_force = 9900.f;
    phys.boost_power = 758000.f;
    phys.boost_force_max = 9750.f;
    phys.boost_drain_rate = 2.f; /* boost tank (100) drains this much per second */
    phys.brake_force_front = 4500.f;
    phys.brake_force_rear = 3200.f;
    phys.drag = 0.88f;
    phys.tyre_friction = 2.35f;
    phys.rear_grip_factor = 1.05f;
    phys.steer_lateral_grip = 45.f;
    phys.key_steer_rise = 5.f;
    phys.key_steer_return = 10.f;
    phys.cornering_stiffness = 12.f;
    phys.low_speed_slip_offset = 2.0f;
    phys.tyre_load_max = 6500.f;
    phys.max_steer_angle = 0.5f;
    phys.steer_falloff_speed = 30.f;
    phys.steer_rate = 3.f;
    phys.spring_front = 26000.f;
    phys.spring_rear = 24000.f;
    phys.damper_compression = 2600.f;
    phys.damper_rebound = 3600.f;
    phys.bump_stop_spring = 250000.f;
    phys.roll_damping = 1100.f;
    phys.yaw_damping = 2000.f;
    phys.pitch_damping = 1000.f;
    phys.air_control_torque = 7000.f;
    phys.lean_roll_per_accel = 0.006f;
    phys.lean_pitch_per_accel = 0.004f;
    phys.shoulder_width = 1.5f;    /* invisible shoulder past the deck edge: wheels still find ground up to shw out, */
    phys.shoulder_drop = 0.25f;    /* shdrop lower at the edge and falling 1:2 outwards, like the original */
    phys.top_speed = 200.f / 3.6f; /* engine cut-off: 200 km/h, */
    phys.top_speed_boost =
        250.f / 3.6f; /* 250 km/h on boost (the boost adds pull, the drag alone would allow 107 m/s) */
}
