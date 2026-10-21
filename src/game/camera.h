/* game/camera.h - chase, cockpit and replay cameras */
#pragma once

/* ---------------------------------------------------------------- camera */
/* the smoothed state of the chase and cockpit cameras */
static struct {
    V3 chase_heading, chase_position; /* chase view */
    V3 cockpit_forward, cockpit_up;   /* cockpit view */
} cam_smooth;

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
        V3 tg = viewer.target;
        cam.pos = vadd(tg, vmul(V(cosf(viewer.pitch) * sinf(viewer.yaw), sinf(viewer.pitch),
                                  cosf(viewer.pitch) * cosf(viewer.yaw)),
                                viewer.distance));
        cam.fwd = vnorm(vsub(tg, cam.pos));
        cam.right = vnorm(vcross(cam.fwd, UP));
        cam.up = vcross(cam.right, cam.fwd);
        return;
    }

    V3 up_ref, Rv, Uv, Fv;
    body_frame(c, &Rv, &Uv, &Fv);

    if (snap) {
        cam_smooth.cockpit_forward = Fv;
        cam_smooth.cockpit_up = Uv;
    }

    float ks = 1.f - expf(-COCKPIT_FOLLOW_RATE * dt);
    cam_smooth.cockpit_forward = vnorm(vlerp(cam_smooth.cockpit_forward, Fv, ks));
    cam_smooth.cockpit_up = vnorm(vlerp(cam_smooth.cockpit_up, Uv, ks));
    cam_smooth.cockpit_up =
        vnorm(vsub(cam_smooth.cockpit_up,
                   vmul(cam_smooth.cockpit_forward, vdot(cam_smooth.cockpit_up, cam_smooth.cockpit_forward))));

    if (c->crash_time > 0 && state != ST_MENU) { /* crash: look at the wreck from outside */
        float a = atan2f(c->F.z, c->F.x) + CRASH_CAM_ANGLE + CRASH_CAM_ORBIT_RATE * c->crash_time;
        cam.pos = vadd(c->x, V(cosf(a) * CRASH_CAM_DISTANCE, CRASH_CAM_HEIGHT, sinf(a) * CRASH_CAM_DISTANCE));
        cam.pos.y = fmaxf(cam.pos.y, ground_h(cam.pos.x, cam.pos.z) + CRASH_CAM_MIN_ABOVE_GROUND);
        cam.fwd = vnorm(vsub(c->x, cam.pos));
        up_ref = UP;
    } else if (fp_car() >= 0) { /* races: driver's view only, like the original */
        float m[12];
        car_body_rows(c, m);
        cam.pos =
            V(m[0] * EYE.x + m[1] * EYE.y + m[2] * EYE.z + m[3], m[4] * EYE.x + m[5] * EYE.y + m[6] * EYE.z + m[7],
              m[8] * EYE.x + m[9] * EYE.y + m[10] * EYE.z + m[11]);
        cam.fwd = vnorm(vsub(cam_smooth.cockpit_forward, vmul(cam_smooth.cockpit_up, COCKPIT_LOOK_DOWN)));
        up_ref = cam_smooth.cockpit_up;
    } else if (state == ST_REPLAY &&
               replay.camera == RC_TV) { /* replay: trackside cameras, each takes the car past it */
        int k = ((c->seg / TV_CAM_SPACING + 1) * TV_CAM_SPACING) % nseg;
        const Seg *s = &seg[k];
        V3 Rh = vnorm(V(s->R.x, 0, s->R.z));
        cam.pos = vadd(s->c, vadd(vmul(Rh, ((c->seg / TV_CAM_SPACING) & 1 ? 1.f : -1.f) * (TRACK_HW + TV_CAM_OFFSET)),
                                  V(0, TV_CAM_HEIGHT, 0)));
        cam.pos.y = fmaxf(cam.pos.y, TV_CAM_MIN_HEIGHT);
        cam.fwd = vnorm(vsub(vadd(c->x, V(0, TV_CAM_AIM_HEIGHT, 0)), cam.pos));
        up_ref = UP;
    } else {
        V3 fh = vnorm(V(c->F.x, c->F.y * CHASE_PITCH_FACTOR, c->F.z));

        if (snap) {
            cam_smooth.chase_heading = fh;
        }

        cam_smooth.chase_heading = vnorm(vlerp(cam_smooth.chase_heading, fh, 1.f - expf(-CHASE_HEADING_RATE * dt)));
        V3 des = vadd(vsub(c->x, vmul(cam_smooth.chase_heading, CHASE_DISTANCE)), V(0, CHASE_HEIGHT, 0));

        if (snap) {
            cam_smooth.chase_position = des;
        }

        cam_smooth.chase_position = vlerp(cam_smooth.chase_position, des, 1.f - expf(-CHASE_POSITION_RATE * dt));
        cam.pos = cam_smooth.chase_position;
        V3 target = vadd(c->x, vadd(vmul(c->U, CHASE_AIM_UP), vmul(cam_smooth.chase_heading, CHASE_AIM_AHEAD)));
        cam.fwd = vnorm(vsub(target, cam.pos));
        up_ref = vnorm(vlerp(UP, cam_smooth.cockpit_up, CHASE_UP_BLEND));
    }
    cam.right = vnorm(vcross(cam.fwd, up_ref));
    cam.up = vcross(cam.right, cam.fwd);
}
