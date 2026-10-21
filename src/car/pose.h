/* car/pose.h - body frame and wheel hub positions */
#pragma once

/* ----------------------------------------------------------- car pose */
/* body frame including the visual roll/pitch lean */
static void body_frame(const Car *c, V3 *R, V3 *U, V3 *F)
{
    float cp = cosf(c->lean_pitch), sp = sinf(c->lean_pitch), cr = cosf(c->lean_roll), sr = sinf(c->lean_roll);
    V3 Fv = vadd(vmul(c->F, cp), vmul(c->U, sp)), Uv = vsub(vmul(c->U, cp), vmul(c->F, sp));
    *U = vadd(vmul(Uv, cr), vmul(c->R, sr));
    *R = vsub(vmul(c->R, cr), vmul(Uv, sr));
    *F = Fv;
}

/* model -> world matrix rows (3 x vec4) for the body: leans about a low roll centre */
static void car_body_rows(const Car *c, float m[12])
{
    V3 R, U, F;
    body_frame(c, &R, &U, &F);
    const V3 pv = {0.f, -0.45f, 0.f};
    V3 t = vadd(c->x, vsub(vadd(vmul(c->R, pv.x), vadd(vmul(c->U, pv.y), vmul(c->F, pv.z))),
                           vadd(vmul(R, pv.x), vadd(vmul(U, pv.y), vmul(F, pv.z)))));
    float r[12] = {R.x, U.x, F.x, t.x, R.y, U.y, F.y, t.y, R.z, U.z, F.z, t.z};
    memcpy(m, r, sizeof r);
}

static V3 car_hub(const Car *c, int i)
{
    return vsub(body_pt(c, WMOUNT[i]), vmul(c->U, c->suspension_len[i]));
}
