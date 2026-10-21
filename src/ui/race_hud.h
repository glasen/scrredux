/* ui/race_hud.h - the in-race HUD */
#pragma once

static void draw_hud_base(void)
{
    char b[96];

    if (state == ST_VIEW) {
        hud_text_c(8, "MODEL VIEWER", 3, 0xFFD030);
        snprintf(b, sizeof b, "%d TRIANGLES PER CAR", mdl_nv / 3);
        hud_text_c(36, b, 1, 0xE0E0E0);
        hud_text_c(226, "DRAG/ARROWS: ROTATE   WHEEL/+-: ZOOM   HOME: RESET   SPACE: TURNTABLE", 1, 0xE0E0E0);
        snprintf(b, sizeof b, "N: PAINT  Z: ANIMATION %s  F: FLAMES %s  R: CAGE %s  E: INTAKE %s  ESC: QUIT",
                 view_anim ? "ON" : "OFF", view_flame ? "ON" : "OFF", (view_hide & 1) ? "OFF" : "ON",
                 (view_hide & 4) ? "OFF" : "ON");
        hud_text_c(238, b, 1, 0xE0E0E0);
        return;
    }

    if (state == ST_MENU) {
        draw_menu();
        return;
    }

    if (state == ST_PREVIEW) {
        draw_preview();
        return;
    }

    if (state == ST_EDIT) {
        ed_draw();
        return;
    }

    if (state == ST_REPLAY) {
        draw_replay_hud();
        return;
    }

    const Car *p = &cars[0]; /* speed, boost and damage are in the cockpit (draw_cockpit) */
    int lp = laps > 0 ? SDL_clamp(p->lap, 1, laps) : SDL_max(p->lap, 1);

    if (laps > 0) {
        snprintf(b, sizeof b, "LAP %d/%d", lp, laps);
    } else {
        snprintf(b, sizeof b, "LAP %d", lp);
    }
    hud_text(8, 33, b, 1.5f, 0xFFFFFF); /* below the cage's top tube, which carries the damage crack */
    fmt_time(b, sizeof b, racetime);
    hud_text(8, 47, b, 1.5f, 0xFFE060);
    char bl[32];

    if (p->bestlap > 0) {
        fmt_time(bl, sizeof bl, p->bestlap);
    } else {
        snprintf(bl, sizeof bl, "-:--.-");
    }
    snprintf(b, sizeof b, "BEST %s", bl);
    hud_text(HW - 8 - text_width(b, 1.5f), 33, b, 1.5f, 0xFFFFFF);

    if (p->lapshow > 0 && state == ST_RACE) {
        char b2[96];
        fmt_time(bl, sizeof bl, p->lastlap);
        snprintf(b2, sizeof b2, "LAP TIME %s", bl);
        hud_text_c(44, b2, 2, 0xFFFFFF);
    }

    if (state == ST_COUNT) {
        snprintf(b, sizeof b, "%d", (int)ceilf(state_t));
        hud_text_c(100, b, 8, 0xFFE040);
    }

    if (msg_t > 0) {
        hud_text_c(80, msg, 3, 0xFFFFFF);
    }

    if (state == ST_DONE) {
        const char *t = result == 1    ? "YOU WIN!"
                        : result == 2  ? "OPPONENT WRECKED - YOU WIN!"
                        : result == -2 ? "YOU ARE WRECKED!"
                                       : "YOU LOSE!";
        hud_text_c(80, t, result == 2 || result == -2 ? 2 : 4, result > 0 ? 0x60FF60 : 0xFF6050);

        if (p->bestlap > 0) {
            char b2[96];
            fmt_time(bl, sizeof bl, p->bestlap);
            snprintf(b2, sizeof b2, "BEST LAP %s", bl);
            hud_text_c(130, b2, 2, 0xFFFFFF);
        }

        if (state_t > 1.5f) {
            hud_text_c(160, "PRESS ENTER", 2, 0xFFE040);
        }
    }

    race_hud_extra();
}

static void draw_hud(void)
{
    draw_hud_base();
}
