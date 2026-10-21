/* track/pieces.h - piece cross-sections and decoding the original tracks */
#pragma once

/* cross-section i of a piece in the original's x/z units (not mirrored), edge 0 and edge 1 */
static void tpl_cross(int at, int i, int cx, int cz, int out[2][2])
{
    int t = at & 15, ns = SCR_TPL_NS[t], ang = at & 0xc0;
    const short *r = SCR_TPL_XZ[t][(at & 0x10) ? ns - i : i];

    for (int e = 0; e < 2; e++) {
        int x = (at & 0x10) ? r[2 - 2 * e] : r[2 * e], z = (at & 0x10) ? r[3 - 2 * e] : r[2 * e + 1], rx = x, rz = z;

        if (ang == 0x40) {
            rx = z;
            rz = 0x800 - x;
        } else if (ang == 0x80) {
            rx = 0x800 - x;
            rz = 0x800 - z;
        } else if (ang == 0xc0) {
            rx = 0x800 - z;
            rz = x;
        }
        out[e][0] = cx * 2048 + rx;
        out[e][1] = cz * 2048 + rz;
    }
}

static int pc_ns(const TPiece *p)
{
    return SCR_TPL_NS[p->at & 15];
}

static void decode_original(int idx, TrackDef *td)
{
    const unsigned char *b = SCR_BIN[idx];
    memset(td, 0, sizeof *td);
    snprintf(td->name, sizeof td->name, "%s", SCR_TITLE[idx]);
    td->np = b[0];
    td->start = b[1];
    td->orig = idx;

    for (int p = 0; p < td->np; p++) {
        TPiece *pc = &td->p[p];
        pc->cx = b[2 + p] & 15;
        pc->cz = b[2 + p] >> 4;
        pc->at = b[102 + p];
        pc->shape = SH_RAW;
        int sh[2] = {(short)(b[402 + 2 * p] << 8 | b[403 + 2 * p]), (short)(b[602 + 2 * p] << 8 | b[603 + 2 * p])};
        int id[2] = {b[202 + p] & 0x7f, b[302 + p] & 0x7f};

        for (int i = 0; i < pc_ns(pc); i++) {
            for (int e = 0; e < 2; e++) {
                pc->h[e][i] = SCR_PROF[id[e]][i] + sh[e];
            }
        }
    }
}
