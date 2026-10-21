/* editor/hud.h - the builder's HUD */
#pragma once

/* ---- the HUD ---- */
static int ed_proj(V3 p, float *sx, float *sy)
{
    V3 d = vsub(p, cam.pos);
    float z = vdot(d, cam.fwd), asp = HW / VH;
    if (z < 0.5f) {
        return 0;
    }
    *sx = (vdot(d, cam.right) / z * FOCAL_Y / asp + 1) * HW * 0.5f;
    *sy = (1 - vdot(d, cam.up) / z * FOCAL_Y) * VH * 0.5f;
    return 1;
}
static const char *piece_kind(const TPiece *p)
{
    switch (p->at & 15) {
        case 0:
            return "Straight";
        case 10:
            return "Diagonal";
        case 1:
        case 3:
            return "Bend 45";
        case 4:
            return "Bend 45, diagonal end";
        default:
            return "Bend 90, half";
    }
}
/* keys in yellow, what they do in grey: {key, words, key, words, ..., NULL}, centred at y */
static void ed_keyline(float y, const char *const *kw, float sc)
{
    float w = 0;
    for (int k = 0; kw[k]; k += 2) {
        w += text_width(kw[k], sc) + 2.5f + text_width(kw[k + 1], sc) + 8;
    }
    float x = (HW - w + 8) / 2;
    for (int k = 0; kw[k]; k += 2) {
        hud_text_raw(x, y, kw[k], sc, 0xF0D040);
        x += text_width(kw[k], sc) + 2.5f;
        hud_text_raw(x, y, kw[k + 1], sc, 0xC8C8D0);
        x += text_width(kw[k + 1], sc) + 8;
    }
}
static void ed_draw_list(void)
{
    float w = 220, x = (HW - w) / 2, y = 30, h = VH - 60;
    hud_a = 235;
    hud_rect(x - 6, y - 6, w + 12, h + 12, 0x18181C);
    hud_a = 255;
    hud_text(x, y, "OPEN A TRACK", 1.5f, 0xF0C040);
    hud_text(x, y + 14, "Originals open as a copy", 1, 0x9C9CA4);
    int vis = (int)((h - 34) / LIST_GAP), first = ed_list - vis + 1 > 0 ? ed_list - vis + 1 : 0;
    for (int r = 0; r < vis && first + r < ntrk; r++) {
        draw_bar(x, y + 28 + r * LIST_GAP, w, trk[first + r].name, first + r == ed_list);
    }
}
static void ed_draw_help(void)
{
    static const char *const help[][2] = {{"FLYING", 0},
                                          {"W A S D", "move"},
                                          {"Q E", "down, up"},
                                          {"SHIFT", "faster"},
                                          {"RIGHT MOUSE / SPACE", "hold to look round"},
                                          {"WHEEL", "forward, back"},
                                          {"V", "look at the selection"},
                                          {"G", "fly along the road"},
                                          {0, 0},
                                          {"BUILDING", 0},
                                          {"UP", "add a straight"},
                                          {"LEFT RIGHT", "add a 45 bend"},
                                          {"SHIFT+LEFT RIGHT", "add a 90 bend"},
                                          {"CLICK A GHOST", "add it"},
                                          {"C", "close the loop"},
                                          {"DEL", "remove the selected pieces"},
                                          {0, 0},
                                          {"SELECTING", 0},
                                          {"CLICK", "select a piece"},
                                          {"SHIFT+CLICK", "select a run"},
                                          {"TAB / SHIFT+TAB", "next, previous"},
                                          {"PAGE UP/DOWN", "previous, next (SHIFT: run)"},
                                          {"ESC", "back to one piece"},
                                          {0, 0},
                                          {"SHAPING", 0},
                                          {"R F", "raise, lower (SHIFT: 2 m)"},
                                          {", .", "bank left, right"},
                                          {"1-6", "smooth linear hump kicker step pit"},
                                          {"- +", "smaller, bigger hump/kicker/pit"},
                                          {"L", "start line here"},
                                          {0, 0},
                                          {"TRACK", 0},
                                          {"ALT+ARROWS", "move the whole track"},
                                          {"ALT+R", "turn it (SHIFT: the other way)"},
                                          {"T", "test drive"},
                                          {"J", "can the jumps be made?"},
                                          {"CTRL+Z / Y", "undo, redo"},
                                          {"CTRL+S / O", "save, open"},
                                          {"F4  N", "rename, new"}};
    float w = 400, x = (HW - w) / 2, y = 18, cw = w / 2;
    hud_a = 248;
    hud_rect(x - 8, y - 8, w + 16, VH - 44, 0x141418);
    hud_a = 255;
    float cy = y, cx = x;
    for (int k = 0; k < (int)(sizeof help / sizeof help[0]); k++) {
        if (!help[k][0]) {
            cy += 5;
            if (cy > VH - 70 && cx == x) {
                cx = x + cw;
                cy = y;
            }
            continue;
        }
        if (!help[k][1]) {
            hud_text_raw(cx, cy, help[k][0], 1, 0xF08A28);
            cy += 10;
            continue;
        }
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
        hud_a = 190;
        hud_rect(0, 0, HW, 20, 0x08080A);
        hud_rect(0, VH - 17, HW, 17, 0x08080A);
        hud_a = 255;
        snprintf(b, sizeof b, "%s - FLY ALONG", ed.name);
        hud_text(8, 5, b, 1.5f, 0xF08A28);
        if (nseg) {
            int k = (int)fly_u % nseg;
            snprintf(b, sizeof b, "PIECE %d   HEIGHT %.1f M   %+.0f KM/H", seg_sec[k] + 1, seg[k].c.y, fly_v * 3.6f);
            hud_text(HW - 8 - text_width(b, 1.2f), 6, b, 1.2f, 0xFFFFFF);
        }
        static const char *const kw[] = {"UP/DOWN",    "speed (below 0: backwards)", "SPACE", "stop",
                                         "LEFT/RIGHT", "piece before/after",         "G/ESC", "back",
                                         NULL};
        ed_keyline(VH - 12, kw, 0.9f);
        return;
    }
    Move mv[8];
    int n = ed_help ? 0 : ed_ghosts(mv); /* the ghosts' keys, over their ends */
    for (int g = 0; g < n && ed_mode == EM_EDIT; g++) {
        V3 o[2];
        const TPiece *lp = &mv[g].p[mv[g].n - 1];
        piece_world(lp, -1, pc_ns(lp), o);
        float sx, sy;
        if (!ed_proj(vadd(vmul(vadd(o[0], o[1]), 0.5f), V(0, 3, 0)), &sx, &sy)) {
            continue;
        }
        const char *lab = mv[g].turn == 0     ? "UP"
                          : mv[g].turn == 45  ? "LEFT"
                          : mv[g].turn == -45 ? "RIGHT"
                          : mv[g].turn == 90  ? "SHIFT+LEFT"
                                              : "SHIFT+RIGHT";
        float tw = text_width(lab, 0.8f);
        hud_a = 200;
        hud_rect(sx - tw / 2 - 2, sy - 4.5f, tw + 4, 9.5f, 0x101014);
        hud_a = 255;
        hud_text_raw(sx - tw / 2, sy - 2.8f, lab, 0.8f, 0xFFFFFF);
    }
    /* the panel: the track, the selection and the keys that change it */
    int closed = ed_closed(), lo = ed_lo(), hi = ed_hi();
    float pw = 150, px = HW - pw - 6, y = 6;
    if (ed_help) {
        ed_draw_help();
        goto keys;
    }
    hud_a = 185;
    hud_rect(px - 4, y - 3, pw + 8, ed.np ? 61 : 24, 0x101014);
    hud_a = 255;
    snprintf(b, sizeof b, "%s%s", ed.name, ed_dirty ? " *" : "");
    hud_text(px, y, b, 1.2f, 0xFFFFFF);
    y += 11;
    snprintf(b, sizeof b, "%d piece%s, %s", ed.np, ed.np == 1 ? "" : "s",
             closed ? "loop closed" : "open - the blue ghosts fit on");
    hud_text_raw(px, y, b, 0.8f, closed ? 0x60FF60 : 0xFFB060);
    y += 10;
    if (ed.np) {
        const TPiece *pc = &ed.p[ed_sel];
        int jl[MAXPC + 1], nj = ed_joints(jl), same_b = 1, same_s = 1;
        float c, bk, cmin = 1e9f, cmax = -1e9f, b0 = 0;
        for (int k = 0; k < nj; k++) {
            joint_get(jl[k], &c, &bk);
            cmin = fminf(cmin, c);
            cmax = fmaxf(cmax, c);
            if (!k) {
                b0 = bk;
            } else {
                same_b &= fabsf(bk - b0) < 0.05f;
            }
        }
        for (int p = lo; p <= hi; p++) {
            same_s &= ed.p[p].shape == pc->shape && ed.p[p].amp == pc->amp;
        }
        if (lo == hi) {
            snprintf(b, sizeof b, "Piece %d: %s%s", ed_sel + 1, piece_kind(pc),
                     ed_sel == ed.start ? " (start line)" : "");
        } else {
            snprintf(b, sizeof b, "Pieces %d-%d: %d selected", lo + 1, hi + 1, hi - lo + 1);
        }
        hud_text_raw(px, y, b, 0.85f, 0x40E0FF);
        y += 9;
        char row[3][64];
        if (lo == hi) {
            snprintf(row[0], sizeof row[0], "Height at its end (pole)  %.1f m", cmax);
        } else if (cmax - cmin < 0.05f) {
            snprintf(row[0], sizeof row[0], "Height, all of it  %.1f m", cmax);
        } else {
            snprintf(row[0], sizeof row[0], "Height, all of it  %.1f - %.1f m", cmin, cmax);
        }
        if (!same_b) {
            snprintf(row[1], sizeof row[1], "Banking  mixed");
        } else if (fabsf(b0) < 0.05f) {
            snprintf(row[1], sizeof row[1], "Banking  level");
        } else {
            snprintf(row[1], sizeof row[1], "Banking  %.2f m to the %s", fabsf(b0), b0 > 0 ? "right" : "left");
        }
        if (!same_s) {
            snprintf(row[2], sizeof row[2], "Shape  mixed");
        } else if (pc->shape == SH_HUMP || pc->shape == SH_KICKER || pc->shape == SH_PIT) {
            snprintf(row[2], sizeof row[2], "Shape  %s %.1f m", SHAPE_NAME[pc->shape], pc->amp);
        } else {
            snprintf(row[2], sizeof row[2], "Shape  %s", SHAPE_NAME[pc->shape]);
        }
        static const char *const keys[3] = {"R F", ", .", "1-6  - +"};
        for (int r = 0; r < 3; r++, y += 8.5f) {
            hud_text_raw(px, y, row[r], 0.8f, 0xE8E8EC);
            hud_text_raw(px + pw - text_width(keys[r], 0.8f), y, keys[r], 0.8f, 0xF0D040);
        }
    }
    /* the main keys at the bottom, all of them with H */
keys:
    hud_a = 185;
    hud_rect(0, VH - 25, HW, 25, 0x101014);
    hud_a = 255;
    static const char *const k1[] = {"W A S D", "fly",       "Q E",     "down/up",    "RIGHT MOUSE/SPACE",
                                     "look",    "WHEEL",     "forward", "V",          "to the selection",
                                     "G",       "fly along", "T",       "test drive", NULL};
    static const char *const k2[] = {"UP LEFT RIGHT",
                                     "add (SHIFT: 90)",
                                     "CLICK",
                                     "select, add a ghost",
                                     "DEL",
                                     "remove",
                                     "C",
                                     "close",
                                     "CTRL+Z",
                                     "undo",
                                     "CTRL+S",
                                     "save",
                                     "H",
                                     "all keys",
                                     NULL};
    ed_keyline(VH - 22, k1, 0.85f);
    ed_keyline(VH - 11, k2, 0.85f);
    if (ed_mode == EM_LIST) {
        ed_draw_list();
    }
    if (ed_mode == EM_NAME) {
        float w = 200, x = (HW - w) / 2, yy = VH / 2 - 30;
        hud_rect(x - 6, yy - 6, w + 12, 54, 0x202024);
        hud_text(x, yy, "TRACK NAME:", 1.5f, 0xF0C040);
        hud_rect(x, yy + 16, w, 18, 0x56565A);
        hud_rect(x + 1, yy + 17, w - 2, 16, 0x101014);
        snprintf(b, sizeof b, "%s%s", ed_name, (SDL_GetTicks() / 400) & 1 ? "_" : "");
        hud_text_raw(x + 5, yy + 21, b, 1.3f, 0xFFE060);
        hud_text_raw(x, yy + 38, "ENTER SAVE   ESC CANCEL", 1, 0x9C9CA4);
    }
    if (ed_mode == EM_NEW || ed_mode == EM_QUIT) {
        hud_a = 225;
        hud_rect(0, VH / 2 - 6, HW, 7 * 1.5f + 12, 0x08080A);
        hud_a = 255;
        hud_text_c(VH / 2,
                   ed_mode == EM_NEW ? "N AGAIN: START A NEW TRACK (CTRL+Z BRINGS THIS ONE BACK)"
                                     : "UNSAVED CHANGES - ESC/B AGAIN TO LEAVE, CTRL+S TO SAVE",
                   1.5f, 0xFF8070);
    }
    if (ed_msg_t > 0) {
        float tw = text_width(ed_msg, 1.1f);
        hud_a = 200;
        hud_rect((HW - tw) / 2 - 5, VH - 44, tw + 10, 13, 0x101014);
        hud_a = 255;
        hud_text((HW - tw) / 2, VH - 41, ed_msg, 1.1f, 0x60FF60);
    }
}
