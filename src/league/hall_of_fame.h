/* league/hall_of_fame.h - best lap and race time on every track */
#pragma once

/* ---- Hall of Fame: the best lap and the best race time on every track, and who drove them ---- */
typedef struct {
    char track[32], lapby[16], raceby[16];
    float lap, race;
} Hof;

static Hof hof[MAXTRK + SCR_NTRACKS];
static int nhof;

static void hof_save(void)
{
    char p[1024];
    save_path(p, sizeof p, "halloffame.txt", 0);
    FILE *f = fopen(p, "w");
    if (!f) {
        return;
    }
    for (int i = 0; i < nhof; i++) {
        fprintf(f, "%s|%s|%.2f|%s|%.2f\n", hof[i].track, hof[i].lapby, hof[i].lap, hof[i].raceby, hof[i].race);
    }
    fclose(f);
}

static Hof *hof_get(const char *track)
{
    for (int i = 0; i < nhof; i++) {
        if (!strcmp(hof[i].track, track)) {
            return &hof[i];
        }
    }
    if (nhof >= (int)(sizeof hof / sizeof hof[0])) {
        return NULL;
    }
    Hof *h = &hof[nhof++];
    memset(h, 0, sizeof *h);
    snprintf(h->track, sizeof h->track, "%s", track);
    for (int t = 0; t < SCR_NTRACKS; t++) {
        if (!strcmp(SCR_TITLE[t], track)) { /* the originals start with records to beat */
            snprintf(h->lapby, sizeof h->lapby, "%s", DRV_NAME[(t * 3) % 11]);
            h->lap = REF_LAP[t] * 1.05f;
            snprintf(h->raceby, sizeof h->raceby, "%s", DRV_NAME[(t * 3 + 4) % 11]);
            h->race = REF_LAP[t] * 3 * 1.08f + 6.f;
        }
    }
    return h;
}

static void hof_load(void)
{
    nhof = 0;
    for (int t = 0; t < SCR_NTRACKS; t++) {
        hof_get(SCR_TITLE[t]);
    }
    char p[1024], line[256];
    save_path(p, sizeof p, "halloffame.txt", 0);
    FILE *f = fopen(p, "r");
    if (!f) {
        return;
    }
    while (fgets(line, sizeof line, f)) {
        char *fld[5];
        int n = 0;
        line[strcspn(line, "\r\n")] = 0;
        for (char *s = line; n < 5;) {
            fld[n++] = s;
            s = strchr(s, '|');
            if (!s) {
                break;
            }
            *s++ = 0;
        }
        if (n < 5) {
            continue;
        }
        Hof *h = hof_get(fld[0]);
        if (!h) {
            break;
        }
        snprintf(h->lapby, sizeof h->lapby, "%s", fld[1]);
        h->lap = (float)atof(fld[2]);
        snprintf(h->raceby, sizeof h->raceby, "%s", fld[3]);
        h->race = (float)atof(fld[4]);
    }
    fclose(f);
}

static int hof_lap(const char *track, const char *who, float t)
{
    Hof *h = hof_get(track);
    if (!h || t <= 0 || (h->lap > 0 && t >= h->lap)) {
        return 0;
    }
    snprintf(h->lapby, sizeof h->lapby, "%s", who);
    h->lap = t;
    hof_save();
    return 1;
}

static int hof_race(const char *track, const char *who, float t)
{
    Hof *h = hof_get(track);
    if (!h || t <= 0 || (h->race > 0 && t >= h->race)) {
        return 0;
    }
    snprintf(h->raceby, sizeof h->raceby, "%s", who);
    h->race = t;
    hof_save();
    return 1;
}
