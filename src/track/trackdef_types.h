/* track/trackdef_types.h - track definitions: pieces on the 16 x 16 grid */
#pragma once

/* tracks as pieces on the original's 16x16 grid: the eight originals (decoded from track/data/scr_tracks.h) and the
 * player's own from the track builder (tracks/NAME.trk in the player's folder, see user_path).  */
enum { SH_RAW, SH_SMOOTH, SH_LINEAR, SH_HUMP, SH_KICKER, SH_STEP, SH_PIT, NSHAPE };

static const char *const SHAPE_NAME[NSHAPE] = {"Original", "Smooth", "Linear", "Hump", "Kicker", "Step", "Pit"};

/* a piece: its cube, the template and its rotation (the original's angle.and.piece byte), and per template edge the
 * height of each of its cross-sections (the last one is the next piece's first). shape/amp: how the builder made them */
typedef struct {
    unsigned char cx, cz, at, shape;
    float amp;
    int h[2][14];
} TPiece;

typedef struct {
    char name[32], file[64];
    int np, start, orig;
    TPiece p[MAXPC];
} TrackDef;

static TrackDef trk[MAXTRK];
static int ntrk;      /* originals first, then the player's tracks */
static int track_gen; /* bumped on every build: the static mesh follows it */
