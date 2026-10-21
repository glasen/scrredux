/* editor/undo.h - undo and redo */
#pragma once

/* ---- undo: the whole track before each change. A run of the same tweak (kind > 0) on one selection is one step ---- */
enum { UNDO_N = 64, TW_HEIGHT = 1, TW_BANK, TW_SIZE };
typedef struct {
    TrackDef t;
    int endh[2], sel, anchor;
} EdSnap;
static EdSnap ed_un[UNDO_N], ed_re[UNDO_N]; /* ed_un a ring (oldest dropped), ed_re a stack */
static int ed_un_n, ed_un_top, ed_re_n, ed_tweak = -1, ed_tweak_sel;
static void snap_take(EdSnap *s)
{
    s->t = ed;
    memcpy(s->endh, ed_endh, sizeof s->endh);
    s->sel = ed_sel;
    s->anchor = ed_anchor;
}
static void snap_put(const EdSnap *s)
{
    ed = s->t;
    memcpy(ed_endh, s->endh, sizeof ed_endh);
    ed_sel = s->sel;
    ed_anchor = s->anchor;
    ed_nbad = 0;
    ed_dirty = 1;
    ed_tweak = -1;
    ed_gen++;
    ed_gen_t = 0;
}
static void ed_push(int kind)
{
    ed_nbad = 0;
    ed_dirty = 1;
    ed_gen++;
    ed_gen_t = 0;
    if (kind > 0 && kind == ed_tweak && ed_sel * 128 + ed_anchor == ed_tweak_sel) {
        return;
    }
    ed_tweak = kind;
    ed_tweak_sel = ed_sel * 128 + ed_anchor;
    snap_take(&ed_un[ed_un_top]);
    ed_un_top = (ed_un_top + 1) % UNDO_N;
    if (ed_un_n < UNDO_N) {
        ed_un_n++;
    }
    ed_re_n = 0;
}
static void ed_undo(int redo)
{
    if (redo ? !ed_re_n : !ed_un_n) {
        ed_note(redo ? "Nothing to redo" : "Nothing to undo");
        return;
    }
    if (redo) {
        snap_take(&ed_un[ed_un_top]);
        ed_un_top = (ed_un_top + 1) % UNDO_N;
        ed_un_n++;
        snap_put(&ed_re[--ed_re_n]);
    } else {
        snap_take(&ed_re[ed_re_n++]);
        ed_un_top = (ed_un_top + UNDO_N - 1) % UNDO_N;
        ed_un_n--;
        snap_put(&ed_un[ed_un_top]);
    }
}
