/* scenery/props.h - arena props */
#pragma once

/* ------------------------------------------------------------- arena props */
static void container(const Frm *f, uint32_t col)
{
    fbox(f, -1.22f, 1.22f, 0, 2.6f, -3.05f, 3.05f, col);
    uint32_t rib = scalecol(col, 0.8f);

    for (int s = -1; s <= 1; s += 2) {
        for (int k = 0; k < 9; k++) {
            fbox(f, s * 1.22f - 0.04f, s * 1.22f + 0.04f, 0.1f, 2.5f, -2.7f + k * 0.675f - 0.08f,
                 -2.7f + k * 0.675f + 0.08f, rib);
        }
    }

    fbox(f, -1.26f, 1.26f, 0, 0.12f, -3.1f, 3.1f, rib);
    fbox(f, -1.26f, 1.26f, 2.48f, 2.6f, -3.1f, 3.1f, rib);

    for (int s = -1; s <= 1; s += 2) {
        fcyl(f, s * 0.35f, 0.2f, 3.08f, s * 0.35f, 2.4f, 3.08f, 0.035f, 4, 0x505058, 0); /* door bars */
    }
}

static void cabin(const Frm *f, uint32_t col)
{
    fbox(f, -1.25f, 1.25f, 0.3f, 2.9f, -3.2f, 3.2f, col);
    fbox(f, -1.35f, 1.35f, 2.9f, 3.05f, -3.3f, 3.3f, 0x7A7A82);
    fbox(f, -1.2f, 1.2f, 0, 0.3f, -3.0f, 3.0f, 0x3A3A40);

    for (int s = -1; s <= 1; s += 2) {
        for (int k = 0; k < 3; k++) {
            fpanel_x(f, s * 1.26f, -2.6f + k * 1.9f, 1.4f, -1.6f + k * 1.9f, 1.4f, -1.6f + k * 1.9f, 2.3f,
                     -2.6f + k * 1.9f, 2.3f, GLASS);
        }
    }

    fpanel_z(f, 3.21f, -0.45f, 0.45f, 0.3f, 2.3f, 0x5A6A7A); /* door */
    fbox(f, -0.8f, 0.8f, 0, 0.3f, 3.2f, 3.9f, 0x6A6A72);     /* step */
}

static void toilets(const Frm *f, int n)
{
    static const uint32_t tc[3] = {0x2A62C8, 0x2A9A5A, 0x2A62C8};

    for (int k = 0; k < n; k++) {
        float x = (k - (n - 1) * 0.5f) * 1.3f;
        fbox(f, x - 0.58f, x + 0.58f, 0, 2.3f, -0.6f, 0.6f, tc[k % 3]);
        fbox(f, x - 0.62f, x + 0.62f, 2.3f, 2.45f, -0.64f, 0.64f, 0xE8E8E8);
        fpanel_z(f, 0.61f, x - 0.35f, x + 0.35f, 0.15f, 2.0f, scalecol(tc[k % 3], 0.8f));
    }
}

static void tyre_stack(V3 p, int n)
{
    for (int k = 0; k < n; k++) {
        V3 a = vadd(p, V(0, k * 0.24f, 0)), b = vadd(a, V(0, 0.24f, 0));
        cyl(a, b, 0.34f, 0.34f, 10, (k & 1) ? 0x1E1E20 : 0x2A2A2C, 2);
    }

    cyl(vadd(p, V(0, n * 0.24f, 0)), vadd(p, V(0, n * 0.24f + 0.01f, 0)), 0.16f, 0.16f, 8, 0x0E0E10, 2);
}

static void hay_bale(const Frm *f, float x, float z, float y)
{
    fcyl(f, x - 0.6f, y + 0.72f, z, x + 0.6f, y + 0.72f, z, 0.72f, 10, 0xD4B45C, 0);
    fcyl(f, x - 0.6f, y + 0.72f, z, x - 0.61f, y + 0.72f, z, 0.72f, 10, 0xBC9C48, 1);
    fcyl(f, x + 0.6f, y + 0.72f, z, x + 0.61f, y + 0.72f, z, 0.72f, 10, 0xBC9C48, 2);
}

/* scaffold camera tower with the cameraman under an umbrella */
static void tv_tower(const Frm *f)
{
    const float H = 7.0f, w = 1.4f;

    for (int a = -1; a <= 1; a += 2) {
        for (int b = -1; b <= 1; b += 2) {
            fcyl(f, a * w, 0, b * w, a * w, H + 1.0f, b * w, 0.07f, 5, 0xB8B8C0, 0);
        }
    }

    for (int l = 1; l * 2.4f < H + 0.1f; l++) {
        float y = l * 2.4f;
        fcyl(f, -w, y, -w, w, y, -w, 0.05f, 4, 0xB8B8C0, 0);
        fcyl(f, -w, y, w, w, y, w, 0.05f, 4, 0xB8B8C0, 0);
        fcyl(f, -w, y, -w, -w, y, w, 0.05f, 4, 0xB8B8C0, 0);
        fcyl(f, w, y, -w, w, y, w, 0.05f, 4, 0xB8B8C0, 0);
        fcyl(f, -w, y - 2.4f, -w, w, y, -w, 0.04f, 4, 0xA0A0A8, 0);
        fcyl(f, w, y - 2.4f, w, -w, y, w, 0.04f, 4, 0xA0A0A8, 0);
    }

    fbox(f, -w - 0.1f, w + 0.1f, H - 0.1f, H, -w - 0.1f, w + 0.1f, 0x6A5A44);
    fbox(f, -0.3f, 0.3f, H, H + 0.85f, -0.6f, 0.0f, 0x2A62C8); /* cameraman */
    fbox(f, -0.14f, 0.14f, H + 0.85f, H + 1.1f, -0.42f, -0.18f, 0xD8A882);
    fbox(f, -0.22f, 0.22f, H + 0.85f, H + 1.15f, 0.1f, 0.85f, 0x2A2A2E); /* camera */
    fcyl(f, 0, H + 1.0f, 0.85f, 0, H + 1.0f, 1.05f, 0.11f, 6, 0x1A1A1E, 3);
    fcyl(f, 0, H, -0.3f, 0, H + 2.6f, -0.3f, 0.03f, 4, 0x5A5A62, 0);
    cyl(fpt(f, 0, H + 2.2f, -0.3f), fpt(f, 0, H + 2.6f, -0.3f), 1.3f, 0.05f, 8, 0xE8C020, 1); /* umbrella */
}

static void marshal_post(const Frm *f)
{
    fbox(f, -1.0f, 1.0f, 0, 1.1f, -1.0f, 1.0f, 0xE87020);
    fbox(f, -1.0f, -0.9f, 1.1f, 2.2f, -1.0f, -0.9f, 0xE87020);
    fbox(f, 0.9f, 1.0f, 1.1f, 2.2f, -1.0f, -0.9f, 0xE87020);
    fbox(f, -1.0f, -0.9f, 1.1f, 2.2f, 0.9f, 1.0f, 0xE87020);
    fbox(f, 0.9f, 1.0f, 1.1f, 2.2f, 0.9f, 1.0f, 0xE87020);
    fbox(f, -1.15f, 1.15f, 2.2f, 2.4f, -1.15f, 1.15f, 0xF0F0F0);
    fbox(f, -0.25f, 0.25f, 1.1f, 1.8f, -0.2f, 0.2f, 0xF07020);
    fbox(f, -0.13f, 0.13f, 1.8f, 2.05f, -0.12f, 0.12f, 0xD8A882);
    flag(fpt(f, 1.1f, 2.4f, 1.1f), 2.2f, 0xF0D020);
}

static void girder_pile(const Frm *f)
{
    for (int l = 0; l < 2; l++) {
        for (int k = 0; k < 3 - l; k++) {
            float x = (k - (2 - l) * 0.5f) * 1.3f;
            fbox(f, x - 0.55f, x + 0.55f, 0.15f + l * 1.0f, 1.05f + l * 1.0f, -6.0f, 6.0f,
                 ((k + l) & 1) ? COL_RED : 0xE6E6E6);
        }
    }

    for (int k = -1; k <= 1; k++) {
        fbox(f, -2.2f, 2.2f, 0, 0.15f, k * 4.5f - 0.15f, k * 4.5f + 0.15f, 0x6A5034);
    }
}

static void dirt_mound(float x, float z, float r, float h)
{
    float g = ground_h(x, z);
    V3 top = V(x, g + h, z);

    for (int k = 0; k < 7; k++) {
        float a0 = k * 2 * PI_F / 7, a1 = (k + 1) * 2 * PI_F / 7;
        float r0 = r * (0.85f + 0.3f * (float)(hash2((int)x + k, (int)z) & 255) / 255.f);
        float r1 = r * (0.85f + 0.3f * (float)(hash2((int)x + (k + 1) % 7, (int)z) & 255) / 255.f);
        V3 p0 = V(x + cosf(a0) * r0, 0, z + sinf(a0) * r0), p1 = V(x + cosf(a1) * r1, 0, z + sinf(a1) * r1);
        p0.y = ground_h(p0.x, p0.z) - 0.1f;
        p1.y = ground_h(p1.x, p1.z) - 0.1f;
        draw_tri(p0, p1, top, (k & 1) ? 0x8A6E4A : 0x7E6444);
    }
}
