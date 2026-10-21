/* replay/files.h - replay files */
#pragma once

static int rp_read_head(int slot, RHead *h)
{
    char p[1024];
    save_path(p, sizeof p, "replay%d.rpl", slot);
    FILE *f = fopen(p, "rb");
    if (!f) {
        return -1;
    }
    int ok = fread(h, sizeof *h, 1, f) == 1 && h->magic == RP_MAGIC && h->ver == 1;
    fclose(f);
    return ok ? 0 : -1;
}
/* the race just driven goes over the oldest of the five */
static void rec_save(const char *title, const char *n0, const char *n1, int result)
{
    rec_on = 0;
    if (nrec < RP_HZ * 5) {
        return;
    }
    int slot = 1;
    uint32_t seq = 0, oldest = 0xffffffffu;
    for (int k = 1; k <= RP_SLOTS; k++) {
        RHead h;
        if (rp_read_head(k, &h)) {
            if (oldest) {
                oldest = 0;
                slot = k;
            }
            continue;
        }
        if (h.seq > seq) {
            seq = h.seq;
        }
        if (h.seq < oldest) {
            oldest = h.seq;
            slot = k;
        }
    }
    RHead h;
    memset(&h, 0, sizeof h);
    h.magic = RP_MAGIC;
    h.ver = 1;
    h.seq = seq + 1;
    h.nt = (uint32_t)nrec;
    h.result = result;
    h.laps = laps;
    snprintf(h.title, sizeof h.title, "%s", title);
    snprintf(h.names[0], sizeof h.names[0], "%s", n0);
    snprintf(h.names[1], sizeof h.names[1], "%s", n1);
    for (int i = 0; i < 2; i++) {
        h.col[i] = cars[i].col;
    }
    h.when = (int64_t)time(NULL);
    h.td = trk[cur_track];
    char p[1024];
    save_path(p, sizeof p, "replay%d.rpl", slot);
    FILE *f = fopen(p, "wb");
    if (!f) {
        return;
    }
    fwrite(&h, sizeof h, 1, f);
    fwrite(rec, sizeof *rec, (size_t)nrec, f);
    fclose(f);
}
static int rp_load(int slot)
{
    char p[1024];
    save_path(p, sizeof p, "replay%d.rpl", slot);
    FILE *f = fopen(p, "rb");
    if (!f) {
        return -1;
    }
    int ok =
        fread(&rp_head, sizeof rp_head, 1, f) == 1 && rp_head.magic == RP_MAGIC && rp_head.ver == 1 && rp_head.nt > 0;
    if (ok) {
        free(rpb);
        rpb = malloc((size_t)rp_head.nt * sizeof *rpb);
        ok = rpb && fread(rpb, sizeof *rpb, rp_head.nt, f) == rp_head.nt;
        nrp = ok ? (int)rp_head.nt : 0;
    }
    fclose(f);
    return ok ? 0 : -1;
}
