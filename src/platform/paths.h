/* platform/paths.h - where game data and the player's files live */
#pragma once

static void data_path(char *out, size_t n, const char *rel)
{
    const char *b = SDL_GetBasePath();
    snprintf(out, n, "%s%s", b ? b : "", rel);
}
/* the player's own files - settings, saves, replays, tracks - go in SDL's folder for them, not beside the game (an app
 * bundle or a folder the game cannot write to): ~/Library/Application Support/SCR Redux/SCR Redux/ on macOS,
 * %APPDATA%\SCR Redux\SCR Redux\ on Windows, ~/.local/share/SCR Redux/SCR Redux/ on Linux. SCR_HOME overrides it
 * (a portable copy, tests) */
static void user_path(char *out, size_t n, const char *rel)
{
    static char dir[1024];
    if (!dir[0]) {
        const char *e = getenv("SCR_HOME");
        char *pp = e && *e ? NULL : SDL_GetPrefPath(PREF_ORG, PREF_APP);
        if (e && *e) { size_t k = strlen(e); snprintf(dir, sizeof dir, "%s%s", e, e[k - 1] == '/' || e[k - 1] == '\\' ? "" : "/"); SDL_CreateDirectory(dir); }
        else if (pp) snprintf(dir, sizeof dir, "%s", pp);
        else data_path(dir, sizeof dir, "");                    /* no such folder here: beside the game, as before */
        SDL_free(pp);
    }
    snprintf(out, n, "%s%s", dir, rel);
}
