/* render/sky.h - sky dome and mountains that follow the camera */
#pragma once

/* sky dome and mountains follow the camera */
static void draw_sky(void)
{
    static const float el[] = {-90, -3, 0, 4, 9, 16, 26, 40, 58, 90};
    enum { NB = 9, NS = 32 };
    const float R = 5800;
    for (int b = 0; b < NB; b++) {
        float t = b < 2 ? 0.f : clampf((el[b] - 2) / 62.f, 0, 1);
        uint32_t col = mixcol(FOGCOL, ZENCOL, sqrtf(t));
        float e0 = el[b] * PI_F / 180, e1 = el[b + 1] * PI_F / 180;
        for (int s = 0; s < NS; s++) {
            float a0 = s * 2 * PI_F / NS, a1 = (s + 1) * 2 * PI_F / NS;
            V3 p00 = vadd(cam.pos, V(R * cosf(e0) * cosf(a0), R * sinf(e0), R * cosf(e0) * sinf(a0)));
            V3 p01 = vadd(cam.pos, V(R * cosf(e0) * cosf(a1), R * sinf(e0), R * cosf(e0) * sinf(a1)));
            V3 p10 = vadd(cam.pos, V(R * cosf(e1) * cosf(a0), R * sinf(e1), R * cosf(e1) * sinf(a0)));
            V3 p11 = vadd(cam.pos, V(R * cosf(e1) * cosf(a1), R * sinf(e1), R * cosf(e1) * sinf(a1)));
            draw_tri_unlit(p00, p01, p11, col);
            draw_tri_unlit(p00, p11, p10, col);
        }
    }
    /* fair-weather cumulus behind the mountains: clusters of flat puffs facing the camera, white with grey undersides.
     * Every puff sits on its own depth layer (neighbouring clusters too), so overlapping puffs never z-fight. */
    for (int c = 0; c < 22; c++) {
        uint32_t h = hash2(c * 97 + 11, 5);
        float az = c * 2 * PI_F / 22 + (float)(h & 255) / 255.f * 0.25f,
              el = (4.f + (float)((h >> 8) & 255) / 255.f * 16.f) * PI_F / 180;
        V3 dir = V(cosf(el) * cosf(az), sinf(el), cosf(el) * sinf(az));
        V3 rt = vnorm(V(-sinf(az), 0, cosf(az))), up = UP;
        float w = (160 + (float)((h >> 16) & 255) * 1.2f) * 1.4f;
        for (int k = 0; k < 7; k++) {
            uint32_t hk = hash2(c * 13 + k, 77);
            float dist = 5000.f + (c & 1) * 300.f - k * 40.f, sc = dist / 5000.f;
            V3 ctr = vadd(cam.pos, vmul(dir, dist));
            float ox = ((float)(hk & 255) / 255.f - 0.5f) * w * 1.6f * sc,
                  oy = (float)((hk >> 8) & 255) / 255.f * w * 0.28f * sc;
            float r = w * sc * (0.30f + 0.25f * (float)((hk >> 16) & 255) / 255.f);
            V3 pc = vadd(ctr, vadd(vmul(rt, ox), vmul(up, oy)));
            for (int q = 0; q < 10; q++) {
                float a0 = q * 2 * PI_F / 10, a1 = (q + 1) * 2 * PI_F / 10;
                V3 e0 = vadd(pc, vadd(vmul(rt, cosf(a0) * r), vmul(up, fmaxf(sinf(a0), -0.35f) * r * 0.62f)));
                V3 e1 = vadd(pc, vadd(vmul(rt, cosf(a1) * r), vmul(up, fmaxf(sinf(a1), -0.35f) * r * 0.62f)));
                draw_tri_unlit(pc, e0, e1, sinf(a0) + sinf(a1) < -0.2f ? 0xD6DCE6 : 0xFAFAFC);
            }
        }
    }
}
