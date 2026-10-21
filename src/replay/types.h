/* replay/types.h - replay types and recording */
#pragma once

/* replay.h - the last five season races, recorded at 60 Hz (both cars' pose, suspension, damage and the crane) into
 * save/replayN.rpl and played back with cockpit, chase and trackside cameras.  */
typedef struct {
    float x[3], F[3], U[3], v[3], cx[3], suspension_len[4], wheel_phase[4];
    float steer_angle, lean_roll, lean_pitch, flame, damage, boost, crash_time, crane_time, hook_time, throttle;
    short crane_segment, seg;
    signed char lap;
    unsigned char hole_count, active, wheels_touching;
} RCar;

typedef struct {
    float t;
    int st;
    RCar c[2];
} RTick;

typedef struct {
    uint32_t magic, ver, seq, nt;
    char title[64], names[2][16];
    int result, laps;
    uint32_t col[2];
    float spare[2][6]; /* keeps the file layout */
    int64_t when;
    TrackDef td;
} RHead;

enum { RC_COCKPIT, RC_CHASE, RC_TV, RC_OPP_COCKPIT, RC_OPP_CHASE, NRC };

static const char *const RC_NAME[NRC] = {"COCKPIT", "CHASE", "TV", "OPPONENT COCKPIT", "OPPONENT CHASE"};
static RTick *rec;
static int nrec, caprec, rec_on, rec_k;
static RHead rp_head;
static RTick *rpb;
static int nrp, rp_cam, rp_paused;
static float rp_t, rp_speed = 1, rp_smoke[2];

static void rec_begin(void)
{
    nrec = 0;
    rec_k = 0;
    rec_on = 1;
}

static void rec_pack(const Car *c, RCar *r)
{
    memcpy(r->x, &c->x, 12);
    memcpy(r->F, &c->F, 12);
    memcpy(r->U, &c->U, 12);
    memcpy(r->v, &c->v, 12);
    memcpy(r->cx, &c->crane_target, 12);
    memcpy(r->suspension_len, c->suspension_len, 16);
    memcpy(r->wheel_phase, c->wheel_phase, 16);
    r->steer_angle = c->steer_angle;
    r->lean_roll = c->lean_roll;
    r->lean_pitch = c->lean_pitch;
    r->flame = c->flame;
    r->damage = c->damage;
    r->boost = c->boost;
    r->crash_time = c->crash_time;
    r->crane_time = c->crane_time;
    r->hook_time = c->hook_time;
    r->throttle = c->throttle;
    r->crane_segment = (short)c->crane_segment;
    r->seg = (short)c->seg;
    r->lap = (signed char)c->lap;
    r->hole_count = (unsigned char)c->hole_count;
    r->active = (unsigned char)c->active;
    r->wheels_touching = (unsigned char)c->wheels_touching;
}

static void rec_unpack(Car *c, const RCar *r)
{
    memcpy(&c->x, r->x, 12);
    memcpy(&c->F, r->F, 12);
    memcpy(&c->U, r->U, 12);
    memcpy(&c->v, r->v, 12);
    memcpy(&c->crane_target, r->cx, 12);
    memcpy(c->suspension_len, r->suspension_len, 16);
    memcpy(c->wheel_phase, r->wheel_phase, 16);
    c->R = vcross(c->F, c->U);
    c->steer_angle = r->steer_angle;
    c->lean_roll = r->lean_roll;
    c->lean_pitch = r->lean_pitch;
    c->flame = r->flame;
    c->damage = r->damage;
    c->boost = r->boost;
    c->crash_time = r->crash_time;
    c->crane_time = r->crane_time;
    c->hook_time = r->hook_time;
    c->throttle = r->throttle;
    c->crane_segment = r->crane_segment;
    c->seg = r->seg;
    c->lap = r->lap;
    c->hole_count = r->hole_count;
    c->active = r->active;
    c->wheels_touching = r->wheels_touching;
}

/* every 4th physics tick of a race, and for a few seconds after it */
static void rec_tick(void)
{
    if (!rec_on || (rec_k++ & 3) || (state == ST_DONE && state_t > 5.f) || nrec >= RP_HZ * 60 * 30) {
        return;
    }

    if (nrec == caprec) {
        caprec = caprec ? caprec * 2 : 8192;
        rec = realloc(rec, (size_t)caprec * sizeof *rec);
    }

    RTick *t = &rec[nrec++];
    t->t = racetime;
    t->st = state;

    for (int i = 0; i < 2; i++) {
        rec_pack(&cars[i], &t->c[i]);
    }
}
