/* scenery/frames.h - local frames and boxes, cylinders and panels in them */
#pragma once

/* static scenery: the arena floor and its props, service vehicles, the stadium and its surroundings.
 *  */

/* ---- local frames: o = origin on the ground, F forward, U up, R across ---- */
typedef struct { V3 o, R, U, F; } Frm;
static Frm frm_at(V3 o, V3 F)
{
    Frm f; f.o = o; f.F = vnorm(V(F.x, 0, F.z)); f.U = UP; f.R = vcross(f.F, UP); return f;
}
static Frm frm_ground(float x, float z, float ang) { return frm_at(V(x, ground_h(x, z), z), V(cosf(ang), 0, sinf(ang))); }
static V3 fpt(const Frm *f, float x, float y, float z) { return vadd(f->o, vadd(vmul(f->R, x), vadd(vmul(f->U, y), vmul(f->F, z)))); }
static void fbox(const Frm *f, float x0, float x1, float y0, float y1, float z0, float z1, uint32_t col)
{
    obox(fpt(f, (x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), f->R, f->U, f->F, (x1 - x0) / 2, (y1 - y0) / 2, (z1 - z0) / 2, col);
}
static void fcyl(const Frm *f, float x0, float y0, float z0, float x1, float y1, float z1, float r, int n, uint32_t col, int caps)
{
    cyl(fpt(f, x0, y0, z0), fpt(f, x1, y1, z1), r, r, n, col, caps);
}
/* side profile ({z, y} pairs, star-shaped around the first point) extruded across x0..x1 */
static void fprism(const Frm *f, const float (*p)[2], int n, float x0, float x1, uint32_t col)
{
    for (int k = 0; k < n; k++) {
        int k2 = (k + 1) % n;
        draw_quad(fpt(f, x0, p[k][1], p[k][0]), fpt(f, x0, p[k2][1], p[k2][0]), fpt(f, x1, p[k2][1], p[k2][0]), fpt(f, x1, p[k][1], p[k][0]), col);
    }
    for (int k = 1; k + 1 < n; k++) {
        draw_tri(fpt(f, x0, p[0][1], p[0][0]), fpt(f, x0, p[k][1], p[k][0]), fpt(f, x0, p[k + 1][1], p[k + 1][0]), col);
        draw_tri(fpt(f, x1, p[0][1], p[0][0]), fpt(f, x1, p[k][1], p[k][0]), fpt(f, x1, p[k + 1][1], p[k + 1][0]), col);
    }
}
/* panel in the plane x = const */
static void fpanel_x(const Frm *f, float x, float z0, float y0, float z1, float y1, float z2, float y2, float z3, float y3, uint32_t col)
{
    draw_quad(fpt(f, x, y0, z0), fpt(f, x, y1, z1), fpt(f, x, y2, z2), fpt(f, x, y3, z3), col);
}
/* panel in the plane z = const */
static void fpanel_z(const Frm *f, float z, float x0, float x1, float y0, float y1, uint32_t col)
{
    draw_quad(fpt(f, x0, y0, z), fpt(f, x1, y0, z), fpt(f, x1, y1, z), fpt(f, x0, y1, z), col);
}
