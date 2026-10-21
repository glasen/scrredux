/* editor/state.h - track builder state */
#pragma once

/* the track builder, in 3D in the stadium: fly over the arena and build on the original's 16x16 grid, marked
 * on the floor. The track grows from its open end with the original's pieces (only those that join on and keep clear of
 * the other pieces' cubes are offered: ghosts to click, or the arrow keys), C closes the loop the shortest way round.
 * Removing pieces from a closed loop opens it there: the pieces are renumbered so the gap is always after the last one.
 * Heights are set where two pieces meet (the selected piece's end, or every joint of a selected run); each piece has a
 * shape between its two ends. Every change can be undone. The stadium is built once round the whole grid
 * (scene_builder), the track as its own mesh (tkb) a moment after each change. Saves to tracks/NAME.trk, T test-drives,
 * G flies along the road.  */
enum { EM_EDIT, EM_LIST, EM_NAME, EM_NEW, EM_QUIT, EM_FLY };
static TrackDef ed;
static int ed_sel, ed_anchor, ed_dirty, ed_mode, ed_list, ed_endh[2], ed_help;   /* the selection: the run from ed_anchor to ed_sel */
static int ed_bad[MAXPC], ed_nbad;                     /* pieces before jumps the AI's planner can't get over */
static char ed_msg[96], ed_name[32];
static float ed_msg_t;
static int ed_gen = 1, ed_built = -1;                  /* bumped on every change; the ed_gen the track mesh is of */
static float ed_gen_t;                                 /* seconds since the last change */
static const unsigned char TPL_USE[7] = {0, 1, 3, 4, 6, 7, 10};
typedef struct { int n, turn; TPiece p[3]; } Move;

static void ed_note(const char *s) { snprintf(ed_msg, sizeof ed_msg, "%s", s); ed_msg_t = 3.f; }
static int ed_lo(void) { return ed_sel < ed_anchor ? ed_sel : ed_anchor; }
static int ed_hi(void) { return ed_sel > ed_anchor ? ed_sel : ed_anchor; }
static void ed_select(int p, int extend) { ed_sel = p; if (!extend) ed_anchor = p; }
