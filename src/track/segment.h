/* track/segment.h - the Seg road segment type */
#pragma once

/* ----------------------------------------------------------------- track */
typedef struct {
    V3 c, T, R, N;
    float u, curv, vcurv, bin;
    unsigned char gap, steep;
} Seg; /* bin: sin of the bank into the bend */ /* steep: sheer face, height measured vertically */
