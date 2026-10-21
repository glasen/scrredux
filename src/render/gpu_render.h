/* render/gpu_render.h - shadow cascades and the frame render */
#pragma once

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
    if (state == ST_EDIT) upload_track();
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
    int sn = vwm ? 0 : static_n, tn = state == ST_EDIT ? tkb_n : 0;
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
        if (tn) {
            SDL_GPUBufferBinding b0 = {vbuf_t, 0};
            SDL_BindGPUVertexBuffers(sp_, 0, &b0, 1);
            SDL_DrawGPUPrimitives(sp_, tn, 1, 0, 0);
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
                     state == ST_EDIT ? 3 * FOG0 : FOG0, FOG1, 0, 0,  L.x, L.y, L.z, 0 };   /* the builder: the whole grid clear */
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
    if (tn) {
        SDL_GPUBufferBinding vbt = {vbuf_t, 0};
        SDL_BindGPUVertexBuffers(rp_, 0, &vbt, 1);
        SDL_DrawGPUPrimitives(rp_, tn, 1, 0, 0);
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
