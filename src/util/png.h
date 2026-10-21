/* util/png.h - screenshots */
#pragma once

/* writes the RGBA pixels as an RGB PNG (SDL's encoder; alpha is ignored); 0 on success */
static int save_png(const char *path, const uint8_t *rgba, int w, int h)
{
    SDL_Surface *s = SDL_CreateSurfaceFrom(w, h, SDL_PIXELFORMAT_RGBX32, (void *)rgba, w * 4);
    bool ok = s && SDL_SavePNG(s, path);
    SDL_DestroySurface(s);
    return ok ? 0 : -1;
}
