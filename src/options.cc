#include "options.h"

#include "art.h"
#include "color.h"
#include "cycle.h"
#include "debug.h"
#include "draw.h"
#include "game.h"
#include "game_mouse.h"
#include "game_sound.h"
#include "graph_lib.h"
#include "input.h"
#include "kb.h"
#include "loadsave.h"
#include "memory.h"
#include "message.h"
#include "mouse.h"
#include "preferences.h"
#include "svga.h"
#include "text_font.h"
#include "tile.h"
#include "window_manager.h"

#include <SDL.h>

#include "rt/game/game_trace.h"
#include "rt/game/gamepad_ui.h"
#include "rt/rt_hooks.h"

namespace fallout {

#define OPTIONS_WINDOW_BUTTONS_COUNT (10)

typedef enum PauseWindowFrm {
    PAUSE_WINDOW_FRM_BACKGROUND,
    PAUSE_WINDOW_FRM_DONE_BOX,
    PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_UP,
    PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_DOWN,
    PAUSE_WINDOW_FRM_COUNT,
} PauseWindowFrm;

typedef enum OptionsWindowFrm {
    OPTIONS_WINDOW_FRM_BACKGROUND,
    OPTIONS_WINDOW_FRM_BUTTON_ON,
    OPTIONS_WINDOW_FRM_BUTTON_OFF,
    OPTIONS_WINDOW_FRM_COUNT,
} OptionsWindowFrm;

static int optionsWindowInit();
static int optionsWindowFree();
static void _ShadeScreen(bool a1);

// 0x48FC0C
static const int gPauseWindowFrmIds[PAUSE_WINDOW_FRM_COUNT] = {
    208, // charwin.frm - character editor
    209, // donebox.frm - character editor
    8, // lilredup.frm - little red button up
    9, // lilreddn.frm - little red button down
};

// 0x5197C0
static const int gOptionsWindowFrmIds[OPTIONS_WINDOW_FRM_COUNT] = {
    220, // opbase.frm - character editor
    222, // opbtnon.frm - character editor
    221, // opbtnoff.frm - character editor
};

// 0x6637E8
static MessageList gPreferencesMessageList;

// 0x663840
static MessageListItem gPreferencesMessageListItem;

// 0x663878
static unsigned char* _opbtns[OPTIONS_WINDOW_BUTTONS_COUNT];

// 0x6638FC
static bool gOptionsWindowGameMouseObjectsWasVisible;

// 0x663900
static int gOptionsWindow;

// 0x663908
static unsigned char* gOptionsWindowBuffer;

// 0x66398C
static int gOptionsWindowOldFont;

// 0x663994
static bool gOptionsWindowIsoWasEnabled;

static FrmImage _optionsFrmImages[OPTIONS_WINDOW_FRM_COUNT];

// Button ids of the option rows (Save / Load / Preferences / Exit / Done), so the pad nav can show the
// selected one by swapping its up-image to the "on" art -- which survives the button refresh, unlike
// drawing into the window buffer.
#define OPTIONS_MENU_COUNT (OPTIONS_WINDOW_BUTTONS_COUNT / 2)
static int gOptionsMenuButtons[OPTIONS_MENU_COUNT] = { -1, -1, -1, -1, -1 };

// Show option `sel` highlighted (its "on" art as the resting image) and the rest normal.
static void options_gamepad_highlight(int sel)
{
    for (int i = 0; i < OPTIONS_MENU_COUNT; i++) {
        if (gOptionsMenuButtons[i] == -1) {
            continue;
        }
        unsigned char* up = (i == sel) ? _opbtns[i * 2 + 1] : _opbtns[i * 2];
        _win_register_button_image(gOptionsMenuButtons[i], up, _opbtns[i * 2 + 1], nullptr, true);
    }
}

// Auto-hide the options cursor (controller only): hidden by default, revealed while the right stick
// or the mouse moves it, re-hidden after idle.
#define OPTIONS_CURSOR_IDLE_MS 2000

static void options_tick_cursor(bool* shown, int* lastX, int* lastY, unsigned int* lastMove)
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
    } else if (*shown && getTicksSince(*lastMove) >= OPTIONS_CURSOR_IDLE_MS) {
        gameMouseSetCursor(MOUSE_CURSOR_NONE);
        *shown = false;
    }

    *lastX = x;
    *lastY = y;
}

// --- CONTROLS FROM THE PAUSE MENU (Fallout 2 Realtime, approved 2026-11-10) ------------------------------
//
// The pad's CONTROLS list lived only in the Pip-Boy's archives, and a new Fallout 2 character has no Pip-Boy
// until the Temple of Trials is behind him, so the first hours had no way to look a button up. With a pad, Y
// here shows the same list in a window of its own (the Pip-Boy's rows and Deck names, read from pipboy.cc,
// drawn like the MODS page), and "Y Controls" under the buttons says so.

#define OPTIONS_CONTROLS_PAD 14
#define OPTIONS_CONTROLS_GAP 12

// "Y Controls" under DONE: the bar's Y badge, the word in the menu's gold (a resting button's label colour).
// The strip between the last button and the panel's frame (y 199-213 on opbase.frm, between the corner
// rivets) is bare art no button repaints. The menu's label font (103) is 16 px tall and that strip 15, so the
// word is in the small interface font, its 7 px capitals centred on the badge.
static void options_draw_pad_hint()
{
    int width = _optionsFrmImages[OPTIONS_WINDOW_FRM_BACKGROUND].getWidth();
    int height = _optionsFrmImages[OPTIONS_WINDOW_FRM_BACKGROUND].getHeight();
    int buttonHeight = _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getHeight();

    // The rows start at y 17 and step by a button and 3 px (optionsWindowInit); the badge's disc is 15 px.
    int cy = 17 + OPTIONS_MENU_COUNT * (buttonHeight + 3) - 3 + 7;
    if (gOptionsWindowBuffer == nullptr || cy + 7 >= height) {
        return;
    }

    const char* word = "Controls";
    int oldFont = fontGetCurrent();
    fontSetCurrent(101);
    int wordWidth = fontGetStringWidth(word) + 1;
    int x = (width - (17 + wordWidth)) / 2;
    if (x >= 0) {
        intface_pad_face(gOptionsWindowBuffer, width, width, height, x + 7, cy, 'Y');
        fontDrawText(gOptionsWindowBuffer + width * (cy - 3) + x + 17, word, wordWidth, width, _colorTable[18979] | FONT_SHADOW);
    }
    fontSetCurrent(oldFont);
}

// The window: the title, a rule, then button | action rows as the Pip-Boy page lays them out (the actions
// start where the widest button ends, plus a gap). Modal, on top of this menu, which stays up underneath.
static void options_show_controls()
{
    int count = pipboy_controls_count();
    if (count <= 0) {
        return;
    }

    int oldFont = fontGetCurrent();
    fontSetCurrent(101);
    int lineHeight = fontGetLineHeight();
    const char* title = pipboy_controls_title();

    char button[64];
    char action[96];

    // The actions' column from the widest button, the window's width from the widest row.
    int column = 0;
    int spacers = 0;
    for (int index = 0; index < count; index++) {
        if (!pipboy_controls_row(index, button, sizeof(button), action, sizeof(action))) {
            spacers++;
            continue;
        }
        int buttonWidth = fontGetStringWidth(button);
        if (buttonWidth > column) {
            column = buttonWidth;
        }
    }
    column += OPTIONS_CONTROLS_GAP;

    int titleWidth = fontGetStringWidth(title);
    int textWidth = titleWidth;
    for (int index = 0; index < count; index++) {
        if (pipboy_controls_row(index, button, sizeof(button), action, sizeof(action))) {
            int rowWidth = (button[0] != '\0' ? column : 0) + fontGetStringWidth(action);
            if (rowWidth > textWidth) {
                textWidth = rowWidth;
            }
        }
    }

    // Rows 2 px apart. On a screen too short for that, at the line height with the blank spacers dropped -- the
    // Pip-Boy page drops those first too.
    int ruleY = OPTIONS_CONTROLS_PAD + lineHeight + 4;
    int firstY = ruleY + 8;
    int pitch = lineHeight + 2;
    bool keepSpacers = true;
    int height = firstY + count * pitch - (pitch - lineHeight) + OPTIONS_CONTROLS_PAD;
    if (height > screenGetHeight() - 20) {
        pitch = lineHeight;
        keepSpacers = false;
        height = firstY + (count - spacers) * pitch + OPTIONS_CONTROLS_PAD;
    }
    int width = OPTIONS_CONTROLS_PAD + textWidth + OPTIONS_CONTROLS_PAD;

    int win = windowCreate((screenGetWidth() - width) / 2,
        (screenGetHeight() - height) / 2,
        width,
        height,
        256,
        WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    if (win == -1) {
        fontSetCurrent(oldFont);
        return;
    }

    unsigned char* buf = windowGetBuffer(win);
    bufferFill(buf, width, height, width, _colorTable[0]);
    bufferDrawRect(buf, width, 0, 0, width - 1, height - 1, _colorTable[992]);
    fontDrawText(buf + width * OPTIONS_CONTROLS_PAD + (width - titleWidth) / 2, title, titleWidth, width, _colorTable[32767]);
    bufferDrawLine(buf, width, OPTIONS_CONTROLS_PAD, ruleY, width - OPTIONS_CONTROLS_PAD - 1, ruleY, _colorTable[992]);

    int y = firstY;
    for (int index = 0; index < count; index++) {
        if (!pipboy_controls_row(index, button, sizeof(button), action, sizeof(action))) {
            if (keepSpacers) {
                y += pitch;
            }
            continue;
        }

        unsigned char* row = buf + width * y + OPTIONS_CONTROLS_PAD;
        if (button[0] == '\0') {
            // A heading, in the Pip-Boy page's heading colour.
            fontDrawText(row, action, textWidth, width, _colorTable[32747] | FONT_UNDERLINE);
        } else {
            fontDrawText(row, button, column, width, _colorTable[992]);
            fontDrawText(row + column, action, textWidth - column, width, _colorTable[992]);
        }
        y += pitch;
    }

    windowRefresh(win);
    trace_event(TRACE_FLOW, "controls-open", "rows=%d size=%dx%d", count - spacers, width, height);

    // B or Y puts it away, ESC on a keyboard -- and START, which gamepad_ui.cc turns into ESC while this menu is
    // up. ⚠️ Each pad button must be seen RELEASED first (rt_pad_pressed): the Y that opened it is still down.
    bool armedB = false;
    bool armedY = false;
    const char* closedBy = nullptr;
    while (closedBy == nullptr) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();
        if (keyCode == KEY_ESCAPE) {
            closedBy = "esc";
        } else if (_game_user_wants_to_quit != 0) {
            closedBy = "quit";
        }

        if (closedBy == nullptr && gamepad_ui_get_controller() != nullptr) {
            SDL_GameControllerUpdate();
            if (rt_pad_pressed(SDL_CONTROLLER_BUTTON_B, &armedB)) {
                closedBy = "b";
            } else if (rt_pad_pressed(SDL_CONTROLLER_BUTTON_Y, &armedY)) {
                closedBy = "y";
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    windowDestroy(win);
    fontSetCurrent(oldFont);
    trace_event(TRACE_FLOW, "controls-closed", "by=%s", closedBy);

    // ⚠️ The B or Y that closed it is still down as the menu resumes: nothing queued here may reach the menu, and
    // every edge tracker adopts the live pad -- the menu's own B would otherwise read a fresh press and shut it.
    inputEventQueueReset();
    gamepad_ui_bump_input_epoch();
}

// 0x48FC50
int showOptions()
{
    ScopedGameMode gm(GameMode::kOptions);

    if (optionsWindowInit() == -1) {
        debugPrint("\nOPTION MENU: Error loading option dialog data!\n");
        return -1;
    }

    // Gamepad navigation: highlight the selected row and move it with the D-pad / left stick, confirm
    // with A, close with B -- the main menu's feel, without the jumpy snap cursor. Y shows the CONTROLS.
    // ⚠️ THE PAD IS LOOKED FOR EVERY FRAME, NOT LATCHED AT ENTRY (prefs-gamepad-nav; review 2026-11-10): one
    // that comes in while the menu is up gets the pad nav, the hint and Y's CONTROLS from that frame on.
    // `optnPadSetUp` says the set-up in the loop has run, and owes the clean-up on the way out.
    bool optnPadSetUp = false;
    int optnSel = 0;
    bool optnPrevUp = false;
    bool optnPrevDown = false;
    bool optnPrevA = false;
    bool optnPrevB = false;
    bool optnPrevY = false;
    bool optnCursorShown = true;
    int optnCursorX = 0;
    int optnCursorY = 0;
    unsigned int optnCursorMove = getTicks();

    unsigned int optnEpoch = gamepad_ui_input_epoch();

    int rc = -1;
    while (rc == -1) {
        sharedFpsLimiter.mark();

        // ⚠️ The handle is fetched again every frame, never kept from the entry: gamepad_ui_get_controller()
        // closes a pad that went away and opens the next one, so a kept handle can dangle (prefs-gamepad-nav).
        SDL_GameController* pad = gamepad_ui_get_controller();
        if (pad != nullptr && !optnPadSetUp) {
            // The first frame with a pad: the menu's opening frame, or the one a pad arrives on.
            optnPadSetUp = true;
            gamepad_ui_set_self_nav(true);
            options_gamepad_highlight(optnSel);
            options_draw_pad_hint();
            windowRefresh(gOptionsWindow);
            mouseGetPosition(&optnCursorX, &optnCursorY);
            gameMouseSetCursor(MOUSE_CURSOR_NONE);
            optnCursorShown = false;

            // Seeded as HELD: the button that opened this menu is very likely still down, and nothing down
            // as a pad arrives is a press.
            SDL_GameControllerUpdate();
            optnPrevA = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A) != 0;
            optnPrevB = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B) != 0;
            optnPrevY = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_Y) != 0;
            optnPrevUp = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0;
            optnPrevDown = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0;
        }
        if (pad != nullptr) {
            // Re-assert after a sub-screen (Save / Load / Preferences run their own loops and clear
            // self_nav / restore the cursor on exit).
            gamepad_ui_set_self_nav(true);

            SDL_GameControllerUpdate();
            bool upNow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0
                || SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY) < -16000;
            bool downNow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0
                || SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY) > 16000;
            bool aNow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A) != 0;
            bool bNow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B) != 0;
            bool yNow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_Y) != 0;

            // A sub-screen ran its own loop: the button that closed it is still down. Adopt the live
            // state, act on nothing this frame. (Y is DONE on Preferences, so it often comes back held.)
            unsigned int epoch = gamepad_ui_input_epoch();
            if (epoch != optnEpoch) {
                optnEpoch = epoch;
                optnPrevUp = upNow;
                optnPrevDown = downNow;
                optnPrevA = aNow;
                optnPrevB = bNow;
                optnPrevY = yNow;
                options_gamepad_highlight(optnSel);
                windowRefresh(gOptionsWindow);
                gameMouseSetCursor(MOUSE_CURSOR_NONE);
                optnCursorShown = false;
            }

            if (upNow && !optnPrevUp) {
                optnSel = (optnSel + OPTIONS_MENU_COUNT - 1) % OPTIONS_MENU_COUNT;
                options_gamepad_highlight(optnSel);
                windowRefresh(gOptionsWindow);
                soundPlayFile("ib1p1xx1");
            } else if (downNow && !optnPrevDown) {
                optnSel = (optnSel + 1) % OPTIONS_MENU_COUNT;
                options_gamepad_highlight(optnSel);
                windowRefresh(gOptionsWindow);
                soundPlayFile("ib1p1xx1");
            }

            if (aNow && !optnPrevA) {
                // The highlighted row's own event code (500 + index), handled by the switch below.
                enqueueInputEvent(500 + optnSel);
            } else if (bNow && !optnPrevB) {
                enqueueInputEvent(KEY_ESCAPE);
            } else if (yNow && !optnPrevY) {
                // The CONTROLS window, over this menu. It bumps the input epoch on the way out, so the next
                // frame adopts whatever it was closed with.
                soundPlayFile("ib1p1xx1");
                options_show_controls();
            }

            optnPrevUp = upNow;
            optnPrevDown = downNow;
            optnPrevA = aNow;
            optnPrevB = bNow;
            optnPrevY = yNow;

            options_tick_cursor(&optnCursorShown, &optnCursorX, &optnCursorY, &optnCursorMove);
        }

        int keyCode = inputGetInput();

        if (keyCode == KEY_ESCAPE || keyCode == 504 || _game_user_wants_to_quit != 0) {
            rc = 0;
        } else {
            switch (keyCode) {
            case KEY_RETURN:
            case KEY_UPPERCASE_O:
            case KEY_LOWERCASE_O:
            case KEY_UPPERCASE_D:
            case KEY_LOWERCASE_D:
                soundPlayFile("ib1p1xx1");
                rc = 0;
                break;
            case KEY_UPPERCASE_S:
            case KEY_LOWERCASE_S:
            case 500:
                if (lsgSaveGame(LOAD_SAVE_MODE_NORMAL) == 1) {
                    rc = 1;
                }
                break;
            case KEY_UPPERCASE_L:
            case KEY_LOWERCASE_L:
            case 501:
                if (lsgLoadGame(LOAD_SAVE_MODE_NORMAL) == 1) {
                    rc = 1;
                }
                break;
            case KEY_UPPERCASE_P:
            case KEY_LOWERCASE_P:
                soundPlayFile("ib1p1xx1");
                // FALLTHROUGH
            case 502:
                // PREFERENCES
                doPreferences(false);
                break;
            case KEY_PLUS:
            case KEY_EQUAL:
                brightnessIncrease();
                break;
            case KEY_UNDERSCORE:
            case KEY_MINUS:
                brightnessDecrease();
                break;
            case KEY_F12:
                takeScreenshot();
                break;
            case KEY_UPPERCASE_E:
            case KEY_LOWERCASE_E:
            case KEY_CTRL_Q:
            case KEY_CTRL_X:
            case KEY_F10:
            case 503:
                showQuitConfirmationDialog();
                break;
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    if (optnPadSetUp) {
        gamepad_ui_set_self_nav(false);
        gameMouseSetCursor(MOUSE_CURSOR_ARROW);
    }

    optionsWindowFree();

    // ⚠️ DRAIN: this menu's pad injects 500 + row, and those exact numbers mean DONE / PRINT / CANCEL
    // in the character editor, which can open on the very next frame. And the B that closed it is
    // still held as the game resumes -- the same counter the load/save screen and the boxes use.
    inputEventQueueReset();
    gamepad_ui_bump_input_epoch();

    return rc;
}

// 0x48FE14
static int optionsWindowInit()
{
    gOptionsWindowOldFont = fontGetCurrent();

    if (!messageListInit(&gPreferencesMessageList)) {
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", asc_5186C8, "options.msg");
    if (!messageListLoad(&gPreferencesMessageList, path)) {
        return -1;
    }

    for (int index = 0; index < OPTIONS_WINDOW_FRM_COUNT; index++) {
        int fid = buildFid(OBJ_TYPE_INTERFACE, gOptionsWindowFrmIds[index], 0, 0, 0);
        if (!_optionsFrmImages[index].lock(fid)) {
            while (--index >= 0) {
                _optionsFrmImages[index].unlock();
            }

            messageListFree(&gPreferencesMessageList);

            return -1;
        }
    }

    int cycle = 0;
    for (int index = 0; index < OPTIONS_WINDOW_BUTTONS_COUNT; index++) {
        _opbtns[index] = (unsigned char*)internal_malloc(_optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth() * _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getHeight() + 1024);
        if (_opbtns[index] == nullptr) {
            while (--index >= 0) {
                internal_free(_opbtns[index]);
            }

            for (int index = 0; index < OPTIONS_WINDOW_FRM_COUNT; index++) {
                _optionsFrmImages[index].unlock();
            }

            messageListFree(&gPreferencesMessageList);

            return -1;
        }

        cycle = cycle ^ 1;

        memcpy(_opbtns[index], _optionsFrmImages[cycle + 1].getData(), _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth() * _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getHeight());
    }

    int optionsWindowX = (screenGetWidth() - _optionsFrmImages[OPTIONS_WINDOW_FRM_BACKGROUND].getWidth()) / 2;
    int optionsWindowY = (screenGetHeight() - _optionsFrmImages[OPTIONS_WINDOW_FRM_BACKGROUND].getHeight()) / 2 - 60;
    gOptionsWindow = windowCreate(optionsWindowX,
        optionsWindowY,
        _optionsFrmImages[0].getWidth(),
        _optionsFrmImages[0].getHeight(),
        256,
        WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);

    if (gOptionsWindow == -1) {
        for (int index = 0; index < OPTIONS_WINDOW_BUTTONS_COUNT; index++) {
            internal_free(_opbtns[index]);
        }

        for (int index = 0; index < OPTIONS_WINDOW_FRM_COUNT; index++) {
            _optionsFrmImages[index].unlock();
        }

        messageListFree(&gPreferencesMessageList);

        return -1;
    }

    gOptionsWindowIsoWasEnabled = isoDisable();

    gOptionsWindowGameMouseObjectsWasVisible = gameMouseObjectsIsVisible();
    if (gOptionsWindowGameMouseObjectsWasVisible) {
        gameMouseObjectsHide();
    }

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    gOptionsWindowBuffer = windowGetBuffer(gOptionsWindow);
    memcpy(gOptionsWindowBuffer, _optionsFrmImages[OPTIONS_WINDOW_FRM_BACKGROUND].getData(), _optionsFrmImages[OPTIONS_WINDOW_FRM_BACKGROUND].getWidth() * _optionsFrmImages[OPTIONS_WINDOW_FRM_BACKGROUND].getHeight());

    fontSetCurrent(103);

    int textY = (_optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getHeight() - fontGetLineHeight()) / 2 + 1;
    int buttonY = 17;

    for (int index = 0; index < OPTIONS_WINDOW_BUTTONS_COUNT; index += 2) {
        char text[128];

        const char* msg = getmsg(&gPreferencesMessageList, &gPreferencesMessageListItem, index / 2);
        strcpy(text, msg);

        int textX = (_optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth() - fontGetStringWidth(text)) / 2;
        if (textX < 0) {
            textX = 0;
        }

        fontDrawText(_opbtns[index] + _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth() * textY + textX, text, _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth(), _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth(), _colorTable[18979]);
        fontDrawText(_opbtns[index + 1] + _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth() * textY + textX, text, _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth(), _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth(), _colorTable[14723]);

        int btn = buttonCreate(gOptionsWindow,
            13,
            buttonY,
            _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getWidth(),
            _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getHeight(),
            -1,
            -1,
            -1,
            index / 2 + 500,
            _opbtns[index],
            _opbtns[index + 1],
            nullptr,
            BUTTON_FLAG_TRANSPARENT);
        gOptionsMenuButtons[index / 2] = btn;
        if (btn != -1) {
            buttonSetCallbacks(btn, _gsound_lrg_butt_press, _gsound_lrg_butt_release);
        }

        buttonY += _optionsFrmImages[OPTIONS_WINDOW_FRM_BUTTON_ON].getHeight() + 3;
    }

    fontSetCurrent(101);

    windowRefresh(gOptionsWindow);

    return 0;
}

// 0x490244
static int optionsWindowFree()
{
    windowDestroy(gOptionsWindow);
    fontSetCurrent(gOptionsWindowOldFont);
    messageListFree(&gPreferencesMessageList);

    for (int index = 0; index < OPTIONS_WINDOW_BUTTONS_COUNT; index++) {
        internal_free(_opbtns[index]);
    }

    for (int index = 0; index < OPTIONS_WINDOW_FRM_COUNT; index++) {
        _optionsFrmImages[index].unlock();
    }

    if (gOptionsWindowGameMouseObjectsWasVisible) {
        gameMouseObjectsShow();
    }

    if (gOptionsWindowIsoWasEnabled) {
        isoEnable();
    }

    return 0;
}

// 0x4902B0
int showPause(bool a1)
{
    bool gameMouseWasVisible;
    if (!a1) {
        gOptionsWindowIsoWasEnabled = isoDisable();
        colorCycleDisable();

        gameMouseWasVisible = gameMouseObjectsIsVisible();
        if (gameMouseWasVisible) {
            gameMouseObjectsHide();
        }
    }

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
    _ShadeScreen(a1);

    FrmImage frmImages[PAUSE_WINDOW_FRM_COUNT];
    for (int index = 0; index < PAUSE_WINDOW_FRM_COUNT; index++) {
        int fid = buildFid(OBJ_TYPE_INTERFACE, gPauseWindowFrmIds[index], 0, 0, 0);
        if (!frmImages[index].lock(fid)) {
            debugPrint("\n** Error loading pause window graphics! **\n");
            return -1;
        }
    }

    if (!messageListInit(&gPreferencesMessageList)) {
        // FIXME: Leaking graphics.
        return -1;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", asc_5186C8, "options.msg");
    if (!messageListLoad(&gPreferencesMessageList, path)) {
        // FIXME: Leaking graphics.
        return -1;
    }

    int pauseWindowX = (screenGetWidth() - frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth()) / 2;
    int pauseWindowY = (screenGetHeight() - frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getHeight()) / 2;

    if (a1) {
        pauseWindowX -= 65;
        pauseWindowY -= 24;
    } else {
        pauseWindowY -= 54;
    }

    int window = windowCreate(pauseWindowX,
        pauseWindowY,
        frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth(),
        frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getHeight(),
        256,
        WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);
    if (window == -1) {
        messageListFree(&gPreferencesMessageList);

        debugPrint("\n** Error opening pause window! **\n");
        return -1;
    }

    unsigned char* windowBuffer = windowGetBuffer(window);
    memcpy(windowBuffer,
        frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getData(),
        frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth() * frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getHeight());

    blitBufferToBufferTrans(frmImages[PAUSE_WINDOW_FRM_DONE_BOX].getData(),
        frmImages[PAUSE_WINDOW_FRM_DONE_BOX].getWidth(),
        frmImages[PAUSE_WINDOW_FRM_DONE_BOX].getHeight(),
        frmImages[PAUSE_WINDOW_FRM_DONE_BOX].getWidth(),
        windowBuffer + frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth() * 42 + 13,
        frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth());

    gOptionsWindowOldFont = fontGetCurrent();
    fontSetCurrent(103);

    char* messageItemText;

    messageItemText = getmsg(&gPreferencesMessageList, &gPreferencesMessageListItem, 300);
    fontDrawText(windowBuffer + frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth() * 45 + 52,
        messageItemText,
        frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth(),
        frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth(),
        _colorTable[18979]);

    fontSetCurrent(104);

    messageItemText = getmsg(&gPreferencesMessageList, &gPreferencesMessageListItem, 301);
    strcpy(path, messageItemText);

    int length = fontGetStringWidth(path);
    fontDrawText(windowBuffer + frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth() * 10 + 2 + (frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth() - length) / 2,
        path,
        frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth(),
        frmImages[PAUSE_WINDOW_FRM_BACKGROUND].getWidth(),
        _colorTable[18979]);

    int doneBtn = buttonCreate(window,
        26,
        46,
        frmImages[PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_UP].getWidth(),
        frmImages[PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_UP].getHeight(),
        -1,
        -1,
        -1,
        504,
        frmImages[PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_UP].getData(),
        frmImages[PAUSE_WINDOW_FRM_LITTLE_RED_BUTTON_DOWN].getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (doneBtn != -1) {
        buttonSetCallbacks(doneBtn, _gsound_red_butt_press, _gsound_red_butt_release);
    }

    windowRefresh(window);

    bool done = false;
    while (!done) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();
        switch (keyCode) {
        case KEY_PLUS:
        case KEY_EQUAL:
            brightnessIncrease();
            break;
        case KEY_MINUS:
        case KEY_UNDERSCORE:
            brightnessDecrease();
            break;
        default:
            if (keyCode != -1 && keyCode != -2) {
                done = true;
            }

            if (_game_user_wants_to_quit != 0) {
                done = true;
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    if (!a1) {
        tileWindowRefresh();
    }

    windowDestroy(window);
    messageListFree(&gPreferencesMessageList);

    if (!a1) {
        if (gameMouseWasVisible) {
            gameMouseObjectsShow();
        }

        if (gOptionsWindowIsoWasEnabled) {
            isoEnable();
        }

        colorCycleEnable();

        gameMouseSetCursor(MOUSE_CURSOR_ARROW);
    }

    fontSetCurrent(gOptionsWindowOldFont);

    return 0;
}

// 0x490748
static void _ShadeScreen(bool a1)
{
    if (a1) {
        mouseHideCursor();
    } else {
        mouseHideCursor();
        tileWindowRefresh();

        int windowWidth = windowGetWidth(gIsoWindow);
        int windowHeight = windowGetHeight(gIsoWindow);
        unsigned char* windowBuffer = windowGetBuffer(gIsoWindow);
        grayscalePaletteApply(windowBuffer, windowWidth, windowHeight, windowWidth);

        windowRefresh(gIsoWindow);
    }

    mouseShowCursor();
}

// init_options_menu
// 0x4928B8
int _init_options_menu()
{
    preferencesInit();

    grayscalePaletteUpdate(0, 255);

    return 0;
}

} // namespace fallout
