/* editor/input.h - builder input and entry points */
#pragma once

/* ---- input ---- */
static void ed_text(const char *t)
{
    if (ed_mode != EM_NAME) {
        return;
    }
    for (; *t; t++) {
        size_t n = strlen(ed_name);
        if (n + 1 < sizeof ed_name && (strchr(NAME_CHARS, toupper((unsigned char)*t)) || (*t == ' ' && n))) {
            ed_name[n] = (char)toupper((unsigned char)*t);
            ed_name[n + 1] = 0;
        }
    }
}
static void ed_name_begin(void)
{
    snprintf(ed_name, sizeof ed_name, "%s", strcmp(ed.name, "NEW TRACK") ? ed.name : "");
    ed_mode = EM_NAME;
    fc_set_look(0);
    if (win) {
        SDL_StartTextInput(win);
    }
}
static void ed_save_key(void)
{
    if (!strcmp(ed.name, "NEW TRACK")) {
        if (ed_ready()) {
            ed_name_begin();
        }
    } else {
        ed_save();
    }
}
static void ed_leave(void)
{
    fc_set_look(0);
    ed_dirty = 0;
    menu_go(MS_MAIN);
}
static void ed_fly_key(SDL_Keycode k)
{
    switch (k) {
        case SDLK_ESCAPE:
        case SDLK_G:
            ed_fly(0);
            break;
        case SDLK_UP:
            fly_v = fminf(fly_v + 5, 70);
            break;
        case SDLK_DOWN:
            fly_v = fmaxf(fly_v - 5, -70);
            break;
        case SDLK_SPACE:
            if (fly_v != 0) {
                fly_was = fly_v;
                fly_v = 0;
            } else {
                fly_v = fly_was ? fly_was : 25;
            }
            break;
        case SDLK_LEFT:
        case SDLK_RIGHT: { /* to the start of the piece before or after */
            int p = seg_sec[(int)fly_u % nseg], here = ed_seg_of(p), q;
            if (k == SDLK_LEFT && here >= 0 && here < (int)fly_u - (int)(5 / ds)) {
                q = here; /* first back to this one's start */
            } else {
                q = ed_seg_of((p + (k == SDLK_LEFT ? ed.np - 1 : 1)) % ed.np);
            }
            if (q >= 0) {
                fly_u = (float)q;
            }
            break;
        }
        default:
            break;
    }
}
static void ed_key(SDL_Keycode k, SDL_Keymod mod)
{
    int sh = (mod & SDL_KMOD_SHIFT) != 0, cmd = (mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0,
        alt = (mod & SDL_KMOD_ALT) != 0;
    if (ed_mode == EM_LIST) {
        if (k == SDLK_UP) {
            ed_list = (ed_list + ntrk - 1) % ntrk;
        } else if (k == SDLK_DOWN) {
            ed_list = (ed_list + 1) % ntrk;
        } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
            ed_import(ed_list);
            ed_mode = EM_EDIT;
            ed_view_sel();
        } else if (k == SDLK_ESCAPE) {
            ed_mode = EM_EDIT;
        }
        return;
    }
    if (ed_mode == EM_NAME) {
        if (k == SDLK_BACKSPACE && ed_name[0]) {
            ed_name[strlen(ed_name) - 1] = 0;
        } else if (k == SDLK_ESCAPE) {
            ed_mode = EM_EDIT;
            if (win) {
                SDL_StopTextInput(win);
            }
        } else if ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && ed_name[0]) {
            int other = track_find(ed_name);
            if (other >= 0 && other < SCR_NTRACKS) {
                ed_note("That name belongs to an original track");
                return;
            }
            snprintf(ed.name, sizeof ed.name, "%s", ed_name);
            ed_mode = EM_EDIT;
            if (win) {
                SDL_StopTextInput(win);
            }
            ed_save();
        }
        return;
    }
    if (ed_mode == EM_FLY) {
        ed_fly_key(k);
        return;
    }
    if (ed_mode == EM_NEW) {
        ed_mode = EM_EDIT;
        if (k == SDLK_N) {
            ed_push(0);
            ed_new();
            ed_note("New track - CTRL+Z brings the old one back");
        }
        return;
    }
    if (ed_mode == EM_QUIT) {
        ed_mode = EM_EDIT;
        if (k == SDLK_ESCAPE) {
            ed_leave();
            return;
        }
    }
    TPiece *pc = &ed.p[ed_sel];
    int tweak = 0;
    if (cmd) {
        switch (k) {
            case SDLK_Z:
                ed_undo(sh);
                break;
            case SDLK_Y:
                ed_undo(1);
                break;
            case SDLK_S:
                ed_save_key();
                break;
            case SDLK_O:
                ed_list = 0;
                ed_mode = EM_LIST;
                break;
            case SDLK_N:
                ed_mode = EM_NEW;
                break;
            default:
                break;
        }
    } else if (alt) {
        switch (k) {
            case SDLK_UP:
                ed_move(0, 1, 0);
                break; /* along the grid as seen from the south, like the map was */
            case SDLK_DOWN:
                ed_move(0, -1, 0);
                break;
            case SDLK_LEFT:
                ed_move(-1, 0, 0);
                break;
            case SDLK_RIGHT:
                ed_move(1, 0, 0);
                break;
            case SDLK_R:
                ed_move(0, 0, sh ? 3 : 1);
                break;
            default:
                break;
        }
    } else {
        switch (k) {
            case SDLK_UP:
                ed_extend(0);
                break;
            case SDLK_LEFT:
                ed_extend(sh ? 90 : 45);
                break;
            case SDLK_RIGHT:
                ed_extend(sh ? -90 : -45);
                break;
            case SDLK_BACKSPACE:
            case SDLK_DELETE:
                ed_remove();
                break;
            case SDLK_C:
                ed_autoclose();
                break;
            case SDLK_TAB:
                ed_select((ed_sel + (sh ? ed.np - 1 : 1)) % ed.np, 0);
                break;
            case SDLK_PAGEUP:
            case SDLK_PAGEDOWN: { /* a run stops at the ends */
                int d = k == SDLK_PAGEUP ? -1 : 1;
                if (!sh || (ed_sel + d >= 0 && ed_sel + d < ed.np)) {
                    ed_select((ed_sel + d + ed.np) % ed.np, sh);
                }
                break;
            }
            case SDLK_HOME:
                ed_select(0, sh);
                break;
            case SDLK_END:
                ed_select(ed.np - 1, sh);
                break;
            case SDLK_R:
                ed_tweak_joints(TW_HEIGHT, sh ? 2.f : 0.5f, 0);
                tweak = 1;
                break;
            case SDLK_F:
                ed_tweak_joints(TW_HEIGHT, sh ? -2.f : -0.5f, 0);
                tweak = 1;
                break;
            case SDLK_COMMA:
                ed_tweak_joints(TW_BANK, 0, -0.25f);
                tweak = 1;
                break;
            case SDLK_PERIOD:
                ed_tweak_joints(TW_BANK, 0, 0.25f);
                tweak = 1;
                break;
            case SDLK_1:
            case SDLK_2:
            case SDLK_3:
            case SDLK_4:
            case SDLK_5:
            case SDLK_6:
                ed_push(0);
                for (int p = ed_lo(); p <= ed_hi(); p++) {
                    TPiece *q = &ed.p[p];
                    q->shape = (unsigned char)(SH_SMOOTH + (k - SDLK_1));
                    if (q->amp == 0) {
                        q->amp = q->shape == SH_HUMP     ? 2.f
                                 : q->shape == SH_KICKER ? 3.f
                                 : q->shape == SH_PIT    ? 6.f
                                                         : 0.f;
                    }
                    regen(p);
                }
                break;
            case SDLK_MINUS:
            case SDLK_KP_MINUS:
            case SDLK_EQUALS:
            case SDLK_PLUS:
            case SDLK_KP_PLUS:
                ed_push(TW_SIZE);
                tweak = 1;
                for (int p = ed_lo(); p <= ed_hi(); p++) {
                    TPiece *q = &ed.p[p];
                    q->amp = k == SDLK_MINUS || k == SDLK_KP_MINUS ? q->amp - 0.5f : fminf(q->amp + 0.5f, 30.f);
                    if (q->shape != SH_HUMP && q->amp < 0) {
                        q->amp = 0;
                    }
                    regen(p);
                }
                break;
            case SDLK_L:
                if ((pc->at & 15) != 0) {
                    ed_note("The start line goes on a straight piece");
                } else if (ed.start != ed_sel) {
                    ed_push(0);
                    ed.start = ed_sel;
                }
                break;
            case SDLK_V:
                ed_view_sel();
                break;
            case SDLK_G:
                ed_fly(1);
                break;
            case SDLK_H:
                ed_help = !ed_help;
                break;
            case SDLK_T:
                fc_set_look(0);
                ed_test();
                break;
            case SDLK_J:
                ed_check_jumps();
                break;
            case SDLK_F2:
                ed_save_key();
                break;
            case SDLK_F3:
                ed_list = 0;
                ed_mode = EM_LIST;
                break;
            case SDLK_F4:
                ed_name_begin();
                break;
            case SDLK_N:
                if (ed_dirty) {
                    ed_mode = EM_NEW;
                } else {
                    ed_push(0);
                    ed_new();
                }
                break;
            case SDLK_ESCAPE:
                if (ed_help) {
                    ed_help = 0;
                } else if (ed_lo() != ed_hi()) {
                    ed_select(ed_sel, 0); /* first the run goes back to one piece */
                } else if (ed_dirty) {
                    ed_mode = EM_QUIT;
                } else {
                    ed_leave();
                }
                break;
            default:
                break; /* W A S D Q E Space: the camera (ed_tick) */
        }
    }
    if (!tweak) {
        ed_tweak = -1;
    }
}
static void ed_button(float hx, float hy, int b, int down)
{
    if (b == SDL_BUTTON_RIGHT) {
        fc_set_look(down && ed_mode == EM_EDIT);
        return;
    }
    if (b != SDL_BUTTON_LEFT || !down || ed_mode != EM_EDIT || !ed.np) {
        return;
    }
    ed_tweak = -1;
    ed_hx = hx;
    ed_hy = hy;
    Pick k = ed_pick(hx, hy);
    if (k.kind == 2) {
        ed_extend(k.turn);
    } else if (k.kind == 1) {
        ed_select(k.p, (SDL_GetModState() & SDL_KMOD_SHIFT) != 0);
    }
}
static void ed_motion(float hx, float hy, float xr, float yr)
{
    if (fc_look) {
        fc_yaw -= xr * 0.004f;
        fc_pitch = clampf(fc_pitch - yr * 0.004f, -1.5f, 1.5f);
        return;
    }
    ed_hx = hx;
    ed_hy = hy;
}
static void ed_wheel(float w)
{
    if (ed_mode != EM_EDIT) {
        return;
    }
    fc_pos = vadd(fc_pos, vmul(fc_dir(), w * fmaxf(6.f, fc_pos.y * 0.12f)));
    fc_clamp();
}
static void ed_open(void)
{
    if (!ed.np) {
        ed_new();
    }
    memset(cars, 0, sizeof cars);
    npart = 0;
    ed_mode = EM_EDIT;
    state = ST_EDIT;
    paused = 0;
    fc_look = fc_space = 0;
    if (win) {
        SDL_StopTextInput(win);
    }
    ed_build_scene();
    ed_build_mesh();
    static int seen;
    if (!seen++) {
        ed_view_sel(); /* the first time: close to where the track starts */
    }
}
static void ed_return(void)
{
    ed_open();
    ed_jumps_note();
}

/* debug (--ui-shot): fly:X,Y,Z,YAW,PITCH puts the camera there; the track checked by the builder's own rules */
static void ed_fly_to(const float *v)
{
    fc_pos = V(v[0], v[1], v[2]);
    fc_yaw = v[3] * PI_F / 180;
    fc_pitch = v[4] * PI_F / 180;
}
static void ed_report(void)
{
    int joins = 1, clash = 0, cl = ed_closed(), jl[MAXPC + 1], nj = ed_joints(jl);
    for (int p = 0; p + 1 < ed.np; p++) {
        int e[2][2], s0[2][2];
        pc_endxs(&ed.p[p], e);
        pc_xs(&ed.p[p + 1], 0, s0);
        joins &= xs_eq(e, s0);
    }
    for (int a = 0; a < ed.np; a++) {
        for (int b = a + 2; b < ed.np; b++) {
            if (!(cl && a == 0 && b == ed.np - 1) && pc_clash(&ed.p[a], &ed.p[b])) {
                clash++;
            }
        }
    }
    printf("editor: %d pieces, %s, start %d, sel %d-%d, joins %s, %d clashes, undo %d redo %d, mode %d, mesh %d tris\n",
           ed.np, cl ? "closed" : "open", ed.start, ed_lo(), ed_hi(), joins ? "ok" : "BROKEN", clash, ed_un_n, ed_re_n,
           ed_mode, tkb.n / 3);
    printf("editor: joints");
    for (int k = 0; k < nj; k++) {
        float c, bk;
        joint_get(jl[k], &c, &bk);
        printf(" %d:%.1f/%+.2f", jl[k], c, bk);
    }
    printf("  camera %.0f,%.0f,%.0f\n", fc_pos.x, fc_pos.y, fc_pos.z);
    Move mv[8];
    int n = ed_ghosts(mv);
    ed_cam(0);
    for (int g = 0; g < n; g++) {
        float sx, sy;
        V3 m = piece_mid(&mv[g].p[mv[g].n - 1], -1);
        if (ed_proj(m, &sx, &sy)) {
            printf("editor: ghost %+d at %.0f,%.0f\n", mv[g].turn, sx, sy);
        }
    }
}
/* debug: the originals' pieces keep clear of each other by the builder's own rule (--check-tracks) */
static void ed_check_originals(void)
{
    for (int t = 0; t < SCR_NTRACKS; t++) {
        const TrackDef *td = &trk[t];
        int bad = 0;
        for (int a = 0; a < td->np; a++) {
            for (int b = a + 2; b < td->np; b++) {
                if (!(a == 0 && b == td->np - 1) && pc_clash(&td->p[a], &td->p[b])) {
                    bad++;
                    if (bad < 4) {
                        printf("  %s: pieces %d and %d share a cube\n", td->name, a, b);
                    }
                }
            }
        }
        int ok = 1;
        for (int p = 0; p < td->np; p++) {
            int e[2][2], s[2][2];
            pc_endxs(&td->p[p], e);
            pc_xs(&td->p[(p + 1) % td->np], 0, s);
            ok &= xs_eq(e, s);
        }
        printf("%-16s %3d pieces, joins %s, %d clashes\n", td->name, td->np, ok ? "ok" : "BROKEN", bad);
    }
}
