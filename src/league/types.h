/* league/types.h - season types and state */
#pragma once

enum { RM_DEBUG, RM_SEASON, RM_PRACTICE, RM_TEST };

static int race_mode = RM_DEBUG, opp_drv = 9;
static float opp_vcap = 62, opp_alat = 13.5f;

typedef struct {
    int a, b, track, done, winner, fast;
    float ta, tb, la, lb;
} Race; /* times < 0: did not finish */

typedef struct {
    int on, season, race; /* race: the next of each division's six, raced in step */
    char pname[16];
    int div[NDRV], pts[NDRV], raced[NDRV], wins[NDRV], lapp[NDRV];
    int nholes; /* the player's cage holes stay for the season */
    Race sched[4][6];
} Season;

static Season sea, sea_end; /* sea_end: the season just finished, for its final tables */
static int sea_moved;       /* player at the season's end: -1 promoted, +1 relegated, 2 champion */
static Race last_res;
static int last_sim[6], nlast_sim, last_retired;
static uint32_t sea_rng = 77;

static float sea_rand(void)
{
    sea_rng = sea_rng * 1664525u + 1013904223u;
    return (sea_rng >> 8) / 16777216.0f;
}
