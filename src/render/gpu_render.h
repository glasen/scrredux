/* render/gpu_render.h - shadow cascades and the frame render */
#pragma once

/* orthographic sun cascade: fills 4 rows mapping world -> light clip (x,y in -1..1, z in 0..1) */
static void make_cascade(V3 center, float E, float D, int size, float *rows)
{
    V3 L = vnorm(V(0.42f, 0.78f, 0.46f)), lf = vmul(L, -1.f);
    V3 lr = vnorm(vcross(lf, UP)), lu = vcross(lr, lf);
    float texel = 2 * E / size, cxr = vdot(center, lr), cyu = vdot(center, lu);
    center =
        vadd(center, vadd(vmul(lr, floorf(cxr / texel) * texel - cxr), vmul(lu, floorf(cyu / texel) * texel - cyu)));
    float r0[4] = {lr.x / E, lr.y / E, lr.z / E, -vdot(center, lr) / E};
    float r1[4] = {lu.x / E, lu.y / E, lu.z / E, -vdot(center, lu) / E};
    float r2[4] = {lf.x / (2 * D), lf.y / (2 * D), lf.z / (2 * D), (D - vdot(center, lf)) / (2 * D)};
    float r3[4] = {0, 0, 0, 1};
    memcpy(rows, r0, 16);
    memcpy(rows + 4, r1, 16);
    memcpy(rows + 8, r2, 16);
    memcpy(rows + 12, r3, 16);
}

/* how many vertices of each buffer a frame draws */
typedef struct {
    int dynamic, hud, world, track;
} FrameCounts;

/* the world buffer, the builder's track buffer and the per-frame geometry up to `dynamic_count` (a prefix of the
 * dynamic buffer, after the sky) into whatever render pass is open */
static void draw_world_buffers(SDL_GPURenderPass *pass, int world_count, int track_count, int dynamic_first,
                               int dynamic_count)
{
    if (world_count) {
        SDL_GPUBufferBinding binding = {gpu.vbuf_static, 0};
        SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);
        SDL_DrawGPUPrimitives(pass, world_count, 1, 0, 0);
    }

    if (track_count) {
        SDL_GPUBufferBinding binding = {gpu.vbuf_track, 0};
        SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);
        SDL_DrawGPUPrimitives(pass, track_count, 1, 0, 0);
    }

    if (dynamic_count > 0) {
        SDL_GPUBufferBinding binding = {gpu.vbuf_dynamic, (Uint32)((size_t)dynamic_first * sizeof(Vtx))};
        SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);
        SDL_DrawGPUPrimitives(pass, dynamic_count, 1, 0, 0);
    }
}

/* the frame's dynamic and HUD vertices into the GPU buffer */
static void upload_frame_vertices(SDL_GPUCommandBuffer *cmd, const FrameCounts *n)
{
    uint8_t *map = SDL_MapGPUTransferBuffer(gpu.device, gpu.upload_buffer, true);
    memcpy(map, db.v, (size_t)n->dynamic * sizeof(Vtx));
    memcpy(map + (size_t)n->dynamic * sizeof(Vtx), hb.v, (size_t)n->hud * sizeof(Vtx));
    SDL_UnmapGPUTransferBuffer(gpu.device, gpu.upload_buffer);
    SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTransferBufferLocation src = {gpu.upload_buffer, 0};
    SDL_GPUBufferRegion dst = {gpu.vbuf_dynamic, 0, (Uint32)((n->dynamic + n->hud) * sizeof(Vtx))};
    SDL_UploadToGPUBuffer(cp, &src, &dst, true);
    SDL_EndGPUCopyPass(cp);
}

/* the near and far sun shadow cascades; *near_depth and *far_depth are their depth ranges */
static void sun_cascades(float *near_rows, float *far_rows, float *near_depth, float *far_depth)
{
    float far_extent = 0.5f * sqrtf(sq(ar_x1 - ar_x0) + sq(ar_z1 - ar_z0)) + SHADOW_FAR_MARGIN;
    float far_range = SHADOW_FAR_DEPTH_FACTOR * far_extent + SHADOW_FAR_DEPTH_BASE;
    float near_extent = SHADOW_NEAR_EXTENT, near_range = SHADOW_NEAR_DEPTH;
    int viewer_mode = state == ST_VIEW; /* the car alone: tight cascades round it */

    if (viewer_mode) {
        near_extent = 6.f;
        near_range = 24.f;
        far_extent = 24.f;
        far_range = 50.f;
    }

    make_cascade(viewer_mode ? V(0, 0.6f, 0) : vadd(cam.pos, vmul(cam.fwd, SHADOW_NEAR_AHEAD)), near_extent, near_range,
                 SH_SIZE[0], near_rows);
    make_cascade(viewer_mode ? V(0, 0, 0) : V((ar_x0 + ar_x1) / 2, SHADOW_FAR_CENTRE_HEIGHT, (ar_z0 + ar_z1) / 2),
                 far_extent, far_range, SH_SIZE[1], far_rows);
    *near_depth = near_range;
    *far_depth = far_range;
}

/* one depth-only pass per cascade: the world, the builder's track, the dynamic scenery and the cars */
static void shadow_passes(SDL_GPUCommandBuffer *cmd, const FrameCounts *n, const float *near_rows,
                          const float *far_rows)
{
    for (int cascade = 0; cascade < 2; cascade++) {
        SDL_GPUDepthStencilTargetInfo depth;
        SDL_zero(depth);
        depth.texture = gpu.shadow_texture[cascade];
        depth.clear_depth = 1.f;
        depth.load_op = SDL_GPU_LOADOP_CLEAR;
        depth.store_op = SDL_GPU_STOREOP_STORE;
        depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
        depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
        SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, NULL, 0, &depth);
        SDL_BindGPUGraphicsPipeline(pass, gpu.pipeline_shadow);
        SDL_PushGPUVertexUniformData(cmd, 0, cascade ? far_rows : near_rows, 64);
        SDL_PushGPUVertexUniformData(cmd, 1, IDENT_ROWS, sizeof IDENT_ROWS);
        int shadow_end = SDL_min(gpu.dynamic_shadow_end, n->dynamic); /* what casts shadows: up to the cars */
        draw_world_buffers(pass, n->world, n->track, gpu.sky_vertices, shadow_end - gpu.sky_vertices);
        draw_cars_gpu(pass, cmd, 1, NULL);
        SDL_EndGPURenderPass(pass);
    }
}

/* the camera's view-projection rows: reversed depth (near 1, far 0) in a float buffer, so precision stays fine out to
 * the mountains and nothing z-fights */
static void view_projection_rows(float pr[16])
{
    float fy = FOCAL_Y, fx = fy / ((float)gpu.target_w / gpu.target_h), B = NEAR_Z / (FAR_Z - NEAR_Z);
    float rows[16] = {fx * cam.right.x, fx * cam.right.y, fx * cam.right.z, -fx * vdot(cam.pos, cam.right),
                      fy * cam.up.x,    fy * cam.up.y,    fy * cam.up.z,    -fy * vdot(cam.pos, cam.up),
                      -B * cam.fwd.x,   -B * cam.fwd.y,   -B * cam.fwd.z,   B * (FAR_Z + vdot(cam.pos, cam.fwd)),
                      cam.fwd.x,        cam.fwd.y,        cam.fwd.z,        -vdot(cam.pos, cam.fwd)};
    memcpy(pr, rows, sizeof rows);
}

/* the fragment shader's constants: camera, fog, sun, both shadow cascades and their texel and bias sizes */
static void fragment_uniforms(float fu[56], const float *near_rows, const float *far_rows, float near_depth,
                              float far_depth)
{
    uint32_t fc = FOGCOL;
    V3 L = vnorm(V(0.42f, 0.78f, 0.46f));
    float head[16] = {cam.pos.x,
                      cam.pos.y,
                      cam.pos.z,
                      0,
                      ((fc >> 16) & 255) / 255.f,
                      ((fc >> 8) & 255) / 255.f,
                      (fc & 255) / 255.f,
                      0,
                      state == ST_EDIT ? 3 * FOG0 : FOG0, /* the builder: the whole grid clear */
                      FOG1,
                      0,
                      0,
                      L.x,
                      L.y,
                      L.z,
                      0};
    memset(fu, 0, 56 * sizeof(float));
    memcpy(fu, head, sizeof head);
    memcpy(fu + 16, near_rows, 64);
    memcpy(fu + 32, far_rows, 64);
    fu[48] = 1.f / SH_SIZE[0];
    fu[49] = 1.f / SH_SIZE[1];
    fu[50] = SHADOW_NEAR_BIAS / (2 * near_depth);
    fu[51] = SHADOW_FAR_BIAS / (2 * far_depth);
}

/* the lit world, the cars and then the HUD on top, with 4x MSAA resolved into the output texture */
static void colour_pass(SDL_GPUCommandBuffer *cmd, const FrameCounts *n, const float *near_rows, const float *far_rows,
                        float near_depth, float far_depth)
{
    uint32_t fc = FOGCOL;
    SDL_GPUColorTargetInfo colour;
    SDL_zero(colour);
    colour.texture = gpu.msaa > 1 ? gpu.target_msaa : gpu.target_resolved;
    colour.load_op = SDL_GPU_LOADOP_CLEAR;

    if (gpu.msaa > 1) {
        colour.resolve_texture = gpu.target_resolved;
        colour.store_op = SDL_GPU_STOREOP_RESOLVE;
    } else {
        colour.store_op = SDL_GPU_STOREOP_STORE;
    }

    colour.clear_color = (SDL_FColor){((fc >> 16) & 255) / 255.f, ((fc >> 8) & 255) / 255.f, (fc & 255) / 255.f, 1};
    SDL_GPUDepthStencilTargetInfo depth;
    SDL_zero(depth);
    depth.texture = gpu.depth_texture;
    depth.clear_depth = 0.f;
    depth.load_op = SDL_GPU_LOADOP_CLEAR;
    depth.store_op = SDL_GPU_STOREOP_DONT_CARE;
    depth.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depth.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &colour, 1, &depth);

    float view_projection[16], fu[56];
    view_projection_rows(view_projection);
    fragment_uniforms(fu, near_rows, far_rows, near_depth, far_depth);
    SDL_PushGPUFragmentUniformData(cmd, 0, fu, sizeof fu);
    SDL_BindGPUGraphicsPipeline(pass, gpu.pipeline_world);
    SDL_GPUTextureSamplerBinding shadow_maps[2] = {{gpu.shadow_texture[0], gpu.shadow_sampler},
                                                   {gpu.shadow_texture[1], gpu.shadow_sampler}};
    SDL_BindGPUFragmentSamplers(pass, 0, shadow_maps, 2);
    SDL_PushGPUVertexUniformData(cmd, 0, view_projection, sizeof view_projection);
    SDL_PushGPUVertexUniformData(cmd, 1, IDENT_ROWS, sizeof IDENT_ROWS);
    draw_world_buffers(pass, n->world, n->track, 0, n->dynamic);
    draw_cars_gpu(pass, cmd, 0, fu);

    if (n->hud) {
        static const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
        SDL_BindGPUGraphicsPipeline(pass, gpu.pipeline_hud);
        SDL_PushGPUVertexUniformData(cmd, 0, identity, sizeof identity);
        SDL_PushGPUVertexUniformData(cmd, 1, IDENT_ROWS, sizeof IDENT_ROWS);
        SDL_GPUBufferBinding binding = {gpu.vbuf_dynamic, (Uint32)((size_t)n->dynamic * sizeof(Vtx))};
        SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);
        SDL_DrawGPUPrimitives(pass, n->hud, 1, 0, 0);
    }

    SDL_EndGPURenderPass(pass);
}

/* reads the rendered image back and writes it as a PNG */
static void save_screenshot(SDL_GPUCommandBuffer *cmd, const char *path)
{
    SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureRegion region;
    SDL_zero(region);
    region.texture = gpu.target_resolved;
    region.w = gpu.target_w;
    region.h = gpu.target_h;
    region.d = 1;
    SDL_GPUTextureTransferInfo transfer;
    SDL_zero(transfer);
    transfer.transfer_buffer = gpu.download_buffer;
    transfer.pixels_per_row = gpu.target_w;
    transfer.rows_per_layer = gpu.target_h;
    SDL_DownloadFromGPUTexture(copy, &region, &transfer);
    SDL_EndGPUCopyPass(copy);
    SDL_GPUFence *fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmd);
    SDL_WaitForGPUFences(gpu.device, true, &fence, 1);
    SDL_ReleaseGPUFence(gpu.device, fence);
    uint8_t *pixels = SDL_MapGPUTransferBuffer(gpu.device, gpu.download_buffer, false);
    save_png(path, pixels, gpu.target_w, gpu.target_h);
    SDL_UnmapGPUTransferBuffer(gpu.device, gpu.download_buffer);
}

/* scales the rendered image onto the window's swapchain image and presents it */
static void present_to_window(SDL_GPUCommandBuffer *cmd)
{
    SDL_GPUTexture *swapchain = NULL;
    Uint32 w = 0, h = 0;

    if (SDL_WaitAndAcquireGPUSwapchainTexture(cmd, win, &swapchain, &w, &h) && swapchain) {
        SDL_GPUBlitInfo blit;
        SDL_zero(blit);
        blit.source.texture = gpu.target_resolved;
        blit.source.w = gpu.target_w;
        blit.source.h = gpu.target_h;
        blit.destination.texture = swapchain;
        blit.destination.w = w;
        blit.destination.h = h;
        blit.load_op = SDL_GPU_LOADOP_DONT_CARE;
        blit.filter = SDL_GPU_FILTER_NEAREST;
        SDL_BlitGPUTexture(cmd, &blit);
    }

    SDL_SubmitGPUCommandBuffer(cmd);
}

/* If shot_path != NULL: render offscreen and write PNG. Otherwise present to the window. */
static void gpu_render(const char *shot_path)
{
    if (win) {
        int w, h;
        SDL_GetWindowSizeInPixels(win, &w, &h);

        if (ensure_targets(SDL_max(w, 64), SDL_max(h, 64))) {
            return;
        }
    }

    if (state == ST_PREVIEW || state == ST_MENU) {
        preview_cam((float)gpu.target_w / gpu.target_h);
    }

    upload_static();

    if (state == ST_EDIT) {
        upload_track();
    }

    build_frame();
    FrameCounts n = {
        .dynamic = SDL_min(db.n, DYN_CAP),
        .hud = SDL_min(hb.n, HUD_CAP),
        .world = state == ST_VIEW ? 0 : gpu.static_vertices,
        .track = state == ST_EDIT ? gpu.track_vertices : 0,
    };
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gpu.device);
    upload_frame_vertices(cmd, &n);
    float near_rows[16], far_rows[16], near_depth, far_depth;
    sun_cascades(near_rows, far_rows, &near_depth, &far_depth);
    shadow_passes(cmd, &n, near_rows, far_rows);
    colour_pass(cmd, &n, near_rows, far_rows, near_depth, far_depth);

    if (shot_path) {
        save_screenshot(cmd, shot_path);
    } else {
        present_to_window(cmd);
    }
}
