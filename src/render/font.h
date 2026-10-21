/* render/font.h - the text font as triangles (glyph access) */
#pragma once

/* the text font: Rajdhani Bold as triangles (font_vec.h, made by tools/gen_font.py); cap height 1, y up */
/* calls tri() for each of ch's triangles (x0, y0, x1, y1, x2, y2) */
static void glyph_tris(char ch, void (*tri)(void *u, const float *t), void *u)
{
    if (ch < 32 || ch > 126) return;
    for (int k = 0; k < FV_GLYPH[ch - 32].n; k++) tri(u, FV_TRI + 6 * (FV_GLYPH[ch - 32].first + k));
}
static float glyph_adv(char ch) { return ch < 32 || ch > 126 ? 0 : FV_GLYPH[ch - 32].adv; }
/* what may be typed into a name */
static const char NAME_CHARS[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ.:-/!+<>?,abcdefghijklmnopqrstuvwxyz'()_=*%";
