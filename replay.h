/* replay.h - the last five season races, recorded at 60 Hz (both cars' pose, suspension, damage and the crane) into
 * save/replayN.rpl and played back with cockpit, chase and trackside cameras. Included by scrredux.c */
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
#define RP_MAGIC 0x52504c59u
#define RP_SLOTS 5
#define RP_HZ 60
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
static int rp_read_head(int slot, RHead *h)
{
    char p[1024]; save_path(p, sizeof p, "replay%d.rpl", slot);
    FILE *f = fopen(p, "rb"); if (!f) return -1;
    int ok = fread(h, sizeof *h, 1, f) == 1 && h->magic == RP_MAGIC && h->ver == 1;
    fclose(f);
    return ok ? 0 : -1;
}
/* the race just driven goes over the oldest of the five */
static void rec_save(const char *title, const char *n0, const char *n1, int result)
{
    rec_on = 0;
    if (nrec < RP_HZ * 5) return;
    int slot = 1; uint32_t seq = 0, oldest = 0xffffffffu;
    for (int k = 1; k <= RP_SLOTS; k++) {
        RHead h;
        if (rp_read_head(k, &h)) { if (oldest) { oldest = 0; slot = k; } continue; }
        if (h.seq > seq) seq = h.seq;
        if (h.seq < oldest) { oldest = h.seq; slot = k; }
    }
    RHead h; memset(&h, 0, sizeof h);
    h.magic = RP_MAGIC; h.ver = 1; h.seq = seq + 1; h.nt = (uint32_t)nrec; h.result = result; h.laps = laps;
    snprintf(h.title, sizeof h.title, "%s", title);
    snprintf(h.names[0], sizeof h.names[0], "%s", n0); snprintf(h.names[1], sizeof h.names[1], "%s", n1);
    for (int i = 0; i < 2; i++) h.col[i] = cars[i].col;
    h.when = (int64_t)time(NULL);
    h.td = trk[cur_track];
    char p[1024]; save_path(p, sizeof p, "replay%d.rpl", slot);
    FILE *f = fopen(p, "wb"); if (!f) return;
    fwrite(&h, sizeof h, 1, f); fwrite(rec, sizeof *rec, (size_t)nrec, f);
    fclose(f);
}
static int rp_load(int slot)
{
    char p[1024]; save_path(p, sizeof p, "replay%d.rpl", slot);
    FILE *f = fopen(p, "rb"); if (!f) return -1;
    int ok = fread(&rp_head, sizeof rp_head, 1, f) == 1 && rp_head.magic == RP_MAGIC && rp_head.ver == 1 && rp_head.nt > 0;
    if (ok) {
        free(rpb); rpb = malloc((size_t)rp_head.nt * sizeof *rpb);
        ok = rpb && fread(rpb, sizeof *rpb, rp_head.nt, f) == rp_head.nt;
        nrp = ok ? (int)rp_head.nt : 0;
    }
    fclose(f);
    return ok ? 0 : -1;
}
