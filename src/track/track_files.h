/* track/track_files.h - the player's track files */
#pragma once

/* ---- the player's tracks: plain text, one line per piece ---- */
static void track_filename(const char *name, char *out, size_t n)
{
    size_t k = 0;
    for (const char *s = name; *s && k + 5 < n; s++) out[k++] = isalnum((unsigned char)*s) ? (char)tolower((unsigned char)*s) : '_';
    if (!k) out[k++] = 't';
    snprintf(out + k, n - k, ".trk");
}
static int track_save(TrackDef *td)
{
    char fn[64], path[1024];
    track_filename(td->name, fn, sizeof fn);
    user_path(path, sizeof path, "tracks"); SDL_CreateDirectory(path);
    user_path(path, sizeof path, "tracks/"); snprintf(path + strlen(path), sizeof path - strlen(path), "%s", fn);
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    fprintf(f, "SCRREDUX-TRACK 1\nNAME %s\nSTART %d\n", td->name, td->start);
    for (int p = 0; p < td->np; p++) {
        const TPiece *pc = &td->p[p];
        fprintf(f, "PIECE %d %d %d %d %.2f", pc->cx, pc->cz, pc->at, pc->shape, pc->amp);
        for (int e = 0; e < 2; e++) for (int i = 0; i < pc_ns(pc); i++) fprintf(f, " %d", pc->h[e][i]);
        fputc('\n', f);
    }
    fclose(f);
    if (td->file[0] && strcmp(td->file, fn)) {                   /* renamed: the old file goes */
        user_path(path, sizeof path, "tracks/"); snprintf(path + strlen(path), sizeof path - strlen(path), "%s", td->file);
        remove(path);
    }
    snprintf(td->file, sizeof td->file, "%s", fn);
    return 0;
}
static int track_load(const char *path, TrackDef *td)
{
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[2048];
    memset(td, 0, sizeof *td); td->orig = -1;
    int ok = fgets(line, sizeof line, f) && (!strncmp(line, "SCRREDUX-TRACK", 14) || !strncmp(line, "STUNTRACER-TRACK", 16));   /* also the old name */
    while (ok && fgets(line, sizeof line, f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!strncmp(line, "NAME ", 5)) SDL_strlcpy(td->name, line + 5, sizeof td->name);
        else if (!strncmp(line, "START ", 6)) td->start = atoi(line + 6);
        else if (!strncmp(line, "PIECE ", 6) && td->np < MAXPC) {
            TPiece *pc = &td->p[td->np];
            int cx, cz, at, shp, nn; float amp;
            char *s = line + 6;
            if (sscanf(s, "%d %d %d %d %f%n", &cx, &cz, &at, &shp, &amp, &nn) != 5) { ok = 0; break; }
            pc->cx = (unsigned char)(cx & 15); pc->cz = (unsigned char)(cz & 15); pc->at = (unsigned char)at;
            pc->shape = (unsigned char)(shp >= 0 && shp < NSHAPE ? shp : SH_RAW); pc->amp = amp;
            if (!SCR_TPL_NS[pc->at & 15]) { ok = 0; break; }
            s += nn;
            for (int e = 0; e < 2; e++) for (int i = 0; i < pc_ns(pc); i++) { pc->h[e][i] = (int)strtol(s, &s, 10); }
            td->np++;
        }
    }
    fclose(f);
    if (!ok || td->np < 4 || td->start < 0 || td->start >= td->np) return -1;
    if (!td->name[0]) snprintf(td->name, sizeof td->name, "UNNAMED");
    return 0;
}
static int trk_cmp(const void *a, const void *b) { return strcmp(((const TrackDef *)a)->name, ((const TrackDef *)b)->name); }
/* the originals, then every tracks/NAME.trk sorted by name */
static void tracks_scan(void)
{
    ntrk = 0;
    for (int i = 0; i < SCR_NTRACKS; i++) decode_original(i, &trk[ntrk++]);
    char dir[1024]; int n = 0;
    user_path(dir, sizeof dir, "tracks");
    char **files = SDL_GlobDirectory(dir, "*.trk", 0, &n);
    for (int i = 0; files && i < n && ntrk < ED_SLOT; i++) {
        char path[1024];
        if (snprintf(path, sizeof path, "%s/%s", dir, files[i]) >= (int)sizeof path) { fprintf(stderr, "track %s: path too long\n", files[i]); continue; }
        if (track_load(path, &trk[ntrk]) == 0) { snprintf(trk[ntrk].file, sizeof trk[ntrk].file, "%s", files[i]); ntrk++; }
        else fprintf(stderr, "track %s: cannot read\n", files[i]);
    }
    SDL_free(files);
    qsort(trk + SCR_NTRACKS, ntrk - SCR_NTRACKS, sizeof(TrackDef), trk_cmp);
}
static int track_find(const char *name)
{
    for (int i = 0; i < ntrk; i++) if (!strcmp(trk[i].name, name)) return i;
    return -1;
}
