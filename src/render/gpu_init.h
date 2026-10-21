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
    SDL_GPUShader *s = SDL_CreateGPUShader(dev, &ci);
    SDL_free(code);
    if (!s) {
        fprintf(stderr, "shader %s: %s\n", file, SDL_GetError());
    }
    return s;
}

static int ensure_targets(int w, int h)
{
    if (w == rtw && h == rth && rt_res) {
        return 0;
    }
    if (rt_ms) {
        SDL_ReleaseGPUTexture(dev, rt_ms);
    }
    if (rt_res) {
        SDL_ReleaseGPUTexture(dev, rt_res);
    }
    if (depthtex) {
        SDL_ReleaseGPUTexture(dev, depthtex);
    }
    rt_ms = rt_res = depthtex = NULL;
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
    rt_res = SDL_CreateGPUTexture(dev, &ti);
    ti.sample_count = msaa > 1 ? SDL_GPU_SAMPLECOUNT_4 : SDL_GPU_SAMPLECOUNT_1;
    if (msaa > 1) {
        ti.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        rt_ms = SDL_CreateGPUTexture(dev, &ti);
    }
    ti.format = dfmt;
    ti.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    depthtex = SDL_CreateGPUTexture(dev, &ti);
    if (!rt_res || !depthtex || (msaa > 1 && !rt_ms)) {
        fprintf(stderr, "targets: %s\n", SDL_GetError());
        return -1;
    }
    rtw = w;
    rth = h;
    if ((size_t)w * h * 4 != dlsize) {
        if (dlbuf) {
            SDL_ReleaseGPUTransferBuffer(dev, dlbuf);
        }
        SDL_GPUTransferBufferCreateInfo tb;
        SDL_zero(tb);
        tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
        tb.size = (Uint32)((size_t)w * h * 4);
        dlbuf = SDL_CreateGPUTransferBuffer(dev, &tb);
        dlsize = (size_t)w * h * 4;
    }
    return 0;
}

static int gpu_init(void)
{
    dev = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, NULL);
    if (!dev) {
        fprintf(stderr, "GPU device: %s\n", SDL_GetError());
        return -1;
    }
    if (win) {
        if (!SDL_ClaimWindowForGPUDevice(dev, win)) {
            fprintf(stderr, "claim: %s\n", SDL_GetError());
            return -1;
        }
        SDL_SetGPUSwapchainParameters(dev, win, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, SDL_GPU_PRESENTMODE_VSYNC);
    }
    SDL_GPUShader *vs = load_shader("game.vert.spv", SDL_GPU_SHADERSTAGE_VERTEX, 2, 0);
    SDL_GPUShader *fs = load_shader("game.frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 2);
    SDL_GPUShader *hfs = load_shader("hud.frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0);
    if (!vs || !fs || !hfs) {
        return -1;
    }
    dfmt = SDL_GPUTextureSupportsFormat(dev, SDL_GPU_TEXTUREFORMAT_D32_FLOAT, SDL_GPU_TEXTURETYPE_2D,
                                        SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)
               ? SDL_GPU_TEXTUREFORMAT_D32_FLOAT
               : SDL_GPU_TEXTUREFORMAT_D24_UNORM;
    if (SDL_GPUTextureSupportsSampleCount(dev, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_SAMPLECOUNT_4) &&
        SDL_GPUTextureSupportsSampleCount(dev, dfmt, SDL_GPU_SAMPLECOUNT_4)) {
        msaa = 4;
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
        pi.multisample_state.sample_count = msaa > 1 ? SDL_GPU_SAMPLECOUNT_4 : SDL_GPU_SAMPLECOUNT_1;
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
        pi.target_info.depth_stencil_format = dfmt;
        pi.target_info.has_depth_stencil_target = true;
        SDL_GPUGraphicsPipeline *pl = SDL_CreateGPUGraphicsPipeline(dev, &pi);
        if (!pl) {
            fprintf(stderr, "pipeline: %s\n", SDL_GetError());
            return -1;
        }
        if (pass == 0) {
            pipe3 = pl;
        } else {
            pipe2 = pl;
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
        pipe_sh = SDL_CreateGPUGraphicsPipeline(dev, &pi);
        if (!pipe_sh) {
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
            sh_tex[i] = SDL_CreateGPUTexture(dev, &ti);
            if (!sh_tex[i]) {
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
        sh_samp = SDL_CreateGPUSampler(dev, &sc);
        if (!sh_samp) {
            fprintf(stderr, "shadow sampler: %s\n", SDL_GetError());
            return -1;
        }
    }
    SDL_GPUBufferCreateInfo bi;
    SDL_zero(bi);
    bi.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    bi.size = (DYN_CAP + HUD_CAP) * sizeof(Vtx);
    vbuf_d = SDL_CreateGPUBuffer(dev, &bi);
    SDL_GPUTransferBufferCreateInfo tb;
    SDL_zero(tb);
    tb.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tb.size = (DYN_CAP + HUD_CAP) * sizeof(Vtx);
    tbuf = SDL_CreateGPUTransferBuffer(dev, &tb);
    if (!vbuf_d || !tbuf) {
        fprintf(stderr, "buffers: %s\n", SDL_GetError());
        return -1;
    }
    {
        SDL_GPUBufferCreateInfo mb;
        SDL_zero(mb);
        mb.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
        mb.size = (Uint32)((size_t)mdl_nv * sizeof(Vtx));
        vbuf_m = SDL_CreateGPUBuffer(dev, &mb);
        SDL_GPUTransferBufferCreateInfo mt;
        SDL_zero(mt);
        mt.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
        mt.size = mb.size;
        SDL_GPUTransferBuffer *t = SDL_CreateGPUTransferBuffer(dev, &mt);
        void *m = SDL_MapGPUTransferBuffer(dev, t, false);
        memcpy(m, mdl_v, mb.size);
        SDL_UnmapGPUTransferBuffer(dev, t);
        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(dev);
        SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(cmd);
        SDL_GPUTransferBufferLocation src = {t, 0};
        SDL_GPUBufferRegion dst = {vbuf_m, 0, mb.size};
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
