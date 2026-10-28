#include "character_selector.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <vector>

#include "art.h"
#include "character_editor.h"
#include "color.h"
#include "critter.h"
#include "db.h"
#include "debug.h"
#include "draw.h"
#include "game.h"
#include "game_sound.h"
#include "input.h"
#include "kb.h"
#include "memory.h"
#include "message.h"
#include "mouse.h"
#include "object.h"
#include "palette.h"
#include "platform_compat.h"
#include "preferences.h"
#include "proto.h"
#include "settings.h"
#include "sfall_config.h"
#include "skill.h"

#include <SDL.h>

// Realtime / controller (fallout1-ce's select.cc changes, ported).
#include "rt/game/gamepad_ui.h"
#include "stat.h"
#include "svga.h"
#include "text_font.h"
#include "trait.h"
#include "window_manager.h"

namespace fallout {

#define CS_WINDOW_WIDTH (640)
#define CS_WINDOW_HEIGHT (480)

#define CS_WINDOW_BACKGROUND_X (40)
#define CS_WINDOW_BACKGROUND_Y (30)
#define CS_WINDOW_BACKGROUND_WIDTH (560)
#define CS_WINDOW_BACKGROUND_HEIGHT (300)

#define CS_WINDOW_PREVIOUS_BUTTON_X (292)
#define CS_WINDOW_PREVIOUS_BUTTON_Y (320)

#define CS_WINDOW_NEXT_BUTTON_X (318)
#define CS_WINDOW_NEXT_BUTTON_Y (320)

#define CS_WINDOW_TAKE_BUTTON_X (81)
#define CS_WINDOW_TAKE_BUTTON_Y (323)

#define CS_WINDOW_MODIFY_BUTTON_X (435)
#define CS_WINDOW_MODIFY_BUTTON_Y (320)

#define CS_WINDOW_CREATE_BUTTON_X (80)
#define CS_WINDOW_CREATE_BUTTON_Y (425)

#define CS_WINDOW_BACK_BUTTON_X (461)
#define CS_WINDOW_BACK_BUTTON_Y (425)

#define CS_WINDOW_NAME_MID_X (318)
#define CS_WINDOW_PRIMARY_STAT_MID_X (362)
#define CS_WINDOW_SECONDARY_STAT_MID_X (379)
#define CS_WINDOW_BIO_X (438)

typedef enum PremadeCharacter {
    PREMADE_CHARACTER_NARG,
    PREMADE_CHARACTER_CHITSA,
    PREMADE_CHARACTER_MINGUN,
    PREMADE_CHARACTER_COUNT,
} PremadeCharacter;

typedef struct PremadeCharacterDescription {
    char fileName[20];
    int face;
    char field_18[20];
} PremadeCharacterDescription;

static bool characterSelectorWindowInit();
static void characterSelectorWindowFree();
static bool characterSelectorWindowRefresh();
static bool characterSelectorWindowRenderFace();
static bool characterSelectorWindowRenderStats();
static bool characterSelectorWindowRenderBio();
static bool characterSelectorWindowFatalError(bool result);

static void premadeCharactersLocalizePath(char* path);

// 0x51C84C
static int gCurrentPremadeCharacter = PREMADE_CHARACTER_NARG;

// 0x51C850
static PremadeCharacterDescription gPremadeCharacterDescriptions[PREMADE_CHARACTER_COUNT] = {
    { "premade\\combat", 201, "VID 208-197-88-125" },
    { "premade\\stealth", 202, "VID 208-206-49-229" },
    { "premade\\diplomat", 203, "VID 208-206-49-227" },
};

// 0x51C8D4
static int gPremadeCharacterCount = PREMADE_CHARACTER_COUNT;

// 0x51C7F8
// Realtime build: not static -- the gamepad layer snaps the stick between this screen's buttons
// (rt/f1compat.h maps F1's select_window_id here).
int gCharacterSelectorWindow = -1;

// 0x51C7FC
static unsigned char* gCharacterSelectorWindowBuffer = nullptr;

// 0x51C800
static unsigned char* gCharacterSelectorBackground = nullptr;

// 0x51C804
static int gCharacterSelectorWindowPreviousButton = -1;

// 0x51C810
static int gCharacterSelectorWindowNextButton = -1;

// 0x51C81C
static int gCharacterSelectorWindowTakeButton = -1;

// 0x51C828
static int gCharacterSelectorWindowModifyButton = -1;

// 0x51C834
static int gCharacterSelectorWindowCreateButton = -1;

// 0x51C840
static int gCharacterSelectorWindowBackButton = -1;

static FrmImage _takeButtonNormalFrmImage;
static FrmImage _takeButtonPressedFrmImage;
static FrmImage _modifyButtonNormalFrmImage;
static FrmImage _modifyButtonPressedFrmImage;
static FrmImage _createButtonNormalFrmImage;
static FrmImage _createButtonPressedFrmImage;
static FrmImage _backButtonNormalFrmImage;
static FrmImage _backButtonPressedFrmImage;
static FrmImage _nextButtonNormalFrmImage;
static FrmImage _nextButtonPressedFrmImage;
static FrmImage _previousButtonNormalFrmImage;
static FrmImage _previousButtonPressedFrmImage;

static std::vector<PremadeCharacterDescription> gCustomPremadeCharacterDescriptions;

// CONTROLLER NAVIGATION for the character SELECTION screen -- the first thing New Game shows. Every
// clickable thing is a stop, the arrows under the portrait included; the D-pad / stick steps the
// focus SPATIALLY, A presses the focused button (the very code the button sends), B backs out.
//
// The indicator is a 1px frame in Fallout's selection yellow, drawn through a SAVE-UNDER (the labels
// are painted once and never repainted, so anything erased against the background would eat them),
// hence the strict erase -> input -> draw order in the loop.
struct SelFocusItem {
    int x;
    int y;
    int w;
    int h;
    int code; // what pressing A sends -- the same code the on-screen button emits
};

static const SelFocusItem sel_items[] = {
    { CS_WINDOW_TAKE_BUTTON_X, CS_WINDOW_TAKE_BUTTON_Y, 15, 16, KEY_LOWERCASE_T },
    { CS_WINDOW_PREVIOUS_BUTTON_X, CS_WINDOW_PREVIOUS_BUTTON_Y, 20, 18, 500 },
    { CS_WINDOW_NEXT_BUTTON_X, CS_WINDOW_NEXT_BUTTON_Y, 20, 18, 501 },
    { CS_WINDOW_MODIFY_BUTTON_X, CS_WINDOW_MODIFY_BUTTON_Y, 15, 16, KEY_LOWERCASE_M },
    { CS_WINDOW_CREATE_BUTTON_X, CS_WINDOW_CREATE_BUTTON_Y, 15, 16, KEY_LOWERCASE_C },
    { CS_WINDOW_BACK_BUTTON_X, CS_WINDOW_BACK_BUTTON_Y, 15, 16, KEY_ESCAPE },
};

#define SEL_FOCUS_COUNT ((int)(sizeof(sel_items) / sizeof(sel_items[0])))
#define SEL_FOCUS_TAKE 0

#define SEL_HL_MAX_W 40
#define SEL_HL_MAX_H 40

static unsigned char sel_hl_top[SEL_HL_MAX_W];
static unsigned char sel_hl_bottom[SEL_HL_MAX_W];
static unsigned char sel_hl_left[SEL_HL_MAX_H];
static unsigned char sel_hl_right[SEL_HL_MAX_H];
static int sel_hl_x = -1;
static int sel_hl_y;
static int sel_hl_w;
static int sel_hl_h;
static int sel_focus = SEL_FOCUS_TAKE;

// Three pixels of air around the focused item.
static void sel_focus_bounds(int* x, int* y, int* w, int* h)
{
    const SelFocusItem* it = &(sel_items[sel_focus]);
    *x = it->x - 3;
    *y = it->y - 3;
    *w = it->w + 6;
    *h = it->h + 6;
}

// Drag the POINTER along with the frame: two indicators of the same thing must never disagree.
static void sel_park_cursor()
{
    if (gCharacterSelectorWindow == -1) {
        return;
    }

    Rect rect;
    if (windowGetRect(gCharacterSelectorWindow, &rect) != 0) {
        return;
    }

    const SelFocusItem* it = &(sel_items[sel_focus]);
    _mouse_set_position(rect.left + it->x + it->w / 2, rect.top + it->y + it->h / 2);
}

// SPATIAL step: the nearest item that genuinely lies in the direction pushed. The candidate must move
// at least SEL_STEP_MIN along the pushed axis, and candidates that OVERLAP the current item on the
// other axis win outright; others only within SEL_STEP_SLOP, or the focus stays put.
#define SEL_STEP_MIN 10
#define SEL_STEP_SLOP 40

static void sel_focus_step(int dx, int dy)
{
    const SelFocusItem* cur = &(sel_items[sel_focus]);
    int cx = cur->x + cur->w / 2;
    int cy = cur->y + cur->h / 2;

    int best = -1;
    int bestScore = 0;
    bool bestOverlaps = false;

    for (int i = 0; i < SEL_FOCUS_COUNT; i++) {
        if (i == sel_focus) {
            continue;
        }

        const SelFocusItem* it = &(sel_items[i]);
        int ix = it->x + it->w / 2;
        int iy = it->y + it->h / 2;

        int along = dx != 0 ? (ix - cx) * dx : (iy - cy) * dy;
        if (along < SEL_STEP_MIN) {
            continue;
        }

        int across = dx != 0 ? (iy - cy) : (ix - cx);
        if (across < 0) {
            across = -across;
        }

        bool overlaps = dx != 0
            ? (it->y < cur->y + cur->h && it->y + it->h > cur->y)
            : (it->x < cur->x + cur->w && it->x + it->w > cur->x);

        if (!overlaps && across > SEL_STEP_SLOP) {
            continue;
        }

        int score = along + 2 * across;

        if (best == -1 || (overlaps && !bestOverlaps)
            || (overlaps == bestOverlaps && score < bestScore)) {
            best = i;
            bestScore = score;
            bestOverlaps = overlaps;
        }
    }

    if (best != -1) {
        sel_focus = best;
        sel_park_cursor();
        soundPlayFile("ib1p1xx1");
    }
}

static void sel_hl_erase()
{
    if (sel_hl_x < 0 || gCharacterSelectorWindow == -1) {
        return;
    }

    unsigned char* buf = windowGetBuffer(gCharacterSelectorWindow);
    if (buf == nullptr) {
        sel_hl_x = -1;
        return;
    }

    memcpy(buf + CS_WINDOW_WIDTH * sel_hl_y + sel_hl_x, sel_hl_top, sel_hl_w);
    memcpy(buf + CS_WINDOW_WIDTH * (sel_hl_y + sel_hl_h - 1) + sel_hl_x, sel_hl_bottom, sel_hl_w);
    for (int i = 0; i < sel_hl_h; i++) {
        unsigned char* row = buf + CS_WINDOW_WIDTH * (sel_hl_y + i);
        row[sel_hl_x] = sel_hl_left[i];
        row[sel_hl_x + sel_hl_w - 1] = sel_hl_right[i];
    }

    sel_hl_x = -1;
}

static void sel_hl_draw()
{
    if (gCharacterSelectorWindow == -1) {
        return;
    }

    unsigned char* buf = windowGetBuffer(gCharacterSelectorWindow);
    if (buf == nullptr) {
        return;
    }

    int x;
    int y;
    int w;
    int h;
    sel_focus_bounds(&x, &y, &w, &h);

    if (x < 0 || y < 0 || w < 3 || h < 3 || w > SEL_HL_MAX_W || h > SEL_HL_MAX_H
        || x + w > CS_WINDOW_WIDTH || y + h > CS_WINDOW_HEIGHT) {
        return;
    }

    // Whole footprint saved before any of it is drawn, or the corners come back yellow.
    memcpy(sel_hl_top, buf + CS_WINDOW_WIDTH * y + x, w);
    memcpy(sel_hl_bottom, buf + CS_WINDOW_WIDTH * (y + h - 1) + x, w);
    for (int i = 0; i < h; i++) {
        const unsigned char* row = buf + CS_WINDOW_WIDTH * (y + i);
        sel_hl_left[i] = row[x];
        sel_hl_right[i] = row[x + w - 1];
    }

    unsigned char color = _colorTable[32747];
    memset(buf + CS_WINDOW_WIDTH * y + x, color, w);
    memset(buf + CS_WINDOW_WIDTH * (y + h - 1) + x, color, w);
    for (int i = 0; i < h; i++) {
        unsigned char* row = buf + CS_WINDOW_WIDTH * (y + i);
        row[x] = color;
        row[x + w - 1] = color;
    }

    sel_hl_x = x;
    sel_hl_y = y;
    sel_hl_w = w;
    sel_hl_h = h;
}

static bool sel_prev_up;
static bool sel_prev_down;
static bool sel_prev_left;
static bool sel_prev_right;
static bool sel_prev_a;
static bool sel_prev_b;

// Latch the CURRENT state as "already seen", so the A that opened this screen (on the main menu)
// does not instantly hit TAKE.
static void sel_pad_prime(SDL_GameController* pad)
{
    SDL_GameControllerUpdate();
    sel_prev_up = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0
        || SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY) < -16000;
    sel_prev_down = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0
        || SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY) > 16000;
    sel_prev_left = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0
        || SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX) < -16000;
    sel_prev_right = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0
        || SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX) > 16000;
    sel_prev_a = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A) != 0;
    sel_prev_b = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B) != 0;
}

static void sel_pad_poll(SDL_GameController* pad)
{
    SDL_GameControllerUpdate();

    bool up = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0
        || SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY) < -16000;
    bool down = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0
        || SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY) > 16000;
    bool left = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0
        || SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX) < -16000;
    bool right = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0
        || SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX) > 16000;
    bool a = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A) != 0;
    bool b = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B) != 0;

    // All four directions move the FOCUS; switching character is A on one of the arrows.
    if (up && !sel_prev_up) {
        sel_focus_step(0, -1);
    }
    if (down && !sel_prev_down) {
        sel_focus_step(0, 1);
    }
    if (left && !sel_prev_left) {
        sel_focus_step(-1, 0);
    }
    if (right && !sel_prev_right) {
        sel_focus_step(1, 0);
    }

    if (a && !sel_prev_a) {
        enqueueInputEvent(sel_items[sel_focus].code);
    }
    if (b && !sel_prev_b) {
        enqueueInputEvent(KEY_ESCAPE);
    }

    sel_prev_up = up;
    sel_prev_down = down;
    sel_prev_left = left;
    sel_prev_right = right;
    sel_prev_a = a;
    sel_prev_b = b;
}

// 0x4A71D0
int characterSelectorOpen()
{
    if (!characterSelectorWindowInit()) {
        return 0;
    }

    bool cursorWasHidden = cursorIsHidden();
    if (cursorWasHidden) {
        mouseShowCursor();
    }

    // Same reason as the character editor: this screen injects its own button codes and must not
    // inherit anyone else's.
    inputEventQueueReset();

    colorPaletteLoad("color.pal");
    paletteFadeTo(_cmap);

    // Re-fetched every frame, never latched once on entry: gamepad_ui opens the controller LAZILY.
    sel_focus = SEL_FOCUS_TAKE;
    sel_hl_x = -1;
    bool selSuppressWas = false;
    {
        SDL_GameController* pad = gamepad_ui_get_controller();
        if (pad != nullptr) {
            sel_pad_prime(pad);
            sel_park_cursor(); // start with the pointer already on the framed item
            // A is ours here (it sends the focused item's own key code); stop the shared router from
            // also firing a raw click at the pointer, and the shared snap from moving it.
            selSuppressWas = gamepad_ui_set_suppress_a_click(true);
            gamepad_ui_set_suppress_snap(true);
        }
    }

    int rc = 0;
    bool done = false;
    while (!done) {
        sharedFpsLimiter.mark();

        if (_game_user_wants_to_quit != 0) {
            break;
        }

        // Erase -> input -> draw, once per frame, which is what keeps the save-under honest.
        SDL_GameController* selPad = gamepad_ui_get_controller();
        if (selPad != nullptr) {
            sel_hl_erase();
            sel_pad_poll(selPad);
        }

        int keyCode = inputGetInput();

        switch (keyCode) {
        case KEY_MINUS:
        case KEY_UNDERSCORE:
            brightnessDecrease();
            break;
        case KEY_EQUAL:
        case KEY_PLUS:
            brightnessIncrease();
            break;
        case KEY_UPPERCASE_B:
        case KEY_LOWERCASE_B:
        case KEY_ESCAPE:
            rc = 3;
            done = true;
            break;
        case KEY_UPPERCASE_C:
        case KEY_LOWERCASE_C:
            _ResetPlayer();
            if (characterEditorShow(1) == 0) {
                rc = 2;
                done = true;
            } else {
                characterSelectorWindowRefresh();
            }

            break;
        case KEY_UPPERCASE_M:
        case KEY_LOWERCASE_M:
            if (!characterEditorShow(1)) {
                rc = 2;
                done = true;
            } else {
                characterSelectorWindowRefresh();
            }

            break;
        case KEY_UPPERCASE_T:
        case KEY_LOWERCASE_T:
            rc = 2;
            done = true;

            break;
        case KEY_F10:
            showQuitConfirmationDialog();
            break;
        case KEY_ARROW_LEFT:
            soundPlayFile("ib2p1xx1");
            // FALLTHROUGH
        case 500:
            gCurrentPremadeCharacter -= 1;
            if (gCurrentPremadeCharacter < 0) {
                gCurrentPremadeCharacter = gPremadeCharacterCount - 1;
            }

            characterSelectorWindowRefresh();
            break;
        case KEY_ARROW_RIGHT:
            soundPlayFile("ib2p1xx1");
            // FALLTHROUGH
        case 501:
            gCurrentPremadeCharacter += 1;
            if (gCurrentPremadeCharacter >= gPremadeCharacterCount) {
                gCurrentPremadeCharacter = 0;
            }

            characterSelectorWindowRefresh();
            break;
        }

        if (selPad != nullptr && !done) {
            // The editor (MODIFY / CREATE) ran its own loop; its suppressions came back off with it.
            gamepad_ui_set_suppress_a_click(true);
            gamepad_ui_set_suppress_snap(true);
            sel_hl_draw();
            windowRefresh(gCharacterSelectorWindow);
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    sel_hl_erase();
    inputEventQueueReset();
    gamepad_ui_set_suppress_a_click(selSuppressWas);
    gamepad_ui_set_suppress_snap(false);

    paletteFadeTo(gPaletteBlack);
    characterSelectorWindowFree();

    if (cursorWasHidden) {
        mouseHideCursor();
    }

    return rc;
}

// 0x4A7468
static bool characterSelectorWindowInit()
{
    if (gCharacterSelectorWindow != -1) {
        return false;
    }

    int characterSelectorWindowX = (screenGetWidth() - CS_WINDOW_WIDTH) / 2;
    int characterSelectorWindowY = (screenGetHeight() - CS_WINDOW_HEIGHT) / 2;
    gCharacterSelectorWindow = windowCreate(characterSelectorWindowX, characterSelectorWindowY, CS_WINDOW_WIDTH, CS_WINDOW_HEIGHT, _colorTable[0], 0);
    if (gCharacterSelectorWindow == -1) {
        return characterSelectorWindowFatalError(false);
    }

    gCharacterSelectorWindowBuffer = windowGetBuffer(gCharacterSelectorWindow);
    if (gCharacterSelectorWindowBuffer == nullptr) {
        return characterSelectorWindowFatalError(false);
    }

    FrmImage backgroundFrmImage;
    int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, 174, 0, 0, 0);
    if (!backgroundFrmImage.lock(backgroundFid)) {
        return characterSelectorWindowFatalError(false);
    }

    blitBufferToBuffer(backgroundFrmImage.getData(),
        CS_WINDOW_WIDTH,
        CS_WINDOW_HEIGHT,
        CS_WINDOW_WIDTH,
        gCharacterSelectorWindowBuffer,
        CS_WINDOW_WIDTH);

    gCharacterSelectorBackground = (unsigned char*)internal_malloc(CS_WINDOW_BACKGROUND_WIDTH * CS_WINDOW_BACKGROUND_HEIGHT);
    if (gCharacterSelectorBackground == nullptr)
        return characterSelectorWindowFatalError(false);

    blitBufferToBuffer(backgroundFrmImage.getData() + CS_WINDOW_WIDTH * CS_WINDOW_BACKGROUND_Y + CS_WINDOW_BACKGROUND_X,
        CS_WINDOW_BACKGROUND_WIDTH,
        CS_WINDOW_BACKGROUND_HEIGHT,
        CS_WINDOW_WIDTH,
        gCharacterSelectorBackground,
        CS_WINDOW_BACKGROUND_WIDTH);

    backgroundFrmImage.unlock();

    int fid;

    // Setup "Previous" button.
    fid = buildFid(OBJ_TYPE_INTERFACE, 122, 0, 0, 0);
    if (!_previousButtonNormalFrmImage.lock(fid)) {
        return characterSelectorWindowFatalError(false);
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 123, 0, 0, 0);
    if (!_previousButtonPressedFrmImage.lock(fid)) {
        return characterSelectorWindowFatalError(false);
    }

    gCharacterSelectorWindowPreviousButton = buttonCreate(gCharacterSelectorWindow,
        CS_WINDOW_PREVIOUS_BUTTON_X,
        CS_WINDOW_PREVIOUS_BUTTON_Y,
        20,
        18,
        -1,
        -1,
        -1,
        500,
        _previousButtonNormalFrmImage.getData(),
        _previousButtonPressedFrmImage.getData(),
        nullptr,
        0);
    if (gCharacterSelectorWindowPreviousButton == -1) {
        return characterSelectorWindowFatalError(false);
    }

    buttonSetCallbacks(gCharacterSelectorWindowPreviousButton, _gsound_med_butt_press, _gsound_med_butt_release);

    // Setup "Next" button.
    fid = buildFid(OBJ_TYPE_INTERFACE, 124, 0, 0, 0);
    if (!_nextButtonNormalFrmImage.lock(fid)) {
        return characterSelectorWindowFatalError(false);
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 125, 0, 0, 0);
    if (!_nextButtonPressedFrmImage.lock(fid)) {
        return characterSelectorWindowFatalError(false);
    }

    gCharacterSelectorWindowNextButton = buttonCreate(gCharacterSelectorWindow,
        CS_WINDOW_NEXT_BUTTON_X,
        CS_WINDOW_NEXT_BUTTON_Y,
        20,
        18,
        -1,
        -1,
        -1,
        501,
        _nextButtonNormalFrmImage.getData(),
        _nextButtonPressedFrmImage.getData(),
        nullptr,
        0);
    if (gCharacterSelectorWindowNextButton == -1) {
        return characterSelectorWindowFatalError(false);
    }

    buttonSetCallbacks(gCharacterSelectorWindowNextButton, _gsound_med_butt_press, _gsound_med_butt_release);

    // Setup "Take" button.
    fid = buildFid(OBJ_TYPE_INTERFACE, 8, 0, 0, 0);
    if (!_takeButtonNormalFrmImage.lock(fid)) {
        return characterSelectorWindowFatalError(false);
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 9, 0, 0, 0);
    if (!_takeButtonPressedFrmImage.lock(fid)) {
        return characterSelectorWindowFatalError(false);
    }

    gCharacterSelectorWindowTakeButton = buttonCreate(gCharacterSelectorWindow,
        CS_WINDOW_TAKE_BUTTON_X,
        CS_WINDOW_TAKE_BUTTON_Y,
        15,
        16,
        -1,
        -1,
        -1,
        KEY_LOWERCASE_T,
        _takeButtonNormalFrmImage.getData(),
        _takeButtonPressedFrmImage.getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (gCharacterSelectorWindowTakeButton == -1) {
        return characterSelectorWindowFatalError(false);
    }

    buttonSetCallbacks(gCharacterSelectorWindowTakeButton, _gsound_red_butt_press, _gsound_red_butt_release);

    // Setup "Modify" button.
    fid = buildFid(OBJ_TYPE_INTERFACE, 8, 0, 0, 0);
    if (!_modifyButtonNormalFrmImage.lock(fid))
        return characterSelectorWindowFatalError(false);

    fid = buildFid(OBJ_TYPE_INTERFACE, 9, 0, 0, 0);
    if (!_modifyButtonPressedFrmImage.lock(fid)) {
        return characterSelectorWindowFatalError(false);
    }

    gCharacterSelectorWindowModifyButton = buttonCreate(gCharacterSelectorWindow,
        CS_WINDOW_MODIFY_BUTTON_X,
        CS_WINDOW_MODIFY_BUTTON_Y,
        15,
        16,
        -1,
        -1,
        -1,
        KEY_LOWERCASE_M,
        _modifyButtonNormalFrmImage.getData(),
        _modifyButtonPressedFrmImage.getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (gCharacterSelectorWindowModifyButton == -1) {
        return characterSelectorWindowFatalError(false);
    }

    buttonSetCallbacks(gCharacterSelectorWindowModifyButton, _gsound_red_butt_press, _gsound_red_butt_release);

    // Setup "Create" button.
    fid = buildFid(OBJ_TYPE_INTERFACE, 8, 0, 0, 0);
    if (!_createButtonNormalFrmImage.lock(fid)) {
        return characterSelectorWindowFatalError(false);
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 9, 0, 0, 0);
    if (!_createButtonPressedFrmImage.lock(fid)) {
        return characterSelectorWindowFatalError(false);
    }

    gCharacterSelectorWindowCreateButton = buttonCreate(gCharacterSelectorWindow,
        CS_WINDOW_CREATE_BUTTON_X,
        CS_WINDOW_CREATE_BUTTON_Y,
        15,
        16,
        -1,
        -1,
        -1,
        KEY_LOWERCASE_C,
        _createButtonNormalFrmImage.getData(),
        _createButtonPressedFrmImage.getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (gCharacterSelectorWindowCreateButton == -1) {
        return characterSelectorWindowFatalError(false);
    }

    buttonSetCallbacks(gCharacterSelectorWindowCreateButton, _gsound_red_butt_press, _gsound_red_butt_release);

    // Setup "Back" button.
    fid = buildFid(OBJ_TYPE_INTERFACE, 8, 0, 0, 0);
    if (!_backButtonNormalFrmImage.lock(fid)) {
        return characterSelectorWindowFatalError(false);
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 9, 0, 0, 0);
    if (!_backButtonPressedFrmImage.lock(fid)) {
        return characterSelectorWindowFatalError(false);
    }

    gCharacterSelectorWindowBackButton = buttonCreate(gCharacterSelectorWindow,
        CS_WINDOW_BACK_BUTTON_X,
        CS_WINDOW_BACK_BUTTON_Y,
        15,
        16,
        -1,
        -1,
        -1,
        KEY_ESCAPE,
        _backButtonNormalFrmImage.getData(),
        _backButtonPressedFrmImage.getData(),
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    if (gCharacterSelectorWindowBackButton == -1) {
        return characterSelectorWindowFatalError(false);
    }

    buttonSetCallbacks(gCharacterSelectorWindowBackButton, _gsound_red_butt_press, _gsound_red_butt_release);

    gCurrentPremadeCharacter = PREMADE_CHARACTER_NARG;

    windowRefresh(gCharacterSelectorWindow);

    if (!characterSelectorWindowRefresh()) {
        return characterSelectorWindowFatalError(false);
    }

    return true;
}

// 0x4A7AD4
static void characterSelectorWindowFree()
{
    if (gCharacterSelectorWindow == -1) {
        return;
    }

    if (gCharacterSelectorWindowPreviousButton != -1) {
        buttonDestroy(gCharacterSelectorWindowPreviousButton);
        gCharacterSelectorWindowPreviousButton = -1;
    }

    _previousButtonNormalFrmImage.unlock();
    _previousButtonPressedFrmImage.unlock();

    if (gCharacterSelectorWindowNextButton != -1) {
        buttonDestroy(gCharacterSelectorWindowNextButton);
        gCharacterSelectorWindowNextButton = -1;
    }

    _nextButtonNormalFrmImage.unlock();
    _nextButtonPressedFrmImage.unlock();

    if (gCharacterSelectorWindowTakeButton != -1) {
        buttonDestroy(gCharacterSelectorWindowTakeButton);
        gCharacterSelectorWindowTakeButton = -1;
    }

    _takeButtonNormalFrmImage.unlock();
    _takeButtonPressedFrmImage.unlock();

    if (gCharacterSelectorWindowModifyButton != -1) {
        buttonDestroy(gCharacterSelectorWindowModifyButton);
        gCharacterSelectorWindowModifyButton = -1;
    }

    _modifyButtonNormalFrmImage.unlock();
    _modifyButtonPressedFrmImage.unlock();

    if (gCharacterSelectorWindowCreateButton != -1) {
        buttonDestroy(gCharacterSelectorWindowCreateButton);
        gCharacterSelectorWindowCreateButton = -1;
    }

    _createButtonNormalFrmImage.unlock();
    _createButtonPressedFrmImage.unlock();

    if (gCharacterSelectorWindowBackButton != -1) {
        buttonDestroy(gCharacterSelectorWindowBackButton);
        gCharacterSelectorWindowBackButton = -1;
    }

    _backButtonNormalFrmImage.unlock();
    _backButtonPressedFrmImage.unlock();

    if (gCharacterSelectorBackground != nullptr) {
        internal_free(gCharacterSelectorBackground);
        gCharacterSelectorBackground = nullptr;
    }

    windowDestroy(gCharacterSelectorWindow);
    gCharacterSelectorWindow = -1;
}

// 0x4A7D58
static bool characterSelectorWindowRefresh()
{
    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s.gcd", gCustomPremadeCharacterDescriptions[gCurrentPremadeCharacter].fileName);
    premadeCharactersLocalizePath(path);

    if (_proto_dude_init(path) == -1) {
        debugPrint("\n ** Error in dude init! **\n");
        return false;
    }

    blitBufferToBuffer(gCharacterSelectorBackground,
        CS_WINDOW_BACKGROUND_WIDTH,
        CS_WINDOW_BACKGROUND_HEIGHT,
        CS_WINDOW_BACKGROUND_WIDTH,
        gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * CS_WINDOW_BACKGROUND_Y + CS_WINDOW_BACKGROUND_X,
        CS_WINDOW_WIDTH);

    bool success = false;
    if (characterSelectorWindowRenderFace()) {
        if (characterSelectorWindowRenderStats()) {
            success = characterSelectorWindowRenderBio();
        }
    }

    windowRefresh(gCharacterSelectorWindow);

    return success;
}

// 0x4A7E08
static bool characterSelectorWindowRenderFace()
{
    bool success = false;

    FrmImage faceFrmImage;
    int faceFid = buildFid(OBJ_TYPE_INTERFACE, gCustomPremadeCharacterDescriptions[gCurrentPremadeCharacter].face, 0, 0, 0);
    if (faceFrmImage.lock(faceFid)) {
        unsigned char* data = faceFrmImage.getData();
        if (data != nullptr) {
            int width = faceFrmImage.getWidth();
            int height = faceFrmImage.getHeight();
            blitBufferToBufferTrans(data, width, height, width, (gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * 23 + 27), CS_WINDOW_WIDTH);
            success = true;
        }
        faceFrmImage.unlock();
    }

    return success;
}

// 0x4A7EA8
static bool characterSelectorWindowRenderStats()
{
    char* str;
    char text[260];
    int length;
    int value;
    MessageListItem messageListItem;

    int oldFont = fontGetCurrent();
    fontSetCurrent(101);

    fontGetCharacterWidth(0x20);

    int vh = fontGetLineHeight();
    int y = 40;

    // NAME
    str = objectGetName(gDude);
    strcpy(text, str);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_NAME_MID_X - (length / 2), text, 160, CS_WINDOW_WIDTH, _colorTable[992]);

    // STRENGTH
    y += vh + vh + vh;

    value = critterGetStat(gDude, STAT_STRENGTH);
    str = statGetName(STAT_STRENGTH);

    snprintf(text, sizeof(text), "%s %02d", str, value);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    str = statGetValueDescription(value);
    snprintf(text, sizeof(text), "  %s", str);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    // PERCEPTION
    y += vh;

    value = critterGetStat(gDude, STAT_PERCEPTION);
    str = statGetName(STAT_PERCEPTION);

    snprintf(text, sizeof(text), "%s %02d", str, value);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    str = statGetValueDescription(value);
    snprintf(text, sizeof(text), "  %s", str);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    // ENDURANCE
    y += vh;

    value = critterGetStat(gDude, STAT_ENDURANCE);
    str = statGetName(STAT_ENDURANCE);

    snprintf(text, sizeof(text), "%s %02d", str, value);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    str = statGetValueDescription(value);
    snprintf(text, sizeof(text), "  %s", str);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    // CHARISMA
    y += vh;

    value = critterGetStat(gDude, STAT_CHARISMA);
    str = statGetName(STAT_CHARISMA);

    snprintf(text, sizeof(text), "%s %02d", str, value);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    str = statGetValueDescription(value);
    snprintf(text, sizeof(text), "  %s", str);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    // INTELLIGENCE
    y += vh;

    value = critterGetStat(gDude, STAT_INTELLIGENCE);
    str = statGetName(STAT_INTELLIGENCE);

    snprintf(text, sizeof(text), "%s %02d", str, value);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    str = statGetValueDescription(value);
    snprintf(text, sizeof(text), "  %s", str);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    // AGILITY
    y += vh;

    value = critterGetStat(gDude, STAT_AGILITY);
    str = statGetName(STAT_AGILITY);

    snprintf(text, sizeof(text), "%s %02d", str, value);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    str = statGetValueDescription(value);
    snprintf(text, sizeof(text), "  %s", str);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    // LUCK
    y += vh;

    value = critterGetStat(gDude, STAT_LUCK);
    str = statGetName(STAT_LUCK);

    snprintf(text, sizeof(text), "%s %02d", str, value);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    str = statGetValueDescription(value);
    snprintf(text, sizeof(text), "  %s", str);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_PRIMARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    y += vh; // blank line

    // HIT POINTS
    y += vh;

    messageListItem.num = 16;
    text[0] = '\0';
    if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
        strcpy(text, messageListItem.text);
    }

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    value = critterGetStat(gDude, STAT_MAXIMUM_HIT_POINTS);
    snprintf(text, sizeof(text), " %d/%d", critterGetHitPoints(gDude), value);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    // ARMOR CLASS
    y += vh;

    str = statGetName(STAT_ARMOR_CLASS);
    strcpy(text, str);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    value = critterGetStat(gDude, STAT_ARMOR_CLASS);
    snprintf(text, sizeof(text), " %d", value);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    // ACTION POINTS
    y += vh;

    messageListItem.num = 15;
    text[0] = '\0';
    if (messageListGetItem(&gMiscMessageList, &messageListItem)) {
        strcpy(text, messageListItem.text);
    }

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    value = critterGetStat(gDude, STAT_MAXIMUM_ACTION_POINTS);
    snprintf(text, sizeof(text), " %d", value);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    // MELEE DAMAGE
    y += vh;

    str = statGetName(STAT_MELEE_DAMAGE);
    strcpy(text, str);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    value = critterGetStat(gDude, STAT_MELEE_DAMAGE);
    snprintf(text, sizeof(text), " %d", value);

    length = fontGetStringWidth(text);
    fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

    y += vh; // blank line

    // SKILLS
    int skills[DEFAULT_TAGGED_SKILLS];
    skillsGetTagged(skills, DEFAULT_TAGGED_SKILLS);

    for (int index = 0; index < DEFAULT_TAGGED_SKILLS; index++) {
        y += vh;

        str = skillGetName(skills[index]);
        strcpy(text, str);

        length = fontGetStringWidth(text);
        fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);

        value = skillGetValue(gDude, skills[index]);
        snprintf(text, sizeof(text), " %d%%", value);

        length = fontGetStringWidth(text);
        fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X, text, length, CS_WINDOW_WIDTH, _colorTable[992]);
    }

    // TRAITS
    int traits[TRAITS_MAX_SELECTED_COUNT];
    traitsGetSelected(&(traits[0]), &(traits[1]));

    for (int index = 0; index < TRAITS_MAX_SELECTED_COUNT; index++) {
        y += vh;

        str = traitGetName(traits[index]);
        strcpy(text, str);

        length = fontGetStringWidth(text);
        fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_SECONDARY_STAT_MID_X - length, text, length, CS_WINDOW_WIDTH, _colorTable[992]);
    }

    fontSetCurrent(oldFont);

    return true;
}

// 0x4A8AE4
static bool characterSelectorWindowRenderBio()
{
    int oldFont = fontGetCurrent();
    fontSetCurrent(101);

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s.bio", gCustomPremadeCharacterDescriptions[gCurrentPremadeCharacter].fileName);
    premadeCharactersLocalizePath(path);

    File* stream = fileOpen(path, "rt");
    if (stream != nullptr) {
        int y = 40;
        int lineHeight = fontGetLineHeight();

        char string[256];
        while (fileReadString(string, 256, stream) && y < 260) {
            fontDrawText(gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * y + CS_WINDOW_BIO_X, string, CS_WINDOW_WIDTH - CS_WINDOW_BIO_X, CS_WINDOW_WIDTH, _colorTable[992]);
            y += lineHeight;
        }

        fileClose(stream);
    }

    fontSetCurrent(oldFont);

    return true;
}

// NOTE: Inlined.
//
// 0x4A8BD0
static bool characterSelectorWindowFatalError(bool result)
{
    characterSelectorWindowFree();
    return result;
}

void premadeCharactersInit()
{
    char* fileNamesString;
    configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_PREMADE_CHARACTERS_FILE_NAMES_KEY, &fileNamesString);
    if (fileNamesString != nullptr && *fileNamesString == '\0') {
        fileNamesString = nullptr;
    }

    char* faceFidsString;
    configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_PREMADE_CHARACTERS_FACE_FIDS_KEY, &faceFidsString);
    if (faceFidsString != nullptr && *faceFidsString == '\0') {
        faceFidsString = nullptr;
    }

    if (fileNamesString != nullptr && faceFidsString != nullptr) {
        int fileNamesLength = 0;
        for (char* pch = fileNamesString; pch != nullptr; pch = strchr(pch + 1, ',')) {
            fileNamesLength++;
        }

        int faceFidsLength = 0;
        for (char* pch = faceFidsString; pch != nullptr; pch = strchr(pch + 1, ',')) {
            faceFidsLength++;
        }

        int premadeCharactersCount = std::min(fileNamesLength, faceFidsLength);
        gCustomPremadeCharacterDescriptions.resize(premadeCharactersCount);

        for (int index = 0; index < premadeCharactersCount; index++) {
            char* pch;

            pch = strchr(fileNamesString, ',');
            if (pch != nullptr) {
                *pch = '\0';
            }

            if (strlen(fileNamesString) > 11) {
                // Sfall fails here.
                continue;
            }

            snprintf(gCustomPremadeCharacterDescriptions[index].fileName, sizeof(gCustomPremadeCharacterDescriptions[index].fileName), "premade\\%s", fileNamesString);

            if (pch != nullptr) {
                *pch = ',';
            }

            fileNamesString = pch + 1;

            pch = strchr(faceFidsString, ',');
            if (pch != nullptr) {
                *pch = '\0';
            }

            gCustomPremadeCharacterDescriptions[index].face = atoi(faceFidsString);

            if (pch != nullptr) {
                *pch = ',';
            }

            faceFidsString = pch + 1;

            gCustomPremadeCharacterDescriptions[index].field_18[0] = '\0';
        }
    }

    if (gCustomPremadeCharacterDescriptions.empty()) {
        gCustomPremadeCharacterDescriptions.resize(PREMADE_CHARACTER_COUNT);

        for (int index = 0; index < PREMADE_CHARACTER_COUNT; index++) {
            strcpy(gCustomPremadeCharacterDescriptions[index].fileName, gPremadeCharacterDescriptions[index].fileName);
            gCustomPremadeCharacterDescriptions[index].face = gPremadeCharacterDescriptions[index].face;
            strcpy(gCustomPremadeCharacterDescriptions[index].field_18, gPremadeCharacterDescriptions[index].field_18);
        }
    }

    gPremadeCharacterCount = gCustomPremadeCharacterDescriptions.size();
}

void premadeCharactersExit()
{
    gCustomPremadeCharacterDescriptions.clear();
}

static void premadeCharactersLocalizePath(char* path)
{
    if (compat_strnicmp(path, "premade\\", 8) != 0) {
        return;
    }

    const char* language = settings.system.language.c_str();
    if (compat_stricmp(language, ENGLISH) == 0) {
        return;
    }

    char localizedPath[COMPAT_MAX_PATH];
    strncpy(localizedPath, path, 8);
    strcpy(localizedPath + 8, language);
    strcpy(localizedPath + 8 + strlen(language), path + 7);

    int fileSize;
    if (dbGetFileSize(localizedPath, &fileSize) == 0) {
        strcpy(path, localizedPath);
    }
}

} // namespace fallout
