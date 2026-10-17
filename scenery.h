/* scenery.h - static scenery: the arena floor and its props, service vehicles, the stadium and its surroundings.
 * Included by stuntracer.c; build_static() emits all of it into the static vertex buffer. */

/* ---- local frames: o = origin on the ground, F forward, U up, R across ---- */
typedef struct { V3 o, R, U, F; } Frm;
static Frm frm_at(V3 o, V3 F)
{
    Frm f; f.o = o; f.F = vnorm(V(F.x, 0, F.z)); f.U = UP; f.R = vcross(f.F, UP); return f;
}
static Frm frm_ground(float x, float z, float ang) { return frm_at(V(x, ground_h(x, z), z), V(cosf(ang), 0, sinf(ang))); }
static V3 fpt(const Frm *f, float x, float y, float z) { return vadd(f->o, vadd(vmul(f->R, x), vadd(vmul(f->U, y), vmul(f->F, z)))); }
static void fbox(const Frm *f, float x0, float x1, float y0, float y1, float z0, float z1, uint32_t col)
{
    obox(fpt(f, (x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), f->R, f->U, f->F, (x1 - x0) / 2, (y1 - y0) / 2, (z1 - z0) / 2, col);
}
static void fcyl(const Frm *f, float x0, float y0, float z0, float x1, float y1, float z1, float r, int n, uint32_t col, int caps)
{
    cyl(fpt(f, x0, y0, z0), fpt(f, x1, y1, z1), r, r, n, col, caps);
}
/* side profile ({z, y} pairs, star-shaped around the first point) extruded across x0..x1 */
static void fprism(const Frm *f, const float (*p)[2], int n, float x0, float x1, uint32_t col)
{
    for (int k = 0; k < n; k++) {
        int k2 = (k + 1) % n;
        draw_quad(fpt(f, x0, p[k][1], p[k][0]), fpt(f, x0, p[k2][1], p[k2][0]), fpt(f, x1, p[k2][1], p[k2][0]), fpt(f, x1, p[k][1], p[k][0]), col);
    }
    for (int k = 1; k + 1 < n; k++) {
        draw_tri(fpt(f, x0, p[0][1], p[0][0]), fpt(f, x0, p[k][1], p[k][0]), fpt(f, x0, p[k + 1][1], p[k + 1][0]), col);
        draw_tri(fpt(f, x1, p[0][1], p[0][0]), fpt(f, x1, p[k][1], p[k][0]), fpt(f, x1, p[k + 1][1], p[k + 1][0]), col);
    }
}
/* panel in the plane x = const */
static void fpanel_x(const Frm *f, float x, float z0, float y0, float z1, float y1, float z2, float y2, float z3, float y3, uint32_t col)
{
    draw_quad(fpt(f, x, y0, z0), fpt(f, x, y1, z1), fpt(f, x, y2, z2), fpt(f, x, y3, z3), col);
}
/* panel in the plane z = const */
static void fpanel_z(const Frm *f, float z, float x0, float x1, float y0, float y1, uint32_t col)
{
    draw_quad(fpt(f, x0, y0, z), fpt(f, x1, y0, z), fpt(f, x1, y1, z), fpt(f, x0, y1, z), col);
}

/* bitmap-font lettering on a plane; o = top-left corner, rows run along -up, c = pixel size */
static void world_text(V3 o, V3 al, V3 up, const char *s, float c, uint32_t col, int unlit)
{
    for (; *s; s++, o = vadd(o, vmul(al, 6 * c))) {
        if (*s == ' ') continue;
        const char *p = strchr(FONT_CHARS, *s);
        if (!p) continue;
        const uint8_t *g = FONT[p - FONT_CHARS];
        for (int row = 0; row < 7; row++)
            for (int cx = 0, c0 = -1; cx <= 5; cx++) {
                int on = cx < 5 && (g[row] & (0x10 >> cx));
                if (on && c0 < 0) c0 = cx;
                if (!on && c0 >= 0) {
                    V3 a = vadd(o, vsub(vmul(al, c0 * c), vmul(up, row * c)));
                    V3 b = vadd(a, vmul(al, (cx - c0) * c)), d = vsub(a, vmul(up, c)), e = vsub(b, vmul(up, c));
                    if (unlit) draw_quad_unlit(a, b, e, d, col); else draw_quad(a, b, e, d, col);
                    c0 = -1;
                }
            }
    }
}
/* text centred on m, reading along 'rd' */
static void world_text_c(V3 m, V3 rd, V3 up, const char *s, float c, uint32_t col, int unlit)
{
    float tw = (float)strlen(s) * 6 * c - c;
    world_text(vadd(vsub(m, vmul(rd, tw / 2)), vmul(up, 3.5f * c)), rd, up, s, c, col, unlit);
}

/* ---- sponsors (made up) ---- */
#define NBRAND 10
static const char *BRAND[NBRAND] = {"TURBO OIL", "ACE TYRES", "V8 POWER", "STUNT COLA", "MEGA FUEL",
                                    "GT PARTS", "ROCKET", "HOT RODS", "SPEEDWAY", "BIG RED"};
static const uint32_t BR_BG[NBRAND] = {0x1E50C8, 0xF0C020, 0xC82018, 0xF2F2F2, 0x1E8A3C, 0x202024, 0xE86A18, 0x2858C0, 0xF2F2F2, 0xC81E18};
static const uint32_t BR_FG[NBRAND] = {0xF8F8F8, 0x1A1A1A, 0xF8F8F8, 0xC81E18, 0xF8F0A0, 0xF0C020, 0xF8F8F8, 0xF8E040, 0x1E50C8, 0xF8F8F8};
/* advertising board from a to a + al*w, h tall, readable from the side 'face' points to */
static void ad_board(V3 a, V3 al, V3 face, float w, float h, int b)
{
    b = ((b % NBRAND) + NBRAND) % NBRAND;
    V3 a1 = vadd(a, vmul(al, w));
    draw_quad(a, a1, vadd(a1, V(0, h, 0)), vadd(a, V(0, h, 0)), BR_BG[b]);
    V3 rd = vnorm(vcross(vmul(face, -1), UP));                 /* left to right for a viewer looking at the board */
    float c = fminf(h * 0.6f / 7, w * 0.86f / ((float)strlen(BRAND[b]) * 6));
    V3 m = vadd(vadd(vmul(vadd(a, a1), 0.5f), V(0, h / 2, 0)), vmul(face, 0.04f));
    world_text_c(m, rd, UP, BRAND[b], c, BR_FG[b], 0);
}

/* ------------------------------------------------------------ vehicles */
#define GLASS 0x24344A
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

/* ------------------------------------------------------------- arena props */
static void container(const Frm *f, uint32_t col)
{
    fbox(f, -1.22f, 1.22f, 0, 2.6f, -3.05f, 3.05f, col);
    uint32_t rib = scalecol(col, 0.8f);
    for (int s = -1; s <= 1; s += 2)
        for (int k = 0; k < 9; k++) fbox(f, s * 1.22f - 0.04f, s * 1.22f + 0.04f, 0.1f, 2.5f, -2.7f + k * 0.675f - 0.08f, -2.7f + k * 0.675f + 0.08f, rib);
    fbox(f, -1.26f, 1.26f, 0, 0.12f, -3.1f, 3.1f, rib);
    fbox(f, -1.26f, 1.26f, 2.48f, 2.6f, -3.1f, 3.1f, rib);
    for (int s = -1; s <= 1; s += 2) fcyl(f, s * 0.35f, 0.2f, 3.08f, s * 0.35f, 2.4f, 3.08f, 0.035f, 4, 0x505058, 0);   /* door bars */
}
static void cabin(const Frm *f, uint32_t col)
{
    fbox(f, -1.25f, 1.25f, 0.3f, 2.9f, -3.2f, 3.2f, col);
    fbox(f, -1.35f, 1.35f, 2.9f, 3.05f, -3.3f, 3.3f, 0x7A7A82);
    fbox(f, -1.2f, 1.2f, 0, 0.3f, -3.0f, 3.0f, 0x3A3A40);
    for (int s = -1; s <= 1; s += 2) {
        for (int k = 0; k < 3; k++) fpanel_x(f, s * 1.26f, -2.6f + k * 1.9f, 1.4f, -1.6f + k * 1.9f, 1.4f, -1.6f + k * 1.9f, 2.3f, -2.6f + k * 1.9f, 2.3f, GLASS);
    }
    fpanel_z(f, 3.21f, -0.45f, 0.45f, 0.3f, 2.3f, 0x5A6A7A);                                        /* door */
    fbox(f, -0.8f, 0.8f, 0, 0.3f, 3.2f, 3.9f, 0x6A6A72);                                            /* step */
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
    for (int a = -1; a <= 1; a += 2)
        for (int b = -1; b <= 1; b += 2) fcyl(f, a * w, 0, b * w, a * w, H + 1.0f, b * w, 0.07f, 5, 0xB8B8C0, 0);
    for (int l = 1; l * 2.4f < H + 0.1f; l++) {
        float y = l * 2.4f;
        fcyl(f, -w, y, -w, w, y, -w, 0.05f, 4, 0xB8B8C0, 0); fcyl(f, -w, y, w, w, y, w, 0.05f, 4, 0xB8B8C0, 0);
        fcyl(f, -w, y, -w, -w, y, w, 0.05f, 4, 0xB8B8C0, 0); fcyl(f, w, y, -w, w, y, w, 0.05f, 4, 0xB8B8C0, 0);
        fcyl(f, -w, y - 2.4f, -w, w, y, -w, 0.04f, 4, 0xA0A0A8, 0);
        fcyl(f, w, y - 2.4f, w, -w, y, w, 0.04f, 4, 0xA0A0A8, 0);
    }
    fbox(f, -w - 0.1f, w + 0.1f, H - 0.1f, H, -w - 0.1f, w + 0.1f, 0x6A5A44);
    fbox(f, -0.3f, 0.3f, H, H + 0.85f, -0.6f, 0.0f, 0x2A62C8);                                       /* cameraman */
    fbox(f, -0.14f, 0.14f, H + 0.85f, H + 1.1f, -0.42f, -0.18f, 0xD8A882);
    fbox(f, -0.22f, 0.22f, H + 0.85f, H + 1.15f, 0.1f, 0.85f, 0x2A2A2E);                              /* camera */
    fcyl(f, 0, H + 1.0f, 0.85f, 0, H + 1.0f, 1.05f, 0.11f, 6, 0x1A1A1E, 3);
    fcyl(f, 0, H, -0.3f, 0, H + 2.6f, -0.3f, 0.03f, 4, 0x5A5A62, 0);
    cyl(fpt(f, 0, H + 2.2f, -0.3f), fpt(f, 0, H + 2.6f, -0.3f), 1.3f, 0.05f, 8, 0xE8C020, 1);      /* umbrella */
}
static void marshal_post(const Frm *f)
{
    fbox(f, -1.0f, 1.0f, 0, 1.1f, -1.0f, 1.0f, 0xE87020);
    fbox(f, -1.0f, -0.9f, 1.1f, 2.2f, -1.0f, -0.9f, 0xE87020); fbox(f, 0.9f, 1.0f, 1.1f, 2.2f, -1.0f, -0.9f, 0xE87020);
    fbox(f, -1.0f, -0.9f, 1.1f, 2.2f, 0.9f, 1.0f, 0xE87020); fbox(f, 0.9f, 1.0f, 1.1f, 2.2f, 0.9f, 1.0f, 0xE87020);
    fbox(f, -1.15f, 1.15f, 2.2f, 2.4f, -1.15f, 1.15f, 0xF0F0F0);
    fbox(f, -0.25f, 0.25f, 1.1f, 1.8f, -0.2f, 0.2f, 0xF07020);
    fbox(f, -0.13f, 0.13f, 1.8f, 2.05f, -0.12f, 0.12f, 0xD8A882);
    flag(fpt(f, 1.1f, 2.4f, 1.1f), 2.2f, 0xF0D020);
}
static void girder_pile(const Frm *f)
{
    for (int l = 0; l < 2; l++)
        for (int k = 0; k < 3 - l; k++) {
            float x = (k - (2 - l) * 0.5f) * 1.3f;
            fbox(f, x - 0.55f, x + 0.55f, 0.15f + l * 1.0f, 1.05f + l * 1.0f, -6.0f, 6.0f, ((k + l) & 1) ? COL_RED : 0xE6E6E6);
        }
    for (int k = -1; k <= 1; k++) fbox(f, -2.2f, 2.2f, 0, 0.15f, k * 4.5f - 0.15f, k * 4.5f + 0.15f, 0x6A5034);
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
        p0.y = ground_h(p0.x, p0.z) - 0.1f; p1.y = ground_h(p1.x, p1.z) - 0.1f;
        draw_tri(p0, p1, top, (k & 1) ? 0x8A6E4A : 0x7E6444);
    }
}

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

/* ------------------------------------------------------------ infield props */
typedef struct { float x, z, r; } Spot;
static Spot spots[256]; static int nspots;
static int spot_free(float x, float z, float r, float clear)
{
    if (x < ar_x0 + 12 + r || x > ar_x1 - 12 - r || z < ar_z0 + 12 + r || z > ar_z1 - 12 - r) return 0;
    if (track_dist(x, z, 2) < clear + r) return 0;
    if (seg_dist(x, z, ar_x1, gate_z, svc_x, svc_z) < 5.f + r) return 0;
    for (int i = 0; i < nspots; i++) if (sq(spots[i].x - x) + sq(spots[i].z - z) < sq(spots[i].r + r + 2.f)) return 0;
    return 1;
}
static void take_spot(float x, float z, float r) { if (nspots < 256) spots[nspots++] = (Spot){x, z, r}; }

/* choose the service compound (the infield spot furthest from the track) and the gate before the floor is drawn */
static void plan_arena(void)
{
    float best = -1;
    for (float z = ar_z0 + 40; z < ar_z1 - 40; z += 8)
        for (float x = ar_x0 + 40; x < ar_x1 - 40; x += 8) {
            float d = track_dist(x, z, 3);
            if (d > best) { best = d; svc_x = x; svc_z = z; }
        }
    gate_z = clampf(svc_z, ar_z0 + 60, ar_z1 - 60);
    nspots = 0;
    if (getenv("SR_DEBUG")) printf("arena x %.0f..%.0f z %.0f..%.0f, service %.0f,%.0f, gate z %.0f\n", ar_x0, ar_x1, ar_z0, ar_z1, svc_x, svc_z, gate_z);
}

static void build_infield(void)
{
    rngs = 4242 + cur_track;
    /* service compound: vehicles, cabins, containers, toilets */
    Frm f;
    float ang = atan2f(gate_z - svc_z, ar_x1 - svc_x);
    float ca = cosf(ang), sa = sinf(ang);
    struct { float u, v, a, r; int kind; } lay[] = {
        {-6, 12, PI_F / 2, 4, 0}, {2, 12, PI_F / 2, 4, 1}, {10, 12, PI_F / 2, 4, 2}, {-6, -12, -PI_F / 2, 4, 3},
        {4, -12, -PI_F / 2, 3, 4}, {-16, 2, 0, 4, 5}, {-16, -6, 0, 4, 6}, {-24, 6, PI_F / 2, 4, 7},
        {-24, -4, PI_F / 2, 4, 8}, {14, -12, 0, 3, 9}, {-32, 1, 0, 2, 10}};
    for (unsigned i = 0; i < sizeof lay / sizeof lay[0]; i++) {
        float u = lay[i].u, v = lay[i].v;
        float x = svc_x + ca * u - sa * v, z = svc_z + sa * u + ca * v;
        if (track_dist(x, z, 2) < 12 + lay[i].r) continue;
        f = frm_ground(x, z, ang + lay[i].a);
        switch (lay[i].kind) {
        case 0: tow_truck(&f); break;
        case 1: tanker(&f); break;
        case 2: ambulance(&f); break;
        case 3: flatbed(&f); break;
        case 4: pickup(&f, 0x2A5AB0); break;
        case 5: cabin(&f, 0xE8E8E0); break;
        case 6: cabin(&f, 0xE8E8E0); break;
        case 7: container(&f, 0x2E6AA8); break;
        case 8: container(&f, 0xB8442C); break;
        case 9: tv_truck(&f); break;
        case 10: toilets(&f, 5); break;
        }
        take_spot(x, z, lay[i].r + 2);
    }
    /* scattered: TV towers and marshal posts beside the track, tyre stacks, hay bales, girders, dirt heaps */
    for (int kind = 0; kind < 7; kind++) {
        int want = (int[]){3, 5, 7, 6, 2, 4, 3}[kind], tries = 0;
        for (int n = 0; n < want && tries < 3000; tries++) {
            int k = (int)(frand() * nseg) % nseg;
            float x, z, a = frand() * 2 * PI_F;
            if (kind <= 1) {                                                   /* right beside the track, facing it */
                float side = frand() < 0.5f ? -1.f : 1.f, off = 16.f + frand() * 6.f;
                x = seg[k].c.x + seg[k].R.x * side * off; z = seg[k].c.z + seg[k].R.z * side * off;
                a = atan2f(-seg[k].R.z * side, -seg[k].R.x * side);
            } else {
                x = ar_x0 + 20 + frand() * (ar_x1 - ar_x0 - 40); z = ar_z0 + 20 + frand() * (ar_z1 - ar_z0 - 40);
            }
            float r = (float[]){3, 2, 3, 4, 8, 3, 5}[kind];
            if (!spot_free(x, z, r, kind <= 1 ? 13.f : 15.f)) continue;
            take_spot(x, z, r);
            f = frm_ground(x, z, a);
            switch (kind) {
            case 0: tv_tower(&f); break;
            case 1: marshal_post(&f); break;
            case 2:                                                            /* a row of tyre stacks */
                for (int q = 0; q < 5; q++) tyre_stack(fpt(&f, 0, 0, (q - 2) * 0.75f), 3 + (hash2(q, n) & 1));
                break;
            case 3:                                                            /* hay bales, some stacked */
                hay_bale(&f, -0.7f, 0, 0); hay_bale(&f, 0.7f, 0, 0); hay_bale(&f, 0, 1.6f, 0);
                if (n & 1) hay_bale(&f, 0, 0, 1.25f);
                break;
            case 4: girder_pile(&f); break;
            case 5: dirt_mound(x, z, 3.5f + frand() * 2, 1.4f + frand()); break;
            case 6: container(&f, (n & 1) ? 0x3E8A4A : 0xD8A030); break;
            }
            n++;
        }
    }
}

/* ------------------------------------------------------------ stadium */
static const uint32_t SHIRT[12] = {0x2A2A30, 0x2C3C64, 0xE6E6E6, 0x8C8C98, 0xC23030, 0x3062C0, 0xE0C040, 0x3A8A40,
                                   0xE07028, 0x86AAD8, 0x7A5A3A, 0xB04070};
static const uint32_t SKIN[4] = {0xEEC4A4, 0xD8A882, 0xB07850, 0x7A5032};
static const uint32_t HAIR[4] = {0x2A1E16, 0x5A3A20, 0xC8A050, 0x1A1A1A};
#define CONCRETE 0xA4A29C

/* terraced rows with seats and spectators. Row r: riser at depth d0 + r*rd up to y0 + (r+1)*rh, then the tread. */
static void stand_rows(V3 base, V3 al, V3 out, float len, float d0, float y0, int rows, float rd, float rh,
                       uint32_t seat, int seed, float dens, int vom)
{
    const float pitch = 0.62f, aisle = 1.3f; const int per = 20;
    float blk = per * pitch + aisle;
    int nb = (int)((len - aisle) / blk); if (nb < 1) nb = 1;
    float lead = (len - nb * blk + aisle) * 0.5f;
    #define SP(d, y, a) vadd(base, vadd(vmul(out, d), vadd(V(0, y, 0), vmul(al, a))))
    int vr = rows / 3;                                                          /* vomitory rows vr .. vr+2 */
    for (int r = 0; r < rows; r++) {
        float dr = d0 + r * rd, yt = y0 + (r + 1) * rh;
        uint32_t cc = scalecol(CONCRETE, 0.95f + 0.05f * (r & 1));
        draw_quad(SP(dr, yt - rh, 0), SP(dr, yt - rh, len), SP(dr, yt, len), SP(dr, yt, 0), scalecol(cc, 0.86f));
        draw_quad(SP(dr, yt, 0), SP(dr, yt, len), SP(dr + rd, yt, len), SP(dr + rd, yt, 0), cc);
        for (int b = 0; b < nb; b++) {
            float a0 = lead + b * blk, a1 = a0 + per * pitch;
            int tunnel = vom && (b % 2 == 1) && r >= vr && r < vr + 3;
            float t0 = a0 + per * pitch * 0.5f - 1.4f, t1 = t0 + 2.8f;
            if (!tunnel) draw_quad(SP(dr + rd * 0.8f, yt, a0), SP(dr + rd * 0.8f, yt, a1), SP(dr + rd * 0.8f, yt + 0.42f, a1), SP(dr + rd * 0.8f, yt + 0.42f, a0), seat);
            else {
                draw_quad(SP(dr + rd * 0.8f, yt, a0), SP(dr + rd * 0.8f, yt, t0), SP(dr + rd * 0.8f, yt + 0.42f, t0), SP(dr + rd * 0.8f, yt + 0.42f, a0), seat);
                draw_quad(SP(dr + rd * 0.8f, yt, t1), SP(dr + rd * 0.8f, yt, a1), SP(dr + rd * 0.8f, yt + 0.42f, a1), SP(dr + rd * 0.8f, yt + 0.42f, t1), seat);
            }
            float bd = dens * (0.7f + 0.6f * vnoise(b * 1.7f + seed, r * 0.21f, seed));
            for (int s = 0; s < per; s++) {
                float a = a0 + (s + 0.5f) * pitch;
                if (tunnel && a > t0 - 0.3f && a < t1 + 0.3f) continue;
                uint32_t h = hash2(seed * 7919 + r * 131 + b, s * 17 + seed);
                if ((float)(h & 1023) / 1023.f > bd) continue;
                a += ((float)((h >> 10) & 15) / 15.f - 0.5f) * 0.16f;
                int stand_up = ((h >> 14) & 31) == 0;
                float yb = yt + 0.08f, ys = yb + (stand_up ? 1.0f : 0.56f) + ((float)((h >> 19) & 7) / 7.f - 0.5f) * 0.08f;
                float dp = dr + rd * 0.48f, hw = 0.2f + 0.03f * ((h >> 22) & 1);
                uint32_t shirt = SHIRT[(h >> 23) % 12], skin = SKIN[(h >> 27) & 3];
                draw_quad(SP(dp, yb, a - hw), SP(dp, yb, a + hw), SP(dp, ys, a + hw * 0.9f), SP(dp, ys, a - hw * 0.9f), shirt);
                draw_quad(SP(dp, ys, a - 0.11f), SP(dp, ys, a + 0.11f), SP(dp, ys + 0.24f, a + 0.11f), SP(dp, ys + 0.24f, a - 0.11f), skin);
                if ((h >> 29) & 1) draw_quad(SP(dp - 0.02f, ys + 0.17f, a - 0.12f), SP(dp - 0.02f, ys + 0.17f, a + 0.12f), SP(dp - 0.02f, ys + 0.28f, a + 0.1f), SP(dp - 0.02f, ys + 0.28f, a - 0.1f), HAIR[(h >> 4) & 3]);
                if (stand_up && ((h >> 30) & 1)) {                              /* cheering: arms up */
                    draw_quad(SP(dp, ys - 0.1f, a - hw), SP(dp, ys - 0.1f, a - hw + 0.09f), SP(dp, ys + 0.45f, a - hw - 0.1f), SP(dp, ys + 0.45f, a - hw - 0.19f), skin);
                    draw_quad(SP(dp, ys - 0.1f, a + hw), SP(dp, ys - 0.1f, a + hw - 0.09f), SP(dp, ys + 0.45f, a + hw + 0.1f), SP(dp, ys + 0.45f, a + hw + 0.19f), skin);
                }
            }
            if (vom && (b % 2 == 1) && r == vr) {                               /* tunnel mouth */
                float yb0 = yt - rh, yb1 = y0 + (vr + 3) * rh + 0.5f, dq = dr - 0.02f;
                float dk = d0 + (vr + 3) * rd;
                draw_quad(SP(dk, yb0, t0), SP(dk, yb0, t1), SP(dk, yb1, t1), SP(dk, yb1, t0), 0x18181C);
                for (int e = 0; e < 2; e++) {
                    float ae = e ? t1 : t0;
                    draw_quad(SP(dq, yb0, ae), SP(dk, yb0, ae), SP(dk, yb1, ae), SP(dq, yb1, ae), scalecol(CONCRETE, 0.9f));
                }
                draw_quad(SP(dq, yb1, t0 - 0.3f), SP(dq, yb1, t1 + 0.3f), SP(dk, yb1, t1 + 0.3f), SP(dk, yb1, t0 - 0.3f), CONCRETE);
                draw_quad(SP(dq, yb1 - 0.5f, t0 - 0.3f), SP(dq, yb1 - 0.5f, t1 + 0.3f), SP(dq, yb1, t1 + 0.3f), SP(dq, yb1, t0 - 0.3f), scalecol(CONCRETE, 0.9f));
                cyl(SP(dq, yb1, t0 - 0.3f), SP(dq, yb1 + 0.9f, t0 - 0.3f), 0.03f, 0.03f, 4, 0xD0D0D8, 0);   /* railing */
                cyl(SP(dq, yb1, t1 + 0.3f), SP(dq, yb1 + 0.9f, t1 + 0.3f), 0.03f, 0.03f, 4, 0xD0D0D8, 0);
                cyl(SP(dq, yb1 + 0.9f, t0 - 0.3f), SP(dq, yb1 + 0.9f, t1 + 0.3f), 0.03f, 0.03f, 4, 0xD0D0D8, 0);
            }
        }
    }
    #undef SP
}

/* a grandstand facing the arena: a podium wall, terraced seating, an optional VIP band with an upper tier, a back
 * wall with pilasters and an optional cantilever roof with a coloured fascia. base = front-left corner on the ground. */
static void grandstand(V3 base, V3 al, V3 out, float len, int rows, int upper, int roofed, uint32_t seat, uint32_t accent, int seed)
{
    const float rd = 0.85f, rh = 0.46f, ph = 2.4f;
    #define SP(d, y, a) vadd(base, vadd(vmul(out, d), vadd(V(0, y, 0), vmul(al, a))))
    draw_quad(SP(0, 0, 0), SP(0, 0, len), SP(0, ph + 1.05f, len), SP(0, ph + 1.05f, 0), scalecol(CONCRETE, 0.92f));   /* podium wall */
    draw_quad(SP(0, ph + 1.05f, 0), SP(0, ph + 1.05f, len), SP(0.25f, ph + 1.05f, len), SP(0.25f, ph + 1.05f, 0), 0xC8C6C0);
    draw_quad(SP(0.02f, ph + 0.35f, 0), SP(0.02f, ph + 0.35f, len), SP(0.02f, ph + 0.75f, len), SP(0.02f, ph + 0.75f, 0), accent);
    draw_quad(SP(0.25f, ph, 0), SP(0.25f, ph, len), SP(1.0f, ph, len), SP(1.0f, ph, 0), CONCRETE);    /* front walkway */
    draw_quad(SP(0.25f, 0, 0), SP(0.25f, 0, len), SP(0.25f, ph + 1.05f, len), SP(0.25f, ph + 1.05f, 0), scalecol(CONCRETE, 0.8f));
    stand_rows(base, al, out, len, 1.0f, ph, rows, rd, rh, seat, seed, 0.86f, 1);
    float D = 1.0f + rows * rd, Y = ph + rows * rh, Dt = D, Yt = Y;
    if (upper) {
        const float vb = 3.4f, rh2 = rh * 1.2f;
        draw_quad(SP(D + 0.5f, Y, 0), SP(D + 0.5f, Y, len), SP(D + 0.5f, Y + vb - 0.6f, len), SP(D + 0.5f, Y + vb - 0.6f, 0), 0x34506E);   /* VIP glass */
        for (float a = 0; a <= len + 0.01f; a += 3.f)
            obox(SP(D + 0.45f, Y + (vb - 0.6f) / 2, a), al, UP, out, 0.08f, (vb - 0.6f) / 2, 0.08f, 0xD8D8DC);
        draw_quad(SP(D, Y, 0), SP(D, Y, len), SP(D + 0.5f, Y, len), SP(D + 0.5f, Y, 0), CONCRETE);
        draw_quad(SP(D + 0.3f, Y + vb - 0.6f, 0), SP(D + 0.3f, Y + vb - 0.6f, len), SP(D + 0.3f, Y + vb + 0.9f, len), SP(D + 0.3f, Y + vb + 0.9f, 0), accent);
        draw_quad(SP(D + 0.3f, Y + vb - 0.6f, 0), SP(D + 0.3f, Y + vb - 0.6f, len), SP(D + 0.5f, Y + vb - 0.6f, len), SP(D + 0.5f, Y + vb - 0.6f, 0), 0x50525A);
        int nb = (int)(len / 36);
        for (int k = 0; k < nb; k++)                                                            /* sponsor names on the fascia */
            world_text_c(SP(D + 0.26f, Y + vb + 0.15f, (k + 0.5f) * len / nb), vnorm(vcross(out, UP)), UP,
                         BRAND[(seed + k) % NBRAND], 0.17f, 0xF8F8F8, 0);
        stand_rows(base, al, out, len, D + 0.9f, Y + vb - 0.6f, upper, rd, rh2, seat, seed + 7, 0.78f, 0);
        Dt = D + 0.9f + upper * rd; Yt = Y + vb - 0.6f + upper * rh2;
    }
    /* back wall with pilasters, end walls */
    draw_quad(SP(Dt, 0, 0), SP(Dt, 0, len), SP(Dt, Yt + 1.2f, len), SP(Dt, Yt + 1.2f, 0), 0x8E8C88);
    for (float a = 0; a <= len + 0.01f; a += 7.f) obox(SP(Dt + 0.3f, (Yt + 1.2f) / 2, a), al, UP, out, 0.35f, (Yt + 1.2f) / 2, 0.3f, 0x7E7C78);
    for (int e = 0; e < 2; e++) {
        float a = e ? len : 0;
        draw_quad(SP(0, 0, a), SP(Dt, 0, a), SP(Dt, Yt + 1.2f, a), SP(0, ph + 1.05f, a), scalecol(CONCRETE, 0.86f));
    }
    if (roofed) {
        float yb = Yt + 4.2f, yf = yb + 1.4f, df = -1.0f, db = Dt + 0.6f;
        uint32_t top = 0xB8BCC4, under = 0x4E5058;
        draw_quad(SP(df, yf, 0), SP(df, yf, len), SP(db, yb, len), SP(db, yb, 0), top);
        draw_quad(SP(df, yf - 0.45f, 0), SP(df, yf - 0.45f, len), SP(db, yb - 0.45f, len), SP(db, yb - 0.45f, 0), under);
        draw_quad(SP(df, yf - 1.1f, 0), SP(df, yf - 1.1f, len), SP(df, yf + 0.3f, len), SP(df, yf + 0.3f, 0), accent);  /* fascia */
        draw_quad(SP(df, yf - 1.1f, 0), SP(df, yf - 1.1f, len), SP(df + 0.5f, yf - 0.45f, len), SP(df + 0.5f, yf - 0.45f, 0), scalecol(accent, 0.7f));
        for (int e = 0; e < 2; e++) {
            float a = e ? len : 0;
            draw_quad(SP(df, yf - 1.1f, a), SP(df, yf + 0.3f, a), SP(db, yb, a), SP(db, yb - 0.45f, a), scalecol(top, 0.8f));
        }
        for (int k = 0; k * 22.f < len + 1; k++) {                                              /* trusses and back columns */
            float a = fminf(k * 22.f, len);
            cyl(SP(db + 0.2f, 0, a), SP(db + 0.2f, yb + 5.5f, a), 0.32f, 0.26f, 8, 0xD8D8DE, 3);
            cyl(SP(db + 0.2f, yb + 5.4f, a), SP(df + 0.5f, yf + 0.25f, a), 0.12f, 0.12f, 6, 0xD8D8DE, 0);
            cyl(SP(db + 0.2f, yb - 0.5f, a), SP(df + 0.5f, yf - 0.5f, a), 0.18f, 0.14f, 6, 0x8A8C94, 0);
        }
        for (int k = 0; k * 11.f < len - 4; k++)                                                /* flags along the roof */
            flag(SP(df + 0.3f, yf + 0.3f, k * 11.f + 5.5f), 3.2f, (uint32_t[]){0xF0D020, 0xD02820, 0x2858C0, 0xF8F8F8, 0x20A040}[(k + seed) % 5]);
    }
    #undef SP
}

/* stair and service block in the gaps between the stands */
static void stair_block(V3 base, V3 al, V3 out, float w, float d, float h)
{
    Frm f = frm_at(vadd(base, vmul(al, w / 2)), out);
    fbox(&f, -w / 2, w / 2, 0, h, 0, d, 0x96948E);
    fbox(&f, -w / 2 - 0.2f, w / 2 + 0.2f, h, h + 0.4f, -0.2f, d + 0.2f, 0x7E7C78);
    for (int k = 0; k < 3; k++) fpanel_z(&f, -0.02f, -w / 2 + 0.8f + k * (w - 1.6f) / 2.f - 0.6f, -w / 2 + 0.8f + k * (w - 1.6f) / 2.f + 0.6f, 0, 2.3f, 0x2A2E36);
    for (int l = 1; l * 3.2f < h - 1; l++) fpanel_z(&f, -0.02f, -w / 2 + 0.6f, w / 2 - 0.6f, l * 3.2f + 0.9f, l * 3.2f + 1.7f, 0x34506E);
}

/* floodlight: tapered steel mast, service platform and a lamp head facing the arena. The head is an open box whose
 * lamps fill the front plane, so no two surfaces lie on top of each other (no z-fighting at a distance), and nothing
 * is thinner than a pixel from the track. */
static void floodlight(float x, float z, float face)
{
    const float H = 46;
    V3 c = V(x, 0, z), F = V(cosf(face), 0, sinf(face)), Rr = vcross(F, UP);
    cyl(c, vadd(c, V(0, H, 0)), 1.1f, 0.55f, 12, 0xB8BAC2, 2);
    cyl(vadd(c, V(0, H * 0.55f, 0)), vadd(c, V(0, H * 0.55f + 0.4f, 0)), 1.6f, 1.6f, 12, 0x8A8C94, 3);   /* rest platform */
    obox(vadd(c, V(0, H + 0.2f, 0)), Rr, UP, F, 3.0f, 0.2f, 1.6f, 0x6A6C74);                           /* head platform */
    const float hw = 6.0f, hh = 3.8f, dep = 0.9f;
    V3 hc = vadd(vadd(c, V(0, H + 0.6f + hh, 0)), vmul(F, 0.9f));                                      /* centre of the lamp plane */
    #define HP(u, v, w) vadd(hc, vadd(vmul(Rr, u), vadd(V(0, v, 0), vmul(F, w))))
    draw_quad(HP(-hw, -hh, -dep), HP(hw, -hh, -dep), HP(hw, hh, -dep), HP(-hw, hh, -dep), 0x2A2C32);    /* back */
    draw_quad(HP(-hw, hh, -dep), HP(hw, hh, -dep), HP(hw, hh, 0), HP(-hw, hh, 0), 0x5A5C64);            /* top */
    draw_quad(HP(-hw, -hh, -dep), HP(hw, -hh, -dep), HP(hw, -hh, 0), HP(-hw, -hh, 0), 0x5A5C64);        /* bottom */
    for (int s = -1; s <= 1; s += 2)
        draw_quad(HP(s * hw, -hh, -dep), HP(s * hw, hh, -dep), HP(s * hw, hh, 0), HP(s * hw, -hh, 0), 0x5A5C64);
    const int nc = 4, nr = 3; const float gap = 0.6f;
    float lw = (2 * hw - 0.6f - (nc - 1) * gap) / nc, lh = (2 * hh - 0.6f - (nr - 1) * gap) / nr;
    for (int r = 0; r < nr; r++)
        for (int q = 0; q < nc; q++) {
            float u0 = -hw + 0.3f + q * (lw + gap), v0 = -hh + 0.3f + r * (lh + gap);
            draw_quad_unlit(HP(u0, v0, 0), HP(u0 + lw, v0, 0), HP(u0 + lw, v0 + lh, 0), HP(u0, v0 + lh, 0), 0xFFF6D0);
        }
    obox(vadd(c, vadd(V(0, H + 0.6f + hh, 0), vmul(F, -0.45f))), Rr, UP, F, 1.2f, 1.2f, 0.4f, 0x6A6C74);  /* bracket to the head */
    #undef HP
}

/* scoreboard on legs behind the west terrace */
static void scoreboard(V3 c, V3 face)
{
    V3 rd = vnorm(vcross(vmul(face, -1), UP));
    const float W = 15, H = 6.5f, y0 = 16;
    for (int s = -1; s <= 1; s += 2) {
        V3 b = vadd(vadd(c, vmul(rd, s * W * 0.6f)), vmul(face, -1.5f));
        cyl(b, vadd(b, V(0, y0 + 1, 0)), 0.7f, 0.6f, 8, 0x8A8C94, 3);
    }
    V3 m = vadd(c, V(0, y0 + H, 0));
    obox(vadd(m, vmul(face, -0.9f)), rd, UP, face, W + 0.6f, H + 0.6f, 0.8f, 0x26262C);
    draw_quad_unlit(vadd(vadd(m, vmul(rd, -W)), V(0, -H, 0)), vadd(vadd(m, vmul(rd, W)), V(0, -H, 0)),
                    vadd(vadd(m, vmul(rd, W)), V(0, H, 0)), vadd(vadd(m, vmul(rd, -W)), V(0, H, 0)), 0x0C0E14);
    V3 tf = vmul(face, 0.03f);
    world_text_c(vadd(vadd(m, V(0, 3.4f, 0)), tf), rd, UP, "STUNT RACER", 0.42f, 0xF8D030, 1);
    world_text_c(vadd(vadd(m, V(0, -0.6f, 0)), tf), rd, UP, "LAP RECORD", 0.3f, 0xF0F0F0, 1);
    world_text_c(vadd(vadd(m, V(0, -3.8f, 0)), tf), rd, UP, "0:49.7", 0.36f, 0xF04030, 1);
    ad_board(vadd(vadd(m, vmul(rd, -W)), V(0, -H - 2.6f, 0)), rd, face, 2 * W, 1.8f, 3);
}

/* the stadium: perimeter wall with sponsor boards, grandstands of different kinds, stair blocks, gate,
 * scoreboard, floodlights */
static void build_stadium(void)
{
    float Lx = ar_x1 - ar_x0, Lz = ar_z1 - ar_z0;
    int bi = 0;
    for (int side = 0; side < 4; side++) {
        V3 base = side == 0 ? V(ar_x0, 0, ar_z0) : side == 1 ? V(ar_x1, 0, ar_z0) : side == 2 ? V(ar_x1, 0, ar_z1) : V(ar_x0, 0, ar_z1);
        V3 along = side == 0 ? V(1, 0, 0) : side == 1 ? V(0, 0, 1) : side == 2 ? V(-1, 0, 0) : V(0, 0, -1);
        V3 out = side == 0 ? V(0, 0, -1) : side == 1 ? V(1, 0, 0) : side == 2 ? V(0, 0, 1) : V(-1, 0, 0);
        V3 in = vmul(out, -1);
        float L = (side & 1) ? Lz : Lx;
        float gate = side == 1 ? gate_z - ar_z0 : -100;
        /* perimeter wall: red and white crash barrier, sponsor boards above, white coping */
        int nst = (int)(L / 3.0f);
        for (int k = 0; k < nst; k++) {
            float a0 = k * 3.0f, a1 = (k + 1) * 3.0f;
            if (a1 > gate - 6 && a0 < gate + 6) continue;
            V3 p0 = vadd(base, vmul(along, a0)), p1 = vadd(base, vmul(along, a1));
            draw_quad(p0, p1, vadd(p1, V(0, 1.1f, 0)), vadd(p0, V(0, 1.1f, 0)), (k & 1) ? 0xE8E8E8 : 0xB02020);
        }
        for (float a = 0; a + 8 <= L; a += 8) {
            if (a + 8 > gate - 6 && a < gate + 6) continue;
            ad_board(vadd(vadd(base, vmul(along, a)), V(0, 1.1f, 0)), along, in, 8, 1.5f, bi++);
            draw_quad(vadd(vadd(base, vmul(along, a)), V(0, 2.6f, 0)), vadd(vadd(base, vmul(along, a + 8)), V(0, 2.6f, 0)),
                      vadd(vadd(vadd(base, vmul(along, a + 8)), V(0, 2.6f, 0)), vmul(out, 0.5f)), vadd(vadd(vadd(base, vmul(along, a)), V(0, 2.6f, 0)), vmul(out, 0.5f)), 0xEEEEEA);
        }
        /* stands with stair blocks between them */
        int rows = side == 0 ? 16 : side == 2 ? 20 : 12, upper = side == 0 ? 9 : 0, roofed = side == 0 || side == 2;
        uint32_t seat = side == 0 ? 0x2A54B4 : side == 2 ? 0xB42A2A : side == 1 ? 0xE0B020 : 0x2A8A44;
        uint32_t accent = side == 0 ? 0xC81E18 : side == 2 ? 0x2858C0 : side == 1 ? 0x1E8A3C : 0xE8B020;
        float gapw = 12, start = 22, end = L - 22;
        int nseg_st = (int)ceilf((end - start) / 110.f);
        float sl = (end - start - (nseg_st - 1) * gapw) / nseg_st;
        float off = 7.f;
        for (int k = 0; k < nseg_st; k++) {
            float a = start + k * (sl + gapw);
            float a0 = a, a1 = a + sl;
            if (side == 1 && gate > a0 - 8 && gate < a1 + 8) {                                 /* leave room for the gate */
                float g0 = gate - 9, g1 = gate + 9;
                if (g0 - a0 > 20) grandstand(vadd(vadd(base, vmul(along, a0)), vmul(out, off)), along, out, g0 - a0, rows, upper, roofed, seat, accent, side * 31 + k);
                if (a1 - g1 > 20) grandstand(vadd(vadd(base, vmul(along, g1)), vmul(out, off)), along, out, a1 - g1, rows, upper, roofed, seat, accent, side * 31 + k + 50);
            } else grandstand(vadd(vadd(base, vmul(along, a0)), vmul(out, off)), along, out, sl, rows, upper, roofed, seat, accent, side * 31 + k);
            if (k + 1 < nseg_st) {
                float depth = 1.0f + (rows + upper) * 0.85f + (upper ? 1 : 0);
                stair_block(vadd(vadd(base, vmul(along, a1 + 0.5f)), vmul(out, off + depth * 0.25f)), along, out, gapw - 1.0f, depth * 0.75f, 2.4f + rows * 0.46f + (upper ? 9 : 2));
            }
        }
        /* walkway between wall and stands */
        draw_quad(vadd(base, V(0, 0.02f, 0)), vadd(vadd(base, vmul(along, L)), V(0, 0.02f, 0)),
                  vadd(vadd(vadd(base, vmul(along, L)), vmul(out, off)), V(0, 0.02f, 0)), vadd(vadd(base, vmul(out, off)), V(0, 0.02f, 0)), 0x8C8A84);
        if (side == 1) {                                                                       /* gate: posts, sign, open doors */
            V3 g = vadd(base, vmul(along, gate));
            for (int s = -1; s <= 1; s += 2) obox(vadd(vadd(g, vmul(along, s * 6.6f)), V(0, 3.5f, 0)), along, UP, out, 0.6f, 3.5f, 0.6f, 0x8E8C88);
            obox(vadd(g, V(0, 7.6f, 0)), along, UP, out, 7.2f, 0.7f, 0.6f, 0x2858C0);
            world_text_c(vadd(vadd(g, V(0, 7.6f, 0)), vmul(in, 0.62f)), vnorm(vcross(out, UP)), UP, "SERVICE GATE", 0.14f, 0xF8F8F8, 0);
            for (int s = -1; s <= 1; s += 2) {
                V3 h0 = vadd(g, vmul(along, s * 6.0f)), h1 = vadd(h0, vmul(in, 5.6f));
                draw_quad(h0, h1, vadd(h1, V(0, 3.2f, 0)), vadd(h0, V(0, 3.2f, 0)), 0x5A6A72);
            }
            draw_quad(vadd(vadd(g, vmul(along, -6)), V(0, 0.03f, 0)), vadd(vadd(g, vmul(along, 6)), V(0, 0.03f, 0)),
                      vadd(vadd(vadd(g, vmul(along, 6)), vmul(out, 40)), V(0, 0.03f, 0)), vadd(vadd(vadd(g, vmul(along, -6)), vmul(out, 40)), V(0, 0.03f, 0)), 0x6E6C68);
        }
        if (side == 3) scoreboard(vadd(vadd(base, vmul(along, L * 0.5f)), vmul(out, off + 1.0f + 12 * 0.85f + 4.f)), in);
    }
    floodlight(ar_x0 - 12, ar_z0 - 12, PI_F / 4); floodlight(ar_x1 + 12, ar_z0 - 12, 3 * PI_F / 4);
    floodlight(ar_x1 + 12, ar_z1 + 12, -3 * PI_F / 4); floodlight(ar_x0 - 12, ar_z1 + 12, -PI_F / 4);
}

/* ------------------------------------------------------------ outside */
static void tree(float x, float z, float s, int kind)
{
    float g = ground_h(x, z);
    cyl(V(x, g, z), V(x, g + 2.2f * s, z), 0.25f * s, 0.2f * s, 5, 0x5A4030, 0);
    uint32_t c = kind ? 0x2E5A2C : 0x3E6E30;
    if (kind) {                                                               /* conifer */
        cyl(V(x, g + 1.5f * s, z), V(x, g + 6.5f * s, z), 2.0f * s, 0.05f, 7, c, 1);
        cyl(V(x, g + 4.0f * s, z), V(x, g + 8.5f * s, z), 1.4f * s, 0.05f, 7, scalecol(c, 1.08f), 1);
    } else {                                                                  /* broadleaf: a lumpy faceted crown */
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
    static const uint32_t cc[10] = {0xC82820, 0x2A5AB0, 0xE8E8E8, 0x2A2A2E, 0x9A9AA2, 0xE0C030, 0x2E7A3E, 0x7A2A2A, 0x4A6AA0, 0xD8D0C0};
    float cx = (ar_x0 + ar_x1) / 2, cz = (ar_z0 + ar_z1) / 2;
    /* car parks behind the long stands */
    for (int side = 0; side < 2; side++) {
        float zl = side ? ar_z1 + 62 : ar_z0 - 62 - 70, x0 = cx - 160, x1 = cx + 160;
        draw_quad(V(x0, 0.04f, zl), V(x1, 0.04f, zl), V(x1, 0.04f, zl + 70), V(x0, 0.04f, zl + 70), 0x55555A);
        for (int row = 0; row < 6; row++) {
            float z = zl + 6 + row * 11.5f + (row & 1) * 5.0f;
            for (float x = x0 + 4; x < x1 - 4; x += 2.9f) {
                draw_quad(V(x - 1.42f, 0.06f, z - 2.4f), V(x - 1.32f, 0.06f, z - 2.4f), V(x - 1.32f, 0.06f, z + 2.4f), V(x - 1.42f, 0.06f, z + 2.4f), 0xD8D8D8);
                uint32_t h = hash2((int)(x * 3), row + side * 17);
                if ((h & 7) < 2) continue;
                parked_car(V(x, 0.04f, z), V(0, 0, (row & 1) ? -1.f : 1.f), cc[(h >> 4) % 10]);
            }
        }
    }
    /* trees in groups around the outside, keeping clear of the car parks and big tops */
    for (int n = 0, tries = 0; n < 420 && tries < 6000; tries++) {
        float a = frand() * 2 * PI_F, d = 60 + frand() * 260;
        float x = cx + cosf(a) * ((ar_x1 - ar_x0) / 2 + d), z = cz + sinf(a) * ((ar_z1 - ar_z0) / 2 + d);
        if (x > ar_x0 - 55 && x < ar_x1 + 55 && z > ar_z0 - 55 && z < ar_z1 + 55) continue;
        if (fabsf(x - cx) < 175 && ((z > ar_z1 + 55 && z < ar_z1 + 140) || (z < ar_z0 - 55 && z > ar_z0 - 140))) continue;
        if (vnoise(x / 70.f, z / 70.f, 31) < 0.45f) continue;
        int skip = 0;
        static const float bt[4][2] = {{-70, -70}, {70, -70}, {70, 70}, {-70, 70}};
        for (int k = 0; k < 4; k++) {
            float bx = (bt[k][0] < 0 ? ar_x0 : ar_x1) + bt[k][0], bz = (bt[k][1] < 0 ? ar_z0 : ar_z1) + bt[k][1];
            if (sq(x - bx) + sq(z - bz) < sq(45)) skip = 1;
        }
        if (fabsf(z - gate_z) < 12 && x > ar_x1) skip = 1;
        if (skip) continue;
        tree(x, z, 0.9f + frand() * 0.8f, frand() < 0.4f);
        n++;
    }
}
