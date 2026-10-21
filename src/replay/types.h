/* replay/types.h - replay types and recording */
#pragma once

/* replay.h - the last five season races, recorded at 60 Hz (both cars' pose, suspension, damage and the crane) into
 * save/replayN.rpl and played back with cockpit, chase and trackside cameras.  */
typedef struct {
    float x[3], F[3], U[3], v[3], cx[3], wl[4], wph[4];
    float delta, lr, lp, flame, damage, boost, crash, crane, hook, thr;
    short crane_seg, seg; signed char lap; unsigned char nholes, active, nwc;
} RCar;
typedef struct { float t; int st; RCar c[2]; } RTick;
typedef struct {
    uint32_t magic, ver, seq, nt;
    char title[64], names[2][16];
    int result, laps;
    uint32_t col[2];
    float spare[2][6];                      /* keeps the file layout */
    int64_t when;
    TrackDef td;
} RHead;
enum { RC_COCKPIT, RC_CHASE, RC_TV, RC_OPP_COCKPIT, RC_OPP_CHASE, NRC };
static const char *const RC_NAME[NRC] = {"COCKPIT", "CHASE", "TV", "OPPONENT COCKPIT", "OPPONENT CHASE"};
static RTick *rec; static int nrec, caprec, rec_on, rec_k;
static RHead rp_head; static RTick *rpb; static int nrp, rp_cam, rp_paused;
static float rp_t, rp_speed = 1, rp_smoke[2];

static void rec_begin(void) { nrec = 0; rec_k = 0; rec_on = 1; }
static void rec_pack(const Car *c, RCar *r)
{
    memcpy(r->x, &c->x, 12); memcpy(r->F, &c->F, 12); memcpy(r->U, &c->U, 12); memcpy(r->v, &c->v, 12); memcpy(r->cx, &c->crane_x, 12);
    memcpy(r->wl, c->wl, 16); memcpy(r->wph, c->wph, 16);
    r->delta = c->delta; r->lr = c->lr; r->lp = c->lp; r->flame = c->flame; r->damage = c->damage; r->boost = c->boost;
    r->crash = c->crash; r->crane = c->crane; r->hook = c->hook; r->thr = c->thr;
    r->crane_seg = (short)c->crane_seg; r->seg = (short)c->seg; r->lap = (signed char)c->lap;
    r->nholes = (unsigned char)c->nholes; r->active = (unsigned char)c->active; r->nwc = (unsigned char)c->nwc;
}
static void rec_unpack(Car *c, const RCar *r)
{
    memcpy(&c->x, r->x, 12); memcpy(&c->F, r->F, 12); memcpy(&c->U, r->U, 12); memcpy(&c->v, r->v, 12); memcpy(&c->crane_x, r->cx, 12);
    memcpy(c->wl, r->wl, 16); memcpy(c->wph, r->wph, 16);
    c->R = vcross(c->F, c->U);
    c->delta = r->delta; c->lr = r->lr; c->lp = r->lp; c->flame = r->flame; c->damage = r->damage; c->boost = r->boost;
    c->crash = r->crash; c->crane = r->crane; c->hook = r->hook; c->thr = r->thr;
    c->crane_seg = r->crane_seg; c->seg = r->seg; c->lap = r->lap; c->nholes = r->nholes; c->active = r->active; c->nwc = r->nwc;
}
/* every 4th physics tick of a race, and for a few seconds after it */
static void rec_tick(void)
{
    if (!rec_on || (rec_k++ & 3) || (state == ST_DONE && state_t > 5.f) || nrec >= RP_HZ * 60 * 30) return;
    if (nrec == caprec) { caprec = caprec ? caprec * 2 : 8192; rec = realloc(rec, (size_t)caprec * sizeof *rec); }
    RTick *t = &rec[nrec++];
    t->t = racetime; t->st = state;
    for (int i = 0; i < 2; i++) rec_pack(&cars[i], &t->c[i]);
}
