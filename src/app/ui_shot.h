/* app/ui_shot.h - scripted menu screenshots (--ui-shot) */
#pragma once

/* debug: drive the menus and the builder headless with a script of key names, then render one frame:
 * "Down Down Return wait2 text:Bob" (wait N runs N seconds of the game, text: types into a name field) */
static void ui_shot(const char *path, const char *script)
{
    if (!SDL_Init(SDL_INIT_VIDEO) || gpu_init() != 0 || ensure_targets(1920, 1080) != 0) {
        return;
    }

    setup_menu();
    update_camera(&cars[0], 1.f / 60.f, 1);
    autoplay = getenv("SCR_AUTO") != NULL; /* the autopilot drives the player's races */
    char buf[2048];
    snprintf(buf, sizeof buf, "%s", script);

    for (char *tok = strtok(buf, " "); tok; tok = strtok(NULL, " ")) {
        if (!strncmp(tok, "wait", 4)) {
            int n = (int)(atof(tok + 4) * 60);

            for (int f = 0; f < n; f++) {
                for (int k = 0; k < 4; k++) {
                    if (!paused) {
                        game_tick(PHYS_DT);
                    }
                }

                frame_timers(1.f / 60);
                update_camera(&cars[cam_car()], 1.f / 60.f, 0);
            }

            continue;
        }

        if (!strncmp(tok, "text:", 5)) {
            if (state == ST_MENU) {
                menu_text(tok + 5);
            } else {
                ed_text(tok + 5);
            }
            continue;
        }
        SDL_Keymod mod = 0;

        for (int more = 1; more;) { /* shift+ ctrl+ alt+, in any order */
            more = 0;

            if (!strncmp(tok, "shift+", 6)) {
                mod |= SDL_KMOD_SHIFT;
                tok += 6;
                more = 1;
            }

            if (!strncmp(tok, "ctrl+", 5)) {
                mod |= SDL_KMOD_CTRL;
                tok += 5;
                more = 1;
            }

            if (!strncmp(tok, "alt+", 4)) {
                mod |= SDL_KMOD_ALT;
                tok += 4;
                more = 1;
            }
        }

        if (!strncmp(tok, "click:", 6)) { /* click:X,Y[,B] - a mouse click in the HUD's units (B 1 left, 3 right) */
            float cx = 0, cy = 0;
            int cb = 1;
            sscanf(tok + 6, "%f,%f,%d", &cx, &cy, &cb);

            if (state == ST_EDIT) {
                ed_cam(0);
                SDL_SetModState(mod);
                ed_button(cx, cy, cb, 1);
                ed_button(cx, cy, cb, 0);
                SDL_SetModState(0);
            }

            continue;
        }

        if (!strncmp(tok, "fly:", 4)) { /* fly:X,Y,Z,YAW,PITCH - put the builder's camera there (degrees) */
            float v[5] = {0};
            sscanf(tok + 4, "%f,%f,%f,%f,%f", &v[0], &v[1], &v[2], &v[3], &v[4]);

            if (state == ST_EDIT) {
                ed_fly_to(v);
            }

            continue;
        }

        SDL_Keycode k = SDL_GetKeyFromName(tok);

        if (k == SDLK_UNKNOWN) {
            fprintf(stderr, "ui-shot: unknown key %s\n", tok);
            continue;
        }

        if (!handle_key(k, mod)) {
            break;
        }

        update_camera(&cars[cam_car()], 1.f / 60.f, 1);
    }

    if (getenv("SCR_OVERVIEW")) { /* the whole track from high up, like --shot's CAM 5 */
        V3 mid = V((tr_minx + tr_maxx) * 0.5f, 6, (tr_minz + tr_maxz) * 0.5f);
        cam.pos = V(mid.x + 40, 150, tr_minz - 170);
        cam.fwd = vnorm(vsub(mid, cam.pos));
        cam.right = vnorm(vcross(cam.fwd, UP));
        cam.up = vcross(cam.right, cam.fwd);
    }

    if (state == ST_EDIT) {
        ed_report();
    }

    if (state == ST_RACE || state == ST_DONE) {
        printf("race: t=%.1f lap %d dmg %.1f falls %d best %.1f\n", racetime, cars[0].lap, cars[0].damage, stat_falls,
               cars[0].bestlap);
    }

    gpu_render(path);
    printf("wrote %s\n", path);
    SDL_Quit();
}
