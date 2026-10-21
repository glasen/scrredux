/* scenery/infield.h - infield props */
#pragma once

/* ------------------------------------------------------------ infield props */
typedef struct {
    float x, z, r;
} Spot;
static Spot spots[256];
static int nspots;
static int spot_free(float x, float z, float r, float clear)
{
    if (x < ar_x0 + 12 + r || x > ar_x1 - 12 - r || z < ar_z0 + 12 + r || z > ar_z1 - 12 - r) {
        return 0;
    }
    if (track_dist(x, z, 2) < clear + r) {
        return 0;
    }
    if (seg_dist(x, z, ar_x1, gate_z, svc_x, svc_z) < 5.f + r) {
        return 0;
    }
    for (int i = 0; i < nspots; i++) {
        if (sq(spots[i].x - x) + sq(spots[i].z - z) < sq(spots[i].r + r + 2.f)) {
            return 0;
        }
    }
    return 1;
}
static void take_spot(float x, float z, float r)
{
    if (nspots < 256) {
        spots[nspots++] = (Spot){x, z, r};
    }
}

/* choose the service compound (the infield spot furthest from the track) and the gate before the floor is drawn */
static void plan_arena(void)
{
    float best = -1;
    for (float z = ar_z0 + 40; z < ar_z1 - 40; z += 8) {
        for (float x = ar_x0 + 40; x < ar_x1 - 40; x += 8) {
            float d = track_dist(x, z, 3);
            if (d > best) {
                best = d;
                svc_x = x;
                svc_z = z;
            }
        }
    }
    gate_z = clampf(svc_z, ar_z0 + 60, ar_z1 - 60);
    nspots = 0;
    if (getenv("SCR_DEBUG")) {
        printf("arena x %.0f..%.0f z %.0f..%.0f, service %.0f,%.0f, gate z %.0f\n", ar_x0, ar_x1, ar_z0, ar_z1, svc_x,
               svc_z, gate_z);
    }
}

static void build_infield(void)
{
    rngs = 4242 + cur_track;
    /* service compound: vehicles, cabins, containers, toilets */
    Frm f;
    float ang = atan2f(gate_z - svc_z, ar_x1 - svc_x);
    float ca = cosf(ang), sa = sinf(ang);
    struct {
        float u, v, a, r;
        int kind;
    } lay[] = {{-6, 12, PI_F / 2, 4, 0},  {2, 12, PI_F / 2, 4, 1}, {10, 12, PI_F / 2, 4, 2}, {-6, -12, -PI_F / 2, 4, 3},
               {4, -12, -PI_F / 2, 3, 4}, {-16, 2, 0, 4, 5},       {-16, -6, 0, 4, 6},       {-24, 6, PI_F / 2, 4, 7},
               {-24, -4, PI_F / 2, 4, 8}, {14, -12, 0, 3, 9},      {-32, 1, 0, 2, 10}};
    for (unsigned i = 0; i < sizeof lay / sizeof lay[0]; i++) {
        float u = lay[i].u, v = lay[i].v;
        float x = svc_x + ca * u - sa * v, z = svc_z + sa * u + ca * v;
        if (track_dist(x, z, 2) < 12 + lay[i].r) {
            continue;
        }
        f = frm_ground(x, z, ang + lay[i].a);
        switch (lay[i].kind) {
            case 0:
                tow_truck(&f);
                break;
            case 1:
                tanker(&f);
                break;
            case 2:
                ambulance(&f);
                break;
            case 3:
                flatbed(&f);
                break;
            case 4:
                pickup(&f, 0x2A5AB0);
                break;
            case 5:
                cabin(&f, 0xE8E8E0);
                break;
            case 6:
                cabin(&f, 0xE8E8E0);
                break;
            case 7:
                container(&f, 0x2E6AA8);
                break;
            case 8:
                container(&f, 0xB8442C);
                break;
            case 9:
                tv_truck(&f);
                break;
            case 10:
                toilets(&f, 5);
                break;
        }
        take_spot(x, z, lay[i].r + 2);
    }
    /* scattered: TV towers and marshal posts beside the track, tyre stacks, hay bales, girders, dirt heaps */
    for (int kind = 0; kind < 7; kind++) {
        int want = (int[]){3, 5, 7, 6, 2, 4, 3}[kind], tries = 0;
        for (int n = 0; n < want && tries < 3000; tries++) {
            int k = (int)(frand() * nseg) % nseg;
            float x, z, a = frand() * 2 * PI_F;
            if (kind <= 1) { /* right beside the track, facing it */
                float side = frand() < 0.5f ? -1.f : 1.f, off = 16.f + frand() * 6.f;
                x = seg[k].c.x + seg[k].R.x * side * off;
                z = seg[k].c.z + seg[k].R.z * side * off;
                a = atan2f(-seg[k].R.z * side, -seg[k].R.x * side);
            } else {
                x = ar_x0 + 20 + frand() * (ar_x1 - ar_x0 - 40);
                z = ar_z0 + 20 + frand() * (ar_z1 - ar_z0 - 40);
            }
            float r = (float[]){3, 2, 3, 4, 8, 3, 5}[kind];
            if (!spot_free(x, z, r, kind <= 1 ? 13.f : 15.f)) {
                continue;
            }
            take_spot(x, z, r);
            f = frm_ground(x, z, a);
            switch (kind) {
                case 0:
                    tv_tower(&f);
                    break;
                case 1:
                    marshal_post(&f);
                    break;
                case 2: /* a row of tyre stacks */
                    for (int q = 0; q < 5; q++) {
                        tyre_stack(fpt(&f, 0, 0, (q - 2) * 0.75f), 3 + (hash2(q, n) & 1));
                    }
                    break;
                case 3: /* hay bales, some stacked */
                    hay_bale(&f, -0.7f, 0, 0);
                    hay_bale(&f, 0.7f, 0, 0);
                    hay_bale(&f, 0, 1.6f, 0);
                    if (n & 1) {
                        hay_bale(&f, 0, 0, 1.25f);
                    }
                    break;
                case 4:
                    girder_pile(&f);
                    break;
                case 5:
                    dirt_mound(x, z, 3.5f + frand() * 2, 1.4f + frand());
                    break;
                case 6:
                    container(&f, (n & 1) ? 0x3E8A4A : 0xD8A030);
                    break;
            }
            n++;
        }
    }
}
