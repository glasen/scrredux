/* ui/hud_text.h - HUD text drawing and time formatting */
#pragma once

/* text: sc is the old 5x7 font's pixel size, so capitals are 7 * sc tall with their top at y */
static float text_width(const char *s, float sc)
{
    float w = 0;
    for (; *s; s++) {
        w += glyph_adv(*s);
    }
    return w * 7 * sc;
}
typedef struct {
    float x, base, cap, shear;
    uint32_t ctop, cbot;
} HudGlyph;
static void hud_glyph_tri(void *u, const float *t)
{
    const HudGlyph *h = u;
#define GV(k)                                                                                                          \
    h->x + (t[2 * k] + t[2 * k + 1] * h->shear) * h->cap, h->base - t[2 * k + 1] * h->cap,                             \
        (h->ctop == h->cbot ? h->ctop : mixcol(h->ctop, h->cbot, clampf(1 - t[2 * k + 1], 0, 1)))
    hud_tri3(GV(0), GV(1), GV(2));
#undef GV
}
/* rows shade from col_top to col_bot; shear > 0 leans the letters forward */
static void hud_text_ex(float x, float y, const char *s, float sc, uint32_t ctop, uint32_t cbot, float shear)
{
    HudGlyph h = {x, y + 7 * sc, 7 * sc, shear, ctop, cbot};
    for (; *s; s++) {
        glyph_tris(*s, hud_glyph_tri, &h);
        h.x += glyph_adv(*s) * h.cap;
    }
}
static void hud_text_raw(float x, float y, const char *s, float sc, uint32_t col)
{
    hud_text_ex(x, y, s, sc, col, col, 0);
}
static void hud_text(float x, float y, const char *s, float sc, uint32_t col)
{
    hud_text_raw(x + sc * 0.6f, y + sc * 0.6f, s, sc, 0x101018);
    hud_text_raw(x, y, s, sc, col);
}
static void hud_text_c(float y, const char *s, float sc, uint32_t col)
{
    hud_text((HW - text_width(s, sc)) / 2, y, s, sc, col);
}
static void fmt_time(char *b, size_t n, float t)
{
    int m = (int)(t / 60);
    float s = t - m * 60;
    snprintf(b, n, "%d:%04.1f", m, s);
}
