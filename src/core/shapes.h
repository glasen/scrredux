/* core/shapes.h - solid shapes built from triangles: oriented boxes and tapered cylinders */
#pragma once

static void obox(V3 c, V3 ax, V3 ay, V3 az, float hx, float hy, float hz, uint32_t col)
{
    V3 X = vmul(ax, hx), Y = vmul(ay, hy), Z = vmul(az, hz), v[8];
    for (int i = 0; i < 8; i++) {
        float sx = (i & 1) ? 1.f : -1.f, sy = (i & 2) ? 1.f : -1.f, sz = (i & 4) ? 1.f : -1.f;
        v[i] = vadd(c, vadd(vmul(X, sx), vadd(vmul(Y, sy), vmul(Z, sz))));
    }
    static const int f[6][4] = {{0,1,3,2},{4,5,7,6},{0,4,5,1},{2,3,7,6},{0,2,6,4},{1,5,7,3}};
    for (int i = 0; i < 6; i++) draw_quad(v[f[i][0]], v[f[i][1]], v[f[i][2]], v[f[i][3]], col);
}
/* tapered cylinder between two points; caps: 1 = start, 2 = end */
static void cyl(V3 a, V3 b, float r0, float r1, int n, uint32_t col, int caps)
{
    V3 ax = vsub(b, a); float L = vlen(ax);
    if (L < 1e-6f) return;
    ax = vmul(ax, 1 / L);
    V3 t = fabsf(ax.y) < 0.9f ? UP : V(1, 0, 0);
    V3 u = vnorm(vcross(ax, t)), w = vcross(ax, u), pa[32], pb[32];
    if (n > 32) n = 32;
    for (int k = 0; k < n; k++) {
        float an = 2 * PI_F * k / n;
        V3 d = vadd(vmul(u, cosf(an)), vmul(w, sinf(an)));
        pa[k] = vadd(a, vmul(d, r0)); pb[k] = vadd(b, vmul(d, r1));
    }
    for (int k = 0; k < n; k++) {
        int k2 = (k + 1) % n;
        draw_quad(pa[k], pa[k2], pb[k2], pb[k], col);
        if (caps & 1) draw_tri(a, pa[k2], pa[k], col);
        if (caps & 2) draw_tri(b, pb[k], pb[k2], col);
    }
}
