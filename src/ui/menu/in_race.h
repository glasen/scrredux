/* ui/menu/in_race.h - in the car: race HUD extras, the pause menu, race and replay keys */
#pragma once

/* ---- in the car: race HUD extras, the pause menu, and keys ---- */
static int pause_items(const char **it)
{
    int n = 0;
    it[n++] = "Continue";

    if (race_mode != RM_SEASON) {
        it[n++] = "Restart"; /* a season race counts: no second go */
    }

    it[n++] = race_mode == RM_SEASON ? "Retire" : race_mode == RM_TEST ? "Back to the builder" : "Leave practice";
    return n;
}

static void draw_pause(void)
{
    const char *it[3];
    int n = pause_items(it);
    float w = 220, h = 46 + n * BAR_GAP, x = (HW - w) / 2, y = 64;
    hud_a = 110;
    hud_rect(0, 0, HW, VH, 0x000000);
    hud_a = 255;
    soft_shadow(x, y, w, h, 8);
    chq_border(x, y, x + w, y + h, 4);
    plate(x + 5, y + 5, w - 10, h - 10, 0x24767A, 0x123E42);
    hud_text_c(y + 12, "PAUSED", 2, 0xF0C040);

    for (int i = 0; i < n; i++) {
        draw_bar(x + 16, y + 36 + i * BAR_GAP, w - 32, it[i], i == menu.pause_cursor);
    }

    hud_text_c(y + h + 8, "UP/DOWN SELECT   ENTER CHOOSE   ESC/P CONTINUE", 1, 0xE0E0E0);
}

static void race_hud_extra(void)
{
    char b[96];

    if (race_mode == RM_SEASON && state == ST_COUNT) {
        snprintf(b, sizeof b, "%s  v  %s", player_name(), drv_name(opp_drv));
        hud_text_c(70, b, 2, 0xFFFFFF);
    }

    if (race_mode == RM_PRACTICE) {
        hud_text_c(36, "PRACTISE", 1.5f, 0xFFE060);
    }

    if (race_mode == RM_TEST) {
        hud_text_c(36, "TEST DRIVE - ESC: MENU", 1.5f, 0xFFE060);
    }

    if (menu.pause_open && !menu.confirm_quit) {
        draw_pause();
    }

    if (menu.confirm_quit) {
        ask_box("YOU RETIRE AND LOSE THE RACE");
    }
}

static void pause_set(int on)
{
    menu.pause_open = paused = on;
    menu.pause_cursor = 0;
}

static void pause_key(SDL_Keycode k)
{
    const char *it[3];
    int n = pause_items(it), *sel = &menu.pause_cursor;

    if (k == SDLK_UP) {
        *sel = (*sel + n - 1) % n;
    }

    if (k == SDLK_DOWN) {
        *sel = (*sel + 1) % n;
    }

    if (k == SDLK_ESCAPE || k == SDLK_P) {
        pause_set(0);
        return;
    }

    if (k != SDLK_RETURN && k != SDLK_KP_ENTER) {
        return;
    }

    if (*sel == 0) {
        pause_set(0);
    } else if (*sel < n - 1) {
        pause_set(0);
        start_race();
    } /* Restart */
    else if (race_mode == RM_SEASON) {
        menu.confirm_quit = 1; /* Retire: asks first */
    } else {
        pause_set(0);
        race_leave(1);
    }
}

static int race_key(SDL_Keycode k)
{
    if (menu.confirm_quit) { /* "Really exit?" over the pause menu */
        int a = ask_answer(k);

        if (a >= 0) {
            menu.confirm_quit = 0;
        }

        if (a == 1) {
            pause_set(0);
            result = -1;
            race_leave(1);
        }

        return 1;
    }

    if (menu.pause_open) {
        pause_key(k);
        return 1;
    }

    if (state == ST_DONE) {
        if (k == SDLK_ESCAPE || ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && state_t > 1.5f)) {
            race_leave(0);
        }

        return 1;
    }

    if (k == SDLK_ESCAPE || k == SDLK_P) {
        pause_set(1);
    } else if (k == SDLK_R && state == ST_RACE && !car_out(&cars[0])) {
        car_hurt(&cars[0], 8, 0);
        start_crane(&cars[0], restart_seg(&cars[0]), 0, CRANE_LOWER_TIME);
    }
    return 1;
}

/* ---- replay ---- */
static void draw_replay_hud(void)
{
    char b[128], t[16];
    hud_text(8, 8, "REPLAY", 2, 0xFF5050);
    hud_text(8, 26, replay.head.title, 1, 0xE8E8EC);
    fmt_time(t, sizeof t, racetime);
    int lp = cars[0].lap < 1 ? 1 : SDL_min(cars[0].lap, replay.head.laps);
    snprintf(b, sizeof b, "LAP %d/%d  %s", lp, replay.head.laps, t);
    hud_text(HW - 8 - text_width(b, 1.5f), 8, b, 1.5f, 0xFFE060);
    snprintf(b, sizeof b, "CAMERA: %s%s", RC_NAME[replay.camera],
             replay.paused      ? "   PAUSED"
             : replay.speed > 1 ? "   >>"
             : replay.speed < 0 ? "   <<"
                                : "");
    hud_text(HW - 8 - text_width(b, 1), 26, b, 1, 0xE8E8EC);
    float W = HW - 40, f = replay.count > 1 ? replay.time / ((replay.count - 1) / (float)RP_HZ) : 0;
    hud_rect(20, VH - 22, W, 4, 0x303036);
    hud_rect(20, VH - 22, W * f, 4, 0xF08C1C);
    hud_text_c(VH - 12, "C CAMERA   SPACE PAUSE   LEFT/RIGHT REWIND/FAST   HOME START   ESC BACK", 1, 0xC0C0C8);
}

static void replay_key(SDL_Keycode k)
{
    if (k == SDLK_ESCAPE) {
        replay_leave();
    } else if (k == SDLK_C) {
        replay.camera = (replay.camera + 1) % NRC;

        if (!cars[1].active && (replay.camera == RC_OPP_COCKPIT || replay.camera == RC_OPP_CHASE)) {
            replay.camera = RC_COCKPIT;
        }

        update_camera(&cars[cam_car()], 0.016f, 1);
    } else if (k == SDLK_SPACE) {
        replay.paused = !replay.paused;
    } else if (k == SDLK_HOME) {
        replay.time = 0;
        npart = 0;
    }
}

static void replay_hold(void) /* held keys: wind back or forward at four times the speed */
{
    replay.speed = 1;
    PadIn pi = pad_read();

    if ((keys && keys[SDL_SCANCODE_RIGHT]) || pi.rt) {
        replay.speed = 4;
    }

    if ((keys && keys[SDL_SCANCODE_LEFT]) || pi.lf) {
        replay.speed = -4;
    }
}
