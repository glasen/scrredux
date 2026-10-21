/* editor/camera.h - the flying camera */
#pragma once

/* ---- the flying camera ---- */

static V3 fc_dir(void)
{
    return V(sinf(editor.cam.yaw) * cosf(editor.cam.pitch), sinf(editor.cam.pitch),
             cosf(editor.cam.yaw) * cosf(editor.cam.pitch));
}

static void fc_clamp(void)
{
    editor.cam.pos.y = clampf(editor.cam.pos.y, 2, 700);
    editor.cam.pos.x = clampf(editor.cam.pos.x, -1600, 1600);
    editor.cam.pos.z = clampf(editor.cam.pos.z, -1600, 1600);
}

static void fc_set_look(int on)
{
    if (on != editor.cam.looking) {
        editor.cam.looking = on;

        if (win) {
            SDL_SetWindowRelativeMouseMode(win, on);
        }
    }
}

/* the camera to the selection, from the way it looks now: one piece's end (where the track goes on), a run's middle */
static V3 joint_world(int j);

static void ed_view_sel(void)
{
    V3 a = piece_mid(&editor.track.p[ed_lo()], ed_lo()), b = piece_mid(&editor.track.p[ed_hi()], ed_hi()),
       t = vmul(vadd(a, b), 0.5f);

    if (ed_lo() == ed_hi()) { /* one piece: from behind it, looking the way it goes */
        V3 e = joint_world(editor.sel + 1), h = vsub(e, a);

        if (h.x * h.x + h.z * h.z > 1) {
            editor.cam.yaw = atan2f(h.x, h.z);
        }

        t = vadd(e, vmul(vnorm(V(h.x, 0, h.z)), 8));
    }

    editor.cam.pitch = -0.6f;
    editor.cam.pos = vsub(t, vmul(fc_dir(), 110 + vlen(vsub(a, b)) * 0.6f));
    fc_clamp();
}

static int ed_seg_of(int p)
{
    for (int k = 0; k < nseg; k++) {
        if (seg_sec[k] == p && !seg[k].gap) {
            return k;
        }
    }

    return -1;
}

static void ed_fly(int on)
{
    if (!on) {
        editor.mode = EM_EDIT;
        return;
    }

    if (editor.built_generation != editor.generation) {
        ed_build_mesh();
    }

    int k = ed_seg_of(ed_lo());

    if (!nseg || k < 0) {
        return;
    }

    editor.mode = EM_FLY;
    editor.fly.position = (float)k;
    editor.fly.speed = 25;
    editor.fly.snap = 1;
    fc_set_look(0);
}

static void ed_tick(float dt)
{
    editor.since_change += dt;

    if (editor.track.np && editor.built_generation != editor.generation &&
        (editor.since_change > 0.08f || editor.built_generation < 0)) {
        ed_build_mesh();
    }

    if (editor.mode == EM_FLY && nseg) {
        editor.fly.position = fmodf(editor.fly.position + editor.fly.speed * dt / ds + nseg, (float)nseg);

        for (int g = 0; g < nseg && seg[(int)editor.fly.position % nseg].gap; g++) {
            editor.fly.position = fmodf(editor.fly.position + (editor.fly.speed < 0 ? -1 : 1) + nseg, (float)nseg);
        }

        return;
    }

    if (editor.mode != EM_EDIT || !keys) {
        return;
    }

    SDL_Keymod m = SDL_GetModState();
    int sp = keys[SDL_SCANCODE_SPACE];

    if (sp != editor.cam.space) {
        editor.cam.space = sp;
        fc_set_look(sp);
    }

    if (m & (SDL_KMOD_CTRL | SDL_KMOD_GUI | SDL_KMOD_ALT)) {
        return;
    }

    float v = fmaxf(25.f, editor.cam.pos.y * 0.9f) * (m & SDL_KMOD_SHIFT ? 3.f : 1.f);
    V3 f = V(sinf(editor.cam.yaw), 0, cosf(editor.cam.yaw)), r = V(-f.z, 0, f.x), d = V(0, 0, 0);

    if (keys[SDL_SCANCODE_W]) {
        d = vadd(d, f);
    }

    if (keys[SDL_SCANCODE_S]) {
        d = vsub(d, f);
    }

    if (keys[SDL_SCANCODE_D]) {
        d = vadd(d, r);
    }

    if (keys[SDL_SCANCODE_A]) {
        d = vsub(d, r);
    }

    if (keys[SDL_SCANCODE_E]) {
        d = vadd(d, UP);
    }

    if (keys[SDL_SCANCODE_Q]) {
        d = vsub(d, UP);
    }

    editor.cam.pos = vadd(editor.cam.pos, vmul(d, v * dt));
    fc_clamp();
}

static void ed_cam(float dt)
{
    V3 up = UP;

    if (editor.mode == EM_FLY && nseg) { /* at a driver's eye height, looking 8 m up the road */
        int k = (int)editor.fly.position % nseg, ka = (k + (int)(8 / ds)) % nseg;
        float f = editor.fly.position - floorf(editor.fly.position),
              kf = editor.fly.snap ? 1.f : 1.f - expf(-10.f * dt);
        const Seg *a = &seg[k], *b = &seg[(k + 1) % nseg];
        V3 n = vnorm(vlerp(a->N, b->N, f));
        cam.pos = vadd(vlerp(a->c, b->c, f), vmul(n, 1.8f));
        V3 fw = seg[ka].gap ? a->T : vnorm(vsub(vadd(seg[ka].c, vmul(seg[ka].N, 1.4f)), cam.pos));
        editor.fly.forward = vnorm(vlerp(editor.fly.forward, fw, kf));
        editor.fly.normal = vnorm(vlerp(editor.fly.normal, n, kf));
        editor.fly.snap = 0;
        cam.fwd = editor.fly.forward;
        up = editor.fly.normal;
    } else {
        cam.pos = editor.cam.pos;
        cam.fwd = fc_dir();
    }
    cam.right = vnorm(vcross(cam.fwd, up));
    cam.up = vcross(cam.right, cam.fwd);
}
