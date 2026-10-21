/* ui/hud.h - 2D HUD primitives (virtual 480 x 270 screen) */
#pragma once

/* ------------------------------------------------------------- HUD (2D) */
static int mdl_nv;
static float HW = 480;      /* HUD virtual width (height is VH) */
static uint8_t hud_a = 255; /* opacity of what the HUD draws next */
static void hud_rect(float x, float y, float w, float h, uint32_t col)
{
    VB *save = cur;
    cur = &hb;
    vb_reserve(&hb, 6);
    float x0 = x / (HW * 0.5f) - 1, x1 = (x + w) / (HW * 0.5f) - 1;
    float y0 = 1 - y / (VH * 0.5f), y1 = 1 - (y + h) / (VH * 0.5f);
    emit(V(x0, y0, 0), col, hud_a);
    emit(V(x1, y0, 0), col, hud_a);
    emit(V(x1, y1, 0), col, hud_a);
    emit(V(x0, y0, 0), col, hud_a);
    emit(V(x1, y1, 0), col, hud_a);
    emit(V(x0, y1, 0), col, hud_a);
    cur = save;
}
/* a triangle with a colour at each corner */
static void hud_tri3(float x0, float y0, uint32_t c0, float x1, float y1, uint32_t c1, float x2, float y2, uint32_t c2)
{
    VB *save = cur;
    cur = &hb;
    vb_reserve(&hb, 3);
    float sx = 2.f / HW, sy = 2.f / VH;
    emit(V(x0 * sx - 1, 1 - y0 * sy, 0), c0, hud_a);
    emit(V(x1 * sx - 1, 1 - y1 * sy, 0), c1, hud_a);
    emit(V(x2 * sx - 1, 1 - y2 * sy, 0), c2, hud_a);
    cur = save;
}
/* a rectangle shading from ctop to cbot */
static void hud_vgrad(float x, float y, float w, float h, uint32_t ctop, uint32_t cbot)
{
    VB *save = cur;
    cur = &hb;
    vb_reserve(&hb, 6);
    float x0 = x / (HW * 0.5f) - 1, x1 = (x + w) / (HW * 0.5f) - 1;
    float y0 = 1 - y / (VH * 0.5f), y1 = 1 - (y + h) / (VH * 0.5f);
    emit(V(x0, y0, 0), ctop, hud_a);
    emit(V(x1, y0, 0), ctop, hud_a);
    emit(V(x1, y1, 0), cbot, hud_a);
    emit(V(x0, y0, 0), ctop, hud_a);
    emit(V(x1, y1, 0), cbot, hud_a);
    emit(V(x0, y1, 0), cbot, hud_a);
    cur = save;
}
