/* sr_main.h - HUD, PNG writer, SDL_GPU layer and main(). Included by stuntracer.c */

#define PHYS_DT (1.0f / 240.0f)

/* ------------------------------------------------------------- HUD (2D) */
static int mdl_nv;
static float HW = 480;           /* HUD virtual width (height is VH) */
static uint8_t hud_a = 255;      /* opacity of what the HUD draws next */
static void hud_rect(float x, float y, float w, float h, uint32_t col)
{
    VB *save = cur; cur = &hb; vb_reserve(&hb, 6);
    float x0 = x / (HW * 0.5f) - 1, x1 = (x + w) / (HW * 0.5f) - 1;
    float y0 = 1 - y / (VH * 0.5f), y1 = 1 - (y + h) / (VH * 0.5f);
    emit(V(x0, y0, 0), col, hud_a); emit(V(x1, y0, 0), col, hud_a); emit(V(x1, y1, 0), col, hud_a);
    emit(V(x0, y0, 0), col, hud_a); emit(V(x1, y1, 0), col, hud_a); emit(V(x0, y1, 0), col, hud_a);
    cur = save;
}
/* a triangle with a colour at each corner */
static void hud_tri3(float x0, float y0, uint32_t c0, float x1, float y1, uint32_t c1, float x2, float y2, uint32_t c2)
{
    VB *save = cur; cur = &hb; vb_reserve(&hb, 3);
    float sx = 2.f / HW, sy = 2.f / VH;
    emit(V(x0 * sx - 1, 1 - y0 * sy, 0), c0, hud_a); emit(V(x1 * sx - 1, 1 - y1 * sy, 0), c1, hud_a); emit(V(x2 * sx - 1, 1 - y2 * sy, 0), c2, hud_a);
    cur = save;
}
/* a rectangle shading from ctop to cbot */
static void hud_vgrad(float x, float y, float w, float h, uint32_t ctop, uint32_t cbot)
{
    VB *save = cur; cur = &hb; vb_reserve(&hb, 6);
    float x0 = x / (HW * 0.5f) - 1, x1 = (x + w) / (HW * 0.5f) - 1;
    float y0 = 1 - y / (VH * 0.5f), y1 = 1 - (y + h) / (VH * 0.5f);
    emit(V(x0, y0, 0), ctop, hud_a); emit(V(x1, y0, 0), ctop, hud_a); emit(V(x1, y1, 0), cbot, hud_a);
    emit(V(x0, y0, 0), ctop, hud_a); emit(V(x1, y1, 0), cbot, hud_a); emit(V(x0, y1, 0), cbot, hud_a);
    cur = save;
}

static void hud_tri(float x0, float y0, float x1, float y1, float x2, float y2, uint32_t col)
{
    VB *save = cur; cur = &hb; vb_reserve(&hb, 3);
    float sx = 2.f / HW, sy = 2.f / VH;
    emit(V(x0 * sx - 1, 1 - y0 * sy, 0), col, hud_a); emit(V(x1 * sx - 1, 1 - y1 * sy, 0), col, hud_a); emit(V(x2 * sx - 1, 1 - y2 * sy, 0), col, hud_a);
    cur = save;
}
static void hud_quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, uint32_t col)
{
    hud_tri(x0, y0, x1, y1, x2, y2, col); hud_tri(x0, y0, x2, y2, x3, y3, col);
}
static void hud_line(float x0, float y0, float x1, float y1, float w, uint32_t col)
{
    float dx = x1 - x0, dy = y1 - y0, l = sqrtf(dx * dx + dy * dy);
    if (l < 1e-4f) return;
    float nx = -dy / l * w * 0.5f, ny = dx / l * w * 0.5f;
    hud_quad(x0 + nx, y0 + ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny, x0 - nx, y0 - ny, col);
}

/* text: sc is the old 5x7 font's pixel size, so capitals are 7 * sc tall with their top at y */
static float text_width(const char *s, float sc) { float w = 0; for (; *s; s++) w += glyph_adv(*s); return w * 7 * sc; }
typedef struct { float x, base, cap, shear; uint32_t ctop, cbot; } HudGlyph;
static void hud_glyph_tri(void *u, const float *t)
{
    const HudGlyph *h = u;
#define GV(k) h->x + (t[2 * k] + t[2 * k + 1] * h->shear) * h->cap, h->base - t[2 * k + 1] * h->cap, \
              (h->ctop == h->cbot ? h->ctop : mixcol(h->ctop, h->cbot, clampf(1 - t[2 * k + 1], 0, 1)))
    hud_tri3(GV(0), GV(1), GV(2));
#undef GV
}
/* rows shade from col_top to col_bot; shear > 0 leans the letters forward */
static void hud_text_ex(float x, float y, const char *s, float sc, uint32_t ctop, uint32_t cbot, float shear)
{
    HudGlyph h = {x, y + 7 * sc, 7 * sc, shear, ctop, cbot};
    for (; *s; s++) { glyph_tris(*s, hud_glyph_tri, &h); h.x += glyph_adv(*s) * h.cap; }
}
static void hud_text_raw(float x, float y, const char *s, float sc, uint32_t col) { hud_text_ex(x, y, s, sc, col, col, 0); }
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
    int m = (int)(t / 60); float s = t - m * 60;
    snprintf(b, n, "%d:%04.1f", m, s);
}
static SDL_Window *win;
#include "menu.h"
#include "editor.h"

/* ------------------------------------------------------- vehicle physics constants */
static void phys_defaults(void)
{
    ph.mass = 1000.f;
    ph.grav = 13.f;
    ph.power = 337000.f;
    ph.fmax = 9900.f;
    ph.boostp = 758000.f;
    ph.boostf = 9750.f;
    ph.boostrate = 2.f;              /* boost tank (100) drains this much per second */
    ph.brkf = 4500.f;
    ph.brkr = 3200.f;
    ph.drag = 0.88f;
    ph.mu = 2.35f;
    ph.rgrip = 1.05f;
    ph.steerlat = 45.f;
    ph.keyrise = 5.f;
    ph.keyret = 10.f;
    ph.corner = 12.f;
    ph.lowspd = 2.0f;
    ph.loadmax = 6500.f;
    ph.steer = 0.5f;
    ph.steerfall = 30.f;
    ph.steerrate = 3.f;
    ph.kf = 26000.f;
    ph.kr = 24000.f;
    ph.ccomp = 2600.f;
    ph.creb = 3600.f;
    ph.kbump = 250000.f;
    ph.sroll = 1100.f;
    ph.syaw = 2000.f;
    ph.spitch = 1000.f;
    ph.air = 7000.f;
    ph.leanr = 0.006f;
    ph.leanp = 0.004f;
    ph.shw = 1.5f;                   /* invisible shoulder past the deck edge: wheels still find ground up to shw out, */
    ph.shdrop = 0.25f;               /* shdrop lower at the edge and falling 1:2 outwards, like the original */
    ph.vtop = 200.f / 3.6f;          /* engine cut-off: 200 km/h, */
    ph.vtopb = 250.f / 3.6f;         /* 250 km/h on boost (the boost adds pull, the drag alone would allow 107 m/s) */
}

static void draw_hud_base(void)
{
    char b[96];
    if (state == ST_VIEW) {
        hud_text_c(8, "MODEL VIEWER", 3, 0xFFD030);
        snprintf(b, sizeof b, "%d TRIANGLES PER CAR", mdl_nv / 3);
        hud_text_c(36, b, 1, 0xE0E0E0);
        hud_text_c(226, "DRAG/ARROWS: ROTATE   WHEEL/+-: ZOOM   HOME: RESET   SPACE: TURNTABLE", 1, 0xE0E0E0);
        snprintf(b, sizeof b, "N: PAINT  Z: ANIMATION %s  F: FLAMES %s  R: CAGE %s  E: INTAKE %s  ESC: QUIT",
                 view_anim ? "ON" : "OFF", view_flame ? "ON" : "OFF", (view_hide & 1) ? "OFF" : "ON", (view_hide & 4) ? "OFF" : "ON");
        hud_text_c(238, b, 1, 0xE0E0E0);
        return;
    }
    if (state == ST_MENU) { draw_menu(); return; }
    if (state == ST_PREVIEW) { draw_preview(); return; }
    if (state == ST_EDIT) { ed_draw(); return; }
    if (state == ST_REPLAY) { draw_replay_hud(); return; }
    const Car *p = &cars[0];                     /* speed, boost and damage are in the cockpit (draw_cockpit) */
    int lp = p->lap < 1 ? 1 : (laps > 0 && p->lap > laps ? laps : p->lap);
    if (laps > 0) snprintf(b, sizeof b, "LAP %d/%d", lp, laps); else snprintf(b, sizeof b, "LAP %d", lp);
    hud_text(8, 33, b, 1.5f, 0xFFFFFF);          /* below the cage's top tube, which carries the damage crack */
    fmt_time(b, sizeof b, racetime);
    hud_text(8, 47, b, 1.5f, 0xFFE060);
    char bl[32];
    if (p->bestlap > 0) fmt_time(bl, sizeof bl, p->bestlap); else snprintf(bl, sizeof bl, "-:--.-");
    snprintf(b, sizeof b, "BEST %s", bl);
    hud_text(HW - 8 - text_width(b, 1.5f), 33, b, 1.5f, 0xFFFFFF);
    if (p->lapshow > 0 && state == ST_RACE) {
        char b2[96]; fmt_time(b, sizeof b, p->lastlap);
        snprintf(b2, sizeof b2, "LAP TIME %s", b);
        hud_text_c(44, b2, 2, 0xFFFFFF);
    }
    if (state == ST_COUNT) { snprintf(b, sizeof b, "%d", (int)ceilf(state_t)); hud_text_c(100, b, 8, 0xFFE040); }
    if (msg_t > 0) hud_text_c(80, msg, 3, 0xFFFFFF);
    if (state == ST_DONE) {
        const char *t = result == 1 ? "YOU WIN!" : result == 2 ? "OPPONENT WRECKED - YOU WIN!" :
                        result == -2 ? "YOU ARE WRECKED!" : "YOU LOSE!";
        hud_text_c(80, t, result == 2 || result == -2 ? 2 : 4, result > 0 ? 0x60FF60 : 0xFF6050);
        if (p->bestlap > 0) { char b2[96]; fmt_time(b, sizeof b, p->bestlap); snprintf(b2, sizeof b2, "BEST LAP %s", b); hud_text_c(130, b2, 2, 0xFFFFFF); }
        if (state_t > 1.5f) hud_text_c(160, "PRESS ENTER", 2, 0xFFE040);
    }
    if (paused && !ask_quit) hud_text_c(110, "PAUSED", 4, 0xFFFFFF);
    race_hud_extra();
}
static void draw_hud(void) { draw_hud_base(); }

/* ------------------------------------------------------------ PNG writer */
static uint32_t crc_tab[256];
static uint32_t crc32u(uint32_t crc, const uint8_t *b, size_t n)
{
    if (!crc_tab[1]) for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1; crc_tab[i] = c; }
    crc = ~crc;
    while (n--) crc = crc_tab[(crc ^ *b++) & 255] ^ (crc >> 8);
    return ~crc;
}
static void png_chunk(FILE *f, const char *type, const uint8_t *d, uint32_t len)
{
    uint8_t h[8] = {len >> 24, len >> 16, len >> 8, len, type[0], type[1], type[2], type[3]};
    fwrite(h, 1, 8, f); if (len) fwrite(d, 1, len, f);
    uint32_t c = crc32u(0, h + 4, 4); if (len) c = crc32u(c, d, len);
    uint8_t cb[4] = {c >> 24, c >> 16, c >> 8, c}; fwrite(cb, 1, 4, f);
}
static int save_png(const char *path, const uint8_t *rgba, int w, int h)
{
    FILE *f = fopen(path, "wb"); if (!f) return -1;
    size_t rawn = (size_t)h * (1 + w * 3);
    uint8_t *raw = malloc(rawn), *z = malloc(rawn + rawn / 60000 * 5 + 64);
    for (int y = 0; y < h; y++) { raw[y * (1 + w * 3)] = 0; for (int x = 0; x < w; x++) memcpy(raw + y * (1 + w * 3) + 1 + x * 3, rgba + ((size_t)y * w + x) * 4, 3); }
    size_t zn = 0; z[zn++] = 0x78; z[zn++] = 0x01;
    for (size_t o = 0; o < rawn; ) {
        size_t bl = rawn - o > 65535 ? 65535 : rawn - o;
        z[zn++] = (o + bl == rawn); z[zn++] = bl & 255; z[zn++] = bl >> 8; z[zn++] = ~bl & 255; z[zn++] = (~bl >> 8) & 255;
        memcpy(z + zn, raw + o, bl); zn += bl; o += bl;
    }
    uint32_t a = 1, bb = 0;
    for (size_t i = 0; i < rawn; i++) { a = (a + raw[i]) % 65521; bb = (bb + a) % 65521; }
    z[zn++] = bb >> 8; z[zn++] = bb; z[zn++] = a >> 8; z[zn++] = a;
    static const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    fwrite(sig, 1, 8, f);
    uint8_t ihdr[13] = {w >> 24, w >> 16, w >> 8, w, h >> 24, h >> 16, h >> 8, h, 8, 2, 0, 0, 0};
    png_chunk(f, "IHDR", ihdr, 13); png_chunk(f, "IDAT", z, (uint32_t)zn); png_chunk(f, "IEND", NULL, 0);
    fclose(f); free(raw); free(z);
    return 0;
}

/* ------------------------------------------------------------- GPU layer */
static SDL_GPUDevice *dev;
static SDL_GPUGraphicsPipeline *pipe3, *pipe2;
static SDL_GPUBuffer *vbuf_s, *vbuf_d;
static SDL_GPUTransferBuffer *tbuf, *dlbuf;
static SDL_GPUTexture *rt_ms, *rt_res, *depthtex;
static SDL_GPUTexture *sh_tex[2];
static SDL_GPUSampler *sh_samp;
static SDL_GPUGraphicsPipeline *pipe_sh;
static const int SH_SIZE[2] = {3072, 4096};
static int sky_n;
static int rtw, rth, msaa = 1, static_gen = -1, static_n;
static size_t dlsize;
static SDL_GPUTextureFormat dfmt;
#define DYN_CAP 400000
#define HUD_CAP 400000

/* ---- the car model, loaded from models/stuntcar.scm (car_model.h) ---- */
typedef struct { char name[8]; int role, corner, link; uint32_t first, count; } MGroup;
static MGroup mdl_g[64]; static int mdl_ng, mdl_nv;
static Vtx *mdl_v;
static SDL_GPUBuffer *vbuf_m;

#include "car_model.h"

static SDL_GPUShader *load_shader(const char *file, SDL_GPUShaderStage stage, int nunif, int nsamp)
{
    char path[1024]; size_t sz;
    snprintf(path, sizeof path, "%s%s", SDL_GetBasePath(), file);
    void *code = SDL_LoadFile(path, &sz);
    if (!code) { snprintf(path, sizeof path, "%s", file); code = SDL_LoadFile(path, &sz); }
    if (!code) { fprintf(stderr, "cannot load shader %s (run make)\n", file); return NULL; }
    SDL_GPUShaderCreateInfo ci; SDL_zero(ci);
    ci.code_size = sz; ci.code = code; ci.entrypoint = "main";
    ci.format = SDL_GPU_SHADERFORMAT_SPIRV; ci.stage = stage; ci.num_uniform_buffers = nunif; ci.num_samplers = nsamp;
    SDL_GPUShader *s = SDL_CreateGPUShader(dev, &ci);
    SDL_free(code);
    if (!s) fprintf(stderr, "shader %s: %s\n", file, SDL_GetError());
    return s;
}

static int ensure_targets(int w, int h)
{
    if (w == rtw && h == rth && rt_res) return 0;
    if (rt_ms) SDL_ReleaseGPUTexture(dev, rt_ms);
    if (rt_res) SDL_ReleaseGPUTexture(dev, rt_res);
    if (depthtex) SDL_ReleaseGPUTexture(dev, depthtex);
    rt_ms = rt_res = depthtex = NULL;
    SDL_GPUTextureCreateInfo ti; SDL_zero(ti);
    ti.type = SDL_GPU_TEXTURETYPE_2D; ti.width = w; ti.height = h; ti.layer_count_or_depth = 1; ti.num_levels = 1;
    ti.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ti.sample_count = SDL_GPU_SAMPLECOUNT_1;
    ti.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    rt_res = SDL_CreateGPUTexture(dev, &ti);
    ti.sample_count = msaa > 1 ? SDL_GPU_SAMPLECOUNT_4 : SDL_GPU_SAMPLECOUNT_1;
    if (msaa > 1) { ti.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET; rt_ms = SDL_CreateGPUTexture(dev, &ti); }
    ti.format = dfmt; ti.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    depthtex = SDL_CreateGPUTexture(dev, &ti);
    if (!rt_res || !depthtex || (msaa > 1 && !rt_ms)) { fprintf(stderr, "targets: %s\n", SDL_GetError()); return -1; }
    rtw = w; rth = h;
    if ((size_t)w * h * 4 != dlsize) {
        if (dlbuf) SDL_ReleaseGPUTransferBuffer(dev, dlbuf);
        SDL_GPUTransferBufferCreateInfo tb; SDL_zero(tb);
        tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD; tb.size = (Uint32)((size_t)w * h * 4);
        dlbuf = SDL_CreateGPUTransferBuffer(dev, &tb); dlsize = (size_t)w * h * 4;
    }
    return 0;
}

static int gpu_init(void)
{
    dev = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, NULL);
    if (!dev) { fprintf(stderr, "GPU device: %s\n", SDL_GetError()); return -1; }
    if (win) {
        if (!SDL_ClaimWindowForGPUDevice(dev, win)) { fprintf(stderr, "claim: %s\n", SDL_GetError()); return -1; }
        SDL_SetGPUSwapchainParameters(dev, win, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);
    }
    SDL_GPUShader *vs = load_shader("game.vert.spv", SDL_GPU_SHADERSTAGE_VERTEX, 2, 0);
    SDL_GPUShader *fs = load_shader("game.frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 2);
    SDL_GPUShader *hfs = load_shader("hud.frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0);
    if (!vs || !fs || !hfs) return -1;
    dfmt = SDL_GPUTextureSupportsFormat(dev, SDL_GPU_TEXTUREFORMAT_D32_FLOAT, SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)
           ? SDL_GPU_TEXTUREFORMAT_D32_FLOAT : SDL_GPU_TEXTUREFORMAT_D24_UNORM;
    if (SDL_GPUTextureSupportsSampleCount(dev, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_SAMPLECOUNT_4) &&
        SDL_GPUTextureSupportsSampleCount(dev, dfmt, SDL_GPU_SAMPLECOUNT_4)) msaa = 4;
    SDL_GPUVertexBufferDescription vbd; SDL_zero(vbd);
    vbd.slot = 0; vbd.pitch = sizeof(Vtx); vbd.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    SDL_GPUVertexAttribute at[2]; SDL_zero(at);
    at[0].location = 0; at[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3; at[0].offset = 0;
    at[1].location = 1; at[1].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM; at[1].offset = 12;
    SDL_GPUColorTargetDescription ctd; SDL_zero(ctd);
    ctd.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    for (int pass = 0; pass < 2; pass++) {
        SDL_GPUGraphicsPipelineCreateInfo pi; SDL_zero(pi);
        pi.vertex_shader = vs; pi.fragment_shader = pass == 0 ? fs : hfs;
        pi.vertex_input_state.vertex_buffer_descriptions = &vbd; pi.vertex_input_state.num_vertex_buffers = 1;
        pi.vertex_input_state.vertex_attributes = at; pi.vertex_input_state.num_vertex_attributes = 2;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL; pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pi.multisample_state.sample_count = msaa > 1 ? SDL_GPU_SAMPLECOUNT_4 : SDL_GPU_SAMPLECOUNT_1;
        SDL_GPUColorTargetDescription hctd = ctd;                                 /* the HUD blends by vertex alpha */
        hctd.blend_state.enable_blend = true;
        hctd.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA; hctd.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        hctd.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        hctd.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO; hctd.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        hctd.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_GREATER_OR_EQUAL;   /* reversed depth: near = 1, far = 0 */
        pi.depth_stencil_state.enable_depth_test = (pass == 0);
        pi.depth_stencil_state.enable_depth_write = (pass == 0);
        pi.target_info.color_target_descriptions = pass == 0 ? &ctd : &hctd; pi.target_info.num_color_targets = 1;
        pi.target_info.depth_stencil_format = dfmt; pi.target_info.has_depth_stencil_target = true;
        SDL_GPUGraphicsPipeline *pl = SDL_CreateGPUGraphicsPipeline(dev, &pi);
        if (!pl) { fprintf(stderr, "pipeline: %s\n", SDL_GetError()); return -1; }
        if (pass == 0) pipe3 = pl; else pipe2 = pl;
    }
    /* shadow mapping: two cascades (D16), depth-only pipeline, comparison sampler */
    {
        SDL_GPUShader *sfs = load_shader("shadow.frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0);
        if (!sfs) return -1;
        SDL_GPUGraphicsPipelineCreateInfo pi; SDL_zero(pi);
        pi.vertex_shader = vs; pi.fragment_shader = sfs;
        pi.vertex_input_state.vertex_buffer_descriptions = &vbd; pi.vertex_input_state.num_vertex_buffers = 1;
        pi.vertex_input_state.vertex_attributes = at; pi.vertex_input_state.num_vertex_attributes = 2;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL; pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.enable_depth_bias = true;
        pi.rasterizer_state.depth_bias_constant_factor = 2.0f; pi.rasterizer_state.depth_bias_slope_factor = 2.0f;
        pi.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test = true; pi.depth_stencil_state.enable_depth_write = true;
        pi.target_info.num_color_targets = 0;
        pi.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM; pi.target_info.has_depth_stencil_target = true;
        pipe_sh = SDL_CreateGPUGraphicsPipeline(dev, &pi);
        if (!pipe_sh) { fprintf(stderr, "shadow pipeline: %s\n", SDL_GetError()); return -1; }
        for (int i = 0; i < 2; i++) {
            SDL_GPUTextureCreateInfo ti; SDL_zero(ti);
            ti.type = SDL_GPU_TEXTURETYPE_2D; ti.format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
            ti.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
            ti.width = ti.height = SH_SIZE[i]; ti.layer_count_or_depth = 1; ti.num_levels = 1; ti.sample_count = SDL_GPU_SAMPLECOUNT_1;
            sh_tex[i] = SDL_CreateGPUTexture(dev, &ti);
            if (!sh_tex[i]) { fprintf(stderr, "shadow texture: %s\n", SDL_GetError()); return -1; }
        }
        SDL_GPUSamplerCreateInfo sc; SDL_zero(sc);
        sc.min_filter = SDL_GPU_FILTER_LINEAR; sc.mag_filter = SDL_GPU_FILTER_LINEAR; sc.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sc.address_mode_u = sc.address_mode_v = sc.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sc.enable_compare = true; sc.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        sh_samp = SDL_CreateGPUSampler(dev, &sc);
        if (!sh_samp) { fprintf(stderr, "shadow sampler: %s\n", SDL_GetError()); return -1; }
    }
    SDL_GPUBufferCreateInfo bi; SDL_zero(bi);
    bi.usage = SDL_GPU_BUFFERUSAGE_VERTEX; bi.size = (DYN_CAP + HUD_CAP) * sizeof(Vtx);
    vbuf_d = SDL_CreateGPUBuffer(dev, &bi);
    SDL_GPUTransferBufferCreateInfo tb; SDL_zero(tb);
    tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD; tb.size = (DYN_CAP + HUD_CAP) * sizeof(Vtx);
    tbuf = SDL_CreateGPUTransferBuffer(dev, &tb);
    if (!vbuf_d || !tbuf) { fprintf(stderr, "buffers: %s\n", SDL_GetError()); return -1; }
    {
        SDL_GPUBufferCreateInfo mb; SDL_zero(mb);
        mb.usage = SDL_GPU_BUFFERUSAGE_VERTEX; mb.size = (Uint32)((size_t)mdl_nv * sizeof(Vtx));
        vbuf_m = SDL_CreateGPUBuffer(dev, &mb);
        SDL_GPUTransferBufferCreateInfo mt; SDL_zero(mt);
        mt.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD; mt.size = mb.size;
        SDL_GPUTransferBuffer *t = SDL_CreateGPUTransferBuffer(dev, &mt);
        void *m = SDL_MapGPUTransferBuffer(dev, t, false);
        memcpy(m, mdl_v, mb.size);
        SDL_UnmapGPUTransferBuffer(dev, t);
        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(dev);
        SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(cmd);
        SDL_GPUTransferBufferLocation src = {t, 0}; SDL_GPUBufferRegion dst = {vbuf_m, 0, mb.size};
        SDL_UploadToGPUBuffer(cp, &src, &dst, false);
        SDL_EndGPUCopyPass(cp);
        SDL_GPUFence *f = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
        SDL_WaitForGPUFences(dev, true, &f, 1);
        SDL_ReleaseGPUFence(dev, f);
        SDL_ReleaseGPUTransferBuffer(dev, t);
        fprintf(stderr, "car model: %d triangles per car\n", mdl_nv / 3);
    }
    return 0;
}

/* upload the static world mesh (track + stadium) once per track */
static void upload_static(void)
{
    if (static_gen == track_gen) return;
    if (vbuf_s) SDL_ReleaseGPUBuffer(dev, vbuf_s);
    SDL_GPUBufferCreateInfo bi; SDL_zero(bi);
    bi.usage = SDL_GPU_BUFFERUSAGE_VERTEX; bi.size = (Uint32)((size_t)sb.n * sizeof(Vtx));
    vbuf_s = SDL_CreateGPUBuffer(dev, &bi);
    SDL_GPUTransferBufferCreateInfo tb; SDL_zero(tb);
    tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD; tb.size = bi.size;
    SDL_GPUTransferBuffer *t = SDL_CreateGPUTransferBuffer(dev, &tb);
    void *m = SDL_MapGPUTransferBuffer(dev, t, false);
    memcpy(m, sb.v, bi.size);
    SDL_UnmapGPUTransferBuffer(dev, t);
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(dev);
    SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTransferBufferLocation src = {t, 0};
    SDL_GPUBufferRegion dst = {vbuf_s, 0, bi.size};
    SDL_UploadToGPUBuffer(cp, &src, &dst, false);
    SDL_EndGPUCopyPass(cp);
    SDL_GPUFence *f = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    SDL_WaitForGPUFences(dev, true, &f, 1);
    SDL_ReleaseGPUFence(dev, f);
    SDL_ReleaseGPUTransferBuffer(dev, t);
    static_gen = track_gen; static_n = sb.n;
    fprintf(stderr, "static mesh: %d triangles\n", sb.n / 3);
}

static int dyn_shadow_end, frame_no;
static void draw_flames(void);

/* ---------------------------------------------------------------- crane and chains */
static void chain(V3 a, V3 b)
{
    V3 d = vsub(b, a); float L = vlen(d); if (L < 0.05f) return;
    V3 ax = vmul(d, 1 / L), u = vnorm(vcross(ax, fabsf(ax.y) < 0.9f ? UP : V(1, 0, 0))), w = vcross(ax, u);
    int n = (int)(L / 0.11f);
    for (int k = 0; k < n; k++) {                                     /* links alternate by 90 degrees */
        V3 c = vadd(a, vmul(ax, (k + 0.5f) * L / n));
        obox(c, ax, (k & 1) ? u : w, (k & 1) ? w : u, 0.07f, 0.035f, 0.012f, 0x5A5A62);
    }
}
static void lattice(V3 a, V3 b, V3 side, float wdt, uint32_t col)  /* two-chord lattice beam with zig-zag web */
{
    V3 o = vmul(vnorm(side), wdt * 0.5f);
    V3 a0 = vsub(a, o), a1 = vadd(a, o), b0 = vsub(b, o), b1 = vadd(b, o);
    cyl(a0, b0, 0.11f, 0.11f, 6, col, 0); cyl(a1, b1, 0.11f, 0.11f, 6, col, 0);
    int n = (int)(vlen(vsub(b, a)) / 1.6f) + 1;
    for (int k = 0; k < n; k++) {
        float t0 = (float)k / n, t1 = (float)(k + 1) / n;
        cyl(vlerp(a0, b0, t0), vlerp(a1, b1, t1), 0.06f, 0.06f, 5, col, 0);
        cyl(vlerp(a1, b1, t0), vlerp(a0, b0, t0), 0.06f, 0.06f, 5, col, 0);
    }
}
static void draw_crane(const Car *c)
{
    if (!c->active || (c->crane <= 0 && c->hook <= 0)) return;
    const Seg *s = &seg[c->crane_seg];
    V3 Rh = vnorm(V(s->R.x, 0, s->R.z)), Th = vnorm(V(s->T.x, 0, s->T.z));
    V3 base = vadd(c->crane_x, vmul(Rh, TRACK_HW + 11.f)); base.y = ground_h(base.x, base.z);
    float topy = c->crane_x.y + 12.f;
    V3 top = V(base.x, topy, base.z), tip = V(c->crane_x.x, topy + 0.8f, c->crane_x.z);
    { Frm cf = frm_at(V(base.x + Th.x * 1.5f, base.y, base.z + Th.z * 1.5f), Th); crane_carrier(&cf); }   /* carrier */
    for (int k = 0; k < 4; k++) {                                                                   /* tower */
        V3 o = vadd(vmul(Rh, (k & 1) ? 0.8f : -0.8f), vmul(Th, (k & 2) ? 0.8f : -0.8f));
        cyl(vadd(vadd(base, V(0, 1.35f, 0)), o), vadd(top, o), 0.13f, 0.13f, 6, 0xE8C020, 0);
    }
    for (float y = 3.f; y < topy - base.y - 1; y += 2.2f) {
        V3 p0 = V(base.x, base.y + y, base.z);
        cyl(vadd(p0, vmul(Rh, -0.8f)), vadd(vadd(p0, vmul(Rh, 0.8f)), V(0, 1.1f, 0)), 0.06f, 0.06f, 5, 0x2A2A2E, 0);
        cyl(vadd(p0, vmul(Th, -0.8f)), vadd(vadd(p0, vmul(Th, 0.8f)), V(0, 1.1f, 0)), 0.06f, 0.06f, 5, 0x2A2A2E, 0);
    }
    lattice(top, tip, Th, 1.4f, 0xE8C020);                                                          /* jib over the track */
    obox(vsub(top, vmul(Rh, 3.f)), Rh, UP, Th, 1.4f, 1.0f, 1.2f, 0x3A3A40);                          /* counterweight */
    lattice(top, vsub(top, vmul(Rh, 3.f)), Th, 1.4f, 0xE8C020);
    /* four chains hang straight down from a spreader frame: the front pair to the bumper hoop, so the driver sees them
     * in front of the windscreen like in the original, the rear pair to the back of the roll cage */
    static const V3 att[4] = {{-0.5f, 0.17f, 2.08f}, {0.5f, 0.17f, 2.08f}, {-0.6f, 1.107f, -1.438f}, {0.6f, 1.107f, -1.438f}};
    const float sy = 4.2f;
    V3 hook, pts[4], spr[4];
    for (int k = 0; k < 4; k++) { pts[k] = body_pt(c, att[k]); spr[k] = body_pt(c, V(att[k].x, sy, att[k].z)); }
    V3 above = body_pt(c, V(0, sy + 2.4f, 0.32f));
    if (c->crane > 0) hook = above;
    else hook = vlerp(tip, above, smooth01(c->hook / 2.0f));                                         /* released: hook winds up */
    cyl(tip, hook, 0.04f, 0.04f, 4, 0x1A1A1E, 0);
    obox(hook, V(1, 0, 0), UP, V(0, 0, 1), 0.22f, 0.3f, 0.12f, 0xE8C020);
    if (c->crane > 0) {
        for (int k = 0; k < 4; k++) {
            cyl(spr[k], spr[(k == 0) ? 1 : (k == 1) ? 3 : (k == 3) ? 2 : 0], 0.07f, 0.07f, 5, 0xE8C020, 0);   /* spreader frame */
            cyl(hook, spr[k], 0.03f, 0.03f, 4, 0x1A1A1E, 0);                                             /* slings */
            chain(spr[k], pts[k]);
        }
    }
}

/* ---------------------------------------------------------------- particles */
static void draw_parts(void)
{
    for (int i = 0; i < npart; i++) {
        const Part *q = &parts[i];
        float a = q->t / q->life;
        if (q->kind == P_SPARK) {
            V3 d = vmul(q->v, 0.05f); float dl = vlen(d);
            if (dl < 1e-3f) continue;
            if (dl > 0.22f) d = vmul(d, 0.22f / dl);                                   /* short streaks, even when flying past the camera */
            V3 tail = vsub(q->p, d);
            float wdt = fminf(0.03f, 0.006f * vlen(vsub(q->p, cam.pos)));               /* thin close to the eye */
            V3 sd = vmul(vnorm(vcross(d, cam.fwd)), wdt);
            draw_quad_unlit(vsub(tail, sd), vadd(tail, sd), vadd(q->p, sd), vsub(q->p, sd), q->col);
            continue;
        }
        float r = q->size * (q->kind == P_FIRE ? 1.f - a : 1.f - a * a * a);
        uint32_t col = q->kind == P_FIRE ? q->col : mixcol(q->col, FOGCOL, a * a * 0.8f);       /* thin out into the air */
        V3 pr[6];
        for (int k = 0; k < 6; k++) {
            float an = k * PI_F / 3 + i;
            pr[k] = vadd(q->p, vadd(vmul(cam.right, cosf(an) * r), vmul(cam.up, sinf(an) * r)));
        }
        for (int k = 0; k < 6; k++) draw_tri_unlit(q->p, pr[k], pr[(k + 1) % 6], col);
    }
}

/* ---------------------------------------------------------------- cockpit instruments (driver's view only) */
static float ck_m[12];                            /* body rows of the player's car: car frame -> world */
static V3 ckp(V3 l) { return m34_apply(ck_m, l); }
static V3 ckd(V3 l) { return vsub(ckp(l), ckp(V(0, 0, 0))); }
static void ck_ring(V3 C, V3 u, V3 v, float r0, float r1, int n, uint32_t col)
{
    for (int k = 0; k < n; k++) {
        float a0 = 2 * PI_F * k / n, a1 = 2 * PI_F * (k + 1) / n;
        V3 d0 = vadd(vmul(u, cosf(a0)), vmul(v, sinf(a0))), d1 = vadd(vmul(u, cosf(a1)), vmul(v, sinf(a1)));
        draw_quad(ckp(vadd(C, vmul(d0, r0))), ckp(vadd(C, vmul(d0, r1))), ckp(vadd(C, vmul(d1, r1))), ckp(vadd(C, vmul(d1, r0))), col);
    }
}
static void ck_disc(V3 C, V3 u, V3 v, float r, int n, uint32_t col)
{
    for (int k = 0; k < n; k++) {
        float a0 = 2 * PI_F * k / n, a1 = 2 * PI_F * (k + 1) / n;
        draw_tri(ckp(C), ckp(vadd(C, vmul(vadd(vmul(u, cosf(a1)), vmul(v, sinf(a1))), r))),
                 ckp(vadd(C, vmul(vadd(vmul(u, cosf(a0)), vmul(v, sinf(a0))), r))), col);
    }
}
/* round gauge in a cup on a bracket, facing the eye; frac 0..1 sweeps 270 degrees from lower left to lower right */
static void gauge(V3 C, float r, float frac, int nmaj, const char **lab, const char *title, V3 mount)
{
    V3 n = vnorm(vsub(EYE, C)), u = vnorm(vcross(UP, n));
    if (u.x < 0) u = vmul(u, -1);
    V3 v = vcross(n, u); if (v.y < 0) v = vmul(v, -1);
    cyl(ckp(vsub(C, vmul(n, 0.055f))), ckp(vadd(C, vmul(n, 0.002f))), r * 0.9f, r * 1.12f, 24, 0x1C1C20, 1);
    cyl(ckp(vsub(vsub(C, vmul(n, 0.03f)), vmul(v, r * 0.7f))), ckp(mount), 0.016f, 0.016f, 8, 0x34343A, 0);
    ck_ring(vadd(C, vmul(n, 0.004f)), u, v, r * 0.95f, r * 1.12f, 32, 0xC8CCD6);           /* chrome bezel */
    ck_disc(vadd(C, vmul(n, 0.003f)), u, v, r * 0.96f, 32, 0x101014);
    V3 F = vadd(C, vmul(n, 0.0045f));                                                        /* backlit markings */
    const float A0 = 1.25f * PI_F, SW = 1.5f * PI_F;
    for (int k = 0; k <= 4 * nmaj; k++) {
        float f = (float)k / (4 * nmaj), a = A0 - SW * f, w = r * (k % 4 ? 0.012f : 0.025f), r0 = r * (k % 4 ? 0.82f : 0.74f);
        V3 d = vadd(vmul(u, cosf(a)), vmul(v, sinf(a))), sd = vmul(vadd(vmul(u, -sinf(a)), vmul(v, cosf(a))), w);
        V3 p0 = vadd(F, vmul(d, r0)), p1 = vadd(F, vmul(d, r * 0.92f));
        draw_quad_unlit(ckp(vsub(p0, sd)), ckp(vsub(p1, sd)), ckp(vadd(p1, sd)), ckp(vadd(p0, sd)), 0xF0F0F4);
        if (k % 4 == 0 && lab[k / 4][0]) world_text_c(ckp(vadd(F, vmul(d, r * 0.58f))), ckd(u), ckd(v), lab[k / 4], r * (nmaj > 6 ? 0.017f : 0.02f), 0xF0F0F4, 1);
    }
    world_text_c(ckp(vsub(F, vmul(v, r * 0.6f))), ckd(u), ckd(v), title, r * 0.018f, 0xA0A8B8, 1);
    float a = A0 - SW * clampf(frac, 0, 1);
    V3 d = vadd(vmul(u, cosf(a)), vmul(v, sinf(a))), sd = vmul(vadd(vmul(u, -sinf(a)), vmul(v, cosf(a))), r * 0.035f);
    V3 N = vadd(C, vmul(n, 0.007f)), tail = vsub(N, vmul(d, r * 0.18f));
    draw_tri_unlit(ckp(vsub(tail, sd)), ckp(vadd(N, vmul(d, r * 0.86f))), ckp(vadd(tail, sd)), 0xFF5A10);
    ck_disc(vadd(C, vmul(n, 0.009f)), u, v, r * 0.1f, 12, 0x303036);
}
/* damage: a crack grows along the lit face of the front top tube (like the original), holes where big hits landed;
 * both fit the stretch of the tube the window shows */
static void draw_crack(const Car *c)
{
    const V3 C0 = V(0, 1.0405f, -0.2627f), T = vnorm(V(0, 0.028f, -0.012f)), N = V(0, -0.38f, -0.92f);
    const float X1 = fminf(0.60f, 0.93f * (C0.z - EYE.z) * rtw / rth / FOCAL_Y), X0 = -X1;
    uint32_t r = 12345; float s = 0, x = X0, xe = X0 + (X1 - X0) * clampf(c->damage / 100.f, 0, 1);
    V3 prev = vadd(vadd(C0, V(x, 0, 0)), vmul(N, 0.0015f)), w = vmul(T, 0.0021f), lo = vmul(T, -0.0026f), lo2 = vmul(T, -0.0037f);
    while (x < xe) {
        r = r * 1664525u + 1013904223u; float ra = ((r >> 8) & 0xffff) / 65535.f;
        r = r * 1664525u + 1013904223u; float rb = ((r >> 8) & 0xffff) / 65535.f;
        x = fminf(xe, x + 0.008f + 0.012f * ra);
        s = clampf(s + (rb - 0.5f) * 0.009f, -0.009f, 0.009f);
        V3 p = vadd(vadd(vadd(C0, vmul(T, s)), V(x, 0, 0)), vmul(N, 0.0015f));
        draw_quad(ckp(vadd(prev, lo2)), ckp(vadd(p, lo2)), ckp(vadd(p, lo)), ckp(vadd(prev, lo)), 0xF07060);   /* chipped paint edge */
        draw_quad(ckp(vsub(prev, w)), ckp(vsub(p, w)), ckp(vadd(p, w)), ckp(vadd(prev, w)), 0x1A0404);
        prev = p;
    }
    r = 777;
    for (int h = 0; h < c->nholes; h++) {
        V3 hc = vadd(vadd(C0, V(X0 + (X1 - X0) * (hole_at(h) + HOLE_W / 2) / 100.f, 0, 0)), vmul(N, 0.0018f)), pts[12];
        for (int k = 0; k < 12; k++) {
            r = r * 1664525u + 1013904223u;
            float an = 2 * PI_F * k / 12, rr = 0.013f + 0.005f * ((r >> 8) & 0xff) / 255.f;
            pts[k] = vadd(hc, vadd(V(cosf(an) * rr * 3.6f * X1, 0, 0), vmul(T, sinf(an) * rr)));
        }
        for (int k = 0; k < 12; k++)
            draw_tri(ckp(vadd(hc, vmul(T, -0.0015f))), ckp(vadd(pts[k], vmul(T, -0.0015f))), ckp(vadd(pts[(k + 1) % 12], vmul(T, -0.0015f))), 0xF07060);
        for (int k = 0; k < 12; k++)
            draw_tri(ckp(vadd(hc, vmul(N, 0.0003f))), ckp(vadd(pts[k], vmul(N, 0.0003f))), ckp(vadd(pts[(k + 1) % 12], vmul(N, 0.0003f))), 0x0C0303);
    }
}
static void draw_cockpit(void)
{
    int fp = fp_car();
    if (fp < 0 || !cars[fp].active) return;
    const Car *c = &cars[fp];
    car_body_rows(c, ck_m);
    static const char *bst[] = {"0", "", "50", "", "100"}, *spd[] = {"0", "40", "80", "120", "160", "200", "240", "280"};
    gauge(V(-0.35f, 0.535f, -0.10f), 0.052f, c->boost / 100.f, 4, bst, "BOOST", V(-0.35f, 0.45f, -0.04f));
    gauge(V(0.35f, 0.54f, -0.10f), 0.058f, fmaxf(vdot(c->v, c->F), 0) * 3.6f / 280.f, 7, spd, "KPH", V(0.35f, 0.45f, -0.04f));
    draw_crack(c);
}

static void build_frame(void)
{
    db.n = 0; hb.n = 0; cur = &db;
    draw_sky();
    sky_n = db.n;
    if (state == ST_VIEW) {                       /* plain checkered floor for the model viewer */
        for (int iz = -16; iz < 16; iz++) for (int ix = -16; ix < 16; ix++)
            draw_quad(V(ix * 1.5f, 0, iz * 1.5f), V(ix * 1.5f + 1.5f, 0, iz * 1.5f), V(ix * 1.5f + 1.5f, 0, iz * 1.5f + 1.5f), V(ix * 1.5f, 0, iz * 1.5f + 1.5f), ((ix + iz) & 1) ? 0x6A707C : 0x8C929E);
    }
    if (state != ST_VIEW && state != ST_EDIT) for (int i = 0; i < 2; i++) draw_crane(&cars[i]);
    dyn_shadow_end = db.n;                        /* everything after this (flames, effects) is unlit and casts no shadow */
    draw_cockpit();
    draw_flames();
    draw_parts();
    frame_no++;
    HW = VH * (float)rtw / (float)rth;
    if (!getenv("SR_NOHUD")) draw_hud();
    cur = &db;
}

static const float IDENT_ROWS[12] = {1,0,0,0, 0,1,0,0, 0,0,1,0};

/* draw both cars from the shared model; the body leans, the wheels follow the suspension */
static void draw_cars_gpu(SDL_GPURenderPass *rp, SDL_GPUCommandBuffer *cmd, int shadow, float *fu)
{
    SDL_GPUBufferBinding mb = {vbuf_m, 0};
    SDL_BindGPUVertexBuffers(rp, 0, &mb, 1);
    for (int ci = 0; ci < 2; ci++) {
        const Car *c = &cars[ci];
        if (!c->active) continue;
        int first_person = ci == fp_car();
        int vw = state == ST_VIEW;
        if (!shadow) {
            fu[52] = ((c->col >> 16) & 255) / 255.f * 1.25f; fu[53] = ((c->col >> 8) & 255) / 255.f * 1.25f;
            fu[54] = (c->col & 255) / 255.f * 1.25f; fu[55] = 0;
            SDL_PushGPUFragmentUniformData(cmd, 0, fu, 56 * 4);
        }
        for (int g = 0; g < mdl_ng; g++) {                                   /* the engine top stays visible in the cockpit */
            float m[12];
            if (!car_group_rows(c, g, m, first_person, vw ? view_hide : 0)) continue;
            SDL_PushGPUVertexUniformData(cmd, 1, m, sizeof m);
            SDL_DrawGPUPrimitives(rp, mdl_g[g].count, 1, mdl_g[g].first, 0);
        }
    }
    SDL_PushGPUVertexUniformData(cmd, 1, IDENT_ROWS, sizeof IDENT_ROWS);
}

/* ---------------------------------------------------------------- live exhaust flames */
static void flame_cone(V3 base, V3 ax1, V3 ax2, V3 apex, float r, uint32_t col)
{
    V3 ring[6];
    for (int k = 0; k < 6; k++) { float an = k * PI_F / 3; ring[k] = vadd(base, vadd(vmul(ax1, cosf(an) * r), vmul(ax2, sinf(an) * r))); }
    for (int k = 0; k < 6; k++) draw_tri_unlit(ring[k], ring[(k + 1) % 6], apex, col);
}
/* flames come out of the eight zoomie pipes while the boost burns; they flicker every frame */
static void draw_flames(void)
{
    for (int ci = 0; ci < 2; ci++) {
        const Car *c = &cars[ci];
        if (!c->active || c->flame < 0.03f) continue;
        float m[12]; car_body_rows(c, m);
        V3 R = V(m[0], m[4], m[8]), U = V(m[1], m[5], m[9]), F = V(m[2], m[6], m[10]), T = V(m[3], m[7], m[11]);
        float spd = clampf(vdot(c->v, c->F) / 40.f, 0, 1.5f);
        for (int sd = -1; sd <= 1; sd += 2)
            for (int k = 0; k < 4; k++) {
                V3 lp = FLAMEP[(sd + 1) / 2 * 4 + k], ld = FLAMED[(sd + 1) / 2 * 4 + k];
                V3 base = vadd(T, vadd(vmul(R, lp.x), vadd(vmul(U, lp.y), vmul(F, lp.z))));
                V3 D = vnorm(vadd(vmul(R, ld.x), vadd(vmul(U, ld.y), vmul(F, ld.z))));   /* out of the pipe */
                float rnd = (float)(hash2(frame_no * 13 + ci * 7 + k + (sd + 1) * 5, 99) & 1023) / 1023.f;
                float rn2 = (float)(hash2(frame_no * 7 + ci * 11 + k * 3 + (sd + 1) * 2, 17) & 1023) / 1023.f;
                float fs = ci == fp_car() ? 0.75f : 1.f;          /* smaller when seen from the driver's eye */
                float h = fs * (0.07f + 0.40f * c->flame) * (0.55f + 0.9f * rnd), w = fs * 0.058f * (0.85f + 0.25f * c->flame);   /* base just fills the 12 cm zoomie mouth */
                V3 tilt = vadd(vmul(F, -h * (0.15f + 0.45f * spd)), vmul(R, sd * h * 0.08f * (rn2 - 0.5f)));     /* blown back by the airflow */
                V3 a1 = vnorm(vcross(D, fabsf(vdot(D, F)) < 0.9f ? F : R)), a2 = vcross(D, a1);                /* cone base fills the pipe mouth */
                flame_cone(base, a1, a2, vadd(vadd(base, vmul(D, h)), tilt), w, 0xFF7A10);                           /* orange outer */
                flame_cone(base, a1, a2, vadd(vadd(base, vmul(D, h * 0.62f)), vmul(tilt, 0.6f)), w * 1.08f, 0xFFC020);          /* yellow body, wider so it shows at the base */
                flame_cone(base, a1, a2, vadd(vadd(base, vmul(D, h * 0.32f)), vmul(tilt, 0.3f)), w * 1.15f, 0xFFF4B0);          /* hot white-yellow root */
            }
    }
}

/* orthographic sun cascade: fills 4 rows mapping world -> light clip (x,y in -1..1, z in 0..1) */
static void make_cascade(V3 center, float E, float D, int size, float *rows)
{
    V3 L = vnorm(V(0.42f, 0.78f, 0.46f)), lf = vmul(L, -1.f);
    V3 lr = vnorm(vcross(lf, UP)), lu = vcross(lr, lf);
    float texel = 2 * E / size, cxr = vdot(center, lr), cyu = vdot(center, lu);
    center = vadd(center, vadd(vmul(lr, floorf(cxr / texel) * texel - cxr), vmul(lu, floorf(cyu / texel) * texel - cyu)));
    float r0[4] = {lr.x / E, lr.y / E, lr.z / E, -vdot(center, lr) / E};
    float r1[4] = {lu.x / E, lu.y / E, lu.z / E, -vdot(center, lu) / E};
    float r2[4] = {lf.x / (2 * D), lf.y / (2 * D), lf.z / (2 * D), (D - vdot(center, lf)) / (2 * D)};
    float r3[4] = {0, 0, 0, 1};
    memcpy(rows, r0, 16); memcpy(rows + 4, r1, 16); memcpy(rows + 8, r2, 16); memcpy(rows + 12, r3, 16);
}

/* If shot_path != NULL: render offscreen and write PNG. Otherwise present to the window. */
static void gpu_render(const char *shot_path)
{
    if (win) { int w, h; SDL_GetWindowSizeInPixels(win, &w, &h); if (w < 64) w = 64; if (h < 64) h = 64; if (ensure_targets(w, h)) return; }
    if (state == ST_PREVIEW || state == ST_MENU) preview_cam((float)rtw / rth);
    upload_static();
    build_frame();
    int nd = db.n > DYN_CAP ? DYN_CAP : db.n, nhud = hb.n > HUD_CAP ? HUD_CAP : hb.n;
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(dev);
    uint8_t *map = SDL_MapGPUTransferBuffer(dev, tbuf, true);
    memcpy(map, db.v, (size_t)nd * sizeof(Vtx));
    memcpy(map + (size_t)nd * sizeof(Vtx), hb.v, (size_t)nhud * sizeof(Vtx));
    SDL_UnmapGPUTransferBuffer(dev, tbuf);
    SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTransferBufferLocation src = {tbuf, 0};
    SDL_GPUBufferRegion dst = {vbuf_d, 0, (Uint32)((nd + nhud) * sizeof(Vtx))};
    SDL_UploadToGPUBuffer(cp, &src, &dst, true);
    SDL_EndGPUCopyPass(cp);

    /* sun shadow cascades */
    float nrows[16], frows[16];
    float Ef = 0.5f * sqrtf(sq(ar_x1 - ar_x0) + sq(ar_z1 - ar_z0)) + 80.f, Df = 0.7f * Ef + 160.f, En = 70.f, Dn = 250.f;
    int vwm = state == ST_VIEW;
    if (vwm) { En = 6.f; Dn = 24.f; Ef = 24.f; Df = 50.f; }
    make_cascade(vwm ? V(0, 0.6f, 0) : vadd(cam.pos, vmul(cam.fwd, 40.f)), En, Dn, SH_SIZE[0], nrows);
    make_cascade(vwm ? V(0, 0, 0) : V((ar_x0 + ar_x1) / 2, 20.f, (ar_z0 + ar_z1) / 2), Ef, Df, SH_SIZE[1], frows);
    int sn = vwm ? 0 : static_n;
    for (int ci = 0; ci < 2; ci++) {
        SDL_GPUDepthStencilTargetInfo sd; SDL_zero(sd);
        sd.texture = sh_tex[ci]; sd.clear_depth = 1.f; sd.load_op = SDL_GPU_LOADOP_CLEAR; sd.store_op = SDL_GPU_STOREOP_STORE;
        sd.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE; sd.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        SDL_GPURenderPass *sp_ = SDL_BeginGPURenderPass(cmd, NULL, 0, &sd);
        SDL_BindGPUGraphicsPipeline(sp_, pipe_sh);
        SDL_PushGPUVertexUniformData(cmd, 0, ci ? frows : nrows, 64);
        SDL_PushGPUVertexUniformData(cmd, 1, IDENT_ROWS, sizeof IDENT_ROWS);
        if (sn) {
            SDL_GPUBufferBinding b0 = {vbuf_s, 0};
            SDL_BindGPUVertexBuffers(sp_, 0, &b0, 1);
            SDL_DrawGPUPrimitives(sp_, sn, 1, 0, 0);
        }
        int she = dyn_shadow_end < nd ? dyn_shadow_end : nd;
        if (she > sky_n) {
            SDL_GPUBufferBinding b1 = {vbuf_d, (Uint32)((size_t)sky_n * sizeof(Vtx))};
            SDL_BindGPUVertexBuffers(sp_, 0, &b1, 1);
            SDL_DrawGPUPrimitives(sp_, she - sky_n, 1, 0, 0);
        }
        draw_cars_gpu(sp_, cmd, 1, NULL);
        SDL_EndGPURenderPass(sp_);
    }

    uint32_t fc = FOGCOL;
    SDL_GPUColorTargetInfo ct; SDL_zero(ct);
    ct.texture = msaa > 1 ? rt_ms : rt_res; ct.load_op = SDL_GPU_LOADOP_CLEAR;
    if (msaa > 1) { ct.resolve_texture = rt_res; ct.store_op = SDL_GPU_STOREOP_RESOLVE; } else ct.store_op = SDL_GPU_STOREOP_STORE;
    ct.clear_color = (SDL_FColor){((fc >> 16) & 255) / 255.f, ((fc >> 8) & 255) / 255.f, (fc & 255) / 255.f, 1};
    SDL_GPUDepthStencilTargetInfo dt; SDL_zero(dt);
    dt.texture = depthtex; dt.clear_depth = 0.f; dt.load_op = SDL_GPU_LOADOP_CLEAR; dt.store_op = SDL_GPU_STOREOP_DONT_CARE;
    dt.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE; dt.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    SDL_GPURenderPass *rp_ = SDL_BeginGPURenderPass(cmd, &ct, 1, &dt);

    /* reversed depth (near 1, far 0) in a float buffer: precision stays fine out to the mountains, no z-fighting */
    float fy = FOCAL_Y, fx = fy / ((float)rtw / rth), B = NEAR_Z / (FAR_Z - NEAR_Z);
    float pr[16] = {
        fx*cam.right.x, fx*cam.right.y, fx*cam.right.z, -fx*vdot(cam.pos, cam.right),
        fy*cam.up.x,    fy*cam.up.y,    fy*cam.up.z,    -fy*vdot(cam.pos, cam.up),
        -B*cam.fwd.x,   -B*cam.fwd.y,   -B*cam.fwd.z,   B*(FAR_Z + vdot(cam.pos, cam.fwd)),
        cam.fwd.x,      cam.fwd.y,      cam.fwd.z,      -vdot(cam.pos, cam.fwd) };
    static const float ident[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    V3 L = vnorm(V(0.42f, 0.78f, 0.46f));
    float fu[56] = { cam.pos.x, cam.pos.y, cam.pos.z, 0,
                     ((fc >> 16) & 255) / 255.f, ((fc >> 8) & 255) / 255.f, (fc & 255) / 255.f, 0,
                     FOG0, FOG1, 0, 0,  L.x, L.y, L.z, 0 };
    memcpy(fu + 16, nrows, 64); memcpy(fu + 32, frows, 64);
    fu[48] = 1.f / SH_SIZE[0]; fu[49] = 1.f / SH_SIZE[1]; fu[50] = 0.15f / (2 * Dn); fu[51] = 1.6f / (2 * Df);
    SDL_PushGPUFragmentUniformData(cmd, 0, fu, sizeof fu);
    SDL_BindGPUGraphicsPipeline(rp_, pipe3);
    SDL_GPUTextureSamplerBinding shb[2] = {{sh_tex[0], sh_samp}, {sh_tex[1], sh_samp}};
    SDL_BindGPUFragmentSamplers(rp_, 0, shb, 2);
    SDL_PushGPUVertexUniformData(cmd, 0, pr, sizeof pr);
    SDL_PushGPUVertexUniformData(cmd, 1, IDENT_ROWS, sizeof IDENT_ROWS);
    if (sn) {
        SDL_GPUBufferBinding vbs = {vbuf_s, 0};
        SDL_BindGPUVertexBuffers(rp_, 0, &vbs, 1);
        SDL_DrawGPUPrimitives(rp_, sn, 1, 0, 0);
    }
    SDL_GPUBufferBinding vbd_ = {vbuf_d, 0};
    SDL_BindGPUVertexBuffers(rp_, 0, &vbd_, 1);
    if (nd) SDL_DrawGPUPrimitives(rp_, nd, 1, 0, 0);
    draw_cars_gpu(rp_, cmd, 0, fu);
    if (nhud) {
        SDL_BindGPUGraphicsPipeline(rp_, pipe2);
        SDL_PushGPUVertexUniformData(cmd, 0, ident, sizeof ident);
        SDL_PushGPUVertexUniformData(cmd, 1, IDENT_ROWS, sizeof IDENT_ROWS);
        SDL_GPUBufferBinding vb2 = {vbuf_d, (Uint32)((size_t)nd * sizeof(Vtx))};
        SDL_BindGPUVertexBuffers(rp_, 0, &vb2, 1);
        SDL_DrawGPUPrimitives(rp_, nhud, 1, 0, 0);
    }
    SDL_EndGPURenderPass(rp_);

    if (shot_path) {
        SDL_GPUCopyPass *dp = SDL_BeginGPUCopyPass(cmd);
        SDL_GPUTextureRegion reg; SDL_zero(reg);
        reg.texture = rt_res; reg.w = rtw; reg.h = rth; reg.d = 1;
        SDL_GPUTextureTransferInfo ti; SDL_zero(ti);
        ti.transfer_buffer = dlbuf; ti.pixels_per_row = rtw; ti.rows_per_layer = rth;
        SDL_DownloadFromGPUTexture(dp, &reg, &ti);
        SDL_EndGPUCopyPass(dp);
        SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
        SDL_WaitForGPUFences(dev, true, &fence, 1);
        SDL_ReleaseGPUFence(dev, fence);
        uint8_t *px = SDL_MapGPUTransferBuffer(dev, dlbuf, false);
        save_png(shot_path, px, rtw, rth);
        SDL_UnmapGPUTransferBuffer(dev, dlbuf);
        return;
    }
    SDL_GPUTexture *sw = NULL; Uint32 w = 0, h = 0;
    if (SDL_WaitAndAcquireGPUSwapchainTexture(cmd, win, &sw, &w, &h) && sw) {
        SDL_GPUBlitInfo bl; SDL_zero(bl);
        bl.source.texture = rt_res; bl.source.w = rtw; bl.source.h = rth;
        bl.destination.texture = sw; bl.destination.w = w; bl.destination.h = h;
        bl.load_op = SDL_GPU_LOADOP_DONT_CARE;
        bl.filter = SDL_GPU_FILTER_NEAREST;
        SDL_BlitGPUTexture(cmd, &bl);
    }
    SDL_SubmitGPUCommandBuffer(cmd);
}

/* ------------------------------------------------------------------ main */
/* debug: gap between each drawn tyre's lowest point and the road while physics has the wheel on the ground */
static double wg_sum[4], wg_max[4], wg_min[4]; static int wg_n[4], wg_float[4];
static float drawn_tyre_gap(const Car *c, int g)
{
    float m[12], lo = 1e9f;
    if (!car_group_rows(c, g, m, 0, 0)) return 1e9f;
    for (uint32_t k = 0; k < mdl_g[g].count; k++) {
        const Vtx *v = &mdl_v[mdl_g[g].first + k];
        Q q; track_query(m34_apply(m, V(v->x, v->y, v->z)), c->seg, 12, &q);
        if (q.valid && q.road) lo = fminf(lo, q.h);
    }
    return lo;
}
static void wheel_gap_sample(const Car *c)
{
    for (int g = 0; g < mdl_ng; g++) {
        if (mdl_g[g].role != R_WHEEL) continue;
        int i = mdl_g[g].corner;
        if (c->wl[i] >= WLF[i] - 1e-3f || car_out(c)) continue;           /* wheel in the air */
        float gap = drawn_tyre_gap(c, g);
        if (gap > 1e8f) continue;
        if (gap < -0.05f && getenv("SR_WHEELS")[0] == '2') {
            float body[12]; car_body_rows(c, body);
            V3 lm = m34_apply(body, WMOUNT[i]), pm = body_pt(c, WMOUNT[i]);
            printf("SINK %d gap %+.3f  wl-lmin %+.3f  lean %+.3f  dz %+.3f bump %.3f  lr %.3f lp %.3f\n", i, gap, c->wl[i] - WLMIN[i],
                   vdot(c->U, vsub(lm, pm)), WREST[i] - c->wl[i] - vdot(c->U, vsub(lm, pm)), WBUMP[i], c->lr, c->lp);
        }
        if (!wg_n[i]) wg_min[i] = wg_max[i] = gap;
        wg_sum[i] += gap; wg_n[i]++; wg_max[i] = fmax(wg_max[i], gap); wg_min[i] = fmin(wg_min[i], gap);
        if (gap > 0.02f) wg_float[i]++;
    }
}
static void wheel_gap_report(void)
{
    static const char *nm[4] = {"FL", "FR", "RL", "RR"};
    for (int i = 0; i < 4; i++)
        if (wg_n[i]) printf("GAP %s: mean %+.3f  min %+.3f  max %+.3f m, %d of %d samples float > 2 cm\n",
                            nm[i], wg_sum[i] / wg_n[i], wg_min[i], wg_max[i], wg_float[i], wg_n[i]);
}

static void run_headless(int shot, const char *path, float secs, int track, int camsel)
{
    sel_track = track; build_track(track); cockpit = camsel;
    if (getenv("SR_PROFILE")) { for (int k = 0; k < nseg; k += 25) printf("%d x=%.0f y=%.1f z=%.0f gap=%d\n", k, seg[k].c.x, seg[k].c.y, seg[k].c.z, seg[k].gap); }
    autoplay = 1; start_race();
    if (getenv("SR_LAUNCH")) { int lk = 0; float lv = 0; sscanf(getenv("SR_LAUNCH"), "%d,%f", &lk, &lv);
        car_place(&cars[0], lk, 0, lv); car_place(&cars[1], (lk + 300) % nseg, 2.0f, 0); state = ST_RACE;
        cars[0].crane = cars[1].crane = 0; }                    /* else the start crane puts them back on the grid */
    if (camsel == 4) { start_view(); view_auto = 0; view_yaw = secs * PI_F / 180.f; secs = 0;
        if (getenv("SR_VIEW")) sscanf(getenv("SR_VIEW"), "%f,%f", &view_pitch, &view_dist);
        if (getenv("SR_TARGET")) sscanf(getenv("SR_TARGET"), "%f,%f,%f", &view_target.x, &view_target.y, &view_target.z);
        if (getenv("SR_FLAME")) view_flame = 1;
        if (getenv("SR_FREEZE")) view_anim = 0;
        if (getenv("SR_POSE")) {                     /* debug: SR_POSE="travel,steer", travel -1 = full droop .. +1 = full bump */
            float f = 0, st = 0; sscanf(getenv("SR_POSE"), "%f,%f", &f, &st); view_anim = 0; cars[0].delta = st;
            for (int i = 0; i < 4; i++) cars[0].wl[i] = WREST[i] - f * (f > 0 ? WREST[i] - WLMIN[i] : WLF[i] - WREST[i]);
        } }
    int ticks = (int)(secs * 240);
    for (int i = 0; i < ticks; i++) {
        game_tick(PHYS_DT);
        if (state == ST_DONE) break;
        if (getenv("SR_TRACE") && i % 24 == 0) { Q q; track_query(cars[0].x, cars[0].seg, 30, &q); fprintf(stderr, "t=%5.2f seg %4d lat %5.2f h %5.2f v %5.1f str %5.2f delta %5.2f thr %.2f brk %.2f nwc %d yaw %5.2f roll %5.2f lean %5.3f %5.3f\n", i*PHYS_DT, cars[0].seg, q.lat, q.h, vdot(cars[0].v,cars[0].F), cars[0].str, cars[0].delta, cars[0].thr, cars[0].brk, cars[0].nwc, cars[0].w.y, vdot(cars[0].w, cars[0].F), cars[0].lr, cars[0].lp); }
        if (i % 1200 == 0)
            printf("t=%5.1f p: seg %4d/%d lap %d v=%5.1f dmg=%4.1f | ai: lap %d v=%5.1f dmg=%4.1f falls=%d\n",
                   i * PHYS_DT, cars[0].seg, nseg, cars[0].lap, vlen(cars[0].v), cars[0].damage,
                   cars[1].lap, vlen(cars[1].v), cars[1].damage, stat_falls);
        if (i % 4 == 0) update_camera(&cars[0], 1.f / 60.f, i == 0);
        if (getenv("SR_WHEELS") && i % 24 == 0 && state == ST_RACE) wheel_gap_sample(&cars[0]);
    }
    if (getenv("SR_WHEELS")) wheel_gap_report();
    if (getenv("SR_WHEELS")) {                       /* debug: how far each drawn tyre's lowest point sits above the road */
        const Car *c = &cars[0];
        for (int g = 0; g < mdl_ng; g++) {
            if (mdl_g[g].role != R_WHEEL) continue;
            float m[12]; if (!car_group_rows(c, g, m, 0, 0)) continue;
            float lo = 1e9f;
            for (uint32_t k = 0; k < mdl_g[g].count; k++) {
                const Vtx *v = &mdl_v[mdl_g[g].first + k];
                Q q; track_query(m34_apply(m, V(v->x, v->y, v->z)), c->seg, 12, &q);
                if (q.valid && q.road) lo = fminf(lo, q.h);
            }
            int i = mdl_g[g].corner;
            Q hq; track_query(car_hub(c, i), c->seg, 12, &hq);
            printf("WHEEL %s: drawn tyre bottom %+.3f m above road, physics hub %.3f above road (radius %.3f), wl %.3f\n",
                   mdl_g[g].name, lo, hq.h, WRAD[i], c->wl[i]);
        }
    }
    printf("CONTACT samples=%d impulse=%.0f Ns sparks=%d\n", stat_contacts, stat_push, stat_sparks);
    printf("END state=%d result=%d racetime=%.1f p lap=%d dmg=%.1f best=%.1f | ai lap=%d dmg=%.1f | falls=%d nseg=%d len=%.0f\n",
           state, result, racetime, cars[0].lap, cars[0].damage, cars[0].bestlap, cars[1].lap, cars[1].damage, stat_falls, nseg, nseg * ds);
    if (shot) {
        if (!SDL_Init(SDL_INIT_VIDEO)) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return; }
        if (gpu_init() == 0 && ensure_targets(1920, 1080) == 0) {
            update_camera(&cars[0], 1.f / 60.f, 1);
            if (camsel == 4) {
                cars[0].flame = view_flame ? 1.f : 0.f;
                update_camera(&cars[0], 1.f / 60.f, 1);
            }
            if (camsel == 5) {                       /* debug: overview of the whole track, like the original's title shots */
                V3 mid = V((tr_minx + tr_maxx) * 0.5f, 6, (tr_minz + tr_maxz) * 0.5f);
                cam.pos = V(mid.x + 40, 190, tr_minz - 230);
                cam.fwd = vnorm(vsub(mid, cam.pos));
                cam.right = vnorm(vcross(cam.fwd, UP)); cam.up = vcross(cam.right, cam.fwd);
            }
            if (camsel == 3) {                       /* debug: side view */
                const Car *c = &cars[0];
                cam.pos = vadd(vadd(c->x, vmul(c->R, 6.5f)), vmul(c->U, 0.6f));
                cam.fwd = vnorm(vsub(vadd(c->x, vmul(c->U, 0.0f)), cam.pos));
                cam.right = vnorm(vcross(cam.fwd, UP)); cam.up = vcross(cam.right, cam.fwd);
            }
            if (getenv("SR_CAM")) {                  /* debug: free camera "px,py,pz,tx,ty,tz" */
                V3 p = cam.pos, t = vadd(cam.pos, cam.fwd);
                sscanf(getenv("SR_CAM"), "%f,%f,%f,%f,%f,%f", &p.x, &p.y, &p.z, &t.x, &t.y, &t.z);
                cam.pos = p; cam.fwd = vnorm(vsub(t, p)); cam.right = vnorm(vcross(cam.fwd, UP)); cam.up = vcross(cam.right, cam.fwd);
            }
            if (camsel == 2) {                       /* debug: front three-quarter view of the car */
                const Car *c = &cars[0];
                cam.pos = vadd(vadd(vadd(c->x, vmul(c->F, 6.5f)), vmul(c->R, 4.0f)), vmul(c->U, 1.6f));
                cam.fwd = vnorm(vsub(vadd(c->x, vmul(c->U, 0.3f)), cam.pos));
                cam.right = vnorm(vcross(cam.fwd, UP)); cam.up = vcross(cam.right, cam.fwd);
            }
            gpu_render(path); printf("wrote %s\n", path);
        }
        SDL_Quit();
    }
}

/* one key press, wherever the game is; 0 = quit */
static int full;
static int handle_key(SDL_Keycode k, SDL_Keymod mod)
{
    if (k == SDLK_F11 && win) { full = !full; SDL_SetWindowFullscreen(win, full); return 1; }
    switch (state) {
    case ST_VIEW: if (k == SDLK_ESCAPE) return 0; view_key(k); return 1;
    case ST_MENU: return menu_key(k);
    case ST_EDIT: ed_key(k, mod); return 1;
    case ST_REPLAY: replay_key(k); return 1;
    case ST_PREVIEW: preview_key(k); return 1;
    default: return race_key(k);
    }
}
static void frame_timers(float frame)
{
    if (ms_note_t > 0) ms_note_t -= frame;
    if (ed_msg_t > 0) ed_msg_t -= frame;
    if (state == ST_REPLAY) replay_hold();
}

/* debug: drive the menus and the builder headless with a script of key names, then render one frame:
 * "Down Down Return wait2 text:Bob" (wait N runs N seconds of the game, text: types into a name field) */
static void ui_shot(const char *path, const char *script)
{
    if (!SDL_Init(SDL_INIT_VIDEO) || gpu_init() != 0 || ensure_targets(1920, 1080) != 0) return;
    setup_menu(); update_camera(&cars[0], 1.f / 60.f, 1);
    autoplay = getenv("SR_AUTO") != NULL;                 /* the autopilot drives the player's races */
    char buf[2048]; snprintf(buf, sizeof buf, "%s", script);
    for (char *tok = strtok(buf, " "); tok; tok = strtok(NULL, " ")) {
        if (!strncmp(tok, "wait", 4)) {
            int n = (int)(atof(tok + 4) * 60);
            for (int f = 0; f < n; f++) { for (int k = 0; k < 4; k++) if (!paused) game_tick(PHYS_DT); frame_timers(1.f / 60); update_camera(&cars[cam_car()], 1.f / 60.f, 0); }
            continue;
        }
        if (!strncmp(tok, "text:", 5)) { if (state == ST_MENU) menu_text(tok + 5); else ed_text(tok + 5); continue; }
        SDL_Keymod mod = 0;
        if (!strncmp(tok, "shift+", 6)) { mod = SDL_KMOD_SHIFT; tok += 6; }
        SDL_Keycode k = SDL_GetKeyFromName(tok);
        if (k == SDLK_UNKNOWN) { fprintf(stderr, "ui-shot: unknown key %s\n", tok); continue; }
        if (!handle_key(k, mod)) break;
        update_camera(&cars[cam_car()], 1.f / 60.f, 1);
    }
    if (getenv("SR_OVERVIEW")) {                             /* the whole track from high up, like --shot's CAM 5 */
        V3 mid = V((tr_minx + tr_maxx) * 0.5f, 6, (tr_minz + tr_maxz) * 0.5f);
        cam.pos = V(mid.x + 40, 150, tr_minz - 170);
        cam.fwd = vnorm(vsub(mid, cam.pos)); cam.right = vnorm(vcross(cam.fwd, UP)); cam.up = vcross(cam.right, cam.fwd);
    }
    if (state == ST_RACE || state == ST_DONE) printf("race: t=%.1f lap %d dmg %.1f falls %d best %.1f\n", racetime, cars[0].lap, cars[0].damage, stat_falls, cars[0].bestlap);
    gpu_render(path); printf("wrote %s\n", path);
    SDL_Quit();
}

int main(int argc, char **argv)
{
    if (!car_model_load()) return 1;
    tracks_scan();
    phys_defaults();
    hof_load();
    if (season_peek(0, &sea) || !sea.on) memset(&sea, 0, sizeof sea);
    if (argc >= 2 && !strcmp(argv[1], "--check-tracks")) { ed_check_originals(); return 0; }
    if (argc >= 4 && !strcmp(argv[1], "--ui-shot")) { ui_shot(argv[2], argv[3]); return 0; }
    if (argc >= 3 && !strcmp(argv[1], "--sim")) {
        run_headless(0, NULL, (float)atof(argv[2]), argc > 3 ? atoi(argv[3]) % NTRACKS : 0, 0);
        snd_report();
        return 0;
    }
    if (argc >= 4 && !strcmp(argv[1], "--audio-dump")) {     /* the autopilot's race, mixed into a WAV instead of the device */
        snd_load(); snd_dump_begin();
        run_headless(0, NULL, (float)atof(argv[3]), argc > 4 ? atoi(argv[4]) % NTRACKS : 0, 0);
        snd_report(); snd_dump_write(argv[2]);
        return 0;
    }
    if (argc >= 4 && !strcmp(argv[1], "--menu-frames")) {       /* debug: record the menu at 60 fps into DIR/fNNN.png */
        if (!SDL_Init(SDL_INIT_VIDEO) || gpu_init() != 0 || ensure_targets(1920, 1080) != 0) return 1;
        setup_menu(); update_camera(&cars[0], 1.f / 60.f, 1);
        int n = (int)(atof(argv[3]) * 60);
        for (int f = 0; f < n; f++) {
            for (int k = 0; k < 4; k++) game_tick(PHYS_DT);
            update_camera(&cars[0], 1.f / 60.f, 0);
            char path[1024]; snprintf(path, sizeof path, "%s/f%03d.png", argv[2], f);
            gpu_render(path);
        }
        SDL_Quit();
        return 0;
    }
    if (argc >= 4 && !strcmp(argv[1], "--shot")) {
        run_headless(1, argv[2], (float)atof(argv[3]), argc > 4 ? atoi(argv[4]) % NTRACKS : 0, argc > 5 ? atoi(argv[5]) : 0);
        return 0;
    }
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
    win = SDL_CreateWindow("SCR REDUX", 1600, 900, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (!win || gpu_init() != 0) { fprintf(stderr, "init failed: %s\n", SDL_GetError()); return 1; }
    snd_open();
    keys = SDL_GetKeyboardState(NULL);

    setup_menu();
    for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "--view")) start_view();
    update_camera(&cars[0], 0.016f, 1);
    uint64_t last = SDL_GetTicksNS(); float acc = 0;
    for (int running = 1; running; ) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = 0;
            else if (e.type == SDL_EVENT_MOUSE_MOTION && state == ST_VIEW && (e.motion.state & SDL_BUTTON_LMASK)) {
                view_yaw -= e.motion.xrel * 0.008f; view_pitch += e.motion.yrel * 0.006f; view_auto = 0;
            } else if (e.type == SDL_EVENT_MOUSE_WHEEL && state == ST_VIEW) view_dist *= 1.f - 0.1f * e.wheel.y;
            else if (e.type == SDL_EVENT_MOUSE_WHEEL && state == ST_EDIT) ed_mouse(0, 0, 0, e.wheel.y);
            else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN && state == ST_EDIT) {
                int ww, wh; SDL_GetWindowSize(win, &ww, &wh);
                ed_mouse(e.button.x / (float)ww * HW, e.button.y / (float)wh * VH, e.button.button, 0);
            } else if (e.type == SDL_EVENT_TEXT_INPUT) { if (state == ST_MENU) menu_text(e.text.text); else if (state == ST_EDIT) ed_text(e.text.text); }
            else if (e.type == SDL_EVENT_KEY_DOWN && (!e.key.repeat || ((state == ST_EDIT || state == ST_MENU) &&
                     e.key.key != SDLK_RETURN && e.key.key != SDLK_ESCAPE && e.key.key != SDLK_C && e.key.key != SDLK_T))) {
                if (!handle_key(e.key.key, e.key.mod)) running = 0;
            }
        }
        uint64_t now = SDL_GetTicksNS();
        float frame = (float)(now - last) * 1e-9f; last = now;
        if (frame > 0.1f) frame = 0.1f;
        frame_timers(frame);
        if (!paused) {
            acc += frame;
            while (acc >= PHYS_DT) { game_tick(PHYS_DT); acc -= PHYS_DT; }
        } else snd_paused();
        update_camera(&cars[cam_car()], frame, 0);
        gpu_render(NULL);
    }
    SDL_Quit();
    return 0;
}
