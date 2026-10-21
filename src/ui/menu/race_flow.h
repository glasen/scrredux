/* ui/menu/race_flow.h - starting and leaving races and replays from the menus */
#pragma once

static void season_race(void)
{
    Race *r = player_race();

    if (!r) {
        return;
    }

    race_mode = RM_SEASON;
    set_opponent(r->a == PLAYER ? r->b : r->a);
    sel_track = r->track;

    if (cur_track != sel_track) {
        build_track(sel_track);
    }

    cockpit = 1;
    preview_start();
}

/* out of a race (ENTER at the end, or ESC): the season's books, then back to where it came from */
static void ed_return(void);

static void race_leave(int retired)
{
    rec_on = rec_on && race_mode == RM_SEASON;

    if (race_mode == RM_SEASON) {
        Race *r = player_race();

        if (!r) {
            menu_go(MS_SEASON);
            return;
        }

        const Car *p = &cars[0], *o = &cars[1];
        float tp = !retired && p->finished && !p->wrecked ? p->ftime : -1,
              to = !retired && o->finished && !o->wrecked ? o->ftime : -1;
        int win_p = !retired && result > 0;
        float ta = r->a == PLAYER ? tp : to, tb = r->a == PLAYER ? to : tp;
        float la = r->a == PLAYER ? p->bestlap : o->bestlap, lb = r->a == PLAYER ? o->bestlap : p->bestlap;
        race_score(r, win_p ? PLAYER : opp_drv, ta, tb, la, lb);
        last_res = *r;
        last_retired = retired;
        last_lap_rec = last_race_rec = 0;
        const char *tn = trk[r->track].name;

        if (tp > 0) {
            last_race_rec |= hof_race(tn, player_name(), tp);
        }

        if (to > 0) {
            hof_race(tn, drv_name(opp_drv), to);
        }

        sea.nholes = p->nholes;
        char title[64];
        snprintf(title, sizeof title, "%s - %s v %s - %s", tn, player_name(), drv_name(opp_drv),
                 win_p ? "won" : "lost");
        rec_save(title, player_name(), drv_name(opp_drv), result);
        season_advance();
        season_write(0);
        menu_go(MS_RESULT);
    } else if (race_mode == RM_TEST) {
        ed_return();
    } else {
        menu_go(race_mode == RM_PRACTICE ? MS_PRACTICE : MS_MAIN);
    }
}

static void replay_start(int i)
{
    if (rp_load(rp_slot[i])) {
        menu_note("That replay cannot be read");
        return;
    }

    RHead *h = &rp_head;

    if (h->td.orig >= 0 && h->td.orig < SCR_NTRACKS) {
        if (cur_track != h->td.orig) {
            build_track(h->td.orig);
        }
    } else {
        trk[ED_SLOT] = h->td;
        build_track(ED_SLOT);
    }
    memset(cars, 0, sizeof cars);

    for (int k = 0; k < 2; k++) {
        cars[k].col = h->col[k];
    }

    laps = h->laps;
    npart = 0;
    rp_t = 0;
    rp_speed = 1;
    rp_paused = 0;
    rp_cam = RC_COCKPIT;
    paused = 0;
    state = ST_REPLAY;
    rp_step(0);
    update_camera(&cars[0], 0.016f, 1);
}

static void replay_leave(void)
{
    free(rpb);
    rpb = NULL;
    nrp = 0;
    sel_track = sel_track < ntrk ? sel_track : 0;
    menu_go(MS_REPLAYS);
}
