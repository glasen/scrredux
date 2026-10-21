/* editor/building.h - extending, removing, closing and saving the track */
#pragma once

/* ---- building ---- */
static void ed_new(void)
{
    memset(&ed, 0, sizeof ed);
    snprintf(ed.name, sizeof ed.name, "NEW TRACK"); ed.orig = -1; ed.np = 1;
    ed.p[0].cx = 7; ed.p[0].cz = 2; ed.p[0].at = 0; ed.p[0].shape = SH_SMOOTH;
    ed_endh[0] = ed_endh[1] = 0; ed_sel = ed_anchor = 0; ed_dirty = 0; ed_gen++;
}
static void ed_append(const Move *m, int closes)
{
    ed_push(0);
    for (int k = 0; k < m->n; k++) {
        TPiece p = m->p[k];
        p.shape = SH_SMOOTH; p.amp = 0;
        for (int e = 0; e < 2; e++) for (int i = 0; i < 14; i++) p.h[e][i] = ed_endh[e];   /* new pieces carry on at the open end's height */
        ed.p[ed.np++] = p;
    }
    if (closes) regen(ed.np - 1);
    ed_select(ed.np - 1, 0);
}
/* the move for this turn at the open end that fits, or NULL (*joins: there is one but no room, 2: too many pieces) */
static const Move *ed_move_for(int turn, Move *mv, int *closes, int *joins)
{
    int e[2][2]; open_end(e);
    int n = moves(e, mv);
    if (joins) *joins = 0;
    for (int k = 0; k < n; k++) if (mv[k].turn == turn) {
        if (move_ok(&mv[k], closes)) return &mv[k];
        if (joins) *joins = ed.np + mv[k].n > MAXPC ? 2 : 1;
    }
    return NULL;
}
static void ed_extend(int turn)
{
    if (ed_closed()) { ed_note("The loop is closed - select pieces and press DEL to open it there"); return; }
    Move mv[8]; int closes, joins;
    const Move *m = ed_move_for(turn, mv, &closes, &joins);
    if (m) {
        ed_append(m, closes);
        if (closes) ed_note("Loop closed! T to test drive, CTRL+S to save");
        return;
    }
    const char *what = turn == 0 ? "straight" : abs(turn) == 45 ? "45 degree bend" : "90 degree bend";
    char b[96];
    if (joins) {
        snprintf(b, sizeof b, "No room for a %s: the grid ends or the track is in the way", what);
        ed_note(joins == 2 ? "100 pieces is the most a track can have" : b); return;
    }
    /* would it join on one straight further on? (the original's bends only start at some places in a cube) */
    const Move *s = ed_move_for(0, mv, &closes, NULL);
    int after = 0, n0 = ed.np;
    if (s && !closes) {
        for (int k = 0; k < s->n; k++) ed.p[ed.np++] = s->p[k];
        after = ed_move_for(turn, mv, &closes, &joins) != NULL || joins;
        ed.np = n0;
    }
    snprintf(b, sizeof b, "No %s joins on here%s", what, after ? " - add a straight (UP) first" : abs(turn) == 90 ? ": 90 degree bends start along the grid" : " - the ghosts show what does");
    ed_note(b);
}
/* the same loop numbered from piece k */
static void ed_renumber(int k)
{
    TPiece t[MAXPC];
    for (int i = 0; i < ed.np; i++) t[i] = ed.p[(i + k) % ed.np];
    memcpy(ed.p, t, sizeof(TPiece) * ed.np);
    ed.start = (ed.start - k + ed.np) % ed.np; ed_sel = (ed_sel - k + ed.np) % ed.np; ed_anchor = (ed_anchor - k + ed.np) % ed.np;
}
/* the selected pieces come off: a closed loop opens there, an open track loses pieces at either end */
static void ed_remove(void)
{
    int a = ed_lo(), b = ed_hi(), n = b - a + 1, cl = ed_closed();
    if (n >= ed.np) { ed_note(ed.np == 1 ? "The last piece stays - ALT+arrows move it, ALT+R turns it" : "One piece has to stay"); return; }
    if (!cl && a != 0 && b != ed.np - 1) { ed_note("The loop is open: pieces only come off at its ends (C closes it)"); return; }
    ed_push(0);
    if (cl) { ed_renumber(b + 1); a = ed.np - n; b = ed.np - 1; }       /* the selection last: the gap opens where it was */
    int gone = ed.start >= a && ed.start <= b;
    if (b == ed.np - 1) {
        ed_endh[0] = ed.p[a].h[0][0]; ed_endh[1] = ed.p[a].h[1][0];
        ed.np -= n; ed_select(ed.np - 1, 0);
    } else {                                                    /* the first pieces of an open track */
        memmove(ed.p, ed.p + n, sizeof(TPiece) * (ed.np - n));
        ed.np -= n; ed_select(0, 0);
        if (!gone) ed.start -= n;
    }
    if (gone) {                                                 /* the start line goes to the next straight */
        ed.start = 0;
        for (int k = 0; k < ed.np; k++) if ((ed.p[k].at & 15) == 0) { ed.start = k; break; }
    }
}
/* the whole track a cube over (dx, dz) and q quarter turns clockwise (seen from above) about the grid's middle */
static void ed_move(int dx, int dz, int q)
{
    TrackDef t = ed;
    for (int p = 0; p < t.np; p++) {
        TPiece *pc = &t.p[p];
        int x = pc->cx + dx, z = pc->cz + dz;
        for (int r = 0; r < q; r++) { int x0 = x; x = z; z = 15 - x0; }
        if (x < 0 || x > 15 || z < 0 || z > 15) { ed_note("The track would leave the grid"); return; }
        pc->cx = (unsigned char)x; pc->cz = (unsigned char)z; pc->at = (unsigned char)((pc->at & 0x3f) | ((pc->at + (q << 6)) & 0xc0));
        if (!pc_ongrid(pc)) { ed_note("The track would leave the grid"); return; }
    }
    ed_push(0); ed = t;
}
/* the shortest way back to the first piece: breadth first over single pieces, clear of the track */
static int ed_autoclose(void)
{
    if (ed_closed()) { ed_note("The loop is already closed"); return 0; }
    typedef struct { int e[2][2], prev, depth; TPiece p; } BN;
    enum { CAP = 40000 };
    static BN q[CAP]; static unsigned char vis[256 * 256 * 8];
    memset(vis, 0, sizeof vis);
    int s0[2][2], qt = 1;
    pc_xs(&ed.p[0], 0, s0);
    open_end(q[0].e); q[0].prev = -1; q[0].depth = 0;
    for (int qh = 0; qh < qt; qh++) {
        TPiece c[64]; int n = cands(q[qh].e, c);
        for (int k = 0; k < n; k++) {
            int x[2][2]; pc_endxs(&c[k], x);
            int closes = xs_eq(x, s0);
            if (!fits(&c[k], q[qh].depth, closes)) continue;
            if (closes) {
                TPiece path[MAXPC]; int L = q[qh].depth + 1;
                if (ed.np + L > MAXPC) continue;
                path[L - 1] = c[k];
                for (int i = qh, d = L - 2; d >= 0; i = q[i].prev, d--) path[d] = q[i].p;
                int bad = 0;
                for (int a = 0; a < L && !bad; a++) for (int b = a + 2; b < L && !bad; b++) bad = pc_clash(&path[a], &path[b]);
                if (bad) continue;
                ed_push(0);
                int h0[2] = {ed_endh[0], ed_endh[1]}, n0 = ed.np;
                for (int a = 0; a < L; a++) {
                    TPiece p = path[a]; p.shape = SH_SMOOTH; p.amp = 0;
                    for (int e = 0; e < 2; e++) {
                        int h = h0[e] + (ed.p[0].h[e][0] - h0[e]) * a / L;
                        for (int i = 0; i < 14; i++) p.h[e][i] = h;
                    }
                    ed.p[ed.np++] = p;
                }
                for (int a = n0; a < ed.np; a++) regen(a);
                ed_select(ed.np - 1, 0);
                char b[64]; snprintf(b, sizeof b, "Loop closed with %d pieces", L);
                ed_note(b);
                return 1;
            }
            float mx = (x[0][0] + x[1][0]) * 0.5f, mz = (x[0][1] + x[1][1]) * 0.5f;
            int oct = ((int)lroundf(xs_ang(x) * 4 / PI_F) + 8) & 7;
            int kx = (int)clampf(mx / 128.f, 0, 255), kz = (int)clampf(mz / 128.f, 0, 255), key = (kz * 256 + kx) * 8 + oct;
            if (vis[key] || qt >= CAP || q[qh].depth + 2 + ed.np > MAXPC) continue;
            vis[key] = 1;
            memcpy(q[qt].e, x, sizeof x); q[qt].prev = qh; q[qt].depth = q[qh].depth + 1; q[qt].p = c[k]; qt++;
        }
    }
    ed_note("No way back to the start from here - remove a few pieces (DEL) and try again");
    return 0;
}
/* ready to drive: a closed loop with a straight for the start line */
static int ed_ready(void)
{
    if (!ed_closed()) { ed_note("Close the loop first (C does it for you)"); return 0; }
    if (ed.np < 4) { ed_note("A track needs at least 4 pieces"); return 0; }
    if ((ed.p[ed.start].at & 15) != 0) {
        int k = 0;
        while (k < ed.np && (ed.p[k].at & 15) != 0) k++;
        if (k == ed.np) { ed_note("The start line needs a straight piece"); return 0; }
        ed.start = k;
    }
    return 1;
}
/* the AI's jump planner on the track as it stands: the pieces whose jumps can't be cleared */
static void ed_jumps_note(void)
{
    ed_nbad = 0;
    for (int k = 0; k < nbadj && ed_nbad < MAXPC; k++) ed_bad[ed_nbad++] = seg_sec[badj[k]];
    char b[96];
    if (!ed_nbad) { ed_note("Every jump can be made"); return; }
    snprintf(b, sizeof b, "The jump after piece %d cannot be cleared - lower it or give it more run-up", ed_bad[0] + 1);
    ed_note(b); ed_msg_t = 8.f;
}
static void ed_check_jumps(void)
{
    if (!ed_ready()) return;
    float keep[4] = {ar_x0, ar_x1, ar_z0, ar_z1}; int tg = track_gen;
    trk[ED_SLOT] = ed; trk[ED_SLOT].orig = -1;
    build_track_geometry(ED_SLOT); cur_track = -1;
    plan_need();
    ar_x0 = keep[0]; ar_x1 = keep[1]; ar_z0 = keep[2]; ar_z1 = keep[3]; track_gen = tg;   /* the builder's stadium stays */
    ed_jumps_note();
    ed_built = -1;                                              /* the mesh again (an open track has its gap) */
}
static void ed_test(void)
{
    if (!ed_ready()) return;
    trk[ED_SLOT] = ed; trk[ED_SLOT].orig = -1;
    build_track(ED_SLOT);
    race_mode = RM_TEST; cockpit = 1; start_race();
}
static void ed_save(void)
{
    if (!ed_ready()) return;
    char keep[32]; snprintf(keep, sizeof keep, "%s", sel_track < ntrk ? trk[sel_track].name : "");
    if (track_save(&ed)) { ed_note("Could not write the track file"); return; }
    tracks_scan();
    int k = track_find(keep); sel_track = k >= 0 ? k : 0; cur_track = -1;
    char b[96]; snprintf(b, sizeof b, "Saved as tracks/%s", ed.file);
    ed_note(b); ed_dirty = 0;
}
/* an original comes in with its jump edits baked into the heights */
static void ed_import(int i)
{
    ed_push(0);
    ed = trk[i];
    if (ed.orig >= 0) {
        track_xsecs(&ed);
        for (int q = 0, n = 0; q < ed.np; q++) {
            TPiece *pc = &ed.p[(ed.start + 1 + q) % ed.np];
            for (int r = 0; r < pc_ns(pc); r++, n++) for (int e = 0; e < 2; e++) pc->h[e][r] = tx_ey[n][e];
        }
        char nm[40]; snprintf(nm, sizeof nm, "MY %s", ed.name); SDL_strlcpy(ed.name, nm, sizeof ed.name);
        ed.orig = -1; ed.file[0] = 0;
    }
    ed_endh[0] = ed.p[0].h[0][0]; ed_endh[1] = ed.p[0].h[1][0];
    ed_select(0, 0); ed_dirty = ed.file[0] == 0;
}
