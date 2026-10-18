/* controls.h - the driving keys, two per action, changed in the Controls menu and kept in save/controls.txt; and gamepads and
 * joysticks: held sticks, D-pads, triggers and buttons drive the car, and they shake on hits. Included by scrredux.c */
static const bool *keys;                               /* the keyboard, held keys; NULL when headless */
enum { CT_ACCEL, CT_BRAKE, CT_LEFT, CT_RIGHT, CT_BOOST, NCT };
static const char *const CT_NAME[NCT] = {"Accelerate", "Brake", "Steer left", "Steer right", "Boost"};
static const char *const CT_ID[NCT] = {"accelerate", "brake", "left", "right", "boost"};
static const SDL_Scancode CT_DEF[NCT][2] = {
    {SDL_SCANCODE_UP, SDL_SCANCODE_W}, {SDL_SCANCODE_DOWN, SDL_SCANCODE_S}, {SDL_SCANCODE_LEFT, SDL_SCANCODE_A},
    {SDL_SCANCODE_RIGHT, SDL_SCANCODE_D}, {SDL_SCANCODE_SPACE, SDL_SCANCODE_LSHIFT}};
static SDL_Scancode ctl[NCT][2];

static void ctl_defaults(void) { memcpy(ctl, CT_DEF, sizeof ctl); }
static void ctl_save(void)
{
    char p[1024]; save_path(p, sizeof p, "controls.txt", 0);
    FILE *f = fopen(p, "w"); if (!f) return;
    for (int a = 0; a < NCT; a++) fprintf(f, "%s|%s|%s\n", CT_ID[a], SDL_GetScancodeName(ctl[a][0]), SDL_GetScancodeName(ctl[a][1]));
    fclose(f);
}
static void ctl_load(void)
{
    ctl_defaults();
    char p[1024], line[128]; save_path(p, sizeof p, "controls.txt", 0);
    FILE *f = fopen(p, "r"); if (!f) return;
    while (fgets(line, sizeof line, f)) {
        char *fld[3]; int n = 0;
        line[strcspn(line, "\r\n")] = 0;
        for (char *s = line; n < 3; ) { fld[n++] = s; s = strchr(s, '|'); if (!s) break; *s++ = 0; }
        if (n < 3) continue;
        for (int a = 0; a < NCT; a++) if (!strcmp(fld[0], CT_ID[a]))
            for (int k = 0; k < 2; k++) ctl[a][k] = SDL_GetScancodeFromName(fld[k + 1]);
    }
    fclose(f);
}
/* a key goes to one place only: where it was, it swaps with the key it replaces */
static void ctl_bind(int a, int k, SDL_Scancode sc)
{
    for (int b = 0; b < NCT; b++) for (int j = 0; j < 2; j++) if (ctl[b][j] == sc) ctl[b][j] = ctl[a][k];
    ctl[a][k] = sc;
    ctl_save();
}
static int ctl_down(int a) { return keys && ((ctl[a][0] && keys[ctl[a][0]]) || (ctl[a][1] && keys[ctl[a][1]])); }

/* ---- gamepads, and joysticks SDL has no gamepad mapping for (a stick or a hat and fire buttons) ---- */
#define NPAD 4
static SDL_Gamepad *gpad[NPAD];
static SDL_Joystick *gjoy[NPAD];
static void pad_added(SDL_JoystickID id)
{
    for (int i = 0; i < NPAD; i++) {
        if (gpad[i] || gjoy[i]) continue;
        if (SDL_IsGamepad(id)) gpad[i] = SDL_OpenGamepad(id); else gjoy[i] = SDL_OpenJoystick(id);
        return;
    }
}
static void pad_removed(SDL_JoystickID id)
{
    for (int i = 0; i < NPAD; i++) {
        if (gpad[i] && SDL_GetGamepadID(gpad[i]) == id) { SDL_CloseGamepad(gpad[i]); gpad[i] = NULL; }
        if (gjoy[i] && SDL_GetJoystickID(gjoy[i]) == id) { SDL_CloseJoystick(gjoy[i]); gjoy[i] = NULL; }
    }
}
static const char *pad_name(void)
{
    for (int i = 0; i < NPAD; i++) {
        const char *s = gpad[i] ? SDL_GetGamepadName(gpad[i]) : gjoy[i] ? SDL_GetJoystickName(gjoy[i]) : NULL;
        if (gpad[i] || gjoy[i]) return s ? s : "Unnamed controller";
    }
    return NULL;
}
static float pad_axis(Sint16 v) { return clampf(v / 32767.f, -1, 1); }
static float pad_dz(float v) { float a = fabsf(v); return a < 0.2f ? 0 : copysignf((a - 0.2f) / 0.8f, v); }  /* dead zone round the middle */

/* everything held on every pad: steer -1..1, throttle and brake 0..1, and the four directions for the menus */
typedef struct { float steer, thr, brk; int bst, up, dn, lf, rt; } PadIn;
static PadIn pad_read(void)
{
    PadIn r = {0};
    for (int i = 0; i < NPAD; i++) {
        float sx, sy, steer, gas, brk; int u, d, l, rr, bst;
        if (gpad[i]) {                     /* stick or D-pad steer, RT/A/up accelerate, LT/X/down brake, RB/B boost */
            SDL_Gamepad *g = gpad[i];
            sx = pad_dz(pad_axis(SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTX)));
            sy = pad_dz(pad_axis(SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTY)));
            u = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_UP); d = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
            l = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_LEFT); rr = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
            steer = l != rr ? (rr ? 1.f : -1.f) : sx;
            gas = fmaxf(pad_dz(pad_axis(SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER))), u || SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_SOUTH));
            brk = fmaxf(pad_dz(pad_axis(SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFT_TRIGGER))), d || SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_WEST));
            bst = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) || SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_EAST);
        } else if (gjoy[i]) {              /* stick or hat: left/right steer, forward accelerates, back brakes; fire boosts */
            SDL_Joystick *j = gjoy[i];
            int na = SDL_GetNumJoystickAxes(j);
            Uint8 h = SDL_GetNumJoystickHats(j) > 0 ? SDL_GetJoystickHat(j, 0) : 0;
            sx = na > 0 ? pad_dz(pad_axis(SDL_GetJoystickAxis(j, 0))) : 0;
            sy = na > 1 ? pad_dz(pad_axis(SDL_GetJoystickAxis(j, 1))) : 0;
            u = !!(h & SDL_HAT_UP); d = !!(h & SDL_HAT_DOWN); l = !!(h & SDL_HAT_LEFT); rr = !!(h & SDL_HAT_RIGHT);
            steer = l != rr ? (rr ? 1.f : -1.f) : sx;
            gas = fmaxf(u, -sy); brk = fmaxf(d, sy);
            bst = SDL_GetNumJoystickButtons(j) > 0 && SDL_GetJoystickButton(j, 0);
        } else continue;
        if (fabsf(steer) > fabsf(r.steer)) r.steer = steer;
        r.thr = fmaxf(r.thr, gas); r.brk = fmaxf(r.brk, brk); r.bst |= bst;
        r.up |= u || sy < -0.5f; r.dn |= d || sy > 0.5f; r.lf |= l || sx < -0.5f; r.rt |= rr || sx > 0.5f;
    }
    return r;
}
/* a shake on every pad (Options: gamepad rumble): strength 0..1, for ms milliseconds */
static void pad_rumble(float strength, int ms)
{
    if (!opt.rumble || strength <= 0) return;
    Uint16 lo = (Uint16)(clampf(strength, 0, 1) * 0xFFFF), hi = (Uint16)(lo * 0.6f);
    for (int i = 0; i < NPAD; i++) {
        if (gpad[i]) SDL_RumbleGamepad(gpad[i], lo, hi, (Uint32)ms);
        else if (gjoy[i]) SDL_RumbleJoystick(gjoy[i], lo, hi, (Uint32)ms);
    }
}
