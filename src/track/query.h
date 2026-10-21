/* track/query.h - ground height and nearest-road queries */
#pragma once

/* flat stadium floor, like the original */
static float ground_h(float x, float z)
{
    (void)x;
    (void)z;
    return 0;
}

typedef struct {
    int seg, valid;
    float u, lat, h;
    V3 N, R, T;
    int road;
} Q;

static void track_query(V3 p, int hint, int range, Q *q)
{
    float best = 1e30f;
    int bj = -1, bg = 0;
    float bu = 0;
    for (int o = -range; o <= range; o++) {
        int j = ((hint + o) % nseg + nseg) % nseg;
        const Seg *s = &seg[j], *s2 = &seg[(j + 1) % nseg];
        V3 ch = vsub(s2->c, s->c), d = vsub(p, s->c);
        if (s->steep) {
            ch.y = 0;
            d.y = 0;
        }
        float cc2 = vdot(ch, ch), al = vdot(d, ch) / cc2;
        /* on the outside of a bend consecutive segments leave thin wedges that neither covers: accept a small
         * overshoot, clamped to the segment end, so a wheel crossing a wedge never loses the deck for a tick */
        if (al < -0.25f || al > 1.25f) {
            continue;
        }
        float alc = clampf(al, 0, 0.9999f);
        V3 e = vsub(p, vlerp(s->c, s2->c, alc));
        float h = vdot(e, s->N), lt = vdot(e, s->R), score = h * h + lt * lt + sq(al - alc) * cc2;
        if (score < best) {
            best = score;
            bj = j;
            bu = alc;
            bg = 1;
        }
    }
    if (!bg) {
        q->valid = 0;
        q->seg = hint;
        q->road = 0;
        return;
    }
    const Seg *s = &seg[bj], *s2 = &seg[(bj + 1) % nseg];
    V3 cc = vlerp(s->c, s2->c, bu);
    q->N = vnorm(vlerp(s->N, s2->N, bu));
    q->R = vnorm(vlerp(s->R, s2->R, bu));
    q->T = vnorm(vcross(q->N, q->R));
    V3 e = vsub(p, cc);
    q->h = vdot(e, q->N);
    q->lat = vdot(e, q->R);
    q->seg = bj;
    q->u = bu;
    q->road = !s->gap;
    q->valid = 1;
}
