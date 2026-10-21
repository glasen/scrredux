/* config/render.h - camera, fog and buffer-size constants */
#pragma once

#define FOCAL_Y 2.2222f /* 1/tan(fovy/2) */
#define NEAR_Z 0.25f
#define FAR_Z 6000.0f
#define FOG0 300.0f
#define FOG1 3500.0f
#define FOGCOL 0xB8CCE0u
#define ZENCOL 0x3060B8u
#define VH 270.0f /* HUD virtual height */
#define DYN_CAP 400000
#define HUD_CAP 400000

/* ---- sun shadows: two orthographic cascades (render/gpu_render.h) ---- */
#define SHADOW_NEAR_EXTENT 70.f       /* m half width of the near cascade, which follows the camera */
#define SHADOW_NEAR_DEPTH 250.f       /* m depth range of the near cascade */
#define SHADOW_NEAR_AHEAD 40.f        /* m ahead of the camera its centre lies */
#define SHADOW_FAR_MARGIN 80.f        /* m added to half the arena's diagonal for the far cascade */
#define SHADOW_FAR_DEPTH_FACTOR 0.7f  /* far cascade depth range: this times its extent ... */
#define SHADOW_FAR_DEPTH_BASE 160.f   /* ... plus this */
#define SHADOW_FAR_CENTRE_HEIGHT 20.f /* m: the far cascade is centred on the arena at this height */
#define SHADOW_NEAR_BIAS 0.15f        /* shadow depth bias, in units of the cascade's depth range / 2 */
#define SHADOW_FAR_BIAS 1.6f
