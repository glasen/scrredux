/* app/input.h - key handling, gamepad navigation and per-frame timers */
#pragma once

/* one key press, wherever the game is; 0 = quit */
static int handle_key(SDL_Keycode k, SDL_Keymod mod)
{
    if (k == SDLK_F11 && win) {
        set_fullscreen(!opt.full);
        return 1;
    }

    switch (state) {
        case ST_VIEW:
            if (k == SDLK_ESCAPE) {
                return 0;
            }

            view_key(k);
            return 1;
        case ST_MENU:
            return menu_key(k);
        case ST_EDIT:
            ed_key(k, mod);
            return 1;
        case ST_REPLAY:
            replay_key(k);
            return 1;
        case ST_PREVIEW:
            preview_key(k);
            return 1;
        default:
            return race_key(k);
    }
}

/* pads: a button press becomes the key it stands for where the game is; held directions move through the menus */
static SDL_Keycode pad_key(int b)
{
    int a = b == SDL_GAMEPAD_BUTTON_SOUTH, x = b == SDL_GAMEPAD_BUTTON_EAST, y = b == SDL_GAMEPAD_BUTTON_NORTH;
    int st = b == SDL_GAMEPAD_BUTTON_START, bk = b == SDL_GAMEPAD_BUTTON_BACK;

    if (ask_quit) {
        return a ? SDLK_Y : x || bk ? SDLK_N : 0;
    }

    switch (state) {
        case ST_MENU:
            if (ct_wait) {
                return x || bk ? SDLK_ESCAPE : 0;
            }

            /* fall through */
        case ST_PREVIEW:
            return a || st ? SDLK_RETURN : x || bk ? SDLK_ESCAPE : 0;
        case ST_REPLAY:
            return a || st ? SDLK_SPACE : y ? SDLK_C : x || bk ? SDLK_ESCAPE : 0;
        case ST_DONE:
            return a || st ? SDLK_RETURN : bk ? SDLK_ESCAPE : 0;
        case ST_EDIT:
            return x || bk ? SDLK_ESCAPE : 0; /* the builder wants keys and mouse; B/Back at least gets out */
        case ST_COUNT:
        case ST_RACE:
            if (pause_on) {
                return a ? SDLK_RETURN : x || bk || st ? SDLK_ESCAPE : 0;
            }

            return st ? SDLK_P : y ? SDLK_R : bk ? SDLK_ESCAPE : 0;
        default:
            return 0;
    }
}

static SDL_Keycode pad_nav(float dt)
{
    static const SDL_Keycode NAV[5] = {0, SDLK_UP, SDLK_DOWN, SDLK_LEFT, SDLK_RIGHT};
    static int was;
    static float t;
    PadIn pi = pad_read();
    int d = pi.up ? 1 : pi.dn ? 2 : pi.lf ? 3 : pi.rt ? 4 : 0;

    if ((state != ST_MENU && !pause_on) || ask_quit || ct_wait) {
        was = d;
        return 0;
    }

    if (d != was) {
        was = d;
        t = 0.4f;
        return NAV[d];
    }

    if (!d || (t -= dt) > 0) {
        return 0;
    }

    t = 0.12f;
    return NAV[d];
}

static void frame_timers(float frame)
{
    if (ms_note_t > 0) {
        ms_note_t -= frame;
    }

    if (editor.message_time > 0) {
        editor.message_time -= frame;
    }

    if (state == ST_REPLAY) {
        replay_hold();
    }
}
