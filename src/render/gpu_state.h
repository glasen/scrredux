/* render/gpu_state.h - SDL_GPU objects and render-target state */
#pragma once

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

/* ---- the car model, loaded from models/stuntcar.scm (car/model_loader.h) ---- */
typedef struct {
    char name[8];
    int role, corner, link;
    uint32_t first, count;
} MGroup;

static MGroup mdl_g[64];
static int mdl_ng, mdl_nv;
static Vtx *mdl_v;
static SDL_GPUBuffer *vbuf_m;

static int dyn_shadow_end, frame_no;
static void draw_flames(void);
