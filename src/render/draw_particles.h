/* render/draw_particles.h - sparks, dust, smoke and fire as camera-facing shapes */
#pragma once

/* ---------------------------------------------------------------- particles */
static void draw_parts(void)
{
    for (int i = 0; i < npart; i++) {
        const Part *q = &parts[i];
        float a = q->t / q->life;
        if (q->kind == P_SPARK) {
            V3 d = vmul(q->v, 0.05f);
            float dl = vlen(d);
            if (dl < 1e-3f) {
                continue;
            }
            if (dl > 0.22f) {
                d = vmul(d, 0.22f / dl); /* short streaks, even when flying past the camera */
            }
            V3 tail = vsub(q->p, d);
            float wdt = fminf(0.03f, 0.006f * vlen(vsub(q->p, cam.pos))); /* thin close to the eye */
            V3 sd = vmul(vnorm(vcross(d, cam.fwd)), wdt);
            draw_quad_unlit(vsub(tail, sd), vadd(tail, sd), vadd(q->p, sd), vsub(q->p, sd), q->col);
            continue;
        }
        float r = q->size * (q->kind == P_FIRE ? 1.f - a : 1.f - a * a * a);
        uint32_t col = q->kind == P_FIRE ? q->col : mixcol(q->col, FOGCOL, a * a * 0.8f); /* thin out into the air */
        V3 pr[6];
        for (int k = 0; k < 6; k++) {
            float an = k * PI_F / 3 + i;
            pr[k] = vadd(q->p, vadd(vmul(cam.right, cosf(an) * r), vmul(cam.up, sinf(an) * r)));
        }
        for (int k = 0; k < 6; k++) {
            draw_tri_unlit(q->p, pr[k], pr[(k + 1) % 6], col);
        }
    }
}
