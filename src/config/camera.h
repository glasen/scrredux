/* config/camera.h - the chase, cockpit, crash and trackside cameras (game/camera.h) */
#pragma once

/* ---- cockpit ---- */
#define COCKPIT_FOLLOW_RATE 40.f /* per second the cockpit camera follows the car's attitude */
#define COCKPIT_LOOK_DOWN 0.04f  /* the view is tipped this much down from the car's forward axis */

/* ---- crash: the wreck seen from outside ---- */
#define CRASH_CAM_ANGLE 2.2f       /* rad round from the car's heading */
#define CRASH_CAM_ORBIT_RATE 0.25f /* rad per second it circles the wreck */
#define CRASH_CAM_DISTANCE 14.f    /* m */
#define CRASH_CAM_HEIGHT 6.f       /* m above the car */
#define CRASH_CAM_MIN_ABOVE_GROUND 3.f

/* ---- replay: trackside cameras, each takes the car past it ---- */
#define TV_CAM_SPACING 120     /* segments between cameras */
#define TV_CAM_OFFSET 9.f      /* m beside the road edge, alternating sides */
#define TV_CAM_HEIGHT 3.5f     /* m above the road */
#define TV_CAM_MIN_HEIGHT 2.f  /* m above the ground */
#define TV_CAM_AIM_HEIGHT 0.5f /* m above the car it looks at */

/* ---- chase ---- */
#define CHASE_PITCH_FACTOR 0.3f /* how much of the car's pitch the chase heading takes */
#define CHASE_HEADING_RATE 5.f  /* per second the heading follows the car */
#define CHASE_POSITION_RATE 9.f /* per second the camera follows its place behind the car */
#define CHASE_DISTANCE 10.0f    /* m behind */
#define CHASE_HEIGHT 3.6f       /* m above */
#define CHASE_AIM_UP 0.6f       /* m above the car it looks at, and */
#define CHASE_AIM_AHEAD 3.f     /* m ahead of it */
#define CHASE_UP_BLEND 0.3f     /* how far the camera's up axis leans to the car's */
