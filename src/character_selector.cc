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

// Realtime / controller: the pad's own buttons for this screen, and their badges (2026-11-14).
#include "rt/game/game_trace.h"
#include "rt/game/gamepad_ui.h"
#include "rt/rt_hooks.h"
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
// Realtime build: not static -- the gamepad layer reads it to know this screen is up
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

// THE PAD ON CHARACTER SELECTION (Fallout 2 Realtime 2026-11-14), his layout: „upravíme char select, lb a rb a bude
// u toho posouvá výběr A bude u Take, X bude u create character, Y bude modify B u back“. Every action of the screen
// has a button of its own -- LB / RB the previous / next character, A TAKE, X CREATE CHARACTER, Y MODIFY, B BACK --
// and a press sends the code that action already has here, so the loop's own dispatch below stays the only one. It
// replaces the focus frame the D-pad stepped and A pressed (fallout1-ce's select.cc, ported): with a button for
// everything there is nothing left to step to.
//
// ⚠️ THE SCREEN OWNS THE PAD (self_nav, from its first input frame to its last). It is not modal and used to set
// nothing, so the WORLD's shortcuts ran in it through the shared mouse hook: LB / RB fired the quick slots, and Y
// swapped hands on a bar whose hands still held the init sentinel -- itemGetType read (Object*)-1 and the game was
// gone („hra spadla v char selectu po zmáčknutí Y“). The pad layer also refuses every world shortcut while no map is
// up now (gpui_no_world in gamepad_ui.cc), whatever the screen.
//
// ⚠️ EVERY BUTTON ARMS ON ITS RELEASE. The A that pressed NEW GAME on the title menu is still down as this opens, and
// the button that closed the editor (MODIFY / CREATE) or the quit box is still down as they come back: a press counts
// only once the button has been seen up.
typedef struct SelPadKey {
    SDL_GameControllerButton button;
    int code; // what the press sends -- the code this screen already acts on
    const char* sound; // the click its on-screen button makes under the mouse, or nullptr when the code plays it
    const char* name; // for the trace
} SelPadKey;

static const SelPadKey sel_pad_keys[] = {
    { SDL_CONTROLLER_BUTTON_A, KEY_LOWERCASE_T, "ib1p1xx1", "A" }, // TAKE
    { SDL_CONTROLLER_BUTTON_X, KEY_LOWERCASE_C, "ib1p1xx1", "X" }, // CREATE CHARACTER
    { SDL_CONTROLLER_BUTTON_Y, KEY_LOWERCASE_M, "ib1p1xx1", "Y" }, // MODIFY
    { SDL_CONTROLLER_BUTTON_B, KEY_ESCAPE, "ib1p1xx1", "B" }, // BACK
    // The arrows as the keyboard's arrow keys step them: the very 500 / 501 the arrow buttons send, with the arrow
    // button's own click in front.
    { SDL_CONTROLLER_BUTTON_LEFTSHOULDER, KEY_ARROW_LEFT, nullptr, "LB" },
    { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, KEY_ARROW_RIGHT, nullptr, "RB" },
};

#define SEL_PAD_KEY_COUNT ((int)(sizeof(sel_pad_keys) / sizeof(sel_pad_keys[0])))

// Every button counts as held until it is seen up: on entry, and after a screen opened from here comes back.
static void sel_pad_rearm(bool* held)
{
    for (int i = 0; i < SEL_PAD_KEY_COUNT; i++) {
        held[i] = true;
    }
}

// One frame of the pad: the first button down since the last frame sends its code (one action a frame).
static void sel_pad_poll(SDL_GameController* pad, bool* held)
{
    SDL_GameControllerUpdate();

    bool sent = false;
    for (int i = 0; i < SEL_PAD_KEY_COUNT; i++) {
        bool down = SDL_GameControllerGetButton(pad, sel_pad_keys[i].button) != 0;
        if (down && !held[i] && !sent) {
            if (sel_pad_keys[i].sound != nullptr) {
                soundPlayFile(sel_pad_keys[i].sound);
            }
            enqueueInputEvent(sel_pad_keys[i].code);
            trace_event(TRACE_INPUT, "charsel-pad", "button=%s code=%d", sel_pad_keys[i].name, sel_pad_keys[i].code);
            sent = true;
        }
        held[i] = down;
    }
}

// THE BADGES: with a pad in, each of those buttons is named on the screen by the bar's own badge (intface_pad_face /
// intface_pad_pill: a dark disc or pill, the letters gold). No words: the labels are the art's.
//   - LB and RB: pills on the window art either side of the arrows' plate, clear of the arrows' rects (a press
//     repaints those) and of every line of text; what was under each is kept, so a pad pulled out takes them with it.
//     Both sit above y 330, inside the part a new character repaints (characterSelectorWindowRefresh), which draws
//     them again.
//   - A, Y, X and B: ON the red buttons themselves, the whole badge in place of the red dot (sel_red_*, below) -- his
//     pick of three mock-ups on 2026-11-14 („Celá značka“; the others were the badge beside the plate, as first
//     built, and the gold letter on the red dot).
typedef struct SelPadBadge {
    int cx;
    int cy;
    const char* key; // two letters: a shoulder's pill
} SelPadBadge;

static const SelPadBadge sel_pad_badges[] = {
    { 270, 329, "LB" }, // the arrows' plate: 281..349
    { 361, 329, "RB" },
};

#define SEL_BADGE_COUNT ((int)(sizeof(sel_pad_badges) / sizeof(sel_pad_badges[0])))

// The box kept under each badge: as wide as the pill (17), as tall as the disc (15).
#define SEL_BADGE_HALF_W 8
#define SEL_BADGE_HALF_H 7
#define SEL_BADGE_W (2 * SEL_BADGE_HALF_W + 1)
#define SEL_BADGE_H (2 * SEL_BADGE_HALF_H + 1)

static unsigned char sel_badge_under[SEL_BADGE_COUNT][SEL_BADGE_W * SEL_BADGE_H];
static bool sel_badge_under_kept = false;
static bool sel_badges_shown = false;

// Once, on the bare art, before any badge is drawn.
static void sel_badges_keep_under()
{
    for (int i = 0; i < SEL_BADGE_COUNT; i++) {
        const SelPadBadge* badge = &(sel_pad_badges[i]);
        unsigned char* src = gCharacterSelectorWindowBuffer
            + CS_WINDOW_WIDTH * (badge->cy - SEL_BADGE_HALF_H) + badge->cx - SEL_BADGE_HALF_W;
        for (int y = 0; y < SEL_BADGE_H; y++) {
            memcpy(sel_badge_under[i] + SEL_BADGE_W * y, src + CS_WINDOW_WIDTH * y, SEL_BADGE_W);
        }
    }
    sel_badge_under_kept = true;
    sel_badges_shown = false;
}

// The badges with a pad in, the art they covered without one. The caller refreshes the window.
static void sel_badges_paint(bool pad)
{
    if (gCharacterSelectorWindowBuffer == nullptr || !sel_badge_under_kept) {
        return;
    }

    for (int i = 0; i < SEL_BADGE_COUNT; i++) {
        const SelPadBadge* badge = &(sel_pad_badges[i]);
        if (pad) {
            intface_pad_pill(gCharacterSelectorWindowBuffer, CS_WINDOW_WIDTH, CS_WINDOW_WIDTH, CS_WINDOW_HEIGHT,
                badge->cx, badge->cy, badge->key);
        } else if (sel_badges_shown) {
            unsigned char* dest = gCharacterSelectorWindowBuffer
                + CS_WINDOW_WIDTH * (badge->cy - SEL_BADGE_HALF_H) + badge->cx - SEL_BADGE_HALF_W;
            for (int y = 0; y < SEL_BADGE_H; y++) {
                memcpy(dest + CS_WINDOW_WIDTH * y, sel_badge_under[i] + SEL_BADGE_W * y, SEL_BADGE_W);
            }
        }
    }
    sel_badges_shown = pad;
}

// THE RED BUTTONS WEAR THEIR PAD BUTTON (2026-11-14, see THE BADGES above). Drawn into COPIES OF THE BUTTONS' OWN
// IMAGES, not on the window: a GNW button blits its image into the window buffer at every window refresh
// (_GNW_button_refresh), so a badge painted on the art under a red button was simply covered by it -- the first try.
// Each copy is the whole 15x16 tile made opaque: the bare art that lies under the button, the button's own image over
// it (its transparent pixels kept), then the badge. Opaque, because the buttons are BUTTON_FLAG_TRANSPARENT and blit
// only their non-zero pixels: a copy with holes would leave whatever the previous image drew there. The pressed copy
// is the same badge over the pressed art, so a mouse press on a badged button changes nothing a player could misread
// (the press still clicks). Without a pad the buttons get the art's own images back, over the bare art put back first.
#define SEL_RED_W 15
#define SEL_RED_H 16
enum {
    SEL_RED_TAKE,
    SEL_RED_MODIFY,
    SEL_RED_CREATE,
    SEL_RED_BACK,
    SEL_RED_COUNT,
};
static const char sel_red_keys[SEL_RED_COUNT] = { 'A', 'Y', 'X', 'B' };
static const int sel_red_at[SEL_RED_COUNT][2] = {
    { CS_WINDOW_TAKE_BUTTON_X, CS_WINDOW_TAKE_BUTTON_Y },
    { CS_WINDOW_MODIFY_BUTTON_X, CS_WINDOW_MODIFY_BUTTON_Y },
    { CS_WINDOW_CREATE_BUTTON_X, CS_WINDOW_CREATE_BUTTON_Y },
    { CS_WINDOW_BACK_BUTTON_X, CS_WINDOW_BACK_BUTTON_Y },
};
static unsigned char sel_red_under[SEL_RED_COUNT][SEL_RED_W * SEL_RED_H]; // the bare art under each red button
static unsigned char sel_red_badged[SEL_RED_COUNT][2][SEL_RED_W * SEL_RED_H]; // [normal, pressed], opaque
static bool sel_red_ready = false;
static bool sel_red_shown = false;

static int sel_red_button(int i)
{
    switch (i) {
    case SEL_RED_TAKE:
        return gCharacterSelectorWindowTakeButton;
    case SEL_RED_MODIFY:
        return gCharacterSelectorWindowModifyButton;
    case SEL_RED_CREATE:
        return gCharacterSelectorWindowCreateButton;
    case SEL_RED_BACK:
        return gCharacterSelectorWindowBackButton;
    }
    return -1;
}

static FrmImage* sel_red_art(int i, bool pressed)
{
    switch (i) {
    case SEL_RED_TAKE:
        return pressed ? &_takeButtonPressedFrmImage : &_takeButtonNormalFrmImage;
    case SEL_RED_MODIFY:
        return pressed ? &_modifyButtonPressedFrmImage : &_modifyButtonNormalFrmImage;
    case SEL_RED_CREATE:
        return pressed ? &_createButtonPressedFrmImage : &_createButtonNormalFrmImage;
    case SEL_RED_BACK:
        return pressed ? &_backButtonPressedFrmImage : &_backButtonNormalFrmImage;
    }
    return nullptr;
}

// Once, on the bare art, before the buttons exist (they draw themselves into the window when made).
static void sel_red_keep_under()
{
    for (int i = 0; i < SEL_RED_COUNT; i++) {
        const unsigned char* src = gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * sel_red_at[i][1] + sel_red_at[i][0];
        for (int y = 0; y < SEL_RED_H; y++) {
            memcpy(sel_red_under[i] + SEL_RED_W * y, src + CS_WINDOW_WIDTH * y, SEL_RED_W);
        }
    }
    sel_red_ready = false;
    sel_red_shown = false;
}

// Once the four buttons and their art exist: the badged tiles. Art of another size than the buttons' 15x16 gets no
// badge (the plain buttons stay).
static void sel_red_make()
{
    sel_red_ready = false;
    for (int i = 0; i < SEL_RED_COUNT; i++) {
        for (int p = 0; p < 2; p++) {
            FrmImage* art = sel_red_art(i, p != 0);
            if (art == nullptr || art->getData() == nullptr || art->getWidth() != SEL_RED_W
                || art->getHeight() != SEL_RED_H) {
                return;
            }
            unsigned char* tile = sel_red_badged[i][p];
            memcpy(tile, sel_red_under[i], SEL_RED_W * SEL_RED_H);
            const unsigned char* img = art->getData();
            for (int k = 0; k < SEL_RED_W * SEL_RED_H; k++) {
                if (img[k] != 0) {
                    tile[k] = img[k];
                }
            }
            intface_pad_face(tile, SEL_RED_W, SEL_RED_W, SEL_RED_H, SEL_RED_W / 2, SEL_RED_H / 2, sel_red_keys[i]);
        }
    }
    sel_red_ready = true;
}

// The badged tiles with a pad in, the art's own images without one; nothing when that is already so. The window is
// refreshed by the button swap itself.
static void sel_red_show(bool pad)
{
    if (!sel_red_ready || pad == sel_red_shown) {
        return;
    }
    for (int i = 0; i < SEL_RED_COUNT; i++) {
        int btn = sel_red_button(i);
        if (btn == -1) {
            continue;
        }
        // The bare art back under the button first: a transparent image only covers its own non-zero pixels.
        unsigned char* dest = gCharacterSelectorWindowBuffer + CS_WINDOW_WIDTH * sel_red_at[i][1] + sel_red_at[i][0];
        for (int y = 0; y < SEL_RED_H; y++) {
            memcpy(dest + CS_WINDOW_WIDTH * y, sel_red_under[i] + SEL_RED_W * y, SEL_RED_W);
        }
        if (pad) {
            _win_register_button_image(btn, sel_red_badged[i][0], sel_red_badged[i][1], nullptr, true);
        } else {
            _win_register_button_image(btn, sel_red_art(i, false)->getData(), sel_red_art(i, true)->getData(), nullptr,
                true);
        }
    }
    sel_red_shown = pad;
    trace_event(TRACE_FLOW, "charsel-red", "badged=%d", pad ? 1 : 0);
}

// 0x4A71D0
int characterSelectorOpen()
{
    if (!characterSelectorWindowInit()) {
        return 0;
    }

    // The pad is this screen's (see sel_pad_keys) -- with no pad as well, which costs nothing, as on the title menu.
    gamepad_ui_set_self_nav(true);

    bool cursorWasHidden = cursorIsHidden();
    if (cursorWasHidden) {
        mouseShowCursor();
    }

    // Same reason as the character editor: this screen injects its own button codes and must not
    // inherit anyone else's.
    inputEventQueueReset();

    colorPaletteLoad("color.pal");
    paletteFadeTo(_cmap);

    bool padHeld[SEL_PAD_KEY_COUNT];
    sel_pad_rearm(padHeld);
    trace_event(TRACE_FLOW, "charsel", "open premades=%d pad=%d", gPremadeCharacterCount,
        gamepad_ui_get_controller() != nullptr ? 1 : 0);

    int rc = 0;
    bool done = false;
    while (!done) {
        sharedFpsLimiter.mark();

        if (_game_user_wants_to_quit != 0) {
            break;
        }

        // Re-fetched every frame, never latched on entry: gamepad_ui opens the controller LAZILY, and one can come or
        // go while the screen is up -- its badges with it. Read before the input, so a press is answered this frame.
        SDL_GameController* pad = gamepad_ui_get_controller();
        if (pad != nullptr) {
            sel_pad_poll(pad, padHeld);
        }
        if ((pad != nullptr) != sel_badges_shown) {
            sel_badges_paint(pad != nullptr);
            windowRefresh(gCharacterSelectorWindow);
        }
        sel_red_show(pad != nullptr); // the red buttons' badges: nothing unless the pad came or went

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
            // The editor wants the right stick's pointer, which self_nav takes away (it reaches PRINT / DONE / OPTIONS
            // with it); the pad is this screen's again when it comes back, every button once let go.
            gamepad_ui_set_self_nav(false);
            if (characterEditorShow(1) == 0) {
                rc = 2;
                done = true;
            } else {
                characterSelectorWindowRefresh();
            }
            gamepad_ui_set_self_nav(true);
            sel_pad_rearm(padHeld);

            break;
        case KEY_UPPERCASE_M:
        case KEY_LOWERCASE_M:
            gamepad_ui_set_self_nav(false); // as CREATE's, above
            if (!characterEditorShow(1)) {
                rc = 2;
                done = true;
            } else {
                characterSelectorWindowRefresh();
            }
            gamepad_ui_set_self_nav(true);
            sel_pad_rearm(padHeld);

            break;
        case KEY_UPPERCASE_T:
        case KEY_LOWERCASE_T:
            rc = 2;
            done = true;

            break;
        case KEY_F10:
            showQuitConfirmationDialog();
            sel_pad_rearm(padHeld); // its A / B are still down as it closes
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

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    inputEventQueueReset();
    // Every way out comes through here. The button that closed the screen is still down, and the title menu or the
    // game after it must not read it as a fresh press (the pad layer re-primes its edges on the epoch).
    gamepad_ui_set_self_nav(false);
    gamepad_ui_bump_input_epoch();
    trace_event(TRACE_FLOW, "charsel", "close rc=%d", rc);

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

    // Realtime build: the bare art under the pad's badges and under the red buttons, before anything is drawn over it.
    sel_badges_keep_under();
    sel_red_keep_under();

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

    // Realtime build: the red buttons' badged tiles (sel_red_show puts them on with a pad in).
    sel_red_make();

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

    // Realtime build: the next opening keeps its own bare art and makes its own badged tiles.
    sel_badge_under_kept = false;
    sel_badges_shown = false;
    sel_red_ready = false;
    sel_red_shown = false;
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

    // Realtime build: the badges the background blit above just painted over (sel_pad_badges), and on the first refresh
    // the red buttons' (the window refresh below redraws every button from its image anyway).
    sel_badges_paint(gamepad_ui_get_controller() != nullptr);
    sel_red_show(gamepad_ui_get_controller() != nullptr);
    trace_event(TRACE_FLOW, "charsel-show", "premade=%d name=%s", gCurrentPremadeCharacter, objectGetName(gDude));

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
