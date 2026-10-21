/* game/camera.h - chase, cockpit and replay cameras */
#pragma once

/* ---------------------------------------------------------------- camera */
static V3 cam_dir, cam_pos_s, cam_f, cam_u;
static void ed_cam(float dt);
static void update_camera(const Car *c, float dt, int snap)
{
    if (state == ST_PREVIEW || state == ST_MENU) {
        return; /* set by preview_cam */
    }
    if (state == ST_EDIT) {
        ed_cam(dt);
        return;
    } /* the track builder's flying camera */
    if (state == ST_VIEW) {
        V3 tg = view_target;
        cam.pos =
            vadd(tg, vmul(V(cosf(view_pitch) * sinf(view_yaw), sinf(view_pitch), cosf(view_pitch) * cosf(view_yaw)),
                          view_dist));
        cam.fwd = vnorm(vsub(tg, cam.pos));
        cam.right = vnorm(vcross(cam.fwd, UP));
        cam.up = vcross(cam.right, cam.fwd);
        return;
    }
    V3 up_ref, Rv, Uv, Fv;
    body_frame(c, &Rv, &Uv, &Fv);
    if (snap) {
        cam_f = Fv;
        cam_u = Uv;
    }
    float ks = 1.f - expf(-40.f * dt);
    cam_f = vnorm(vlerp(cam_f, Fv, ks));
    cam_u = vnorm(vlerp(cam_u, Uv, ks));
    cam_u = vnorm(vsub(cam_u, vmul(cam_f, vdot(cam_u, cam_f))));
    if (c->crash > 0 && state != ST_MENU) { /* crash: look at the wreck from outside */
        float a = atan2f(c->F.z, c->F.x) + 2.2f + 0.25f * c->crash;
        cam.pos = vadd(c->x, V(cosf(a) * 14.f, 6.f, sinf(a) * 14.f));
        cam.pos.y = fmaxf(cam.pos.y, ground_h(cam.pos.x, cam.pos.z) + 3.f);
        cam.fwd = vnorm(vsub(c->x, cam.pos));
        up_ref = UP;
    } else if (fp_car() >= 0) { /* races: driver's view only, like the original */
        float m[12];
        car_body_rows(c, m);
        cam.pos =
            V(m[0] * EYE.x + m[1] * EYE.y + m[2] * EYE.z + m[3], m[4] * EYE.x + m[5] * EYE.y + m[6] * EYE.z + m[7],
              m[8] * EYE.x + m[9] * EYE.y + m[10] * EYE.z + m[11]);
        cam.fwd = vnorm(vsub(cam_f, vmul(cam_u, 0.04f)));
        up_ref = cam_u;
    } else if (state == ST_REPLAY && rp_cam == RC_TV) { /* replay: trackside cameras, each takes the car past it */
        int k = ((c->seg / 120 + 1) * 120) % nseg;
        const Seg *s = &seg[k];
        V3 Rh = vnorm(V(s->R.x, 0, s->R.z));
        cam.pos = vadd(s->c, vadd(vmul(Rh, ((c->seg / 120) & 1 ? 1.f : -1.f) * (TRACK_HW + 9.f)), V(0, 3.5f, 0)));
        cam.pos.y = fmaxf(cam.pos.y, 2.f);
        cam.fwd = vnorm(vsub(vadd(c->x, V(0, 0.5f, 0)), cam.pos));
        up_ref = UP;
    } else {
        V3 fh = vnorm(V(c->F.x, c->F.y * 0.3f, c->F.z));
        if (snap) {
            cam_dir = fh;
        }
        cam_dir = vnorm(vlerp(cam_dir, fh, 1.f - expf(-5.f * dt)));
        V3 des = vadd(vsub(c->x, vmul(cam_dir, 10.0f)), V(0, 3.6f, 0));
        if (snap) {
            cam_pos_s = des;
        }
        cam_pos_s = vlerp(cam_pos_s, des, 1.f - expf(-9.f * dt));
        cam.pos = cam_pos_s;
        V3 target = vadd(c->x, vadd(vmul(c->U, 0.6f), vmul(cam_dir, 3.f)));
        cam.fwd = vnorm(vsub(target, cam.pos));
        up_ref = vnorm(vlerp(UP, cam_u, 0.3f));
    }
    cam.right = vnorm(vcross(cam.fwd, up_ref));
    cam.up = vcross(cam.right, cam.fwd);
}
