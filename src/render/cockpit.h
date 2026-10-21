/* render/cockpit.h - the driver's-view cockpit instruments */
#pragma once

/* ---------------------------------------------------------------- cockpit instruments (driver's view only) */
static float ck_m[12]; /* body rows of the player's car: car frame -> world */

static V3 ckp(V3 l)
{
    return m34_apply(ck_m, l);
}

static V3 ckd(V3 l)
{
    return vsub(ckp(l), ckp(V(0, 0, 0)));
}

static void ck_ring(V3 C, V3 u, V3 v, float r0, float r1, int n, uint32_t col)
{
    for (int k = 0; k < n; k++) {
        float a0 = 2 * PI_F * k / n, a1 = 2 * PI_F * (k + 1) / n;
        V3 d0 = vadd(vmul(u, cosf(a0)), vmul(v, sinf(a0))), d1 = vadd(vmul(u, cosf(a1)), vmul(v, sinf(a1)));
        draw_quad(ckp(vadd(C, vmul(d0, r0))), ckp(vadd(C, vmul(d0, r1))), ckp(vadd(C, vmul(d1, r1))),
                  ckp(vadd(C, vmul(d1, r0))), col);
    }
}

static void ck_disc(V3 C, V3 u, V3 v, float r, int n, uint32_t col)
{
    for (int k = 0; k < n; k++) {
        float a0 = 2 * PI_F * k / n, a1 = 2 * PI_F * (k + 1) / n;
        draw_tri(ckp(C), ckp(vadd(C, vmul(vadd(vmul(u, cosf(a1)), vmul(v, sinf(a1))), r))),
                 ckp(vadd(C, vmul(vadd(vmul(u, cosf(a0)), vmul(v, sinf(a0))), r))), col);
    }
}

/* round gauge in a cup on a bracket, facing the eye; frac 0..1 sweeps 270 degrees from lower left to lower right */
static void gauge(V3 C, float r, float frac, int nmaj, const char **lab, const char *title, V3 mount)
{
    V3 n = vnorm(vsub(EYE, C)), u = vnorm(vcross(UP, n));

    if (u.x < 0) {
        u = vmul(u, -1);
    }

    V3 v = vcross(n, u);

    if (v.y < 0) {
        v = vmul(v, -1);
    }

    cyl(ckp(vsub(C, vmul(n, 0.055f))), ckp(vadd(C, vmul(n, 0.002f))), r * 0.9f, r * 1.12f, 24, 0x1C1C20, 1);
    cyl(ckp(vsub(vsub(C, vmul(n, 0.03f)), vmul(v, r * 0.7f))), ckp(mount), 0.016f, 0.016f, 8, 0x34343A, 0);
    ck_ring(vadd(C, vmul(n, 0.004f)), u, v, r * 0.95f, r * 1.12f, 32, 0xC8CCD6); /* chrome bezel */
    ck_disc(vadd(C, vmul(n, 0.003f)), u, v, r * 0.96f, 32, 0x101014);
    V3 F = vadd(C, vmul(n, 0.0045f)); /* backlit markings */
    const float A0 = 1.25f * PI_F, SW = 1.5f * PI_F;

    for (int k = 0; k <= 4 * nmaj; k++) {
        float f = (float)k / (4 * nmaj), a = A0 - SW * f, w = r * (k % 4 ? 0.012f : 0.025f),
              r0 = r * (k % 4 ? 0.82f : 0.74f);
        V3 d = vadd(vmul(u, cosf(a)), vmul(v, sinf(a))), sd = vmul(vadd(vmul(u, -sinf(a)), vmul(v, cosf(a))), w);
        V3 p0 = vadd(F, vmul(d, r0)), p1 = vadd(F, vmul(d, r * 0.92f));
        draw_quad_unlit(ckp(vsub(p0, sd)), ckp(vsub(p1, sd)), ckp(vadd(p1, sd)), ckp(vadd(p0, sd)), 0xF0F0F4);

        if (k % 4 == 0 && lab[k / 4][0]) {
            world_text_c(ckp(vadd(F, vmul(d, r * 0.58f))), ckd(u), ckd(v), lab[k / 4], r * (nmaj > 6 ? 0.017f : 0.02f),
                         0xF0F0F4, 1);
        }
    }

    world_text_c(ckp(vsub(F, vmul(v, r * 0.6f))), ckd(u), ckd(v), title, r * 0.018f, 0xA0A8B8, 1);
    float a = A0 - SW * clampf(frac, 0, 1);
    V3 d = vadd(vmul(u, cosf(a)), vmul(v, sinf(a))), sd = vmul(vadd(vmul(u, -sinf(a)), vmul(v, cosf(a))), r * 0.035f);
    V3 N = vadd(C, vmul(n, 0.007f)), tail = vsub(N, vmul(d, r * 0.18f));
    draw_tri_unlit(ckp(vsub(tail, sd)), ckp(vadd(N, vmul(d, r * 0.86f))), ckp(vadd(tail, sd)), 0xFF5A10);
    ck_disc(vadd(C, vmul(n, 0.009f)), u, v, r * 0.1f, 12, 0x303036);
}

/* damage: a crack grows along the lit face of the front top tube (like the original), holes where big hits landed;
 * both fit the stretch of the tube the window shows */
static void draw_crack(const Car *c)
{
    const V3 C0 = V(0, 1.0405f, -0.2627f), T = vnorm(V(0, 0.028f, -0.012f)), N = V(0, -0.38f, -0.92f);
    const float X1 = fminf(0.60f, 0.93f * (C0.z - EYE.z) * gpu.target_w / gpu.target_h / FOCAL_Y), X0 = -X1;
    uint32_t r = 12345;
    float s = 0, x = X0, xe = X0 + (X1 - X0) * clampf(c->damage / 100.f, 0, 1);
    V3 prev = vadd(vadd(C0, V(x, 0, 0)), vmul(N, 0.0015f)), w = vmul(T, 0.0021f), lo = vmul(T, -0.0026f),
       lo2 = vmul(T, -0.0037f);

    while (x < xe) {
        r = r * 1664525u + 1013904223u;
        float ra = ((r >> 8) & 0xffff) / 65535.f;
        r = r * 1664525u + 1013904223u;
        float rb = ((r >> 8) & 0xffff) / 65535.f;
        x = fminf(xe, x + 0.008f + 0.012f * ra);
        s = clampf(s + (rb - 0.5f) * 0.009f, -0.009f, 0.009f);
        V3 p = vadd(vadd(vadd(C0, vmul(T, s)), V(x, 0, 0)), vmul(N, 0.0015f));
        draw_quad(ckp(vadd(prev, lo2)), ckp(vadd(p, lo2)), ckp(vadd(p, lo)), ckp(vadd(prev, lo)),
                  0xF07060); /* chipped paint edge */
        draw_quad(ckp(vsub(prev, w)), ckp(vsub(p, w)), ckp(vadd(p, w)), ckp(vadd(prev, w)), 0x1A0404);
        prev = p;
    }

    r = 777;

    for (int h = 0; h < c->hole_count; h++) {
        V3 hc = vadd(vadd(C0, V(X0 + (X1 - X0) * (hole_at(h) + HOLE_W / 2) / 100.f, 0, 0)), vmul(N, 0.0018f)), pts[12];

        for (int k = 0; k < 12; k++) {
            r = r * 1664525u + 1013904223u;
            float an = 2 * PI_F * k / 12, rr = 0.013f + 0.005f * ((r >> 8) & 0xff) / 255.f;
            pts[k] = vadd(hc, vadd(V(cosf(an) * rr * 3.6f * X1, 0, 0), vmul(T, sinf(an) * rr)));
        }

        for (int k = 0; k < 12; k++) {
            draw_tri(ckp(vadd(hc, vmul(T, -0.0015f))), ckp(vadd(pts[k], vmul(T, -0.0015f))),
                     ckp(vadd(pts[(k + 1) % 12], vmul(T, -0.0015f))), 0xF07060);
        }

        for (int k = 0; k < 12; k++) {
            draw_tri(ckp(vadd(hc, vmul(N, 0.0003f))), ckp(vadd(pts[k], vmul(N, 0.0003f))),
                     ckp(vadd(pts[(k + 1) % 12], vmul(N, 0.0003f))), 0x0C0303);
        }
    }
}

static void draw_cockpit(void)
{
    int fp = fp_car();

    if (fp < 0 || !cars[fp].active) {
        return;
    }

    const Car *c = &cars[fp];
    car_body_rows(c, ck_m);
    static const char *bst[] = {"0", "", "50", "", "100"},
                      *spd[] = {"0", "40", "80", "120", "160", "200", "240", "280"};
    gauge(V(-0.35f, 0.535f, -0.10f), 0.052f, c->boost / 100.f, 4, bst, "BOOST", V(-0.35f, 0.45f, -0.04f));
    gauge(V(0.35f, 0.54f, -0.10f), 0.058f, fmaxf(vdot(c->v, c->F), 0) * 3.6f / 280.f, 7, spd, "KPH",
          V(0.35f, 0.45f, -0.04f));
    draw_crack(c);
}
