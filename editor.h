/* editor.h - the track builder: the original's 16x16 grid seen from above. The track grows from its open end with the
 * original's pieces (only those that join on and keep clear of the other pieces' cubes are offered), C closes the loop
 * the shortest way round. Heights are set where two pieces meet; each piece has a shape between its two ends.
 * Saves to tracks/NAME.trk, T test-drives. Included by scr_main.h */
enum { EM_EDIT, EM_LIST, EM_NAME, EM_NEW, EM_QUIT };
static TrackDef ed;
static int ed_sel, ed_dirty, ed_mode, ed_list, ed_endh[2];
static int ed_bad[MAXPC], ed_nbad;                     /* pieces before jumps the AI's planner can't get over */
static char ed_msg[96], ed_name[32];
static float ed_msg_t, ed_mx, ed_my, ed_cs;
static const unsigned char TPL_USE[7] = {0, 1, 3, 4, 6, 7, 10};
typedef struct { int n, turn; TPiece p[3]; } Move;

static void ed_note(const char *s) { snprintf(ed_msg, sizeof ed_msg, "%s", s); ed_msg_t = 3.f; }
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

/* ---- building ---- */
static void ed_new(void)
{
    memset(&ed, 0, sizeof ed);
    snprintf(ed.name, sizeof ed.name, "NEW TRACK"); ed.orig = -1; ed.np = 1;
    ed.p[0].cx = 7; ed.p[0].cz = 2; ed.p[0].at = 0; ed.p[0].shape = SH_SMOOTH;
    ed_endh[0] = ed_endh[1] = 0; ed_sel = 0; ed_dirty = 0;
}
static void ed_append(const Move *m, int closes)
{
    for (int k = 0; k < m->n; k++) {
        TPiece p = m->p[k];
        p.shape = SH_SMOOTH; p.amp = 0;
        for (int e = 0; e < 2; e++) for (int i = 0; i < 14; i++) p.h[e][i] = ed_endh[e];
        ed.p[ed.np++] = p;
    }
    if (closes) regen(ed.np - 1);
    ed_sel = ed.np - 1; ed_dirty = 1;
}
static void ed_extend(int turn)
{
    if (ed_closed()) { ed_note("The loop is closed - BACKSPACE opens it again"); return; }
    int e[2][2], closes; open_end(e);
    Move mv[8]; int n = moves(e, mv);
    for (int k = 0; k < n; k++) {
        if (mv[k].turn != turn) continue;
        if (!move_ok(&mv[k], &closes)) { ed_note(ed.np + mv[k].n > MAXPC ? "100 pieces is the most a track can have" : "No room there: the grid ends or another piece is in the way"); return; }
        ed_append(&mv[k], closes);
        if (closes) ed_note("Loop closed! T to test drive, F2 to save");
        return;
    }
    ed_note(abs(turn) == 90 ? "90 degree bends start from a heading along the grid" : "That piece does not join on here");
}
static void ed_remove(void)
{
    if (ed.np <= 1) { ed_note("The first piece stays"); return; }
    const TPiece *l = &ed.p[ed.np - 1];
    ed_endh[0] = l->h[0][0]; ed_endh[1] = l->h[1][0];
    ed.np--;
    if (ed.start >= ed.np) ed.start = 0;
    if (ed_sel >= ed.np) ed_sel = ed.np - 1;
    ed_dirty = 1;
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
                ed_sel = ed.np - 1; ed_dirty = 1;
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
    ed_note("No way back to the start from here - remove a few pieces (BACKSPACE) and try again");
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
    trk[ED_SLOT] = ed; trk[ED_SLOT].orig = -1;
    build_track_geometry(ED_SLOT); cur_track = -1;
    plan_need();
    ed_jumps_note();
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
    ed = trk[i];
    if (ed.orig >= 0) {
        track_xsecs(&ed);
        for (int q = 0, n = 0; q < ed.np; q++) {
            TPiece *pc = &ed.p[(ed.start + 1 + q) % ed.np];
            for (int r = 0; r < pc_ns(pc); r++, n++) for (int e = 0; e < 2; e++) pc->h[e][r] = tx_ey[n][e];
        }
        char nm[32]; snprintf(nm, sizeof nm, "MY %s", ed.name); snprintf(ed.name, sizeof ed.name, "%s", nm);
        ed.orig = -1; ed.file[0] = 0;
    }
    ed_endh[0] = ed.p[0].h[0][0]; ed_endh[1] = ed.p[0].h[1][0];
    ed_sel = 0; ed_dirty = ed.file[0] == 0;
}
static void ed_open(void)
{
    if (!ed.np) ed_new();
    memset(cars, 0, sizeof cars); npart = 0;
    ed_mode = EM_EDIT; state = ST_EDIT; paused = 0;
    if (win) SDL_StopTextInput(win);
}
static void ed_return(void) { ed_open(); ed_jumps_note(); }

/* ---- drawing ---- */
static void ed_xy(float x, float z, float *sx, float *sy) { *sx = ed_mx + x / 2048.f * ed_cs; *sy = ed_my + (16 - z / 2048.f) * ed_cs; }
static float ed_hmax = 30;                              /* the map's colours run grey - yellow - red up to the track's top */
static uint32_t hcol(float m)
{
    float t = clampf(m / ed_hmax, 0, 1);
    return t < 0.5f ? mixcol(0x8C8C96, 0xF0D040, t * 2) : mixcol(0xF0D040, 0xE04020, t * 2 - 1);
}
static void ed_xs_line(const int a[2][2], float w, uint32_t col)
{
    float x0, y0, x1, y1; ed_xy((float)a[0][0], (float)a[0][1], &x0, &y0); ed_xy((float)a[1][0], (float)a[1][1], &x1, &y1);
    hud_line(x0, y0, x1, y1, w, col);
}
static void ed_draw_piece(int p, int sel)
{
    const TPiece *pc = &ed.p[p];
    int ns = pc_ns(pc), a[2][2], b[2][2];
    pc_xs(pc, 0, a);
    for (int i = 0; i < ns; i++) {
        pc_xs(pc, i + 1, b);
        float P[4][2];
        ed_xy((float)a[0][0], (float)a[0][1], &P[0][0], &P[0][1]); ed_xy((float)a[1][0], (float)a[1][1], &P[1][0], &P[1][1]);
        ed_xy((float)b[1][0], (float)b[1][1], &P[2][0], &P[2][1]); ed_xy((float)b[0][0], (float)b[0][1], &P[3][0], &P[3][1]);
        int n0 = i + 1 < ns ? pc->h[0][i + 1] : *joint_ptr(p + 1, 0), n1 = i + 1 < ns ? pc->h[1][i + 1] : *joint_ptr(p + 1, 1);
        uint32_t col = hcol((pc->h[0][i] + pc->h[1][i] + n0 + n1) * 0.25f / U_PER_M);
        if (sel) col = mixcol(col, 0xFFFFFF, 0.45f);
        hud_quad(P[0][0], P[0][1], P[1][0], P[1][1], P[2][0], P[2][1], P[3][0], P[3][1], col);
        if (abs(n0 - pc->h[0][i]) >= 640 || abs(n1 - pc->h[1][i]) >= 640) ed_xs_line(b, 1.2f, 0xFF2020);   /* a sheer face */
        memcpy(a, b, sizeof a);
    }
}
static void ed_outline(const TPiece *pc, float w, uint32_t col)
{
    int ns = pc_ns(pc), a[2][2], b[2][2];
    pc_xs(pc, 0, a);
    for (int i = 0; i < ns; i++) {
        pc_xs(pc, i + 1, b);
        for (int e = 0; e < 2; e++) {
            float x0, y0, x1, y1; ed_xy((float)a[e][0], (float)a[e][1], &x0, &y0); ed_xy((float)b[e][0], (float)b[e][1], &x1, &y1);
            hud_line(x0, y0, x1, y1, w, col);
        }
        memcpy(a, b, sizeof a);
    }
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
static void ed_profile(float x, float y, float w, float h)
{
    hud_rect(x, y, w, h, 0x18181C);
    int tot = 0;
    for (int p = 0; p < ed.np; p++) tot += pc_ns(&ed.p[p]);
    if (!tot) return;
    float bw = w / tot, hm = 1;
    for (int p = 0; p < ed.np; p++) for (int i = 0; i < pc_ns(&ed.p[p]); i++) hm = fmaxf(hm, (ed.p[p].h[0][i] + ed.p[p].h[1][i]) * 0.5f / U_PER_M);
    hm = fmaxf(10.f, ceilf(hm / 10.f) * 10.f);
    for (int m = 10; m < hm; m += 10) hud_rect(x, y + h - h * m / hm, w, 0.5f, 0x34343C);
    float bx = x;
    for (int p = 0; p < ed.np; p++)
        for (int i = 0; i < pc_ns(&ed.p[p]); i++, bx += bw) {
            float hh = (ed.p[p].h[0][i] + ed.p[p].h[1][i]) * 0.5f / U_PER_M, bh = fmaxf(0.6f, h * hh / hm);
            uint32_t col = hcol(hh);
            if (p == ed_sel) col = mixcol(col, 0xFFFFFF, 0.5f);
            hud_rect(bx, y + h - bh, fmaxf(bw, 0.6f), bh, col);
        }
    char b[32]; snprintf(b, sizeof b, "%.0f M", hm);
    hud_text_raw(x + 2, y + 2, b, 0.8f, 0x9C9CA4);
}
static void ed_draw_list(void)
{
    float x = ed_mx + 8, w = 16 * ed_cs - 16, y = ed_my + 8;
    hud_rect(x - 4, y - 4, w + 8, 16 * ed_cs - 8, 0x202024);
    hud_text(x, y, "OPEN A TRACK", 1.5f, 0xF0C040);
    hud_text(x, y + 14, "Originals open as a copy", 1, 0x9C9CA4);
    int vis = (int)((16 * ed_cs - 50) / 15), first = ed_list - vis + 1 > 0 ? ed_list - vis + 1 : 0;
    for (int r = 0; r < vis && first + r < ntrk; r++)
        draw_bar(x, y + 28 + r * 15, w, 13, trk[first + r].name, first + r == ed_list, 0.95f);
}
static void ed_draw(void)
{
    hud_rect(0, 0, HW, VH, 0x141418);
    float S = fminf(VH - 28, HW - 230);
    ed_cs = S / 16; ed_mx = 8; ed_my = 8;
    hud_rect(ed_mx, ed_my, S, S, 0x24402A);
    for (int k = 0; k <= 16; k++) {
        hud_rect(ed_mx + k * ed_cs - 0.25f, ed_my, 0.5f, S, 0x36583C);
        hud_rect(ed_mx, ed_my + k * ed_cs - 0.25f, S, 0.5f, 0x36583C);
    }
    ed_hmax = 30;
    for (int p = 0; p < ed.np; p++) for (int i = 0; i < pc_ns(&ed.p[p]); i++) ed_hmax = fmaxf(ed_hmax, (ed.p[p].h[0][i] + ed.p[p].h[1][i]) * 0.5f / U_PER_M);
    for (int p = 0; p < ed.np; p++) ed_draw_piece(p, p == ed_sel);
    for (int k = 0; k < ed_nbad; k++) if (ed_bad[k] < ed.np) ed_outline(&ed.p[ed_bad[k]], 0.8f, 0xFF3030);
    if (ed.np) ed_outline(&ed.p[ed_sel], 0.6f, 0x40E0FF);
    if (ed.np) {                                                    /* the start line: chequers across the start piece's end */
        int e[2][2]; pc_endxs(&ed.p[ed.start], e);
        float x0, y0, x1, y1; ed_xy((float)e[0][0], (float)e[0][1], &x0, &y0); ed_xy((float)e[1][0], (float)e[1][1], &x1, &y1);
        for (int k = 0; k < 6; k++) hud_line(x0 + (x1 - x0) * k / 6, y0 + (y1 - y0) * k / 6, x0 + (x1 - x0) * (k + 1) / 6, y0 + (y1 - y0) * (k + 1) / 6, 1.6f, k & 1 ? 0x101010 : 0xF8F8F8);
    }
    int closed = ed_closed();
    if (ed.np && !closed && ed_mode == EM_EDIT) {                   /* what can come next, and the keys for it */
        int e[2][2], cl; open_end(e);
        Move mv[8]; int n = moves(e, mv);
        for (int k = 0; k < n; k++) {
            if (!move_ok(&mv[k], &cl)) continue;
            for (int j = 0; j < mv[k].n; j++) ed_outline(&mv[k].p[j], 0.45f, 0x90A8C0);
            int x[2][2]; pc_endxs(&mv[k].p[mv[k].n - 1], x);
            float sx, sy; ed_xy((x[0][0] + x[1][0]) * 0.5f, (x[0][1] + x[1][1]) * 0.5f, &sx, &sy);
            const char *lab = mv[k].turn == 0 ? "UP" : mv[k].turn == 45 ? "L" : mv[k].turn == -45 ? "R" : mv[k].turn == 90 ? "SH+L" : "SH+R";
            hud_text(sx - text_width(lab, 0.8f) / 2, sy - 3, lab, 0.8f, 0xFFFFFF);
        }
    }
    /* the panel */
    float px = ed_mx + S + 10, y = 8;
    char b[128];
    hud_text(px, y, "TRACK BUILDER", 2, 0xF08A28); y += 20;
    snprintf(b, sizeof b, "%s%s", ed.name, ed_dirty ? " *" : "");
    hud_text(px, y, b, 1.5f, 0xFFFFFF); y += 14;
    snprintf(b, sizeof b, "%d piece%s, %s", ed.np, ed.np == 1 ? "" : "s", closed ? "loop closed" : "open");
    hud_text(px, y, b, 1, closed ? 0x60FF60 : 0xFFB060); y += 14;
    if (ed.np) {
        const TPiece *pc = &ed.p[ed_sel];
        float c, bk; joint_get(ed_sel, &c, &bk);
        snprintf(b, sizeof b, "Piece %d: %s%s", ed_sel + 1, piece_kind(pc), ed_sel == ed.start ? " (START)" : "");
        hud_text(px, y, b, 1, 0x40E0FF); y += 11;
        snprintf(b, sizeof b, "Height %.1f m  bank %+.1f m", c, bk);
        hud_text(px, y, b, 1, 0xE8E8EC); y += 11;
        if (pc->shape == SH_HUMP || pc->shape == SH_KICKER || pc->shape == SH_PIT) snprintf(b, sizeof b, "Shape %s %.1f m", SHAPE_NAME[pc->shape], pc->amp);
        else snprintf(b, sizeof b, "Shape %s", SHAPE_NAME[pc->shape]);
        hud_text(px, y, b, 1, 0xE8E8EC); y += 15;
    }
    static const char *help[] = {"UP/LEFT/RIGHT: ADD PIECE", "SHIFT+LEFT/RIGHT: 90 BEND", "BACKSPACE: REMOVE  C: CLOSE LOOP",
                                 "TAB/CLICK: SELECT PIECE", "W/S: HEIGHT  Q/E: BANK", "1-6: SHAPE  -/=: SHAPE SIZE",
                                 "  1 SMOOTH 2 LINEAR 3 HUMP", "  4 KICKER 5 STEP 6 PIT", "F: START LINE  T: TEST DRIVE", "J: CAN THE JUMPS BE MADE?",
                                 "F2 SAVE F3 OPEN F4 NAME N NEW", "ESC: MAIN MENU"};
    for (int k = 0; k < (int)(sizeof help / sizeof help[0]); k++, y += 9) hud_text_raw(px, y, help[k], 1, 0xA8A8B0);
    y += 4;
    hud_text_raw(px, y, "HEIGHT PROFILE", 0.8f, 0x9C9CA4);
    ed_profile(px, y + 7, HW - px - 8, fmaxf(VH - y - 30, 20));
    if (ed_mode == EM_LIST) ed_draw_list();
    if (ed_mode == EM_NAME) {
        float w = 200, x = ed_mx + (S - w) / 2, yy = ed_my + S / 2 - 30;
        hud_rect(x - 6, yy - 6, w + 12, 54, 0x202024);
        hud_text(x, yy, "TRACK NAME:", 1.5f, 0xF0C040);
        hud_rect(x, yy + 16, w, 18, 0x56565A); hud_rect(x + 1, yy + 17, w - 2, 16, 0x101014);
        snprintf(b, sizeof b, "%s%s", ed_name, (SDL_GetTicks() / 400) & 1 ? "_" : "");
        hud_text_raw(x + 5, yy + 21, b, 1.3f, 0xFFE060);
        hud_text_raw(x, yy + 38, "ENTER SAVE   ESC CANCEL", 1, 0x9C9CA4);
    }
    if (ed_mode == EM_NEW) hud_text_c(VH / 2, "N AGAIN: START A NEW TRACK (UNSAVED CHANGES ARE LOST)", 1.5f, 0xFF8070);
    if (ed_mode == EM_QUIT) hud_text_c(VH / 2, "UNSAVED CHANGES - ESC AGAIN TO LEAVE, F2 TO SAVE", 1.5f, 0xFF8070);
    if (ed_msg_t > 0) hud_text(ed_mx, VH - 13, ed_msg, 1, 0x60FF60);
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
    ed_mode = EM_NAME;
    if (win) SDL_StartTextInput(win);
}
static void ed_key(SDL_Keycode k, SDL_Keymod mod)
{
    int sh = (mod & SDL_KMOD_SHIFT) != 0;
    if (ed_mode == EM_LIST) {
        if (k == SDLK_UP) ed_list = (ed_list + ntrk - 1) % ntrk;
        else if (k == SDLK_DOWN) ed_list = (ed_list + 1) % ntrk;
        else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) { ed_import(ed_list); ed_mode = EM_EDIT; }
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
    if (ed_mode == EM_NEW) { ed_mode = EM_EDIT; if (k == SDLK_N) ed_new(); return; }
    if (ed_mode == EM_QUIT) { ed_mode = EM_EDIT; if (k == SDLK_ESCAPE) { ed_dirty = 0; menu_go(MS_MAIN); return; } }
    float c = 0, bk = 0;
    if (ed.np) joint_get(ed_sel, &c, &bk);
    TPiece *pc = &ed.p[ed_sel];
    switch (k) {
    case SDLK_UP: ed_extend(0); break;
    case SDLK_LEFT: ed_extend(sh ? 90 : 45); break;
    case SDLK_RIGHT: ed_extend(sh ? -90 : -45); break;
    case SDLK_BACKSPACE: case SDLK_DELETE: ed_remove(); break;
    case SDLK_C: ed_autoclose(); break;
    case SDLK_TAB: ed_sel = (ed_sel + (sh ? ed.np - 1 : 1)) % ed.np; break;
    case SDLK_COMMA: ed_sel = (ed_sel + ed.np - 1) % ed.np; break;
    case SDLK_PERIOD: ed_sel = (ed_sel + 1) % ed.np; break;
    case SDLK_HOME: ed_sel = 0; break;
    case SDLK_END: ed_sel = ed.np - 1; break;
    case SDLK_W: case SDLK_PAGEUP: joint_set(ed_sel, c + (sh ? 2.f : 0.5f), bk); break;
    case SDLK_S: case SDLK_PAGEDOWN: joint_set(ed_sel, c - (sh ? 2.f : 0.5f), bk); break;
    case SDLK_Q: joint_set(ed_sel, c, bk + 0.25f); break;
    case SDLK_E: joint_set(ed_sel, c, bk - 0.25f); break;
    case SDLK_1: case SDLK_2: case SDLK_3: case SDLK_4: case SDLK_5: case SDLK_6:
        pc->shape = (unsigned char)(SH_SMOOTH + (k - SDLK_1));
        if (pc->amp == 0) pc->amp = pc->shape == SH_HUMP ? 2.f : pc->shape == SH_KICKER ? 3.f : pc->shape == SH_PIT ? 6.f : 0.f;
        regen(ed_sel); ed_dirty = 1; break;
    case SDLK_MINUS: case SDLK_KP_MINUS:
        pc->amp -= 0.5f; if (pc->shape != SH_HUMP && pc->amp < 0) pc->amp = 0; regen(ed_sel); ed_dirty = 1; break;
    case SDLK_EQUALS: case SDLK_PLUS: case SDLK_KP_PLUS:
        pc->amp = fminf(pc->amp + 0.5f, 30.f); regen(ed_sel); ed_dirty = 1; break;
    case SDLK_F:
        if ((pc->at & 15) != 0) ed_note("The start line goes on a straight piece");
        else { ed.start = ed_sel; ed_dirty = 1; }
        break;
    case SDLK_T: ed_test(); break;
    case SDLK_J: ed_check_jumps(); break;
    case SDLK_F2: if (!strcmp(ed.name, "NEW TRACK")) { if (ed_ready()) ed_name_begin(); } else ed_save(); break;
    case SDLK_F3: ed_list = 0; ed_mode = EM_LIST; break;
    case SDLK_F4: ed_name_begin(); break;
    case SDLK_N: ed_mode = EM_NEW; break;
    case SDLK_ESCAPE: if (ed_dirty) ed_mode = EM_QUIT; else menu_go(MS_MAIN); break;
    default: break;
    }
}
static void ed_mouse(float hx, float hy, int button, float wheel)
{
    if (ed_mode != EM_EDIT || !ed.np) return;
    if (wheel != 0) { float c, bk; joint_get(ed_sel, &c, &bk); joint_set(ed_sel, c + (wheel > 0 ? 0.5f : -0.5f), bk); return; }
    if (button != 1 || hx < ed_mx || hy < ed_my || hx > ed_mx + 16 * ed_cs || hy > ed_my + 16 * ed_cs) return;
    float best = ed_cs * 0.8f;
    for (int p = 0; p < ed.np; p++) {
        int ns = pc_ns(&ed.p[p]), a[2][2];
        for (int i = 0; i <= ns; i++) {
            pc_xs(&ed.p[p], i, a);
            float sx, sy; ed_xy((a[0][0] + a[1][0]) * 0.5f, (a[0][1] + a[1][1]) * 0.5f, &sx, &sy);
            float d = hypotf(sx - hx, sy - hy);
            if (d < best) { best = d; ed_sel = p; }
        }
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
