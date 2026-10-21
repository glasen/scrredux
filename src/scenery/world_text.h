/* scenery/world_text.h - text on signs in the world */
#pragma once

/* lettering on a plane; o = top-left corner (top of the capitals), rows run along -up, c = 1/7 of the cap height */
typedef struct {
    V3 o, al, up;
    float cap;
    uint32_t col;
    int unlit;
} WorldGlyph;

static void world_glyph_tri(void *u, const float *t)
{
    const WorldGlyph *w = u;
#define WP(k) vadd(w->o, vadd(vmul(w->al, t[2 * (k)] * w->cap), vmul(w->up, (t[2 * (k) + 1] - 1) * w->cap)))
    if (w->unlit) {
        draw_tri_unlit(WP(0), WP(1), WP(2), w->col);
    } else {
        draw_tri(WP(0), WP(1), WP(2), w->col);
    }
#undef WP
}

static void world_text(V3 o, V3 al, V3 up, const char *s, float c, uint32_t col, int unlit)
{
    WorldGlyph w = {o, al, up, 7 * c, col, unlit};

    for (; *s; s++) {
        glyph_tris(*s, world_glyph_tri, &w);
        w.o = vadd(w.o, vmul(al, glyph_adv(*s) * w.cap));
    }
}

/* text centred on m, reading along 'rd' */
static void world_text_c(V3 m, V3 rd, V3 up, const char *s, float c, uint32_t col, int unlit)
{
    float tw = 0;

    for (const char *p = s; *p; p++) {
        tw += glyph_adv(*p) * 7 * c;
    }

    world_text(vadd(vsub(m, vmul(rd, tw / 2)), vmul(up, 3.5f * c)), rd, up, s, c, col, unlit);
}
