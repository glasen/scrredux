/* scenery/sponsors.h - sponsor boards (made up) */
#pragma once

/* ---- sponsors (made up) ---- */
#define NBRAND 10
static const char *BRAND[NBRAND] = {"TURBO OIL", "ACE TYRES", "V8 POWER", "STUNT COLA", "MEGA FUEL",
                                    "GT PARTS", "ROCKET", "HOT RODS", "SPEEDWAY", "BIG RED"};
static const uint32_t BR_BG[NBRAND] = {0x1E50C8, 0xF0C020, 0xC82018, 0xF2F2F2, 0x1E8A3C, 0x202024, 0xE86A18, 0x2858C0, 0xF2F2F2, 0xC81E18};
static const uint32_t BR_FG[NBRAND] = {0xF8F8F8, 0x1A1A1A, 0xF8F8F8, 0xC81E18, 0xF8F0A0, 0xF0C020, 0xF8F8F8, 0xF8E040, 0x1E50C8, 0xF8F8F8};
/* advertising board from a to a + al*w, h tall, readable from the side 'face' points to */
static void ad_board(V3 a, V3 al, V3 face, float w, float h, int b)
{
    b = ((b % NBRAND) + NBRAND) % NBRAND;
    V3 a1 = vadd(a, vmul(al, w));
    draw_quad(a, a1, vadd(a1, V(0, h, 0)), vadd(a, V(0, h, 0)), BR_BG[b]);
    V3 rd = vnorm(vcross(vmul(face, -1), UP));                 /* left to right for a viewer looking at the board */
    float c = fminf(h * 0.6f / 7, w * 0.86f / ((float)strlen(BRAND[b]) * 6));
    V3 m = vadd(vadd(vmul(vadd(a, a1), 0.5f), V(0, h / 2, 0)), vmul(face, 0.04f));
    world_text_c(m, rd, UP, BRAND[b], c, BR_FG[b], 0);
}
