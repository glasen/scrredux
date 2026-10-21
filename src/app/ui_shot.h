/* app/ui_shot.h - scripted menu screenshots (--ui-shot) */
#pragma once

/* debug: drive the menus and the builder headless with a script of key names, then render one frame:
 * "Down Down Return wait2 text:Bob" (wait N runs N seconds of the game, text: types into a name field) */
/* waitS: lets the game run for S seconds, a frame at a time */
static void ui_shot_wait(float seconds)
{
    int frames = (int)(seconds * 60);

    for (int f = 0; f < frames; f++) {
        for (int k = 0; k < 4; k++) {
            if (!paused) {
                game_tick(PHYS_DT);
            }
        }

        frame_timers(1.f / 60);
        update_camera(&cars[cam_car()], 1.f / 60.f, 0);
    }
}

/* strips "shift+", "ctrl+" and "alt+" prefixes, in any order, from a key token and returns them as modifiers */
static SDL_Keymod ui_shot_modifiers(char **token)
{
    SDL_Keymod mod = 0;

    for (int more = 1; more;) {
        more = 0;

        if (!strncmp(*token, "shift+", 6)) {
            mod |= SDL_KMOD_SHIFT;
            *token += 6;
            more = 1;
        }

        if (!strncmp(*token, "ctrl+", 5)) {
            mod |= SDL_KMOD_CTRL;
            *token += 5;
            more = 1;
        }

        if (!strncmp(*token, "alt+", 4)) {
            mod |= SDL_KMOD_ALT;
            *token += 4;
            more = 1;
        }
    }

    return mod;
}

/* click:X,Y[,B] - a mouse click in the HUD's units (B 1 left, 3 right) */
static void ui_shot_click(const char *args, SDL_Keymod mod)
{
    float x = 0, y = 0;
    int button = 1;
    sscanf(args, "%f,%f,%d", &x, &y, &button);

    if (state == ST_EDIT) {
        ed_cam(0);
        SDL_SetModState(mod);
        ed_button(x, y, button, 1);
        ed_button(x, y, button, 0);
        SDL_SetModState(0);
    }
}

/* fly:X,Y,Z,YAW,PITCH - put the builder's camera there (degrees) */
static void ui_shot_fly(const char *args)
{
    float v[5] = {0};
    sscanf(args, "%f,%f,%f,%f,%f", &v[0], &v[1], &v[2], &v[3], &v[4]);

    if (state == ST_EDIT) {
        ed_fly_to(v);
    }
}

/* one word of the script: wait, text, click, fly, or a key name with optional modifiers. Returns 0 to stop */
static int ui_shot_token(char *token)
{
    if (!strncmp(token, "wait", 4)) {
        ui_shot_wait((float)atof(token + 4));
        return 1;
    }

    if (!strncmp(token, "text:", 5)) {
        if (state == ST_MENU) {
            menu_text(token + 5);
        } else {
            ed_text(token + 5);
        }

        return 1;
    }

    SDL_Keymod mod = ui_shot_modifiers(&token);

    if (!strncmp(token, "click:", 6)) {
        ui_shot_click(token + 6, mod);
        return 1;
    }

    if (!strncmp(token, "fly:", 4)) {
        ui_shot_fly(token + 4);
        return 1;
    }

    SDL_Keycode k = SDL_GetKeyFromName(token);

    if (k == SDLK_UNKNOWN) {
        fprintf(stderr, "ui-shot: unknown key %s\n", token);
        return 1;
    }

    if (!handle_key(k, mod)) {
        return 0;
    }

    update_camera(&cars[cam_car()], 1.f / 60.f, 1);
    return 1;
}

/* --ui-shot FILE SCRIPT: plays the words of SCRIPT as input from the main menu, then writes a screenshot. SCR_AUTO lets
 * the autopilot drive the player's races, SCR_OVERVIEW views the whole track from high up */
static void ui_shot(const char *path, const char *script)
{
    if (!SDL_Init(SDL_INIT_VIDEO) || gpu_init() != 0 || ensure_targets(1920, 1080) != 0) {
        return;
    }

    setup_menu();
    update_camera(&cars[0], 1.f / 60.f, 1);
    autoplay = getenv("SCR_AUTO") != NULL;
    char buf[2048];
    snprintf(buf, sizeof buf, "%s", script);

    for (char *token = strtok(buf, " "); token; token = strtok(NULL, " ")) {
        if (!ui_shot_token(token)) {
            break;
        }
    }

    if (getenv("SCR_OVERVIEW")) { /* like --shot's camera 5 */
        V3 mid = V((tr_minx + tr_maxx) * 0.5f, 6, (tr_minz + tr_maxz) * 0.5f);
        camera_look_from(V(mid.x + 40, 150, tr_minz - 170), mid);
    }

    if (state == ST_EDIT) {
        ed_report();
    }

    if (state == ST_RACE || state == ST_DONE) {
        printf("race: t=%.1f lap %d dmg %.1f falls %d best %.1f\n", racetime, cars[0].lap, cars[0].damage, stats.falls,
               cars[0].best_lap);
    }

    gpu_render(path);
    printf("wrote %s\n", path);
    SDL_Quit();
}
