/* config/track.h - track and arena dimensions, deck mesh layout */
#pragma once

#define MAXSEG 6000
#define TRACK_HW 4.425f /* half the original's 384-unit road */
#define NTRACKS SCR_NTRACKS
#define SCR_U (2.95f / 128.f) /* metres per x/z unit: the car's 2.95 m wheelbase is 128 units */
#define NET_H 8.f             /* top of the catch net on the arena wall */
#define NSTR 30               /* strips across the deck: a thin white line at each edge, 28 deck strips between */
#define EDGE_W 0.22f
#define SLAB 6    /* precast deck slab length in segments */
#define MAXPC 100 /* pieces per track, like the original's road tables */
#define MAXTRK 48
#define ED_SLOT (MAXTRK - 1)  /* the track builder's test drive */
#define U_PER_M (4.f / SCR_U) /* height units (1/4 of an x/z unit) per metre */
#define STADIUM_WALKWAY 7.f   /* m between the arena wall and the front of the stands */
