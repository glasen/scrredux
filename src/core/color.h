/* core/color.h - colour blending on 0xRRGGBB values */
#pragma once

static uint32_t mixcol(uint32_t a, uint32_t b, float t)
{
    int r = (int)(((a >> 16) & 255) * (1 - t) + ((b >> 16) & 255) * t);
    int g = (int)(((a >> 8) & 255) * (1 - t) + ((b >> 8) & 255) * t);
    int bl = (int)((a & 255) * (1 - t) + (b & 255) * t);
    return (uint32_t)(r << 16 | g << 8 | bl);
}
static uint32_t scalecol(uint32_t c, float s)
{
    int r = (int)clampf(((c >> 16) & 255) * s, 0, 255), g = (int)clampf(((c >> 8) & 255) * s, 0, 255),
        b = (int)clampf((c & 255) * s, 0, 255);
    return (uint32_t)(r << 16 | g << 8 | b);
}
