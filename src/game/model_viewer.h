/* game/model_viewer.h - model viewer: the car alone on a plain floor */
#pragma once

/* ---- model viewer: the car alone on a plain floor, orbit camera ---- */
static struct {
    float yaw, pitch, distance; /* orbit camera */
    V3 target;
    int auto_orbit;
    int hide_mask; /* bit 0: body panels hidden, bit 2: suspension hidden */
    int paint;     /* index into PALETTE */
    int flame;     /* exhaust flames shown */
    int animate;   /* wheels and suspension moving */
} viewer = {0.6f, 0.22f, 7.5f, {0, 0.55f, 0}, .auto_orbit = 1, .animate = 1};

static const uint32_t PALETTE[] = {CAR_RED, 0x2860D8, 0xF07818, 0x30A050, 0xE8D030, 0xF0F0F0, 0x303034, 0x9040C0};

static void start_view(void)
{
    memset(&cars[0], 0, sizeof(Car));
    memset(&cars[1], 0, sizeof(Car));
    Car *c = &cars[0];
    c->active = 1;
    c->col = PALETTE[viewer.paint % 8];
    c->x = V(0, REST_H, 0);
    c->F = V(0, 0, 1);
    c->U = V(0, 1, 0);
    c->R = vcross(c->F, c->U);

    for (int i = 0; i < 4; i++) {
        c->suspension_len[i] = WREST[i];
    }

    state = ST_VIEW;
}

static void view_key(SDL_Keycode k)
{
    if (k == SDLK_SPACE) {
        viewer.auto_orbit = !viewer.auto_orbit;
    } else if (k == SDLK_N) {
        viewer.paint = (viewer.paint + 1) % 8;
        cars[0].col = PALETTE[viewer.paint];
    } else if (k == SDLK_R) {
        viewer.hide_mask ^= 1;
    } else if (k == SDLK_E) {
        viewer.hide_mask ^= 4;
    } else if (k == SDLK_F) {
        viewer.flame = !viewer.flame;
    } else if (k == SDLK_Z) {
        viewer.animate = !viewer.animate;
    } else if (k == SDLK_HOME) {
        viewer.yaw = 0.6f;
        viewer.pitch = 0.22f;
        viewer.distance = 7.5f;
        viewer.target = V(0, 0.55f, 0);
    }
}

/* the menu: the camera turns round the empty track (preview_cam) */
static float orbit_t;
