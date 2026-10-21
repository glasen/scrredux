/* physics/defaults.h - default values of the vehicle parameters */
#pragma once

/* ------------------------------------------------------- vehicle physics constants */
static void phys_defaults(void)
{
    ph.mass = 1000.f;
    ph.grav = 13.f;
    ph.power = 337000.f;
    ph.fmax = 9900.f;
    ph.boostp = 758000.f;
    ph.boostf = 9750.f;
    ph.boostrate = 2.f; /* boost tank (100) drains this much per second */
    ph.brkf = 4500.f;
    ph.brkr = 3200.f;
    ph.drag = 0.88f;
    ph.mu = 2.35f;
    ph.rgrip = 1.05f;
    ph.steerlat = 45.f;
    ph.keyrise = 5.f;
    ph.keyret = 10.f;
    ph.corner = 12.f;
    ph.lowspd = 2.0f;
    ph.loadmax = 6500.f;
    ph.steer = 0.5f;
    ph.steerfall = 30.f;
    ph.steerrate = 3.f;
    ph.kf = 26000.f;
    ph.kr = 24000.f;
    ph.ccomp = 2600.f;
    ph.creb = 3600.f;
    ph.kbump = 250000.f;
    ph.sroll = 1100.f;
    ph.syaw = 2000.f;
    ph.spitch = 1000.f;
    ph.air = 7000.f;
    ph.leanr = 0.006f;
    ph.leanp = 0.004f;
    ph.shw = 1.5f;           /* invisible shoulder past the deck edge: wheels still find ground up to shw out, */
    ph.shdrop = 0.25f;       /* shdrop lower at the edge and falling 1:2 outwards, like the original */
    ph.vtop = 200.f / 3.6f;  /* engine cut-off: 200 km/h, */
    ph.vtopb = 250.f / 3.6f; /* 250 km/h on boost (the boost adds pull, the drag alone would allow 107 m/s) */
}
