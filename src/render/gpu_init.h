/* render/gpu_init.h - shader loading, render targets and device/pipeline setup */
#pragma once

static SDL_GPUShader *load_shader(const char *file, SDL_GPUShaderStage stage, int nunif, int nsamp)
{
    char path[1024];
    size_t sz;
    snprintf(path, sizeof path, "%s%s", SDL_GetBasePath(), file);
    void *code = SDL_LoadFile(path, &sz);

    if (!code) {
        snprintf(path, sizeof path, "%s", file);
        code = SDL_LoadFile(path, &sz);
    }

    if (!code) {
        fprintf(stderr, "cannot load shader %s (run make)\n", file);
        return NULL;
    }

    SDL_GPUShaderCreateInfo ci;
    SDL_zero(ci);
    ci.code_size = sz;
    ci.code = code;
    ci.entrypoint = "main";
    ci.format = SDL_GPU_SHADERFORMAT_SPIRV;
    ci.stage = stage;
    ci.num_uniform_buffers = nunif;
    ci.num_samplers = nsamp;
    SDL_GPUShader *s = SDL_CreateGPUShader(gpu.device, &ci);
    SDL_free(code);

    if (!s) {
        fprintf(stderr, "shader %s: %s\n", file, SDL_GetError());
    }

    return s;
}

static int ensure_targets(int w, int h)
{
    if (w == gpu.target_w && h == gpu.target_h && gpu.target_resolved) {
        return 0;
    }

    if (gpu.target_msaa) {
        SDL_ReleaseGPUTexture(gpu.device, gpu.target_msaa);
    }

    if (gpu.target_resolved) {
        SDL_ReleaseGPUTexture(gpu.device, gpu.target_resolved);
    }

    if (gpu.depth_texture) {
        SDL_ReleaseGPUTexture(gpu.device, gpu.depth_texture);
    }

    gpu.target_msaa = gpu.target_resolved = gpu.depth_texture = NULL;
    SDL_GPUTextureCreateInfo ti;
    SDL_zero(ti);
    ti.type = SDL_GPU_TEXTURETYPE_2D;
    ti.width = w;
    ti.height = h;
    ti.layer_count_or_depth = 1;
    ti.num_levels = 1;
    ti.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    ti.sample_count = SDL_GPU_SAMPLECOUNT_1;
    ti.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
    gpu.target_resolved = SDL_CreateGPUTexture(gpu.device, &ti);
    ti.sample_count = gpu.msaa > 1 ? SDL_GPU_SAMPLECOUNT_4 : SDL_GPU_SAMPLECOUNT_1;

    if (gpu.msaa > 1) {
        ti.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        gpu.target_msaa = SDL_CreateGPUTexture(gpu.device, &ti);
    }

    ti.format = gpu.depth_format;
    ti.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    gpu.depth_texture = SDL_CreateGPUTexture(gpu.device, &ti);

    if (!gpu.target_resolved || !gpu.depth_texture || (gpu.msaa > 1 && !gpu.target_msaa)) {
        fprintf(stderr, "targets: %s\n", SDL_GetError());
        return -1;
    }

    gpu.target_w = w;
    gpu.target_h = h;

    if ((size_t)w * h * 4 != gpu.download_size) {
        if (gpu.download_buffer) {
            SDL_ReleaseGPUTransferBuffer(gpu.device, gpu.download_buffer);
        }

        SDL_GPUTransferBufferCreateInfo tb;
        SDL_zero(tb);
        tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
        tb.size = (Uint32)((size_t)w * h * 4);
        gpu.download_buffer = SDL_CreateGPUTransferBuffer(gpu.device, &tb);
        gpu.download_size = (size_t)w * h * 4;
    }

    return 0;
}

static int gpu_init(void)
{
    gpu.device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, NULL);

    if (!gpu.device) {
        fprintf(stderr, "GPU device: %s\n", SDL_GetError());
        return -1;
    }

    if (win) {
        if (!SDL_ClaimWindowForGPUDevice(gpu.device, win)) {
            fprintf(stderr, "claim: %s\n", SDL_GetError());
            return -1;
        }

        SDL_SetGPUSwapchainParameters(gpu.device, win, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);
    }

    SDL_GPUShader *vs = load_shader("game.vert.spv", SDL_GPU_SHADERSTAGE_VERTEX, 2, 0);
    SDL_GPUShader *fs = load_shader("game.frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 2);
    SDL_GPUShader *hfs = load_shader("hud.frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0);

    if (!vs || !fs || !hfs) {
        return -1;
    }

    gpu.depth_format = SDL_GPUTextureSupportsFormat(gpu.device, SDL_GPU_TEXTUREFORMAT_D32_FLOAT, SDL_GPU_TEXTURETYPE_2D,
                                                    SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)
                           ? SDL_GPU_TEXTUREFORMAT_D32_FLOAT
                           : SDL_GPU_TEXTUREFORMAT_D24_UNORM;
    if (SDL_GPUTextureSupportsSampleCount(gpu.device, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_SAMPLECOUNT_4) &&
        SDL_GPUTextureSupportsSampleCount(gpu.device, gpu.depth_format, SDL_GPU_SAMPLECOUNT_4)) {
        gpu.msaa = 4;
    }
    SDL_GPUVertexBufferDescription vbd;
    SDL_zero(vbd);
    vbd.slot = 0;
    vbd.pitch = sizeof(Vtx);
    vbd.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    SDL_GPUVertexAttribute at[2];
    SDL_zero(at);
    at[0].location = 0;
    at[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    at[0].offset = 0;
    at[1].location = 1;
    at[1].format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
    at[1].offset = 12;
    SDL_GPUColorTargetDescription ctd;
    SDL_zero(ctd);
    ctd.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

    for (int pass = 0; pass < 2; pass++) {
        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader = vs;
        pi.fragment_shader = pass == 0 ? fs : hfs;
        pi.vertex_input_state.vertex_buffer_descriptions = &vbd;
        pi.vertex_input_state.num_vertex_buffers = 1;
        pi.vertex_input_state.vertex_attributes = at;
        pi.vertex_input_state.num_vertex_attributes = 2;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pi.multisample_state.sample_count = gpu.msaa > 1 ? SDL_GPU_SAMPLECOUNT_4 : SDL_GPU_SAMPLECOUNT_1;
        SDL_GPUColorTargetDescription hctd = ctd; /* the HUD blends by vertex alpha */
        hctd.blend_state.enable_blend = true;
        hctd.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        hctd.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        hctd.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
        hctd.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
        hctd.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        hctd.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_GREATER_OR_EQUAL; /* reversed depth: near = 1, far = 0 */
        pi.depth_stencil_state.enable_depth_test = (pass == 0);
        pi.depth_stencil_state.enable_depth_write = (pass == 0);
        pi.target_info.color_target_descriptions = pass == 0 ? &ctd : &hctd;
        pi.target_info.num_color_targets = 1;
        pi.target_info.depth_stencil_format = gpu.depth_format;
        pi.target_info.has_depth_stencil_target = true;
        SDL_GPUGraphicsPipeline *pl = SDL_CreateGPUGraphicsPipeline(gpu.device, &pi);

        if (!pl) {
            fprintf(stderr, "pipeline: %s\n", SDL_GetError());
            return -1;
        }

        if (pass == 0) {
            gpu.pipeline_world = pl;
        } else {
            gpu.pipeline_hud = pl;
        }
    }
    /* shadow mapping: two cascades (D16), depth-only pipeline, comparison sampler */
    {
        SDL_GPUShader *sfs = load_shader("shadow.frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0);

        if (!sfs) {
            return -1;
        }

        SDL_GPUGraphicsPipelineCreateInfo pi;
        SDL_zero(pi);
        pi.vertex_shader = vs;
        pi.fragment_shader = sfs;
        pi.vertex_input_state.vertex_buffer_descriptions = &vbd;
        pi.vertex_input_state.num_vertex_buffers = 1;
        pi.vertex_input_state.vertex_attributes = at;
        pi.vertex_input_state.num_vertex_attributes = 2;
        pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
        pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
        pi.rasterizer_state.enable_depth_bias = true;
        pi.rasterizer_state.depth_bias_constant_factor = 2.0f;
        pi.rasterizer_state.depth_bias_slope_factor = 2.0f;
        pi.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
        pi.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
        pi.depth_stencil_state.enable_depth_test = true;
        pi.depth_stencil_state.enable_depth_write = true;
        pi.target_info.num_color_targets = 0;
        pi.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
        pi.target_info.has_depth_stencil_target = true;
        gpu.pipeline_shadow = SDL_CreateGPUGraphicsPipeline(gpu.device, &pi);

        if (!gpu.pipeline_shadow) {
            fprintf(stderr, "shadow pipeline: %s\n", SDL_GetError());
            return -1;
        }

        for (int i = 0; i < 2; i++) {
            SDL_GPUTextureCreateInfo ti;
            SDL_zero(ti);
            ti.type = SDL_GPU_TEXTURETYPE_2D;
            ti.format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
            ti.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
            ti.width = ti.height = SH_SIZE[i];
            ti.layer_count_or_depth = 1;
            ti.num_levels = 1;
            ti.sample_count = SDL_GPU_SAMPLECOUNT_1;
            gpu.shadow_texture[i] = SDL_CreateGPUTexture(gpu.device, &ti);

            if (!gpu.shadow_texture[i]) {
                fprintf(stderr, "shadow texture: %s\n", SDL_GetError());
                return -1;
            }
        }

        SDL_GPUSamplerCreateInfo sc;
        SDL_zero(sc);
        sc.min_filter = SDL_GPU_FILTER_LINEAR;
        sc.mag_filter = SDL_GPU_FILTER_LINEAR;
        sc.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        sc.address_mode_u = sc.address_mode_v = sc.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        sc.enable_compare = true;
        sc.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        gpu.shadow_sampler = SDL_CreateGPUSampler(gpu.device, &sc);

        if (!gpu.shadow_sampler) {
            fprintf(stderr, "shadow sampler: %s\n", SDL_GetError());
            return -1;
        }
    }
    SDL_GPUBufferCreateInfo bi;
    SDL_zero(bi);
    bi.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bi.size = (DYN_CAP + HUD_CAP) * sizeof(Vtx);
    gpu.vbuf_dynamic = SDL_CreateGPUBuffer(gpu.device, &bi);
    SDL_GPUTransferBufferCreateInfo tb;
    SDL_zero(tb);
    tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb.size = (DYN_CAP + HUD_CAP) * sizeof(Vtx);
    gpu.upload_buffer = SDL_CreateGPUTransferBuffer(gpu.device, &tb);

    if (!gpu.vbuf_dynamic || !gpu.upload_buffer) {
        fprintf(stderr, "buffers: %s\n", SDL_GetError());
        return -1;
    }

    {
        SDL_GPUBufferCreateInfo mb;
        SDL_zero(mb);
        mb.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        mb.size = (Uint32)((size_t)car_mesh.vertex_count * sizeof(Vtx));
        car_mesh.vbuf = SDL_CreateGPUBuffer(gpu.device, &mb);
        SDL_GPUTransferBufferCreateInfo mt;
        SDL_zero(mt);
        mt.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        mt.size = mb.size;
        SDL_GPUTransferBuffer *t = SDL_CreateGPUTransferBuffer(gpu.device, &mt);
        void *m = SDL_MapGPUTransferBuffer(gpu.device, t, false);
        memcpy(m, car_mesh.vertices, mb.size);
        SDL_UnmapGPUTransferBuffer(gpu.device, t);
        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gpu.device);
        SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(cmd);
        SDL_GPUTransferBufferLocation src = {t, 0};
        SDL_GPUBufferRegion dst = {car_mesh.vbuf, 0, mb.size};
        SDL_UploadToGPUBuffer(cp, &src, &dst, false);
        SDL_EndGPUCopyPass(cp);
        SDL_GPUFence *f = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
        SDL_WaitForGPUFences(gpu.device, true, &f, 1);
        SDL_ReleaseGPUFence(gpu.device, f);
        SDL_ReleaseGPUTransferBuffer(gpu.device, t);
        fprintf(stderr, "car model: %d triangles per car\n", car_mesh.vertex_count / 3);
    }
    return 0;
}
