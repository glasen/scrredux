/* render/car_mesh.h - the car model's mesh, loaded from models/stuntcar.scm (car/model_loader.h) */
#pragma once

typedef struct {
    char name[8];
    int role, corner, link;
    uint32_t first, count;
} MGroup;

static struct {
    MGroup groups[64];
    int group_count, vertex_count;
    Vtx *vertices;
    SDL_GPUBuffer *vbuf;
} car_mesh;
