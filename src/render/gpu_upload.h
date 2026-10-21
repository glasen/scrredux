/* render/gpu_upload.h - uploading vertex buffers to the GPU */
#pragma once

/* upload the static world mesh (track + stadium) once per track */
/* a mesh that stays on the GPU until it changes: into *buf (released and made again), 0 if empty */
static int upload_mesh(const VB *b, SDL_GPUBuffer **buf)
{
    if (*buf) {
        SDL_ReleaseGPUBuffer(dev, *buf);
        *buf = NULL;
    }

    if (!b->n) {
        return 0;
    }

    SDL_GPUBufferCreateInfo bi;
    SDL_zero(bi);
    bi.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bi.size = (Uint32)((size_t)b->n * sizeof(Vtx));
    *buf = SDL_CreateGPUBuffer(dev, &bi);
    SDL_GPUTransferBufferCreateInfo tb;
    SDL_zero(tb);
    tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb.size = bi.size;
    SDL_GPUTransferBuffer *t = SDL_CreateGPUTransferBuffer(dev, &tb);
    void *m = SDL_MapGPUTransferBuffer(dev, t, false);
    memcpy(m, b->v, bi.size);
    SDL_UnmapGPUTransferBuffer(dev, t);
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(dev);
    SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTransferBufferLocation src = {t, 0};
    SDL_GPUBufferRegion dst = {*buf, 0, bi.size};
    SDL_UploadToGPUBuffer(cp, &src, &dst, false);
    SDL_EndGPUCopyPass(cp);
    SDL_GPUFence *f = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    SDL_WaitForGPUFences(dev, true, &f, 1);
    SDL_ReleaseGPUFence(dev, f);
    SDL_ReleaseGPUTransferBuffer(dev, t);
    return b->n;
}

static void upload_static(void)
{
    if (static_gen == track_gen) {
        return;
    }

    static_n = upload_mesh(&sb, &vbuf_s);
    static_gen = track_gen;
    fprintf(stderr, "static mesh: %d triangles\n", sb.n / 3);
}

static SDL_GPUBuffer *vbuf_t; /* the track builder's track */
static int tkb_up = -1, tkb_n;

static void upload_track(void)
{
    if (tkb_up != tkb_gen) {
        tkb_n = upload_mesh(&tkb, &vbuf_t);
        tkb_up = tkb_gen;
    }
}
