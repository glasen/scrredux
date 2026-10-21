/* scenery/landscape.h - big tops, flags and mountains */
#pragma once

/* ---- stadium ---- */
static void flag(V3 base, float h, uint32_t col);

/* circus big top like the ones around the original's stadium: striped wall, striped cone roof, flag on the king pole */
static void big_top(float x, float z, float R, float wallh, float peak)
{
    enum { NS = 28 };

    float g = ground_h(x, z);
    for (int k = 0; k < NS; k++) {
        float a0 = k * 2 * PI_F / NS, a1 = (k + 1) * 2 * PI_F / NS;
        V3 b0 = V(x + cosf(a0) * R, g, z + sinf(a0) * R), b1 = V(x + cosf(a1) * R, g, z + sinf(a1) * R);
        V3 t0 = vadd(b0, V(0, wallh, 0)), t1 = vadd(b1, V(0, wallh, 0));
        uint32_t c = (k & 1) ? 0xF4F4F0 : 0xC81E18;
        draw_quad(b0, b1, t1, t0, c);
        V3 e0 = V(x + cosf(a0) * R * 1.06f, g + wallh - 0.6f, z + sinf(a0) * R * 1.06f),
           e1 = V(x + cosf(a1) * R * 1.06f, g + wallh - 0.6f, z + sinf(a1) * R * 1.06f);
        draw_quad(t0, t1, e1, e0, c);
        draw_tri(e0, e1, V(x, g + peak, z), c); /* roof gore */
    }
    cyl(V(x, g + peak - 0.5f, z), V(x, g + peak + 7, z), 0.25f, 0.18f, 6, 0xD8D8E0, 3);
    draw_tri(V(x, g + peak + 7, z), V(x, g + peak + 5.4f, z), V(x + 4.5f, g + peak + 6.2f, z), 0xF0D020);
}

static void flag(V3 base, float h, uint32_t col)
{
    cyl(base, vadd(base, V(0, h, 0)), 0.09f, 0.07f, 5, 0xD8D8E0, 3);
    V3 t = vadd(base, V(0, h, 0));
    draw_tri(t, vadd(t, V(0, -1.6f, 0)), vadd(t, V(2.6f, -0.7f, 0.4f)), col);
}

/* real 3D faceted mountains in a ring around the arena (static, lit, fogged) */
static void build_mountains(void)
{
    float cx = (ar_x0 + ar_x1) / 2, cz = (ar_z0 + ar_z1) / 2;
    float rad = 0.5f * sqrtf(sq(ar_x1 - ar_x0) + sq(ar_z1 - ar_z0));
    rngs = 9001 + cur_track;

    enum { NM = 52, NR = 7, NA = 16 };

    for (int m = 0; m < NM; m++) {
        float an = (m + frand() * 0.8f) * 2 * PI_F / NM;
        float dist = rad + 1500 + frand() * 1300;
        float R = 500 + frand() * 600, H = 110 + frand() * 240;
        if (m % 5 == 0) {
            H *= 1.5f;
        }
        float px = cx + cosf(an) * dist, pz = cz + sinf(an) * dist;
        static V3 pt[NR + 1][NA];
        float hz[NR + 1];
        for (int r = 0; r <= NR; r++) {
            float t = (float)r / NR;
            hz[r] = H * (1.f - powf(1.f - t, 1.7f));
            for (int a = 0; a < NA; a++) {
                float aa = 2 * PI_F * a / NA;
                float j1 = (float)(hash2(m * 131 + r, a * 17 + 3) & 255) / 255.f,
                      j2 = (float)(hash2(m * 71 + a, r * 29 + 9) & 255) / 255.f;
                float rr = R * (1.f - t) * (1.f + 0.22f * (j1 - 0.5f)) * (1.f + 0.25f * sinf(aa * 3 + m));
                float hh = hz[r] * (1.f + 0.16f * (j2 - 0.5f) * (r > 0 && r < NR));
                pt[r][a] = V(px + cosf(aa) * rr, hh, pz + sinf(aa) * rr);
            }
        }
        for (int r = 0; r < NR; r++) {
            for (int a = 0; a < NA; a++) {
                int a2 = (a + 1) % NA;
                float hf = (hz[r] + hz[r + 1]) * 0.5f / H;
                uint32_t col = hf > 0.74f ? 0xF2F2FA : hf > 0.5f ? 0x8E8C96 : hf > 0.22f ? 0x6E6A64 : 0x4C5E46;
                col = scalecol(col, 0.88f + 0.24f * (float)(hash2(m * 7 + r, a * 3) & 255) / 255.f);
                draw_quad(pt[r][a], pt[r][a2], pt[r + 1][a2], pt[r + 1][a], col);
            }
        }
    }
}
