/* config/physics.h - physics step and the shorthand names for the tunable vehicle parameters */
#pragma once

#define GRAV (ph.grav)
#define CAR_M (ph.mass)
#define K_F (ph.kf)
#define K_R (ph.kr)
#define C_COMP (ph.ccomp)
#define C_REB (ph.creb)
#define K_BUMP (ph.kbump)
#define MU (ph.mu)
#define IX (2200.f * ph.mass / 1000.f)
#define IY (2400.f * ph.mass / 1000.f)
#define IZ (900.f * ph.mass / 1000.f)
#define PHYS_DT (1.0f / 240.0f)
