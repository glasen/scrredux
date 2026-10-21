/* input/controls.h - driving key bindings */
#pragma once

/* controls.h - the driving keys, two per action, changed in the Controls menu and kept in save/controls.txt; and gamepads and
 * joysticks: held sticks, D-pads, triggers and buttons drive the car, and they shake on hits.  */
static const bool *keys; /* the keyboard, held keys; NULL when headless */

enum { CT_ACCEL, CT_BRAKE, CT_LEFT, CT_RIGHT, CT_BOOST, NCT };

static const char *const CT_NAME[NCT] = {"Accelerate", "Brake", "Steer left", "Steer right", "Boost"};
static const char *const CT_ID[NCT] = {"accelerate", "brake", "left", "right", "boost"};
static const SDL_Scancode CT_DEF[NCT][2] = {{SDL_SCANCODE_UP, SDL_SCANCODE_W},
                                            {SDL_SCANCODE_DOWN, SDL_SCANCODE_S},
                                            {SDL_SCANCODE_LEFT, SDL_SCANCODE_A},
                                            {SDL_SCANCODE_RIGHT, SDL_SCANCODE_D},
                                            {SDL_SCANCODE_SPACE, SDL_SCANCODE_LSHIFT}};
static SDL_Scancode ctl[NCT][2];

static void ctl_defaults(void)
{
    memcpy(ctl, CT_DEF, sizeof ctl);
}

static void ctl_save(void)
{
    char p[1024];
    save_path(p, sizeof p, "controls.txt", 0);
    FILE *f = fopen(p, "w");
    if (!f) {
        return;
    }
    for (int a = 0; a < NCT; a++) {
        fprintf(f, "%s|%s|%s\n", CT_ID[a], SDL_GetScancodeName(ctl[a][0]), SDL_GetScancodeName(ctl[a][1]));
    }
    fclose(f);
}

static void ctl_load(void)
{
    ctl_defaults();
    char p[1024], line[128];
    save_path(p, sizeof p, "controls.txt", 0);
    FILE *f = fopen(p, "r");
    if (!f) {
        return;
    }
    while (fgets(line, sizeof line, f)) {
        char *fld[3];
        int n = 0;
        line[strcspn(line, "\r\n")] = 0;
        for (char *s = line; n < 3;) {
            fld[n++] = s;
            s = strchr(s, '|');
            if (!s) {
                break;
            }
            *s++ = 0;
        }
        if (n < 3) {
            continue;
        }
        for (int a = 0; a < NCT; a++) {
            if (!strcmp(fld[0], CT_ID[a])) {
                for (int k = 0; k < 2; k++) {
                    ctl[a][k] = SDL_GetScancodeFromName(fld[k + 1]);
                }
            }
        }
    }
    fclose(f);
}

/* a key goes to one place only: where it was, it swaps with the key it replaces */
static void ctl_bind(int a, int k, SDL_Scancode sc)
{
    for (int b = 0; b < NCT; b++) {
        for (int j = 0; j < 2; j++) {
            if (ctl[b][j] == sc) {
                ctl[b][j] = ctl[a][k];
            }
        }
    }
    ctl[a][k] = sc;
    ctl_save();
}

static int ctl_down(int a)
{
    return keys && ((ctl[a][0] && keys[ctl[a][0]]) || (ctl[a][1] && keys[ctl[a][1]]));
}
