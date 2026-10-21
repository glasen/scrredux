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

/* the key that was pressed, classified once for every screen */
typedef struct {
    SDL_Keycode key;
    int up, down, ok, back;
} MenuKey;

/* moves a cursor over `count` lines with up and down, wrapping round */
static void menu_cursor_move(int *cursor, const MenuKey *in, int count)
{
    if (in->up) {
        *cursor = (*cursor + count - 1) % count;
    }

    if (in->down) {
        *cursor = (*cursor + 1) % count;
    }
}

static void menu_key_main(MenuKey in)
{
    const char *items[8];
    int *cursor = &menu.cursor[MS_MAIN], n = main_items(items);

    if (in.key >= SDLK_1 && in.key < SDLK_1 + n) {
        *cursor = (int)(in.key - SDLK_1);
        in.ok = 1;
    }

    menu_cursor_move(cursor, &in, n);

    if (in.back) {
        menu.confirm_quit = 1;
        return;
    }

    if (!in.ok) {
        return;
    }

    switch (*cursor) {
        case 0:
            hof_load();
            menu_go(MS_HOF);
            break;
        case 1:
            menu.cursor[MS_PRACTICE] = sel_track < ntrk ? sel_track : 0;
            menu_go(MS_PRACTICE);
            break;
        case 2:
            menu_go(sea.on ? MS_SEASON : MS_NAME);
            break;
        case 3:
            menu_go(MS_LSR);
            break;
        case 4:
            ed_open();
            break;
        default:
            menu.cursor[MS_OPT] = 0;
            menu_go(MS_OPT);
            break;
    }
}

static void menu_key_hall_of_fame(const MenuKey *in)
{
    if (in->up) {
        menu.scroll = menu.scroll > 0 ? menu.scroll - 1 : 0;
    }

    if (in->down && menu.scroll + 15 < ntrk) {
        menu.scroll++;
    }

    if (in->back || in->ok) {
        menu_go(MS_MAIN);
    }
}

static void menu_key_practice(const MenuKey *in)
{
    int *cursor = &menu.cursor[MS_PRACTICE];
    menu_cursor_move(cursor, in, ntrk);

    if (in->back) {
        menu_go(MS_MAIN);
    }

    if (in->ok) {
        sel_track = *cursor;

        if (cur_track != sel_track) {
            build_track(sel_track);
        }

        race_mode = RM_PRACTICE;
        cockpit = 1;
        preview_start();
    }
}

static void menu_key_name(const MenuKey *in)
{
    if (in->key == SDLK_BACKSPACE && menu.name_input[0]) {
        menu.name_input[strlen(menu.name_input) - 1] = 0;
    }

    if (in->back) {
        menu_go(MS_MAIN);
    }

    if (in->ok && menu.name_input[0]) {
        season_new(menu.name_input);
        season_write(0);
        menu_go(MS_SEASON);
    }
}

static void menu_key_season(const MenuKey *in)
{
    int *cursor = &menu.cursor[MS_SEASON];

    if (in->up || in->down) {
        *cursor ^= 1;
    }

    if (!player_race()) {
        *cursor = 1;
    }

    if (in->back || (in->ok && *cursor == 1)) {
        menu_go(MS_MAIN);
    } else if (in->ok) {
        season_race();
    }
}

/* the load/save/replay choice */
static void menu_key_load_save(MenuKey in)
{
    int *cursor = &menu.cursor[MS_LSR];

    if (in.key >= SDLK_1 && in.key <= SDLK_4) {
        *cursor = (int)(in.key - SDLK_1);
        in.ok = 1;
    }

    menu_cursor_move(cursor, &in, 4);

    if (in.back || (in.ok && *cursor == 3)) {
        menu_go(MS_MAIN);
    } else if (in.ok && *cursor == 2) {
        menu_go(MS_REPLAYS);
    } else if (in.ok) {
        if (*cursor == 1 && !sea.on) {
            menu_note("No season to save yet");
            return;
        }

        menu.save_mode = *cursor == 1;
        menu.cursor[MS_SLOTS] = 0;
        menu_go(MS_SLOTS);
    }
}

static void menu_key_slots(const MenuKey *in)
{
    int *cursor = &menu.cursor[MS_SLOTS];
    menu_cursor_move(cursor, in, 5);

    if (in->back || (in->ok && *cursor == 4)) {
        menu_go(MS_LSR);
    } else if (in->ok && menu.save_mode) {
        menu_note(season_write(*cursor + 1) ? "Could not save" : "Season saved");
    } else if (in->ok) {
        if (season_read(*cursor + 1)) {
            menu_note("That slot is empty");
        } else {
            season_write(0);
            menu_note("Season loaded");
            menu_go(MS_SEASON);
        }
    }
}

static void menu_key_replays(const MenuKey *in)
{
    int *cursor = &menu.cursor[MS_REPLAYS], n = menu.replay_count + 1;
    menu_cursor_move(cursor, in, n);

    if (in->back || (in->ok && *cursor == menu.replay_count)) {
        menu_go(MS_LSR);
    } else if (in->ok) {
        replay_start(*cursor);
    }
}

static void menu_key_controls(const MenuKey *in)
{
    int *cursor = &menu.cursor[MS_CTL];

    if (menu.ctl_waiting) { /* the next key is the new one; P, R and F11 stay with pause, rescue and fullscreen */
        SDL_Scancode sc = SDL_GetScancodeFromKey(in->key, NULL);

        if (in->back) {
            menu.ctl_waiting = 0;
        } else if (in->key == SDLK_P || in->key == SDLK_R || !sc) {
            menu_note("P and R are taken");
        } else {
            ctl_bind(*cursor, menu.ctl_column, sc);
            menu.ctl_waiting = 0;
        }

        return;
    }

    menu_cursor_move(cursor, in, CT_ROWS);

    if (in->key == SDLK_LEFT || in->key == SDLK_RIGHT) {
        menu.ctl_column ^= 1;
    }

    if (in->back || (in->ok && *cursor == NCT + 1)) {
        menu_go(MS_OPT);
    } else if (in->ok && *cursor == NCT) {
        ctl_defaults();
        ctl_save();
        menu_note("Default keys restored");
    } else if (in->ok) {
        menu.ctl_waiting = 1;
    } else if ((in->key == SDLK_DELETE || in->key == SDLK_BACKSPACE) && *cursor < NCT) {
        ctl[*cursor][menu.ctl_column] = SDL_SCANCODE_UNKNOWN;
        ctl_save();
    }
}

static void menu_key_options(const MenuKey *in)
{
    int *cursor = &menu.cursor[MS_OPT];
    int direction = (in->key == SDLK_RIGHT) - (in->key == SDLK_LEFT);
    menu_cursor_move(cursor, in, NOP);

    if (in->back || (in->ok && *cursor == OP_BACK)) {
        menu_go(MS_MAIN);
        return;
    }

    if (in->ok && *cursor == OP_KEYS) {
        menu.cursor[MS_CTL] = menu.ctl_column = 0;
        menu_go(MS_CTL);
        return;
    }

    if (*cursor <= OP_FX && direction) {
        int *volume = *cursor == OP_ENG ? &opt.eng : &opt.fx;
        *volume = (int)clampf(*volume + direction * 10, 0, 100);
        opt_apply();
        opt_save();

        if (*cursor == OP_FX) {
            snd_play(S_CHIME, 150, 40); /* hear the new level */
        }
    }

    if (*cursor == OP_FULL && (direction || in->ok)) {
        set_fullscreen(!opt.full);
    }

    if (*cursor == OP_RUMBLE && (direction || in->ok)) {
        opt.rumble = !opt.rumble;
        opt_save();
        pad_rumble(0.5f, 250);
    }
}

/* returns 0 to quit the game */
static int menu_key(SDL_Keycode k)
{
    if (menu.confirm_quit) {
        int answer = ask_answer(k);

        if (answer >= 0) {
            menu.confirm_quit = 0;
        }

        return answer != 1;
    }

    MenuKey in = {k, k == SDLK_UP, k == SDLK_DOWN, k == SDLK_RETURN || k == SDLK_KP_ENTER, k == SDLK_ESCAPE};

    switch (menu.screen) {
        case MS_MAIN:
            menu_key_main(in);
            break;
        case MS_HOF:
            menu_key_hall_of_fame(&in);
            break;
        case MS_PRACTICE:
            menu_key_practice(&in);
            break;
        case MS_NAME:
            menu_key_name(&in);
            break;
        case MS_SEASON:
            menu_key_season(&in);
            break;
        case MS_RESULT:
        case MS_END:
            if (in.ok || in.back) {
                menu_go(MS_SEASON);
            }

            break;
        case MS_LSR:
            menu_key_load_save(in);
            break;
        case MS_SLOTS:
            menu_key_slots(&in);
            break;
        case MS_REPLAYS:
            menu_key_replays(&in);
            break;
        case MS_CTL:
            menu_key_controls(&in);
            break;
        case MS_OPT:
            menu_key_options(&in);
            break;
    }

    return 1;
}
