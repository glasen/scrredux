/* physics/steering.h - steering limit from speed and grip */
#pragma once

static float steer_max(float v, float gl)        /* gl: gravity's help toward the side being steered (banking), m/s^2 */
{
    float d = ph.steer / (1.f + (v / ph.steerfall) * (v / ph.steerfall));
    float lim = atanf(2.67f * fmaxf(ph.steerlat + gl, 5.f) / fmaxf(v * v, 1.f));   /* never steer harder than the tyres can hold: no oversteer snap */
    return fminf(d, lim);
}
