/* track/state.h - the loaded track: segments, sections, arena bounds */
#pragma once

static Seg seg[MAXSEG]; static int nseg; static float ds;
static float jump_v[MAXSEG];                           /* AI: speed to hold here for the jump ahead (its run-up and the bend before it), 0 = none */
static unsigned char jump_b[MAXSEG];                   /* AI: that run-up needs the boost */
static int cur_track = -1;
/* the track builder: an open track (its gap left out, trk_endh the open end's heights); the stadium built round the
 * whole grid without a track (scene_builder) */
static int trk_open, trk_endh[2], scene_builder;
static float tr_lift;                                  /* metres the road is raised by (roads at ground level sit on a slab) */
static short seg_sec[MAXSEG]; static int sec_mid[MAXPC], nsec; static unsigned char sec_ok[MAXPC];
static float tr_minx, tr_maxx, tr_minz, tr_maxz;
static float ar_x0, ar_x1, ar_z0, ar_z1;        /* stadium arena bounds */

static float smooth01(float t) { t = clampf(t, 0, 1); return t * t * (3 - 2 * t); }
static void smooth_circ(float *a, int passes)
{
    static float tmp[MAXSEG];
    for (int p = 0; p < passes; p++) {
        for (int i = 0; i < nseg; i++) tmp[i] = (a[(i + nseg - 1) % nseg] + 2 * a[i] + a[(i + 1) % nseg]) * 0.25f;
        memcpy(a, tmp, sizeof(float) * nseg);
    }
}

static void plan_jumps(void);
static int planning;                                   /* a scratch car is on the track (plan_jumps): no debug output */
