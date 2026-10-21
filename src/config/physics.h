/* config/physics.h - physics step and the shorthand names for the tunable vehicle parameters */
#pragma once

#define GRAV (phys.gravity)
#define CAR_M (phys.mass)
#define K_F (phys.spring_front)
#define K_R (phys.spring_rear)
#define C_COMP (phys.damper_compression)
#define C_REB (phys.damper_rebound)
#define K_BUMP (phys.bump_stop_spring)
#define MU (phys.tyre_friction)
#define IX (2200.f * phys.mass / 1000.f)
#define IY (2400.f * phys.mass / 1000.f)
#define IZ (900.f * phys.mass / 1000.f)
#define PHYS_DT (1.0f / 240.0f)
