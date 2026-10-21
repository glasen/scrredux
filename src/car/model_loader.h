/* car/model_loader.h - loading models/stuntcar.scm */
#pragma once

/* the stunt buggy, loaded from models/stuntcar.scm (exported from the Blender model), and the posing of
 * its independent suspension.
 * . The game does not start without the file.
 *
 * File (little endian): "SCRCAR1\0", u32 version (1 or 2), f32 rest_h, place_h, eye[3], cpts[16][3], flames[8][3],
 * (version 2: flame directions[8][3]),
 * steering wheel centre[3], axis[3], ratio; 4 x wheel {mount[3], radius, lmin, lf, rest, bump, droop,
 * kingpin point[3], kingpin axis[3], hub[3]}; 4 x rig {lower pivots 2x[3], upper pivots 2x[3], lower/upper
 * ball joint[3], coilover lower/upper eye[3], u32 nlinks, nlinks x {inner[3], outer[3]}, u32 n, n x {dz, a, b}};
 * u32 ngroups, ngroups x {char name[8], u32 role, i32 corner, i32 link, u32 first, u32 count};
 * u32 nverts, nverts x Vtx. All in the car frame (+x right, +y up, +z forward, origin = centre of mass).
 */

/* ---- loading ---- */
typedef struct {
    const uint8_t *p, *end;
    int bad;
} Rd;

/* the next 4 bytes of the file; a read past the end sets r->bad and gives 0 */
static uint32_t rd_u32(Rd *r)
{
    uint32_t v = 0;

    if (r->p + 4 > r->end) {
        r->bad = 1;
        return 0;
    }

    memcpy(&v, r->p, 4);
    r->p += 4;
    return v;
}

static float rd_f(Rd *r)
{
    uint32_t u = rd_u32(r);
    float v;
    memcpy(&v, &u, 4);
    return v;
}

static V3 rd_v3(Rd *r)
{
    float x = rd_f(r), y = rd_f(r), z = rd_f(r);
    return V(x, y, z);
}

/* the car's geometry: ride heights, eye, collision points, flames, steering wheel and the four wheels with their
 * suspension geometry */
static void read_car_geometry(Rd *r, uint32_t version)
{
    REST_H = rd_f(r);
    PLACE_H = rd_f(r);
    EYE = rd_v3(r);

    for (int i = 0; i < 16; i++) {
        CPTS[i] = rd_v3(r);
    }

    for (int i = 0; i < 8; i++) {
        FLAMEP[i] = rd_v3(r);
    }

    if (version >= 2) {
        for (int i = 0; i < 8; i++) {
            FLAMED[i] = vnorm(rd_v3(r));
        }
    }

    STW_C = rd_v3(r);
    STW_A = rd_v3(r);
    STW_RATIO = rd_f(r);

    for (int i = 0; i < 4; i++) {
        WMOUNT[i] = rd_v3(r);
        WRAD[i] = rd_f(r);
        WLMIN[i] = rd_f(r);
        WLF[i] = rd_f(r);
        WREST[i] = rd_f(r);
        WBUMP[i] = rd_f(r);
        WDROOP[i] = rd_f(r);
        KP_P[i] = rd_v3(r);
        KP_A[i] = rd_v3(r);
        HUB0[i] = rd_v3(r);
    }
}

/* the four independent suspensions: wishbone pivots, ball joints, coilover eyes, steering links and the tabulated
 * motion; sets r->bad when a count is out of range */
static void read_rigs(Rd *r)
{
    for (int i = 0; i < 4 && !r->bad; i++) {
        Rig *g = &rig[i];
        g->lb0 = rd_v3(r);
        g->lb1 = rd_v3(r);
        g->ub0 = rd_v3(r);
        g->ub1 = rd_v3(r);
        g->L0 = rd_v3(r);
        g->U0 = rd_v3(r);
        g->elo = rd_v3(r);
        g->ehi = rd_v3(r);
        g->nlink = (int)rd_u32(r);

        if (g->nlink > 2) {
            r->bad = 1;
            break;
        }

        for (int k = 0; k < g->nlink; k++) {
            g->lin[k] = rd_v3(r);
            g->lout[k] = rd_v3(r);
        }

        g->ntab = (int)rd_u32(r);

        if (g->ntab < 2 || g->ntab > 10000) {
            r->bad = 1;
            break;
        }

        g->tab = malloc((size_t)g->ntab * 3 * sizeof(float));

        for (int k = 0; k < g->ntab * 3; k++) {
            g->tab[k] = rd_f(r);
        }
    }
}

/* the mesh groups: name, role, which wheel, link, and the range of vertices */
static void read_mesh_groups(Rd *r, uint32_t count)
{
    for (uint32_t i = 0; i < count; i++) {
        if (r->p + 8 > r->end) {
            r->bad = 1;
            break;
        }

        MGroup *g = &car_mesh.groups[i];
        memcpy(g->name, r->p, 8);
        g->name[7] = 0;
        r->p += 8;
        g->role = (int)rd_u32(r);
        g->corner = (int)rd_u32(r);
        g->link = (int)rd_u32(r);
        g->first = rd_u32(r);
        g->count = rd_u32(r);
    }
}

/* parses a whole model file; prints what is wrong and returns 0 when it is not usable */
static int parse_car_model(const uint8_t *buf, size_t size)
{
    Rd r = {buf, buf + size, 0};
    uint32_t version = 0;

    if (size < 12 || memcmp(buf, "SCRCAR1", 8) || (r.p += 8, version = rd_u32(&r)) < 1 || version > 2) {
        fprintf(stderr, "stuntcar.scm: bad header\n");
        return 0;
    }

    read_car_geometry(&r, version);
    read_rigs(&r);
    uint32_t groups = rd_u32(&r);

    if (r.bad || groups > (uint32_t)(sizeof car_mesh.groups / sizeof car_mesh.groups[0])) {
        fprintf(stderr, "stuntcar.scm: bad rig\n");
        return 0;
    }

    read_mesh_groups(&r, groups);
    uint32_t vertices = rd_u32(&r);

    if (r.bad || r.p + (size_t)vertices * sizeof(Vtx) > r.end) {
        fprintf(stderr, "stuntcar.scm: truncated\n");
        return 0;
    }

    for (uint32_t i = 0; i < groups; i++) {
        const MGroup *g = &car_mesh.groups[i];

        if (g->first + g->count > vertices || g->corner > 3 || g->link > 1) {
            fprintf(stderr, "stuntcar.scm: bad group\n");
            return 0;
        }
    }

    car_mesh.vertices = malloc((size_t)vertices * sizeof(Vtx));
    memcpy(car_mesh.vertices, r.p, (size_t)vertices * sizeof(Vtx));
    car_mesh.vertex_count = (int)vertices;
    car_mesh.group_count = (int)groups;
    return 1;
}

/* loads models/stuntcar.scm (next to the game, or in the current folder); the game does not start without it */
static int car_model_load(void)
{
    char path[1024];
    size_t size = 0;
    const char *base = SDL_GetBasePath();
    snprintf(path, sizeof path, "%smodels/stuntcar.scm", base ? base : "");
    uint8_t *buf = SDL_LoadFile(path, &size);

    if (!buf) {
        buf = SDL_LoadFile("models/stuntcar.scm", &size);
    }

    if (!buf) {
        fprintf(stderr, "models/stuntcar.scm not found\n");
        return 0;
    }

    int ok = parse_car_model(buf, size);
    SDL_free(buf);
    return ok;
}
