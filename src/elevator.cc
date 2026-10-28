#include "elevator.h"

#include <ctype.h>
#include <string.h>

#include <algorithm>

#include "art.h"
#include "cycle.h"
#include "debug.h"
#include "delay.h"
#include "draw.h"
#include "game_mouse.h"
#include "game_sound.h"
#include "geometry.h"
#include "input.h"
#include "interface.h"
#include "kb.h"
#include "map.h"
#include "pipboy.h"
#include "scripts.h"
#include "sfall_config.h"
#include "svga.h"
#include "window_manager.h"

#include <SDL.h>

// Realtime / controller (fallout1-ce's elevator.cc changes, ported).
#include "rt/game/game_trace.h"
#include "rt/game/gamepad_ui.h"
#include "rt/game/interact_prompt.h"

namespace fallout {

// The maximum number of elevator levels.
#define ELEVATOR_LEVEL_MAX (4)

// Max number of elevators that can be loaded from elevators.ini. This limit is
// emposed by Sfall.
#define ELEVATORS_MAX 50

typedef enum ElevatorFrm {
    ELEVATOR_FRM_BUTTON_DOWN,
    ELEVATOR_FRM_BUTTON_UP,
    ELEVATOR_FRM_GAUGE,
    ELEVATOR_FRM_COUNT,
} ElevatorFrm;

typedef struct ElevatorBackground {
    int backgroundFrmId;
    int panelFrmId;
} ElevatorBackground;

typedef struct ElevatorDescription {
    int map;
    int elevation;
    int tile;
} ElevatorDescription;

static int elevatorWindowInit(int elevator);
static void elevatorWindowFree();
static int elevatorGetLevelFromKeyCode(int elevator, int keyCode);

// 0x43E950
static const int gElevatorFrmIds[ELEVATOR_FRM_COUNT] = {
    141, // ebut_in.frm - map elevator screen
    142, // ebut_out.frm - map elevator screen
    149, // gaj000.frm - map elevator screen
};

// 0x43E95C
static ElevatorBackground gElevatorBackgrounds[ELEVATORS_MAX] = {
    { 143, -1 },
    { 143, 150 },
    { 144, -1 },
    { 144, 145 },
    { 146, -1 },
    { 146, 147 },
    { 146, -1 },
    { 146, 151 },
    { 148, -1 },
    { 146, -1 },
    { 146, -1 },
    { 146, 147 },
    { 388, -1 },
    { 143, 150 },
    { 148, -1 },
    { 148, -1 },
    { 148, -1 },
    { 143, 150 },
    { 143, 150 },
    { 143, 150 },
    { 143, 150 },
    { 143, 150 },
    { 143, 150 },
    { 143, 150 },
};

// Number of levels for eleveators.
//
// 0x43EA1C
static int gElevatorLevels[ELEVATORS_MAX] = {
    4,
    2,
    3,
    2,
    3,
    2,
    3,
    3,
    3,
    3,
    3,
    2,
    4,
    2,
    3,
    3,
    3,
    2,
    2,
    2,
    2,
    2,
    2,
    2,
};

// 0x43EA7C
static ElevatorDescription gElevatorDescriptions[ELEVATORS_MAX][ELEVATOR_LEVEL_MAX] = {
    {
        { 14, 0, 18940 },
        { 14, 1, 18936 },
        { 15, 0, 21340 },
        { 15, 1, 21340 },
    },
    {
        { 13, 0, 20502 },
        { 14, 0, 14912 },
        { 0, 0, -1 },
        { 0, 0, -1 },
    },
    {
        { 33, 0, 12498 },
        { 33, 1, 20094 },
        { 34, 0, 17312 },
        { 0, 0, -1 },
    },
    {
        { 34, 0, 16140 },
        { 34, 1, 16140 },
        { 0, 0, -1 },
        { 0, 0, -1 },
    },
    {
        { 49, 0, 14920 },
        { 49, 1, 15120 },
        { 50, 0, 12944 },
        { 0, 0, -1 },
    },
    {
        { 50, 0, 24520 },
        { 50, 1, 25316 },
        { 0, 0, -1 },
        { 0, 0, -1 },
    },
    {
        { 42, 0, 22526 },
        { 42, 1, 22526 },
        { 42, 2, 22526 },
        { 0, 0, -1 },
    },
    {
        { 42, 2, 14086 },
        { 43, 0, 14086 },
        { 43, 2, 14086 },
        { 0, 0, -1 },
    },
    {
        { 40, 0, 14104 },
        { 40, 1, 22504 },
        { 40, 2, 17312 },
        { 0, 0, -1 },
    },
    {
        { 9, 0, 13704 },
        { 9, 1, 23302 },
        { 9, 2, 17308 },
        { 0, 0, -1 },
    },
    {
        { 28, 0, 19300 },
        { 28, 1, 19300 },
        { 28, 2, 20110 },
        { 0, 0, -1 },
    },
    {
        { 28, 2, 20118 },
        { 29, 0, 21710 },
        { 0, 0, -1 },
        { 0, 0, -1 },
    },
    {
        { 28, 0, 20122 },
        { 28, 1, 20124 },
        { 28, 2, 20940 },
        { 29, 0, 22540 },
    },
    {
        { 12, 1, 16052 },
        { 12, 2, 14480 },
        { 0, 0, -1 },
        { 0, 0, -1 },
    },
    {
        { 6, 0, 14104 },
        { 6, 1, 22504 },
        { 6, 2, 17312 },
        { 0, 0, -1 },
    },
    {
        { 30, 0, 14104 },
        { 30, 1, 22504 },
        { 30, 2, 17312 },
        { 0, 0, -1 },
    },
    {
        { 36, 0, 13704 },
        { 36, 1, 23302 },
        { 36, 2, 17308 },
        { 0, 0, -1 },
    },
    {
        { 39, 0, 17285 },
        { 36, 0, 19472 },
        { 0, 0, -1 },
        { 0, 0, -1 },
    },
    {
        { 109, 2, 10701 },
        { 109, 1, 10705 },
        { 0, 0, -1 },
        { 0, 0, -1 },
    },
    {
        { 109, 2, 14697 },
        { 109, 1, 15099 },
        { 0, 0, -1 },
        { 0, 0, -1 },
    },
    {
        { 109, 2, 23877 },
        { 109, 1, 25481 },
        { 0, 0, -1 },
        { 0, 0, -1 },
    },
    {
        { 109, 2, 26130 },
        { 109, 1, 24721 },
        { 0, 0, -1 },
        { 0, 0, -1 },
    },
    {
        { 137, 0, 23953 },
        { 148, 1, 16526 },
        { 0, 0, -1 },
        { 0, 0, -1 },
    },
    {
        { 62, 0, 13901 },
        { 63, 1, 17923 },
        { 0, 0, -1 },
        { 0, 0, -1 },
    },
};

// NOTE: These values are also used as key bindings.
//
// 0x43EEFC
static char gElevatorLevelLabels[ELEVATORS_MAX][ELEVATOR_LEVEL_MAX] = {
    { '1', '2', '3', '4' },
    { 'G', '1', '\0', '\0' },
    { '1', '2', '3', '\0' },
    { '3', '4', '\0', '\0' },
    { '1', '2', '3', '\0' },
    { '3', '4', '\0', '\0' },
    { '1', '2', '3', '\0' },
    { '3', '4', '6', '\0' },
    { '1', '2', '3', '\0' },
    { '1', '2', '3', '\0' },
    { '1', '2', '3', '\0' },
    { '3', '4', '\0', '\0' },
    { '1', '2', '3', '4' },
    { '1', '2', '\0', '\0' },
    { '1', '2', '3', '\0' },
    { '1', '2', '3', '\0' },
    { '1', '2', '3', '\0' },
    { '1', '2', '\0', '\0' },
    { '1', '2', '\0', '\0' },
    { '1', '2', '\0', '\0' },
    { '1', '2', '\0', '\0' },
    { '1', '2', '\0', '\0' },
    { '1', '2', '\0', '\0' },
    { '1', '2', '\0', '\0' },
};

// 0x51862C
static const char* gElevatorSoundEffects[ELEVATOR_LEVEL_MAX - 1][ELEVATOR_LEVEL_MAX] = {
    {
        "ELV1_1",
        "ELV1_1",
        "ERROR",
        "ERROR",
    },
    {
        "ELV1_2",
        "ELV1_2",
        "ELV1_1",
        "ERROR",
    },
    {
        "ELV1_3",
        "ELV1_3",
        "ELV2_3",
        "ELV1_1",
    },
};

// 0x570A54
static int gElevatorWindow;

// 0x570A6C
static unsigned char* gElevatorWindowBuffer;

// 0x570A70
static bool gElevatorWindowIsoWasEnabled;

static FrmImage _elevatorFrmImages[ELEVATOR_FRM_COUNT];
static FrmImage _elevatorBackgroundFrmImage;
static FrmImage _elevatorPanelFrmImage;

// Presents elevator dialog for player to pick a desired level.
//
// 0x43EF5C
// THE SELECTED FLOOR is marked on the panel itself with the panel's OWN pressed-button art
// (ELEVATOR_FRM_BUTTON_DOWN) -- no yellow frame ("highlighted žlutě floors u výtahu, I fucking hate
// that"). A parked cursor alone was not enough: on this screen it is not even reliably visible.
//
// ⚠️ SAVE-UNDER, because this window is painted ONCE and keeps no clean copy to erase against: keep
// the pixels the mark covers and put them back before anything else draws. That only stays
// consistent if nothing draws while the mark is up, so the loop is strictly ERASE -> input -> DRAW.
#define ELEV_HL_MAX 64

static unsigned char elev_hl_save[ELEV_HL_MAX * ELEV_HL_MAX];
static int elev_hl_x = -1;
static int elev_hl_y;
static int elev_hl_w;
static int elev_hl_h;

static void elevator_hl_erase()
{
    if (elev_hl_x < 0 || gElevatorWindowBuffer == nullptr) {
        return;
    }

    int pitch = _elevatorBackgroundFrmImage.getWidth();
    for (int i = 0; i < elev_hl_h; i++) {
        memcpy(gElevatorWindowBuffer + pitch * (elev_hl_y + i) + elev_hl_x, elev_hl_save + elev_hl_w * i, elev_hl_w);
    }

    elev_hl_x = -1;
}

static void elevator_hl_draw(int level)
{
    if (gElevatorWindowBuffer == nullptr || _elevatorFrmImages[ELEVATOR_FRM_BUTTON_DOWN].getData() == nullptr) {
        return;
    }

    // Exactly where elevatorWindowInit registers the button: x 13, y 40 + 60 per level.
    int x = 13;
    int y = 40 + 60 * level;
    int w = _elevatorFrmImages[ELEVATOR_FRM_BUTTON_DOWN].getWidth();
    int h = _elevatorFrmImages[ELEVATOR_FRM_BUTTON_DOWN].getHeight();

    int pitch = _elevatorBackgroundFrmImage.getWidth();
    if (x < 0 || y < 0 || x + w > pitch || y + h > _elevatorBackgroundFrmImage.getHeight()) {
        return;
    }
    if (w < 1 || h < 1 || w > ELEV_HL_MAX || h > ELEV_HL_MAX) {
        return;
    }

    for (int i = 0; i < h; i++) {
        memcpy(elev_hl_save + w * i, gElevatorWindowBuffer + pitch * (y + i) + x, w);
    }

    blitBufferToBufferTrans(_elevatorFrmImages[ELEVATOR_FRM_BUTTON_DOWN].getData(), w, h, w, gElevatorWindowBuffer + pitch * y + x, pitch);

    elev_hl_x = x;
    elev_hl_y = y;
    elev_hl_w = w;
    elev_hl_h = h;
}

// Park the cursor on floor button `level` (x 13, y 40 + 60 per level, plus half the button art).
static void elevator_pad_point(int level)
{
    Rect winRect;
    if (windowGetRect(gElevatorWindow, &winRect) != 0) {
        return;
    }

    _mouse_set_position(winRect.left + 13 + _elevatorFrmImages[ELEVATOR_FRM_BUTTON_DOWN].getWidth() / 2,
        winRect.top + 40 + 60 * level + _elevatorFrmImages[ELEVATOR_FRM_BUTTON_DOWN].getHeight() / 2);
}

int elevatorSelectLevel(int elevator, int* mapPtr, int* elevationPtr, int* tilePtr)
{
    if (elevator < 0 || elevator >= ELEVATORS_MAX) {
        trace_event(TRACE_WARN, "elev-refused", "why=type type=%d count=%d", elevator, ELEVATORS_MAX);
        return -1;
    }

    // ⚠️ Take the world prompt down BEFORE the panel paints: the floating hand lives in its own
    // always-on-top window and only gamepad_ui_tick hides it -- which does not run inside this loop,
    // so the hand that pointed at the elevator stayed over the floor buttons ("ta ruka mě sere").
    interact_prompt_reset();

    // SFALL
    if (elevatorWindowInit(elevator) == -1) {
        trace_event(TRACE_WARN, "elev-refused", "why=start type=%d", elevator);
        return -1;
    }

    const ElevatorDescription* elevatorDescription = gElevatorDescriptions[elevator];

    int index;
    for (index = 0; index < ELEVATOR_LEVEL_MAX; index++) {
        if (elevatorDescription[index].map == *mapPtr) {
            break;
        }
    }

    if (index < ELEVATOR_LEVEL_MAX) {
        if (elevatorDescription[*elevationPtr + index].tile != -1) {
            *elevationPtr += index;
        }
    }

    if (elevator == ELEVATOR_SIERRA_2) {
        if (*elevationPtr <= 2) {
            *elevationPtr -= 2;
        } else {
            *elevationPtr -= 3;
        }
    } else if (elevator == ELEVATOR_MILITARY_BASE_LOWER) {
        if (*elevationPtr >= 2) {
            *elevationPtr -= 2;
        }
    } else if (elevator == ELEVATOR_MILITARY_BASE_UPPER && *elevationPtr == 4) {
        *elevationPtr -= 2;
    }

    if (*elevationPtr > 3) {
        *elevationPtr -= 3;
    }

    debugPrint("\n the start elev level %d\n", *elevationPtr);

    int v18 = (_elevatorFrmImages[ELEVATOR_FRM_GAUGE].getWidth() * _elevatorFrmImages[ELEVATOR_FRM_GAUGE].getHeight()) / 13;
    float v42 = 12.0f / (float)(gElevatorLevels[elevator] - 1);
    blitBufferToBuffer(
        _elevatorFrmImages[ELEVATOR_FRM_GAUGE].getData() + v18 * (int)((float)(*elevationPtr) * v42),
        _elevatorFrmImages[ELEVATOR_FRM_GAUGE].getWidth(),
        _elevatorFrmImages[ELEVATOR_FRM_GAUGE].getHeight() / 13,
        _elevatorFrmImages[ELEVATOR_FRM_GAUGE].getWidth(),
        gElevatorWindowBuffer + _elevatorBackgroundFrmImage.getWidth() * 41 + 121,
        _elevatorBackgroundFrmImage.getWidth());
    windowRefresh(gElevatorWindow);

    // ⚠️ THE PANEL HAD NO GAMEPAD AT ALL: its only inputs were ESC and the floor BUTTONS, which a pad
    // reaches solely by happening to leave the cursor on one. Same shape as every other modal this pad
    // drives: self_nav so the gameplay layer stops synthesising world clicks behind the panel, D-pad /
    // stick picks, A confirms, B cancels.
    int sel = *elevationPtr;
    if (sel < 0 || sel >= gElevatorLevels[elevator]) {
        sel = 0;
    }

    SDL_GameController* elevPad = gamepad_ui_get_controller();
    if (elevPad != nullptr) {
        gamepad_ui_set_self_nav(true);
        elevator_pad_point(sel);
        elevator_hl_draw(sel);
        windowRefresh(gElevatorWindow);
    }

    bool prevUp = false;
    bool prevDown = false;
    bool prevA = false;
    bool prevB = false;
    bool aArmed = false; // both buttons must be seen RELEASED before they count -- see below
    bool bArmed = false;
    unsigned int lastStep = 0;

    bool done = false;
    int keyCode;
    while (!done) {
        sharedFpsLimiter.mark();

        // ⚠️ The mark comes down BEFORE anything can draw, and goes back up at the bottom.
        elevator_hl_erase();

        if (elevPad != nullptr) {
            gamepad_ui_set_self_nav(true); // re-assert: nothing else may take the pad while we are up

            SDL_GameControllerUpdate();
            bool upNow = SDL_GameControllerGetButton(elevPad, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0;
            bool downNow = SDL_GameControllerGetButton(elevPad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0;
            bool aNow = SDL_GameControllerGetButton(elevPad, SDL_CONTROLLER_BUTTON_A) != 0;
            bool bNow = SDL_GameControllerGetButton(elevPad, SDL_CONTROLLER_BUTTON_B) != 0;

            // Held D-pad repeats after a beat, like every other list on this pad.
            bool repeat = (upNow || downNow) && getTicksSince(lastStep) >= 220;

            int step = 0;
            if ((upNow && !prevUp) || (upNow && repeat)) {
                step = -1;
            } else if ((downNow && !prevDown) || (downNow && repeat)) {
                step = 1;
            } else {
                // The left stick does it too -- it is the thing your thumb is already on.
                step = gamepad_ui_menu_stick_step();
            }

            if (step != 0) {
                lastStep = getTicks();
                int next = sel + step;
                if (next >= 0 && next < gElevatorLevels[elevator]) {
                    sel = next;
                    soundPlayFile("ib1p1xx1");
                    elevator_pad_point(sel);
                }
            }

            // ⚠️⚠️ THE PRESS THAT OPENED THIS SCREEN IS STILL DOWN, AND IT USED TO CONFIRM: A is held
            // from the world action that ran the elevator script, so the first iteration read a fresh
            // edge, confirmed the floor you were on and closed the panel -- all you saw was the doors.
            // So each button must be RELEASED ONCE before it means anything here.
            if (!aNow) {
                aArmed = true;
            }
            if (!bNow) {
                bArmed = true;
            }

            if (aArmed && aNow && !prevA) {
                keyCode = 500 + sel;
                done = true;
            } else if (bArmed && bNow && !prevB) {
                keyCode = KEY_ESCAPE;
                done = true;
            }

            prevUp = upNow;
            prevDown = downNow;
            prevA = aNow;
            prevB = bNow;

            if (done) {
                renderPresent();
                sharedFpsLimiter.throttle();
                break;
            }
        }

        keyCode = inputGetInput();
        if (keyCode == KEY_ESCAPE) {
            done = true;
        }

        if (keyCode >= 500 && keyCode < 504) {
            done = true;
        }

        if (keyCode > 0 && keyCode < 500) {
            int level = elevatorGetLevelFromKeyCode(elevator, keyCode);
            if (level != 0) {
                keyCode = 500 + level - 1;
                done = true;
            }
        }

        if (!done && elevPad != nullptr) {
            elevator_hl_draw(sel);
            windowRefresh(gElevatorWindow);
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    // Down for good: the travel animation below repaints the gauge and must not find a mark it knows
    // nothing about sitting in the buffer.
    elevator_hl_erase();
    windowRefresh(gElevatorWindow);

    // How the panel was left. A picked= that equals the floor you were already on, one frame after it
    // opened, is the held-A signature.
    trace_event(TRACE_FLOW, "elev-closed", "picked=%d from=%d escaped=%d pad=%d",
        keyCode == KEY_ESCAPE ? -1 : keyCode - 500, *elevationPtr, keyCode == KEY_ESCAPE ? 1 : 0,
        elevPad != nullptr ? 1 : 0);

    if (keyCode != KEY_ESCAPE) {
        keyCode -= 500;

        if (*elevationPtr != keyCode) {
            float v43 = (float)(gElevatorLevels[elevator] - 1) / 12.0f;

            unsigned int delay = (unsigned int)(v43 * 276.92307);

            if (keyCode < *elevationPtr) {
                v43 = -v43;
            }

            int numberOfLevelsTravelled = keyCode - *elevationPtr;
            if (numberOfLevelsTravelled < 0) {
                numberOfLevelsTravelled = -numberOfLevelsTravelled;
            }

            soundPlayFile(gElevatorSoundEffects[gElevatorLevels[elevator] - 2][numberOfLevelsTravelled]);

            float v41 = (float)keyCode * v42;
            float v44 = (float)(*elevationPtr) * v42;
            do {
                sharedFpsLimiter.mark();

                unsigned int tick = getTicks();
                v44 += v43;
                blitBufferToBuffer(
                    _elevatorFrmImages[ELEVATOR_FRM_GAUGE].getData() + v18 * (int)v44,
                    _elevatorFrmImages[ELEVATOR_FRM_GAUGE].getWidth(),
                    _elevatorFrmImages[ELEVATOR_FRM_GAUGE].getHeight() / 13,
                    _elevatorFrmImages[ELEVATOR_FRM_GAUGE].getWidth(),
                    gElevatorWindowBuffer + _elevatorBackgroundFrmImage.getWidth() * 41 + 121,
                    _elevatorBackgroundFrmImage.getWidth());

                windowRefresh(gElevatorWindow);

                delay_ms(delay - (getTicks() - tick));

                renderPresent();
                sharedFpsLimiter.throttle();
            } while ((v43 <= 0.0 || v44 < v41) && (v43 > 0.0 || v44 > v41));

            inputPauseForTocks(200);
        }
    }

    elevatorWindowFree();

    // Hand the pad back. ⚠️ A leaked self_nav means the controller is dead in the game world.
    if (elevPad != nullptr) {
        gamepad_ui_set_self_nav(false);
        gamepad_ui_bump_input_epoch();
    }

    if (keyCode != KEY_ESCAPE) {
        const ElevatorDescription* description = &(elevatorDescription[keyCode]);
        *mapPtr = description->map;
        *elevationPtr = description->elevation;
        *tilePtr = description->tile;
    }

    return 0;
}

// 0x43F324
static int elevatorWindowInit(int elevator)
{
    gElevatorWindowIsoWasEnabled = isoDisable();
    colorCycleDisable();

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
    gameMouseObjectsHide();

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
    scriptsDisable();

    int index;
    for (index = 0; index < ELEVATOR_FRM_COUNT; index++) {
        int fid = buildFid(OBJ_TYPE_INTERFACE, gElevatorFrmIds[index], 0, 0, 0);
        if (!_elevatorFrmImages[index].lock(fid)) {
            break;
        }
    }

    if (index != ELEVATOR_FRM_COUNT) {
        for (int reversedIndex = index - 1; reversedIndex >= 0; reversedIndex--) {
            _elevatorFrmImages[reversedIndex].unlock();
        }

        if (gElevatorWindowIsoWasEnabled) {
            isoEnable();
        }

        colorCycleEnable();
        gameMouseSetCursor(MOUSE_CURSOR_ARROW);
        return -1;
    }

    const ElevatorBackground* elevatorBackground = &(gElevatorBackgrounds[elevator]);
    bool backgroundsLoaded = true;

    int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, elevatorBackground->backgroundFrmId, 0, 0, 0);
    if (_elevatorBackgroundFrmImage.lock(backgroundFid)) {
        if (elevatorBackground->panelFrmId != -1) {
            int panelFid = buildFid(OBJ_TYPE_INTERFACE, elevatorBackground->panelFrmId, 0, 0, 0);
            if (!_elevatorPanelFrmImage.lock(panelFid)) {
                backgroundsLoaded = false;
            }
        }
    } else {
        backgroundsLoaded = false;
    }

    if (!backgroundsLoaded) {
        _elevatorBackgroundFrmImage.unlock();
        _elevatorPanelFrmImage.unlock();

        for (int index = 0; index < ELEVATOR_FRM_COUNT; index++) {
            _elevatorFrmImages[index].unlock();
        }

        if (gElevatorWindowIsoWasEnabled) {
            isoEnable();
        }

        colorCycleEnable();
        gameMouseSetCursor(MOUSE_CURSOR_ARROW);
        return -1;
    }

    int elevatorWindowX = (screenGetWidth() - _elevatorBackgroundFrmImage.getWidth()) / 2;
    int elevatorWindowY = (screenGetHeight() - INTERFACE_BAR_HEIGHT - 1 - _elevatorBackgroundFrmImage.getHeight()) / 2;
    gElevatorWindow = windowCreate(
        elevatorWindowX,
        elevatorWindowY,
        _elevatorBackgroundFrmImage.getWidth(),
        _elevatorBackgroundFrmImage.getHeight(),
        256,
        WINDOW_MODAL | WINDOW_DONT_MOVE_TOP);
    if (gElevatorWindow == -1) {
        _elevatorBackgroundFrmImage.unlock();
        _elevatorPanelFrmImage.unlock();

        for (int index = 0; index < ELEVATOR_FRM_COUNT; index++) {
            _elevatorFrmImages[index].unlock();
        }

        if (gElevatorWindowIsoWasEnabled) {
            isoEnable();
        }

        colorCycleEnable();
        gameMouseSetCursor(MOUSE_CURSOR_ARROW);
        return -1;
    }

    gElevatorWindowBuffer = windowGetBuffer(gElevatorWindow);
    memcpy(gElevatorWindowBuffer, _elevatorBackgroundFrmImage.getData(), _elevatorBackgroundFrmImage.getWidth() * _elevatorBackgroundFrmImage.getHeight());

    if (_elevatorPanelFrmImage.isLocked()) {
        blitBufferToBuffer(_elevatorPanelFrmImage.getData(),
            _elevatorPanelFrmImage.getWidth(),
            _elevatorPanelFrmImage.getHeight(),
            _elevatorPanelFrmImage.getWidth(),
            gElevatorWindowBuffer + _elevatorBackgroundFrmImage.getWidth() * (_elevatorBackgroundFrmImage.getHeight() - _elevatorPanelFrmImage.getHeight()),
            _elevatorBackgroundFrmImage.getWidth());
    }

    int y = 40;
    for (int level = 0; level < gElevatorLevels[elevator]; level++) {
        int btn = buttonCreate(gElevatorWindow,
            13,
            y,
            _elevatorFrmImages[ELEVATOR_FRM_BUTTON_DOWN].getWidth(),
            _elevatorFrmImages[ELEVATOR_FRM_BUTTON_DOWN].getHeight(),
            -1,
            -1,
            -1,
            500 + level,
            _elevatorFrmImages[ELEVATOR_FRM_BUTTON_UP].getData(),
            _elevatorFrmImages[ELEVATOR_FRM_BUTTON_DOWN].getData(),
            nullptr,
            BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            buttonSetCallbacks(btn, _gsound_red_butt_press, nullptr);
        }
        y += 60;
    }

    return 0;
}

// 0x43F6D0
static void elevatorWindowFree()
{
    windowDestroy(gElevatorWindow);

    _elevatorBackgroundFrmImage.unlock();
    _elevatorPanelFrmImage.unlock();

    for (int index = 0; index < ELEVATOR_FRM_COUNT; index++) {
        _elevatorFrmImages[index].unlock();
    }

    scriptsEnable();

    if (gElevatorWindowIsoWasEnabled) {
        isoEnable();
    }

    colorCycleEnable();

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);
}

// 0x43F73C
static int elevatorGetLevelFromKeyCode(int elevator, int keyCode)
{
    // TODO: Check if result is really unused?
    toupper(keyCode);

    for (int index = 0; index < ELEVATOR_LEVEL_MAX; index++) {
        char c = gElevatorLevelLabels[elevator][index];
        if (c == '\0') {
            break;
        }

        if (c == (char)(keyCode & 0xFF)) {
            return index + 1;
        }
    }
    return 0;
}

void elevatorsInit()
{
    char* elevatorsFileName;
    configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_ELEVATORS_FILE_KEY, &elevatorsFileName);
    if (elevatorsFileName != nullptr && *elevatorsFileName == '\0') {
        elevatorsFileName = nullptr;
    }

    if (elevatorsFileName != nullptr) {
        Config elevatorsConfig;
        if (configInit(&elevatorsConfig)) {
            if (configRead(&elevatorsConfig, elevatorsFileName, false)) {
                char sectionKey[4];
                char key[32];
                for (int index = 0; index < ELEVATORS_MAX; index++) {
                    snprintf(sectionKey, sizeof(sectionKey), "%d", index);

                    if (index >= ELEVATOR_COUNT) {
                        int levels = 0;
                        configGetInt(&elevatorsConfig, sectionKey, "ButtonCount", &levels);
                        gElevatorLevels[index] = std::clamp(levels, 2, ELEVATOR_LEVEL_MAX);
                    }

                    configGetInt(&elevatorsConfig, sectionKey, "MainFrm", &(gElevatorBackgrounds[index].backgroundFrmId));
                    configGetInt(&elevatorsConfig, sectionKey, "ButtonsFrm", &(gElevatorBackgrounds[index].panelFrmId));

                    for (int level = 0; level < ELEVATOR_LEVEL_MAX; level++) {
                        snprintf(key, sizeof(key), "ID%d", level + 1);
                        configGetInt(&elevatorsConfig, sectionKey, key, &(gElevatorDescriptions[index][level].map));

                        snprintf(key, sizeof(key), "Elevation%d", level + 1);
                        configGetInt(&elevatorsConfig, sectionKey, key, &(gElevatorDescriptions[index][level].elevation));

                        snprintf(key, sizeof(key), "Tile%d", level + 1);
                        configGetInt(&elevatorsConfig, sectionKey, key, &(gElevatorDescriptions[index][level].tile));
                    }
                }

                // NOTE: Sfall implementation is slightly different. It uses one
                // loop and stores `type` value in a separate lookup table. This
                // value is then used in the certain places to remap from
                // requested elevator to the new one.
                for (int index = 0; index < ELEVATORS_MAX; index++) {
                    snprintf(sectionKey, sizeof(sectionKey), "%d", index);

                    int type;
                    if (configGetInt(&elevatorsConfig, sectionKey, "Image", &type)) {
                        type = std::clamp(type, 0, ELEVATORS_MAX - 1);
                        if (index != type) {
                            memcpy(&(gElevatorBackgrounds[index]), &(gElevatorBackgrounds[type]), sizeof(*gElevatorBackgrounds));
                            memcpy(&(gElevatorLevels[index]), &(gElevatorLevels[type]), sizeof(*gElevatorLevels));
                            memcpy(&(gElevatorLevelLabels[index]), &(gElevatorLevelLabels[type]), sizeof(*gElevatorLevelLabels));
                        }
                    }
                }
            }

            configFree(&elevatorsConfig);
        }
    }
}

} // namespace fallout
