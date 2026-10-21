/* physics/params.h - the tunable vehicle parameters */
#pragma once

/* ---- tunable vehicle physics (menu F2, saved to physics.cfg) ---- */
typedef struct {
    float mass, grav, power, fmax, boostp, boostf, brkf, brkr, drag, mu, corner, lowspd, loadmax;
    float steer, steerfall, steerrate, kf, kr, ccomp, creb, kbump;
    float sroll, syaw, spitch, air, leanr, leanp, rgrip, steerlat, keyrise, keyret;
    float boostrate, shw, shdrop, vtop, vtopb;
} Phys;

static Phys ph;
