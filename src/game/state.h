/* game/state.h - game state machine and shared globals */
#pragma once

enum { ST_MENU, ST_COUNT, ST_RACE, ST_DONE, ST_VIEW, ST_REPLAY, ST_EDIT, ST_PREVIEW };
static Car cars[2];
static int state = ST_MENU, laps = 3, sel_track = 0, result = 0, paused = 0, cockpit = 1;
static float state_t, racetime;
static char msg[64];
static float msg_t;
static int stat_falls, stat_contacts;
static float stat_push;
static const float skill_vcap[3] = {56, 62, 69}, skill_alat[3] = {11.0f, 13.5f, 16.0f};

static void set_msg(const char *s, float t)
{
    snprintf(msg, sizeof msg, "%s", s);
    msg_t = t;
}
