/* effects/particles.h - particle spawning and stepping */
#pragma once

/* ---- particles: sparks, dust, smoke, fire (world space, drawn as unlit camera-facing shapes) ---- */
enum { P_SPARK, P_DUST, P_SMOKE, P_FIRE };

typedef struct {
    V3 p, v;
    float t, life, size;
    uint32_t col;
    int kind;
} Part;

static Part parts[MAXPART];
static int npart;

static void spawn(int kind, V3 p, V3 v, float life, float size, uint32_t col)
{
    if (kind == P_SPARK) {
        stats.sparks++;
    }

    if (npart >= MAXPART) {
        return;
    }

    Part *q = &parts[npart++];
    q->p = p;
    q->v = v;
    q->t = 0;
    q->life = life;
    q->size = size;
    q->col = col;
    q->kind = kind;
}

static V3 rnd_dir(void)
{
    return vnorm(V(frand() - 0.5f, frand() - 0.5f, frand() - 0.5f));
}

static void dust_burst(V3 p, int n, float speed)
{
    for (int k = 0; k < n; k++) {
        V3 d = rnd_dir();
        d.y = fabsf(d.y) * 0.6f + 0.2f;
        spawn(P_DUST, vadd(p, vmul(d, 0.8f)), vmul(d, speed * (0.3f + frand())), 1.2f + frand() * 1.2f,
              0.25f + frand() * 0.35f, (k & 1) ? 0xA58A62 : 0x8E7552);
    }
}

static void parts_step(float dt)
{
    for (int i = 0; i < npart; i++) {
        Part *q = &parts[i];
        q->t += dt;

        if (q->t >= q->life) {
            parts[i--] = parts[--npart];
            continue;
        }

        if (q->kind == P_SPARK) {
            q->v.y -= GRAV * dt;

            if (q->p.y < ground_h(q->p.x, q->p.z)) {
                parts[i--] = parts[--npart];
                continue;
            }
        } else {
            float drag = q->kind == P_DUST ? 1.6f : 0.9f;
            q->v = vmul(q->v, expf(-drag * dt));
            q->v.y += (q->kind == P_DUST ? 0.35f : q->kind == P_SMOKE ? 1.4f : 2.5f) * dt;
            q->size += (q->kind == P_DUST ? 0.55f : q->kind == P_SMOKE ? 0.6f : -0.3f) * dt;
        }
        q->p = vadd(q->p, vmul(q->v, dt));
    }
}
