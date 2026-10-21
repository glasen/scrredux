/* ui/menu/screens.h - the menu screens: main, Hall of Fame, practise, season, results, saves, replays, controls */
#pragma once

/* ---- screens ---- */
static int main_items(const char **it)
{
    it[0] = "Hall of Fame";
    it[1] = "Practise";
    it[2] = sea.on ? "Continue the Racing Season" : "Start the Racing Season";
    it[3] = "Load/Save/Replay";
    it[4] = "Track Builder";
    it[5] = "Options";
    return 6;
}

/* a table row: cell i at column at[i] (in widths of the old 5x7 font's characters), right-aligned there if at[i] < 0 */
static void hud_row(float x, float y, float sc, uint32_t col, int n, const float *at, const char *const *cell)
{
    for (int i = 0; i < n; i++) {
        float cx = x + fabsf(at[i]) * 6 * sc;
        hud_text(at[i] < 0 ? cx - text_width(cell[i], sc) : cx, y, cell[i], sc, col);
    }
}

static void draw_hof(float x0)
{
    hud_text_c(80, "HALL of FAME", 1.5f, 0xF0C040);
    static const float hat[3] = {0, 17, 36}, at[5] = {0, 17, -34, 36, -54};
    const char *head[3] = {"TRACK", "LAP RECORD", "RACE RECORD"};
    hud_row(x0 + 4, 96, 1, 0xF08A28, 3, hat, head);
    int n = ntrk, vis = 15, first = clampf(menu.scroll, 0, fmaxf(0, n - vis));

    for (int r = 0; r < vis && first + r < n; r++) {
        const TrackDef *td = &trk[first + r];
        const Hof *h = hof_get(td->name);
        char l[16], rr[16], lb[16], rb[16];
        fmt_time2(l, sizeof l, h ? h->lap : 0);
        fmt_time2(rr, sizeof rr, h ? h->race : 0);
        snprintf(lb, sizeof lb, "%.12s", h && h->lap > 0 ? h->lapby : "-");
        snprintf(rb, sizeof rb, "%.12s", h && h->race > 0 ? h->raceby : "-");
        const char *cell[5] = {td->name, lb, l, rb, rr};
        hud_row(x0 + 4, 108 + r * 10, 1, first + r < SCR_NTRACKS ? 0xE8E8EC : 0xA0D0FF, 5, at, cell);
    }

    menu_help("UP/DOWN SCROLL   ESC BACK");
}

static void draw_practice(void)
{
    hud_text_c(80, "PRACTISE - choose a track", 1.5f, 0xF0C040);
    int n = ntrk, vis = 9, sel = menu.cursor[MS_PRACTICE];

    if (sel < menu.scroll) {
        menu.scroll = sel;
    }

    if (sel >= menu.scroll + vis) {
        menu.scroll = sel - vis + 1;
    }

    float W = panel_w() - 30, x = (HW - W) / 2;

    for (int r = 0; r < vis && menu.scroll + r < n; r++) {
        int i = menu.scroll + r;
        char b[64];
        snprintf(b, sizeof b, "%s%s", trk[i].name, i < SCR_NTRACKS ? "" : "  (own)");
        draw_bar(x, 96 + r * LIST_GAP, W, b, i == sel);
    }

    menu_help("ENTER DRIVE   ESC BACK   (ESC IN THE CAR ENDS PRACTISE)");
}

static void draw_name(void)
{
    hud_text_c(96, "A new Racing Season", 1.5f, 0xF0C040);
    hud_text_c(122, "Enter your name:", 1.5f, 0xE8E8EC);
    float W = 170, x = (HW - W) / 2;
    soft_shadow(x, 140, W, 20, 3);
    hud_rect(x - 1.5f, 138.5f, W + 3, 23, 0xB4B4B4);
    plate(x, 140, W, 20, 0x2A2C30, 0x060608);
    char b[24];
    snprintf(b, sizeof b, "%s%s", menu.name_input, (SDL_GetTicks() / 400) & 1 ? "_" : "");
    hud_text_raw(x + 6, 140 + (20 - 7 * 1.5f) / 2, b, 1.5f, 0xFFE060);
    hud_text_c(176, "You start in DIVISION 4 against", 1, 0xB0B0B8);
    hud_text_c(188, "Jumping Jack and Road Hog.", 1, 0xB0B0B8);
    menu_help("ENTER START   ESC BACK");
}

static void div_rows(const Season *s, int d, float x, float y, float sc, int hl)
{
    int tab[3], w = sc < 1 ? 11 : 13;
    div_table(s, d, tab);
    const float at[5] = {0, -(w + 6.f), -(w + 10.f), -(w + 14.f), -(w + 18.f)};
    const char *head[5] = {"DRIVER", "RACED", "WIN", "LAP", "PTS"};
    hud_row(x, y, sc, 0xF08A28, 5, at, head);

    for (int r = 0; r < 3; r++) {
        int i = tab[r];
        char nm[16], c[4][8];
        snprintf(nm, sizeof nm, "%.*s", w, i == PLAYER ? s->pname : DRV_NAME[i]);
        snprintf(c[0], 8, "%d", s->raced[i]);
        snprintf(c[1], 8, "%d", s->wins[i]);
        snprintf(c[2], 8, "%d", s->lapp[i]);
        snprintf(c[3], 8, "%d", s->pts[i]);
        const char *cell[5] = {nm, c[0], c[1], c[2], c[3]};
        hud_row(x, y + (r + 1) * 10 * sc, sc, hl && i == PLAYER ? 0xFFE060 : 0xE8E8EC, 5, at, cell);
    }
}

static const char *season_items[2] = {"Race", "Main Menu"};

static void draw_season(float x0)
{
    char b[128], t0[48], t1[48];
    int d = player_div();
    snprintf(b, sizeof b, "Season %d", sea.season);
    hud_text_c(80, b, 1.5f, 0xF0C040);
    div_rows(&sea, d, x0 + 30, 98, 1, 1);
    track_title(t0, sizeof t0, DIV_TRACK[d][0]);
    track_title(t1, sizeof t1, DIV_TRACK[d][1]);
    snprintf(b, sizeof b, "Tracks in DIVISION %s: %s, %s", ROMAN[d], t0, t1);
    hud_text_c(146, b, 1, 0xB0B0B8);
    const Race *r = player_race();

    if (r) {
        int o = r->a == PLAYER ? r->b : r->a;
        snprintf(b, sizeof b, "Race %d of 4:  %s  v  %s", player_races_done() + 1, sea.pname, DRV_NAME[o]);
        hud_text_c(162, b, 1.3f, 0xFFFFFF);
        const Hof *h = hof_get(trk[r->track].name);
        track_title(t0, sizeof t0, r->track);
        char l[16];
        fmt_time2(l, sizeof l, h ? h->lap : 0);
        snprintf(b, sizeof b, "%s - lap record %s, %s", t0, l, h && h->lap > 0 ? h->lapby : "-");
        hud_text_c(178, b, 1, 0xB0B0B8);
        draw_bars(196, season_items, 2, menu.cursor[MS_SEASON]);
    } else {
        hud_text_c(170, "The season is over", 1.5f, 0xFFFFFF);
        draw_bars(196, season_items + 1, 1, 0);
    }
    menu_help("ENTER CHOOSE   ESC MAIN MENU");
}

static void result_line(float x, float y, int drv, float t, float lap, int pts, const char *why)
{
    char nm[16], tt[16], ll[16], p[8];

    if (t > 0) {
        fmt_time(tt, sizeof tt, t);
    } else {
        snprintf(tt, sizeof tt, "%s", why);
    }
    fmt_time2(ll, sizeof ll, lap);
    snprintf(nm, sizeof nm, "%.13s", drv_name(drv));
    snprintf(p, sizeof p, "+%d", pts);
    static const float at[4] = {0, -23, -33, 37};
    const char *cell[4] = {nm, tt, ll, p};
    hud_row(x, y, 1.2f, drv == PLAYER ? 0xFFE060 : 0xE8E8EC, 4, at, cell);
}

static void draw_result(float x0)
{
    const Race *r = &last_res;
    hud_text_c(80, "RACE RESULT", 1.5f, 0xF0C040);
    char b[128], t[48];
    track_title(t, sizeof t, r->track);
    hud_text_c(96, t, 1, 0xB0B0B8);
    static const float hat[4] = {0, -23, -33, 36};
    const char *head[4] = {"DRIVER", "RACE-TIME", "BEST-LAP", "POINTS"};
    hud_row(x0 + 16, 110, 1.2f, 0xF08A28, 4, hat, head);

    for (int k = 0; k < 2; k++) {
        int dv = k ? r->b : r->a;
        float tm = k ? r->tb : r->ta, lp = k ? r->lb : r->la;
        int pts = (r->winner == dv) * 2 + (r->fast == dv);
        const char *why = dv == PLAYER && last_retired                  ? "RETIRED"
                          : (dv == PLAYER ? result == -2 : result == 2) ? "WRECKED"
                                                                        : "-";
        result_line(x0 + 16, 124 + k * 13, dv, tm, lp, pts, why);
    }

    snprintf(b, sizeof b, "%s wins the race%s", drv_name(r->winner >= 0 ? r->winner : r->b), r->fast >= 0 ? "" : "");
    hud_text_c(156, b, 1.5f, r->winner == PLAYER ? 0x60FF60 : 0xFF8070);

    if (r->fast >= 0) {
        snprintf(b, sizeof b, "Fastest lap: %s", drv_name(r->fast));
        hud_text_c(170, b, 1, 0xE8E8EC);
    }

    if (menu.last_lap_record || menu.last_race_record) {
        hud_text_c(182, menu.last_race_record ? "New track record (race time)" : "New track record (lap)", 1, 0x60FF60);
    }

    float y = 196;

    for (int k = 0; k < nlast_sim; k++) {
        const Race *o = &sea.sched[player_div()][last_sim[k]];
        snprintf(b, sizeof b, "%s beat %s%s%s", drv_name(o->winner), drv_name(o->winner == o->a ? o->b : o->a),
                 o->fast >= 0 ? ", fastest lap " : "", o->fast >= 0 ? drv_name(o->fast) : "");
        hud_text_c(y, b, 1, 0xB0B0B8);
        y += 11;
    }

    if (nlast_sim) {
        hud_text_c(196 - 12, "Also raced in your division:", 1, 0x9C9CA4);
    }

    menu_help("ENTER CONTINUE");
}

static void draw_end(float x0)
{
    char b[96];
    snprintf(b, sizeof b, "Final tables, season %d", sea_end.season);
    hud_text_c(80, b, 1.5f, 0xF0C040);
    float W = panel_w(), cw = (W - 12) / 2;

    for (int d = 0; d < 4; d++) {
        float x = x0 + 4 + (d & 1) * cw, y = 96 + (d >> 1) * 52;
        snprintf(b, sizeof b, "DIVISION %s", ROMAN[d]);
        hud_text(x, y, b, 1, 0xF0C040);
        div_rows(&sea_end, d, x, y + 9, 0.8f, 1);
    }

    const char *m = sea_moved == 2  ? "EXCELLENT DRIVING - WELL DONE"
                    : sea_moved < 0 ? "PROMOTED!"
                    : sea_moved > 0 ? "RELEGATED"
                                    : "You stay in your division";
    hud_text_c(206, m, 2, sea_moved == 1 ? 0xFF8070 : 0x60FF60);
    snprintf(b, sizeof b, "Next season you race in DIVISION %s", ROMAN[player_div()]);
    hud_text_c(226, b, 1, 0xE8E8EC);
    menu_help("ENTER CONTINUE");
}

static const char *lsr_items[4] = {"Load", "Save", "Replay", "Cancel"};

static void slot_label(int k, char *b, size_t n)
{
    Season s;

    if (season_peek(k, &s)) {
        snprintf(b, n, "-- empty --");
        return;
    }

    snprintf(b, n, "%s, season %d, Division %s", s.pname, s.season, ROMAN[s.div[PLAYER]]);
}

static void draw_slots(void)
{
    hud_text_c(80, menu.save_mode ? "SAVE the season" : "LOAD a season", 1.5f, 0xF0C040);
    char lab[4][80];
    const char *it[5];

    for (int k = 0; k < 4; k++) {
        slot_label(k + 1, lab[k], sizeof lab[k]);
        it[k] = lab[k];
    }

    it[4] = "Cancel";
    draw_bars(98, it, 5, menu.cursor[MS_SLOTS]);
    menu_help("ENTER CHOOSE   ESC BACK");
}

static void rp_scan(void)
{
    menu.replay_count = 0;

    for (int k = 1; k <= RP_SLOTS; k++) {
        if (!rp_read_head(k, &menu.replay_heads[menu.replay_count])) {
            menu.replay_slots[menu.replay_count++] = k;
        }
    }

    for (int i = 0; i < menu.replay_count; i++) {
        for (int j = i + 1; j < menu.replay_count; j++) {
            if (menu.replay_heads[j].seq > menu.replay_heads[i].seq) {
                RHead h = menu.replay_heads[i];
                menu.replay_heads[i] = menu.replay_heads[j];
                menu.replay_heads[j] = h;
                int s = menu.replay_slots[i];
                menu.replay_slots[i] = menu.replay_slots[j];
                menu.replay_slots[j] = s;
            }
        }
    }
}

static void draw_replays(void)
{
    hud_text_c(80, "REPLAY a race", 1.5f, 0xF0C040);

    if (!menu.replay_count) {
        hud_text_c(120, "No races recorded yet.", 1.5f, 0xE8E8EC);
        hud_text_c(140, "Season races are recorded, the last five are kept.", 1, 0xB0B0B8);
    }

    char lab[RP_SLOTS][96];
    const char *it[RP_SLOTS + 1];

    for (int i = 0; i < menu.replay_count; i++) {
        snprintf(lab[i], sizeof lab[i], "%s", menu.replay_heads[i].title);
        it[i] = lab[i];
    }

    it[menu.replay_count] = "Cancel";
    draw_bars(menu.replay_count ? 98 : 170, it, menu.replay_count + 1, menu.cursor[MS_REPLAYS]);
    menu_help("ENTER WATCH   ESC BACK");
}

/* the driving keys: an action per line, its two keys in cells on the right; then the gamepad's layout */
static void key_label(char *b, size_t n, SDL_Scancode sc)
{
    const char *s = sc ? SDL_GetKeyName(SDL_GetKeyFromScancode(sc, 0, false)) : "";
    snprintf(b, n, "%s", s && s[0] ? s : sc ? SDL_GetScancodeName(sc) : "-");
}

static void draw_controls(void)
{
    hud_text_c(80, "CONTROLS", 1.5f, 0xF0C040);
    float W = panel_w() - 30, x = (HW - W) / 2, cw = 74, h = BAR_H;
    static const char *const tail[2] = {"Default keys", "Back"};
    int sel = menu.cursor[MS_CTL];

    for (int r = 0; r < CT_ROWS; r++) {
        float y = 94 + r * BAR_GAP;
        draw_bar(x, y, W, r < NCT ? CT_NAME[r] : tail[r - NCT], r == sel);

        if (r >= NCT) {
            continue;
        }

        for (int k = 0; k < 2; k++) {
            float cx = x + W - (2 - k) * (cw + 3) - 1;
            int on = r == sel && k == menu.ctl_column;
            char b[48];
            key_label(b, sizeof b, ctl[r][k]);

            if (on && menu.ctl_waiting) {
                snprintf(b, sizeof b, "%s", (SDL_GetTicks() / 300) & 1 ? "PRESS A KEY" : "");
            }

            hud_rect(cx - 1, y + 1, cw + 2, h - 2, on ? 0xFFFFFF : 0x0A2426);
            plate(cx, y + 2, cw, h - 4, 0x2A2C30, 0x060608);
            float sc = fminf(1.f, (cw - 6) / fmaxf(text_width(b, 1), 1));
            hud_text_raw(cx + (cw - text_width(b, sc)) / 2, y + (h - 7 * sc) / 2, b, sc,
                         on ? (menu.ctl_waiting ? 0x60FF60 : 0xFFE060) : 0xE8ECEC);
        }
    }

    const char *pad = pad_name();
    char b[96];
    snprintf(b, sizeof b, "%.60s", pad ? pad : "No gamepad or joystick connected");
    hud_text_c(224, b, 1, pad ? 0x60FF60 : 0x9C9CA4);
    const char *pl =
        "PAD: STICK/D-PAD STEER   RT/A/UP GAS   LT/X/DOWN BRAKE   RB/B BOOST   Y RESCUE   START/BACK PAUSE";
    hud_text_c(236, pl, fminf(0.8f, (panel_w() - 12) / text_width(pl, 1)), 0xB0B0B8);
    menu_help(menu.ctl_waiting ? "PRESS THE NEW KEY   ESC CANCEL"
                               : "ENTER CHANGE   LEFT/RIGHT KEY 1/2   DEL CLEAR   ESC BACK");
}
