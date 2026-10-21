/* render/draw_crane.h - recovery cranes and chains */
#pragma once

/* ---------------------------------------------------------------- crane and chains */
static void chain(V3 a, V3 b)
{
    V3 d = vsub(b, a);
    float L = vlen(d);

    if (L < 0.05f) {
        return;
    }

    V3 ax = vmul(d, 1 / L), u = vnorm(vcross(ax, fabsf(ax.y) < 0.9f ? UP : V(1, 0, 0))), w = vcross(ax, u);
    int n = (int)(L / 0.11f);

    for (int k = 0; k < n; k++) { /* links alternate by 90 degrees */
        V3 c = vadd(a, vmul(ax, (k + 0.5f) * L / n));
        obox(c, ax, (k & 1) ? u : w, (k & 1) ? w : u, 0.07f, 0.035f, 0.012f, 0x5A5A62);
    }
}

static void lattice(V3 a, V3 b, V3 side, float wdt, uint32_t col) /* two-chord lattice beam with zig-zag web */
{
    V3 o = vmul(vnorm(side), wdt * 0.5f);
    V3 a0 = vsub(a, o), a1 = vadd(a, o), b0 = vsub(b, o), b1 = vadd(b, o);
    cyl(a0, b0, 0.11f, 0.11f, 6, col, 0);
    cyl(a1, b1, 0.11f, 0.11f, 6, col, 0);
    int n = (int)(vlen(vsub(b, a)) / 1.6f) + 1;

    for (int k = 0; k < n; k++) {
        float t0 = (float)k / n, t1 = (float)(k + 1) / n;
        cyl(vlerp(a0, b0, t0), vlerp(a1, b1, t1), 0.06f, 0.06f, 5, col, 0);
        cyl(vlerp(a1, b1, t0), vlerp(a0, b0, t0), 0.06f, 0.06f, 5, col, 0);
    }
}

static void draw_crane(const Car *c)
{
    if (!c->active || (c->crane <= 0 && c->hook <= 0)) {
        return;
    }

    const Seg *s = &seg[c->crane_seg];
    V3 Rh = vnorm(V(s->R.x, 0, s->R.z)), Th = vnorm(V(s->T.x, 0, s->T.z));
    V3 base = vadd(c->crane_x, vmul(Rh, TRACK_HW + 11.f));
    base.y = ground_h(base.x, base.z);
    float topy = c->crane_x.y + 12.f;
    V3 top = V(base.x, topy, base.z), tip = V(c->crane_x.x, topy + 0.8f, c->crane_x.z);
    {
        Frm cf = frm_at(V(base.x + Th.x * 1.5f, base.y, base.z + Th.z * 1.5f), Th);
        crane_carrier(&cf);
    } /* carrier */
    for (int k = 0; k < 4; k++) { /* tower */
        V3 o = vadd(vmul(Rh, (k & 1) ? 0.8f : -0.8f), vmul(Th, (k & 2) ? 0.8f : -0.8f));
        cyl(vadd(vadd(base, V(0, 1.35f, 0)), o), vadd(top, o), 0.13f, 0.13f, 6, 0xE8C020, 0);
    }

    for (float y = 3.f; y < topy - base.y - 1; y += 2.2f) {
        V3 p0 = V(base.x, base.y + y, base.z);
        cyl(vadd(p0, vmul(Rh, -0.8f)), vadd(vadd(p0, vmul(Rh, 0.8f)), V(0, 1.1f, 0)), 0.06f, 0.06f, 5, 0x2A2A2E, 0);
        cyl(vadd(p0, vmul(Th, -0.8f)), vadd(vadd(p0, vmul(Th, 0.8f)), V(0, 1.1f, 0)), 0.06f, 0.06f, 5, 0x2A2A2E, 0);
    }

    lattice(top, tip, Th, 1.4f, 0xE8C020);                                  /* jib over the track */
    obox(vsub(top, vmul(Rh, 3.f)), Rh, UP, Th, 1.4f, 1.0f, 1.2f, 0x3A3A40); /* counterweight */
    lattice(top, vsub(top, vmul(Rh, 3.f)), Th, 1.4f, 0xE8C020);
    /* four chains hang straight down from a spreader frame: the front pair to the bumper hoop, so the driver sees them
     * in front of the windscreen like in the original, the rear pair to the back of the roll cage */
    static const V3 att[4] = {
        {-0.5f, 0.17f, 2.08f}, {0.5f, 0.17f, 2.08f}, {-0.6f, 1.107f, -1.438f}, {0.6f, 1.107f, -1.438f}};
    const float sy = 4.2f;
    V3 hook, pts[4], spr[4];

    for (int k = 0; k < 4; k++) {
        pts[k] = body_pt(c, att[k]);
        spr[k] = body_pt(c, V(att[k].x, sy, att[k].z));
    }

    V3 above = body_pt(c, V(0, sy + 2.4f, 0.32f));

    if (c->crane > 0) {
        hook = above;
    } else {
        hook = vlerp(tip, above, smooth01(c->hook / 2.0f)); /* released: hook winds up */
    }
    cyl(tip, hook, 0.04f, 0.04f, 4, 0x1A1A1E, 0);
    obox(hook, V(1, 0, 0), UP, V(0, 0, 1), 0.22f, 0.3f, 0.12f, 0xE8C020);

    if (c->crane > 0) {
        for (int k = 0; k < 4; k++) {
            cyl(spr[k],
                spr[(k == 0)   ? 1
                    : (k == 1) ? 3
                    : (k == 3) ? 2
                               : 0],
                0.07f, 0.07f, 5, 0xE8C020, 0);               /* spreader frame */
            cyl(hook, spr[k], 0.03f, 0.03f, 4, 0x1A1A1E, 0); /* slings */
            chain(spr[k], pts[k]);
        }
    }
}
