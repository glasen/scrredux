/* ui/menu/input.h - menu key handling */
#pragma once

static void menu_text(const char *t)
{
    if (menu.screen != MS_NAME) {
        return;
    }

    for (; *t; t++) {
        size_t n = strlen(menu.name_input);

        if (n + 1 < sizeof menu.name_input && (strchr(NAME_CHARS, *t) || (*t == ' ' && n))) {
            menu.name_input[n] = *t;
            menu.name_input[n + 1] = 0;
        }
    }
}

/* returns 0 to quit the game */
static int menu_key(SDL_Keycode k)
{
    if (menu.confirm_quit) {
        int a = ask_answer(k);

        if (a >= 0) {
            menu.confirm_quit = 0;
        }

        return a != 1;
    }

    int *sel = &menu.cursor[menu.screen], up = k == SDLK_UP, dn = k == SDLK_DOWN,
        ok = k == SDLK_RETURN || k == SDLK_KP_ENTER, back = k == SDLK_ESCAPE;
    const char *it[8];

    switch (menu.screen) {
        case MS_MAIN: {
            int n = main_items(it);

            if (k >= SDLK_1 && k < SDLK_1 + n) {
                *sel = (int)(k - SDLK_1);
                ok = 1;
            }

            if (up) {
                *sel = (*sel + n - 1) % n;
            }

            if (dn) {
                *sel = (*sel + 1) % n;
            }

            if (back) {
                menu.confirm_quit = 1;
                break;
            }

            if (!ok) {
                break;
            }

            if (*sel == 0) {
                hof_load();
                menu_go(MS_HOF);
            } else if (*sel == 1) {
                menu.cursor[MS_PRACTICE] = sel_track < ntrk ? sel_track : 0;
                menu_go(MS_PRACTICE);
            } else if (*sel == 2) {
                menu_go(sea.on ? MS_SEASON : MS_NAME);
            } else if (*sel == 3) {
                menu_go(MS_LSR);
            } else if (*sel == 4) {
                ed_open();
            } else {
                menu.cursor[MS_OPT] = 0;
                menu_go(MS_OPT);
            }
            break;
        }
        case MS_HOF:
            if (up) {
                menu.scroll = menu.scroll > 0 ? menu.scroll - 1 : 0;
            }

            if (dn && menu.scroll + 15 < ntrk) {
                menu.scroll++;
            }

            if (back || ok) {
                menu_go(MS_MAIN);
            }

            break;
        case MS_PRACTICE:
            if (up) {
                *sel = (*sel + ntrk - 1) % ntrk;
            }

            if (dn) {
                *sel = (*sel + 1) % ntrk;
            }

            if (back) {
                menu_go(MS_MAIN);
            }

            if (ok) {
                sel_track = *sel;

                if (cur_track != sel_track) {
                    build_track(sel_track);
                }

                race_mode = RM_PRACTICE;
                cockpit = 1;
                preview_start();
            }

            break;
        case MS_NAME:
            if (k == SDLK_BACKSPACE && menu.name_input[0]) {
                menu.name_input[strlen(menu.name_input) - 1] = 0;
            }

            if (back) {
                menu_go(MS_MAIN);
            }

            if (ok && menu.name_input[0]) {
                season_new(menu.name_input);
                season_write(0);
                menu_go(MS_SEASON);
            }

            break;
        case MS_SEASON:
            if (up || dn) {
                *sel ^= 1;
            }

            if (!player_race()) {
                *sel = 1;
            }

            if (back || (ok && *sel == 1)) {
                menu_go(MS_MAIN);
            } else if (ok) {
                season_race();
            }
            break;
        case MS_RESULT:
            if (ok || back) {
                menu_go(MS_SEASON);
            }

            break;
        case MS_END:
            if (ok || back) {
                menu_go(MS_SEASON);
            }

            break;
        case MS_LSR:
            if (k >= SDLK_1 && k <= SDLK_4) {
                *sel = (int)(k - SDLK_1);
                ok = 1;
            }

            if (up) {
                *sel = (*sel + 3) % 4;
            }

            if (dn) {
                *sel = (*sel + 1) % 4;
            }

            if (back || (ok && *sel == 3)) {
                menu_go(MS_MAIN);
            } else if (ok && *sel == 2) {
                menu_go(MS_REPLAYS);
            } else if (ok) {
                if (*sel == 1 && !sea.on) {
                    menu_note("No season to save yet");
                    break;
                }

                menu.save_mode = *sel == 1;
                menu.cursor[MS_SLOTS] = 0;
                menu_go(MS_SLOTS);
            }
            break;
        case MS_SLOTS:
            if (up) {
                *sel = (*sel + 4) % 5;
            }

            if (dn) {
                *sel = (*sel + 1) % 5;
            }

            if (back || (ok && *sel == 4)) {
                menu_go(MS_LSR);
            } else if (ok && menu.save_mode) {
                menu_note(season_write(*sel + 1) ? "Could not save" : "Season saved");
            } else if (ok) {
                if (season_read(*sel + 1)) {
                    menu_note("That slot is empty");
                } else {
                    season_write(0);
                    menu_note("Season loaded");
                    menu_go(MS_SEASON);
                }
            }
            break;
        case MS_REPLAYS: {
            int n = menu.replay_count + 1;

            if (up) {
                *sel = (*sel + n - 1) % n;
            }

            if (dn) {
                *sel = (*sel + 1) % n;
            }

            if (back || (ok && *sel == menu.replay_count)) {
                menu_go(MS_LSR);
            } else if (ok) {
                replay_start(*sel);
            }
            break;
        }
        case MS_CTL: {
            if (menu.ctl_waiting) { /* the next key is the new one; P, R and F11 stay with pause, rescue and fullscreen */
                SDL_Scancode sc = SDL_GetScancodeFromKey(k, NULL);

                if (back) {
                    menu.ctl_waiting = 0;
                } else if (k == SDLK_P || k == SDLK_R || !sc) {
                    menu_note("P and R are taken");
                } else {
                    ctl_bind(*sel, menu.ctl_column, sc);
                    menu.ctl_waiting = 0;
                }
                break;
            }

            if (up) {
                *sel = (*sel + CT_ROWS - 1) % CT_ROWS;
            }

            if (dn) {
                *sel = (*sel + 1) % CT_ROWS;
            }

            if (k == SDLK_LEFT || k == SDLK_RIGHT) {
                menu.ctl_column ^= 1;
            }

            if (back || (ok && *sel == NCT + 1)) {
                menu_go(MS_OPT);
            } else if (ok && *sel == NCT) {
                ctl_defaults();
                ctl_save();
                menu_note("Default keys restored");
            } else if (ok) {
                menu.ctl_waiting = 1;
            } else if ((k == SDLK_DELETE || k == SDLK_BACKSPACE) && *sel < NCT) {
                ctl[*sel][menu.ctl_column] = SDL_SCANCODE_UNKNOWN;
                ctl_save();
            }
            break;
        }
        case MS_OPT: {
            int d = (k == SDLK_RIGHT) - (k == SDLK_LEFT);

            if (up) {
                *sel = (*sel + NOP - 1) % NOP;
            }

            if (dn) {
                *sel = (*sel + 1) % NOP;
            }

            if (back || (ok && *sel == OP_BACK)) {
                menu_go(MS_MAIN);
                break;
            }

            if (ok && *sel == OP_KEYS) {
                menu.cursor[MS_CTL] = menu.ctl_column = 0;
                menu_go(MS_CTL);
                break;
            }

            if (*sel <= OP_FX && d) {
                int *v = *sel == OP_ENG ? &opt.eng : &opt.fx;
                *v = (int)clampf(*v + d * 10, 0, 100);
                opt_apply();
                opt_save();

                if (*sel == OP_FX) {
                    snd_play(S_CHIME, 150, 40); /* hear the new level */
                }
            }

            if (*sel == OP_FULL && (d || ok)) {
                set_fullscreen(!opt.full);
            }

            if (*sel == OP_RUMBLE && (d || ok)) {
                opt.rumble = !opt.rumble;
                opt_save();
                pad_rumble(0.5f, 250);
            }

            break;
        }
    }
    return 1;
}
