/* editor/undo.h - undo and redo */
#pragma once

/* ---- undo: the whole track before each change. A run of the same tweak (kind > 0) on one selection is one step ---- */

static void snap_take(EdSnap *s)
{
    s->t = editor.track;
    memcpy(s->endh, editor.end_heights, sizeof s->endh);
    s->sel = editor.sel;
    s->anchor = editor.anchor;
}

static void snap_put(const EdSnap *s)
{
    editor.track = s->t;
    memcpy(editor.end_heights, s->endh, sizeof editor.end_heights);
    editor.sel = s->sel;
    editor.anchor = s->anchor;
    editor.unreachable_count = 0;
    editor.dirty = 1;
    editor.tweak_kind = -1;
    editor.generation++;
    editor.since_change = 0;
}

static void ed_push(int kind)
{
    editor.unreachable_count = 0;
    editor.dirty = 1;
    editor.generation++;
    editor.since_change = 0;

    if (kind > 0 && kind == editor.tweak_kind && editor.sel * 128 + editor.anchor == editor.tweak_sel) {
        return;
    }

    editor.tweak_kind = kind;
    editor.tweak_sel = editor.sel * 128 + editor.anchor;
    snap_take(&editor.undo_ring[editor.undo_top]);
    editor.undo_top = (editor.undo_top + 1) % UNDO_N;

    if (editor.undo_count < UNDO_N) {
        editor.undo_count++;
    }

    editor.redo_count = 0;
}

static void ed_undo(int redo)
{
    if (redo ? !editor.redo_count : !editor.undo_count) {
        ed_note(redo ? "Nothing to redo" : "Nothing to undo");
        return;
    }

    if (redo) {
        snap_take(&editor.undo_ring[editor.undo_top]);
        editor.undo_top = (editor.undo_top + 1) % UNDO_N;
        editor.undo_count++;
        snap_put(&editor.redo_stack[--editor.redo_count]);
    } else {
        snap_take(&editor.redo_stack[editor.redo_count++]);
        editor.undo_top = (editor.undo_top + UNDO_N - 1) % UNDO_N;
        editor.undo_count--;
        snap_put(&editor.undo_ring[editor.undo_top]);
    }
}
