/* settings/options.h - volume, fullscreen and rumble settings (options.txt) */
#pragma once

/* options.h - the Options menu's settings: engine and effects volume, fullscreen and gamepad rumble, kept in
 * save/options.txt.  */
typedef struct {
    int eng, fx, full, rumble;
} Opt; /* volumes in percent */

static Opt opt = {100, 100, 0, 1};

static void opt_apply(void)
{
    snd_vol[0] = opt.fx / 100.f;
    snd_vol[1] = opt.eng / 100.f;
}

static void opt_save(void)
{
    char p[1024];
    save_path(p, sizeof p, "options.txt", 0);
    FILE *f = fopen(p, "w");
    if (!f) {
        return;
    }
    fprintf(f, "engine|%d\neffects|%d\nfullscreen|%d\nrumble|%d\n", opt.eng, opt.fx, opt.full, opt.rumble);
    fclose(f);
}

static void opt_load(void)
{
    char p[1024], line[128];
    save_path(p, sizeof p, "options.txt", 0);
    FILE *f = fopen(p, "r");
    while (f && fgets(line, sizeof line, f)) {
        char *v = strchr(line, '|');
        if (!v) {
            continue;
        }
        *v++ = 0;
        int n = atoi(v);
        if (!strcmp(line, "engine")) {
            opt.eng = (int)clampf(n, 0, 100);
        } else if (!strcmp(line, "effects")) {
            opt.fx = (int)clampf(n, 0, 100);
        } else if (!strcmp(line, "fullscreen")) {
            opt.full = n != 0;
        } else if (!strcmp(line, "rumble")) {
            opt.rumble = n != 0;
        }
    }
    if (f) {
        fclose(f);
    }
    opt_apply();
}
