/* editor.h - the track builder, in 3D in the stadium: fly over the arena and build on the original's 16x16 grid, marked
 * on the floor. The track grows from its open end with the original's pieces (only those that join on and keep clear of
 * the other pieces' cubes are offered: ghosts to click, or the arrow keys), C closes the loop the shortest way round.
 * Removing pieces from a closed loop opens it there: the pieces are renumbered so the gap is always after the last one.
 * Heights are set where two pieces meet (the selected piece's end, or every joint of a selected run); each piece has a
 * shape between its two ends. Every change can be undone. The stadium is built once round the whole grid
 * (scene_builder), the track as its own mesh (tkb) a moment after each change. Saves to tracks/NAME.trk, T test-drives,
 * G flies along the road. Included by scr_main.h */
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

/* ---- undo: the whole track before each change. A run of the same tweak (kind > 0) on one selection is one step ---- */
enum { UNDO_N = 64, TW_HEIGHT = 1, TW_BANK, TW_SIZE };
typedef struct { TrackDef t; int endh[2], sel, anchor; } EdSnap;
static EdSnap ed_un[UNDO_N], ed_re[UNDO_N];             /* ed_un a ring (oldest dropped), ed_re a stack */
static int ed_un_n, ed_un_top, ed_re_n, ed_tweak = -1, ed_tweak_sel;
static void snap_take(EdSnap *s) { s->t = ed; memcpy(s->endh, ed_endh, sizeof s->endh); s->sel = ed_sel; s->anchor = ed_anchor; }
static void snap_put(const EdSnap *s)
{
    ed = s->t; memcpy(ed_endh, s->endh, sizeof ed_endh); ed_sel = s->sel; ed_anchor = s->anchor;
    ed_nbad = 0; ed_dirty = 1; ed_tweak = -1; ed_gen++; ed_gen_t = 0;
}
static void ed_push(int kind)
{
    ed_nbad = 0; ed_dirty = 1; ed_gen++; ed_gen_t = 0;
    if (kind > 0 && kind == ed_tweak && ed_sel * 128 + ed_anchor == ed_tweak_sel) return;
    ed_tweak = kind; ed_tweak_sel = ed_sel * 128 + ed_anchor;
    snap_take(&ed_un[ed_un_top]); ed_un_top = (ed_un_top + 1) % UNDO_N;
    if (ed_un_n < UNDO_N) ed_un_n++;
    ed_re_n = 0;
}
static void ed_undo(int redo)
{
    if (redo ? !ed_re_n : !ed_un_n) { ed_note(redo ? "Nothing to redo" : "Nothing to undo"); return; }
    if (redo) { snap_take(&ed_un[ed_un_top]); ed_un_top = (ed_un_top + 1) % UNDO_N; ed_un_n++; snap_put(&ed_re[--ed_re_n]); }
    else { snap_take(&ed_re[ed_re_n++]); ed_un_top = (ed_un_top + UNDO_N - 1) % UNDO_N; ed_un_n--; snap_put(&ed_un[ed_un_top]); }
}

static int xs_eq(const int a[2][2], const int b[2][2])
{
    for (int e = 0; e < 2; e++) for (int j = 0; j < 2; j++) if (abs(a[e][j] - b[e][j]) > 8) return 0;
    return 1;
}
static void pc_xs(const TPiece *p, int i, int o[2][2]) { tpl_cross(p->at, i, p->cx, p->cz, o); }
static void pc_endxs(const TPiece *p, int o[2][2]) { pc_xs(p, pc_ns(p), o); }
static float xs_ang(const int e[2][2]) { return atan2f((float)(e[1][0] - e[0][0]), (float)-(e[1][1] - e[0][1])); }
static int ed_closed(void)
{
    if (ed.np < 2) return 0;
    int a[2][2], b[2][2]; pc_endxs(&ed.p[ed.np - 1], a); pc_xs(&ed.p[0], 0, b);
    return xs_eq(a, b);
}

/* every piece that joins on at cross-section e, each shape once */
static int cands(const int e[2][2], TPiece *out)
{
    int n = 0;
    for (int ti = 0; ti < 7; ti++) for (int d = 0; d < 2; d++) for (int r = 0; r < 4; r++) {
        int at = TPL_USE[ti] | (d ? 0x10 : 0) | (r << 6), s[2][2], x1[2][2], x2[2][2], dup = 0;
        tpl_cross(at, 0, 0, 0, s);
        int cx = (int)lroundf((e[0][0] - s[0][0]) / 2048.f), cz = (int)lroundf((e[0][1] - s[0][1]) / 2048.f);
        if (cx < 0 || cx > 15 || cz < 0 || cz > 15) continue;
        tpl_cross(at, 0, cx, cz, s);
        if (!xs_eq(s, e)) continue;
        TPiece c; memset(&c, 0, sizeof c);
        c.cx = (unsigned char)cx; c.cz = (unsigned char)cz; c.at = (unsigned char)at; c.shape = SH_SMOOTH;
        pc_endxs(&c, x1);
        for (int k = 0; k < n && !dup; k++) { pc_endxs(&out[k], x2); dup = out[k].cx == cx && out[k].cz == cz && xs_eq(x1, x2); }
        if (!dup) out[n++] = c;
    }
    return n;
}
/* a heading the road can carry on straight from (along the grid or diagonally) */
static int stable(const int e[2][2])
{
    TPiece c[64]; int n = cands(e, c);
    for (int k = 0; k < n; k++) if ((c[k].at & 15) == 0 || (c[k].at & 15) == 10) return 1;
    return 0;
}
/* the cubes a piece's road passes over (-1: off the grid) */
static int pc_cubes(const TPiece *p, int *cb)
{
    int n = 0, ns = pc_ns(p), a[2][2], b[2][2];
    pc_xs(p, 0, a);
    for (int i = 0; i < ns; i++) {
        pc_xs(p, i + 1, b);
        int mx = (a[0][0] + a[1][0] + b[0][0] + b[1][0]) / 4, mz = (a[0][1] + a[1][1] + b[0][1] + b[1][1]) / 4;
        int c = mx < 0 || mz < 0 || mx >= 16 * 2048 || mz >= 16 * 2048 ? -1 : (mz / 2048) * 16 + mx / 2048, dup = 0;
        for (int k = 0; k < n; k++) dup |= cb[k] == c;
        if (!dup) cb[n++] = c;
        memcpy(a, b, sizeof a);
    }
    return n;
}
static int pc_clash(const TPiece *a, const TPiece *b)
{
    int ca[16], cb[16], na = pc_cubes(a, ca), nb = pc_cubes(b, cb);
    for (int i = 0; i < na; i++) for (int j = 0; j < nb; j++) if (ca[i] == cb[j]) return 1;
    return 0;
}
static int pc_ongrid(const TPiece *p) { int c[16], n = pc_cubes(p, c); for (int i = 0; i < n; i++) if (c[i] < 0) return 0; return 1; }
/* new piece c, the k-th after the open end: clear of every piece but its neighbours */
static int fits(const TPiece *c, int k, int closes)
{
    if (!pc_ongrid(c)) return 0;
    for (int j = 0; j < ed.np; j++) {
        if ((j == ed.np - 1 && k == 0) || (j == 0 && closes)) continue;
        if (pc_clash(c, &ed.p[j])) return 0;
    }
    return 1;
}
static void move_find(const int e[2][2], Move *m, Move *out, int *nm)
{
    int x[2][2];
    if (m->n) { pc_endxs(&m->p[m->n - 1], x); } else memcpy(x, e, sizeof x);
    if (m->n && stable(x)) {
        float d = xs_ang(x) - xs_ang(e);
        while (d > PI_F) d -= 2 * PI_F;
        while (d <= -PI_F) d += 2 * PI_F;
        m->turn = (int)lroundf(d * 4 / PI_F) * 45;
        for (int j = 0; j < *nm; j++) if (out[j].turn == m->turn) { if (m->n < out[j].n) out[j] = *m; return; }
        out[(*nm)++] = *m;
        return;
    }
    if (m->n >= 3) return;
    TPiece c[64]; int n = cands(x, c);
    for (int k = 0; k < n; k++) { m->p[m->n++] = c[k]; move_find(e, m, out, nm); m->n--; }
}
/* straight on, 45 and 90 degree bends: the pieces it takes to get to the next heading */
static int moves(const int e[2][2], Move *out)
{
    Move m; memset(&m, 0, sizeof m);
    int nm = 0;
    move_find(e, &m, out, &nm);
    return nm;
}
static void open_end(int e[2][2]) { pc_endxs(&ed.p[ed.np - 1], e); }
static int move_ok(const Move *m, int *closes)
{
    int s0[2][2], x[2][2];
    pc_xs(&ed.p[0], 0, s0); pc_endxs(&m->p[m->n - 1], x);
    *closes = xs_eq(x, s0);
    if (ed.np + m->n > MAXPC) return 0;
    for (int k = 0; k < m->n; k++) if (!fits(&m->p[k], k, *closes && k == m->n - 1)) return 0;
    return 1;
}

/* ---- heights: joint j is where piece j starts (j = np: the open end) ---- */
static int ed_left_edge(void)
{
    int a[2][2], b[2][2];
    pc_xs(&ed.p[0], 0, a); pc_xs(&ed.p[0], 1, b);
    float mx = (a[0][0] + a[1][0]) * 0.5f, mz = (a[0][1] + a[1][1]) * 0.5f;
    float dx = (b[0][0] + b[1][0]) * 0.5f - mx, dz = (b[0][1] + b[1][1]) * 0.5f - mz;
    return ((a[0][0] - mx) * -dz + (a[0][1] - mz) * dx) > 0 ? 0 : 1;
}
static int *joint_ptr(int j, int e)
{
    if (j >= ed.np) return ed_closed() ? &ed.p[0].h[e][0] : &ed_endh[e];
    return &ed.p[j].h[e][0];
}
static void regen(int p)
{
    TPiece *pc = &ed.p[p];
    if (pc->shape == SH_RAW) return;
    int ns = pc_ns(pc);
    for (int e = 0; e < 2; e++) {
        float hs = (float)pc->h[e][0], he = (float)*joint_ptr(p + 1, e), A = pc->amp * U_PER_M;
        for (int i = 1; i < ns; i++) {
            float f = (float)i / ns, g = (float)i / (ns - 1), h;
            switch (pc->shape) {
            case SH_LINEAR: h = hs + (he - hs) * f; break;
            case SH_HUMP: h = hs + (he - hs) * smooth01(f) + A * sinf(PI_F * f); break;
            case SH_KICKER: h = hs + A * g * g; break;                    /* a lip at the end, the drop to the next piece */
            case SH_STEP: h = he; break;                                  /* a sheer face straight after the start */
            case SH_PIT: h = fminf(hs, he) - A; break;
            default: h = hs + (he - hs) * smooth01(f); break;
            }
            pc->h[e][i] = (int)lroundf(clampf(h, 0, 250 * U_PER_M));
        }
    }
}
static void joint_get(int j, float *c, float *b)
{
    int L = ed_left_edge(), hl = *joint_ptr(j, L), hr = *joint_ptr(j, 1 - L);
    *c = (hl + hr) * 0.5f / U_PER_M; *b = (hl - hr) / U_PER_M;
}
static void joint_set(int j, float c, float b)
{
    ed_nbad = 0;
    int L = ed_left_edge(), cl = ed_closed();
    c = clampf(c, fabsf(b) / 2, 250); b = clampf(b, -6, 6);
    *joint_ptr(j, L) = (int)lroundf((c + b / 2) * U_PER_M); *joint_ptr(j, 1 - L) = (int)lroundf((c - b / 2) * U_PER_M);
    if (j < ed.np) regen(j); else if (cl) regen(0);
    int pj = j - 1 < 0 && cl ? ed.np - 1 : j - 1;
    if (pj >= 0) regen(pj);
    ed_dirty = 1;
}

/* the joints the height and banking keys move: one piece's end, or a run's every joint from its first piece's start to
 * its last one's end (the pieces either side become the ramps) */
static int ed_joints(int *j)
{
    int a = ed_lo(), b = ed_hi(), n = 0;
    if (a == b) { j[0] = a + 1; return 1; }
    for (int k = a; k <= b + 1; k++) if (!(k == ed.np && a == 0 && ed_closed())) j[n++] = k;   /* a whole closed loop: its end is its start */
    return n;
}
static void ed_tweak_joints(int kind, float dh, float db)
{
    int jl[MAXPC + 1], nj = ed_joints(jl);
    ed_push(kind);
    for (int k = 0; k < nj; k++) { float c, bk; joint_get(jl[k], &c, &bk); joint_set(jl[k], c + dh, bk + db); }
}

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
/* ---- the scene: the stadium round the whole grid once, the track as its own mesh after each change ---- */
static void ed_build_scene(void)
{
    const float E = 8 * 2048 * SCR_U;
    int ns = nseg; nseg = 0;                                    /* no track: nothing worn, no compound to keep clear of it */
    tr_minx = tr_minz = -E; tr_maxx = tr_maxz = E;
    ar_x0 = ar_z0 = -E - 90; ar_x1 = ar_z1 = E + 90;
    scene_builder = 1; cur_track = -1; build_static(); scene_builder = 0;
    track_gen++; nseg = ns;
}
static void ed_build_mesh(void)
{
    float keep[4] = {ar_x0, ar_x1, ar_z0, ar_z1}; int tg = track_gen;
    trk[ED_SLOT] = ed; trk[ED_SLOT].orig = -1;
    if (!ed_closed()) { trk[ED_SLOT].start = ed.np - 1; trk_open = 1; memcpy(trk_endh, ed_endh, sizeof trk_endh); }
    build_track_geometry(ED_SLOT);
    ar_x0 = keep[0]; ar_x1 = keep[1]; ar_z0 = keep[2]; ar_z1 = keep[3]; track_gen = tg; cur_track = -1;   /* the stadium stays */
    VB *was = cur; cur = &tkb; tkb.n = 0;
    for (int i = 0; i < nseg; i++) build_seg_mesh(i);
    for (int i = 10; i < nseg; i += 30) if (!seg[i].gap) build_tower(i);
    cur = was; trk_open = 0; tkb_gen++; ed_built = ed_gen;
}

/* ---- the pieces in the world ---- */
static float ed_wy(int h) { return h * 0.25f * SCR_U + tr_lift; }
/* cross-section i of a piece, both edges, in metres; p < 0: a ghost at the open end's height */
static void piece_world(const TPiece *pc, int p, int i, V3 o[2])
{
    int a[2][2]; pc_xs(pc, i, a);
    for (int e = 0; e < 2; e++) {
        int h = p < 0 ? ed_endh[e] : i < pc_ns(pc) ? pc->h[e][i] : *joint_ptr(p + 1, e);
        o[e] = V(-(a[e][0] - 8 * 2048) * SCR_U, ed_wy(h), (a[e][1] - 8 * 2048) * SCR_U);
    }
}
static V3 piece_mid(const TPiece *pc, int p) { V3 o[2]; piece_world(pc, p, pc_ns(pc) / 2, o); return vmul(vadd(o[0], o[1]), 0.5f); }
static V3 joint_world(int j)
{
    V3 o[2];
    if (j < ed.np) piece_world(&ed.p[j], j, 0, o); else piece_world(&ed.p[ed.np - 1], ed.np - 1, pc_ns(&ed.p[ed.np - 1]), o);
    return vmul(vadd(o[0], o[1]), 0.5f);
}
/* the ghosts: what can join on at the open end */
static int ed_ghosts(Move *mv)
{
    if (!ed.np || ed_closed()) return 0;
    int e[2][2], cl, n = 0; open_end(e);
    Move all[8]; int na = moves(e, all);
    for (int k = 0; k < na; k++) if (move_ok(&all[k], &cl)) mv[n++] = all[k];
    return n;
}

/* ---- the flying camera ---- */
static V3 fc_pos = {0, 140, -560};
static float fc_yaw, fc_pitch = -0.32f;
static int fc_look, fc_space;                          /* looking round: the right mouse button or Space held */
static float fly_u, fly_v, fly_was;                    /* fly-along: where on the road, how fast */
static V3 fly_f, fly_n;
static int fly_snap;
static V3 fc_dir(void) { return V(sinf(fc_yaw) * cosf(fc_pitch), sinf(fc_pitch), cosf(fc_yaw) * cosf(fc_pitch)); }
static void fc_clamp(void) { fc_pos.y = clampf(fc_pos.y, 2, 700); fc_pos.x = clampf(fc_pos.x, -1600, 1600); fc_pos.z = clampf(fc_pos.z, -1600, 1600); }
static void fc_set_look(int on) { if (on != fc_look) { fc_look = on; if (win) SDL_SetWindowRelativeMouseMode(win, on); } }
/* the camera to the selection, from the way it looks now: one piece's end (where the track goes on), a run's middle */
static V3 joint_world(int j);
static void ed_view_sel(void)
{
    V3 a = piece_mid(&ed.p[ed_lo()], ed_lo()), b = piece_mid(&ed.p[ed_hi()], ed_hi()), t = vmul(vadd(a, b), 0.5f);
    if (ed_lo() == ed_hi()) {                                   /* one piece: from behind it, looking the way it goes */
        V3 e = joint_world(ed_sel + 1), h = vsub(e, a);
        if (h.x * h.x + h.z * h.z > 1) fc_yaw = atan2f(h.x, h.z);
        t = vadd(e, vmul(vnorm(V(h.x, 0, h.z)), 8));
    }
    fc_pitch = -0.6f;
    fc_pos = vsub(t, vmul(fc_dir(), 110 + vlen(vsub(a, b)) * 0.6f)); fc_clamp();
}
static int ed_seg_of(int p)
{
    for (int k = 0; k < nseg; k++) if (seg_sec[k] == p && !seg[k].gap) return k;
    return -1;
}
static void ed_fly(int on)
{
    if (!on) { ed_mode = EM_EDIT; return; }
    if (ed_built != ed_gen) ed_build_mesh();
    int k = ed_seg_of(ed_lo());
    if (!nseg || k < 0) return;
    ed_mode = EM_FLY; fly_u = (float)k; fly_v = 25; fly_snap = 1; fc_set_look(0);
}
static void ed_tick(float dt)
{
    ed_gen_t += dt;
    if (ed.np && ed_built != ed_gen && (ed_gen_t > 0.08f || ed_built < 0)) ed_build_mesh();
    if (ed_mode == EM_FLY && nseg) {
        fly_u = fmodf(fly_u + fly_v * dt / ds + nseg, (float)nseg);
        for (int g = 0; g < nseg && seg[(int)fly_u % nseg].gap; g++) fly_u = fmodf(fly_u + (fly_v < 0 ? -1 : 1) + nseg, (float)nseg);
        return;
    }
    if (ed_mode != EM_EDIT || !keys) return;
    SDL_Keymod m = SDL_GetModState();
    int sp = keys[SDL_SCANCODE_SPACE];
    if (sp != fc_space) { fc_space = sp; fc_set_look(sp); }
    if (m & (SDL_KMOD_CTRL | SDL_KMOD_GUI | SDL_KMOD_ALT)) return;
    float v = fmaxf(25.f, fc_pos.y * 0.9f) * (m & SDL_KMOD_SHIFT ? 3.f : 1.f);
    V3 f = V(sinf(fc_yaw), 0, cosf(fc_yaw)), r = V(-f.z, 0, f.x), d = V(0, 0, 0);
    if (keys[SDL_SCANCODE_W]) d = vadd(d, f);
    if (keys[SDL_SCANCODE_S]) d = vsub(d, f);
    if (keys[SDL_SCANCODE_D]) d = vadd(d, r);
    if (keys[SDL_SCANCODE_A]) d = vsub(d, r);
    if (keys[SDL_SCANCODE_E]) d = vadd(d, UP);
    if (keys[SDL_SCANCODE_Q]) d = vsub(d, UP);
    fc_pos = vadd(fc_pos, vmul(d, v * dt)); fc_clamp();
}
static void ed_cam(float dt)
{
    V3 up = UP;
    if (ed_mode == EM_FLY && nseg) {                           /* at a driver's eye height, looking 8 m up the road */
        int k = (int)fly_u % nseg, ka = (k + (int)(8 / ds)) % nseg;
        float f = fly_u - floorf(fly_u), kf = fly_snap ? 1.f : 1.f - expf(-10.f * dt);
        const Seg *a = &seg[k], *b = &seg[(k + 1) % nseg];
        V3 n = vnorm(vlerp(a->N, b->N, f));
        cam.pos = vadd(vlerp(a->c, b->c, f), vmul(n, 1.8f));
        V3 fw = seg[ka].gap ? a->T : vnorm(vsub(vadd(seg[ka].c, vmul(seg[ka].N, 1.4f)), cam.pos));
        fly_f = vnorm(vlerp(fly_f, fw, kf)); fly_n = vnorm(vlerp(fly_n, n, kf)); fly_snap = 0;
        cam.fwd = fly_f; up = fly_n;
    } else { cam.pos = fc_pos; cam.fwd = fc_dir(); }
    cam.right = vnorm(vcross(cam.fwd, up)); cam.up = vcross(cam.right, cam.fwd);
}

/* ---- the mouse: what is under it ---- */
typedef struct { int kind, p, turn; } Pick;            /* kind 0 nothing, 1 a piece (p), 2 a ghost (turn) */
static float ed_hx = -1, ed_hy = -1;
static float ray_tri(V3 o, V3 d, V3 a, V3 b, V3 c)
{
    V3 e1 = vsub(b, a), e2 = vsub(c, a), p = vcross(d, e2);
    float det = vdot(e1, p);
    if (fabsf(det) < 1e-6f) return -1;
    V3 t = vsub(o, a), q = vcross(t, e1);
    float u = vdot(t, p) / det, v = vdot(d, q) / det, s = vdot(e2, q) / det;
    return u < 0 || v < 0 || u + v > 1 || s <= 0 ? -1 : s;
}
static float ray_piece(V3 o, V3 d, const TPiece *pc, int p)
{
    float best = -1; V3 a[2], b[2];
    piece_world(pc, p, 0, a);
    for (int i = 1; i <= pc_ns(pc); i++, memcpy(a, b, sizeof a)) {
        piece_world(pc, p, i, b);
        float t1 = ray_tri(o, d, a[0], a[1], b[1]), t2 = ray_tri(o, d, a[0], b[1], b[0]);
        if (t1 > 0 && (best < 0 || t1 < best)) best = t1;
        if (t2 > 0 && (best < 0 || t2 < best)) best = t2;
    }
    return best;
}
static Pick ed_pick(float hx, float hy)
{
    Pick k = {0, -1, 0};
    if (hx < 0) return k;
    float asp = HW / VH, best = 1e9f;
    V3 o = cam.pos, d = vnorm(vadd(cam.fwd, vadd(vmul(cam.right, (hx / HW * 2 - 1) * asp / FOCAL_Y), vmul(cam.up, (1 - hy / VH * 2) / FOCAL_Y))));
    for (int p = 0; p < ed.np; p++) { float t = ray_piece(o, d, &ed.p[p], p); if (t > 0 && t < best) { best = t; k.kind = 1; k.p = p; } }
    Move mv[8]; int n = ed_ghosts(mv);
    for (int g = 0; g < n; g++) for (int j = 0; j < mv[g].n; j++) {
        float t = ray_piece(o, d, &mv[g].p[j], -1);
        if (t > 0 && t < best) { best = t; k.kind = 2; k.turn = mv[g].turn; }
    }
    return k;
}

/* ---- drawing in the world: ghosts, the selection, the joints that move ---- */
static void ed_fence(const TPiece *pc, int p, float h0, float h1, uint32_t col)   /* a low fence along both edges */
{
    V3 a[2], b[2];
    piece_world(pc, p, 0, a);
    for (int i = 1; i <= pc_ns(pc); i++, memcpy(a, b, sizeof a)) {
        piece_world(pc, p, i, b);
        for (int e = 0; e < 2; e++) draw_quad_unlit(vadd(a[e], V(0, h0, 0)), vadd(b[e], V(0, h0, 0)), vadd(b[e], V(0, h1, 0)), vadd(a[e], V(0, h1, 0)), col);
    }
}
static void ed_deck(const TPiece *pc, int p, float lift, uint32_t col)   /* a flat copy of the road over it */
{
    V3 a[2], b[2], l = V(0, lift, 0);
    piece_world(pc, p, 0, a);
    for (int i = 1; i <= pc_ns(pc); i++, memcpy(a, b, sizeof a)) {
        piece_world(pc, p, i, b);
        draw_quad_unlit(vadd(a[0], l), vadd(a[1], l), vadd(b[1], l), vadd(b[0], l), col);
    }
}
static void ed_pole(V3 p, float h, uint32_t col)
{
    for (int k = 0; k < 2; k++) {
        V3 s = k ? V(0.35f, 0, 0) : V(0, 0, 0.35f);
        draw_quad_unlit(vsub(p, s), vadd(p, s), vadd(vadd(p, s), V(0, h, 0)), vadd(vsub(p, s), V(0, h, 0)), col);
    }
    V3 t = vadd(p, V(0, h, 0)), r = vmul(cam.right, 1.2f);
    draw_tri_unlit(vadd(t, V(0, 1.8f, 0)), vadd(t, r), vsub(t, r), col);
    draw_tri_unlit(vsub(t, V(0, 1.8f, 0)), vadd(t, r), vsub(t, r), col);
}
static Pick ed_hov;
static void ed_draw3d(void)
{
    if (ed_mode == EM_FLY || !ed.np) return;
    ed_hov = fc_look ? (Pick){0, -1, 0} : ed_pick(ed_hx, ed_hy);
    Move mv[8]; int n = ed_ghosts(mv);
    for (int g = 0; g < n; g++) {
        int hot = ed_hov.kind == 2 && ed_hov.turn == mv[g].turn;
        for (int j = 0; j < mv[g].n; j++) {
            ed_deck(&mv[g].p[j], -1, 0.2f, hot ? 0xF0D060 : 0x78B4E8);
            ed_fence(&mv[g].p[j], -1, 0.2f, hot ? 1.6f : 1.0f, hot ? 0xFFF0A0 : 0x3C78C8);
        }
    }
    for (int k = 0; k < ed_nbad; k++) if (ed_bad[k] < ed.np) ed_fence(&ed.p[ed_bad[k]], ed_bad[k], 0.1f, 1.4f, 0xFF3030);
    if (ed_hov.kind == 1 && (ed_hov.p < ed_lo() || ed_hov.p > ed_hi())) ed_fence(&ed.p[ed_hov.p], ed_hov.p, 0.1f, 0.9f, 0xF0F0F0);
    for (int p = ed_lo(); p <= ed_hi(); p++) ed_fence(&ed.p[p], p, 0.1f, 1.6f, 0x30E0FF);
    int jl[MAXPC + 1], nj = ed_joints(jl);                        /* the joints R/F move: poles at the run's ends */
    for (int k = 0; k < nj; k += nj > 1 ? nj - 1 : 1) ed_pole(joint_world(jl[k]), 7, 0xFFD040);
    if (!ed_closed()) {                                         /* the start line (a closed track has it on the road) */
        V3 o[2]; piece_world(&ed.p[ed.start], ed.start, pc_ns(&ed.p[ed.start]), o);
        V3 s = vmul(vsub(o[1], o[0]), 1.f / 8), f = vmul(vnorm(vcross(vsub(o[1], o[0]), UP)), 1.2f), y = V(0, 0.12f, 0);
        for (int k = 0; k < 8; k++) for (int r = 0; r < 2; r++) {
            V3 a = vadd(vadd(o[0], vmul(s, (float)k)), vadd(vmul(f, (float)r), y));
            draw_quad_unlit(a, vadd(a, s), vadd(vadd(a, s), f), vadd(a, f), (k + r) & 1 ? 0x101010 : 0xF8F8F8);
        }
    }
}

/* ---- the HUD ---- */
static int ed_proj(V3 p, float *sx, float *sy)
{
    V3 d = vsub(p, cam.pos);
    float z = vdot(d, cam.fwd), asp = HW / VH;
    if (z < 0.5f) return 0;
    *sx = (vdot(d, cam.right) / z * FOCAL_Y / asp + 1) * HW * 0.5f; *sy = (1 - vdot(d, cam.up) / z * FOCAL_Y) * VH * 0.5f;
    return 1;
}
static const char *piece_kind(const TPiece *p)
{
    switch (p->at & 15) {
    case 0: return "Straight";
    case 10: return "Diagonal";
    case 1: case 3: return "Bend 45";
    case 4: return "Bend 45, diagonal end";
    default: return "Bend 90, half";
    }
}
/* keys in yellow, what they do in grey: {key, words, key, words, ..., NULL}, centred at y */
static void ed_keyline(float y, const char *const *kw, float sc)
{
    float w = 0;
    for (int k = 0; kw[k]; k += 2) w += text_width(kw[k], sc) + 2.5f + text_width(kw[k + 1], sc) + 8;
    float x = (HW - w + 8) / 2;
    for (int k = 0; kw[k]; k += 2) {
        hud_text_raw(x, y, kw[k], sc, 0xF0D040); x += text_width(kw[k], sc) + 2.5f;
        hud_text_raw(x, y, kw[k + 1], sc, 0xC8C8D0); x += text_width(kw[k + 1], sc) + 8;
    }
}
static void ed_draw_list(void)
{
    float w = 220, x = (HW - w) / 2, y = 30, h = VH - 60;
    hud_a = 235; hud_rect(x - 6, y - 6, w + 12, h + 12, 0x18181C); hud_a = 255;
    hud_text(x, y, "OPEN A TRACK", 1.5f, 0xF0C040);
    hud_text(x, y + 14, "Originals open as a copy", 1, 0x9C9CA4);
    int vis = (int)((h - 34) / LIST_GAP), first = ed_list - vis + 1 > 0 ? ed_list - vis + 1 : 0;
    for (int r = 0; r < vis && first + r < ntrk; r++) draw_bar(x, y + 28 + r * LIST_GAP, w, trk[first + r].name, first + r == ed_list);
}
static void ed_draw_help(void)
{
    static const char *const help[][2] = {
        {"FLYING", 0}, {"W A S D", "move"}, {"Q E", "down, up"}, {"SHIFT", "faster"}, {"RIGHT MOUSE / SPACE", "hold to look round"},
        {"WHEEL", "forward, back"}, {"V", "look at the selection"}, {"G", "fly along the road"}, {0, 0},
        {"BUILDING", 0}, {"UP", "add a straight"}, {"LEFT RIGHT", "add a 45 bend"}, {"SHIFT+LEFT RIGHT", "add a 90 bend"},
        {"CLICK A GHOST", "add it"}, {"C", "close the loop"}, {"DEL", "remove the selected pieces"}, {0, 0},
        {"SELECTING", 0}, {"CLICK", "select a piece"}, {"SHIFT+CLICK", "select a run"}, {"TAB / SHIFT+TAB", "next, previous"},
        {"PAGE UP/DOWN", "previous, next (SHIFT: run)"}, {"ESC", "back to one piece"}, {0, 0},
        {"SHAPING", 0}, {"R F", "raise, lower (SHIFT: 2 m)"}, {", .", "bank left, right"}, {"1-6", "smooth linear hump kicker step pit"},
        {"- +", "smaller, bigger hump/kicker/pit"}, {"L", "start line here"}, {0, 0},
        {"TRACK", 0}, {"ALT+ARROWS", "move the whole track"}, {"ALT+R", "turn it (SHIFT: the other way)"}, {"T", "test drive"},
        {"J", "can the jumps be made?"}, {"CTRL+Z / Y", "undo, redo"}, {"CTRL+S / O", "save, open"}, {"F4  N", "rename, new"}};
    float w = 400, x = (HW - w) / 2, y = 18, cw = w / 2;
    hud_a = 248; hud_rect(x - 8, y - 8, w + 16, VH - 44, 0x141418); hud_a = 255;
    float cy = y, cx = x;
    for (int k = 0; k < (int)(sizeof help / sizeof help[0]); k++) {
        if (!help[k][0]) { cy += 5; if (cy > VH - 70 && cx == x) { cx = x + cw; cy = y; } continue; }
        if (!help[k][1]) { hud_text_raw(cx, cy, help[k][0], 1, 0xF08A28); cy += 10; continue; }
        hud_text_raw(cx + 4, cy, help[k][0], 0.85f, 0xF0D040);
        hud_text_raw(cx + 4 + text_width(help[k][0], 0.85f) + 3, cy, help[k][1], 0.85f, 0xC8C8D0);
        cy += 8.5f;
    }
    hud_text_raw(x + w - text_width("H OR ESC CLOSES THIS", 0.85f), VH - 44, "H OR ESC CLOSES THIS", 0.85f, 0x9C9CA4);
}
static void ed_draw(void)
{
    char b[128];
    if (ed_mode == EM_FLY) {
        hud_a = 190; hud_rect(0, 0, HW, 20, 0x08080A); hud_rect(0, VH - 17, HW, 17, 0x08080A); hud_a = 255;
        snprintf(b, sizeof b, "%s - FLY ALONG", ed.name);
        hud_text(8, 5, b, 1.5f, 0xF08A28);
        if (nseg) {
            int k = (int)fly_u % nseg;
            snprintf(b, sizeof b, "PIECE %d   HEIGHT %.1f M   %+.0f KM/H", seg_sec[k] + 1, seg[k].c.y, fly_v * 3.6f);
            hud_text(HW - 8 - text_width(b, 1.2f), 6, b, 1.2f, 0xFFFFFF);
        }
        static const char *const kw[] = {"UP/DOWN", "speed (below 0: backwards)", "SPACE", "stop", "LEFT/RIGHT", "piece before/after", "G/ESC", "back", NULL};
        ed_keyline(VH - 12, kw, 0.9f);
        return;
    }
    Move mv[8]; int n = ed_help ? 0 : ed_ghosts(mv);            /* the ghosts' keys, over their ends */
    for (int g = 0; g < n && ed_mode == EM_EDIT; g++) {
        V3 o[2]; const TPiece *lp = &mv[g].p[mv[g].n - 1];
        piece_world(lp, -1, pc_ns(lp), o);
        float sx, sy;
        if (!ed_proj(vadd(vmul(vadd(o[0], o[1]), 0.5f), V(0, 3, 0)), &sx, &sy)) continue;
        const char *lab = mv[g].turn == 0 ? "UP" : mv[g].turn == 45 ? "LEFT" : mv[g].turn == -45 ? "RIGHT" : mv[g].turn == 90 ? "SHIFT+LEFT" : "SHIFT+RIGHT";
        float tw = text_width(lab, 0.8f);
        hud_a = 200; hud_rect(sx - tw / 2 - 2, sy - 4.5f, tw + 4, 9.5f, 0x101014); hud_a = 255;
        hud_text_raw(sx - tw / 2, sy - 2.8f, lab, 0.8f, 0xFFFFFF);
    }
    /* the panel: the track, the selection and the keys that change it */
    int closed = ed_closed(), lo = ed_lo(), hi = ed_hi();
    float pw = 150, px = HW - pw - 6, y = 6;
    if (ed_help) { ed_draw_help(); goto keys; }
    hud_a = 185; hud_rect(px - 4, y - 3, pw + 8, ed.np ? 61 : 24, 0x101014); hud_a = 255;
    snprintf(b, sizeof b, "%s%s", ed.name, ed_dirty ? " *" : "");
    hud_text(px, y, b, 1.2f, 0xFFFFFF); y += 11;
    snprintf(b, sizeof b, "%d piece%s, %s", ed.np, ed.np == 1 ? "" : "s", closed ? "loop closed" : "open - the blue ghosts fit on");
    hud_text_raw(px, y, b, 0.8f, closed ? 0x60FF60 : 0xFFB060); y += 10;
    if (ed.np) {
        const TPiece *pc = &ed.p[ed_sel];
        int jl[MAXPC + 1], nj = ed_joints(jl), same_b = 1, same_s = 1;
        float c, bk, cmin = 1e9f, cmax = -1e9f, b0 = 0;
        for (int k = 0; k < nj; k++) {
            joint_get(jl[k], &c, &bk);
            cmin = fminf(cmin, c); cmax = fmaxf(cmax, c);
            if (!k) b0 = bk; else same_b &= fabsf(bk - b0) < 0.05f;
        }
        for (int p = lo; p <= hi; p++) same_s &= ed.p[p].shape == pc->shape && ed.p[p].amp == pc->amp;
        if (lo == hi) snprintf(b, sizeof b, "Piece %d: %s%s", ed_sel + 1, piece_kind(pc), ed_sel == ed.start ? " (start line)" : "");
        else snprintf(b, sizeof b, "Pieces %d-%d: %d selected", lo + 1, hi + 1, hi - lo + 1);
        hud_text_raw(px, y, b, 0.85f, 0x40E0FF); y += 9;
        char row[3][64];
        if (lo == hi) snprintf(row[0], sizeof row[0], "Height at its end (pole)  %.1f m", cmax);
        else if (cmax - cmin < 0.05f) snprintf(row[0], sizeof row[0], "Height, all of it  %.1f m", cmax);
        else snprintf(row[0], sizeof row[0], "Height, all of it  %.1f - %.1f m", cmin, cmax);
        if (!same_b) snprintf(row[1], sizeof row[1], "Banking  mixed");
        else if (fabsf(b0) < 0.05f) snprintf(row[1], sizeof row[1], "Banking  level");
        else snprintf(row[1], sizeof row[1], "Banking  %.2f m to the %s", fabsf(b0), b0 > 0 ? "right" : "left");
        if (!same_s) snprintf(row[2], sizeof row[2], "Shape  mixed");
        else if (pc->shape == SH_HUMP || pc->shape == SH_KICKER || pc->shape == SH_PIT) snprintf(row[2], sizeof row[2], "Shape  %s %.1f m", SHAPE_NAME[pc->shape], pc->amp);
        else snprintf(row[2], sizeof row[2], "Shape  %s", SHAPE_NAME[pc->shape]);
        static const char *const keys[3] = {"R F", ", .", "1-6  - +"};
        for (int r = 0; r < 3; r++, y += 8.5f) {
            hud_text_raw(px, y, row[r], 0.8f, 0xE8E8EC);
            hud_text_raw(px + pw - text_width(keys[r], 0.8f), y, keys[r], 0.8f, 0xF0D040);
        }
    }
    /* the main keys at the bottom, all of them with H */
keys:
    hud_a = 185; hud_rect(0, VH - 25, HW, 25, 0x101014); hud_a = 255;
    static const char *const k1[] = {"W A S D", "fly", "Q E", "down/up", "RIGHT MOUSE/SPACE", "look", "WHEEL", "forward", "V", "to the selection", "G", "fly along", "T", "test drive", NULL};
    static const char *const k2[] = {"UP LEFT RIGHT", "add (SHIFT: 90)", "CLICK", "select, add a ghost", "DEL", "remove", "C", "close", "CTRL+Z", "undo", "CTRL+S", "save", "H", "all keys", NULL};
    ed_keyline(VH - 22, k1, 0.85f); ed_keyline(VH - 11, k2, 0.85f);
    if (ed_mode == EM_LIST) ed_draw_list();
    if (ed_mode == EM_NAME) {
        float w = 200, x = (HW - w) / 2, yy = VH / 2 - 30;
        hud_rect(x - 6, yy - 6, w + 12, 54, 0x202024);
        hud_text(x, yy, "TRACK NAME:", 1.5f, 0xF0C040);
        hud_rect(x, yy + 16, w, 18, 0x56565A); hud_rect(x + 1, yy + 17, w - 2, 16, 0x101014);
        snprintf(b, sizeof b, "%s%s", ed_name, (SDL_GetTicks() / 400) & 1 ? "_" : "");
        hud_text_raw(x + 5, yy + 21, b, 1.3f, 0xFFE060);
        hud_text_raw(x, yy + 38, "ENTER SAVE   ESC CANCEL", 1, 0x9C9CA4);
    }
    if (ed_mode == EM_NEW || ed_mode == EM_QUIT) {
        hud_a = 225; hud_rect(0, VH / 2 - 6, HW, 7 * 1.5f + 12, 0x08080A); hud_a = 255;
        hud_text_c(VH / 2, ed_mode == EM_NEW ? "N AGAIN: START A NEW TRACK (CTRL+Z BRINGS THIS ONE BACK)" : "UNSAVED CHANGES - ESC/B AGAIN TO LEAVE, CTRL+S TO SAVE", 1.5f, 0xFF8070);
    }
    if (ed_msg_t > 0) {
        float tw = text_width(ed_msg, 1.1f);
        hud_a = 200; hud_rect((HW - tw) / 2 - 5, VH - 44, tw + 10, 13, 0x101014); hud_a = 255;
        hud_text((HW - tw) / 2, VH - 41, ed_msg, 1.1f, 0x60FF60);
    }
}

/* ---- input ---- */
static void ed_text(const char *t)
{
    if (ed_mode != EM_NAME) return;
    for (; *t; t++) {
        size_t n = strlen(ed_name);
        if (n + 1 < sizeof ed_name && (strchr(NAME_CHARS, toupper((unsigned char)*t)) || (*t == ' ' && n))) { ed_name[n] = (char)toupper((unsigned char)*t); ed_name[n + 1] = 0; }
    }
}
static void ed_name_begin(void)
{
    snprintf(ed_name, sizeof ed_name, "%s", strcmp(ed.name, "NEW TRACK") ? ed.name : "");
    ed_mode = EM_NAME; fc_set_look(0);
    if (win) SDL_StartTextInput(win);
}
static void ed_save_key(void) { if (!strcmp(ed.name, "NEW TRACK")) { if (ed_ready()) ed_name_begin(); } else ed_save(); }
static void ed_leave(void) { fc_set_look(0); ed_dirty = 0; menu_go(MS_MAIN); }
static void ed_fly_key(SDL_Keycode k)
{
    switch (k) {
    case SDLK_ESCAPE: case SDLK_G: ed_fly(0); break;
    case SDLK_UP: fly_v = fminf(fly_v + 5, 70); break;
    case SDLK_DOWN: fly_v = fmaxf(fly_v - 5, -70); break;
    case SDLK_SPACE: if (fly_v != 0) { fly_was = fly_v; fly_v = 0; } else fly_v = fly_was ? fly_was : 25; break;
    case SDLK_LEFT: case SDLK_RIGHT: {                          /* to the start of the piece before or after */
        int p = seg_sec[(int)fly_u % nseg], here = ed_seg_of(p), q;
        if (k == SDLK_LEFT && here >= 0 && here < (int)fly_u - (int)(5 / ds)) q = here;   /* first back to this one's start */
        else q = ed_seg_of((p + (k == SDLK_LEFT ? ed.np - 1 : 1)) % ed.np);
        if (q >= 0) fly_u = (float)q;
        break;
    }
    default: break;
    }
}
static void ed_key(SDL_Keycode k, SDL_Keymod mod)
{
    int sh = (mod & SDL_KMOD_SHIFT) != 0, cmd = (mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0, alt = (mod & SDL_KMOD_ALT) != 0;
    if (ed_mode == EM_LIST) {
        if (k == SDLK_UP) ed_list = (ed_list + ntrk - 1) % ntrk;
        else if (k == SDLK_DOWN) ed_list = (ed_list + 1) % ntrk;
        else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) { ed_import(ed_list); ed_mode = EM_EDIT; ed_view_sel(); }
        else if (k == SDLK_ESCAPE) ed_mode = EM_EDIT;
        return;
    }
    if (ed_mode == EM_NAME) {
        if (k == SDLK_BACKSPACE && ed_name[0]) ed_name[strlen(ed_name) - 1] = 0;
        else if (k == SDLK_ESCAPE) { ed_mode = EM_EDIT; if (win) SDL_StopTextInput(win); }
        else if ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && ed_name[0]) {
            int other = track_find(ed_name);
            if (other >= 0 && other < SCR_NTRACKS) { ed_note("That name belongs to an original track"); return; }
            snprintf(ed.name, sizeof ed.name, "%s", ed_name);
            ed_mode = EM_EDIT; if (win) SDL_StopTextInput(win);
            ed_save();
        }
        return;
    }
    if (ed_mode == EM_FLY) { ed_fly_key(k); return; }
    if (ed_mode == EM_NEW) { ed_mode = EM_EDIT; if (k == SDLK_N) { ed_push(0); ed_new(); ed_note("New track - CTRL+Z brings the old one back"); } return; }
    if (ed_mode == EM_QUIT) { ed_mode = EM_EDIT; if (k == SDLK_ESCAPE) { ed_leave(); return; } }
    TPiece *pc = &ed.p[ed_sel];
    int tweak = 0;
    if (cmd) switch (k) {
        case SDLK_Z: ed_undo(sh); break;
        case SDLK_Y: ed_undo(1); break;
        case SDLK_S: ed_save_key(); break;
        case SDLK_O: ed_list = 0; ed_mode = EM_LIST; break;
        case SDLK_N: ed_mode = EM_NEW; break;
        default: break;
    } else if (alt) switch (k) {
        case SDLK_UP: ed_move(0, 1, 0); break;                  /* along the grid as seen from the south, like the map was */
        case SDLK_DOWN: ed_move(0, -1, 0); break;
        case SDLK_LEFT: ed_move(-1, 0, 0); break;
        case SDLK_RIGHT: ed_move(1, 0, 0); break;
        case SDLK_R: ed_move(0, 0, sh ? 3 : 1); break;
        default: break;
    } else switch (k) {
    case SDLK_UP: ed_extend(0); break;
    case SDLK_LEFT: ed_extend(sh ? 90 : 45); break;
    case SDLK_RIGHT: ed_extend(sh ? -90 : -45); break;
    case SDLK_BACKSPACE: case SDLK_DELETE: ed_remove(); break;
    case SDLK_C: ed_autoclose(); break;
    case SDLK_TAB: ed_select((ed_sel + (sh ? ed.np - 1 : 1)) % ed.np, 0); break;
    case SDLK_PAGEUP: case SDLK_PAGEDOWN: {                     /* a run stops at the ends */
        int d = k == SDLK_PAGEUP ? -1 : 1;
        if (!sh || (ed_sel + d >= 0 && ed_sel + d < ed.np)) ed_select((ed_sel + d + ed.np) % ed.np, sh);
        break;
    }
    case SDLK_HOME: ed_select(0, sh); break;
    case SDLK_END: ed_select(ed.np - 1, sh); break;
    case SDLK_R: ed_tweak_joints(TW_HEIGHT, sh ? 2.f : 0.5f, 0); tweak = 1; break;
    case SDLK_F: ed_tweak_joints(TW_HEIGHT, sh ? -2.f : -0.5f, 0); tweak = 1; break;
    case SDLK_COMMA: ed_tweak_joints(TW_BANK, 0, -0.25f); tweak = 1; break;
    case SDLK_PERIOD: ed_tweak_joints(TW_BANK, 0, 0.25f); tweak = 1; break;
    case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4: case SDLK_5: case SDLK_6:
        ed_push(0);
        for (int p = ed_lo(); p <= ed_hi(); p++) {
            TPiece *q = &ed.p[p];
            q->shape = (unsigned char)(SH_SMOOTH + (k - SDLK_1));
            if (q->amp == 0) q->amp = q->shape == SH_HUMP ? 2.f : q->shape == SH_KICKER ? 3.f : q->shape == SH_PIT ? 6.f : 0.f;
            regen(p);
        }
        break;
    case SDLK_MINUS: case SDLK_KP_MINUS: case SDLK_EQUALS: case SDLK_PLUS: case SDLK_KP_PLUS:
        ed_push(TW_SIZE); tweak = 1;
        for (int p = ed_lo(); p <= ed_hi(); p++) {
            TPiece *q = &ed.p[p];
            q->amp = k == SDLK_MINUS || k == SDLK_KP_MINUS ? q->amp - 0.5f : fminf(q->amp + 0.5f, 30.f);
            if (q->shape != SH_HUMP && q->amp < 0) q->amp = 0;
            regen(p);
        }
        break;
    case SDLK_L:
        if ((pc->at & 15) != 0) ed_note("The start line goes on a straight piece");
        else if (ed.start != ed_sel) { ed_push(0); ed.start = ed_sel; }
        break;
    case SDLK_V: ed_view_sel(); break;
    case SDLK_G: ed_fly(1); break;
    case SDLK_H: ed_help = !ed_help; break;
    case SDLK_T: fc_set_look(0); ed_test(); break;
    case SDLK_J: ed_check_jumps(); break;
    case SDLK_F2: ed_save_key(); break;
    case SDLK_F3: ed_list = 0; ed_mode = EM_LIST; break;
    case SDLK_F4: ed_name_begin(); break;
    case SDLK_N: if (ed_dirty) ed_mode = EM_NEW; else { ed_push(0); ed_new(); } break;
    case SDLK_ESCAPE:
        if (ed_help) ed_help = 0;
        else if (ed_lo() != ed_hi()) ed_select(ed_sel, 0);          /* first the run goes back to one piece */
        else if (ed_dirty) ed_mode = EM_QUIT;
        else ed_leave();
        break;
    default: break;                                             /* W A S D Q E Space: the camera (ed_tick) */
    }
    if (!tweak) ed_tweak = -1;
}
static void ed_button(float hx, float hy, int b, int down)
{
    if (b == SDL_BUTTON_RIGHT) { fc_set_look(down && ed_mode == EM_EDIT); return; }
    if (b != SDL_BUTTON_LEFT || !down || ed_mode != EM_EDIT || !ed.np) return;
    ed_tweak = -1; ed_hx = hx; ed_hy = hy;
    Pick k = ed_pick(hx, hy);
    if (k.kind == 2) ed_extend(k.turn);
    else if (k.kind == 1) ed_select(k.p, (SDL_GetModState() & SDL_KMOD_SHIFT) != 0);
}
static void ed_motion(float hx, float hy, float xr, float yr)
{
    if (fc_look) { fc_yaw -= xr * 0.004f; fc_pitch = clampf(fc_pitch - yr * 0.004f, -1.5f, 1.5f); return; }
    ed_hx = hx; ed_hy = hy;
}
static void ed_wheel(float w)
{
    if (ed_mode != EM_EDIT) return;
    fc_pos = vadd(fc_pos, vmul(fc_dir(), w * fmaxf(6.f, fc_pos.y * 0.12f))); fc_clamp();
}
static void ed_open(void)
{
    if (!ed.np) ed_new();
    memset(cars, 0, sizeof cars); npart = 0;
    ed_mode = EM_EDIT; state = ST_EDIT; paused = 0; fc_look = fc_space = 0;
    if (win) SDL_StopTextInput(win);
    ed_build_scene(); ed_build_mesh();
    static int seen;
    if (!seen++) ed_view_sel();                                 /* the first time: close to where the track starts */
}
static void ed_return(void) { ed_open(); ed_jumps_note(); }

/* debug (--ui-shot): fly:X,Y,Z,YAW,PITCH puts the camera there; the track checked by the builder's own rules */
static void ed_fly_to(const float *v) { fc_pos = V(v[0], v[1], v[2]); fc_yaw = v[3] * PI_F / 180; fc_pitch = v[4] * PI_F / 180; }
static void ed_report(void)
{
    int joins = 1, clash = 0, cl = ed_closed(), jl[MAXPC + 1], nj = ed_joints(jl);
    for (int p = 0; p + 1 < ed.np; p++) { int e[2][2], s0[2][2]; pc_endxs(&ed.p[p], e); pc_xs(&ed.p[p + 1], 0, s0); joins &= xs_eq(e, s0); }
    for (int a = 0; a < ed.np; a++) for (int b = a + 2; b < ed.np; b++) if (!(cl && a == 0 && b == ed.np - 1) && pc_clash(&ed.p[a], &ed.p[b])) clash++;
    printf("editor: %d pieces, %s, start %d, sel %d-%d, joins %s, %d clashes, undo %d redo %d, mode %d, mesh %d tris\n", ed.np, cl ? "closed" : "open",
           ed.start, ed_lo(), ed_hi(), joins ? "ok" : "BROKEN", clash, ed_un_n, ed_re_n, ed_mode, tkb.n / 3);
    printf("editor: joints");
    for (int k = 0; k < nj; k++) { float c, bk; joint_get(jl[k], &c, &bk); printf(" %d:%.1f/%+.2f", jl[k], c, bk); }
    printf("  camera %.0f,%.0f,%.0f\n", fc_pos.x, fc_pos.y, fc_pos.z);
    Move mv[8]; int n = ed_ghosts(mv);
    ed_cam(0);
    for (int g = 0; g < n; g++) {
        float sx, sy; V3 m = piece_mid(&mv[g].p[mv[g].n - 1], -1);
        if (ed_proj(m, &sx, &sy)) printf("editor: ghost %+d at %.0f,%.0f\n", mv[g].turn, sx, sy);
    }
}
/* debug: the originals' pieces keep clear of each other by the builder's own rule (--check-tracks) */
static void ed_check_originals(void)
{
    for (int t = 0; t < SCR_NTRACKS; t++) {
        const TrackDef *td = &trk[t]; int bad = 0;
        for (int a = 0; a < td->np; a++) for (int b = a + 2; b < td->np; b++)
            if (!(a == 0 && b == td->np - 1) && pc_clash(&td->p[a], &td->p[b])) { bad++; if (bad < 4) printf("  %s: pieces %d and %d share a cube\n", td->name, a, b); }
        int ok = 1;
        for (int p = 0; p < td->np; p++) {
            int e[2][2], s[2][2]; pc_endxs(&td->p[p], e); pc_xs(&td->p[(p + 1) % td->np], 0, s);
            ok &= xs_eq(e, s);
        }
        printf("%-16s %3d pieces, joins %s, %d clashes\n", td->name, td->np, ok ? "ok" : "BROKEN", bad);
    }
}
