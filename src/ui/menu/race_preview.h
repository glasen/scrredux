/* ui/menu/race_preview.h - the pre-race view of the whole track in a chequered frame */
#pragma once

/* ---- before a race: the whole track from the stands in a chequered frame, its name below, like the original ---- */
static void pv_frame(float hw, float f[4]) /* the menu: the whole screen is the picture */
{
    if (state == ST_MENU) {
        f[0] = f[1] = -PV_Q - 1.5f;
        f[2] = hw + PV_Q + 1.5f;
        f[3] = VH + PV_Q + 1.5f;
    } else {
        f[0] = 10;
        f[1] = 8;
        f[2] = hw - 10;
        f[3] = VH - 54;
    }
}

#define CHQ(k) ((k) & 1 ? 0xD8D8D8u : 0x0C0C0Cu)

static void pv_bounds(V3 pos, V3 fwd, V3 right, V3 up, float fx, float b[4])
{
    b[0] = b[1] = 1e9f;
    b[2] = b[3] = -1e9f;

    for (int k = 0; k < nseg; k += 4) {
        for (int e = 0; e < 3; e++) {
            V3 p = e == 2 ? V(seg[k].c.x, 0, seg[k].c.z) : vadd(seg[k].c, vmul(seg[k].R, e ? TRACK_HW : -TRACK_HW));
            V3 d = vsub(p, pos);
            float z = fmaxf(vdot(d, fwd), 0.5f), x = vdot(d, right) / z * fx, y = vdot(d, up) / z * FOCAL_Y;
            b[0] = fminf(b[0], x);
            b[1] = fminf(b[1], y);
            b[2] = fmaxf(b[2], x);
            b[3] = fmaxf(b[3], y);
        }
    }
}

/* menu and preview: the camera turns slowly round the track, starting square to its long axis, from above and at one distance: the
 * nearest that keeps the track in the picture all the way round. Worked out at PV_NA angles, the aim between them */
static V3 pv_fwd(float th)
{
    return vnorm(V(cosf(th) * cosf(PV_EL), -sinf(PV_EL), sinf(th) * cosf(PV_EL)));
}

static void pv_fit(V3 fwd, float fx, const float n[4], V3 *tg, float *D, int fixed)
{
    V3 right = vnorm(vcross(fwd, UP)), up = vcross(right, fwd);
    float b[4];

    for (int it = 0; it < 3; it++) {
        if (!fixed) {
            float lo = 5, hi = 5000;

            for (int k = 0; k < 24; k++) {
                *D = 0.5f * (lo + hi);
                pv_bounds(vsub(*tg, vmul(fwd, *D)), fwd, right, up, fx, b);

                if (b[2] - b[0] <= (n[2] - n[0]) * 0.92f && b[3] - b[1] <= (n[3] - n[1]) * 0.92f) {
                    hi = *D;
                } else {
                    lo = *D;
                }
            }
            *D = hi;
        }
        pv_bounds(vsub(*tg, vmul(fwd, *D)), fwd, right, up, fx, b);
        *tg = vadd(*tg, vadd(vmul(right, ((b[0] + b[2]) - (n[0] + n[2])) * 0.5f / fx * *D),
                             vmul(up, ((b[1] + b[3]) - (n[1] + n[3])) * 0.5f / FOCAL_Y * *D)));
    }
}

static void preview_cam(float asp)
{
    static float was_asp, th0, D;
    static int was_gen = -1, was_menu;
    static V3 tg[PV_NA];

    if (asp != was_asp || track_gen != was_gen || (state == ST_MENU) != was_menu) {
        was_asp = asp;
        was_gen = track_gen;
        was_menu = state == ST_MENU;
        float f[4], hw = VH * asp, in = PV_Q + 1.5f, fx = FOCAL_Y / asp;
        pv_frame(hw, f);
        float n[4] = {(f[0] + in) / hw * 2 - 1, 1 - (f[3] - in) / VH * 2, (f[2] - in) / hw * 2 - 1,
                      1 - (f[1] + in) / VH * 2};
        double mx = 0, mz = 0, sxx = 0, szz = 0, sxz = 0;
        float ymax = 0;

        for (int k = 0; k < nseg; k++) {
            mx += seg[k].c.x;
            mz += seg[k].c.z;
            ymax = fmaxf(ymax, seg[k].c.y);
        }

        mx /= nseg;
        mz /= nseg;

        for (int k = 0; k < nseg; k++) {
            double dx = seg[k].c.x - mx, dz = seg[k].c.z - mz;
            sxx += dx * dx;
            szz += dz * dz;
            sxz += dx * dz;
        }

        th0 = 0.5f * (float)atan2(2 * sxz, sxx - szz) + PI_F / 2 + 0.3f;

        if (sinf(th0) < 0) {
            th0 += PI_F;
        }

        D = 0;

        for (int a = 0; a < PV_NA; a++) {
            float d;
            tg[a] = V((float)mx, ymax * 0.4f, (float)mz);
            pv_fit(pv_fwd(th0 + 2 * PI_F * a / PV_NA), fx, n, &tg[a], &d, 0);
            D = fmaxf(D, d);
        }

        for (int a = 0; a < PV_NA; a++) {
            pv_fit(pv_fwd(th0 + 2 * PI_F * a / PV_NA), fx, n, &tg[a], &D, 1);
        }
    }

    float u = fmodf(orbit_t * 0.12f / (2 * PI_F), 1.f) * PV_NA, fr = u - floorf(u);
    int a = (int)u % PV_NA;
    V3 fwd = pv_fwd(th0 + 2 * PI_F * u / PV_NA), t = vlerp(tg[a], tg[(a + 1) % PV_NA], fr);
    cam.pos = vsub(t, vmul(fwd, D));
    cam.fwd = fwd;
    cam.right = vnorm(vcross(fwd, UP));
    cam.up = vcross(cam.right, fwd);
}

static void draw_preview(void)
{
    float f[4], q = PV_Q;
    pv_frame(HW, f);
    uint32_t teal = 0x1C6266;
    hud_rect(0, 0, HW, f[1], teal);
    hud_rect(0, f[3], HW, VH - f[3], teal);
    hud_rect(0, f[1], f[0], f[3] - f[1], teal);
    hud_rect(f[2], f[1], HW - f[2], f[3] - f[1], teal);
    int n = (int)roundf((f[2] - f[0]) / q), m = (int)roundf((f[3] - f[1]) / q);
    float w = (f[2] - f[0]) / n, hh = (f[3] - f[1]) / m;

    for (int j = 0; j < m; j++) {
        hud_rect(f[0], f[1] + j * hh, q, hh, CHQ(j));
        hud_rect(f[2] - q, f[1] + j * hh, q, hh, CHQ(n - 1 + j));
    }

    for (int i = 0; i < n; i++) {
        hud_rect(f[0] + i * w, f[1], w, q, CHQ(i));
        hud_rect(f[0] + i * w, f[3] - q, w, q, CHQ(i + m - 1));
    }

    hud_rect(f[0] + q, f[1] + q, f[2] - f[0] - 2 * q, 1.5f, 0xF4F4F4);
    hud_rect(f[0] + q, f[3] - q - 1.5f, f[2] - f[0] - 2 * q, 1.5f, 0xF4F4F4);
    hud_rect(f[0] + q, f[1] + q, 1.5f, f[3] - f[1] - 2 * q, 0xF4F4F4);
    hud_rect(f[2] - q - 1.5f, f[1] + q, 1.5f, f[3] - f[1] - 2 * q, 0xF4F4F4);
    char b[48];
    track_title(b, sizeof b, cur_track);
    float sc = 2, bw = fminf(fmaxf(text_width(b, sc) + 60, HW * 0.45f), HW - 40), bh = 24, bx = (HW - bw) / 2,
          by = VH - 46;

    if (text_width(b, sc) > bw - 16) {
        sc = (bw - 16) / text_width(b, 1);
    }

    hud_rect(bx - 2, by - 2, bw + 4, bh + 4, 0xB4B4B4);
    hud_rect(bx, by, bw, bh, 0x6E6E6E);
    hud_rect(bx + 1.5f, by + 1.5f, bw - 3, bh - 3, 0x080808);
    hud_text_raw((HW - text_width(b, sc) + sc) / 2, by + (bh - 7 * sc) / 2, b, sc, 0xF0F0F0);
    hud_text_c(VH - 12, "ENTER START   ESC BACK", 1, 0xA8CCCC);
}

static void preview_start(void)
{
    memset(cars, 0, sizeof cars);
    npart = 0;
    paused = 0;
    state = ST_PREVIEW;
    orbit_t = 0;
}

static void preview_key(SDL_Keycode k)
{
    if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
        start_race();
    } else if (k == SDLK_ESCAPE) {
        menu_go(race_mode == RM_SEASON ? MS_SEASON : MS_PRACTICE);
    }
}
