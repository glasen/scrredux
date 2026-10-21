/* league/saves.h - season saves */
#pragma once

/* ---- saves, in the player's folder (user_path): season0.txt is the running season, 1..4 the Load/Save slots ---- */
static void save_path(char *out, size_t n, const char *fmt, int k)
{
    char rel[64];
    snprintf(rel, sizeof rel, fmt, k);
    user_path(out, n, rel);
}

/* plain text, a line per item, so a later version can add to it and still read this one: lines it does not know are
 * skipped, and a save missing a driver or a race is no save */
static int season_write(int slot)
{
    char p[1024], tmp[1024];
    save_path(p, sizeof p, "season%d.txt", slot);
    save_path(tmp, sizeof tmp, "season%d.tmp", slot);
    FILE *f = fopen(tmp, "w");
    if (!f) {
        return -1;
    }
    fprintf(f, "%s\nname %s\nseason %d\nrace %d\nholes %d\n", SEA_HEAD, sea.pname, sea.season, sea.race, sea.nholes);
    for (int i = 0; i < NDRV; i++) {
        fprintf(f, "driver %d %d %d %d %d %d\n", i, sea.div[i], sea.pts[i], sea.raced[i], sea.wins[i], sea.lapp[i]);
    }
    for (int d = 0; d < 4; d++) {
        for (int k = 0; k < 6; k++) {
            const Race *r = &sea.sched[d][k];
            fprintf(f, "fixture %d %d %d %d %d %d %d %d %.9g %.9g %.9g %.9g\n", d, k, r->a, r->b, r->track, r->done,
                    r->winner, r->fast, r->ta, r->tb, r->la, r->lb);
        }
    }
    int ok = !ferror(f);
    if (fclose(f) || !ok || !SDL_RenamePath(tmp, p)) {
        remove(tmp);
        return -1;
    } /* the old save stays until the new one is whole */
    return 0;
}

static int in_rng(int v, int lo, int hi)
{
    return v >= lo && v <= hi;
}

static int season_peek(int slot, Season *s)
{
    char p[1024], line[256];
    save_path(p, sizeof p, "season%d.txt", slot);
    FILE *f = fopen(p, "r");
    if (!f) {
        return -1;
    }
    memset(s, 0, sizeof *s);
    int ok = fgets(line, sizeof line, f) && !strncmp(line, SEA_HEAD, strlen(SEA_HEAD)), drv = 0, fix = 0;
    while (ok && fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        int i, d, k, v[6];
        Race r;
        if (!strncmp(line, "name ", 5)) {
            SDL_strlcpy(s->pname, line + 5, sizeof s->pname);
        } else if (sscanf(line, "season %d", &v[0]) == 1) {
            s->season = v[0];
        } else if (sscanf(line, "race %d", &v[0]) == 1) {
            s->race = v[0];
        } else if (sscanf(line, "holes %d", &v[0]) == 1) {
            s->nholes = v[0];
        } else if (sscanf(line, "driver %d %d %d %d %d %d", &i, &v[0], &v[1], &v[2], &v[3], &v[4]) == 6) {
            if (!in_rng(i, 0, NDRV - 1) || !in_rng(v[0], 0, 3)) {
                ok = 0;
                break;
            }
            s->div[i] = v[0];
            s->pts[i] = v[1];
            s->raced[i] = v[2];
            s->wins[i] = v[3];
            s->lapp[i] = v[4];
            drv |= 1 << i;
        } else if (sscanf(line, "fixture %d %d %d %d %d %d %d %d %f %f %f %f", &d, &k, &r.a, &r.b, &r.track, &r.done,
                          &r.winner, &r.fast, &r.ta, &r.tb, &r.la, &r.lb) == 12) {
            if (!in_rng(d, 0, 3) || !in_rng(k, 0, 5) || !in_rng(r.a, 0, NDRV - 1) || !in_rng(r.b, 0, NDRV - 1) ||
                !in_rng(r.track, 0, SCR_NTRACKS - 1) || !in_rng(r.winner, -1, NDRV - 1) ||
                !in_rng(r.fast, -1, NDRV - 1)) {
                ok = 0;
                break;
            }
            s->sched[d][k] = r;
            fix |= 1 << (d * 6 + k);
        }
    }
    fclose(f);
    if (!ok || drv != (1 << NDRV) - 1 || fix != (1 << 24) - 1 || s->season < 1 || !in_rng(s->race, 0, 6)) {
        return -1;
    }
    if (!s->pname[0]) {
        snprintf(s->pname, sizeof s->pname, "Player");
    }
    s->nholes = s->nholes < 0 ? 0 : s->nholes > NHOLE ? NHOLE : s->nholes;
    s->on = 1;
    return 0;
}

static int season_read(int slot)
{
    Season s;
    if (season_peek(slot, &s)) {
        return -1;
    }
    sea = s;
    return 0;
}
