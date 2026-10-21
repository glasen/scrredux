/* ui/menu/widgets.h - panels, logo, bars and the yes/no box */
#pragma once

/* a soft shadow under a box: translucent layers, each a little larger */
static void soft_shadow(float x, float y, float w, float h, float r)
{
    hud_a = 22;
    for (int i = 1; i <= 6; i++) {
        float e = r * i / 6.f;
        hud_rect(x - e + r * 0.4f, y - e + r * 0.6f, w + 2 * e, h + 2 * e, 0x000000);
    }
    hud_a = 255;
}

/* a raised plate: vertical gradient, light top edge, dark bottom edge */
static void plate(float x, float y, float w, float h, uint32_t ctop, uint32_t cbot)
{
    hud_vgrad(x, y, w, h, ctop, cbot);
    hud_a = 70;
    hud_rect(x, y, w, 1, 0xFFFFFF);
    hud_rect(x, y + h - 1, w, 1, 0x000000);
    hud_a = 255;
}

/* the preview's chequered border, q wide, with the thin white line inside */
static void chq_border(float x0, float y0, float x1, float y1, float q)
{
    int n = (int)roundf((x1 - x0) / q), m = (int)roundf((y1 - y0) / q);
    float w = (x1 - x0) / n, h = (y1 - y0) / m;
    for (int j = 0; j < m; j++) {
        hud_rect(x0, y0 + j * h, q, h, j & 1 ? 0xD8D8D8 : 0x0C0C0C);
        hud_rect(x1 - q, y0 + j * h, q, h, (n - 1 + j) & 1 ? 0xD8D8D8 : 0x0C0C0C);
    }
    for (int i = 0; i < n; i++) {
        hud_rect(x0 + i * w, y0, w, q, i & 1 ? 0xD8D8D8 : 0x0C0C0C);
        hud_rect(x0 + i * w, y1 - q, w, q, (i + m - 1) & 1 ? 0xD8D8D8 : 0x0C0C0C);
    }
    hud_rect(x0 + q, y0 + q, x1 - x0 - 2 * q, 1, 0xF4F4F4);
    hud_rect(x0 + q, y1 - q - 1, x1 - x0 - 2 * q, 1, 0xF4F4F4);
    hud_rect(x0 + q, y0 + q, 1, y1 - y0 - 2 * q, 0xF4F4F4);
    hud_rect(x1 - q - 1, y0 + q, 1, y1 - y0 - 2 * q, 0xF4F4F4);
}

/* text with a soft shadow */
static void soft_text(float x, float y, const char *s, float sc, uint32_t ctop, uint32_t cbot, float shear)
{
    hud_a = 60;
    hud_text_ex(x + sc * 0.9f, y + sc * 0.9f, s, sc, 0, 0, shear);
    hud_a = 110;
    hud_text_ex(x + sc * 0.45f, y + sc * 0.45f, s, sc, 0, 0, shear);
    hud_a = 255;
    hud_text_ex(x, y, s, sc, ctop, cbot, shear);
}

/* the title logo: italic lettering, SCR in red and REDUX in white */
static void draw_logo(float y)
{
    const char *s = "SCR REDUX";
    float sc = fminf(5.f, (HW - 60) / (9 * 6.f)), x = (HW - text_width(s, sc)) / 2;
    soft_text(x, y, "SCR", sc, 0xFF7A5A, 0xC01C10, 0.3f);
    soft_text(x + text_width("SCR ", sc), y, "REDUX", sc, 0xFFFFFF, 0xB8C4C8, 0.3f);
}

/* the frame every menu screen shares: a teal panel in a chequered border; returns the panel's left edge */
static float menu_frame(void)
{
    float W = panel_w(), x0 = (HW - W) / 2;
    draw_logo(8);
    char b[32];
    snprintf(b, sizeof b, "DIVISION %s", ROMAN[sea.on ? player_div() : 3]);
    float tw = text_width(b, 1.5f) + 20, tx = (HW - tw) / 2;
    soft_shadow(tx, 50, tw, 16, 3);
    hud_rect(tx - 1.5f, 48.5f, tw + 3, 19, 0xB4B4B4);
    plate(tx, 50, tw, 16, 0x2A2C30, 0x060608);
    hud_text_c(50 + (16 - 7 * 1.5f) / 2, b, 1.5f, 0xF0A040);
    soft_shadow(x0 - 8, 71, W + 16, VH - 73, 6);
    chq_border(x0 - 8, 71, x0 + W + 8, VH - 2, 4);
    plate(x0 - 3, 76, W + 6, VH - 83, 0x24767A, 0x123E42);
    return x0;
}

/* a menu line: dark plate, the chosen one gold with a chequered flag at its start */
static void draw_bar(float x, float y, float w, const char *s, int on)
{
    const float h = BAR_H, sc = BAR_SC;
    char b[96];
    float tx = x + 9 + h * 0.5f + 2, room = x + w - 6 - tx;
    snprintf(b, sizeof b, "%s", s);
    if (text_width(b, sc) > room) { /* too long: cut short, never smaller */
        for (size_t n = strlen(b); n > 0 && text_width(b, sc) > room;) {
            b[--n] = 0;
            if (n >= 3) {
                memcpy(b + n - 3, "...", 3);
            }
        }
    }
    soft_shadow(x, y, w, h, 2);
    hud_rect(x - 1, y - 1, w + 2, h + 2, on ? 0xFFE0A0 : 0x0A2426);
    if (on) {
        plate(x, y, w, h, 0xFFC860, 0xE07818);
    } else {
        plate(x, y, w, h, 0x1C2224, 0x0A0C0E);
    }
    float fx = x + 4, q = h / 4.f;
    if (on) {
        for (int j = 0; j < 2; j++) {
            for (int i = 0; i < 2; i++) {
                hud_rect(fx + i * q, y + h / 2 - q + j * q, q, q, (i + j) & 1 ? 0xF4F4F4 : 0x101010);
            }
        }
    }
    hud_text_raw(tx, y + (h - 7 * sc) / 2, b, sc, on ? 0x201008 : 0xE8ECEC);
}

static void draw_bars(float y, const char *const *it, int n, int sel)
{
    float W = panel_w() - 30, x = (HW - W) / 2;
    for (int i = 0; i < n; i++) {
        draw_bar(x, y + i * BAR_GAP, W, it[i], i == sel);
    }
}

/* the help line at the bottom; a note (Season saved, ...) takes its place while it shows */
static void menu_help(const char *s)
{
    if (ms_note_t <= 0) {
        hud_text_c(VH - 18, s, fminf(1.f, (panel_w() - 24) / text_width(s, 1)), 0xA8CCCC);
    }
}

/* "Really exit?": Y yes, N or ESC no */
static void ask_box(const char *sub)
{
    float w = 244, h = sub ? 54 : 40, x = (HW - w) / 2, y = 92;
    soft_shadow(x, y, w, h, 8);
    chq_border(x, y, x + w, y + h, 4);
    plate(x + 5, y + 5, w - 10, h - 10, 0x2A2C30, 0x060608);
    hud_text_c(y + 13, "REALLY EXIT?  Y/N", 2, 0xFFFFFF);
    if (sub) {
        hud_text_c(y + 35, sub, 1, 0xFF8070);
    }
}

static int ask_answer(SDL_Keycode k)
{
    return k == SDLK_Y ? 1 : k == SDLK_N || k == SDLK_ESCAPE ? 0 : -1;
}
