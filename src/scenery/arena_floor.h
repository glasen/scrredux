/* scenery/arena_floor.h - the arena floor */
#pragma once

/* ------------------------------------------------------------ arena floor */
static float track_dist(float x, float z, int step)
{
    float md = 1e9f;
    for (int k = 0; k < nseg; k += step) md = fminf(md, sq(seg[k].c.x - x) + sq(seg[k].c.z - z));
    return sqrtf(md);
}
static float seg_dist(float x, float z, float ax, float az, float bx, float bz)
{
    float dx = bx - ax, dz = bz - az, t = clampf(((x - ax) * dx + (z - az) * dz) / (dx * dx + dz * dz + 1e-6f), 0, 1);
    return sqrtf(sq(x - ax - dx * t) + sq(z - az - dz * t));
}
static float svc_x, svc_z, gate_z;               /* service compound in the infield, gate in the east wall */

static void build_floor(void)
{
    /* the plain outside the stadium */
    const float T = 40;
    int x0 = (int)floorf((ar_x0 - 2900) / T), x1 = (int)ceilf((ar_x1 + 2900) / T);
    int z0 = (int)floorf((ar_z0 - 2900) / T), z1 = (int)ceilf((ar_z1 + 2900) / T);
    for (int iz = z0; iz < z1; iz++)
        for (int ix = x0; ix < x1; ix++) {
            float cx = (ix + 0.5f) * T, cz = (iz + 0.5f) * T;
            if (cx > ar_x0 && cx < ar_x1 && cz > ar_z0 && cz < ar_z1) continue;
            float n = vnoise(cx / 300.f, cz / 300.f, 11), m = vnoise(cx / 90.f, cz / 90.f, 12);
            uint32_t col = mixcol(0x4E7038, 0x6E7A40, clampf(n * 1.4f - 0.3f, 0, 1));
            col = scalecol(col, 0.9f + 0.16f * m);
            draw_quad(V(ix * T, 0, iz * T), V((ix + 1) * T, 0, iz * T), V((ix + 1) * T, 0, (iz + 1) * T), V(ix * T, 0, (iz + 1) * T), col);
        }
    /* arena floor: grass with mowing stripes and patches, worn dirt under the track, a gravel service road */
    const float t2 = 3.5f;
    float gx = ar_x1, gz = gate_z;
    for (float zz = ar_z0; zz < ar_z1; zz += t2)
        for (float xx = ar_x0; xx < ar_x1; xx += t2) {
            float cx = xx + t2 / 2, cz = zz + t2 / 2;
            uint32_t h = hash2((int)floorf(xx / t2) + 911, (int)floorf(zz / t2));
            float md = track_dist(cx, cz, 3);
            float n1 = vnoise(cx / 40.f, cz / 40.f, 21), n2 = vnoise(cx / 11.f, cz / 11.f, 22), n3 = vnoise(cx / 5.f, cz / 5.f, 23);
            uint32_t col = mixcol(0x3E7C34, 0x5A9442, n1);                                     /* grass */
            col = mixcol(col, 0x7E9446, clampf((n2 - 0.62f) * 3.f, 0, 0.7f));                  /* dry patches */
            if (((int)floorf(cx / 7.f)) & 1) col = scalecol(col, 1.05f); else col = scalecol(col, 0.96f);
            float wear = 11.f + 6.f * n2 + 2.f * n3;
            if (md < wear) {                                                                   /* churned dirt under the track */
                float t = clampf((wear - md) / 3.f, 0, 1);
                col = mixcol(col, mixcol(0x8A7050, 0x6E5A40, n3), t);
            } else if (md < wear + 3.f && (h & 3) == 0) col = mixcol(col, 0x7A7048, 0.5f);
            float rd = seg_dist(cx, cz, gx, gz, svc_x, svc_z);
            if (rd < 3.6f) col = mixcol(0x9A8E76, 0x7A6E58, (rd > 0.9f && rd < 2.2f) ? 0.7f : 0.15f * n3);   /* road with ruts */
            else if (rd < 5.f) col = mixcol(col, 0x8A7E62, 0.5f);
            col = scalecol(col, 0.96f + 0.08f * (float)((h >> 8) & 255) / 255.f);
            float xe = fminf(xx + t2, ar_x1), ze = fminf(zz + t2, ar_z1);
            draw_quad(V(xx, ground_h(xx, zz), zz), V(xe, ground_h(xe, zz), zz), V(xe, ground_h(xe, ze), ze), V(xx, ground_h(xx, ze), ze), col);
        }
    /* concrete apron along the arena wall */
    for (int side = 0; side < 4; side++) {
        float w = 4.f;
        V3 a, b, c, d;
        if (side == 0) { a = V(ar_x0, 0.03f, ar_z0); b = V(ar_x1, 0.03f, ar_z0); c = V(ar_x1, 0.03f, ar_z0 + w); d = V(ar_x0, 0.03f, ar_z0 + w); }
        else if (side == 1) { a = V(ar_x1 - w, 0.03f, ar_z0); b = V(ar_x1, 0.03f, ar_z0); c = V(ar_x1, 0.03f, ar_z1); d = V(ar_x1 - w, 0.03f, ar_z1); }
        else if (side == 2) { a = V(ar_x0, 0.03f, ar_z1 - w); b = V(ar_x1, 0.03f, ar_z1 - w); c = V(ar_x1, 0.03f, ar_z1); d = V(ar_x0, 0.03f, ar_z1); }
        else { a = V(ar_x0, 0.03f, ar_z0); b = V(ar_x0 + w, 0.03f, ar_z0); c = V(ar_x0 + w, 0.03f, ar_z1); d = V(ar_x0, 0.03f, ar_z1); }
        draw_quad(a, b, c, d, 0x9C9A94);
    }
}
