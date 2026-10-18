/* trackdef.h - tracks as pieces on the original's 16x16 grid: the eight originals (decoded from scr_tracks.h) and the
 * player's own from the track builder (tracks/NAME.trk). Included by scrredux.c */
#define MAXPC 100                                      /* pieces per track, like the original's road tables */
#define MAXTRK 48
#define ED_SLOT (MAXTRK - 1)                           /* the track builder's test drive */
#define U_PER_M (4.f / SCR_U)                          /* height units (1/4 of an x/z unit) per metre */
enum { SH_RAW, SH_SMOOTH, SH_LINEAR, SH_HUMP, SH_KICKER, SH_STEP, SH_PIT, NSHAPE };
static const char *const SHAPE_NAME[NSHAPE] = {"Original", "Smooth", "Linear", "Hump", "Kicker", "Step", "Pit"};
/* a piece: its cube, the template and its rotation (the original's angle.and.piece byte), and per template edge the
 * height of each of its cross-sections (the last one is the next piece's first). shape/amp: how the builder made them */
typedef struct { unsigned char cx, cz, at, shape; float amp; int h[2][14]; } TPiece;
typedef struct { char name[32], file[64]; int np, start, orig; TPiece p[MAXPC]; } TrackDef;
static TrackDef trk[MAXTRK];
static int ntrk;                                       /* originals first, then the player's tracks */
static int track_gen;                                  /* bumped on every build: the static mesh follows it */

static void data_path(char *out, size_t n, const char *rel)
{
    const char *b = SDL_GetBasePath();
    snprintf(out, n, "%s%s", b ? b : "", rel);
}

/* cross-section i of a piece in the original's x/z units (not mirrored), edge 0 and edge 1 */
static void tpl_cross(int at, int i, int cx, int cz, int out[2][2])
{
    int t = at & 15, ns = SCR_TPL_NS[t], ang = at & 0xc0;
    const short *r = SCR_TPL_XZ[t][(at & 0x10) ? ns - i : i];
    for (int e = 0; e < 2; e++) {
        int x = (at & 0x10) ? r[2 - 2 * e] : r[2 * e], z = (at & 0x10) ? r[3 - 2 * e] : r[2 * e + 1], rx = x, rz = z;
        if (ang == 0x40) { rx = z; rz = 0x800 - x; }
        else if (ang == 0x80) { rx = 0x800 - x; rz = 0x800 - z; }
        else if (ang == 0xc0) { rx = 0x800 - z; rz = x; }
        out[e][0] = cx * 2048 + rx; out[e][1] = cz * 2048 + rz;
    }
}
static int pc_ns(const TPiece *p) { return SCR_TPL_NS[p->at & 15]; }

static void decode_original(int idx, TrackDef *td)
{
    const unsigned char *b = SCR_BIN[idx];
    memset(td, 0, sizeof *td);
    snprintf(td->name, sizeof td->name, "%s", SCR_TITLE[idx]);
    td->np = b[0]; td->start = b[1]; td->orig = idx;
    for (int p = 0; p < td->np; p++) {
        TPiece *pc = &td->p[p];
        pc->cx = b[2 + p] & 15; pc->cz = b[2 + p] >> 4; pc->at = b[102 + p]; pc->shape = SH_RAW;
        int sh[2] = {(short)(b[402 + 2 * p] << 8 | b[403 + 2 * p]), (short)(b[602 + 2 * p] << 8 | b[603 + 2 * p])};
        int id[2] = {b[202 + p] & 0x7f, b[302 + p] & 0x7f};
        for (int i = 0; i < pc_ns(pc); i++) for (int e = 0; e < 2; e++) pc->h[e][i] = SCR_PROF[id[e]][i] + sh[e];
    }
}

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
    data_path(path, sizeof path, "tracks"); SDL_CreateDirectory(path);
    data_path(path, sizeof path, "tracks/"); snprintf(path + strlen(path), sizeof path - strlen(path), "%s", fn);
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
        data_path(path, sizeof path, "tracks/"); snprintf(path + strlen(path), sizeof path - strlen(path), "%s", td->file);
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
        if (!strncmp(line, "NAME ", 5)) snprintf(td->name, sizeof td->name, "%s", line + 5);
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
    data_path(dir, sizeof dir, "tracks");
    char **files = SDL_GlobDirectory(dir, "*.trk", 0, &n);
    for (int i = 0; files && i < n && ntrk < ED_SLOT; i++) {
        char path[1024]; snprintf(path, sizeof path, "%s/%s", dir, files[i]);
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
