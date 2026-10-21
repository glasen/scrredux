/* world/build_static.h - building the static world buffer */
#pragma once

/* the track builder's floor: the 16 x 16 grid the pieces sit on, as lines on the grass */
static void build_grid_lines(void)
{
    const float C = 2048 * SCR_U, E = 8 * C, w = 0.5f, y = 0.06f;
    for (int k = 0; k <= 16; k++) {
        float o = (k - 8) * C;
        uint32_t col = k == 0 || k == 16 ? 0xE8E8D0 : 0xB8C8A0;
        draw_quad(V(o - w, y, -E), V(o + w, y, -E), V(o + w, y, E), V(o - w, y, E), col);
        draw_quad(V(-E, y, o - w), V(E, y, o - w), V(E, y, o + w), V(-E, y, o + w), col);
    }
}

static void build_static(void)
{
    cur = &sb;
    sb.n = 0;
    plan_arena();
    build_floor();
    if (scene_builder) {
        build_grid_lines();
    }
    build_stadium();
    /* big tops at the corners, behind the floodlights */
    big_top(ar_x0 - 70, ar_z0 - 70, 34, 9, 30);
    big_top(ar_x1 + 70, ar_z0 - 70, 30, 8, 26);
    big_top(ar_x1 + 70, ar_z1 + 70, 36, 9, 32);
    big_top(ar_x0 - 70, ar_z1 + 70, 28, 8, 24);
    build_mountains();
    build_outside();
    for (int i = 0; i < nseg; i++) {
        build_seg_mesh(i);
    }
    for (int i = 10; i < nseg; i += 30) {
        if (!seg[i].gap) {
            build_tower(i);
        }
    }
    if (!scene_builder) {
        build_infield();
    }
    cur = &db;
}

static void build_track(int idx)
{
    build_track_geometry(idx);
    build_static();
}
