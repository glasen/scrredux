/* ui/menu/navigation.h - drawing the current menu and moving between screens */
#pragma once

static void draw_menu(void)
{
    float x0 = menu_frame();
    const char *it[8];

    switch (ms) {
        case MS_MAIN:
            draw_bars(96, it, main_items(it), msel[MS_MAIN]);
            menu_help("UP/DOWN SELECT   ENTER CHOOSE   ESC QUIT");
            break;
        case MS_HOF:
            draw_hof(x0);
            break;
        case MS_PRACTICE:
            draw_practice();
            break;
        case MS_NAME:
            draw_name();
            break;
        case MS_SEASON:
            draw_season(x0);
            break;
        case MS_RESULT:
            draw_result(x0);
            break;
        case MS_END:
            draw_end(x0);
            break;
        case MS_LSR:
            hud_text_c(84, "LOAD / SAVE / REPLAY", 1.5f, 0xF0C040);
            draw_bars(100, lsr_items, 4, msel[MS_LSR]);
            menu_help("ENTER CHOOSE   ESC BACK");
            break;
        case MS_SLOTS:
            draw_slots();
            break;
        case MS_REPLAYS:
            draw_replays();
            break;
        case MS_CTL:
            draw_controls();
            break;
        case MS_OPT:
            draw_options();
            break;
    }

    if (ms_note_t > 0) {
        hud_text_c(VH - 19.5f, ms_note, fminf(1.3f, (panel_w() - 24) / text_width(ms_note, 1)), 0x60FF60);
    }

    if (ask_quit) {
        ask_box(NULL);
    }
}

/* ---- going places ---- */
static void ed_open(void);

static void menu_go(int screen)
{
    ms = screen;
    ms_scroll = 0;
    ask_quit = 0;
    ct_wait = 0;

    if (screen == MS_NAME) {
        snprintf(name_buf, sizeof name_buf, "%s", sea.pname[0] ? sea.pname : "Player");

        if (win) {
            SDL_StartTextInput(win);
        }
    } else if (win) {
        SDL_StopTextInput(win);
    }

    if (screen == MS_REPLAYS) {
        rp_scan();
        msel[MS_REPLAYS] = 0;
    }

    if (screen == MS_SEASON && sea.on && sea.race >= 6) { /* a finished season (also one loaded from a save) */
        season_end();
        season_write(0);
        ms = MS_END;
    } else if (screen == MS_SEASON) { /* the menu shows the next race's track */
        const Race *r = player_race();

        if (r && sel_track != r->track) {
            sel_track = r->track;
            setup_menu();
        }

        msel[MS_SEASON] = r ? 0 : 1;
    }

    if (state != ST_MENU) {
        setup_menu();
    }
}
