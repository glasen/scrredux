/* audio/mixer.h - sample mixer for the original Amiga sounds */
#pragma once

/* the original Amiga samples (the WAVs in sounds/, from StuntCarRacer-X) through a small mixer.
 * One mixer.voices per sample, like the 1998 PC port: a sound that is still playing is not restarted, only its pitch and
 * volume follow. Pitch is given as an Amiga period (rate = 3546895 / period), volume as 0..64, both straight from
 * the original's tables. The Amiga panned engine and Smash hard left and the rest hard right; here only 30% apart.
 * When the triggers fire is decided in audio/sound_events.h. */

enum {
    S_CHIME,
    S_WRECK,
    S_HITCAR,
    S_GROUNDED,
    S_CREAK,
    S_SMASH,
    S_OFFROAD,
    S_ENGINE,
    NSND = S_ENGINE + 7
}; /* S_ENGINE+k: TickOver decimated by 2^k */

static const char *snd_file[NSND] = {"Chime",        "Wreck",        "HitCar",       "Grounded",     "Creak",
                                     "Smash",        "OffRoad",      "TickOver",     "EnginePitch2", "EnginePitch3",
                                     "EnginePitch4", "EnginePitch5", "EnginePitch6", "EnginePitch7"};
static const signed char snd_side[S_ENGINE + 1] = {1, 1,  1, 1,
                                                   1, -1, 1, -1}; /* Amiga channels 0 and 3 left, 1 and 2 right */

typedef struct {
    float *d;
    int n;
} Smp;

typedef struct {
    const Smp *s;
    double pos, step;
    float gl, gr;
    int on, loop;
} Voice;

static struct {
    Smp samples[NSND];
    Voice voices[S_ENGINE + 1]; /* one per effect, the last one is the engine */
    SDL_AudioStream *stream;
    int play_count[S_ENGINE + 1]; /* how often each effect started, for --sim's report */
    int log_plays;                /* SCR_SND set: print every start; -1 until the environment was read */
    int engine_running;
    float volume[2]; /* effects and engine, from the Options menu */
    float *dump;     /* --audio-dump: the mix, rendered without a device */
    int dump_frames, dump_capacity;
} mixer = {.log_plays = -1, .volume = {1, 1}};

static void snd_lock(void)
{
    if (mixer.stream) {
        SDL_LockAudioStream(mixer.stream);
    }
}

static void snd_unlock(void)
{
    if (mixer.stream) {
        SDL_UnlockAudioStream(mixer.stream);
    }
}

static void snd_load(void)
{
    const char *base = SDL_GetBasePath();
    int missing = 0;

    for (int i = 0; i < NSND; i++) {
        char path[1024];
        SDL_AudioSpec sp;
        Uint8 *buf = NULL, *cv = NULL;
        Uint32 len = 0;
        int cl = 0;
        snprintf(path, sizeof path, "%ssounds/%s.wav", base ? base : "", snd_file[i]);

        if (!SDL_LoadWAV(path, &sp, &buf, &len)) {
            snprintf(path, sizeof path, "sounds/%s.wav", snd_file[i]);

            if (!SDL_LoadWAV(path, &sp, &buf, &len)) {
                missing++;
                continue;
            }
        }

        SDL_AudioSpec fs = {SDL_AUDIO_F32, 1, sp.freq};

        if (SDL_ConvertAudioSamples(&sp, buf, (int)len, &fs, &cv, &cl) && cl >= 4) {
            mixer.samples[i].d = (float *)cv;
            mixer.samples[i].n = cl / 4;
        } else {
            missing++;
        }
        SDL_free(buf);
    }

    if (missing) {
        fprintf(stderr, "sounds/: %d of %d samples missing, playing without them\n", missing, NSND);
    }
}

static void snd_start(int v, int id, int period, int vol, int loop)
{
    Voice *o = &mixer.voices[v];
    o->s = &mixer.samples[id];
    o->loop = loop;
    o->step = PAULA_HZ / (float)period / SND_RATE;
    float a = vol / 64.f;
    o->gl = a * 0.5f * (1 - SND_SEP * snd_side[v]);
    o->gr = a * 0.5f * (1 + SND_SEP * snd_side[v]);
}

/* a sample effect: started if it is idle, otherwise it keeps playing at the new pitch and volume */
static void snd_play(int id, int period, int vol)
{
    snd_lock();
    Voice *o = &mixer.voices[id];
    int start = !o->on;
    snd_start(id, id, period, vol, 0);

    if (start) {
        o->pos = 0;
        o->on = mixer.samples[id].n > 0;
    }

    snd_unlock();

    if (start) {
        mixer.play_count[id]++;

        if (mixer.log_plays < 0) {
            mixer.log_plays = getenv("SCR_SND") != NULL;
        }

        if (mixer.log_plays) {
            fprintf(stderr, "snd %-8s period %3d vol %2d\n", snd_file[id], period, vol);
        }
    }
}

/* the engine loop: sample S_ENGINE+k at an Amiga period; on a change of sample the play position is scaled, as the original does */
static void snd_engine(int k, int period)
{
    snd_lock();
    Voice *o = &mixer.voices[S_ENGINE];
    const Smp *s = &mixer.samples[S_ENGINE + k];

    if (o->on && o->s != s) {
        o->pos = ldexp(o->pos, (int)(o->s - s));

        if (s->n > 0) {
            o->pos = fmod(o->pos, s->n);
        }
    }

    if (!o->on) {
        o->pos = 0;
    }

    if (!mixer.engine_running) {
        mixer.engine_running = 1;
        mixer.play_count[S_ENGINE]++;
    }

    if (mixer.log_plays < 0) {
        mixer.log_plays = getenv("SCR_SND") != NULL;
    }

    if (mixer.log_plays && o->s != s) {
        fprintf(stderr, "snd engine   %s period %3d (TickOver at %.0f Hz)\n", snd_file[S_ENGINE + k], period,
                PAULA_HZ / period * (1 << k));
    }

    snd_start(S_ENGINE, S_ENGINE + k, period, 48, 1);
    o->on = s->n > 0;
    snd_unlock();
}

static void snd_report(void)
{
    printf("SOUNDS");

    for (int v = 0; v <= S_ENGINE; v++) {
        printf(" %s=%d", snd_file[v], mixer.play_count[v]);
    }

    printf("\n");
}

static void snd_engine_stop(void)
{
    snd_lock();
    mixer.voices[S_ENGINE].on = 0;
    snd_unlock();
    mixer.engine_running = 0;
}

static void snd_stop_all(void)
{
    snd_lock();

    for (int v = 0; v <= S_ENGINE; v++) {
        mixer.voices[v].on = 0;
    }

    snd_unlock();
    mixer.engine_running = 0;
}

static void snd_mix(float *out, int frames)
{
    static float lpl, lpr;
    const float lpk =
        1.f - expf(-2.f * 3.14159265f * 4500.f / SND_RATE); /* the Amiga's fixed output RC filter (~4.5 kHz) */
    for (int f = 0; f < frames; f++) {
        float l = 0, r = 0;

        for (int v = 0; v <= S_ENGINE; v++) {
            Voice *o = &mixer.voices[v];

            if (!o->on) {
                continue;
            }

            int i = (int)o->pos, n = o->s->n;
            float t = (float)(o->pos - i);
            int j = i + 1 < n ? i + 1 : (o->loop ? 0 : i);
            float x = o->s->d[i] + (o->s->d[j] - o->s->d[i]) * t;
            x *= mixer.volume[v == S_ENGINE];
            l += x * o->gl;
            r += x * o->gr;
            o->pos += o->step;

            if (o->pos >= n) {
                if (o->loop) {
                    o->pos = fmod(o->pos, n);
                } else {
                    o->on = 0;
                }
            }
        }
        lpl += (l - lpl) * lpk;
        lpr += (r - lpr) * lpk;
        out[2 * f] = tanhf(lpl);
        out[2 * f + 1] = tanhf(lpr); /* soft limit: only pile-ups (smash + scrape + engine) reach it */
    }
}

static void SDLCALL snd_cb(void *ud, SDL_AudioStream *st, int add, int total)
{
    (void)ud;
    (void)total;
    float buf[2 * 512];

    for (int n = add / 8; n > 0;) {
        int m = n > 512 ? 512 : n;
        snd_mix(buf, m);
        SDL_PutAudioStreamData(st, buf, m * 8);
        n -= m;
    }
}

static void snd_open(void)
{
    snd_load();
    SDL_AudioSpec spec = {SDL_AUDIO_F32, 2, SND_RATE};
    mixer.stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, snd_cb, NULL);

    if (mixer.stream) {
        SDL_ResumeAudioStreamDevice(mixer.stream);
    } else {
        fprintf(stderr, "audio: %s\n", SDL_GetError());
    }
}

/* --audio-dump: render dt seconds of the mix into memory, then write it as a 16-bit stereo WAV */
static void snd_dump_begin(void)
{
    mixer.dump_capacity = SND_RATE * 60;
    mixer.dump = malloc((size_t)mixer.dump_capacity * 2 * sizeof(float));
}

static void snd_dump_frames(float dt)
{
    int n = (int)(dt * SND_RATE + 0.5f);

    if (mixer.dump_frames + n > mixer.dump_capacity) {
        mixer.dump_capacity = (mixer.dump_frames + n) * 2;
        mixer.dump = realloc(mixer.dump, (size_t)mixer.dump_capacity * 2 * sizeof(float));
    }

    snd_mix(mixer.dump + 2 * mixer.dump_frames, n);
    mixer.dump_frames += n;
}

static void snd_dump_write(const char *path)
{
    FILE *f = fopen(path, "wb");

    if (!f) {
        fprintf(stderr, "%s: cannot write\n", path);
        return;
    }

    uint32_t data = (uint32_t)mixer.dump_frames * 4, u32;
    uint16_t u16;
    fwrite("RIFF", 1, 4, f);
    u32 = 36 + data;
    fwrite(&u32, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f);
    u32 = 16;
    fwrite(&u32, 4, 1, f);
    u16 = 1;
    fwrite(&u16, 2, 1, f);
    u16 = 2;
    fwrite(&u16, 2, 1, f);
    u32 = SND_RATE;
    fwrite(&u32, 4, 1, f);
    u32 = SND_RATE * 4;
    fwrite(&u32, 4, 1, f);
    u16 = 4;
    fwrite(&u16, 2, 1, f);
    u16 = 16;
    fwrite(&u16, 2, 1, f);
    fwrite("data", 1, 4, f);
    fwrite(&data, 4, 1, f);
    float peak = 0;
    int clip = 0;

    for (int i = 0; i < 2 * mixer.dump_frames; i++) {
        float x = mixer.dump[i];
        peak = fmaxf(peak, fabsf(x));

        if (fabsf(x) >= 1.f) {
            clip++;
        }

        int16_t s = (int16_t)(x * 32767.f);
        fwrite(&s, 2, 1, f);
    }

    fclose(f);
    printf("AUDIO %s: %.1f s, peak %.2f, %d clipped samples\n", path, (float)mixer.dump_frames / SND_RATE, peak, clip);
}
