/* editor/scene.h - the scene and track meshes */
#pragma once

/* ---- the scene: the stadium round the whole grid once, the track as its own mesh after each change ---- */
static void ed_build_scene(void)
{
    const float E = 8 * 2048 * SCR_U;
    int ns = nseg;
    nseg = 0; /* no track: nothing worn, no compound to keep clear of it */
    tr_minx = tr_minz = -E;
    tr_maxx = tr_maxz = E;
    ar_x0 = ar_z0 = -E - 90;
    ar_x1 = ar_z1 = E + 90;
    scene_builder = 1;
    cur_track = -1;
    build_static();
    scene_builder = 0;
    track_gen++;
    nseg = ns;
}
static void ed_build_mesh(void)
{
    float keep[4] = {ar_x0, ar_x1, ar_z0, ar_z1};
    int tg = track_gen;
    trk[ED_SLOT] = ed;
    trk[ED_SLOT].orig = -1;
    if (!ed_closed()) {
        trk[ED_SLOT].start = ed.np - 1;
        trk_open = 1;
        memcpy(trk_endh, ed_endh, sizeof trk_endh);
    }
    build_track_geometry(ED_SLOT);
    ar_x0 = keep[0];
    ar_x1 = keep[1];
    ar_z0 = keep[2];
    ar_z1 = keep[3];
    track_gen = tg;
    cur_track = -1; /* the stadium stays */
    VB *was = cur;
    cur = &tkb;
    tkb.n = 0;
    for (int i = 0; i < nseg; i++) {
        build_seg_mesh(i);
    }
    for (int i = 10; i < nseg; i += 30) {
        if (!seg[i].gap) {
            build_tower(i);
        }
    }
    cur = was;
    trk_open = 0;
    tkb_gen++;
    ed_built = ed_gen;
}
