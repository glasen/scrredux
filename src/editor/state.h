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

enum { UNDO_N = 64, TW_HEIGHT = 1, TW_BANK, TW_SIZE };

typedef struct {
    TrackDef t;
    int endh[2], sel, anchor;
} EdSnap;

/* what is under the mouse: kind 0 nothing, 1 a piece (p), 2 a ghost (turn) */
typedef struct {
    int kind, p, turn;
} Pick;

/* the track builder: everything it keeps between frames */
static struct {
    TrackDef track;     /* the track being built */
    int mode;           /* EM_* */
    int sel, anchor;    /* the selection: the run of pieces from anchor to sel */
    int dirty;          /* unsaved changes */
    int list_cursor;    /* the track list's current line */
    int end_heights[2]; /* heights of the open end */
    int show_help;
    int unreachable[MAXPC], unreachable_count; /* pieces before jumps the AI's planner can't get over */
    char message[96], name_input[32];
    float message_time;
    int generation;         /* bumped on every change */
    int built_generation;   /* the generation the track mesh is of */
    float since_change;     /* seconds since the last change */
    float mouse_x, mouse_y; /* HUD coordinates */
    Pick hover;

    /* undo: a ring of the last snapshots (oldest dropped) and a redo stack */
    EdSnap undo_ring[UNDO_N], redo_stack[UNDO_N];
    int undo_count, undo_top, redo_count;
    int tweak_kind, tweak_sel; /* the current run of the same height/bank tweak is one undo step */

    /* the flying camera */
    struct {
        V3 pos;
        float yaw, pitch;
        int looking; /* the right mouse button or Space held */
        int space;
    } cam;

    /* fly-along: where on the road, how fast */
    struct {
        float position, speed, saved_speed;
        V3 forward, normal;
        int snap;
    } fly;
} editor = {
    .mode = 0,
    .generation = 1,
    .built_generation = -1,
    .hover = {0, -1, 0},
    .mouse_x = -1,
    .mouse_y = -1,
    .tweak_kind = -1,
    .cam = {.pos = {0, 140, -560}, .pitch = -0.32f},
};

static const unsigned char TPL_USE[7] = {0, 1, 3, 4, 6, 7, 10};

typedef struct {
    int n, turn;
    TPiece p[3];
} Move;

static void ed_note(const char *s)
{
    snprintf(editor.message, sizeof editor.message, "%s", s);
    editor.message_time = 3.f;
}

static int ed_lo(void)
{
    return SDL_min(editor.sel, editor.anchor);
}

static int ed_hi(void)
{
    return SDL_max(editor.sel, editor.anchor);
}

static void ed_select(int p, int extend)
{
    editor.sel = p;

    if (!extend) {
        editor.anchor = p;
    }
}
