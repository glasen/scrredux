/* ui/menu/state.h - menu state and small menu helpers */
#pragma once

/* the menus: the logo and a teal panel in the race preview's chequered border, over the stadium. Hall of Fame,
 * Practise, the Racing Season, Load/Save/Replay, the track builder, Options and the driving keys; and the pause menu.
 *  */
enum { MS_MAIN, MS_HOF, MS_PRACTICE, MS_NAME, MS_SEASON, MS_RESULT, MS_END, MS_LSR, MS_SLOTS, MS_REPLAYS, MS_CTL, MS_OPT };
static int ms = MS_MAIN, msel[16], ms_save, ms_scroll, ask_quit, ct_col, ct_wait, pause_on, pause_sel;
static char name_buf[16], ms_note[80];
static float ms_note_t;
static int rp_slot[RP_SLOTS], nrp_list; static RHead rp_list[RP_SLOTS];
static int last_lap_rec, last_race_rec;

static float panel_w(void) { return fminf(HW - 70.f, 330.f); }
static void menu_note(const char *s) { snprintf(ms_note, sizeof ms_note, "%s", s); ms_note_t = 2.5f; }
static void fmt_time2(char *b, size_t n, float t) { if (t > 0) fmt_time(b, n, t); else snprintf(b, n, "-:--.-"); }
static void track_title(char *b, size_t n, int t) { snprintf(b, n, "The %s", trk[t].name); }
