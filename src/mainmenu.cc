#include "mainmenu.h"

#include <ctype.h>

#include "art.h"
#include "color.h"
#include "draw.h"
#include "game.h"
#include "game_sound.h"
#include "input.h"
#include "kb.h"
#include "mouse.h"
#include "palette.h"
#include "preferences.h"
#include "sfall_config.h"
#include "svga.h"
#include "text_font.h"
#include "version.h"
#include "window_manager.h"

#include <SDL.h>

// Realtime / controller (fallout1-ce's mainmenu.cc changes, ported).
#include "game_mouse.h"
#include "rt/game/gamepad_ui.h"

namespace fallout {

#define MAIN_MENU_WINDOW_WIDTH 640
#define MAIN_MENU_WINDOW_HEIGHT 480

typedef enum MainMenuButton {
    MAIN_MENU_BUTTON_INTRO,
    MAIN_MENU_BUTTON_CONTINUE, // Realtime build (fallout1-ce): the newest save
    MAIN_MENU_BUTTON_NEW_GAME,
    MAIN_MENU_BUTTON_LOAD_GAME,
    MAIN_MENU_BUTTON_OPTIONS,
    MAIN_MENU_BUTTON_CREDITS,
    MAIN_MENU_BUTTON_EXIT,
    MAIN_MENU_BUTTON_COUNT,
} MainMenuButton;

static int main_menu_fatal_error();
static void main_menu_play_sound(const char* fileName);
static void main_menu_draw_option(int index, int color);
static void main_menu_gamepad_highlight(int sel);
static void main_menu_tick_cursor(bool* shown, int* lastX, int* lastY, unsigned int* lastMove);

// Where the labels were drawn and in what colour (sfall can move the buttons and recolour the font),
// kept so the controller highlight can redraw one label in place.
static int gMainMenuLabelOffsetX = 0;
static int gMainMenuLabelOffsetY = 0;
static int gMainMenuLabelColor = 0;

// 0x5194F0
static int gMainMenuWindow = -1;

// 0x5194F4
static unsigned char* gMainMenuWindowBuffer = nullptr;

// 0x519504
// Realtime build: not static -- the gamepad layer reads it (rt/f1compat.h maps F1's in_main_menu here).
bool _in_main_menu = false;

// 0x519508
static bool gMainMenuWindowInitialized = false;

// 0x51950C
static unsigned int gMainMenuScreensaverDelay = 120000;

// 0x519510
static const int gMainMenuButtonKeyBindings[MAIN_MENU_BUTTON_COUNT] = {
    KEY_LOWERCASE_I, // intro
    KEY_LOWERCASE_T, // continue (c is credits, o options: conTinue)
    KEY_LOWERCASE_N, // new game
    KEY_LOWERCASE_L, // load game
    KEY_LOWERCASE_O, // options
    KEY_LOWERCASE_C, // credits
    KEY_LOWERCASE_E, // exit
};

// 0x519528
static const int _return_values[MAIN_MENU_BUTTON_COUNT] = {
    MAIN_MENU_INTRO,
    MAIN_MENU_CONTINUE,
    MAIN_MENU_NEW_GAME,
    MAIN_MENU_LOAD_GAME,
    MAIN_MENU_OPTIONS,
    MAIN_MENU_CREDITS,
    MAIN_MENU_EXIT,
};

// Realtime build (fallout1-ce): THE LABEL IS NO LONGER `9 + index` -- misc.msg 9..14 are INTRO / NEW GAME /
// LOAD GAME / OPTIONS / CREDITS / EXIT in that order, and CONTINUE sits second. CONTINUE's text is
// written here rather than given a msg id: misc.msg lives in MASTER.DAT, and a new id would mean a loose
// copy of the whole file fighting every localisation.
static const int gMainMenuLabelMsg[MAIN_MENU_BUTTON_COUNT] = { 9, -1, 10, 11, 12, 13, 14 };
static const char* gMainMenuLabelLiteral[MAIN_MENU_BUTTON_COUNT] = { nullptr, "CONTINUE", nullptr, nullptr, nullptr, nullptr, nullptr };

static const char* main_menu_label(int index)
{
    if (index < 0 || index >= MAIN_MENU_BUTTON_COUNT) {
        return nullptr;
    }
    if (gMainMenuLabelLiteral[index] != nullptr) {
        return gMainMenuLabelLiteral[index];
    }
    MessageListItem msg;
    msg.num = gMainMenuLabelMsg[index];
    if (messageListGetItem(&gMiscMessageList, &msg)) {
        return msg.text;
    }
    return nullptr;
}

// Realtime build: SIX PLATES IN THE ART, SEVEN BUTTONS. mainmenu.frm paints one riveted plate per button
// down the left, 41 px apart (tops at 13, 54 ... 218, each 37 rows by 191 columns from x 16), with the
// sky and the Chosen One's shoulder below the sixth. The seventh is a copy of the second plate, laid
// in the WINDOW buffer (never the cached art) at the next step down.
#define MAIN_MENU_PLATE_X 16
#define MAIN_MENU_PLATE_WIDTH 191
#define MAIN_MENU_PLATE_TOP 13
#define MAIN_MENU_PLATE_HEIGHT 37
#define MAIN_MENU_PLATE_STEP 41
#define MAIN_MENU_PLATE_ART_COUNT 6

static void main_menu_extend_plates()
{
    int sourceTop = MAIN_MENU_PLATE_TOP + MAIN_MENU_PLATE_STEP; // the second plate
    for (int plate = MAIN_MENU_PLATE_ART_COUNT; plate < MAIN_MENU_BUTTON_COUNT; plate++) {
        int destTop = MAIN_MENU_PLATE_TOP + plate * MAIN_MENU_PLATE_STEP;
        for (int y = 0; y < MAIN_MENU_PLATE_HEIGHT; y++) {
            memcpy(gMainMenuWindowBuffer + MAIN_MENU_WINDOW_WIDTH * (destTop + y) + MAIN_MENU_PLATE_X,
                gMainMenuWindowBuffer + MAIN_MENU_WINDOW_WIDTH * (sourceTop + y) + MAIN_MENU_PLATE_X,
                MAIN_MENU_PLATE_WIDTH);
        }
    }
}

// 0x614840
static int gMainMenuButtons[MAIN_MENU_BUTTON_COUNT];

// 0x614858
static bool gMainMenuWindowHidden;

static FrmImage _mainMenuBackgroundFrmImage;
static FrmImage _mainMenuButtonNormalFrmImage;
static FrmImage _mainMenuButtonPressedFrmImage;

// 0x481650
int mainMenuWindowInit()
{
    int fid;
    MessageListItem msg;
    int len;

    if (gMainMenuWindowInitialized) {
        return 0;
    }

    colorPaletteLoad("color.pal");

    int mainMenuWindowX = (screenGetWidth() - MAIN_MENU_WINDOW_WIDTH) / 2;
    int mainMenuWindowY = (screenGetHeight() - MAIN_MENU_WINDOW_HEIGHT) / 2;
    gMainMenuWindow = windowCreate(mainMenuWindowX,
        mainMenuWindowY,
        MAIN_MENU_WINDOW_WIDTH,
        MAIN_MENU_WINDOW_HEIGHT,
        0,
        WINDOW_HIDDEN | WINDOW_MOVE_ON_TOP);
    if (gMainMenuWindow == -1) {
        // NOTE: Uninline.
        return main_menu_fatal_error();
    }

    gMainMenuWindowBuffer = windowGetBuffer(gMainMenuWindow);

    // mainmenu.frm
    int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, 140, 0, 0, 0);
    if (!_mainMenuBackgroundFrmImage.lock(backgroundFid)) {
        // NOTE: Uninline.
        return main_menu_fatal_error();
    }

    blitBufferToBuffer(_mainMenuBackgroundFrmImage.getData(), 640, 480, 640, gMainMenuWindowBuffer, 640);
    _mainMenuBackgroundFrmImage.unlock();
    main_menu_extend_plates();

    int oldFont = fontGetCurrent();
    fontSetCurrent(100);

    // SFALL: Allow to change font color/flags of copyright/version text
    //        It's the last byte ('3C' by default) that picks the colour used. The first byte supplies additional flags for this option
    //        0x010000 - change the color for version string only
    //        0x020000 - underline text (only for the version string)
    //        0x040000 - monospace font (only for the version string)
    int fontSettings = _colorTable[21091], fontSettingsSFall = 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_FONT_COLOR_KEY, &fontSettingsSFall);
    if (fontSettingsSFall && !(fontSettingsSFall & 0x010000))
        fontSettings = fontSettingsSFall & 0xFF;

    // SFALL: Allow to move copyright text
    int offsetX = 0, offsetY = 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_CREDITS_OFFSET_X_KEY, &offsetX);
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_CREDITS_OFFSET_Y_KEY, &offsetY);

    // Copyright.
    msg.num = 20;
    if (messageListGetItem(&gMiscMessageList, &msg)) {
        windowDrawText(gMainMenuWindow, msg.text, 0, offsetX + 15, offsetY + 460, fontSettings | 0x06000000);
    }

    // SFALL: Make sure font settings are applied when using 0x010000 flag
    if (fontSettingsSFall)
        fontSettings = fontSettingsSFall;

    // TODO: Allow to move version text
    // Version.
    char version[VERSION_MAX];
    versionGetVersion(version, sizeof(version));
    len = fontGetStringWidth(version);
    windowDrawText(gMainMenuWindow, version, 0, 615 - len, 460, fontSettings | 0x06000000);

    // menuup.frm
    fid = buildFid(OBJ_TYPE_INTERFACE, 299, 0, 0, 0);
    if (!_mainMenuButtonNormalFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return main_menu_fatal_error();
    }

    // menudown.frm
    fid = buildFid(OBJ_TYPE_INTERFACE, 300, 0, 0, 0);
    if (!_mainMenuButtonPressedFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return main_menu_fatal_error();
    }

    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        gMainMenuButtons[index] = -1;
    }

    // SFALL: Allow to move menu buttons
    offsetX = offsetY = 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_OFFSET_X_KEY, &offsetX);
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_OFFSET_Y_KEY, &offsetY);

    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        gMainMenuButtons[index] = buttonCreate(gMainMenuWindow,
            offsetX + 30,
            offsetY + 19 + index * 42 - index,
            26,
            26,
            -1,
            -1,
            1111,
            gMainMenuButtonKeyBindings[index],
            _mainMenuButtonNormalFrmImage.getData(),
            _mainMenuButtonPressedFrmImage.getData(),
            nullptr,
            BUTTON_FLAG_TRANSPARENT);
        if (gMainMenuButtons[index] == -1) {
            // NOTE: Uninline.
            return main_menu_fatal_error();
        }

        buttonSetMask(gMainMenuButtons[index], _mainMenuButtonNormalFrmImage.getData());
    }

    fontSetCurrent(104);

    // SFALL: Allow to change font color of buttons
    fontSettings = _colorTable[21091];
    fontSettingsSFall = 0;
    configGetInt(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_MAIN_MENU_BIG_FONT_COLOR_KEY, &fontSettingsSFall);
    if (fontSettingsSFall)
        fontSettings = fontSettingsSFall & 0xFF;

    gMainMenuLabelOffsetX = offsetX;
    gMainMenuLabelOffsetY = offsetY;
    gMainMenuLabelColor = fontSettings;

    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        const char* label = main_menu_label(index);
        if (label != nullptr) {
            len = fontGetStringWidth(label);
            fontDrawText(gMainMenuWindowBuffer + offsetX + 640 * (offsetY + 42 * index - index + 20) + 126 - (len / 2), label, 640 - (126 - (len / 2)) - 1, 640, fontSettings);
        }
    }

    fontSetCurrent(oldFont);

    gMainMenuWindowInitialized = true;
    gMainMenuWindowHidden = true;

    return 0;
}

// 0x481968
void mainMenuWindowFree()
{
    if (!gMainMenuWindowInitialized) {
        return;
    }

    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        // FIXME: Why it tries to free only invalid buttons?
        if (gMainMenuButtons[index] == -1) {
            buttonDestroy(gMainMenuButtons[index]);
        }
    }

    _mainMenuButtonPressedFrmImage.unlock();
    _mainMenuButtonNormalFrmImage.unlock();

    if (gMainMenuWindow != -1) {
        windowDestroy(gMainMenuWindow);
    }

    gMainMenuWindowInitialized = false;
}

// 0x481A00
void mainMenuWindowHide(bool animate)
{
    if (!gMainMenuWindowInitialized) {
        return;
    }

    if (gMainMenuWindowHidden) {
        return;
    }

    soundContinueAll();

    if (animate) {
        paletteFadeTo(gPaletteBlack);
        soundContinueAll();
    }

    windowHide(gMainMenuWindow);

    gMainMenuWindowHidden = true;
}

// 0x481A48
void mainMenuWindowUnhide(bool animate)
{
    if (!gMainMenuWindowInitialized) {
        return;
    }

    if (!gMainMenuWindowHidden) {
        return;
    }

    windowShow(gMainMenuWindow);

    if (animate) {
        colorPaletteLoad("color.pal");
        paletteFadeTo(_cmap);
    }

    gMainMenuWindowHidden = false;
}

// 0x481AA8
int _main_menu_is_enabled()
{
    return 1;
}

// 0x481AEC
int mainMenuWindowHandleEvents()
{
    _in_main_menu = true;

    // Suppress the shared gamepad LEFT-STICK cursor snapping for the WHOLE title menu, right now --
    // before the first inputGetInput() pumps the mouse hook. This menu drives its own D-pad / stick
    // highlight below, so the snap (which moves the cursor to buttons and plays the hover sound) must
    // never run here. Unconditional; harmless with no controller, cleared at the end.
    gamepad_ui_set_self_nav(true);

    bool oldCursorIsHidden = cursorIsHidden();
    if (oldCursorIsHidden) {
        mouseShowCursor();
    }

    // Gamepad menu navigation: highlight the selected option and move it with the D-pad / left stick,
    // confirm with A. Only when a controller is present, so keyboard/mouse keeps the vanilla look.
    SDL_GameController* pad = gamepad_ui_get_controller();

    // ⚠️ THE HIGHLIGHT SURVIVES A SUB-SCREEN: this loop is re-entered every time something it
    // launched comes back, and resetting to NEW GAME put the highlight one press away from throwing
    // the session away ("vyskočím z options, kliknu A, skočí mi to na new game").
    static int selection = MAIN_MENU_BUTTON_NEW_GAME;
    if (selection < 0 || selection >= MAIN_MENU_BUTTON_COUNT) {
        selection = MAIN_MENU_BUTTON_NEW_GAME;
    }

    bool padPrevUp = false;
    bool padPrevDown = false;
    bool padPrevA = false;
    bool padEdgesSeeded = false;
    bool cursorShown = true;
    int cursorX = 0;
    int cursorY = 0;
    unsigned int cursorMove = getTicks();
    // The controller "view" (highlight + hidden cursor) is set up LAZILY, the first frame a pad is
    // actually present -- a wireless pad still waking would otherwise leave the menu dead.
    bool padViewInit = false;

    unsigned int tick = getTicks();

    int rc = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();

        pad = gamepad_ui_get_controller();

        if (pad != nullptr) {
            // Re-assert every frame: keeps the shared hook's snap + A-as-click off for the whole menu.
            gamepad_ui_set_self_nav(true);

            if (!padViewInit) {
                padViewInit = true;
                main_menu_gamepad_highlight(selection);
                windowRefresh(gMainMenuWindow);
                // Hide the pointer (blank shape, not mouseHideCursor -- that gates the input hook),
                // and do NOT teleport it: that left a ghost arrow behind.
                mouseGetPosition(&cursorX, &cursorY);
                gameMouseSetCursor(MOUSE_CURSOR_NONE);
                cursorShown = false;
            }

            SDL_GameControllerUpdate();

            bool upNow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0
                || SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY) < -16000;
            bool downNow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0
                || SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY) > 16000;
            bool aNow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A) != 0;

            // ⚠️ SEED THE EDGES FROM THE LIVE STATE, ONCE: the A that just closed the preferences
            // screen is still down, and it confirmed whatever the highlight sat on ("A stistknutí je
            // new game").
            if (!padEdgesSeeded) {
                padEdgesSeeded = true;
                padPrevUp = upNow;
                padPrevDown = downNow;
                padPrevA = aNow;
            }

            if (upNow && !padPrevUp) {
                selection = (selection + MAIN_MENU_BUTTON_COUNT - 1) % MAIN_MENU_BUTTON_COUNT;
                main_menu_gamepad_highlight(selection);
                windowRefresh(gMainMenuWindow);
                // NOTE: Uninline.
                main_menu_play_sound("nmselec0");
            } else if (downNow && !padPrevDown) {
                selection = (selection + 1) % MAIN_MENU_BUTTON_COUNT;
                main_menu_gamepad_highlight(selection);
                windowRefresh(gMainMenuWindow);
                // NOTE: Uninline.
                main_menu_play_sound("nmselec0");
            }

            if (aNow && !padPrevA && keyCode == -1) {
                // The very key the highlighted button sends, so the loop below handles it exactly
                // like a click (credits + shift = quotes included).
                keyCode = gMainMenuButtonKeyBindings[selection];
            }

            padPrevUp = upNow;
            padPrevDown = downNow;
            padPrevA = aNow;

            main_menu_tick_cursor(&cursorShown, &cursorX, &cursorY, &cursorMove);
        }

        for (int buttonIndex = 0; buttonIndex < MAIN_MENU_BUTTON_COUNT; buttonIndex++) {
            if (keyCode == gMainMenuButtonKeyBindings[buttonIndex] || keyCode == toupper(gMainMenuButtonKeyBindings[buttonIndex])) {
                // NOTE: Uninline.
                main_menu_play_sound("nmselec1");

                rc = _return_values[buttonIndex];

                if (buttonIndex == MAIN_MENU_BUTTON_CREDITS && (gPressedPhysicalKeys[SDL_SCANCODE_RSHIFT] != KEY_STATE_UP || gPressedPhysicalKeys[SDL_SCANCODE_LSHIFT] != KEY_STATE_UP)) {
                    rc = MAIN_MENU_QUOTES;
                }

                break;
            }
        }

        if (rc == -1) {
            if (keyCode == KEY_CTRL_R) {
                rc = MAIN_MENU_SELFRUN;
                continue;
            } else if (keyCode == KEY_PLUS || keyCode == KEY_EQUAL) {
                brightnessIncrease();
            } else if (keyCode == KEY_MINUS || keyCode == KEY_UNDERSCORE) {
                brightnessDecrease();
            } else if (keyCode == KEY_UPPERCASE_D || keyCode == KEY_LOWERCASE_D) {
                rc = MAIN_MENU_SCREENSAVER;
                continue;
            } else if (keyCode == 1111) {
                if (!(mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_REPEAT)) {
                    // NOTE: Uninline.
                    main_menu_play_sound("nmselec0");
                }
                continue;
            }
        }

        if (keyCode == KEY_ESCAPE || _game_user_wants_to_quit == 3) {
            rc = MAIN_MENU_EXIT;

            // NOTE: Uninline.
            main_menu_play_sound("nmselec1");
            break;
        } else if (_game_user_wants_to_quit == 2) {
            _game_user_wants_to_quit = 0;
        } else {
            if (getTicksSince(tick) >= gMainMenuScreensaverDelay) {
                rc = MAIN_MENU_TIMEOUT;
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    // Always clear self-nav (set unconditionally at entry) so it never leaks into gameplay. The
    // cursor restore only matters if we hid it (pad path).
    gamepad_ui_set_self_nav(false);
    if (pad != nullptr) {
        gameMouseSetCursor(MOUSE_CURSOR_ARROW);
    }

    if (oldCursorIsHidden) {
        mouseHideCursor();
    }

    _in_main_menu = false;

    return rc;
}

// NOTE: Inlined.
//
// 0x481C88
static int main_menu_fatal_error()
{
    mainMenuWindowFree();

    return -1;
}

// NOTE: Inlined.
//
// 0x481C94
static void main_menu_play_sound(const char* fileName)
{
    soundPlayFile(fileName);
}

// Redraw a single menu option's label in the given colour. The labels are solid glyph text at fixed
// positions, so redrawing the same string at the same spot in a new colour cleanly overwrites the
// old glyphs -- the highlight moves without touching the background.
static void main_menu_draw_option(int index, int color)
{
    const char* label = main_menu_label(index);
    if (label != nullptr) {
        int oldFont = fontGetCurrent();
        fontSetCurrent(104);
        int len = fontGetStringWidth(label);
        fontDrawText(gMainMenuWindowBuffer + gMainMenuLabelOffsetX + 640 * (gMainMenuLabelOffsetY + 42 * index - index + 20) + 126 - (len / 2),
            label,
            640 - (126 - (len / 2)) - 1,
            640,
            color);
        fontSetCurrent(oldFont);
    }
}

// Move the controller selection highlight to `sel`. Two cues, because the recolour alone did not
// show reliably: (1) every label recoloured, the chosen one red; (2) the chosen row's button in its
// PRESSED (menudown) image, the rest un-pressed -- through the button system's own draw.
static void main_menu_gamepad_highlight(int sel)
{
    for (int index = 0; index < MAIN_MENU_BUTTON_COUNT; index++) {
        main_menu_draw_option(index, index == sel ? _colorTable[32074] : gMainMenuLabelColor);
        if (gMainMenuButtons[index] != -1) {
            _win_register_button_image(gMainMenuButtons[index],
                index == sel ? _mainMenuButtonPressedFrmImage.getData() : _mainMenuButtonNormalFrmImage.getData(),
                _mainMenuButtonPressedFrmImage.getData(),
                nullptr,
                true);
        }
    }
}

// Auto-hide the menu cursor (controller only): hidden by default, revealed while the right stick or
// the real mouse moves it, re-hidden after MAIN_MENU_CURSOR_IDLE_MS of no motion.
#define MAIN_MENU_CURSOR_IDLE_MS 2000

static void main_menu_tick_cursor(bool* shown, int* lastX, int* lastY, unsigned int* lastMove)
{
    int x;
    int y;
    mouseGetPosition(&x, &y);

    if (x != *lastX || y != *lastY) {
        *lastMove = getTicks();
        if (!*shown) {
            gameMouseSetCursor(MOUSE_CURSOR_ARROW);
            *shown = true;
        }
    } else if (*shown && getTicksSince(*lastMove) >= MAIN_MENU_CURSOR_IDLE_MS) {
        gameMouseSetCursor(MOUSE_CURSOR_NONE);
        *shown = false;
    }

    *lastX = x;
    *lastY = y;
}

} // namespace fallout
