/* league/data.h - driver names, divisions and reference lap times */
#pragma once

/* the racing season (the original's single player league: four divisions of three drivers, two tracks
 * each, every driver races the other two on both, 2 points a win and 1 for the fastest lap, the top driver goes up and
 * the bottom one down), the Hall of Fame, season saves and race replays.  */
static const char *const DRV_NAME[11] = {"Hot Rod",   "Whizz Kid",    "Bad Guy",    "The Dodger",
                                         "Big Ed",    "Max Boost",    "Dare Devil", "High Flyer",
                                         "Bully Boy", "Jumping Jack", "Road Hog"};
static const int DIV_TRACK[4][2] = {{5, 4}, {6, 7}, {1, 3}, {0, 2}}; /* Division I .. IV */
/* the autopilot's best laps (--sim): the yardstick for races between computer drivers and the Hall of Fame's
 * starting records */
static const float REF_LAP[SCR_NTRACKS] = {49.4f, 53.4f, 57.7f, 49.0f, 57.8f, 85.6f, 56.9f, 87.4f};
static const char *const ROMAN[4] = {"1", "2", "3", "4"};
