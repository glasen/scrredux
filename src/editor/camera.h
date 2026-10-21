/* editor/camera.h - the flying camera */
#pragma once

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
