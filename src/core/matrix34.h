/* core/matrix34.h - 3x4 affine matrices (rows like car_body_rows()) */
#pragma once

/* ---- 3x4 affine matrices, rows like car_body_rows() ---- */
static V3 m34_apply(const float m[12], V3 p)
{
    return V(m[0] * p.x + m[1] * p.y + m[2] * p.z + m[3], m[4] * p.x + m[5] * p.y + m[6] * p.z + m[7],
             m[8] * p.x + m[9] * p.y + m[10] * p.z + m[11]);
}

static void m34_mul(const float a[12], const float b[12], float o[12]) /* o = a after b */
{
    float r[12];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 4; j++) {
            r[i * 4 + j] =
                a[i * 4] * b[j] + a[i * 4 + 1] * b[4 + j] + a[i * 4 + 2] * b[8 + j] + (j == 3 ? a[i * 4 + 3] : 0);
        }
    }
    memcpy(o, r, sizeof r);
}

static void m34_set3(float m[12], const float R[9], V3 p) /* linear part R about the fixed point p */
{
    for (int i = 0; i < 3; i++) {
        m[i * 4] = R[i * 3];
        m[i * 4 + 1] = R[i * 3 + 1];
        m[i * 4 + 2] = R[i * 3 + 2];
    }
    V3 q = V(R[0] * p.x + R[1] * p.y + R[2] * p.z, R[3] * p.x + R[4] * p.y + R[5] * p.z,
             R[6] * p.x + R[7] * p.y + R[8] * p.z);
    m[3] = p.x - q.x;
    m[7] = p.y - q.y;
    m[11] = p.z - q.z;
}

static void rot3(V3 ax, float an, float R[9])
{
    ax = vnorm(ax);
    float c = cosf(an), s = sinf(an), t = 1 - c;
    float r[9] = {t * ax.x * ax.x + c,        t * ax.x * ax.y - s * ax.z, t * ax.x * ax.z + s * ax.y,
                  t * ax.x * ax.y + s * ax.z, t * ax.y * ax.y + c,        t * ax.y * ax.z - s * ax.x,
                  t * ax.x * ax.z - s * ax.y, t * ax.y * ax.z + s * ax.x, t * ax.z * ax.z + c};
    memcpy(R, r, sizeof r);
}

static void m34_rot(V3 p, V3 ax, float an, float m[12])
{
    float R[9];
    rot3(ax, an, R);
    m34_set3(m, R, p);
}

/* about p: turn direction d0 onto d1; with stretch also scale along d0 so the far end lands on p + d1 */
static void m34_aim(V3 p, V3 d0, V3 d1, int stretch, float m[12])
{
    V3 a = vnorm(d0), b = vnorm(d1), ax = vcross(a, b);
    float s = vlen(ax), R[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1};
    if (s > 1e-7f) {
        rot3(ax, atan2f(s, vdot(a, b)), R);
    }
    if (stretch) {
        float k = vlen(d1) / fmaxf(vlen(d0), 1e-6f) - 1, S[9], RS[9], av[3] = {a.x, a.y, a.z};
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                S[i * 3 + j] = (i == j) + k * av[i] * av[j];
            }
        }
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                RS[i * 3 + j] = R[i * 3] * S[j] + R[i * 3 + 1] * S[3 + j] + R[i * 3 + 2] * S[6 + j];
            }
        }
        memcpy(R, RS, sizeof R);
    }
    m34_set3(m, R, p);
}

static void m34_shift(float m[12], V3 d)
{
    m[3] += d.x;
    m[7] += d.y;
    m[11] += d.z;
}
