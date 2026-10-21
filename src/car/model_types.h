/* car/model_types.h - types for the car model loaded from models/stuntcar.scm */
#pragma once

enum {
    R_BODY,
    R_ENGTOP,
    R_ROOF,
    R_DRIVER,
    R_STWHL,
    R_WHEEL,
    R_UPRIGHT,
    R_LARM,
    R_UARM,
    R_CBOT,
    R_CTOP,
    R_SPRING,
    R_LINK
};

typedef struct {
    V3 lb0, lb1, ub0, ub1, L0, U0, elo, ehi; /* wishbone pivot axes, ball joints, coilover eyes at ride height */
    int nlink;
    V3 lin[2], lout[2]; /* links from a fixed inner point to a point on the upright */
    int ntab;
    float *tab; /* wheel travel dz -> lower / upper wishbone angle, sorted by dz */
} Rig;

static Rig rig[4];
static float WDROOP[4];
static V3 KP_P[4], KP_A[4], HUB0[4];
static V3 STW_C, STW_A;
static float STW_RATIO;
