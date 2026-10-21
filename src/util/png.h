/* util/png.h - minimal PNG writer for screenshots */
#pragma once

/* ------------------------------------------------------------ PNG writer */
static uint32_t crc_tab[256];

static uint32_t crc32u(uint32_t crc, const uint8_t *b, size_t n)
{
    if (!crc_tab[1]) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) {
                c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            }
            crc_tab[i] = c;
        }
    }
    crc = ~crc;
    while (n--) {
        crc = crc_tab[(crc ^ *b++) & 255] ^ (crc >> 8);
    }
    return ~crc;
}

static void png_chunk(FILE *f, const char *type, const uint8_t *d, uint32_t len)
{
    uint8_t h[8] = {len >> 24, len >> 16, len >> 8, len, type[0], type[1], type[2], type[3]};
    fwrite(h, 1, 8, f);
    if (len) {
        fwrite(d, 1, len, f);
    }
    uint32_t c = crc32u(0, h + 4, 4);
    if (len) {
        c = crc32u(c, d, len);
    }
    uint8_t cb[4] = {c >> 24, c >> 16, c >> 8, c};
    fwrite(cb, 1, 4, f);
}

static int save_png(const char *path, const uint8_t *rgba, int w, int h)
{
    FILE *f = fopen(path, "wb");
    if (!f) {
        return -1;
    }
    size_t rawn = (size_t)h * (1 + w * 3);
    uint8_t *raw = malloc(rawn), *z = malloc(rawn + rawn / 60000 * 5 + 64);
    for (int y = 0; y < h; y++) {
        raw[y * (1 + w * 3)] = 0;
        for (int x = 0; x < w; x++) {
            memcpy(raw + y * (1 + w * 3) + 1 + x * 3, rgba + ((size_t)y * w + x) * 4, 3);
        }
    }
    size_t zn = 0;
    z[zn++] = 0x78;
    z[zn++] = 0x01;
    for (size_t o = 0; o < rawn;) {
        size_t bl = rawn - o > 65535 ? 65535 : rawn - o;
        z[zn++] = (o + bl == rawn);
        z[zn++] = bl & 255;
        z[zn++] = bl >> 8;
        z[zn++] = ~bl & 255;
        z[zn++] = (~bl >> 8) & 255;
        memcpy(z + zn, raw + o, bl);
        zn += bl;
        o += bl;
    }
    uint32_t a = 1, bb = 0;
    for (size_t i = 0; i < rawn; i++) {
        a = (a + raw[i]) % 65521;
        bb = (bb + a) % 65521;
    }
    z[zn++] = bb >> 8;
    z[zn++] = bb;
    z[zn++] = a >> 8;
    z[zn++] = a;
    static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    fwrite(sig, 1, 8, f);
    uint8_t ihdr[13] = {w >> 24, w >> 16, w >> 8, w, h >> 24, h >> 16, h >> 8, h, 8, 2, 0, 0, 0};
    png_chunk(f, "IHDR", ihdr, 13);
    png_chunk(f, "IDAT", z, (uint32_t)zn);
    png_chunk(f, "IEND", NULL, 0);
    fclose(f);
    free(raw);
    free(z);
    return 0;
}
