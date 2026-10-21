/* render/gpu_upload.h - uploading vertex buffers to the GPU */
#pragma once

/* upload the static world mesh (track + stadium) once per track */
/* a mesh that stays on the GPU until it changes: into *buf (released and made again), 0 if empty */
static int upload_mesh(const VB *b, SDL_GPUBuffer **buf)
{
    if (*buf) {
        SDL_ReleaseGPUBuffer(gpu.device, *buf);
        *buf = NULL;
    }

    if (!b->n) {
        return 0;
    }

    SDL_GPUBufferCreateInfo bi;
    SDL_zero(bi);
    bi.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bi.size = (Uint32)((size_t)b->n * sizeof(Vtx));
    *buf = SDL_CreateGPUBuffer(gpu.device, &bi);
    SDL_GPUTransferBufferCreateInfo tb;
    SDL_zero(tb);
    tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb.size = bi.size;
    SDL_GPUTransferBuffer *t = SDL_CreateGPUTransferBuffer(gpu.device, &tb);
    void *m = SDL_MapGPUTransferBuffer(gpu.device, t, false);
    memcpy(m, b->v, bi.size);
    SDL_UnmapGPUTransferBuffer(gpu.device, t);
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gpu.device);
    SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTransferBufferLocation src = {t, 0};
    SDL_GPUBufferRegion dst = {*buf, 0, bi.size};
    SDL_UploadToGPUBuffer(cp, &src, &dst, false);
    SDL_EndGPUCopyPass(cp);
    SDL_GPUFence *f = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    SDL_WaitForGPUFences(gpu.device, true, &f, 1);
    SDL_ReleaseGPUFence(gpu.device, f);
    SDL_ReleaseGPUTransferBuffer(gpu.device, t);
    return b->n;
}

static void upload_static(void)
{
    if (gpu.static_generation == track_gen) {
        return;
    }

    gpu.static_vertices = upload_mesh(&sb, &gpu.vbuf_static);
    gpu.static_generation = track_gen;
    fprintf(stderr, "static mesh: %d triangles\n", sb.n / 3);
}

static void upload_track(void)
{
    if (gpu.track_uploaded_generation != tkb_gen) {
        gpu.track_vertices = upload_mesh(&tkb, &gpu.vbuf_track);
        gpu.track_uploaded_generation = tkb_gen;
    }
}
