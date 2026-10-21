/* game/model_viewer.h - model viewer: the car alone on a plain floor */
#pragma once

/* ---- model viewer: the car alone on a plain floor, orbit camera ---- */
static float view_yaw = 0.6f, view_pitch = 0.22f, view_dist = 7.5f;
static int view_auto = 1, view_hide = 0, view_col = 0, view_flame = 0, view_anim = 1;
static V3 view_target = {0, 0.55f, 0};
static const uint32_t PALETTE[] = {CAR_RED, 0x2860D8, 0xF07818, 0x30A050, 0xE8D030, 0xF0F0F0, 0x303034, 0x9040C0};

static void start_view(void)
{
    memset(&cars[0], 0, sizeof(Car));
    memset(&cars[1], 0, sizeof(Car));
    Car *c = &cars[0];
    c->active = 1;
    c->col = PALETTE[view_col % 8];
    c->x = V(0, REST_H, 0);
    c->F = V(0, 0, 1);
    c->U = V(0, 1, 0);
    c->R = vcross(c->F, c->U);
    for (int i = 0; i < 4; i++) {
        c->wl[i] = WREST[i];
    }
    state = ST_VIEW;
}

static void view_key(SDL_Keycode k)
{
    if (k == SDLK_SPACE) {
        view_auto = !view_auto;
    } else if (k == SDLK_N) {
        view_col = (view_col + 1) % 8;
        cars[0].col = PALETTE[view_col];
    } else if (k == SDLK_R) {
        view_hide ^= 1;
    } else if (k == SDLK_E) {
        view_hide ^= 4;
    } else if (k == SDLK_F) {
        view_flame = !view_flame;
    } else if (k == SDLK_Z) {
        view_anim = !view_anim;
    } else if (k == SDLK_HOME) {
        view_yaw = 0.6f;
        view_pitch = 0.22f;
        view_dist = 7.5f;
        view_target = V(0, 0.55f, 0);
    }
}

/* the menu: the camera turns round the empty track (preview_cam) */
static float orbit_t;
