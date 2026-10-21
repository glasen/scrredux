/* physics/steering.h - steering limit from speed and grip */
#pragma once

static float steer_max(float v, float gl) /* gl: gravity's help toward the side being steered (banking), m/s^2 */
{
    float d = phys.max_steer_angle / (1.f + (v / phys.steer_falloff_speed) * (v / phys.steer_falloff_speed));
    float lim =
        atanf(STEER_WHEELBASE * fmaxf(phys.steer_lateral_grip + gl, STEER_MIN_GRIP) /
              fmaxf(v * v, STEER_MIN_SPEED_SQ)); /* never steer harder than the tyres can hold: no oversteer snap */
    return fminf(d, lim);
}
