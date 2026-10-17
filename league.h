/* league.h - the racing season (the original's single player league: four divisions of three drivers, two tracks
 * each, every driver races the other two on both, 2 points a win and 1 for the fastest lap, the top driver goes up and
 * the bottom one down), the Hall of Fame, season saves and race replays. Included by stuntracer.c */
#define NDRV 12
#define PLAYER 11
static const char *const DRV_NAME[11] = {"Hot Rod", "Whizz Kid", "Bad Guy", "The Dodger", "Big Ed", "Max Boost",
                                         "Dare Devil", "High Flyer", "Bully Boy", "Jumping Jack", "Road Hog"};
static const int DIV_TRACK[4][2] = {{5, 4}, {6, 7}, {1, 3}, {0, 2}};   /* Division I .. IV */
/* the autopilot's best laps (--sim): the yardstick for races between computer drivers and the Hall of Fame's
 * starting records */
static const float REF_LAP[SCR_NTRACKS] = {49.4f, 53.4f, 57.7f, 49.0f, 57.8f, 85.6f, 56.9f, 87.4f};
static const char *const ROMAN[4] = {"1", "2", "3", "4"};

enum { RM_DEBUG, RM_SEASON, RM_PRACTICE, RM_TEST };
static int race_mode = RM_DEBUG, opp_drv = 9;
static float opp_vcap = 62, opp_alat = 13.5f;

typedef struct { int a, b, track, done, winner, fast; float ta, tb, la, lb; } Race;   /* times < 0: did not finish */
typedef struct {
    uint32_t magic, ver;
    int on, season, race;                   /* race: the next of each division's six, raced in step */
    char pname[16];
    int div[NDRV], pts[NDRV], raced[NDRV], wins[NDRV], lapp[NDRV];
    float holes[6]; int nholes;             /* the player's cage damage stays for the season */
    Race sched[4][6];
} Season;
#define SEA_MAGIC 0x53525345u
static Season sea, sea_end;                 /* sea_end: the season just finished, for its final tables */
static int sea_moved;                       /* player at the season's end: -1 promoted, +1 relegated, 2 champion */
static Race last_res; static int last_sim[6], nlast_sim, last_retired;
static uint32_t sea_rng = 77;
static float sea_rand(void) { sea_rng = sea_rng * 1664525u + 1013904223u; return (sea_rng >> 8) / 16777216.0f; }

static const char *drv_name(int i) { return i == PLAYER ? sea.pname : DRV_NAME[i]; }
static float drv_rating(int i) { return i == PLAYER ? 0.5f : 1.f - i * 0.085f; }
static void set_opponent(int i)
{
    float r = drv_rating(i);
    opp_drv = i; opp_vcap = 56.f + 13.f * r; opp_alat = 11.f + 5.f * r;
}

static void season_schedule(void)
{
    sea.race = 0; sea.nholes = 0;
    memset(sea.pts, 0, sizeof sea.pts); memset(sea.raced, 0, sizeof sea.raced);
    memset(sea.wins, 0, sizeof sea.wins); memset(sea.lapp, 0, sizeof sea.lapp);
    for (int d = 0; d < 4; d++) {
        int m[3], n = 0;
        if (sea.div[PLAYER] == d) m[n++] = PLAYER;                       /* the player's races come first */
        for (int i = 0; i < 11; i++) if (sea.div[i] == d && n < 3) m[n++] = i;
        static const int pa[3] = {0, 0, 1}, pb[3] = {1, 2, 2};
        for (int k = 0; k < 6; k++) {
            Race *r = &sea.sched[d][k];
            memset(r, 0, sizeof *r);
            r->a = m[pa[k % 3]]; r->b = m[pb[k % 3]]; r->track = DIV_TRACK[d][k / 3]; r->winner = r->fast = -1;
        }
    }
}
static void season_new(const char *name)
{
    memset(&sea, 0, sizeof sea);
    sea.magic = SEA_MAGIC; sea.ver = 1; sea.on = 1; sea.season = 1;
    snprintf(sea.pname, sizeof sea.pname, "%s", name[0] ? name : "Player");
    for (int i = 0; i < 11; i++) sea.div[i] = i < 9 ? i / 3 : 3;
    sea.div[PLAYER] = 3;
    season_schedule();
}
static int player_div(void) { return sea.div[PLAYER]; }
static Race *player_race(void)
{
    if (sea.race >= 6) return NULL;
    Race *r = &sea.sched[player_div()][sea.race];
    return (r->a == PLAYER || r->b == PLAYER) && !r->done ? r : NULL;
}
static int player_races_done(void)
{
    int n = 0;
    for (int k = 0; k < 6; k++) { const Race *r = &sea.sched[player_div()][k]; n += r->done && (r->a == PLAYER || r->b == PLAYER); }
    return n;
}

static void race_score(Race *r, int winner, float ta, float tb, float la, float lb)
{
    r->ta = ta; r->tb = tb; r->la = la; r->lb = lb; r->done = 1;
    r->winner = winner;
    r->fast = la > 0 && (lb <= 0 || la <= lb) ? r->a : lb > 0 ? r->b : -1;
    sea.raced[r->a]++; sea.raced[r->b]++;
    if (r->winner >= 0) { sea.pts[r->winner] += 2; sea.wins[r->winner]++; }
    if (r->fast >= 0) { sea.pts[r->fast] += 1; sea.lapp[r->fast]++; }
}
/* a race between two computer drivers: three laps around their usual pace, now and then a fall off the track and a
 * wait for the crane, rarely a wreck */
static void sim_race(Race *r)
{
    float t[2], l[2];
    for (int k = 0; k < 2; k++) {
        float rt = drv_rating(k ? r->b : r->a), base = REF_LAP[r->track] * (1.12f - 0.14f * rt), tot = 6.f;
        l[k] = 1e9f;
        for (int lap = 0; lap < 3; lap++) {
            float g = (sea_rand() + sea_rand() + sea_rand() - 1.5f) * 0.04f, lt = base * (1 + g);
            if (sea_rand() < 0.12f * (1 - rt) + 0.03f) lt += 12.f + 6.f * sea_rand();
            l[k] = fminf(l[k], lt); tot += lt;
        }
        t[k] = sea_rand() < 0.05f * (1 - rt) + 0.01f ? -1.f : tot;
    }
    int w = t[0] >= 0 && (t[1] < 0 || t[0] <= t[1]) ? r->a : t[1] >= 0 ? r->b : sea_rand() < 0.5f ? r->a : r->b;
    race_score(r, w, t[0], t[1], l[0], l[1]);
}
/* every division races in step; stops at the player's next race */
static void season_advance(void)
{
    nlast_sim = 0;
    while (sea.race < 6) {
        for (int d = 0; d < 4; d++) {
            Race *r = &sea.sched[d][sea.race];
            if (r->done || r->a == PLAYER || r->b == PLAYER) continue;
            sim_race(r);
            if (d == player_div()) last_sim[nlast_sim++] = sea.race;
        }
        if (player_race()) return;
        sea.race++;
    }
}
static int rank_cmp(const Season *s, int a, int b)
{
    if (s->pts[a] != s->pts[b]) return s->pts[b] - s->pts[a];
    if (s->wins[a] != s->wins[b]) return s->wins[b] - s->wins[a];
    if (s->lapp[a] != s->lapp[b]) return s->lapp[b] - s->lapp[a];
    return a == PLAYER ? -1 : b == PLAYER ? 1 : a - b;
}
static void div_table(const Season *s, int d, int out[3])
{
    int n = 0;
    for (int i = 0; i < NDRV; i++) if (s->div[i] == d && n < 3) out[n++] = i;
    for (int i = 0; i < 3; i++) for (int j = i + 1; j < 3; j++) if (rank_cmp(s, out[j], out[i]) < 0) { int t = out[i]; out[i] = out[j]; out[j] = t; }
}
/* season over: the top driver of each division goes up, the bottom one down, then the next season's fixtures */
static void season_end(void)
{
    sea_end = sea;
    int tab[4][3];
    for (int d = 0; d < 4; d++) div_table(&sea, d, tab[d]);
    sea_moved = 0;
    for (int d = 0; d < 4; d++) {
        if (d > 0) { sea.div[tab[d][0]] = d - 1; if (tab[d][0] == PLAYER) sea_moved = -1; }
        if (d < 3) { sea.div[tab[d][2]] = d + 1; if (tab[d][2] == PLAYER) sea_moved = 1; }
    }
    if (tab[0][0] == PLAYER) sea_moved = 2;
    sea.season++;
    season_schedule();
}

/* ---- saves: save/season0.sav is the running season, 1..4 the Load/Save slots ---- */
static void save_path(char *out, size_t n, const char *fmt, int k)
{
    char rel[64]; snprintf(rel, sizeof rel, fmt, k);
    data_path(out, n, "save"); SDL_CreateDirectory(out);
    data_path(out, n, rel);
}
static int season_write(int slot)
{
    char p[1024]; save_path(p, sizeof p, "save/season%d.sav", slot);
    FILE *f = fopen(p, "wb"); if (!f) return -1;
    int ok = fwrite(&sea, sizeof sea, 1, f) == 1;
    fclose(f);
    return ok ? 0 : -1;
}
static int season_peek(int slot, Season *s)
{
    char p[1024]; save_path(p, sizeof p, "save/season%d.sav", slot);
    FILE *f = fopen(p, "rb"); if (!f) return -1;
    int ok = fread(s, sizeof *s, 1, f) == 1 && s->magic == SEA_MAGIC && s->ver == 1;
    fclose(f);
    return ok ? 0 : -1;
}
static int season_read(int slot)
{
    Season s;
    if (season_peek(slot, &s)) return -1;
    sea = s;
    return 0;
}

/* ---- Hall of Fame: the best lap and the best race time on every track, and who drove them ---- */
typedef struct { char track[32], lapby[16], raceby[16]; float lap, race; } Hof;
static Hof hof[MAXTRK + SCR_NTRACKS]; static int nhof;
static void hof_save(void)
{
    char p[1024]; save_path(p, sizeof p, "save/halloffame.txt", 0);
    FILE *f = fopen(p, "w"); if (!f) return;
    for (int i = 0; i < nhof; i++) fprintf(f, "%s|%s|%.2f|%s|%.2f\n", hof[i].track, hof[i].lapby, hof[i].lap, hof[i].raceby, hof[i].race);
    fclose(f);
}
static Hof *hof_get(const char *track)
{
    for (int i = 0; i < nhof; i++) if (!strcmp(hof[i].track, track)) return &hof[i];
    if (nhof >= (int)(sizeof hof / sizeof hof[0])) return NULL;
    Hof *h = &hof[nhof++];
    memset(h, 0, sizeof *h);
    snprintf(h->track, sizeof h->track, "%s", track);
    for (int t = 0; t < SCR_NTRACKS; t++)
        if (!strcmp(SCR_TITLE[t], track)) {                          /* the originals start with records to beat */
            snprintf(h->lapby, sizeof h->lapby, "%s", DRV_NAME[(t * 3) % 11]); h->lap = REF_LAP[t] * 1.05f;
            snprintf(h->raceby, sizeof h->raceby, "%s", DRV_NAME[(t * 3 + 4) % 11]); h->race = REF_LAP[t] * 3 * 1.08f + 6.f;
        }
    return h;
}
static void hof_load(void)
{
    nhof = 0;
    for (int t = 0; t < SCR_NTRACKS; t++) hof_get(SCR_TITLE[t]);
    char p[1024], line[256]; save_path(p, sizeof p, "save/halloffame.txt", 0);
    FILE *f = fopen(p, "r"); if (!f) return;
    while (fgets(line, sizeof line, f)) {
        char *fld[5]; int n = 0;
        line[strcspn(line, "\r\n")] = 0;
        for (char *s = line; n < 5; ) { fld[n++] = s; s = strchr(s, '|'); if (!s) break; *s++ = 0; }
        if (n < 5) continue;
        Hof *h = hof_get(fld[0]); if (!h) break;
        snprintf(h->lapby, sizeof h->lapby, "%s", fld[1]); h->lap = (float)atof(fld[2]);
        snprintf(h->raceby, sizeof h->raceby, "%s", fld[3]); h->race = (float)atof(fld[4]);
    }
    fclose(f);
}
static int hof_lap(const char *track, const char *who, float t)
{
    Hof *h = hof_get(track);
    if (!h || t <= 0 || (h->lap > 0 && t >= h->lap)) return 0;
    snprintf(h->lapby, sizeof h->lapby, "%s", who); h->lap = t; hof_save();
    return 1;
}
static int hof_race(const char *track, const char *who, float t)
{
    Hof *h = hof_get(track);
    if (!h || t <= 0 || (h->race > 0 && t >= h->race)) return 0;
    snprintf(h->raceby, sizeof h->raceby, "%s", who); h->race = t; hof_save();
    return 1;
}
