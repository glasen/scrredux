/* editor/input.h - builder input and entry points */
#pragma once

/* ---- input ---- */
static void ed_text(const char *t)
{
    if (editor.mode != EM_NAME) {
        return;
    }

    for (; *t; t++) {
        size_t n = strlen(editor.name_input);

        if (n + 1 < sizeof editor.name_input && (strchr(NAME_CHARS, toupper((unsigned char)*t)) || (*t == ' ' && n))) {
            editor.name_input[n] = (char)toupper((unsigned char)*t);
            editor.name_input[n + 1] = 0;
        }
    }
}

static void ed_name_begin(void)
{
    snprintf(editor.name_input, sizeof editor.name_input, "%s",
             strcmp(editor.track.name, "NEW TRACK") ? editor.track.name : "");
    editor.mode = EM_NAME;
    fc_set_look(0);

    if (win) {
        SDL_StartTextInput(win);
    }
}

static void ed_save_key(void)
{
    if (!strcmp(editor.track.name, "NEW TRACK")) {
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
    editor.dirty = 0;
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
            editor.fly.speed = fminf(editor.fly.speed + 5, 70);
            break;
        case SDLK_DOWN:
            editor.fly.speed = fmaxf(editor.fly.speed - 5, -70);
            break;
        case SDLK_SPACE:
            if (editor.fly.speed != 0) {
                editor.fly.saved_speed = editor.fly.speed;
                editor.fly.speed = 0;
            } else {
                editor.fly.speed = editor.fly.saved_speed ? editor.fly.saved_speed : 25;
            }
            break;
        case SDLK_LEFT:
        case SDLK_RIGHT: { /* to the start of the piece before or after */
            int p = seg_sec[(int)editor.fly.position % nseg], here = ed_seg_of(p), q;

            if (k == SDLK_LEFT && here >= 0 && here < (int)editor.fly.position - (int)(5 / ds)) {
                q = here; /* first back to this one's start */
            } else {
                q = ed_seg_of((p + (k == SDLK_LEFT ? editor.track.np - 1 : 1)) % editor.track.np);
            }

            if (q >= 0) {
                editor.fly.position = (float)q;
            }

            break;
        }
        default:
            break;
    }
}

static void ed_key_track_list(SDL_Keycode k)
{
    if (k == SDLK_UP) {
        editor.list_cursor = (editor.list_cursor + ntrk - 1) % ntrk;
    } else if (k == SDLK_DOWN) {
        editor.list_cursor = (editor.list_cursor + 1) % ntrk;
    } else if (k == SDLK_RETURN || k == SDLK_KP_ENTER) {
        ed_import(editor.list_cursor);
        editor.mode = EM_EDIT;
        ed_view_sel();
    } else if (k == SDLK_ESCAPE) {
        editor.mode = EM_EDIT;
    }
}

/* typing the name to save the track under */
static void ed_key_name_entry(SDL_Keycode k)
{
    if (k == SDLK_BACKSPACE && editor.name_input[0]) {
        editor.name_input[strlen(editor.name_input) - 1] = 0;
    } else if (k == SDLK_ESCAPE) {
        editor.mode = EM_EDIT;

        if (win) {
            SDL_StopTextInput(win);
        }
    } else if ((k == SDLK_RETURN || k == SDLK_KP_ENTER) && editor.name_input[0]) {
        int other = track_find(editor.name_input);

        if (other >= 0 && other < SCR_NTRACKS) {
            ed_note("That name belongs to an original track");
            return;
        }

        snprintf(editor.track.name, sizeof editor.track.name, "%s", editor.name_input);
        editor.mode = EM_EDIT;

        if (win) {
            SDL_StopTextInput(win);
        }

        ed_save();
    }
}

/* Ctrl (or Cmd): undo, save, open, new */
static void ed_key_command(SDL_Keycode k, int shift)
{
    switch (k) {
        case SDLK_Z:
            ed_undo(shift);
            break;
        case SDLK_Y:
            ed_undo(1);
            break;
        case SDLK_S:
            ed_save_key();
            break;
        case SDLK_O:
            editor.list_cursor = 0;
            editor.mode = EM_LIST;
            break;
        case SDLK_N:
            editor.mode = EM_NEW;
            break;
        default:
            break;
    }
}

/* Alt: move the selected piece on the grid, or turn it */
static void ed_key_move_piece(SDL_Keycode k, int shift)
{
    switch (k) {
        case SDLK_UP:
            ed_move(0, 1, 0); /* along the grid as seen from the south, like the map was */
            break;
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
            ed_move(0, 0, shift ? 3 : 1);
            break;
        default:
            break;
    }
}

/* keys 1-6: the height profile of the selected run */
static void ed_set_shape(SDL_Keycode k)
{
    ed_push(0);

    for (int p = ed_lo(); p <= ed_hi(); p++) {
        TPiece *piece = &editor.track.p[p];
        piece->shape = (unsigned char)(SH_SMOOTH + (k - SDLK_1));

        if (piece->amp == 0) {
            piece->amp = piece->shape == SH_HUMP     ? 2.f
                         : piece->shape == SH_KICKER ? 3.f
                         : piece->shape == SH_PIT    ? 6.f
                                                     : 0.f;
        }

        regen(p);
    }
}

/* + and -: the size of the profile of the selected run */
static void ed_resize_shape(SDL_Keycode k)
{
    ed_push(TW_SIZE);

    for (int p = ed_lo(); p <= ed_hi(); p++) {
        TPiece *piece = &editor.track.p[p];
        piece->amp = k == SDLK_MINUS || k == SDLK_KP_MINUS ? piece->amp - 0.5f : fminf(piece->amp + 0.5f, 30.f);

        if (piece->shape != SH_HUMP && piece->amp < 0) {
            piece->amp = 0;
        }

        regen(p);
    }
}

/* Escape steps back out: help, a run of pieces, unsaved changes, the builder */
static void ed_key_escape(void)
{
    if (editor.show_help) {
        editor.show_help = 0;
    } else if (ed_lo() != ed_hi()) {
        ed_select(editor.sel, 0); /* first the run goes back to one piece */
    } else if (editor.dirty) {
        editor.mode = EM_QUIT;
    } else {
        ed_leave();
    }
}

/* the plain keys: build, select, tweak heights. Returns 1 for a key that tweaks (a run of them is one undo step) */
static int ed_key_build(SDL_Keycode k, int shift)
{
    TPiece *selected = &editor.track.p[editor.sel];
    int tweak = 0;

    switch (k) {
        case SDLK_UP:
            ed_extend(0);
            break;
        case SDLK_LEFT:
            ed_extend(shift ? 90 : 45);
            break;
        case SDLK_RIGHT:
            ed_extend(shift ? -90 : -45);
            break;
        case SDLK_BACKSPACE:
        case SDLK_DELETE:
            ed_remove();
            break;
        case SDLK_C:
            ed_autoclose();
            break;
        case SDLK_TAB:
            ed_select((editor.sel + (shift ? editor.track.np - 1 : 1)) % editor.track.np, 0);
            break;
        case SDLK_PAGEUP:
        case SDLK_PAGEDOWN: { /* a run stops at the ends */
            int d = k == SDLK_PAGEUP ? -1 : 1;

            if (!shift || (editor.sel + d >= 0 && editor.sel + d < editor.track.np)) {
                ed_select((editor.sel + d + editor.track.np) % editor.track.np, shift);
            }

            break;
        }
        case SDLK_HOME:
            ed_select(0, shift);
            break;
        case SDLK_END:
            ed_select(editor.track.np - 1, shift);
            break;
        case SDLK_R:
            ed_tweak_joints(TW_HEIGHT, shift ? 2.f : 0.5f, 0);
            tweak = 1;
            break;
        case SDLK_F:
            ed_tweak_joints(TW_HEIGHT, shift ? -2.f : -0.5f, 0);
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
            ed_set_shape(k);
            break;
        case SDLK_MINUS:
        case SDLK_KP_MINUS:
        case SDLK_EQUALS:
        case SDLK_PLUS:
        case SDLK_KP_PLUS:
            ed_resize_shape(k);
            tweak = 1;
            break;
        case SDLK_L:
            if ((selected->at & 15) != 0) {
                ed_note("The start line goes on a straight piece");
            } else if (editor.track.start != editor.sel) {
                ed_push(0);
                editor.track.start = editor.sel;
            }
            break;
        case SDLK_V:
            ed_view_sel();
            break;
        case SDLK_G:
            ed_fly(1);
            break;
        case SDLK_H:
            editor.show_help = !editor.show_help;
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
            editor.list_cursor = 0;
            editor.mode = EM_LIST;
            break;
        case SDLK_F4:
            ed_name_begin();
            break;
        case SDLK_N:
            if (editor.dirty) {
                editor.mode = EM_NEW;
            } else {
                ed_push(0);
                ed_new();
            }
            break;
        case SDLK_ESCAPE:
            ed_key_escape();
            break;
        default:
            break; /* W A S D Q E Space: the camera (ed_tick) */
    }

    return tweak;
}

static void ed_key(SDL_Keycode k, SDL_Keymod mod)
{
    int shift = (mod & SDL_KMOD_SHIFT) != 0, command = (mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0,
        alt = (mod & SDL_KMOD_ALT) != 0;

    switch (editor.mode) {
        case EM_LIST:
            ed_key_track_list(k);
            return;
        case EM_NAME:
            ed_key_name_entry(k);
            return;
        case EM_FLY:
            ed_fly_key(k);
            return;
        case EM_NEW: /* "new track?" asked while there are unsaved changes: N confirms, anything else cancels */
            editor.mode = EM_EDIT;

            if (k == SDLK_N) {
                ed_push(0);
                ed_new();
                ed_note("New track - CTRL+Z brings the old one back");
            }

            return;
        case EM_QUIT: /* "really leave?": Escape again confirms, anything else carries on with that key */
            editor.mode = EM_EDIT;

            if (k == SDLK_ESCAPE) {
                ed_leave();
                return;
            }

            break;
        default:
            break;
    }

    int tweak = 0;

    if (command) {
        ed_key_command(k, shift);
    } else if (alt) {
        ed_key_move_piece(k, shift);
    } else {
        tweak = ed_key_build(k, shift);
    }

    if (!tweak) {
        editor.tweak_kind = -1;
    }
}

static void ed_button(float hx, float hy, int b, int down)
{
    if (b == SDL_BUTTON_RIGHT) {
        fc_set_look(down && editor.mode == EM_EDIT);
        return;
    }

    if (b != SDL_BUTTON_LEFT || !down || editor.mode != EM_EDIT || !editor.track.np) {
        return;
    }

    editor.tweak_kind = -1;
    editor.mouse_x = hx;
    editor.mouse_y = hy;
    Pick k = ed_pick(hx, hy);

    if (k.kind == 2) {
        ed_extend(k.turn);
    } else if (k.kind == 1) {
        ed_select(k.p, (SDL_GetModState() & SDL_KMOD_SHIFT) != 0);
    }
}

static void ed_motion(float hx, float hy, float xr, float yr)
{
    if (editor.cam.looking) {
        editor.cam.yaw -= xr * 0.004f;
        editor.cam.pitch = clampf(editor.cam.pitch - yr * 0.004f, -1.5f, 1.5f);
        return;
    }

    editor.mouse_x = hx;
    editor.mouse_y = hy;
}

static void ed_wheel(float w)
{
    if (editor.mode != EM_EDIT) {
        return;
    }

    editor.cam.pos = vadd(editor.cam.pos, vmul(fc_dir(), w * fmaxf(6.f, editor.cam.pos.y * 0.12f)));
    fc_clamp();
}

static void ed_open(void)
{
    if (!editor.track.np) {
        ed_new();
    }

    memset(cars, 0, sizeof cars);
    npart = 0;
    editor.mode = EM_EDIT;
    state = ST_EDIT;
    paused = 0;
    editor.cam.looking = editor.cam.space = 0;

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
    editor.cam.pos = V(v[0], v[1], v[2]);
    editor.cam.yaw = v[3] * PI_F / 180;
    editor.cam.pitch = v[4] * PI_F / 180;
}

static void ed_report(void)
{
    int joins = 1, clash = 0, cl = ed_closed(), jl[MAXPC + 1], nj = ed_joints(jl);

    for (int p = 0; p + 1 < editor.track.np; p++) {
        int e[2][2], s0[2][2];
        pc_endxs(&editor.track.p[p], e);
        pc_xs(&editor.track.p[p + 1], 0, s0);
        joins &= xs_eq(e, s0);
    }

    for (int a = 0; a < editor.track.np; a++) {
        for (int b = a + 2; b < editor.track.np; b++) {
            if (!(cl && a == 0 && b == editor.track.np - 1) && pc_clash(&editor.track.p[a], &editor.track.p[b])) {
                clash++;
            }
        }
    }

    printf("editor: %d pieces, %s, start %d, sel %d-%d, joins %s, %d clashes, undo %d redo %d, mode %d, mesh %d tris\n",
           editor.track.np, cl ? "closed" : "open", editor.track.start, ed_lo(), ed_hi(), joins ? "ok" : "BROKEN",
           clash, editor.undo_count, editor.redo_count, editor.mode, tkb.n / 3);
    printf("editor: joints");

    for (int k = 0; k < nj; k++) {
        float c, bk;
        joint_get(jl[k], &c, &bk);
        printf(" %d:%.1f/%+.2f", jl[k], c, bk);
    }

    printf("  camera %.0f,%.0f,%.0f\n", editor.cam.pos.x, editor.cam.pos.y, editor.cam.pos.z);
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
