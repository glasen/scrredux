/* editor/building.h - extending, removing, closing and saving the track */
#pragma once

/* ---- building ---- */
static void ed_new(void)
{
    memset(&editor.track, 0, sizeof editor.track);
    snprintf(editor.track.name, sizeof editor.track.name, "NEW TRACK");
    editor.track.orig = -1;
    editor.track.np = 1;
    editor.track.p[0].cx = 7;
    editor.track.p[0].cz = 2;
    editor.track.p[0].at = 0;
    editor.track.p[0].shape = SH_SMOOTH;
    editor.end_heights[0] = editor.end_heights[1] = 0;
    editor.sel = editor.anchor = 0;
    editor.dirty = 0;
    editor.generation++;
}

static void ed_append(const Move *m, int closes)
{
    ed_push(0);

    for (int k = 0; k < m->n; k++) {
        TPiece p = m->p[k];
        p.shape = SH_SMOOTH;
        p.amp = 0;

        for (int e = 0; e < 2; e++) {
            for (int i = 0; i < 14; i++) {
                p.h[e][i] = editor.end_heights[e]; /* new pieces carry on at the open end's height */
            }
        }

        editor.track.p[editor.track.np++] = p;
    }

    if (closes) {
        regen(editor.track.np - 1);
    }

    ed_select(editor.track.np - 1, 0);
}

/* the move for this turn at the open end that fits, or NULL (*joins: there is one but no room, 2: too many pieces) */
static const Move *ed_move_for(int turn, Move *mv, int *closes, int *joins)
{
    int e[2][2];
    open_end(e);
    int n = moves(e, mv);

    if (joins) {
        *joins = 0;
    }

    for (int k = 0; k < n; k++) {
        if (mv[k].turn == turn) {
            if (move_ok(&mv[k], closes)) {
                return &mv[k];
            }

            if (joins) {
                *joins = editor.track.np + mv[k].n > MAXPC ? 2 : 1;
            }
        }
    }

    return NULL;
}

static void ed_extend(int turn)
{
    if (ed_closed()) {
        ed_note("The loop is closed - select pieces and press DEL to open it there");
        return;
    }

    Move mv[8];
    int closes, joins;
    const Move *m = ed_move_for(turn, mv, &closes, &joins);

    if (m) {
        ed_append(m, closes);

        if (closes) {
            ed_note("Loop closed! T to test drive, CTRL+S to save");
        }

        return;
    }

    const char *what = turn == 0 ? "straight" : abs(turn) == 45 ? "45 degree bend" : "90 degree bend";
    char b[96];

    if (joins) {
        snprintf(b, sizeof b, "No room for a %s: the grid ends or the track is in the way", what);
        ed_note(joins == 2 ? "100 pieces is the most a track can have" : b);
        return;
    }

    /* would it join on one straight further on? (the original's bends only start at some places in a cube) */
    const Move *s = ed_move_for(0, mv, &closes, NULL);
    int after = 0, n0 = editor.track.np;

    if (s && !closes) {
        for (int k = 0; k < s->n; k++) {
            editor.track.p[editor.track.np++] = s->p[k];
        }

        after = ed_move_for(turn, mv, &closes, &joins) != NULL || joins;
        editor.track.np = n0;
    }

    snprintf(b, sizeof b, "No %s joins on here%s", what,
             after             ? " - add a straight (UP) first"
             : abs(turn) == 90 ? ": 90 degree bends start along the grid"
                               : " - the ghosts show what does");
    ed_note(b);
}

/* the same loop numbered from piece k */
static void ed_renumber(int k)
{
    TPiece t[MAXPC];

    for (int i = 0; i < editor.track.np; i++) {
        t[i] = editor.track.p[(i + k) % editor.track.np];
    }

    memcpy(editor.track.p, t, sizeof(TPiece) * editor.track.np);
    editor.track.start = (editor.track.start - k + editor.track.np) % editor.track.np;
    editor.sel = (editor.sel - k + editor.track.np) % editor.track.np;
    editor.anchor = (editor.anchor - k + editor.track.np) % editor.track.np;
}

/* the selected pieces come off: a closed loop opens there, an open track loses pieces at either end */
static void ed_remove(void)
{
    int a = ed_lo(), b = ed_hi(), n = b - a + 1, cl = ed_closed();

    if (n >= editor.track.np) {
        ed_note(editor.track.np == 1 ? "The last piece stays - ALT+arrows move it, ALT+R turns it"
                                     : "One piece has to stay");
        return;
    }

    if (!cl && a != 0 && b != editor.track.np - 1) {
        ed_note("The loop is open: pieces only come off at its ends (C closes it)");
        return;
    }

    ed_push(0);

    if (cl) {
        ed_renumber(b + 1);
        a = editor.track.np - n;
        b = editor.track.np - 1;
    } /* the selection last: the gap opens where it was */
    int gone = editor.track.start >= a && editor.track.start <= b;

    if (b == editor.track.np - 1) {
        editor.end_heights[0] = editor.track.p[a].h[0][0];
        editor.end_heights[1] = editor.track.p[a].h[1][0];
        editor.track.np -= n;
        ed_select(editor.track.np - 1, 0);
    } else { /* the first pieces of an open track */
        memmove(editor.track.p, editor.track.p + n, sizeof(TPiece) * (editor.track.np - n));
        editor.track.np -= n;
        ed_select(0, 0);

        if (!gone) {
            editor.track.start -= n;
        }
    }

    if (gone) { /* the start line goes to the next straight */
        editor.track.start = 0;

        for (int k = 0; k < editor.track.np; k++) {
            if ((editor.track.p[k].at & 15) == 0) {
                editor.track.start = k;
                break;
            }
        }
    }
}

/* the whole track a cube over (dx, dz) and q quarter turns clockwise (seen from above) about the grid's middle */
static void ed_move(int dx, int dz, int q)
{
    TrackDef t = editor.track;

    for (int p = 0; p < t.np; p++) {
        TPiece *pc = &t.p[p];
        int x = pc->cx + dx, z = pc->cz + dz;

        for (int r = 0; r < q; r++) {
            int x0 = x;
            x = z;
            z = 15 - x0;
        }

        if (x < 0 || x > 15 || z < 0 || z > 15) {
            ed_note("The track would leave the grid");
            return;
        }

        pc->cx = (unsigned char)x;
        pc->cz = (unsigned char)z;
        pc->at = (unsigned char)((pc->at & 0x3f) | ((pc->at + (q << 6)) & 0xc0));

        if (!pc_ongrid(pc)) {
            ed_note("The track would leave the grid");
            return;
        }
    }

    ed_push(0);
    editor.track = t;
}

/* the shortest way back to the first piece: breadth first over single pieces, clear of the track */
/* a node of the search back to the start: where the road ends after this piece, the node before it, how many pieces */
typedef struct {
    int e[2][2], prev, depth;
    TPiece p;
} CloseNode;

/* does any piece of the path run into one that is not next to it? */
static int path_clashes(const TPiece *path, int count)
{
    for (int a = 0; a < count; a++) {
        for (int b = a + 2; b < count; b++) {
            if (pc_clash(&path[a], &path[b])) {
                return 1;
            }
        }
    }

    return 0;
}

/* appends the pieces of a path that closes the loop, their heights easing from the open end's to the start piece's */
static void ed_append_path(const TPiece *path, int count)
{
    ed_push(0);
    int h0[2] = {editor.end_heights[0], editor.end_heights[1]}, n0 = editor.track.np;

    for (int a = 0; a < count; a++) {
        TPiece p = path[a];
        p.shape = SH_SMOOTH;
        p.amp = 0;

        for (int e = 0; e < 2; e++) {
            int h = h0[e] + (editor.track.p[0].h[e][0] - h0[e]) * a / count;

            for (int i = 0; i < 14; i++) {
                p.h[e][i] = h;
            }
        }

        editor.track.p[editor.track.np++] = p;
    }

    for (int a = n0; a < editor.track.np; a++) {
        regen(a);
    }

    ed_select(editor.track.np - 1, 0);
    char b[64];
    snprintf(b, sizeof b, "Loop closed with %d pieces", count);
    ed_note(b);
}

/* closes the loop with the fewest pieces: a breadth-first search over the pieces that fit on from the open end, until
 * one meets the start piece. Returns 1 when it closed the loop */
static int ed_autoclose(void)
{
    if (ed_closed()) {
        ed_note("The loop is already closed");
        return 0;
    }

    enum { CAP = 40000 };

    static CloseNode q[CAP];
    static unsigned char vis[256 * 256 * 8]; /* end positions (128 units) and directions (45 degrees) tried */
    memset(vis, 0, sizeof vis);
    int s0[2][2], qt = 1;
    pc_xs(&editor.track.p[0], 0, s0);
    open_end(q[0].e);
    q[0].prev = -1;
    q[0].depth = 0;

    for (int qh = 0; qh < qt; qh++) {
        TPiece c[64];
        int n = cands(q[qh].e, c);

        for (int k = 0; k < n; k++) {
            int x[2][2];
            pc_endxs(&c[k], x);
            int closes = xs_eq(x, s0);

            if (!fits(&c[k], q[qh].depth, closes)) {
                continue;
            }

            if (closes) {
                TPiece path[MAXPC];
                int L = q[qh].depth + 1;

                if (editor.track.np + L > MAXPC) {
                    continue;
                }

                path[L - 1] = c[k];

                for (int i = qh, d = L - 2; d >= 0; i = q[i].prev, d--) {
                    path[d] = q[i].p;
                }

                if (path_clashes(path, L)) {
                    continue;
                }

                ed_append_path(path, L);
                return 1;
            }

            float mx = (x[0][0] + x[1][0]) * 0.5f, mz = (x[0][1] + x[1][1]) * 0.5f;
            int oct = ((int)lroundf(xs_ang(x) * 4 / PI_F) + 8) & 7;
            int kx = (int)clampf(mx / 128.f, 0, 255), kz = (int)clampf(mz / 128.f, 0, 255),
                key = (kz * 256 + kx) * 8 + oct;

            if (vis[key] || qt >= CAP || q[qh].depth + 2 + editor.track.np > MAXPC) {
                continue;
            }

            vis[key] = 1;
            memcpy(q[qt].e, x, sizeof x);
            q[qt].prev = qh;
            q[qt].depth = q[qh].depth + 1;
            q[qt].p = c[k];
            qt++;
        }
    }

    ed_note("No way back to the start from here - remove a few pieces (DEL) and try again");
    return 0;
}

/* ready to drive: a closed loop with a straight for the start line */
static int ed_ready(void)
{
    if (!ed_closed()) {
        ed_note("Close the loop first (C does it for you)");
        return 0;
    }

    if (editor.track.np < 4) {
        ed_note("A track needs at least 4 pieces");
        return 0;
    }

    if ((editor.track.p[editor.track.start].at & 15) != 0) {
        int k = 0;

        while (k < editor.track.np && (editor.track.p[k].at & 15) != 0) {
            k++;
        }

        if (k == editor.track.np) {
            ed_note("The start line needs a straight piece");
            return 0;
        }

        editor.track.start = k;
    }

    return 1;
}

/* the AI's jump planner on the track as it stands: the pieces whose jumps can't be cleared */
static void ed_jumps_note(void)
{
    editor.unreachable_count = 0;

    for (int k = 0; k < nbadj && editor.unreachable_count < MAXPC; k++) {
        editor.unreachable[editor.unreachable_count++] = seg_sec[badj[k]];
    }

    char b[96];

    if (!editor.unreachable_count) {
        ed_note("Every jump can be made");
        return;
    }

    snprintf(b, sizeof b, "The jump after piece %d cannot be cleared - lower it or give it more run-up",
             editor.unreachable[0] + 1);
    ed_note(b);
    editor.message_time = 8.f;
}

static void ed_check_jumps(void)
{
    if (!ed_ready()) {
        return;
    }

    float keep[4] = {ar_x0, ar_x1, ar_z0, ar_z1};
    int tg = track_gen;
    trk[ED_SLOT] = editor.track;
    trk[ED_SLOT].orig = -1;
    build_track_geometry(ED_SLOT);
    cur_track = -1;
    plan_need();
    ar_x0 = keep[0];
    ar_x1 = keep[1];
    ar_z0 = keep[2];
    ar_z1 = keep[3];
    track_gen = tg; /* the builder's stadium stays */
    ed_jumps_note();
    editor.built_generation = -1; /* the mesh again (an open track has its gap) */
}

static void ed_test(void)
{
    if (!ed_ready()) {
        return;
    }

    trk[ED_SLOT] = editor.track;
    trk[ED_SLOT].orig = -1;
    build_track(ED_SLOT);
    race_mode = RM_TEST;
    cockpit = 1;
    start_race();
}

static void ed_save(void)
{
    if (!ed_ready()) {
        return;
    }

    char keep[32];
    snprintf(keep, sizeof keep, "%s", sel_track < ntrk ? trk[sel_track].name : "");

    if (track_save(&editor.track)) {
        ed_note("Could not write the track file");
        return;
    }

    tracks_scan();
    int k = track_find(keep);
    sel_track = k >= 0 ? k : 0;
    cur_track = -1;
    char b[96];
    snprintf(b, sizeof b, "Saved as tracks/%s", editor.track.file);
    ed_note(b);
    editor.dirty = 0;
}

/* an original comes in with its jump edits baked into the heights */
static void ed_import(int i)
{
    ed_push(0);
    editor.track = trk[i];

    if (editor.track.orig >= 0) {
        track_xsecs(&editor.track);

        for (int q = 0, n = 0; q < editor.track.np; q++) {
            TPiece *pc = &editor.track.p[(editor.track.start + 1 + q) % editor.track.np];

            for (int r = 0; r < pc_ns(pc); r++, n++) {
                for (int e = 0; e < 2; e++) {
                    pc->h[e][r] = tx_ey[n][e];
                }
            }
        }

        char nm[40];
        snprintf(nm, sizeof nm, "MY %s", editor.track.name);
        SDL_strlcpy(editor.track.name, nm, sizeof editor.track.name);
        editor.track.orig = -1;
        editor.track.file[0] = 0;
    }

    editor.end_heights[0] = editor.track.p[0].h[0][0];
    editor.end_heights[1] = editor.track.p[0].h[1][0];
    ed_select(0, 0);
    editor.dirty = editor.track.file[0] == 0;
}
