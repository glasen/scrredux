/* input/gamepads.h - gamepads and joysticks, rumble */
#pragma once

/* ---- gamepads, and joysticks SDL has no gamepad mapping for (a stick or a hat and fire buttons) ---- */
static SDL_Gamepad *gpad[NPAD];
static SDL_Joystick *gjoy[NPAD];
static unsigned char gcen[NPAD]; /* the stick axes (bit 0 x, bit 1 y) seen at rest in the middle */

static void pad_added(SDL_JoystickID id)
{
    for (int i = 0; i < NPAD; i++) {
        if (gpad[i] || gjoy[i]) {
            continue;
        }

        gcen[i] = 0;

        if (SDL_IsGamepad(id)) {
            gpad[i] = SDL_OpenGamepad(id);
        } else {
            gjoy[i] = SDL_OpenJoystick(id);
        }
        return;
    }
}

static void pad_removed(SDL_JoystickID id)
{
    for (int i = 0; i < NPAD; i++) {
        if (gpad[i] && SDL_GetGamepadID(gpad[i]) == id) {
            SDL_CloseGamepad(gpad[i]);
            gpad[i] = NULL;
        }

        if (gjoy[i] && SDL_GetJoystickID(gjoy[i]) == id) {
            SDL_CloseJoystick(gjoy[i]);
            gjoy[i] = NULL;
        }
    }
}

static const char *pad_name(void)
{
    for (int i = 0; i < NPAD; i++) {
        const char *s = gpad[i] ? SDL_GetGamepadName(gpad[i]) : gjoy[i] ? SDL_GetJoystickName(gjoy[i]) : NULL;

        if (gpad[i] || gjoy[i]) {
            return s ? s : "Unnamed controller";
        }
    }

    return NULL;
}

static float pad_axis(Sint16 v)
{
    return clampf(v / 32767.f, -1, 1);
}

static float pad_dz(float v)
{
    float a = fabsf(v);
    return a < 0.2f ? 0 : copysignf((a - 0.2f) / 0.8f, v);
} /* dead zone round the middle */

/* a stick axis counts only once it has been in the middle: some keyboards and LED controllers are joysticks too,
 * with an axis that sits at one end for good and would hold the car on full lock and push the menus along */
static float pad_stick(int i, int a, Sint16 v)
{
    float f = pad_axis(v);

    if (fabsf(f) < 0.2f) {
        gcen[i] |= 1 << a;
    }

    return gcen[i] & (1 << a) ? pad_dz(f) : 0;
}

/* everything held on every pad: steer -1..1, throttle and brake 0..1, and the four directions for the menus */
typedef struct {
    float steer, thr, brk;
    int bst, up, dn, lf, rt;
} PadIn;

static PadIn pad_read(void)
{
    PadIn r = {0};

    for (int i = 0; i < NPAD; i++) {
        float sx, sy, steer, gas, brk;
        int u, d, l, rr, bst;

        if (gpad[i]) { /* stick or D-pad steer, RT/A/up accelerate, LT/X/down brake, RB/B boost */
            SDL_Gamepad *g = gpad[i];
            sx = pad_stick(i, 0, SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTX));
            sy = pad_stick(i, 1, SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFTY));
            u = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_UP);
            d = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
            l = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
            rr = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
            steer = l != rr ? (rr ? 1.f : -1.f) : sx;
            gas = fmaxf(pad_dz(pad_axis(SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER))),
                        u || SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_SOUTH));
            brk = fmaxf(pad_dz(pad_axis(SDL_GetGamepadAxis(g, SDL_GAMEPAD_AXIS_LEFT_TRIGGER))),
                        d || SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_WEST));
            bst = SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER) ||
                  SDL_GetGamepadButton(g, SDL_GAMEPAD_BUTTON_EAST);
        } else if (gjoy[i]) { /* stick or hat: left/right steer, forward accelerates, back brakes; fire boosts */
            SDL_Joystick *j = gjoy[i];
            int na = SDL_GetNumJoystickAxes(j);
            Uint8 h = SDL_GetNumJoystickHats(j) > 0 ? SDL_GetJoystickHat(j, 0) : 0;
            sx = na > 0 ? pad_stick(i, 0, SDL_GetJoystickAxis(j, 0)) : 0;
            sy = na > 1 ? pad_stick(i, 1, SDL_GetJoystickAxis(j, 1)) : 0;
            u = !!(h & SDL_HAT_UP);
            d = !!(h & SDL_HAT_DOWN);
            l = !!(h & SDL_HAT_LEFT);
            rr = !!(h & SDL_HAT_RIGHT);
            steer = l != rr ? (rr ? 1.f : -1.f) : sx;
            gas = fmaxf(u, -sy);
            brk = fmaxf(d, sy);
            bst = SDL_GetNumJoystickButtons(j) > 0 && SDL_GetJoystickButton(j, 0);
        } else {
            continue;
        }

        if (fabsf(steer) > fabsf(r.steer)) {
            r.steer = steer;
        }

        r.thr = fmaxf(r.thr, gas);
        r.brk = fmaxf(r.brk, brk);
        r.bst |= bst;
        r.up |= u || sy < -0.5f;
        r.dn |= d || sy > 0.5f;
        r.lf |= l || sx < -0.5f;
        r.rt |= rr || sx > 0.5f;
    }

    return r;
}

/* a shake on every pad (Options: gamepad rumble): strength 0..1, for ms milliseconds */
static void pad_rumble(float strength, int ms)
{
    if (!opt.rumble || strength <= 0) {
        return;
    }

    Uint16 lo = (Uint16)(clampf(strength, 0, 1) * 0xFFFF), hi = (Uint16)(lo * 0.6f);

    for (int i = 0; i < NPAD; i++) {
        if (gpad[i]) {
            SDL_RumbleGamepad(gpad[i], lo, hi, (Uint32)ms);
        } else if (gjoy[i]) {
            SDL_RumbleJoystick(gjoy[i], lo, hi, (Uint32)ms);
        }
    }
}
