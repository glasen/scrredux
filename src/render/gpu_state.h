/* render/gpu_state.h - SDL_GPU objects and render-target state */
#pragma once

/* ------------------------------------------------------------- GPU layer */
/* the GPU: device, pipelines, buffers and render targets */
static struct {
    SDL_GPUDevice *device;
    SDL_GPUGraphicsPipeline *pipeline_world, *pipeline_hud, *pipeline_shadow;
    SDL_GPUBuffer *vbuf_static, *vbuf_dynamic, *vbuf_track; /* the world, per-frame geometry, the builder's track */
    SDL_GPUTransferBuffer *upload_buffer, *download_buffer;
    SDL_GPUTexture *target_msaa, *target_resolved, *depth_texture;
    SDL_GPUTexture *shadow_texture[2]; /* near and far cascade */
    SDL_GPUSampler *shadow_sampler;
    SDL_GPUTextureFormat depth_format;
    int sky_vertices;
    int target_w, target_h, msaa;
    int static_generation, static_vertices;        /* the track_gen the static buffer holds, and its size */
    int track_uploaded_generation, track_vertices; /* the same for the builder's track (tkb_gen) */
    size_t download_size;
    int dynamic_shadow_end, frame_no;
} gpu = {.msaa = 1, .static_generation = -1, .track_uploaded_generation = -1};

static const int SH_SIZE[2] = {3072, 4096};
static void draw_flames(void);
