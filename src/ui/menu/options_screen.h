/* ui/menu/options_screen.h - the options screen */
#pragma once

/* Options: a setting per line, its value in a cell on the right */
enum { OP_ENG, OP_FX, OP_FULL, OP_RUMBLE, OP_KEYS, OP_BACK, NOP };

static const char *const OP_NAME[NOP] = {"Engine volume",  "Effects volume", "Fullscreen",
                                         "Gamepad rumble", "Driving keys",   "Back"};

static void draw_options(void)
{
    hud_text_c(80, "OPTIONS", 1.5f, 0xF0C040);
    float W = panel_w() - 30, x = (HW - W) / 2, cw = 96, h = BAR_H;
    int sel = msel[MS_OPT];

    for (int r = 0; r < NOP; r++) {
        float y = 96 + r * BAR_GAP;
        draw_bar(x, y, W, OP_NAME[r], r == sel);

        if (r > OP_RUMBLE) {
            continue;
        }

        char b[32], v[16];

        if (r <= OP_FX) {
            snprintf(v, sizeof v, "%d%%", r == OP_ENG ? opt.eng : opt.fx);
        } else {
            snprintf(v, sizeof v, "%s", (r == OP_FULL ? opt.full : opt.rumble) ? "ON" : "OFF");
        }
        snprintf(b, sizeof b, r == sel ? "<  %s  >" : "%s", v);
        float cx = x + W - cw - 4;
        hud_rect(cx - 1, y + 1, cw + 2, h - 2, r == sel ? 0xFFFFFF : 0x0A2426);
        plate(cx, y + 2, cw, h - 4, 0x2A2C30, 0x060608);

        if (r <= OP_FX) {
            float f = (r == OP_ENG ? opt.eng : opt.fx) / 100.f;

            if (f > 0) {
                plate(cx + 1, y + 3, (cw - 2) * f, h - 6, 0x3AA4A8, 0x1A6468);
            }
        } /* the panel's teal */
        const float sc = 0.8f; /* a little smaller than the labels: it sits inside the slider */
        hud_text_raw(cx + (cw - text_width(b, sc)) / 2, y + (h - 7 * sc) / 2, b, sc, r == sel ? 0xFFE060 : 0xE8ECEC);
    }
    menu_help("LEFT/RIGHT CHANGE   ENTER CHOOSE   ESC BACK");
}

static void set_fullscreen(int on)
{
    opt.full = on;

    if (win) {
        SDL_SetWindowFullscreen(win, on);
    }

    opt_save();
}
