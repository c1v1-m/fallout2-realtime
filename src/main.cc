#include "main.h"

#include <limits.h>
#include <string.h>

#include <SDL.h>

#include "art.h"
#include "autorun.h"
#include "character_selector.h"
#include "color.h"
#include "credits.h"
#include "cycle.h"
#include "db.h"
#include "debug.h"
#include "draw.h"
#include "endgame.h"
#include "game.h"
#include "game_mouse.h"
#include "game_movie.h"
#include "game_sound.h"
#include "input.h"
#include "kb.h"
#include "loadsave.h"
#include "mainmenu.h"
#include "map.h"
#include "mouse.h"
#include "object.h"
#include "palette.h"
#include "platform_compat.h"
#include "preferences.h"
#include "proto.h"
#include "random.h"
#include "scripts.h"
#include "settings.h"
#include "sfall_config.h"
#include "sfall_global_scripts.h"
#include "svga.h"
#include "text_font.h"
#include "window.h"
#include "window_manager.h"
#include "window_manager_private.h"
#include "word_wrap.h"
#include "worldmap.h"

#include "rt/rt_version.h"
#include "rt/game/autotest.h"
#include "rt/game/combat_rt.h"
#include "rt/game/game_trace.h"
#include "rt/game/gamepad_ui.h"
#include "rt/rt_hooks.h"

namespace fallout {

#define DEATH_WINDOW_WIDTH 640
#define DEATH_WINDOW_HEIGHT 480

static bool falloutInit(int argc, char** argv);
static int main_reset_system();
static void main_exit_system();
static int _main_load_new(char* fname);
static int main_loadgame_new();
static void main_unload_new();
static void main_frame_meter();
static void mainLoop();
static void showDeath();
static void _main_death_voiceover_callback();
static int _mainDeathGrabTextFile(const char* fileName, char* dest);
static int _mainDeathWordWrap(char* text, int width, short* beginnings, short* count);

// 0x5194C8
static char _mainMap[] = "artemple.map";

// 0x5194D8
static int _main_game_paused = 0;

// 0x5194E8
static bool _main_show_death_scene = false;

// 0x614838
static bool _main_death_voiceover_done;

// 0x48099C
int falloutMain(int argc, char** argv)
{
    // --autotest: the game plays a scripted fight by itself and quits (rt/game/autotest.cc). It runs
    // beside a normal game, so it does not take the single-instance mutex.
    bool autotest = autotest_init(argc, argv);

    if (!autotest && !autorunMutexCreate()) {
        return 1;
    }

    if (!falloutInit(argc, argv)) {
        return 1;
    }

    if (autotest) {
        randomSeedPrerandom(autotest_seed());
        _main_load_new(autotest_map());
        autotest_setup_world();
        mainLoop();
        autotest_finish();
    }

    // SFALL: Allow to skip intro movies
    int skipOpeningMovies;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_SKIP_OPENING_MOVIES_KEY, &skipOpeningMovies);
    if (skipOpeningMovies < 1) {
        gameMoviePlay(MOVIE_IPLOGO, GAME_MOVIE_FADE_IN);
        gameMoviePlay(MOVIE_INTRO, 0);
        gameMoviePlay(MOVIE_CREDITS, 0);
    }

    if (mainMenuWindowInit() == 0) {
        bool done = false;
        while (!done) {
            keyboardReset();
            _gsound_background_play_level_music("07desert", 11);
            mainMenuWindowUnhide(1);

            mouseShowCursor();
            int mainMenuRc = mainMenuWindowHandleEvents();
            mouseHideCursor();

            switch (mainMenuRc) {
            case MAIN_MENU_INTRO:
                mainMenuWindowHide(true);
                gameMoviePlay(MOVIE_INTRO, GAME_MOVIE_STOP_MUSIC);
                gameMoviePlay(MOVIE_CREDITS, 0);
                break;
            case MAIN_MENU_NEW_GAME:
                mainMenuWindowHide(true);
                mainMenuWindowFree();
                if (characterSelectorOpen() == 2) {
                    gameMoviePlay(MOVIE_ELDER, GAME_MOVIE_STOP_MUSIC);
                    randomSeedPrerandom(-1);

                    // SFALL: Override starting map.
                    char* mapName = nullptr;
                    if (configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_STARTING_MAP_KEY, &mapName)) {
                        if (*mapName == '\0') {
                            mapName = nullptr;
                        }
                    }

                    char* mapNameCopy = compat_strdup(mapName != nullptr ? mapName : _mainMap);
                    _main_load_new(mapNameCopy);
                    free(mapNameCopy);

                    // SFALL: AfterNewGameStartHook.
                    sfall_gl_scr_exec_start_proc();

                    mainLoop();
                    paletteFadeTo(gPaletteWhite);

                    // NOTE: Uninline.
                    main_unload_new();

                    // NOTE: Uninline.
                    main_reset_system();

                    if (_main_show_death_scene != 0) {
                        showDeath();
                        _main_show_death_scene = 0;
                    }
                }

                mainMenuWindowInit();

                break;
            case MAIN_MENU_LOAD_GAME:
                if (1) {
                    int win = windowCreate(0, 0, screenGetWidth(), screenGetHeight(), _colorTable[0], WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
                    mainMenuWindowHide(true);
                    mainMenuWindowFree();

                    // NOTE: Uninline.
                    main_loadgame_new();

                    colorPaletteLoad("color.pal");
                    paletteFadeTo(_cmap);
                    int loadGameRc = lsgLoadGame(LOAD_SAVE_MODE_FROM_MAIN_MENU);
                    if (loadGameRc == -1) {
                        debugPrint("\n ** Error running LoadGame()! **\n");
                    } else if (loadGameRc != 0) {
                        windowDestroy(win);
                        win = -1;
                        mainLoop();
                    }
                    paletteFadeTo(gPaletteWhite);
                    if (win != -1) {
                        windowDestroy(win);
                    }

                    // NOTE: Uninline.
                    main_unload_new();

                    // NOTE: Uninline.
                    main_reset_system();

                    if (_main_show_death_scene != 0) {
                        showDeath();
                        _main_show_death_scene = 0;
                    }
                    mainMenuWindowInit();
                }
                break;
            case MAIN_MENU_CONTINUE:
                // Realtime build (fallout1-ce): the newest save with no load screen in between -- the
                // LOAD GAME case's shape exactly (cover, fades, the new/unload/reset pairing), which the
                // slot load needs, not the screen.
                if (1) {
                    int win = windowCreate(0, 0, screenGetWidth(), screenGetHeight(), _colorTable[0], WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
                    mainMenuWindowHide(true);
                    mainMenuWindowFree();

                    // NOTE: Uninline.
                    main_loadgame_new();

                    colorPaletteLoad("color.pal");
                    paletteFadeTo(_cmap);
                    int continueRc = LoadGameNewest();
                    if (continueRc > 0) {
                        windowDestroy(win);
                        win = -1;
                        mainLoop();
                    }
                    paletteFadeTo(gPaletteWhite);
                    if (win != -1) {
                        windowDestroy(win);
                    }

                    // NOTE: Uninline.
                    main_unload_new();

                    // NOTE: Uninline.
                    main_reset_system();

                    if (_main_show_death_scene != 0) {
                        showDeath();
                        _main_show_death_scene = 0;
                    }
                    mainMenuWindowInit();

                    // Nothing to resume (0) or the load failed (-1): say so rather than a silent menu.
                    if (continueRc <= 0) {
                        soundPlayFile("iisxxxx1");
                    }
                }
                break;
            case MAIN_MENU_TIMEOUT:
                debugPrint("Main menu timed-out\n");
                // FALLTHROUGH
            case MAIN_MENU_SCREENSAVER:
                mainMenuWindowHide(true);
                gameMoviePlay(MOVIE_INTRO, GAME_MOVIE_PAUSE_MUSIC);
                break;
            case MAIN_MENU_OPTIONS:
                mainMenuWindowHide(true);
                doPreferences(true);
                break;
            case MAIN_MENU_CREDITS:
                mainMenuWindowHide(true);
                creditsOpen("credits.txt", -1, false);
                break;
            case MAIN_MENU_QUOTES:
                // NOTE: There is a strange cmp at 0x480C50. Both operands are
                // zero, set before the loop and do not modify afterwards. For
                // clarity this condition is omitted.
                mainMenuWindowHide(true);
                creditsOpen("quotes.txt", -1, true);
                break;
            case MAIN_MENU_EXIT:
            case -1:
                done = true;
                mainMenuWindowHide(true);
                mainMenuWindowFree();
                backgroundSoundDelete();
                break;
            case MAIN_MENU_SELFRUN:
                break;
            }
        }
    }

    // NOTE: Uninline.
    main_exit_system();

    autorunMutexClose();

    return 0;
}

// 0x480CC0
static bool falloutInit(int argc, char** argv)
{
    // Realtime build: our own window title, which is also how the installer recognises this exe in
    // Steam's Fallout2Launcher.exe place (rt/rt_version.h). " (dev)" on the development build.
    if (gameInitWithOptions(RT_WINDOW_TITLE, false, 0, 0, argc, argv) == -1) {
        return false;
    }

    // Realtime build: the flight recorder -- trace log + crash handler, now that the config is
    // loaded (cfg trace_log). combat_rt provides the heartbeat snapshot (realtime state + health checks).
    trace_init();
    trace_set_snapshot_fn(combat_rt_trace_snapshot);

    return true;
}

// NOTE: Inlined.
//
// 0x480D0C
static int main_reset_system()
{
    gameReset();

    return 1;
}

// NOTE: Inlined.
//
// 0x480D18
static void main_exit_system()
{
    backgroundSoundDelete();

    trace_shutdown();

    gameExit();
}

// 0x480D4C
static int _main_load_new(char* mapFileName)
{
    _game_user_wants_to_quit = 0;
    _main_show_death_scene = 0;
    gDude->flags &= ~OBJECT_FLAT;
    objectShow(gDude, nullptr);
    mouseHideCursor();

    int win = windowCreate(0, 0, screenGetWidth(), screenGetHeight(), _colorTable[0], WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    windowRefresh(win);

    colorPaletteLoad("color.pal");
    paletteFadeTo(_cmap);
    _map_init();
    gameMouseSetCursor(MOUSE_CURSOR_NONE);
    mouseShowCursor();
    mapLoadByName(mapFileName);
    wmMapMusicStart();
    paletteFadeTo(gPaletteWhite);
    windowDestroy(win);
    colorPaletteLoad("color.pal");
    paletteFadeTo(_cmap);
    return 0;
}

// NOTE: Inlined.
//
// 0x480DF8
static int main_loadgame_new()
{
    _game_user_wants_to_quit = 0;
    _main_show_death_scene = 0;

    gDude->flags &= ~OBJECT_FLAT;

    objectShow(gDude, nullptr);
    mouseHideCursor();

    _map_init();

    gameMouseSetCursor(MOUSE_CURSOR_NONE);
    mouseShowCursor();

    return 0;
}

// Realtime build: the frame-pace meter, a small cousin of fallout1-ce's main_frame_meter. Every 30 s
// of the main loop, one `[ANIM/pace]` trace line: frames per second, average and worst frame, how
// many frames ran late (> 1.5x the period), and whether VSync is really pacing the game -- the line
// says IGNORED when the driver refused the swap interval (svga.cc hands pacing back to the sleep).
static void main_frame_meter()
{
    static Uint64 last = 0;
    static Uint64 windowStart = 0;
    static int frames = 0;
    static double sumMs = 0.0;
    static double worstMs = 0.0;
    static int late = 0;

    Uint64 now = SDL_GetPerformanceCounter();
    double freq = (double)SDL_GetPerformanceFrequency();
    if (last == 0) {
        last = now;
        windowStart = now;
        return;
    }

    double ms = (double)(now - last) * 1000.0 / freq;
    last = now;
    if (ms > 250.0) {
        return; // a pause (a modal screen ran its own loop), not a frame
    }

    int interval;
    int refresh;
    bool effective;
    svga_vsync_state(&interval, &refresh, &effective);
    double period = interval > 0 && refresh > 0 ? interval * 1000.0 / refresh : 1000.0 / 60.0;

    frames++;
    sumMs += ms;
    if (ms > worstMs) {
        worstMs = ms;
    }
    if (ms > period * 1.5) {
        late++;
    }

    double windowSec = (double)(now - windowStart) / freq;
    if (windowSec >= 30.0) {
        trace_event(TRACE_ANIM, "pace", "fps=%.1f avg=%.2fms worst=%.1fms late=%d vsync=%d@%dHz%s",
            frames / windowSec, sumMs / frames, worstMs, late, interval, refresh,
            interval > 0 && !effective ? " IGNORED" : "");
        windowStart = now;
        frames = 0;
        sumMs = 0.0;
        worstMs = 0.0;
        late = 0;
    }
}

// 0x480E34
static void main_unload_new()
{
    objectHide(gDude, nullptr);
    _map_exit();
}

// 0x480E48
static void mainLoop()
{
    bool cursorWasHidden = cursorIsHidden();
    if (cursorWasHidden) {
        mouseShowCursor();
    }

    _main_game_paused = 0;

    scriptsEnable();

    unsigned int rtLastTime = getTicks();

    while (_game_user_wants_to_quit == 0) {
        sharedFpsLimiter.mark();

        // Realtime build: frame delta for the flight recorder. Capped like F1's: a modal loop
        // (inventory, options) does not run this loop, and on resume the whole stay would land in
        // one frame.
        unsigned int rtNow = getTicks();
        float rtDelta = (rtNow - rtLastTime) / 1000.0f;
        rtLastTime = rtNow;
        if (rtDelta > 0.25f) {
            rtDelta = 0.25f;
        }

        int keyCode = inputGetInput();

        // SFALL: MainLoopHook.
        sfall_gl_scr_process_main();

        // Debug: F11 toggles a red ring at 15 hex tiles around the dude (the gunshot-hearing
        // distance). Development build only.
#ifndef FALLOUT_RELEASE
        if (keyCode == KEY_F11) {
            tile_debug_ring_set(tile_debug_ring_get() > 0 ? 0 : 15);
        }
#endif

        // The keyboard as a controller first: arrows, Space, Q/E, H, R, F, V and G belong to the pad
        // layer (gamepad_ui_handle_key). Then realtime combat (the optional mouse aim and its
        // right-click called shot); anything left falls through to vanilla.
        bool rtConsumed = gamepad_ui_handle_key(&keyCode) || combat_rt_handle_input(keyCode);
        if (keyCode != -1) {
            trace_event(TRACE_INPUT, "key", "code=%d rt=%d", keyCode, rtConsumed ? 1 : 0);
        }
        if (!rtConsumed) {
            gameHandleKey(keyCode, false);
        }

        // Realtime combat: the whole world's realtime step (AI, attacks, AP, reactions, camera).
        combat_rt_update(rtDelta);

        autotest_tick(rtDelta); // the bot's pad for the next frame, and the end of a self-played run

        // While the debug ring is up, refresh fully when the dude changes tile so it never ghosts.
        if (tile_debug_ring_get() > 0 && gDude != nullptr) {
            static int ringPrevTile = -1;
            if (gDude->tile != ringPrevTile) {
                ringPrevTile = gDude->tile;
                tileWindowRefresh();
            }
        }

        // Realtime build: flight-recorder pump (frame counter, heartbeat snapshot, coalesce ageing).
        trace_tick(rtDelta);
        main_frame_meter();

        // Realtime build: the gamepad overlays that touch windows (wheels, the interact prompt) run
        // here, at a safe point outside the input/mouse hook, to avoid re-entrant windowing.
        gamepad_ui_tick();

        scriptsHandleRequests();

        mapHandleTransition();

        // Realtime build (fallout1-ce): AUTOSAVE on entering a new location and on any quest step -- here,
        // with the map settled and outside the map-load machinery (a save re-enters map saving). Not
        // the map change a load causes (LoadSaveConsumeJustLoaded), not the first map of a session,
        // never in a fight or while a body is still falling (a save stops every animation and would
        // drop it flat). The triggers are LATCHED: one that comes due in a fight waits for its end
        // instead of being lost.
        // ⚠️ AND NOT WHILE THE UI IS DISABLED, which is a script's scene in progress: vanilla cannot save
        // then at all, and the scene's own step counter (a script variable) is not in a save, so a load
        // of one made mid-scene finds it dead. GAMMOVIE's autosave landed right after its map_enter
        // started the Frank Horrigan sequence; the reload stood the farmers and the patrol still for good.
        {
            bool justLoaded = LoadSaveConsumeJustLoaded();
            bool canAutosave = gDude != nullptr && !isInCombat() && !combat_rt_death_anim_pending() && !autotest_active()
                && !gameUiIsDisabled();

            static bool autosavePendingLocation = false;
            static bool autosavePendingQuest = false;

            static int autosaveLastMap = -1;
            int currentMap = mapGetCurrentMap();
            if (currentMap >= 0 && currentMap != autosaveLastMap) {
                bool first = autosaveLastMap == -1;
                autosaveLastMap = currentMap;
                if (!first && !justLoaded) {
                    autosavePendingLocation = true;
                }
            }

            static unsigned int autosaveLastQuest = 0;
            static bool autosaveQuestSeeded = false;
            if (gDude != nullptr) {
                unsigned int currentQuest = pipboy_quest_state_hash();
                if (!autosaveQuestSeeded || justLoaded) {
                    autosaveLastQuest = currentQuest;
                    autosaveQuestSeeded = true;
                } else if (currentQuest != autosaveLastQuest) {
                    autosaveLastQuest = currentQuest;
                    autosavePendingQuest = true;
                }
            }

            // One autosave a frame at most, the location first.
            if (autosavePendingLocation) {
                if (canAutosave) {
                    autosavePendingLocation = false;
                    AutoSaveLocation();
                }
            } else if (autosavePendingQuest) {
                if (canAutosave) {
                    autosavePendingQuest = false;
                    AutoSaveQuest();
                }
            }
        }

        if (_main_game_paused != 0) {
            _main_game_paused = 0;
        }

        // Trigger the death scene only once the dude is dead/knocked out AND his death/knockout animation has
        // finished (from fallout1-ce; Fallout 2 Realtime 2026-10-31, "nepřehrává se animace smrti … dude"). Under
        // combat_rt the lethal hit is committed at swing-connect, so DAM_DEAD is set while the death animation is
        // still only queued; bailing the instant the flag appears skipped the animation entirely (the dude just
        // stood there, then the death-scene fade). Turn-based is unaffected: its damage commits at sequence-end,
        // where the dude is already idle.
        if ((gDude->data.critter.combat.results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0
            && animationIsBusy(gDude) == 0) {
            endgameSetupDeathEnding(ENDGAME_DEATH_ENDING_REASON_DEATH);
            _main_show_death_scene = 1;
            _game_user_wants_to_quit = 2;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    scriptsDisable();

    if (cursorWasHidden) {
        mouseHideCursor();
    }
}

// 0x48118C
static void showDeath()
{
    artCacheFlush();
    colorCycleDisable();
    gameMouseSetCursor(MOUSE_CURSOR_NONE);

    bool oldCursorIsHidden = cursorIsHidden();
    if (oldCursorIsHidden) {
        mouseShowCursor();
    }

    int deathWindowX = (screenGetWidth() - DEATH_WINDOW_WIDTH) / 2;
    int deathWindowY = (screenGetHeight() - DEATH_WINDOW_HEIGHT) / 2;
    int win = windowCreate(deathWindowX,
        deathWindowY,
        DEATH_WINDOW_WIDTH,
        DEATH_WINDOW_HEIGHT,
        0,
        WINDOW_MOVE_ON_TOP);
    if (win != -1) {
        do {
            unsigned char* windowBuffer = windowGetBuffer(win);
            if (windowBuffer == nullptr) {
                break;
            }

            // DEATH.FRM
            FrmImage backgroundFrmImage;
            int fid = buildFid(OBJ_TYPE_INTERFACE, 309, 0, 0, 0);
            if (!backgroundFrmImage.lock(fid)) {
                break;
            }

            while (mouseGetEvent() != 0) {
                sharedFpsLimiter.mark();

                inputGetInput();

                renderPresent();
                sharedFpsLimiter.throttle();
            }

            keyboardReset();
            inputEventQueueReset();

            blitBufferToBuffer(backgroundFrmImage.getData(), 640, 480, 640, windowBuffer, 640);
            backgroundFrmImage.unlock();

            const char* deathFileName = endgameDeathEndingGetFileName();

            if (settings.preferences.subtitles) {
                char text[512];
                if (_mainDeathGrabTextFile(deathFileName, text) == 0) {
                    debugPrint("\n((ShowDeath)): %s\n", text);

                    short beginnings[WORD_WRAP_MAX_COUNT];
                    short count;
                    if (_mainDeathWordWrap(text, 560, beginnings, &count) == 0) {
                        unsigned char* p = windowBuffer + 640 * (480 - fontGetLineHeight() * count - 8);
                        bufferFill(p - 602, 564, fontGetLineHeight() * count + 2, 640, 0);
                        p += 40;
                        for (int index = 0; index < count; index++) {
                            fontDrawText(p, text + beginnings[index], 560, 640, _colorTable[32767]);
                            p += 640 * fontGetLineHeight();
                        }
                    }
                }
            }

            windowRefresh(win);

            colorPaletteLoad("art\\intrface\\death.pal");
            paletteFadeTo(_cmap);

            _main_death_voiceover_done = false;
            speechSetEndCallback(_main_death_voiceover_callback);

            unsigned int delay;
            if (speechLoad(deathFileName, 10, 14, 15) == -1) {
                delay = 3000;
            } else {
                delay = UINT_MAX;
            }

            _gsound_speech_play_preloaded();

            // SFALL: Fix the playback of the speech sound file for the death
            // screen.
            inputBlockForTocks(100);

            unsigned int time = getTicks();
            int keyCode;
            do {
                sharedFpsLimiter.mark();

                keyCode = inputGetInput();

                renderPresent();
                sharedFpsLimiter.throttle();
            } while (keyCode == -1 && !_main_death_voiceover_done && getTicksSince(time) < delay);

            speechSetEndCallback(nullptr);

            speechDelete();

            while (mouseGetEvent() != 0) {
                sharedFpsLimiter.mark();

                inputGetInput();

                renderPresent();
                sharedFpsLimiter.throttle();
            }

            if (keyCode == -1) {
                inputPauseForTocks(500);
            }

            paletteFadeTo(gPaletteBlack);
            colorPaletteLoad("color.pal");
        } while (0);
        windowDestroy(win);
    }

    if (oldCursorIsHidden) {
        mouseHideCursor();
    }

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    colorCycleEnable();
}

// 0x4814A8
static void _main_death_voiceover_callback()
{
    _main_death_voiceover_done = true;
}

// Read endgame subtitle.
//
// 0x4814B4
static int _mainDeathGrabTextFile(const char* fileName, char* dest)
{
    const char* p = strrchr(fileName, '\\');
    if (p == nullptr) {
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "text\\%s\\cuts\\%s%s", settings.system.language.c_str(), p + 1, ".TXT");

    File* stream = fileOpen(path, "rt");
    if (stream == nullptr) {
        return -1;
    }

    while (true) {
        int c = fileReadChar(stream);
        if (c == -1) {
            break;
        }

        if (c == '\n') {
            c = ' ';
        }

        *dest++ = (c & 0xFF);
    }

    fileClose(stream);

    *dest = '\0';

    return 0;
}

// 0x481598
static int _mainDeathWordWrap(char* text, int width, short* beginnings, short* count)
{
    while (true) {
        char* sep = strchr(text, ':');
        if (sep == nullptr) {
            break;
        }

        if (sep - 1 < text) {
            break;
        }
        sep[0] = ' ';
        sep[-1] = ' ';
    }

    if (wordWrap(text, width, beginnings, count) == -1) {
        return -1;
    }

    // TODO: Probably wrong.
    *count -= 1;

    for (int index = 1; index < *count; index++) {
        char* p = text + beginnings[index];
        while (p >= text && *p != ' ') {
            p--;
            beginnings[index]--;
        }

        if (p != nullptr) {
            *p = '\0';
            beginnings[index]++;
        }
    }

    return 0;
}

} // namespace fallout
