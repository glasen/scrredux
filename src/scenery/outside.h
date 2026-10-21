/* scenery/outside.h - car parks, trees and the surroundings */
#pragma once

/* ------------------------------------------------------------ outside */
static void tree(float x, float z, float s, int kind)
{
    float g = ground_h(x, z);
    cyl(V(x, g, z), V(x, g + 2.2f * s, z), 0.25f * s, 0.2f * s, 5, 0x5A4030, 0);
    uint32_t c = kind ? 0x2E5A2C : 0x3E6E30;
    if (kind) { /* conifer */
        cyl(V(x, g + 1.5f * s, z), V(x, g + 6.5f * s, z), 2.0f * s, 0.05f, 7, c, 1);
        cyl(V(x, g + 4.0f * s, z), V(x, g + 8.5f * s, z), 1.4f * s, 0.05f, 7, scalecol(c, 1.08f), 1);
    } else { /* broadleaf: a lumpy faceted crown */
        cyl(V(x, g + 1.8f * s, z), V(x, g + 3.6f * s, z), 1.6f * s, 2.6f * s, 7, c, 1);
        cyl(V(x, g + 3.6f * s, z), V(x, g + 6.2f * s, z), 2.6f * s, 0.6f * s, 7, scalecol(c, 1.1f), 2);
    }
}
static void parked_car(V3 p, V3 F, uint32_t col)
{
    V3 R = vcross(F, UP);
    obox(vadd(p, V(0, 0.55f, 0)), R, UP, F, 0.86f, 0.32f, 2.1f, col);
    obox(vadd(vadd(p, V(0, 1.12f, 0)), vmul(F, -0.2f)), R, UP, F, 0.78f, 0.26f, 1.1f, GLASS);
    obox(vadd(vadd(p, V(0, 1.39f, 0)), vmul(F, -0.2f)), R, UP, F, 0.76f, 0.02f, 1.0f, col);
}
static void build_outside(void)
{
    rngs = 777 + cur_track;
    static const uint32_t cc[10] = {0xC82820, 0x2A5AB0, 0xE8E8E8, 0x2A2A2E, 0x9A9AA2,
                                    0xE0C030, 0x2E7A3E, 0x7A2A2A, 0x4A6AA0, 0xD8D0C0};
    float cx = (ar_x0 + ar_x1) / 2, cz = (ar_z0 + ar_z1) / 2;
    /* car parks behind the long stands */
    for (int side = 0; side < 2; side++) {
        float zl = side ? ar_z1 + 62 : ar_z0 - 62 - 70, x0 = cx - 160, x1 = cx + 160;
        draw_quad(V(x0, 0.04f, zl), V(x1, 0.04f, zl), V(x1, 0.04f, zl + 70), V(x0, 0.04f, zl + 70), 0x55555A);
        for (int row = 0; row < 6; row++) {
            float z = zl + 6 + row * 11.5f + (row & 1) * 5.0f;
            for (float x = x0 + 4; x < x1 - 4; x += 2.9f) {
                draw_quad(V(x - 1.42f, 0.06f, z - 2.4f), V(x - 1.32f, 0.06f, z - 2.4f), V(x - 1.32f, 0.06f, z + 2.4f),
                          V(x - 1.42f, 0.06f, z + 2.4f), 0xD8D8D8);
                uint32_t h = hash2((int)(x * 3), row + side * 17);
                if ((h & 7) < 2) {
                    continue;
                }
                parked_car(V(x, 0.04f, z), V(0, 0, (row & 1) ? -1.f : 1.f), cc[(h >> 4) % 10]);
            }
        }
    }
    /* trees in groups around the outside, keeping clear of the car parks and big tops */
    for (int n = 0, tries = 0; n < 420 && tries < 6000; tries++) {
        float a = frand() * 2 * PI_F, d = 60 + frand() * 260;
        float x = cx + cosf(a) * ((ar_x1 - ar_x0) / 2 + d), z = cz + sinf(a) * ((ar_z1 - ar_z0) / 2 + d);
        if (x > ar_x0 - 55 && x < ar_x1 + 55 && z > ar_z0 - 55 && z < ar_z1 + 55) {
            continue;
        }
        if (fabsf(x - cx) < 175 && ((z > ar_z1 + 55 && z < ar_z1 + 140) || (z < ar_z0 - 55 && z > ar_z0 - 140))) {
            continue;
        }
        if (vnoise(x / 70.f, z / 70.f, 31) < 0.45f) {
            continue;
        }
        int skip = 0;
        static const float bt[4][2] = {{-70, -70}, {70, -70}, {70, 70}, {-70, 70}};
        for (int k = 0; k < 4; k++) {
            float bx = (bt[k][0] < 0 ? ar_x0 : ar_x1) + bt[k][0], bz = (bt[k][1] < 0 ? ar_z0 : ar_z1) + bt[k][1];
            if (sq(x - bx) + sq(z - bz) < sq(45)) {
                skip = 1;
            }
        }
        if (fabsf(z - gate_z) < 12 && x > ar_x1) {
            skip = 1;
        }
        if (skip) {
            continue;
        }
        tree(x, z, 0.9f + frand() * 0.8f, frand() < 0.4f);
        n++;
    }
}
