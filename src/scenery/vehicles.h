/* scenery/vehicles.h - service vehicles */
#pragma once

/* ------------------------------------------------------------ vehicles */
static void vwheel(const Frm *f, float x, float z, float r, float w)
{
    float s = x < 0 ? -1.f : 1.f;
    fcyl(f, x - w / 2, r, z, x + w / 2, r, z, r, 12, 0x1C1C1E, 3);
    fcyl(f, x + s * w / 2, r, z, x + s * (w / 2 + 0.03f), r, z, r * 0.62f, 10, 0xB4B4BC, 2);
    fcyl(f, x + s * (w / 2 + 0.03f), r, z, x + s * (w / 2 + 0.09f), r, z, r * 0.2f, 6, 0x5A5A62, 2);
}
/* cab of a heavy truck with its front face at z = zf */
static void truck_cab(const Frm *f, float zf, uint32_t col, int beacon)
{
    const float zr = zf - 2.1f, W = 1.22f;
    const float p[5][2] = {{zr, 0.95f}, {zf, 0.95f}, {zf, 1.95f}, {zf - 0.42f, 3.0f}, {zr, 3.0f}};
    fprism(f, p, 5, -W, W, col);
    const float nz = 0.928f, ny = 0.371f, o = 0.03f;           /* outward normal of the sloped windscreen face */
    #define WS(x, t) fpt(f, x, 1.95f + 1.05f * (t) + o * ny, zf - 0.42f * (t) + o * nz)
    draw_quad(WS(-1.06f, 0.1f), WS(1.06f, 0.1f), WS(1.06f, 0.9f), WS(-1.06f, 0.9f), GLASS);
    #undef WS
    for (int s = -1; s <= 1; s += 2) {
        float x = s * (W + 0.02f);
        fpanel_x(f, x, zr + 0.35f, 2.0f, zf - 0.08f, 2.0f, zf - 0.39f, 2.78f, zr + 0.35f, 2.78f, GLASS);
        fbox(f, s * 1.05f, s * 1.05f + s * 0.1f, 1.95f, 2.45f, zf - 0.35f, zf - 0.2f, 0x202024);   /* mirror */
        fbox(f, s * 0.95f - 0.25f, s * 0.95f + 0.25f, 0.55f, 0.75f, zr + 0.6f, zr + 1.4f, 0x2A2A2E); /* step */
    }
    fbox(f, -0.95f, 0.95f, 1.08f, 1.8f, zf, zf + 0.04f, 0x2A2A2E);                              /* grille */
    fbox(f, -1.25f, 1.25f, 0.5f, 0.88f, zf - 0.1f, zf + 0.2f, 0x3A3A40);                          /* bumper */
    for (int s = -1; s <= 1; s += 2) fbox(f, s * 0.78f - 0.17f, s * 0.78f + 0.17f, 0.6f, 0.78f, zf + 0.2f, zf + 0.23f, 0xF4F0D0);
    if (beacon) {
        fbox(f, -0.75f, 0.75f, 3.0f, 3.08f, zr + 0.6f, zr + 0.9f, 0x2A2A2E);
        for (int s = -1; s <= 1; s += 2) fbox(f, s * 0.5f - 0.18f, s * 0.5f + 0.18f, 3.08f, 3.3f, zr + 0.62f, zr + 0.88f, 0xF09018);
    }
}
static void truck_chassis(const Frm *f, float z0, float z1, float zfw, const float *rear, int nrear)
{
    fbox(f, -0.5f, 0.5f, 0.55f, 0.92f, z0, z1, 0x1E1E22);
    for (int s = -1; s <= 1; s += 2) {
        vwheel(f, s * 1.0f, zfw, 0.52f, 0.36f);
        for (int k = 0; k < nrear; k++) vwheel(f, s * 0.96f, rear[k], 0.52f, 0.5f);
        fbox(f, s * 1.2f - 0.02f, s * 1.2f + 0.02f, 0.25f, 0.8f, rear[nrear - 1] - 0.75f, rear[nrear - 1] - 0.7f, 0x18181A);  /* mud flap */
    }
}

/* recovery truck with a lifting boom (it fetches the wrecks) */
static void tow_truck(const Frm *f)
{
    const float rear[2] = {-1.6f, -2.75f};
    uint32_t col = 0xC8241C;
    truck_chassis(f, -3.6f, 3.3f, 2.3f, rear, 2);
    truck_cab(f, 3.45f, col, 1);
    fbox(f, -1.2f, 1.2f, 0.95f, 1.75f, -3.6f, 1.15f, 0xDCDCE0);                                    /* tool lockers */
    fbox(f, -1.215f, 1.215f, 1.3f, 1.45f, -3.615f, 1.165f, col);
    for (int s = -1; s <= 1; s += 2)
        for (int k = 0; k < 3; k++) fpanel_x(f, s * 1.225f, -3.3f + k * 1.5f, 1.0f, -2.0f + k * 1.5f, 1.0f, -2.0f + k * 1.5f, 1.27f, -3.3f + k * 1.5f, 1.27f, 0xB8B8BE);
    fcyl(f, -0.85f, 1.75f, -2.4f, -0.12f, 3.7f, -1.3f, 0.1f, 6, 0x2E2E34, 0);                      /* A-frame */
    fcyl(f, 0.85f, 1.75f, -2.4f, 0.12f, 3.7f, -1.3f, 0.1f, 6, 0x2E2E34, 0);
    fcyl(f, 0, 3.6f, -1.0f, 0, 2.7f, -4.6f, 0.16f, 8, 0xE8C020, 3);                                /* boom */
    fcyl(f, 0, 2.62f, -4.55f, 0, 1.25f, -4.55f, 0.04f, 4, 0x202020, 0);                           /* cable and hook */
    fbox(f, -0.12f, 0.12f, 1.0f, 1.25f, -4.62f, -4.48f, 0x303034);
    fbox(f, -1.0f, 1.0f, 0.55f, 0.75f, -3.9f, -3.6f, 0x2A2A2E);                                    /* under-lift */
}
/* water bowser for keeping the dust down */
static void tanker(const Frm *f)
{
    const float rear[2] = {-1.4f, -2.6f};
    truck_chassis(f, -3.5f, 3.3f, 2.3f, rear, 2);
    truck_cab(f, 3.45f, 0x2A5AB0, 0);
    fbox(f, -1.1f, 1.1f, 0.92f, 1.05f, -3.5f, 1.25f, 0x2A2A2E);
    fcyl(f, 0, 2.0f, -3.4f, 0, 2.0f, 1.15f, 0.98f, 14, 0xD8DCE0, 3);
    for (int k = 0; k < 3; k++) fcyl(f, 0, 2.0f, -2.6f + k * 1.4f, 0, 2.0f, -2.5f + k * 1.4f, 1.01f, 14, 0x9A9EA6, 0);
    fbox(f, -0.4f, 0.4f, 2.95f, 3.15f, -1.4f, -0.6f, 0x9A9EA6);                                    /* hatch */
    fcyl(f, -1.0f, 0.75f, -3.6f, 1.0f, 0.75f, -3.6f, 0.06f, 6, 0x2A2A2E, 3);                       /* spray bar */
    world_text_c(fpt(f, 1.0f, 2.05f, -1.15f), vmul(f->F, 1), UP, "WATER", 0.09f, 0x2A5AB0, 0);
    world_text_c(fpt(f, -1.0f, 2.05f, -1.15f), vmul(f->F, -1), UP, "WATER", 0.09f, 0x2A5AB0, 0);
}
/* flatbed carrying spare track girders */
static void flatbed(const Frm *f)
{
    const float rear[2] = {-1.5f, -2.7f};
    truck_chassis(f, -4.2f, 3.3f, 2.3f, rear, 2);
    truck_cab(f, 3.45f, 0xE8E8E8, 0);
    fbox(f, -1.22f, 1.22f, 0.92f, 1.18f, -4.4f, 1.2f, 0x5A4632);
    for (int k = 0; k < 3; k++) {                                                                  /* girder sections */
        float x = -0.8f + k * 0.8f;
        for (int s = 0; s < 2; s++) fbox(f, x - 0.32f, x + 0.32f, 1.18f + s * 0.66f, 1.8f + s * 0.66f, -4.2f, 0.9f, (k + s) & 1 ? COL_RED : 0xE6E6E6);
    }
    for (int k = 0; k < 2; k++) fcyl(f, -1.25f, 2.48f, -3.0f + k * 3.0f, 1.25f, 2.48f, -3.0f + k * 3.0f, 0.04f, 4, 0xE8A020, 0);   /* straps */
}
/* outside-broadcast van with a dish */
static void tv_truck(const Frm *f)
{
    const float rear[2] = {-1.7f, -2.8f};
    truck_chassis(f, -3.8f, 3.3f, 2.3f, rear, 2);
    truck_cab(f, 3.45f, 0xF0F0F0, 0);
    fbox(f, -1.25f, 1.25f, 0.95f, 3.5f, -4.0f, 1.2f, 0xF0F0F0);
    for (int s = -1; s <= 1; s += 2) {
        fpanel_x(f, s * 1.26f, -3.9f, 1.4f, 1.1f, 1.4f, 1.1f, 1.75f, -3.9f, 1.75f, 0x2858C0);
        world_text_c(fpt(f, s * 1.27f, 2.6f, -1.4f), vmul(f->F, (float)s), UP, "TV 1", 0.16f, 0x2858C0, 0);
    }
    fcyl(f, 0, 3.5f, -2.6f, 0, 4.1f, -2.6f, 0.08f, 6, 0x9A9AA2, 0);
    fcyl(f, 0, 4.25f, -2.5f, 0, 4.55f, -2.95f, 0.85f, 12, 0xE0E0E4, 3);                           /* dish */
    fcyl(f, 0, 3.5f, 0.2f, 0, 6.5f, 0.2f, 0.07f, 5, 0x9A9AA2, 0);                                 /* mast */
}
/* pickup truck */
static void pickup(const Frm *f, uint32_t col)
{
    const float lo[5][2] = {{-2.6f, 0.45f}, {2.45f, 0.45f}, {2.6f, 0.78f}, {2.45f, 1.08f}, {-2.6f, 1.08f}};
    fprism(f, lo, 5, -0.95f, 0.95f, col);
    const float cab[4][2] = {{-0.75f, 1.08f}, {1.05f, 1.08f}, {0.3f, 1.78f}, {-0.7f, 1.78f}};
    fprism(f, cab, 4, -0.88f, 0.88f, col);
    for (int s = -1; s <= 1; s += 2) {
        fpanel_x(f, s * 0.89f, -0.62f, 1.15f, 0.88f, 1.15f, 0.3f, 1.7f, -0.62f, 1.7f, GLASS);
        fbox(f, s * 0.95f - 0.06f, s * 0.95f, 1.08f, 1.5f, -2.6f, -0.75f, col);                      /* bed sides */
        vwheel(f, s * 0.82f, 1.55f, 0.4f, 0.3f);
        vwheel(f, s * 0.82f, -1.65f, 0.4f, 0.3f);
        fbox(f, s * 0.62f - 0.16f, s * 0.62f + 0.16f, 0.82f, 0.95f, 2.5f, 2.62f, 0xF4F0D0);
    }
    {   /* windscreen */
        const float nz = 0.68f, ny = 0.73f, o = 0.03f;
        #define WS(x, t) fpt(f, x, 1.08f + 0.7f * (t) + o * ny, 1.05f - 0.75f * (t) + o * nz)
        draw_quad(WS(-0.8f, 0.08f), WS(0.8f, 0.08f), WS(0.8f, 0.92f), WS(-0.8f, 0.92f), GLASS);
        #undef WS
    }
    fbox(f, -0.9f, 0.9f, 1.08f, 1.5f, -2.66f, -2.6f, col);                                          /* tailgate */
    fbox(f, -0.95f, 0.95f, 0.38f, 0.6f, 2.55f, 2.75f, 0x3A3A40);
    fbox(f, -0.95f, 0.95f, 0.38f, 0.6f, -2.75f, -2.55f, 0x3A3A40);
}
/* ambulance */
static void ambulance(const Frm *f)
{
    const float p[6][2] = {{-3.0f, 0.5f}, {2.55f, 0.5f}, {2.7f, 1.15f}, {1.9f, 1.4f}, {1.35f, 2.7f}, {-3.0f, 2.7f}};
    fprism(f, p, 6, -1.05f, 1.05f, 0xF4F4F0);
    for (int s = -1; s <= 1; s += 2) {
        fpanel_x(f, s * 1.06f, -3.0f, 1.3f, 2.2f, 1.3f, 2.2f, 1.6f, -3.0f, 1.6f, 0xD02020);
        fpanel_x(f, s * 1.06f, 0.45f, 1.75f, 1.75f, 1.75f, 1.45f, 2.45f, 0.45f, 2.45f, GLASS);
        vwheel(f, s * 0.9f, 1.75f, 0.42f, 0.3f);
        vwheel(f, s * 0.9f, -2.0f, 0.42f, 0.3f);
        fbox(f, s * 0.7f - 0.2f, s * 0.7f + 0.2f, 0.9f, 1.05f, 2.6f, 2.7f, 0xF4F0D0);
    }
    {
        const float nz = 0.92f, ny = 0.39f, o = 0.03f;
        #define WS(x, t) fpt(f, x, 1.4f + 1.3f * (t) + o * ny, 1.9f - 0.55f * (t) + o * nz)
        draw_quad(WS(-0.95f, 0.1f), WS(0.95f, 0.1f), WS(0.95f, 0.85f), WS(-0.95f, 0.85f), GLASS);
        #undef WS
    }
    fbox(f, -0.6f, 0.6f, 2.7f, 2.9f, 0.9f, 1.2f, 0x2050E0);                                         /* light bar */
    fbox(f, -1.05f, 1.05f, 0.4f, 0.62f, 2.55f, 2.8f, 0x3A3A40);
}

/* mobile crane carrier (the crane tower stands on its rear deck) */
static void crane_carrier(const Frm *f)
{
    const float rear[2] = {-1.2f, -2.5f};
    truck_chassis(f, -3.8f, 3.3f, 2.3f, rear, 2);
    vwheel(f, -1.0f, 0.9f, 0.52f, 0.36f); vwheel(f, 1.0f, 0.9f, 0.52f, 0.36f);
    truck_cab(f, 3.45f, 0xE8B818, 1);
    fbox(f, -1.25f, 1.25f, 0.95f, 1.35f, -3.9f, 1.3f, 0xE8B818);
    fbox(f, -1.26f, 1.26f, 1.0f, 1.12f, -3.91f, 1.31f, 0x2A2A2E);
    for (int s = -1; s <= 1; s += 2)
        for (int e = 0; e < 2; e++) {                                                              /* outriggers */
            float z = e ? 0.9f : -3.4f;
            fbox(f, s * 1.25f, s * 2.6f, 0.75f, 1.0f, z - 0.2f, z + 0.2f, 0xE0E0E0);
            fbox(f, s * 2.6f - 0.12f, s * 2.6f + 0.12f, 0.05f, 0.95f, z - 0.12f, z + 0.12f, 0x3A3A40);
            fbox(f, s * 2.6f - 0.35f, s * 2.6f + 0.35f, 0, 0.08f, z - 0.35f, z + 0.35f, 0x3A3A40);
        }
}
