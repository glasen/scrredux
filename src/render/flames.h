/* render/flames.h - live exhaust flames */
#pragma once

/* ---------------------------------------------------------------- live exhaust flames */
static void flame_cone(V3 base, V3 ax1, V3 ax2, V3 apex, float r, uint32_t col)
{
    V3 ring[6];
    for (int k = 0; k < 6; k++) { float an = k * PI_F / 3; ring[k] = vadd(base, vadd(vmul(ax1, cosf(an) * r), vmul(ax2, sinf(an) * r))); }
    for (int k = 0; k < 6; k++) draw_tri_unlit(ring[k], ring[(k + 1) % 6], apex, col);
}
/* flames come out of the eight zoomie pipes while the boost burns; they flicker every frame */
static void draw_flames(void)
{
    for (int ci = 0; ci < 2; ci++) {
        const Car *c = &cars[ci];
        if (!c->active || c->flame < 0.03f) continue;
        float m[12]; car_body_rows(c, m);
        V3 R = V(m[0], m[4], m[8]), U = V(m[1], m[5], m[9]), F = V(m[2], m[6], m[10]), T = V(m[3], m[7], m[11]);
        float spd = clampf(vdot(c->v, c->F) / 40.f, 0, 1.5f);
        for (int sd = -1; sd <= 1; sd += 2)
            for (int k = 0; k < 4; k++) {
                V3 lp = FLAMEP[(sd + 1) / 2 * 4 + k], ld = FLAMED[(sd + 1) / 2 * 4 + k];
                V3 base = vadd(T, vadd(vmul(R, lp.x), vadd(vmul(U, lp.y), vmul(F, lp.z))));
                V3 D = vnorm(vadd(vmul(R, ld.x), vadd(vmul(U, ld.y), vmul(F, ld.z))));   /* out of the pipe */
                float rnd = (float)(hash2(frame_no * 13 + ci * 7 + k + (sd + 1) * 5, 99) & 1023) / 1023.f;
                float rn2 = (float)(hash2(frame_no * 7 + ci * 11 + k * 3 + (sd + 1) * 2, 17) & 1023) / 1023.f;
                float fs = ci == fp_car() ? 0.75f : 1.f;          /* smaller when seen from the driver's eye */
                float h = fs * (0.07f + 0.40f * c->flame) * (0.55f + 0.9f * rnd), w = fs * 0.058f * (0.85f + 0.25f * c->flame);   /* base just fills the 12 cm zoomie mouth */
                V3 tilt = vadd(vmul(F, -h * (0.15f + 0.45f * spd)), vmul(R, sd * h * 0.08f * (rn2 - 0.5f)));     /* blown back by the airflow */
                V3 a1 = vnorm(vcross(D, fabsf(vdot(D, F)) < 0.9f ? F : R)), a2 = vcross(D, a1);                /* cone base fills the pipe mouth */
                flame_cone(base, a1, a2, vadd(vadd(base, vmul(D, h)), tilt), w, 0xFF7A10);                           /* orange outer */
                flame_cone(base, a1, a2, vadd(vadd(base, vmul(D, h * 0.62f)), vmul(tilt, 0.6f)), w * 1.08f, 0xFFC020);          /* yellow body, wider so it shows at the base */
                flame_cone(base, a1, a2, vadd(vadd(base, vmul(D, h * 0.32f)), vmul(tilt, 0.3f)), w * 1.15f, 0xFFF4B0);          /* hot white-yellow root */
            }
    }
}
