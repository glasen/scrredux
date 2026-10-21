/* scenery/stadium.h - grandstands, floodlights, scoreboard */
#pragma once

/* ------------------------------------------------------------ stadium */
static const uint32_t SHIRT[12] = {0x2A2A30, 0x2C3C64, 0xE6E6E6, 0x8C8C98, 0xC23030, 0x3062C0,
                                   0xE0C040, 0x3A8A40, 0xE07028, 0x86AAD8, 0x7A5A3A, 0xB04070};
static const uint32_t SKIN[4] = {0xEEC4A4, 0xD8A882, 0xB07850, 0x7A5032};
static const uint32_t HAIR[4] = {0x2A1E16, 0x5A3A20, 0xC8A050, 0x1A1A1A};

/* terraced rows with seats and spectators. Row r: riser at depth d0 + r*rd up to y0 + (r+1)*rh, then the tread. */
static void stand_rows(V3 base, V3 al, V3 out, float len, float d0, float y0, int rows, float rd, float rh,
                       uint32_t seat, int seed, float dens, int vom)
{
    const float pitch = 0.62f, aisle = 1.3f;
    const int per = 20;
    float blk = per * pitch + aisle;
    int nb = (int)((len - aisle) / blk);
    if (nb < 1) {
        nb = 1;
    }
    float lead = (len - nb * blk + aisle) * 0.5f;
#define SP(d, y, a) vadd(base, vadd(vmul(out, d), vadd(V(0, y, 0), vmul(al, a))))
    int vr = rows / 3; /* vomitory rows vr .. vr+2 */
    for (int r = 0; r < rows; r++) {
        float dr = d0 + r * rd, yt = y0 + (r + 1) * rh;
        uint32_t cc = scalecol(CONCRETE, 0.95f + 0.05f * (r & 1));
        draw_quad(SP(dr, yt - rh, 0), SP(dr, yt - rh, len), SP(dr, yt, len), SP(dr, yt, 0), scalecol(cc, 0.86f));
        draw_quad(SP(dr, yt, 0), SP(dr, yt, len), SP(dr + rd, yt, len), SP(dr + rd, yt, 0), cc);
        for (int b = 0; b < nb; b++) {
            float a0 = lead + b * blk, a1 = a0 + per * pitch;
            int tunnel = vom && (b % 2 == 1) && r >= vr && r < vr + 3;
            float t0 = a0 + per * pitch * 0.5f - 1.4f, t1 = t0 + 2.8f;
            if (!tunnel) {
                draw_quad(SP(dr + rd * 0.8f, yt, a0), SP(dr + rd * 0.8f, yt, a1), SP(dr + rd * 0.8f, yt + 0.42f, a1),
                          SP(dr + rd * 0.8f, yt + 0.42f, a0), seat);
            } else {
                draw_quad(SP(dr + rd * 0.8f, yt, a0), SP(dr + rd * 0.8f, yt, t0), SP(dr + rd * 0.8f, yt + 0.42f, t0),
                          SP(dr + rd * 0.8f, yt + 0.42f, a0), seat);
                draw_quad(SP(dr + rd * 0.8f, yt, t1), SP(dr + rd * 0.8f, yt, a1), SP(dr + rd * 0.8f, yt + 0.42f, a1),
                          SP(dr + rd * 0.8f, yt + 0.42f, t1), seat);
            }
            float bd = dens * (0.7f + 0.6f * vnoise(b * 1.7f + seed, r * 0.21f, seed));
            for (int s = 0; s < per; s++) {
                float a = a0 + (s + 0.5f) * pitch;
                if (tunnel && a > t0 - 0.3f && a < t1 + 0.3f) {
                    continue;
                }
                uint32_t h = hash2(seed * 7919 + r * 131 + b, s * 17 + seed);
                if ((float)(h & 1023) / 1023.f > bd) {
                    continue;
                }
                a += ((float)((h >> 10) & 15) / 15.f - 0.5f) * 0.16f;
                int stand_up = ((h >> 14) & 31) == 0;
                float yb = yt + 0.08f,
                      ys = yb + (stand_up ? 1.0f : 0.56f) + ((float)((h >> 19) & 7) / 7.f - 0.5f) * 0.08f;
                float dp = dr + rd * 0.48f, hw = 0.2f + 0.03f * ((h >> 22) & 1);
                uint32_t shirt = SHIRT[(h >> 23) % 12], skin = SKIN[(h >> 27) & 3];
                draw_quad(SP(dp, yb, a - hw), SP(dp, yb, a + hw), SP(dp, ys, a + hw * 0.9f), SP(dp, ys, a - hw * 0.9f),
                          shirt);
                draw_quad(SP(dp, ys, a - 0.11f), SP(dp, ys, a + 0.11f), SP(dp, ys + 0.24f, a + 0.11f),
                          SP(dp, ys + 0.24f, a - 0.11f), skin);
                if ((h >> 29) & 1) {
                    draw_quad(SP(dp - 0.02f, ys + 0.17f, a - 0.12f), SP(dp - 0.02f, ys + 0.17f, a + 0.12f),
                              SP(dp - 0.02f, ys + 0.28f, a + 0.1f), SP(dp - 0.02f, ys + 0.28f, a - 0.1f),
                              HAIR[(h >> 4) & 3]);
                }
                if (stand_up && ((h >> 30) & 1)) { /* cheering: arms up */
                    draw_quad(SP(dp, ys - 0.1f, a - hw), SP(dp, ys - 0.1f, a - hw + 0.09f),
                              SP(dp, ys + 0.45f, a - hw - 0.1f), SP(dp, ys + 0.45f, a - hw - 0.19f), skin);
                    draw_quad(SP(dp, ys - 0.1f, a + hw), SP(dp, ys - 0.1f, a + hw - 0.09f),
                              SP(dp, ys + 0.45f, a + hw + 0.1f), SP(dp, ys + 0.45f, a + hw + 0.19f), skin);
                }
            }
            if (vom && (b % 2 == 1) && r == vr) { /* tunnel mouth */
                float yb0 = yt - rh, yb1 = y0 + (vr + 3) * rh + 0.5f, dq = dr - 0.02f;
                float dk = d0 + (vr + 3) * rd;
                draw_quad(SP(dk, yb0, t0), SP(dk, yb0, t1), SP(dk, yb1, t1), SP(dk, yb1, t0), 0x18181C);
                for (int e = 0; e < 2; e++) {
                    float ae = e ? t1 : t0;
                    draw_quad(SP(dq, yb0, ae), SP(dk, yb0, ae), SP(dk, yb1, ae), SP(dq, yb1, ae),
                              scalecol(CONCRETE, 0.9f));
                }
                draw_quad(SP(dq, yb1, t0 - 0.3f), SP(dq, yb1, t1 + 0.3f), SP(dk, yb1, t1 + 0.3f),
                          SP(dk, yb1, t0 - 0.3f), CONCRETE);
                draw_quad(SP(dq, yb1 - 0.5f, t0 - 0.3f), SP(dq, yb1 - 0.5f, t1 + 0.3f), SP(dq, yb1, t1 + 0.3f),
                          SP(dq, yb1, t0 - 0.3f), scalecol(CONCRETE, 0.9f));
                cyl(SP(dq, yb1, t0 - 0.3f), SP(dq, yb1 + 0.9f, t0 - 0.3f), 0.03f, 0.03f, 4, 0xD0D0D8, 0); /* railing */
                cyl(SP(dq, yb1, t1 + 0.3f), SP(dq, yb1 + 0.9f, t1 + 0.3f), 0.03f, 0.03f, 4, 0xD0D0D8, 0);
                cyl(SP(dq, yb1 + 0.9f, t0 - 0.3f), SP(dq, yb1 + 0.9f, t1 + 0.3f), 0.03f, 0.03f, 4, 0xD0D0D8, 0);
            }
        }
    }
#undef SP
}

/* a grandstand facing the arena: a podium wall, terraced seating, an optional VIP band with an upper tier, a back
 * wall with pilasters and an optional cantilever roof with a coloured fascia. base = front-left corner on the ground. */
static void grandstand(V3 base, V3 al, V3 out, float len, int rows, int upper, int roofed, uint32_t seat,
                       uint32_t accent, int seed)
{
    const float rd = 0.85f, rh = 0.46f, ph = 2.4f;
#define SP(d, y, a) vadd(base, vadd(vmul(out, d), vadd(V(0, y, 0), vmul(al, a))))
    draw_quad(SP(0, 0, 0), SP(0, 0, len), SP(0, ph + 1.05f, len), SP(0, ph + 1.05f, 0),
              scalecol(CONCRETE, 0.92f)); /* podium wall */
    draw_quad(SP(0, ph + 1.05f, 0), SP(0, ph + 1.05f, len), SP(0.25f, ph + 1.05f, len), SP(0.25f, ph + 1.05f, 0),
              0xC8C6C0);
    draw_quad(SP(0.02f, ph + 0.35f, 0), SP(0.02f, ph + 0.35f, len), SP(0.02f, ph + 0.75f, len),
              SP(0.02f, ph + 0.75f, 0), accent);
    draw_quad(SP(0.25f, ph, 0), SP(0.25f, ph, len), SP(1.0f, ph, len), SP(1.0f, ph, 0), CONCRETE); /* front walkway */
    draw_quad(SP(0.25f, 0, 0), SP(0.25f, 0, len), SP(0.25f, ph + 1.05f, len), SP(0.25f, ph + 1.05f, 0),
              scalecol(CONCRETE, 0.8f));
    stand_rows(base, al, out, len, 1.0f, ph, rows, rd, rh, seat, seed, 0.86f, 1);
    float D = 1.0f + rows * rd, Y = ph + rows * rh, Dt = D, Yt = Y;
    if (upper) {
        const float vb = 3.4f, rh2 = rh * 1.2f;
        draw_quad(SP(D + 0.5f, Y, 0), SP(D + 0.5f, Y, len), SP(D + 0.5f, Y + vb - 0.6f, len),
                  SP(D + 0.5f, Y + vb - 0.6f, 0), 0x34506E); /* VIP glass */
        for (float a = 0; a <= len + 0.01f; a += 3.f) {
            obox(SP(D + 0.45f, Y + (vb - 0.6f) / 2, a), al, UP, out, 0.08f, (vb - 0.6f) / 2, 0.08f, 0xD8D8DC);
        }
        draw_quad(SP(D, Y, 0), SP(D, Y, len), SP(D + 0.5f, Y, len), SP(D + 0.5f, Y, 0), CONCRETE);
        draw_quad(SP(D + 0.3f, Y + vb - 0.6f, 0), SP(D + 0.3f, Y + vb - 0.6f, len), SP(D + 0.3f, Y + vb + 0.9f, len),
                  SP(D + 0.3f, Y + vb + 0.9f, 0), accent);
        draw_quad(SP(D + 0.3f, Y + vb - 0.6f, 0), SP(D + 0.3f, Y + vb - 0.6f, len), SP(D + 0.5f, Y + vb - 0.6f, len),
                  SP(D + 0.5f, Y + vb - 0.6f, 0), 0x50525A);
        int nb = (int)(len / 36);
        for (int k = 0; k < nb; k++) { /* sponsor names on the fascia */
            world_text_c(SP(D + 0.26f, Y + vb + 0.15f, (k + 0.5f) * len / nb), vnorm(vcross(out, UP)), UP,
                         BRAND[(seed + k) % NBRAND], 0.17f, 0xF8F8F8, 0);
        }
        stand_rows(base, al, out, len, D + 0.9f, Y + vb - 0.6f, upper, rd, rh2, seat, seed + 7, 0.78f, 0);
        Dt = D + 0.9f + upper * rd;
        Yt = Y + vb - 0.6f + upper * rh2;
    }
    /* back wall with pilasters, end walls */
    draw_quad(SP(Dt, 0, 0), SP(Dt, 0, len), SP(Dt, Yt + 1.2f, len), SP(Dt, Yt + 1.2f, 0), 0x8E8C88);
    for (float a = 0; a <= len + 0.01f; a += 7.f) {
        obox(SP(Dt + 0.3f, (Yt + 1.2f) / 2, a), al, UP, out, 0.35f, (Yt + 1.2f) / 2, 0.3f, 0x7E7C78);
    }
    for (int e = 0; e < 2; e++) {
        float a = e ? len : 0;
        draw_quad(SP(0, 0, a), SP(Dt, 0, a), SP(Dt, Yt + 1.2f, a), SP(0, ph + 1.05f, a), scalecol(CONCRETE, 0.86f));
    }
    if (roofed) {
        float yb = Yt + 4.2f, yf = yb + 1.4f, df = -1.0f, db = Dt + 0.6f;
        uint32_t top = 0xB8BCC4, under = 0x4E5058;
        draw_quad(SP(df, yf, 0), SP(df, yf, len), SP(db, yb, len), SP(db, yb, 0), top);
        draw_quad(SP(df, yf - 0.45f, 0), SP(df, yf - 0.45f, len), SP(db, yb - 0.45f, len), SP(db, yb - 0.45f, 0),
                  under);
        draw_quad(SP(df, yf - 1.1f, 0), SP(df, yf - 1.1f, len), SP(df, yf + 0.3f, len), SP(df, yf + 0.3f, 0),
                  accent); /* fascia */
        draw_quad(SP(df, yf - 1.1f, 0), SP(df, yf - 1.1f, len), SP(df + 0.5f, yf - 0.45f, len),
                  SP(df + 0.5f, yf - 0.45f, 0), scalecol(accent, 0.7f));
        for (int e = 0; e < 2; e++) {
            float a = e ? len : 0;
            draw_quad(SP(df, yf - 1.1f, a), SP(df, yf + 0.3f, a), SP(db, yb, a), SP(db, yb - 0.45f, a),
                      scalecol(top, 0.8f));
        }
        for (int k = 0; k * 22.f < len + 1; k++) { /* trusses and back columns */
            float a = fminf(k * 22.f, len);
            cyl(SP(db + 0.2f, 0, a), SP(db + 0.2f, yb + 5.5f, a), 0.32f, 0.26f, 8, 0xD8D8DE, 3);
            cyl(SP(db + 0.2f, yb + 5.4f, a), SP(df + 0.5f, yf + 0.25f, a), 0.12f, 0.12f, 6, 0xD8D8DE, 0);
            cyl(SP(db + 0.2f, yb - 0.5f, a), SP(df + 0.5f, yf - 0.5f, a), 0.18f, 0.14f, 6, 0x8A8C94, 0);
        }
        for (int k = 0; k * 11.f < len - 4; k++) { /* flags along the roof */
            flag(SP(df + 0.3f, yf + 0.3f, k * 11.f + 5.5f), 3.2f,
                 (uint32_t[]){0xF0D020, 0xD02820, 0x2858C0, 0xF8F8F8, 0x20A040}[(k + seed) % 5]);
        }
    }
#undef SP
}

/* stair and service block in the gaps between the stands */
static void stair_block(V3 base, V3 al, V3 out, float w, float d, float h)
{
    Frm f = frm_at(vadd(base, vmul(al, w / 2)), out);
    fbox(&f, -w / 2, w / 2, 0, h, 0, d, 0x96948E);
    fbox(&f, -w / 2 - 0.2f, w / 2 + 0.2f, h, h + 0.4f, -0.2f, d + 0.2f, 0x7E7C78);
    for (int k = 0; k < 3; k++) {
        fpanel_z(&f, -0.02f, -w / 2 + 0.8f + k * (w - 1.6f) / 2.f - 0.6f, -w / 2 + 0.8f + k * (w - 1.6f) / 2.f + 0.6f,
                 0, 2.3f, 0x2A2E36);
    }
    for (int l = 1; l * 3.2f < h - 1; l++) {
        fpanel_z(&f, -0.02f, -w / 2 + 0.6f, w / 2 - 0.6f, l * 3.2f + 0.9f, l * 3.2f + 1.7f, 0x34506E);
    }
}

/* floodlight: tapered steel mast, service platform and a lamp head facing the arena. The head is an open box whose
 * lamps fill the front plane, so no two surfaces lie on top of each other (no z-fighting at a distance), and nothing
 * is thinner than a pixel from the track. */
static void floodlight(float x, float z, float face)
{
    const float H = 46;
    V3 c = V(x, 0, z), F = V(cosf(face), 0, sinf(face)), Rr = vcross(F, UP);
    cyl(c, vadd(c, V(0, H, 0)), 1.1f, 0.55f, 12, 0xB8BAC2, 2);
    cyl(vadd(c, V(0, H * 0.55f, 0)), vadd(c, V(0, H * 0.55f + 0.4f, 0)), 1.6f, 1.6f, 12, 0x8A8C94,
        3);                                                                  /* rest platform */
    obox(vadd(c, V(0, H + 0.2f, 0)), Rr, UP, F, 3.0f, 0.2f, 1.6f, 0x6A6C74); /* head platform */
    const float hw = 6.0f, hh = 3.8f, dep = 0.9f;
    V3 hc = vadd(vadd(c, V(0, H + 0.6f + hh, 0)), vmul(F, 0.9f)); /* centre of the lamp plane */
#define HP(u, v, w) vadd(hc, vadd(vmul(Rr, u), vadd(V(0, v, 0), vmul(F, w))))
    draw_quad(HP(-hw, -hh, -dep), HP(hw, -hh, -dep), HP(hw, hh, -dep), HP(-hw, hh, -dep), 0x2A2C32); /* back */
    draw_quad(HP(-hw, hh, -dep), HP(hw, hh, -dep), HP(hw, hh, 0), HP(-hw, hh, 0), 0x5A5C64);         /* top */
    draw_quad(HP(-hw, -hh, -dep), HP(hw, -hh, -dep), HP(hw, -hh, 0), HP(-hw, -hh, 0), 0x5A5C64);     /* bottom */
    for (int s = -1; s <= 1; s += 2) {
        draw_quad(HP(s * hw, -hh, -dep), HP(s * hw, hh, -dep), HP(s * hw, hh, 0), HP(s * hw, -hh, 0), 0x5A5C64);
    }
    const int nc = 4, nr = 3;
    const float gap = 0.6f;
    float lw = (2 * hw - 0.6f - (nc - 1) * gap) / nc, lh = (2 * hh - 0.6f - (nr - 1) * gap) / nr;
    for (int r = 0; r < nr; r++) {
        for (int q = 0; q < nc; q++) {
            float u0 = -hw + 0.3f + q * (lw + gap), v0 = -hh + 0.3f + r * (lh + gap);
            draw_quad_unlit(HP(u0, v0, 0), HP(u0 + lw, v0, 0), HP(u0 + lw, v0 + lh, 0), HP(u0, v0 + lh, 0), 0xFFF6D0);
        }
    }
    obox(vadd(c, vadd(V(0, H + 0.6f + hh, 0), vmul(F, -0.45f))), Rr, UP, F, 1.2f, 1.2f, 0.4f,
         0x6A6C74); /* bracket to the head */
#undef HP
}

/* scoreboard on legs behind the west terrace */
static void scoreboard(V3 c, V3 face)
{
    V3 rd = vnorm(vcross(vmul(face, -1), UP));
    const float W = 15, H = 6.5f, y0 = 16;
    for (int s = -1; s <= 1; s += 2) {
        V3 b = vadd(vadd(c, vmul(rd, s * W * 0.6f)), vmul(face, -1.5f));
        cyl(b, vadd(b, V(0, y0 + 1, 0)), 0.7f, 0.6f, 8, 0x8A8C94, 3);
    }
    V3 m = vadd(c, V(0, y0 + H, 0));
    obox(vadd(m, vmul(face, -0.9f)), rd, UP, face, W + 0.6f, H + 0.6f, 0.8f, 0x26262C);
    draw_quad_unlit(vadd(vadd(m, vmul(rd, -W)), V(0, -H, 0)), vadd(vadd(m, vmul(rd, W)), V(0, -H, 0)),
                    vadd(vadd(m, vmul(rd, W)), V(0, H, 0)), vadd(vadd(m, vmul(rd, -W)), V(0, H, 0)), 0x0C0E14);
    V3 tf = vmul(face, 0.03f);
    world_text_c(vadd(vadd(m, V(0, 3.4f, 0)), tf), rd, UP, "SCR REDUX", 0.42f, 0xF8D030, 1);
    world_text_c(vadd(vadd(m, V(0, -0.6f, 0)), tf), rd, UP, "LAP RECORD", 0.3f, 0xF0F0F0, 1);
    world_text_c(vadd(vadd(m, V(0, -3.8f, 0)), tf), rd, UP, "0:49.7", 0.36f, 0xF04030, 1);
    ad_board(vadd(vadd(m, vmul(rd, -W)), V(0, -H - 2.6f, 0)), rd, face, 2 * W, 1.8f, 3);
}

/* the stadium: perimeter wall with sponsor boards, grandstands of different kinds, stair blocks, gate,
 * scoreboard, floodlights */
static void build_stadium(void)
{
    float Lx = ar_x1 - ar_x0, Lz = ar_z1 - ar_z0;
    int bi = 0;
    for (int side = 0; side < 4; side++) {
        V3 base = side == 0   ? V(ar_x0, 0, ar_z0)
                  : side == 1 ? V(ar_x1, 0, ar_z0)
                  : side == 2 ? V(ar_x1, 0, ar_z1)
                              : V(ar_x0, 0, ar_z1);
        V3 along = side == 0 ? V(1, 0, 0) : side == 1 ? V(0, 0, 1) : side == 2 ? V(-1, 0, 0) : V(0, 0, -1);
        V3 out = side == 0 ? V(0, 0, -1) : side == 1 ? V(1, 0, 0) : side == 2 ? V(0, 0, 1) : V(-1, 0, 0);
        V3 in = vmul(out, -1);
        float L = (side & 1) ? Lz : Lx;
        float gate = side == 1 ? gate_z - ar_z0 : -100;
        /* perimeter wall: red and white crash barrier, sponsor boards above, white coping */
        int nst = (int)(L / 3.0f);
        for (int k = 0; k < nst; k++) {
            float a0 = k * 3.0f, a1 = (k + 1) * 3.0f;
            if (a1 > gate - 6 && a0 < gate + 6) {
                continue;
            }
            V3 p0 = vadd(base, vmul(along, a0)), p1 = vadd(base, vmul(along, a1));
            draw_quad(p0, p1, vadd(p1, V(0, 1.1f, 0)), vadd(p0, V(0, 1.1f, 0)), (k & 1) ? 0xE8E8E8 : 0xB02020);
        }
        for (float a = 0; a + 8 <= L; a += 8) {
            if (a + 8 > gate - 6 && a < gate + 6) {
                continue;
            }
            ad_board(vadd(vadd(base, vmul(along, a)), V(0, 1.1f, 0)), along, in, 8, 1.5f, bi++);
            draw_quad(vadd(vadd(base, vmul(along, a)), V(0, 2.6f, 0)),
                      vadd(vadd(base, vmul(along, a + 8)), V(0, 2.6f, 0)),
                      vadd(vadd(vadd(base, vmul(along, a + 8)), V(0, 2.6f, 0)), vmul(out, 0.5f)),
                      vadd(vadd(vadd(base, vmul(along, a)), V(0, 2.6f, 0)), vmul(out, 0.5f)), 0xEEEEEA);
        }
        /* catch net above the boards (down to the ground across the gate): steel posts, a mesh of thin strands, cables */
        {
            V3 nb = vadd(base, vmul(out, 0.2f));
            for (float a = 0; a <= L + 0.01f; a += 8) {
                if (a > gate - 7 && a < gate + 7) {
                    continue;
                }
                obox(vadd(vadd(vadd(nb, vmul(along, a)), vmul(out, 0.1f)), V(0, NET_H / 2 + 0.15f, 0)), along, UP, out,
                     0.09f, NET_H / 2 + 0.15f, 0.09f, 0x70747A);
            }
            for (float a = 0; a + 0.5f <= L + 0.01f; a += 0.5f) {
                float y0 = a + 0.5f > gate - 6 && a < gate + 6 ? 0.f : 2.6f;
                V3 p = vadd(nb, vmul(along, a));
                draw_quad(vadd(p, V(0, y0, 0)), vadd(vadd(p, vmul(along, 0.03f)), V(0, y0, 0)),
                          vadd(vadd(p, vmul(along, 0.03f)), V(0, NET_H, 0)), vadd(p, V(0, NET_H, 0)), 0x3A3E44);
            }
            for (float y = 0.5f; y < NET_H; y += 0.5f) {
                float th = y == 5.f ? 0.07f : 0.03f; /* the middle cable */
                if (y < 2.6f) {                      /* below the boards only across the gate */
                    V3 g0 = vadd(nb, vmul(along, gate - 6)), g1 = vadd(nb, vmul(along, gate + 6));
                    if (gate > 0) {
                        draw_quad(vadd(g0, V(0, y, 0)), vadd(g1, V(0, y, 0)), vadd(g1, V(0, y + th, 0)),
                                  vadd(g0, V(0, y + th, 0)), 0x3A3E44);
                    }
                    continue;
                }
                V3 e = vadd(nb, vmul(along, L));
                draw_quad(vadd(nb, V(0, y, 0)), vadd(e, V(0, y, 0)), vadd(e, V(0, y + th, 0)),
                          vadd(nb, V(0, y + th, 0)), th > 0.05f ? 0x2A2C30 : 0x3A3E44);
            }
            V3 e = vadd(nb, vmul(along, L));
            draw_quad(vadd(nb, V(0, NET_H - 0.08f, 0)), vadd(e, V(0, NET_H - 0.08f, 0)), vadd(e, V(0, NET_H, 0)),
                      vadd(nb, V(0, NET_H, 0)), 0x2A2C30);
        }
        /* stands with stair blocks between them */
        int rows = side == 0 ? 16 : side == 2 ? 20 : 12, upper = side == 0 ? 9 : 0, roofed = side == 0 || side == 2;
        uint32_t seat = side == 0 ? 0x2A54B4 : side == 2 ? 0xB42A2A : side == 1 ? 0xE0B020 : 0x2A8A44;
        uint32_t accent = side == 0 ? 0xC81E18 : side == 2 ? 0x2858C0 : side == 1 ? 0x1E8A3C : 0xE8B020;
        float gapw = 12, start = 22, end = L - 22;
        int nseg_st = (int)ceilf((end - start) / 110.f);
        float sl = (end - start - (nseg_st - 1) * gapw) / nseg_st;
        float off = 7.f;
        for (int k = 0; k < nseg_st; k++) {
            float a = start + k * (sl + gapw);
            float a0 = a, a1 = a + sl;
            if (side == 1 && gate > a0 - 8 && gate < a1 + 8) { /* leave room for the gate */
                float g0 = gate - 9, g1 = gate + 9;
                if (g0 - a0 > 20) {
                    grandstand(vadd(vadd(base, vmul(along, a0)), vmul(out, off)), along, out, g0 - a0, rows, upper,
                               roofed, seat, accent, side * 31 + k);
                }
                if (a1 - g1 > 20) {
                    grandstand(vadd(vadd(base, vmul(along, g1)), vmul(out, off)), along, out, a1 - g1, rows, upper,
                               roofed, seat, accent, side * 31 + k + 50);
                }
            } else {
                grandstand(vadd(vadd(base, vmul(along, a0)), vmul(out, off)), along, out, sl, rows, upper, roofed, seat,
                           accent, side * 31 + k);
            }
            if (k + 1 < nseg_st) {
                float depth = 1.0f + (rows + upper) * 0.85f + (upper ? 1 : 0);
                stair_block(vadd(vadd(base, vmul(along, a1 + 0.5f)), vmul(out, off + depth * 0.25f)), along, out,
                            gapw - 1.0f, depth * 0.75f, 2.4f + rows * 0.46f + (upper ? 9 : 2));
            }
        }
        /* walkway between wall and stands */
        draw_quad(vadd(base, V(0, 0.02f, 0)), vadd(vadd(base, vmul(along, L)), V(0, 0.02f, 0)),
                  vadd(vadd(vadd(base, vmul(along, L)), vmul(out, off)), V(0, 0.02f, 0)),
                  vadd(vadd(base, vmul(out, off)), V(0, 0.02f, 0)), 0x8C8A84);
        if (side == 1) { /* gate: posts, sign, open doors */
            V3 g = vadd(base, vmul(along, gate));
            for (int s = -1; s <= 1; s += 2) {
                obox(vadd(vadd(g, vmul(along, s * 6.6f)), V(0, 3.5f, 0)), along, UP, out, 0.6f, 3.5f, 0.6f, 0x8E8C88);
            }
            obox(vadd(g, V(0, 7.6f, 0)), along, UP, out, 7.2f, 0.7f, 0.6f, 0x2858C0);
            world_text_c(vadd(vadd(g, V(0, 7.6f, 0)), vmul(in, 0.62f)), vnorm(vcross(out, UP)), UP, "SERVICE GATE",
                         0.14f, 0xF8F8F8, 0);
            for (int s = -1; s <= 1; s += 2) {
                V3 h0 = vadd(g, vmul(along, s * 6.0f)), h1 = vadd(h0, vmul(in, 5.6f));
                draw_quad(h0, h1, vadd(h1, V(0, 3.2f, 0)), vadd(h0, V(0, 3.2f, 0)), 0x5A6A72);
            }
            draw_quad(vadd(vadd(g, vmul(along, -6)), V(0, 0.03f, 0)), vadd(vadd(g, vmul(along, 6)), V(0, 0.03f, 0)),
                      vadd(vadd(vadd(g, vmul(along, 6)), vmul(out, 40)), V(0, 0.03f, 0)),
                      vadd(vadd(vadd(g, vmul(along, -6)), vmul(out, 40)), V(0, 0.03f, 0)), 0x6E6C68);
        }
        if (side == 3) {
            scoreboard(vadd(vadd(base, vmul(along, L * 0.5f)), vmul(out, off + 1.0f + 12 * 0.85f + 4.f)), in);
        }
    }
    floodlight(ar_x0 - 12, ar_z0 - 12, PI_F / 4);
    floodlight(ar_x1 + 12, ar_z0 - 12, 3 * PI_F / 4);
    floodlight(ar_x1 + 12, ar_z1 + 12, -3 * PI_F / 4);
    floodlight(ar_x0 - 12, ar_z1 + 12, -PI_F / 4);
}
