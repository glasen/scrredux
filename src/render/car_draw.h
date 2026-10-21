/* render/car_draw.h - the cars as dynamic geometry and their GPU draw calls */
#pragma once

static void build_frame(void)
{
    db.n = 0;
    hb.n = 0;
    cur = &db;
    draw_sky();
    sky_n = db.n;

    if (state == ST_VIEW) { /* plain checkered floor for the model viewer */
        for (int iz = -16; iz < 16; iz++) {
            for (int ix = -16; ix < 16; ix++) {
                draw_quad(V(ix * 1.5f, 0, iz * 1.5f), V(ix * 1.5f + 1.5f, 0, iz * 1.5f),
                          V(ix * 1.5f + 1.5f, 0, iz * 1.5f + 1.5f), V(ix * 1.5f, 0, iz * 1.5f + 1.5f),
                          ((ix + iz) & 1) ? 0x6A707C : 0x8C929E);
            }
        }
    }

    if (state != ST_VIEW && state != ST_EDIT) {
        for (int i = 0; i < 2; i++) {
            draw_crane(&cars[i]);
        }
    }

    dyn_shadow_end = db.n; /* everything after this (flames, effects) is unlit and casts no shadow */
    if (state == ST_EDIT) {
        ed_draw3d(); /* the builder's ghost pieces, selection and handles */
    }

    draw_cockpit();
    draw_flames();
    draw_parts();
    frame_no++;
    HW = VH * (float)rtw / (float)rth;

    if (!getenv("SCR_NOHUD")) {
        draw_hud();
    }

    cur = &db;
}

static const float IDENT_ROWS[12] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};

/* draw both cars from the shared model; the body leans, the wheels follow the suspension */
static void draw_cars_gpu(SDL_GPURenderPass *rp, SDL_GPUCommandBuffer *cmd, int shadow, float *fu)
{
    SDL_GPUBufferBinding mb = {vbuf_m, 0};
    SDL_BindGPUVertexBuffers(rp, 0, &mb, 1);

    for (int ci = 0; ci < 2; ci++) {
        const Car *c = &cars[ci];

        if (!c->active) {
            continue;
        }

        int first_person = ci == fp_car();
        int vw = state == ST_VIEW;

        if (!shadow) {
            fu[52] = ((c->col >> 16) & 255) / 255.f * 1.25f;
            fu[53] = ((c->col >> 8) & 255) / 255.f * 1.25f;
            fu[54] = (c->col & 255) / 255.f * 1.25f;
            fu[55] = 0;
            SDL_PushGPUFragmentUniformData(cmd, 0, fu, 56 * 4);
        }

        for (int g = 0; g < mdl_ng; g++) { /* the engine top stays visible in the cockpit */
            float m[12];

            if (!car_group_rows(c, g, m, first_person, vw ? view_hide : 0)) {
                continue;
            }

            SDL_PushGPUVertexUniformData(cmd, 1, m, sizeof m);
            SDL_DrawGPUPrimitives(rp, mdl_g[g].count, 1, mdl_g[g].first, 0);
        }
    }

    SDL_PushGPUVertexUniformData(cmd, 1, IDENT_ROWS, sizeof IDENT_ROWS);
}
