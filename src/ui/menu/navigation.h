/* ui/menu/navigation.h - drawing the current menu and moving between screens */
#pragma once

static void draw_menu(void)
{
    float x0 = menu_frame();
    const char *it[8];

    switch (menu.screen) {
        case MS_MAIN:
            draw_bars(96, it, main_items(it), menu.cursor[MS_MAIN]);
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
            draw_bars(100, lsr_items, 4, menu.cursor[MS_LSR]);
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

    if (menu.note_time > 0) {
        hud_text_c(VH - 19.5f, menu.note, fminf(1.3f, (panel_w() - 24) / text_width(menu.note, 1)), 0x60FF60);
    }

    if (menu.confirm_quit) {
        ask_box(NULL);
    }
}

/* ---- going places ---- */
static void ed_open(void);

static void menu_go(int screen)
{
    menu.screen = screen;
    menu.scroll = 0;
    menu.confirm_quit = 0;
    menu.ctl_waiting = 0;

    if (screen == MS_NAME) {
        snprintf(menu.name_input, sizeof menu.name_input, "%s", sea.pname[0] ? sea.pname : "Player");

        if (win) {
            SDL_StartTextInput(win);
        }
    } else if (win) {
        SDL_StopTextInput(win);
    }

    if (screen == MS_REPLAYS) {
        rp_scan();
        menu.cursor[MS_REPLAYS] = 0;
    }

    if (screen == MS_SEASON && sea.on && sea.race >= 6) { /* a finished season (also one loaded from a save) */
        season_end();
        season_write(0);
        menu.screen = MS_END;
    } else if (screen == MS_SEASON) { /* the menu shows the next race's track */
        const Race *r = player_race();

        if (r && sel_track != r->track) {
            sel_track = r->track;
            setup_menu();
        }

        menu.cursor[MS_SEASON] = r ? 0 : 1;
    }

    if (state != ST_MENU) {
        setup_menu();
    }
}
