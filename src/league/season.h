/* league/season.h - season schedule, simulated races, standings */
#pragma once

static const char *drv_name(int i)
{
    return i == PLAYER ? sea.pname : DRV_NAME[i];
}
static float drv_rating(int i)
{
    return i == PLAYER ? 0.5f : 1.f - i * 0.085f;
}
static void set_opponent(int i)
{
    float r = drv_rating(i);
    opp_drv = i;
    opp_vcap = 56.f + 13.f * r;
    opp_alat = 11.f + 5.f * r;
}

static void season_schedule(void)
{
    sea.race = 0;
    sea.nholes = 0;
    memset(sea.pts, 0, sizeof sea.pts);
    memset(sea.raced, 0, sizeof sea.raced);
    memset(sea.wins, 0, sizeof sea.wins);
    memset(sea.lapp, 0, sizeof sea.lapp);
    for (int d = 0; d < 4; d++) {
        int m[3], n = 0;
        if (sea.div[PLAYER] == d) {
            m[n++] = PLAYER; /* the player's races come first */
        }
        for (int i = 0; i < 11; i++) {
            if (sea.div[i] == d && n < 3) {
                m[n++] = i;
            }
        }
        static const int pa[3] = {0, 0, 1}, pb[3] = {1, 2, 2};
        for (int k = 0; k < 6; k++) {
            Race *r = &sea.sched[d][k];
            memset(r, 0, sizeof *r);
            r->a = m[pa[k % 3]];
            r->b = m[pb[k % 3]];
            r->track = DIV_TRACK[d][k / 3];
            r->winner = r->fast = -1;
        }
    }
}
static void season_new(const char *name)
{
    memset(&sea, 0, sizeof sea);
    sea.on = 1;
    sea.season = 1;
    snprintf(sea.pname, sizeof sea.pname, "%s", name[0] ? name : "Player");
    for (int i = 0; i < 11; i++) {
        sea.div[i] = i < 9 ? i / 3 : 3;
    }
    sea.div[PLAYER] = 3;
    season_schedule();
}
static int player_div(void)
{
    return sea.div[PLAYER];
}
static Race *player_race(void)
{
    if (sea.race >= 6) {
        return NULL;
    }
    Race *r = &sea.sched[player_div()][sea.race];
    return (r->a == PLAYER || r->b == PLAYER) && !r->done ? r : NULL;
}
static int player_races_done(void)
{
    int n = 0;
    for (int k = 0; k < 6; k++) {
        const Race *r = &sea.sched[player_div()][k];
        n += r->done && (r->a == PLAYER || r->b == PLAYER);
    }
    return n;
}

static void race_score(Race *r, int winner, float ta, float tb, float la, float lb)
{
    r->ta = ta;
    r->tb = tb;
    r->la = la;
    r->lb = lb;
    r->done = 1;
    r->winner = winner;
    r->fast = la > 0 && (lb <= 0 || la <= lb) ? r->a : lb > 0 ? r->b : -1;
    sea.raced[r->a]++;
    sea.raced[r->b]++;
    if (r->winner >= 0) {
        sea.pts[r->winner] += 2;
        sea.wins[r->winner]++;
    }
    if (r->fast >= 0) {
        sea.pts[r->fast] += 1;
        sea.lapp[r->fast]++;
    }
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
            if (sea_rand() < 0.12f * (1 - rt) + 0.03f) {
                lt += 12.f + 6.f * sea_rand();
            }
            l[k] = fminf(l[k], lt);
            tot += lt;
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
            if (r->done || r->a == PLAYER || r->b == PLAYER) {
                continue;
            }
            sim_race(r);
            if (d == player_div()) {
                last_sim[nlast_sim++] = sea.race;
            }
        }
        if (player_race()) {
            return;
        }
        sea.race++;
    }
}
static int rank_cmp(const Season *s, int a, int b)
{
    if (s->pts[a] != s->pts[b]) {
        return s->pts[b] - s->pts[a];
    }
    if (s->wins[a] != s->wins[b]) {
        return s->wins[b] - s->wins[a];
    }
    if (s->lapp[a] != s->lapp[b]) {
        return s->lapp[b] - s->lapp[a];
    }
    return a == PLAYER ? -1 : b == PLAYER ? 1 : a - b;
}
static void div_table(const Season *s, int d, int out[3])
{
    int n = 0;
    for (int i = 0; i < NDRV; i++) {
        if (s->div[i] == d && n < 3) {
            out[n++] = i;
        }
    }
    for (int i = 0; i < 3; i++) {
        for (int j = i + 1; j < 3; j++) {
            if (rank_cmp(s, out[j], out[i]) < 0) {
                int t = out[i];
                out[i] = out[j];
                out[j] = t;
            }
        }
    }
}
/* season over: the top driver of each division goes up, the bottom one down, then the next season's fixtures */
static void season_end(void)
{
    sea_end = sea;
    int tab[4][3];
    for (int d = 0; d < 4; d++) {
        div_table(&sea, d, tab[d]);
    }
    sea_moved = 0;
    for (int d = 0; d < 4; d++) {
        if (d > 0) {
            sea.div[tab[d][0]] = d - 1;
            if (tab[d][0] == PLAYER) {
                sea_moved = -1;
            }
        }
        if (d < 3) {
            sea.div[tab[d][2]] = d + 1;
            if (tab[d][2] == PLAYER) {
                sea_moved = 1;
            }
        }
    }
    if (tab[0][0] == PLAYER) {
        sea_moved = 2;
    }
    sea.season++;
    season_schedule();
}
