/* app/headless.h - headless modes: --sim and --shot, and the wheel-gap check */
#pragma once

/* ------------------------------------------------------------------ main */
/* debug: gap between each drawn tyre's lowest point and the road while physics has the wheel on the ground */
static double wg_sum[4], wg_max[4], wg_min[4];
static int wg_n[4], wg_float[4];

static float drawn_tyre_gap(const Car *c, int g)
{
    float m[12], lo = 1e9f;

    if (!car_group_rows(c, g, m, 0, 0)) {
        return 1e9f;
    }

    for (uint32_t k = 0; k < car_mesh.groups[g].count; k++) {
        const Vtx *v = &car_mesh.vertices[car_mesh.groups[g].first + k];
        Q q;
        track_query(m34_apply(m, V(v->x, v->y, v->z)), c->seg, 12, &q);

        if (q.valid && q.road) {
            lo = fminf(lo, q.h);
        }
    }

    return lo;
}

static void wheel_gap_sample(const Car *c)
{
    for (int g = 0; g < car_mesh.group_count; g++) {
        if (car_mesh.groups[g].role != R_WHEEL) {
            continue;
        }

        int i = car_mesh.groups[g].corner;

        if (c->suspension_len[i] >= WLF[i] - 1e-3f || car_out(c)) {
            continue; /* wheel in the air */
        }

        float gap = drawn_tyre_gap(c, g);

        if (gap > 1e8f) {
            continue;
        }

        if (gap < -0.05f && getenv("SCR_WHEELS")[0] == '2') {
            float body[12];
            car_body_rows(c, body);
            V3 lm = m34_apply(body, WMOUNT[i]), pm = body_pt(c, WMOUNT[i]);
            printf("SINK %d gap %+.3f  wl-lmin %+.3f  lean %+.3f  dz %+.3f bump %.3f  lr %.3f lp %.3f\n", i, gap,
                   c->suspension_len[i] - WLMIN[i], vdot(c->U, vsub(lm, pm)),
                   WREST[i] - c->suspension_len[i] - vdot(c->U, vsub(lm, pm)), WBUMP[i], c->lean_roll, c->lean_pitch);
        }

        if (!wg_n[i]) {
            wg_min[i] = wg_max[i] = gap;
        }

        wg_sum[i] += gap;
        wg_n[i]++;
        wg_max[i] = fmax(wg_max[i], gap);
        wg_min[i] = fmin(wg_min[i], gap);

        if (gap > 0.02f) {
            wg_float[i]++;
        }
    }
}

static void wheel_gap_report(void)
{
    static const char *nm[4] = {"FL", "FR", "RL", "RR"};

    for (int i = 0; i < 4; i++) {
        if (wg_n[i]) {
            printf("GAP %s: mean %+.3f  min %+.3f  max %+.3f m, %d of %d samples float > 2 cm\n", nm[i],
                   wg_sum[i] / wg_n[i], wg_min[i], wg_max[i], wg_float[i], wg_n[i]);
        }
    }
}

/* point the camera along (right, up) rebuilt from the forward vector */
static void camera_look_from(V3 position, V3 target)
{
    cam.pos = position;
    cam.fwd = vnorm(vsub(target, position));
    cam.right = vnorm(vcross(cam.fwd, UP));
    cam.up = vcross(cam.right, cam.fwd);
}

/* --shot with camera 4: the model viewer turned to yaw degrees (the seconds argument), set up from SCR_VIEW,
 * SCR_TARGET, SCR_FLAME, SCR_FREEZE and SCR_POSE ("travel,steer": travel -1 = full droop .. +1 = full bump) */
static void headless_model_view(float yaw_degrees)
{
    start_view();
    viewer.auto_orbit = 0;
    viewer.yaw = yaw_degrees * PI_F / 180.f;

    if (getenv("SCR_VIEW")) {
        sscanf(getenv("SCR_VIEW"), "%f,%f", &viewer.pitch, &viewer.distance);
    }

    if (getenv("SCR_TARGET")) {
        sscanf(getenv("SCR_TARGET"), "%f,%f,%f", &viewer.target.x, &viewer.target.y, &viewer.target.z);
    }

    if (getenv("SCR_FLAME")) {
        viewer.flame = 1;
    }

    if (getenv("SCR_FREEZE")) {
        viewer.animate = 0;
    }

    if (getenv("SCR_POSE")) {
        float travel = 0, steer = 0;
        sscanf(getenv("SCR_POSE"), "%f,%f", &travel, &steer);
        viewer.animate = 0;
        cars[0].steer_angle = steer;

        for (int i = 0; i < 4; i++) {
            cars[0].suspension_len[i] = WREST[i] - travel * (travel > 0 ? WREST[i] - WLMIN[i] : WLF[i] - WREST[i]);
        }
    }
}

/* SCR_TRACE: the player's car every 0.1 s on stderr */
static void headless_trace(int tick)
{
    Q q;
    track_query(cars[0].x, cars[0].seg, 30, &q);
    fprintf(stderr,
            "t=%5.2f seg %4d lat %5.2f h %5.2f v %5.1f str %5.2f delta %5.2f thr %.2f brk %.2f nwc %d yaw "
            "%5.2f roll %5.2f lean %5.3f %5.3f\n",
            tick * PHYS_DT, cars[0].seg, q.lat, q.h, vdot(cars[0].v, cars[0].F), cars[0].steer_input,
            cars[0].steer_angle, cars[0].throttle, cars[0].brake, cars[0].wheels_touching, cars[0].w.y,
            vdot(cars[0].w, cars[0].F), cars[0].lean_roll, cars[0].lean_pitch);
}

/* runs the game for secs seconds of physics (240 Hz), printing a status line every 5 s */
static void headless_run(float secs)
{
    int ticks = (int)(secs * 240);

    for (int i = 0; i < ticks; i++) {
        game_tick(PHYS_DT);

        if (state == ST_DONE) {
            break;
        }

        if (getenv("SCR_TRACE") && i % 24 == 0) {
            headless_trace(i);
        }

        if (i % 1200 == 0) {
            printf("t=%5.1f p: seg %4d/%d lap %d v=%5.1f dmg=%4.1f | ai: lap %d v=%5.1f dmg=%4.1f falls=%d\n",
                   i * PHYS_DT, cars[0].seg, nseg, cars[0].lap, vlen(cars[0].v), cars[0].damage, cars[1].lap,
                   vlen(cars[1].v), cars[1].damage, stats.falls);
        }

        if (i % 4 == 0) {
            update_camera(&cars[0], 1.f / 60.f, i == 0);
        }

        if (getenv("SCR_WHEELS") && i % 24 == 0 && state == ST_RACE) {
            wheel_gap_sample(&cars[0]);
        }
    }
}

/* SCR_WHEELS: how far each drawn tyre's lowest point sits above the road at the end */
static void wheel_final_report(const Car *c)
{
    for (int g = 0; g < car_mesh.group_count; g++) {
        if (car_mesh.groups[g].role != R_WHEEL) {
            continue;
        }

        float lowest = drawn_tyre_gap(c, g);

        if (lowest > 1e8f) {
            continue;
        }

        int i = car_mesh.groups[g].corner;
        Q hub_road;
        track_query(car_hub(c, i), c->seg, 12, &hub_road);
        printf("WHEEL %s: drawn tyre bottom %+.3f m above road, physics hub %.3f above road (radius %.3f), wl %.3f\n",
               car_mesh.groups[g].name, lowest, hub_road.h, WRAD[i], c->suspension_len[i]);
    }
}

static void headless_summary(void)
{
    printf("CONTACT samples=%d impulse=%.0f Ns sparks=%d\n", stats.contacts, stats.push, stats.sparks);
    printf("END state=%d result=%d racetime=%.1f p lap=%d dmg=%.1f best=%.1f | ai lap=%d dmg=%.1f | falls=%d nseg=%d "
           "len=%.0f\n",
           state, result, racetime, cars[0].lap, cars[0].damage, cars[0].best_lap, cars[1].lap, cars[1].damage,
           stats.falls, nseg, nseg * ds);
}

/* the debug cameras of --shot: 2 front three-quarter, 3 side, 5 the whole track; SCR_CAM "px,py,pz,tx,ty,tz" */
static void headless_shot_camera(int camsel)
{
    const Car *c = &cars[0];

    if (camsel == 4) {
        cars[0].flame = viewer.flame ? 1.f : 0.f;
        update_camera(&cars[0], 1.f / 60.f, 1);
    }

    if (camsel == 5) { /* overview of the whole track, like the original's title shots */
        V3 mid = V((tr_minx + tr_maxx) * 0.5f, 6, (tr_minz + tr_maxz) * 0.5f);
        camera_look_from(V(mid.x + 40, 190, tr_minz - 230), mid);
    }

    if (camsel == 3) { /* side view */
        camera_look_from(vadd(vadd(c->x, vmul(c->R, 6.5f)), vmul(c->U, 0.6f)), c->x);
    }

    if (getenv("SCR_CAM")) {
        V3 p = cam.pos, t = vadd(cam.pos, cam.fwd);
        sscanf(getenv("SCR_CAM"), "%f,%f,%f,%f,%f,%f", &p.x, &p.y, &p.z, &t.x, &t.y, &t.z);
        camera_look_from(p, t);
    }

    if (camsel == 2) { /* front three-quarter view of the car */
        camera_look_from(vadd(vadd(vadd(c->x, vmul(c->F, 6.5f)), vmul(c->R, 4.0f)), vmul(c->U, 1.6f)),
                         vadd(c->x, vmul(c->U, 0.3f)));
    }
}

static void headless_screenshot(const char *path, int camsel)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return;
    }

    if (gpu_init() == 0 && ensure_targets(1920, 1080) == 0) {
        update_camera(&cars[0], 1.f / 60.f, 1);
        headless_shot_camera(camsel);
        gpu_render(path);
        printf("wrote %s\n", path);
    }

    SDL_Quit();
}

/* --sim and --shot: the autopilot drives the race for `secs` seconds on a track and the result is printed; with shot
 * set, the end is rendered to a PNG from camera camsel (0 chase, 1 cockpit, 2 to 5 debug views). SCR_PROFILE,
 * SCR_LAUNCH ("segment,speed"), SCR_TRACE and SCR_WHEELS add debug output */
static void run_headless(int shot, const char *path, float secs, int track, int camsel)
{
    sel_track = track;
    build_track(track);
    cockpit = camsel;

    if (getenv("SCR_PROFILE")) {
        for (int k = 0; k < nseg; k += 25) {
            printf("%d x=%.0f y=%.1f z=%.0f gap=%d\n", k, seg[k].c.x, seg[k].c.y, seg[k].c.z, seg[k].gap);
        }
    }

    autoplay = 1;
    start_race();

    if (getenv("SCR_LAUNCH")) {
        int launch_segment = 0;
        float launch_speed = 0;
        sscanf(getenv("SCR_LAUNCH"), "%d,%f", &launch_segment, &launch_speed);
        car_place(&cars[0], launch_segment, 0, launch_speed);
        car_place(&cars[1], (launch_segment + 300) % nseg, 2.0f, 0);
        state = ST_RACE;
        cars[0].crane_time = cars[1].crane_time = 0;
    } /* else the start crane puts them back on the grid */

    if (camsel == 4) {
        headless_model_view(secs);
        secs = 0;
    }

    headless_run(secs);

    if (getenv("SCR_WHEELS")) {
        wheel_gap_report();
        wheel_final_report(&cars[0]);
    }

    headless_summary();

    if (shot) {
        headless_screenshot(path, camsel);
    }
}
