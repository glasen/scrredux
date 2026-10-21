/* track/original_edits.h - drop points and jump reshaping of the original tracks */
#pragma once

/* crane drop points, like the original: the car is lowered onto the middle of a section, never onto a bend piece
 * (templates 1, 3, 6, 7) nor onto a section in the track's own list (the tail of each track's road data) */
static const unsigned char SCR_NOREST[SCR_NTRACKS][16] = {{1, 34},
                                                          {16, 17, 18, 19, 20, 21, 22, 47, 5},
                                                          {38},
                                                          {16, 17, 21, 22, 32, 33, 34, 35},
                                                          {32},
                                                          {4, 13, 38, 51, 52, 53, 54},
                                                          {30, 31, 32, 33, 34, 37, 38, 39, 40, 41, 42, 43, 44, 45},
                                                          {0}};
static const unsigned char SCR_NNOREST[SCR_NTRACKS] = {2, 9, 1, 8, 1, 7, 14, 0};
/* our cars top out at 250 km/h on boost, the original's flew further: its jumps reshaped to suit (build_track_geometry) */
enum { ED_FILL, ED_LIFT, ED_STEP, ED_FLAT };
static const struct {
    unsigned char track, sec, kind;
    float m;
} TRACK_EDIT[] = {{0, 33, ED_LIFT, 2},  {0, 0, ED_LIFT, 4},     {1, 15, ED_LIFT, 2}, {1, 17, ED_LIFT, 2},
                  {1, 19, ED_LIFT, 2},  {1, 21, ED_LIFT, 2},    {3, 17, ED_FILL, 0}, {3, 22, ED_FILL, 3},
                  {3, 34, ED_FILL, 10}, {4, 32, ED_STEP, 8.5f}, {5, 3, ED_LIFT, 5},  {5, 12, ED_LIFT, 4},
                  {5, 38, ED_FILL, 0},  {6, 40, ED_LIFT, 8},    {6, 43, ED_FLAT, 0}};
static const struct {
    unsigned char track, sec;
    signed char from;
    float vd, smax, c, R;
} TRACK_HILL[] = {{4, 3, -1, 50, 0.5f, 4, 120}, {5, 37, -1, 50, 0.5f, 4, 120}, {6, 40, 44, 56, 0.9f, 4, 150}};
