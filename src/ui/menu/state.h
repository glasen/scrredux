/* ui/menu/state.h - menu state and small menu helpers */
#pragma once

/* the menus: the logo and a teal panel in the race preview's chequered border, over the stadium. Hall of Fame,
 * Practise, the Racing Season, Load/Save/Replay, the track builder, Options and the driving keys; and the pause menu.
 *  */
enum {
    MS_MAIN,
    MS_HOF,
    MS_PRACTICE,
    MS_NAME,
    MS_SEASON,
    MS_RESULT,
    MS_END,
    MS_LSR,
    MS_SLOTS,
    MS_REPLAYS,
    MS_CTL,
    MS_OPT
};

/* the menus: everything they keep between frames */
static struct {
    int screen;       /* MS_* */
    int cursor[16];   /* selected line on each screen */
    int save_mode;    /* the saves screen saves (1) or loads (0) */
    int scroll;       /* first line of a long list */
    int confirm_quit; /* the "REALLY EXIT?" box is up */
    int ctl_column;   /* controls screen: which of an action's two keys is selected */
    int ctl_waiting;  /* controls screen: waiting for the new key */
    int pause_open, pause_cursor;
    char name_input[16]; /* the player's name being typed */
    char note[80];
    float note_time;
    int replay_slots[RP_SLOTS], replay_count; /* the replays on the Load/Save/Replay screen */
    RHead replay_heads[RP_SLOTS];
    int last_lap_record, last_race_record; /* a Hall of Fame record was just set */
} menu = {.screen = MS_MAIN};

static float panel_w(void)
{
    return fminf(HW - 70.f, 330.f);
}

static void menu_note(const char *s)
{
    snprintf(menu.note, sizeof menu.note, "%s", s);
    menu.note_time = 2.5f;
}

static void fmt_time2(char *b, size_t n, float t)
{
    if (t > 0) {
        fmt_time(b, n, t);
    } else {
        snprintf(b, n, "-:--.-");
    }
}

static void track_title(char *b, size_t n, int t)
{
    snprintf(b, n, "The %s", trk[t].name);
}
