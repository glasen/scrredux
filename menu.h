/* menu.h - the menus, like the original's: logo, division, SELECT and grey bars (the chosen one orange) between two
 * wooden posts, over the stadium. Hall of Fame, Practise, the Racing Season, Load/Save/Replay and the track builder.
 * Included by sr_main.h */
enum { MS_MAIN, MS_HOF, MS_PRACTICE, MS_NAME, MS_SEASON, MS_RESULT, MS_END, MS_LSR, MS_SLOTS, MS_REPLAYS };
static int ms = MS_MAIN, msel[16], ms_save, ms_scroll, ask_quit, ask_paused;
static char name_buf[16], ms_note[80];
static float ms_note_t;
static int rp_slot[RP_SLOTS], nrp_list; static RHead rp_list[RP_SLOTS];
static int last_lap_rec, last_race_rec;

#define BAR_H 17.f
#define BAR_GAP 21.f
static float panel_w(void) { return fminf(HW - 70.f, 330.f); }
static void menu_note(const char *s) { snprintf(ms_note, sizeof ms_note, "%s", s); ms_note_t = 2.5f; }
static void fmt_time2(char *b, size_t n, float t) { if (t > 0) fmt_time(b, n, t); else snprintf(b, n, "-:--.-"); }
static void track_title(char *b, size_t n, int t) { snprintf(b, n, "The %s", trk[t].name); }

/* the title logo: italic two-tone lettering with a dark drop shadow */
static void draw_logo(float y)
{
    const char *s = "STUNT CAR RACER";
    float sc = fminf(4.2f, (HW - 30) / (15 * 6.f)), x = (HW - text_width(s, sc)) / 2;
    hud_text_ex(x + sc * 0.7f, y + sc * 0.7f, s, sc, 0x200808, 0x200808, 0.3f);
    hud_text_ex(x, y, s, sc, 0xFFC890, 0xC81810, 0.3f);
}
static void wood_post(float x, float y0, float w)
{
    hud_rect(x, y0, w, VH - y0, 0x7A4A24);
    hud_rect(x, y0, w * 0.22f, VH - y0, 0xA8703C);
    hud_rect(x + w * 0.78f, y0, w * 0.22f, VH - y0, 0x55311A);
    for (int k = 0; k < 14; k++) {                                  /* grain and knots */
        uint32_t h = hash2((int)x * 7 + k, 31);
        float gy = y0 + 6 + (h % 1000) / 1000.f * (VH - y0 - 12), gx = x + w * (0.3f + 0.4f * ((h >> 10) & 255) / 255.f);
        hud_rect(gx, gy, 1.f, 5.f + (h >> 20) % 9, 0x5A3418);
    }
    hud_rect(x - 1, y0 - 3, w + 2, 4, 0x8E5A2C);                    /* sawn top */
}
/* the frame every menu screen shares; returns the panel's left edge */
static float menu_frame(void)
{
    float W = panel_w(), x0 = (HW - W) / 2;
    draw_logo(10);
    char b[32]; snprintf(b, sizeof b, "DIVISION %s", ROMAN[sea.on ? player_div() : 3]);
    hud_text_c(55, b, 2, 0xF08A28);
    hud_rect(x0 - 4, 76, W + 8, VH - 76, 0x26262A);
    wood_post(x0 - 26, 70, 20); wood_post(x0 + W + 6, 70, 20);
    return x0;
}
static void draw_bar(float x, float y, float w, float h, const char *s, int on, float sc)
{
    uint32_t body = on ? 0xF08C1C : 0xA8A8A2, hi = on ? 0xFFC478 : 0xD8D8D2, lo = on ? 0x9A4A08 : 0x56565A;
    hud_rect(x, y, w, h, lo); hud_rect(x, y, w - 1, h - 1, hi); hud_rect(x + 1, y + 1, w - 2, h - 2, body);
    hud_rect(x + w - 9, y + h / 2 - 0.75f, 4, 1.5f, 0x303030);
    hud_text_raw(x + 7, y + (h - 7 * sc) / 2, s, sc, on ? 0x101010 : 0x1A1A1A);
}
static void draw_bars_sc(float y, const char *const *it, int n, int sel, int numbered, float sc)
{
    float W = panel_w() - 30, x = (HW - W) / 2;
    for (int i = 0; i < n; i++) {
        char b[96];
        if (numbered) snprintf(b, sizeof b, "%d. %s", i + 1, it[i]); else snprintf(b, sizeof b, "%s", it[i]);
        draw_bar(x, y + i * BAR_GAP, W, BAR_H, b, i == sel, fminf(sc, (W - 18) / text_width(b, 1)));
    }
}
static void draw_bars(float y, const char *const *it, int n, int sel, int numbered) { draw_bars_sc(y, it, n, sel, numbered, 1.5f); }
static void menu_help(const char *s) { hud_text_c(VH - 12, s, 1, 0x9C9CA4); }
/* "Really exit?": Y yes, N or ESC no */
static void ask_box(const char *sub)
{
    float w = 236, h = sub ? 46 : 32, x = (HW - w) / 2, y = 96;
    hud_rect(x - 2, y - 2, w + 4, h + 4, 0xF08C1C); hud_rect(x, y, w, h, 0x18181C);
    hud_text_c(y + 9, "REALLY EXIT?  Y/N", 2, 0xFFFFFF);
    if (sub) hud_text_c(y + 31, sub, 1, 0xFF8070);
}
static int ask_answer(SDL_Keycode k) { return k == SDLK_Y ? 1 : k == SDLK_N || k == SDLK_ESCAPE ? 0 : -1; }

/* ---- screens ---- */
static int main_items(const char **it)
{
    it[0] = "Hall of Fame"; it[1] = "Practise";
    it[2] = sea.on ? "Continue the Racing Season" : "Start the Racing Season";
    it[3] = "Load/Save/Replay"; it[4] = "Track Builder";
    return 5;
}
static void draw_hof(float x0)
{
    float W = panel_w();
    hud_text_c(80, "HALL of FAME", 1.5f, 0xF0C040);
    hud_text(x0 + 4, 96, "TRACK            LAP RECORD          RACE RECORD", 1, 0xF08A28);
    int n = ntrk, vis = 15, first = clampf(ms_scroll, 0, fmaxf(0, n - vis));
    for (int r = 0; r < vis && first + r < n; r++) {
        const TrackDef *td = &trk[first + r];
        const Hof *h = hof_get(td->name);
        char b[128], l[16], rr[16];
        fmt_time2(l, sizeof l, h ? h->lap : 0); fmt_time2(rr, sizeof rr, h ? h->race : 0);
        snprintf(b, sizeof b, "%-16.16s %-10.10s %6s  %-10.10s %7s", td->name, h && h->lap > 0 ? h->lapby : "-", l, h && h->race > 0 ? h->raceby : "-", rr);
        hud_text(x0 + 4, 108 + r * 10, b, 1, first + r < SCR_NTRACKS ? 0xE8E8EC : 0xA0D0FF);
    }
    (void)W;
    menu_help("UP/DOWN SCROLL   ESC BACK");
}
static void draw_practice(void)
{
    hud_text_c(80, "PRACTISE - choose a track", 1.5f, 0xF0C040);
    int n = ntrk, vis = 9, sel = msel[MS_PRACTICE];
    if (sel < ms_scroll) ms_scroll = sel;
    if (sel >= ms_scroll + vis) ms_scroll = sel - vis + 1;
    float W = panel_w() - 30, x = (HW - W) / 2;
    for (int r = 0; r < vis && ms_scroll + r < n; r++) {
        int i = ms_scroll + r; char b[64];
        snprintf(b, sizeof b, "%s%s", trk[i].name, i < SCR_NTRACKS ? "" : "  (own)");
        draw_bar(x, 96 + r * 17, W, 14, b, i == sel, 1.2f);
    }
    menu_help("ENTER DRIVE   ESC BACK   (ESC IN THE CAR ENDS PRACTISE)");
}
static void draw_name(void)
{
    hud_text_c(96, "A new Racing Season", 1.5f, 0xF0C040);
    hud_text_c(122, "Enter your name:", 1.5f, 0xE8E8EC);
    float W = 170, x = (HW - W) / 2;
    hud_rect(x, 140, W, 20, 0x56565A); hud_rect(x + 1, 141, W - 2, 18, 0x101014);
    char b[24]; snprintf(b, sizeof b, "%s%s", name_buf, (SDL_GetTicks() / 400) & 1 ? "_" : "");
    hud_text_raw(x + 6, 146, b, 1.5f, 0xFFE060);
    hud_text_c(176, "You start in DIVISION 4 against", 1, 0xB0B0B8);
    hud_text_c(188, "Jumping Jack and Road Hog.", 1, 0xB0B0B8);
    menu_help("ENTER START   ESC BACK");
}
static void div_rows(const Season *s, int d, float x, float y, float sc, int hl)
{
    int tab[3], w = sc < 1 ? 11 : 13; div_table(s, d, tab);
    char b[96];
    snprintf(b, sizeof b, "%-*s %5s %3s %3s %3s", w, "DRIVER", sc < 1 ? "RACED" : "RACED", "WIN", "LAP", "PTS");
    hud_text(x, y, b, sc, 0xF08A28);
    for (int r = 0; r < 3; r++) {
        int i = tab[r];
        snprintf(b, sizeof b, "%-*.*s %5d %3d %3d %3d", w, w, i == PLAYER ? s->pname : DRV_NAME[i], s->raced[i], s->wins[i], s->lapp[i], s->pts[i]);
        hud_text(x, y + (r + 1) * 10 * sc, b, sc, hl && i == PLAYER ? 0xFFE060 : 0xE8E8EC);
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
    track_title(t0, sizeof t0, DIV_TRACK[d][0]); track_title(t1, sizeof t1, DIV_TRACK[d][1]);
    snprintf(b, sizeof b, "Tracks in DIVISION %s: %s, %s", ROMAN[d], t0, t1);
    hud_text_c(146, b, 1, 0xB0B0B8);
    const Race *r = player_race();
    if (r) {
        int o = r->a == PLAYER ? r->b : r->a;
        snprintf(b, sizeof b, "Race %d of 4:  %s  v  %s", player_races_done() + 1, sea.pname, DRV_NAME[o]);
        hud_text_c(162, b, 1.3f, 0xFFFFFF);
        const Hof *h = hof_get(trk[r->track].name);
        track_title(t0, sizeof t0, r->track);
        char l[16]; fmt_time2(l, sizeof l, h ? h->lap : 0);
        snprintf(b, sizeof b, "%s - lap record %s, %s", t0, l, h && h->lap > 0 ? h->lapby : "-");
        hud_text_c(178, b, 1, 0xB0B0B8);
        draw_bars(196, season_items, 2, msel[MS_SEASON], 0);
    } else {
        hud_text_c(170, "The season is over", 1.5f, 0xFFFFFF);
        draw_bars(196, season_items + 1, 1, 0, 0);
    }
    menu_help("ENTER CHOOSE   ESC MAIN MENU");
}
static void result_line(float x, float y, int drv, float t, float lap, int pts, const char *why)
{
    char b[96], tt[16], ll[16];
    if (t > 0) fmt_time(tt, sizeof tt, t); else snprintf(tt, sizeof tt, "%s", why);
    fmt_time2(ll, sizeof ll, lap);
    snprintf(b, sizeof b, "%-13.13s %9s %9s    +%d", drv_name(drv), tt, ll, pts);
    hud_text(x, y, b, 1.2f, drv == PLAYER ? 0xFFE060 : 0xE8E8EC);
}
static void draw_result(float x0)
{
    const Race *r = &last_res;
    hud_text_c(80, "RACE RESULT", 1.5f, 0xF0C040);
    char b[128], t[48]; track_title(t, sizeof t, r->track);
    hud_text_c(96, t, 1, 0xB0B0B8);
    hud_text(x0 + 16, 110, "DRIVER         RACE-TIME  BEST-LAP  POINTS", 1.2f, 0xF08A28);
    for (int k = 0; k < 2; k++) {
        int dv = k ? r->b : r->a;
        float tm = k ? r->tb : r->ta, lp = k ? r->lb : r->la;
        int pts = (r->winner == dv) * 2 + (r->fast == dv);
        const char *why = dv == PLAYER && last_retired ? "RETIRED" : (dv == PLAYER ? result == -2 : result == 2) ? "WRECKED" : "-";
        result_line(x0 + 16, 124 + k * 13, dv, tm, lp, pts, why);
    }
    snprintf(b, sizeof b, "%s wins the race%s", drv_name(r->winner >= 0 ? r->winner : r->b),
             r->fast >= 0 ? "" : "");
    hud_text_c(156, b, 1.5f, r->winner == PLAYER ? 0x60FF60 : 0xFF8070);
    if (r->fast >= 0) { snprintf(b, sizeof b, "Fastest lap: %s", drv_name(r->fast)); hud_text_c(170, b, 1, 0xE8E8EC); }
    if (last_lap_rec || last_race_rec) hud_text_c(182, last_race_rec ? "New track record (race time)" : "New track record (lap)", 1, 0x60FF60);
    float y = 196;
    for (int k = 0; k < nlast_sim; k++) {
        const Race *o = &sea.sched[player_div()][last_sim[k]];
        snprintf(b, sizeof b, "%s beat %s%s%s", drv_name(o->winner), drv_name(o->winner == o->a ? o->b : o->a),
                 o->fast >= 0 ? ", fastest lap " : "", o->fast >= 0 ? drv_name(o->fast) : "");
        hud_text_c(y, b, 1, 0xB0B0B8); y += 11;
    }
    if (nlast_sim) hud_text_c(196 - 12, "Also raced in your division:", 1, 0x9C9CA4);
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
    const char *m = sea_moved == 2 ? "EXCELLENT DRIVING - WELL DONE" : sea_moved < 0 ? "PROMOTED!" : sea_moved > 0 ? "RELEGATED" : "You stay in your division";
    hud_text_c(206, m, 2, sea_moved == 1 ? 0xFF8070 : 0x60FF60);
    snprintf(b, sizeof b, "Next season you race in DIVISION %s", ROMAN[player_div()]);
    hud_text_c(226, b, 1, 0xE8E8EC);
    menu_help("ENTER CONTINUE");
}
static const char *lsr_items[4] = {"Load", "Save", "Replay", "Cancel"};
static void slot_label(int k, char *b, size_t n)
{
    Season s;
    if (season_peek(k, &s)) { snprintf(b, n, "-- empty --"); return; }
    snprintf(b, n, "%s, season %d, Division %s", s.pname, s.season, ROMAN[s.div[PLAYER]]);
}
static void draw_slots(void)
{
    hud_text_c(80, ms_save ? "SAVE the season" : "LOAD a season", 1.5f, 0xF0C040);
    char lab[4][80]; const char *it[5];
    for (int k = 0; k < 4; k++) { slot_label(k + 1, lab[k], sizeof lab[k]); it[k] = lab[k]; }
    it[4] = "Cancel";
    draw_bars(98, it, 5, msel[MS_SLOTS], 0);
    menu_help("ENTER CHOOSE   ESC BACK");
}
static void rp_scan(void)
{
    nrp_list = 0;
    for (int k = 1; k <= RP_SLOTS; k++) if (!rp_read_head(k, &rp_list[nrp_list])) rp_slot[nrp_list++] = k;
    for (int i = 0; i < nrp_list; i++) for (int j = i + 1; j < nrp_list; j++)
        if (rp_list[j].seq > rp_list[i].seq) { RHead h = rp_list[i]; rp_list[i] = rp_list[j]; rp_list[j] = h; int s = rp_slot[i]; rp_slot[i] = rp_slot[j]; rp_slot[j] = s; }
}
static void draw_replays(void)
{
    hud_text_c(80, "REPLAY a race", 1.5f, 0xF0C040);
    if (!nrp_list) { hud_text_c(120, "No races recorded yet.", 1.5f, 0xE8E8EC); hud_text_c(140, "Season races are recorded, the last five are kept.", 1, 0xB0B0B8); }
    char lab[RP_SLOTS][96]; const char *it[RP_SLOTS + 1];
    for (int i = 0; i < nrp_list; i++) { snprintf(lab[i], sizeof lab[i], "%s", rp_list[i].title); it[i] = lab[i]; }
    it[nrp_list] = "Cancel";
    draw_bars(nrp_list ? 98 : 170, it, nrp_list + 1, msel[MS_REPLAYS], 0);
    menu_help("ENTER WATCH   ESC BACK");
}
static void draw_menu(void)
{
    float x0 = menu_frame();
    const char *it[8];
    switch (ms) {
    case MS_MAIN: hud_text_c(84, "SELECT", 1.5f, 0xFFFFFF); draw_bars(100, it, main_items(it), msel[MS_MAIN], 1);
                  menu_help("UP/DOWN SELECT   ENTER CHOOSE   ESC QUIT"); break;
    case MS_HOF: draw_hof(x0); break;
    case MS_PRACTICE: draw_practice(); break;
    case MS_NAME: draw_name(); break;
    case MS_SEASON: draw_season(x0); break;
    case MS_RESULT: draw_result(x0); break;
    case MS_END: draw_end(x0); break;
    case MS_LSR: hud_text_c(84, "SELECT", 1.5f, 0xFFFFFF); draw_bars(100, lsr_items, 4, msel[MS_LSR], 1);
                 menu_help("ENTER CHOOSE   ESC BACK"); break;
    case MS_SLOTS: draw_slots(); break;
    case MS_REPLAYS: draw_replays(); break;
    }
    if (ms_note_t > 0) hud_text_c(VH - 26, ms_note, 1.5f, 0x60FF60);
    if (ask_quit) ask_box(NULL);
}

/* ---- going places ---- */
static void ed_open(void);
static void menu_go(int screen)
{
    ms = screen; ms_scroll = 0; ask_quit = 0;
    if (screen == MS_NAME) { snprintf(name_buf, sizeof name_buf, "%s", sea.pname[0] ? sea.pname : "Player"); if (win) SDL_StartTextInput(win); }
    else if (win) SDL_StopTextInput(win);
    if (screen == MS_REPLAYS) { rp_scan(); msel[MS_REPLAYS] = 0; }
    if (screen == MS_SEASON && sea.on && sea.race >= 6) {        /* a finished season (also one loaded from a save) */
        season_end(); season_write(0); ms = MS_END;
    } else if (screen == MS_SEASON) {                             /* the menu shows the next race's track */
        const Race *r = player_race();
        if (r && sel_track != r->track) { sel_track = r->track; setup_menu(); }
        msel[MS_SEASON] = r ? 0 : 1;
    }
    if (state != ST_MENU) setup_menu();
}
/* ---- before a race: the whole track from the stands in a chequered frame, its name below, like the original ---- */
#define PV_Q 8.f                                                 /* the chequers */
static void pv_frame(float hw, float f[4])                       /* the menu: the whole screen is the picture */
{
    if (state == ST_MENU) { f[0] = f[1] = -PV_Q - 1.5f; f[2] = hw + PV_Q + 1.5f; f[3] = VH + PV_Q + 1.5f; }
    else { f[0] = 10; f[1] = 8; f[2] = hw - 10; f[3] = VH - 54; }
}
#define CHQ(k) ((k) & 1 ? 0xD8D8D8u : 0x0C0C0Cu)
static void pv_bounds(V3 pos, V3 fwd, V3 right, V3 up, float fx, float b[4])
{
    b[0] = b[1] = 1e9f; b[2] = b[3] = -1e9f;
    for (int k = 0; k < nseg; k += 4) for (int e = 0; e < 3; e++) {
        V3 p = e == 2 ? V(seg[k].c.x, 0, seg[k].c.z) : vadd(seg[k].c, vmul(seg[k].R, e ? TRACK_HW : -TRACK_HW));
        V3 d = vsub(p, pos); float z = fmaxf(vdot(d, fwd), 0.5f), x = vdot(d, right) / z * fx, y = vdot(d, up) / z * FOCAL_Y;
        b[0] = fminf(b[0], x); b[1] = fminf(b[1], y); b[2] = fmaxf(b[2], x); b[3] = fmaxf(b[3], y);
    }
}
/* menu and preview: the camera turns slowly round the track, starting square to its long axis, from above and at one distance: the
 * nearest that keeps the track in the picture all the way round. Worked out at PV_NA angles, the aim between them */
#define PV_NA 24
#define PV_EL 0.42f
static V3 pv_fwd(float th) { return vnorm(V(cosf(th) * cosf(PV_EL), -sinf(PV_EL), sinf(th) * cosf(PV_EL))); }
static void pv_fit(V3 fwd, float fx, const float n[4], V3 *tg, float *D, int fixed)
{
    V3 right = vnorm(vcross(fwd, UP)), up = vcross(right, fwd);
    float b[4];
    for (int it = 0; it < 3; it++) {
        if (!fixed) {
            float lo = 5, hi = 5000;
            for (int k = 0; k < 24; k++) {
                *D = 0.5f * (lo + hi);
                pv_bounds(vsub(*tg, vmul(fwd, *D)), fwd, right, up, fx, b);
                if (b[2] - b[0] <= (n[2] - n[0]) * 0.92f && b[3] - b[1] <= (n[3] - n[1]) * 0.92f) hi = *D; else lo = *D;
            }
            *D = hi;
        }
        pv_bounds(vsub(*tg, vmul(fwd, *D)), fwd, right, up, fx, b);
        *tg = vadd(*tg, vadd(vmul(right, ((b[0] + b[2]) - (n[0] + n[2])) * 0.5f / fx * *D), vmul(up, ((b[1] + b[3]) - (n[1] + n[3])) * 0.5f / FOCAL_Y * *D)));
    }
}
static void preview_cam(float asp)
{
    static float was_asp, th0, D; static int was_gen = -1, was_menu; static V3 tg[PV_NA];
    if (asp != was_asp || track_gen != was_gen || (state == ST_MENU) != was_menu) {
        was_asp = asp; was_gen = track_gen; was_menu = state == ST_MENU;
        float f[4], hw = VH * asp, in = PV_Q + 1.5f, fx = FOCAL_Y / asp; pv_frame(hw, f);
        float n[4] = {(f[0] + in) / hw * 2 - 1, 1 - (f[3] - in) / VH * 2, (f[2] - in) / hw * 2 - 1, 1 - (f[1] + in) / VH * 2};
        double mx = 0, mz = 0, sxx = 0, szz = 0, sxz = 0; float ymax = 0;
        for (int k = 0; k < nseg; k++) { mx += seg[k].c.x; mz += seg[k].c.z; ymax = fmaxf(ymax, seg[k].c.y); }
        mx /= nseg; mz /= nseg;
        for (int k = 0; k < nseg; k++) { double dx = seg[k].c.x - mx, dz = seg[k].c.z - mz; sxx += dx * dx; szz += dz * dz; sxz += dx * dz; }
        th0 = 0.5f * (float)atan2(2 * sxz, sxx - szz) + PI_F / 2 + 0.3f;
        if (sinf(th0) < 0) th0 += PI_F;
        D = 0;
        for (int a = 0; a < PV_NA; a++) {
            float d; tg[a] = V((float)mx, ymax * 0.4f, (float)mz);
            pv_fit(pv_fwd(th0 + 2 * PI_F * a / PV_NA), fx, n, &tg[a], &d, 0);
            D = fmaxf(D, d);
        }
        for (int a = 0; a < PV_NA; a++) pv_fit(pv_fwd(th0 + 2 * PI_F * a / PV_NA), fx, n, &tg[a], &D, 1);
    }
    float u = fmodf(orbit_t * 0.12f / (2 * PI_F), 1.f) * PV_NA, fr = u - floorf(u);
    int a = (int)u % PV_NA;
    V3 fwd = pv_fwd(th0 + 2 * PI_F * u / PV_NA), t = vlerp(tg[a], tg[(a + 1) % PV_NA], fr);
    cam.pos = vsub(t, vmul(fwd, D)); cam.fwd = fwd; cam.right = vnorm(vcross(fwd, UP)); cam.up = vcross(cam.right, fwd);
}
static void draw_preview(void)
{
    float f[4], q = PV_Q; pv_frame(HW, f);
    uint32_t teal = 0x1C6266;
    hud_rect(0, 0, HW, f[1], teal); hud_rect(0, f[3], HW, VH - f[3], teal);
    hud_rect(0, f[1], f[0], f[3] - f[1], teal); hud_rect(f[2], f[1], HW - f[2], f[3] - f[1], teal);
    int n = (int)roundf((f[2] - f[0]) / q), m = (int)roundf((f[3] - f[1]) / q);
    float w = (f[2] - f[0]) / n, hh = (f[3] - f[1]) / m;
    for (int j = 0; j < m; j++) {
        hud_rect(f[0], f[1] + j * hh, q, hh, CHQ(j)); hud_rect(f[2] - q, f[1] + j * hh, q, hh, CHQ(n - 1 + j));
    }
    for (int i = 0; i < n; i++) {
        hud_rect(f[0] + i * w, f[1], w, q, CHQ(i)); hud_rect(f[0] + i * w, f[3] - q, w, q, CHQ(i + m - 1));
    }
    hud_rect(f[0] + q, f[1] + q, f[2] - f[0] - 2 * q, 1.5f, 0xF4F4F4); hud_rect(f[0] + q, f[3] - q - 1.5f, f[2] - f[0] - 2 * q, 1.5f, 0xF4F4F4);
    hud_rect(f[0] + q, f[1] + q, 1.5f, f[3] - f[1] - 2 * q, 0xF4F4F4); hud_rect(f[2] - q - 1.5f, f[1] + q, 1.5f, f[3] - f[1] - 2 * q, 0xF4F4F4);
    char b[48]; track_title(b, sizeof b, cur_track);
    float sc = 2, bw = fminf(fmaxf(text_width(b, sc) + 60, HW * 0.45f), HW - 40), bh = 24, bx = (HW - bw) / 2, by = VH - 46;
    if (text_width(b, sc) > bw - 16) sc = (bw - 16) / text_width(b, 1);
    hud_rect(bx - 2, by - 2, bw + 4, bh + 4, 0xB4B4B4); hud_rect(bx, by, bw, bh, 0x6E6E6E); hud_rect(bx + 1.5f, by + 1.5f, bw - 3, bh - 3, 0x080808);
    hud_text_raw((HW - text_width(b, sc) + sc) / 2, by + (bh - 7 * sc) / 2, b, sc, 0xF0F0F0);
    hud_text_c(VH - 12, "ENTER START   ESC BACK", 1, 0xA8CCCC);
}
static void preview_start(void)
{
    memset(cars, 0, sizeof cars); npart = 0; paused = 0;
    state = ST_PREVIEW; orbit_t = 0;
}
static void preview_key(SDL_Keycode k)
{
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) start_race();
    else if (k == SDLK_ESCAPE) menu_go(race_mode == RM_SEASON ? MS_SEASON : MS_PRACTICE);
}
static void season_race(void)
{
    Race *r = player_race();
    if (!r) return;
    race_mode = RM_SEASON; set_opponent(r->a == PLAYER ? r->b : r->a);
    sel_track = r->track;
    if (cur_track != sel_track) build_track(sel_track);
    cockpit = 1; preview_start();
}
/* out of a race (ENTER at the end, or ESC): the season's books, then back to where it came from */
static void ed_return(void);
static void race_leave(int retired)
{
    rec_on = rec_on && race_mode == RM_SEASON;
    if (race_mode == RM_SEASON) {
        Race *r = player_race();
        if (!r) { menu_go(MS_SEASON); return; }
        const Car *p = &cars[0], *o = &cars[1];
        float tp = !retired && p->finished && !p->wrecked ? p->ftime : -1, to = !retired && o->finished && !o->wrecked ? o->ftime : -1;
        int win_p = !retired && result > 0;
        float ta = r->a == PLAYER ? tp : to, tb = r->a == PLAYER ? to : tp;
        float la = r->a == PLAYER ? p->bestlap : o->bestlap, lb = r->a == PLAYER ? o->bestlap : p->bestlap;
        race_score(r, win_p ? PLAYER : opp_drv, ta, tb, la, lb);
        last_res = *r; last_retired = retired; last_lap_rec = last_race_rec = 0;
        const char *tn = trk[r->track].name;
        if (tp > 0) last_race_rec |= hof_race(tn, player_name(), tp);
        if (to > 0) hof_race(tn, drv_name(opp_drv), to);
        sea.nholes = p->nholes; memcpy(sea.holes, p->holes, sizeof sea.holes);
        char title[64];
        snprintf(title, sizeof title, "%s - %s v %s - %s", tn, player_name(), drv_name(opp_drv), win_p ? "won" : "lost");
        rec_save(title, player_name(), drv_name(opp_drv), result);
        season_advance();
        season_write(0);
        menu_go(MS_RESULT);
    } else if (race_mode == RM_TEST) ed_return();
    else menu_go(race_mode == RM_PRACTICE ? MS_PRACTICE : MS_MAIN);
}
static void replay_start(int i)
{
    if (rp_load(rp_slot[i])) { menu_note("That replay cannot be read"); return; }
    RHead *h = &rp_head;
    if (h->td.orig >= 0 && h->td.orig < SCR_NTRACKS) { if (cur_track != h->td.orig) build_track(h->td.orig); }
    else { trk[ED_SLOT] = h->td; build_track(ED_SLOT); }
    memset(cars, 0, sizeof cars);
    for (int k = 0; k < 2; k++) { cars[k].col = h->col[k]; memcpy(cars[k].holes, h->holes[k], sizeof cars[k].holes); }
    laps = h->laps; npart = 0; rp_t = 0; rp_speed = 1; rp_paused = 0; rp_cam = RC_COCKPIT; paused = 0;
    state = ST_REPLAY;
    rp_step(0);
    update_camera(&cars[0], 0.016f, 1);
}
static void replay_leave(void) { free(rpb); rpb = NULL; nrp = 0; sel_track = sel_track < ntrk ? sel_track : 0; menu_go(MS_REPLAYS); }

static void menu_text(const char *t)
{
    if (ms != MS_NAME) return;
    for (; *t; t++) {
        size_t n = strlen(name_buf);
        if (n + 1 < sizeof name_buf && (strchr(FONT_CHARS, *t) || (*t == ' ' && n))) { name_buf[n] = *t; name_buf[n + 1] = 0; }
    }
}
/* returns 0 to quit the game */
static int menu_key(SDL_Keycode k)
{
    if (ask_quit) { int a = ask_answer(k); if (a >= 0) ask_quit = 0; return a != 1; }
    int *sel = &msel[ms], up = k == SDLK_UP, dn = k == SDLK_DOWN, ok = k == SDLK_RETURN || k == SDLK_KP_ENTER, back = k == SDLK_ESCAPE;
    const char *it[8];
    switch (ms) {
    case MS_MAIN: {
        int n = main_items(it);
        if (k >= SDLK_1 && k < SDLK_1 + n) { *sel = (int)(k - SDLK_1); ok = 1; }
        if (up) *sel = (*sel + n - 1) % n;
        if (dn) *sel = (*sel + 1) % n;
        if (back) { ask_quit = 1; break; }
        if (!ok) break;
        if (*sel == 0) { hof_load(); menu_go(MS_HOF); }
        else if (*sel == 1) { msel[MS_PRACTICE] = sel_track < ntrk ? sel_track : 0; menu_go(MS_PRACTICE); }
        else if (*sel == 2) menu_go(sea.on ? MS_SEASON : MS_NAME);
        else if (*sel == 3) menu_go(MS_LSR);
        else ed_open();
        break; }
    case MS_HOF:
        if (up) ms_scroll = ms_scroll > 0 ? ms_scroll - 1 : 0;
        if (dn && ms_scroll + 15 < ntrk) ms_scroll++;
        if (back || ok) menu_go(MS_MAIN);
        break;
    case MS_PRACTICE:
        if (up) *sel = (*sel + ntrk - 1) % ntrk;
        if (dn) *sel = (*sel + 1) % ntrk;
        if (back) menu_go(MS_MAIN);
        if (ok) { sel_track = *sel; if (cur_track != sel_track) build_track(sel_track); race_mode = RM_PRACTICE; cockpit = 1; preview_start(); }
        break;
    case MS_NAME:
        if (k == SDLK_BACKSPACE && name_buf[0]) name_buf[strlen(name_buf) - 1] = 0;
        if (back) menu_go(MS_MAIN);
        if (ok && name_buf[0]) { season_new(name_buf); season_write(0); menu_go(MS_SEASON); }
        break;
    case MS_SEASON:
        if (up || dn) *sel ^= 1;
        if (!player_race()) *sel = 1;
        if (back || (ok && *sel == 1)) menu_go(MS_MAIN);
        else if (ok) season_race();
        break;
    case MS_RESULT:
        if (ok || back) menu_go(MS_SEASON);
        break;
    case MS_END:
        if (ok || back) menu_go(MS_SEASON);
        break;
    case MS_LSR:
        if (k >= SDLK_1 && k <= SDLK_4) { *sel = (int)(k - SDLK_1); ok = 1; }
        if (up) *sel = (*sel + 3) % 4;
        if (dn) *sel = (*sel + 1) % 4;
        if (back || (ok && *sel == 3)) menu_go(MS_MAIN);
        else if (ok && *sel == 2) menu_go(MS_REPLAYS);
        else if (ok) {
            if (*sel == 1 && !sea.on) { menu_note("No season to save yet"); break; }
            ms_save = *sel == 1; msel[MS_SLOTS] = 0; menu_go(MS_SLOTS);
        }
        break;
    case MS_SLOTS:
        if (up) *sel = (*sel + 4) % 5;
        if (dn) *sel = (*sel + 1) % 5;
        if (back || (ok && *sel == 4)) menu_go(MS_LSR);
        else if (ok && ms_save) { menu_note(season_write(*sel + 1) ? "Could not save" : "Season saved"); }
        else if (ok) {
            if (season_read(*sel + 1)) menu_note("That slot is empty");
            else { season_write(0); menu_note("Season loaded"); menu_go(MS_SEASON); }
        }
        break;
    case MS_REPLAYS: {
        int n = nrp_list + 1;
        if (up) *sel = (*sel + n - 1) % n;
        if (dn) *sel = (*sel + 1) % n;
        if (back || (ok && *sel == nrp_list)) menu_go(MS_LSR);
        else if (ok) replay_start(*sel);
        break; }
    }
    return 1;
}

/* ---- in the car: race HUD extras, and keys ---- */
static void race_hud_extra(void)
{
    char b[96];
    if (race_mode == RM_SEASON && state == ST_COUNT) {
        snprintf(b, sizeof b, "%s  v  %s", player_name(), drv_name(opp_drv));
        hud_text_c(70, b, 2, 0xFFFFFF);
    }
    if (race_mode == RM_PRACTICE) hud_text_c(36, "PRACTISE", 1.5f, 0xFFE060);
    if (race_mode == RM_TEST) hud_text_c(36, "TEST DRIVE - ESC: BUILDER", 1.5f, 0xFFE060);
    if (ask_quit) ask_box(race_mode == RM_SEASON ? "YOU RETIRE AND LOSE THE RACE" : NULL);
}
static int race_key(SDL_Keycode k)
{
    if (ask_quit) {
        int a = ask_answer(k);
        if (a >= 0) { ask_quit = 0; paused = ask_paused; }
        if (a == 1) { paused = 0; if (race_mode == RM_SEASON) result = -1; race_leave(1); }
        return 1;
    }
    if (k == SDLK_ESCAPE) {
        if (state == ST_DONE) race_leave(0);
        else { ask_quit = 1; ask_paused = paused; paused = 1; }
        return 1;
    }
    if (k == SDLK_P && state == ST_RACE) paused = !paused;
    else if (k == SDLK_R && state == ST_RACE && !car_out(&cars[0])) { car_hurt(&cars[0], 8, 0); start_crane(&cars[0], restart_seg(&cars[0]), 0, 2.6f); }
    else if (state == ST_DONE && (k == SDLK_RETURN || k == SDLK_KP_ENTER) && state_t > 1.5f) race_leave(0);
    return 1;
}

/* ---- replay ---- */
static void draw_replay_hud(void)
{
    char b[128], t[16];
    hud_text(8, 8, "REPLAY", 2, 0xFF5050);
    hud_text(8, 26, rp_head.title, 1, 0xE8E8EC);
    fmt_time(t, sizeof t, racetime);
    int lp = cars[0].lap < 1 ? 1 : cars[0].lap > rp_head.laps ? rp_head.laps : cars[0].lap;
    snprintf(b, sizeof b, "LAP %d/%d  %s", lp, rp_head.laps, t);
    hud_text(HW - 8 - text_width(b, 1.5f), 8, b, 1.5f, 0xFFE060);
    snprintf(b, sizeof b, "CAMERA: %s%s", RC_NAME[rp_cam], rp_paused ? "   PAUSED" : rp_speed > 1 ? "   >>" : rp_speed < 0 ? "   <<" : "");
    hud_text(HW - 8 - text_width(b, 1), 26, b, 1, 0xE8E8EC);
    float W = HW - 40, f = nrp > 1 ? rp_t / ((nrp - 1) / (float)RP_HZ) : 0;
    hud_rect(20, VH - 22, W, 4, 0x303036); hud_rect(20, VH - 22, W * f, 4, 0xF08C1C);
    hud_text_c(VH - 12, "C CAMERA   SPACE PAUSE   LEFT/RIGHT REWIND/FAST   HOME START   ESC BACK", 1, 0xC0C0C8);
}
static void replay_key(SDL_Keycode k)
{
    if (k == SDLK_ESCAPE) replay_leave();
    else if (k == SDLK_C) { rp_cam = (rp_cam + 1) % NRC; if (!cars[1].active && (rp_cam == RC_OPP_COCKPIT || rp_cam == RC_OPP_CHASE)) rp_cam = RC_COCKPIT; update_camera(&cars[cam_car()], 0.016f, 1); }
    else if (k == SDLK_SPACE) rp_paused = !rp_paused;
    else if (k == SDLK_HOME) { rp_t = 0; npart = 0; }
}
static void replay_hold(void)          /* held keys: wind back or forward at four times the speed */
{
    rp_speed = 1;
    if (keys && keys[SDL_SCANCODE_RIGHT]) rp_speed = 4;
    if (keys && keys[SDL_SCANCODE_LEFT]) rp_speed = -4;
}
