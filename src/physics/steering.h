/* physics/steering.h - steering limit from speed and grip */
#pragma once

static float steer_max(float v, float gl) /* gl: gravity's help toward the side being steered (banking), m/s^2 */
{
    float d = phys.max_steer_angle / (1.f + (v / phys.steer_falloff_speed) * (v / phys.steer_falloff_speed));
    float lim = atanf(2.67f * fmaxf(phys.steer_lateral_grip + gl, 5.f) /
                      fmaxf(v * v, 1.f)); /* never steer harder than the tyres can hold: no oversteer snap */
    return fminf(d, lim);
}
