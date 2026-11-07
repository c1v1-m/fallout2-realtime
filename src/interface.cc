#include "interface.h"

#include <stdio.h>
#include <string.h>

#include "animation.h"
#include "art.h"
#include "color.h"
#include "combat.h"
#include "config.h"
#include "critter.h"
#include "cycle.h"
#include "debug.h"
#include "display_monitor.h"
#include "draw.h"
#include "endgame.h"
#include "game.h"
#include "game_mouse.h"
#include "game_sound.h"
#include "geometry.h"
#include "input.h"
#include "item.h"
#include "kb.h"
#include "memory.h"
#include "mouse.h"
#include "object.h"
#include "platform_compat.h"
#include "proto.h"
#include "proto_instance.h"
#include "proto_types.h"
#include "skill.h"
#include "stat.h"
#include "svga.h"
#include "text_font.h"
#include "tile.h"
#include "window_manager.h"

// Realtime (fallout1-ce's intface.cc changes, ported).
#include "rt/game/combat_rt.h"
#include "rt/game/game_trace.h"
#include "rt/game/gamepad_ui.h"
#include "rt/rt_hooks.h"

namespace fallout {

// The width of connectors in the indicator box.
//
// There are male connectors on the left, and female connectors on the right.
// When displaying series of boxes they appear to be plugged into a chain.
#define INDICATOR_BOX_CONNECTOR_WIDTH 3

// Minimum radiation amount to display RADIATED indicator.
#define RADATION_INDICATOR_THRESHOLD 65

// Minimum poison amount to display POISONED indicator.
#define POISON_INDICATOR_THRESHOLD 0

// The maximum number of indicator boxes the indicator bar can display.
//
// For unknown reason this number is 6, even though there are only 5 different
// indicator types. In addition to that, default screen width 640px cannot hold
// 6 boxes 130px each.
#define INDICATOR_SLOTS_COUNT (6)

// The values of it's members are offsets to beginning of numbers in
// numbers.frm.
typedef enum InterfaceNumbersColor {
    INTERFACE_NUMBERS_COLOR_WHITE = 0,
    INTERFACE_NUMBERS_COLOR_YELLOW = 120,
    INTERFACE_NUMBERS_COLOR_RED = 240,
} InterfaceNumbersColor;

// Available indicators.
//
// Indicator boxes in the bar are displayed according to the order of this enum.
typedef enum Indicator {
    INDICATOR_ADDICT,
    INDICATOR_SNEAK,
    INDICATOR_LEVEL,
    INDICATOR_POISONED,
    INDICATOR_RADIATED,
    INDICATOR_COUNT,
} Indicator;

// Provides metadata about indicator boxes.
typedef struct IndicatorDescription {
    // An identifier of title in `intrface.msg`.
    int title;

    // A flag denoting this box represents something harmful to the player. It
    // affects color of the title.
    bool isBad;

    // Prerendered indicator data.
    //
    // This value is provided at runtime during indicator box initialization.
    // It includes indicator box background with it's title positioned in the
    // center and is green colored if indicator is good, or red otherwise, as
    // denoted by [isBad] property.
    unsigned char* data;
} IndicatorDescription;

typedef struct InterfaceItemState {
    Object* item;
    unsigned char isDisabled;
    unsigned char isWeapon;
    int primaryHitMode;
    int secondaryHitMode;
    int action;
    int itemFid;
} InterfaceItemState;

static int _intface_redraw_items_callback(Object* a1, Object* a2);
static int _intface_change_fid_callback(Object* a1, Object* a2);
static void interfaceBarSwapHandsAnimatePutAwayTakeOutSequence(int previousWeaponAnimationCode, int weaponAnimationCode);
static int intface_init_items();
static int interfaceBarRefreshMainAction();
static void intface_build_ap_lights();
static int endTurnButtonInit();
static int endTurnButtonFree();
static int endCombatButtonInit();
static int endCombatButtonFree();
static void interfaceUpdateAmmoBar(int x, int ratio);
static int _intface_item_reload();
static void interfaceRenderCounter(int x, int y, int previousValue, int value, int offset, int delay);
static int intface_fatal_error(int rc);
static int indicatorBarInit();
static void interfaceBarFree();
static void indicatorBarReset();
static int indicatorBoxCompareByPosition(const void* a, const void* b);
static void indicatorBarRender(int count);
static bool indicatorBarAdd(int indicator);

static void customInterfaceBarInit();
static void customInterfaceBarExit();

static void sidePanelsInit();
static void sidePanelsExit();
static void sidePanelsHide();
static void sidePanelsShow();
static void sidePanelsDraw(const char* path, int win, bool isLeading);

// 0x518F08
static bool gInterfaceBarInitialized = false;

// 0x518F0C
static bool gInterfaceBarSwapHandsInProgress = false;

// 0x518F10
static bool gInterfaceBarEnabled = false;

// 0x518F14
static bool gInterfaceBarHidden = false;

// 0x518F18
static int gInventoryButton = -1;

// 0x518F24
static int gOptionsButton = -1;

// 0x518F30
static int gSkilldexButton = -1;

// 0x518F40
static int gMapButton = -1;

// 0x518F50
static int gPipboyButton = -1;

// 0x518F5C
static int gCharacterButton = -1;

// 0x518F68
static int gSingleAttackButton = -1;

// 0x518F78
static int gInterfaceCurrentHand = HAND_LEFT;

// 0x518F7C
static Rect gInterfaceBarMainActionRect;

// 0x518F8C
static int gChangeHandsButton = -1;

// 0x518F9C
static bool gInterfaceBarEndButtonsIsVisible = false;

// Combat mode curtains rect.
//
// 0x518FA0
static Rect gInterfaceBarEndButtonsRect;

// 0x518FB0
static int gEndTurnButton = -1;

// 0x518FBC
static int gEndCombatButton = -1;

// 0x518FD4
static Rect gInterfaceBarActionPointsBarRect;

// 0x518FE8
static IndicatorDescription gIndicatorDescriptions[INDICATOR_COUNT] = {
    { 102, true, nullptr }, // ADDICT
    { 100, false, nullptr }, // SNEAK
    { 101, false, nullptr }, // LEVEL
    { 103, true, nullptr }, // POISONED
    { 104, true, nullptr }, // RADIATED
};

// 0x519024
int gInterfaceBarWindow = -1;

// 0x519028
static int gIndicatorBarWindow = -1;

// Last hit points rendered in interface.
//
// Used to animate changes.
//
// 0x51902C
static int gInterfaceLastRenderedHitPoints = 0;

// Last color used to render hit points in interface.
//
// Used to animate changes.
//
// 0x519030
static int gInterfaceLastRenderedHitPointsColor = INTERFACE_NUMBERS_COLOR_RED;

// Last armor class rendered in interface.
//
// Used to animate changes.
//
// 0x519034
static int gInterfaceLastRenderedArmorClass = 0;

// Each slot contains one of indicators or -1 if slot is empty.
//
// 0x5970E0
static int gIndicatorSlots[INDICATOR_SLOTS_COUNT];

// 0x5970F8
static InterfaceItemState gInterfaceItemStates[HAND_COUNT];

// 0x597138
static bool gIndicatorBarIsVisible;

// 0x597154
static unsigned char _itemButtonDown[188 * 67];

// 0x59A2B4
static unsigned char _itemButtonUp[188 * 67];

// 0x59D3F4
static unsigned char* gInterfaceWindowBuffer;

// A slice of main interface background containing 10 shadowed action point
// dots. In combat mode individual colored dots are rendered on top of this
// background.
//
// This buffer is initialized once and does not change throughout the game.
//
// 0x59D40C
static unsigned char gInterfaceActionPointsBarBackground[90 * 5];

// Should the game window stretch all the way to the bottom or sit at the top of the interface bar (default)
bool gInterfaceBarMode = false;

static FrmImage _inventoryButtonNormalFrmImage;
static FrmImage _inventoryButtonPressedFrmImage;
static FrmImage _optionsButtonNormalFrmImage;
static FrmImage _optionsButtonPressedFrmImage;
static FrmImage _skilldexButtonNormalFrmImage;
static FrmImage _skilldexButtonPressedFrmImage;
static FrmImage _skilldexButtonMaskFrmImage;
static FrmImage _mapButtonNormalFrmImage;
static FrmImage _mapButtonPressedFrmImage;
static FrmImage _mapButtonMaskFrmImage;
static FrmImage _pipboyButtonNormalFrmImage;
static FrmImage _pipboyButtonPressedFrmImage;
static FrmImage _characterButtonNormalFrmImage;
static FrmImage _characterButtonPressedFrmImage;
static FrmImage _changeHandsButtonNormalFrmImage;
static FrmImage _changeHandsButtonPressedFrmImage;
static FrmImage _changeHandsButtonMaskFrmImage;
static FrmImage _itemButtonNormalFrmImage;
static FrmImage _itemButtonPressedFrmImage;
static FrmImage _itemButtonDisabledFrmImage;
static FrmImage _endTurnButtonNormalFrmImage;
static FrmImage _endTurnButtonPressedFrmImage;
static FrmImage _endCombatButtonNormalFrmImage;
static FrmImage _endCombatButtonPressedFrmImage;
static FrmImage _numbersFrmImage;
static FrmImage _greenLightFrmImage;
static FrmImage _yellowLightFrmImage;
static FrmImage _redLightFrmImage;

int gInterfaceBarContentOffset = 0;
int gInterfaceBarWidth = -1;
bool gInterfaceBarIsCustom = false;
static Art* gCustomInterfaceBarBackground = nullptr;

// Realtime build: the IFACE_BAR_WIDTH f2_res.ini asked for, when svga.cc had to narrow the bar to leave
// the LB / RB slots their room (0 = not narrowed). customInterfaceBarInit then cuts the narrower bar
// out of that wider art -- see interface_bar_cut.
int gInterfaceBarSourceWidth = 0;
static unsigned char* gCustomInterfaceBarCut = nullptr;

int gInterfaceSidePanelsImageId = 2;
bool gInterfaceSidePanelsExtendFromScreenEdge = false;
static int gInterfaceSidePanelsLeadingWindow = -1;
static int gInterfaceSidePanelsTrailingWindow = -1;

// intface_init
// 0x45D880
int interfaceInit()
{
    int fid;

    if (gInterfaceBarWindow != -1) {
        return -1;
    }

    customInterfaceBarInit();

    gInterfaceBarActionPointsBarRect = { 316 + gInterfaceBarContentOffset, 14, 406 + gInterfaceBarContentOffset, 19 };
    gInterfaceBarEndButtonsRect = { 580 + gInterfaceBarContentOffset, 38, 637 + gInterfaceBarContentOffset, 96 };
    gInterfaceBarMainActionRect = { 267 + gInterfaceBarContentOffset, 26, 455 + gInterfaceBarContentOffset, 93 };

    gInterfaceBarInitialized = true;

    int interfaceBarWindowX = (screenGetWidth() - gInterfaceBarWidth) / 2;
    int interfaceBarWindowY = screenGetHeight() - INTERFACE_BAR_HEIGHT;

    gInterfaceBarWindow = windowCreate(interfaceBarWindowX, interfaceBarWindowY, gInterfaceBarWidth, INTERFACE_BAR_HEIGHT, _colorTable[0], WINDOW_HIDDEN);
    if (gInterfaceBarWindow == -1) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    gInterfaceWindowBuffer = windowGetBuffer(gInterfaceBarWindow);
    if (gInterfaceWindowBuffer == nullptr) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    if (gInterfaceBarIsCustom) {
        blitBufferToBuffer(customInterfaceBarGetBackgroundImageData(), gInterfaceBarWidth, INTERFACE_BAR_HEIGHT - 1, gInterfaceBarWidth, gInterfaceWindowBuffer, gInterfaceBarWidth);
    } else {
        FrmImage backgroundFrmImage;
        fid = buildFid(OBJ_TYPE_INTERFACE, 16, 0, 0, 0);
        if (!backgroundFrmImage.lock(fid)) {
            return intface_fatal_error(-1);
        }

        blitBufferToBuffer(backgroundFrmImage.getData(), gInterfaceBarWidth, INTERFACE_BAR_HEIGHT - 1, gInterfaceBarWidth, gInterfaceWindowBuffer, gInterfaceBarWidth);
        backgroundFrmImage.unlock();
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 47, 0, 0, 0);
    if (!_inventoryButtonNormalFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 46, 0, 0, 0);
    if (!_inventoryButtonPressedFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    gInventoryButton = buttonCreate(gInterfaceBarWindow, 211 + gInterfaceBarContentOffset, 40, 32, 21, -1, -1, -1, KEY_LOWERCASE_I, _inventoryButtonNormalFrmImage.getData(), _inventoryButtonPressedFrmImage.getData(), nullptr, 0);
    if (gInventoryButton == -1) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    buttonSetCallbacks(gInventoryButton, _gsound_med_butt_press, _gsound_med_butt_release);

    fid = buildFid(OBJ_TYPE_INTERFACE, 18, 0, 0, 0);
    if (!_optionsButtonNormalFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 17, 0, 0, 0);
    if (!_optionsButtonPressedFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    gOptionsButton = buttonCreate(gInterfaceBarWindow, 210 + gInterfaceBarContentOffset, 61, 34, 34, -1, -1, -1, KEY_LOWERCASE_O, _optionsButtonNormalFrmImage.getData(), _optionsButtonPressedFrmImage.getData(), nullptr, 0);
    if (gOptionsButton == -1) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    buttonSetCallbacks(gOptionsButton, _gsound_med_butt_press, _gsound_med_butt_release);

    fid = buildFid(OBJ_TYPE_INTERFACE, 6, 0, 0, 0);
    if (!_skilldexButtonNormalFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 7, 0, 0, 0);
    if (!_skilldexButtonPressedFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 6, 0, 0, 0);
    if (!_skilldexButtonMaskFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    gSkilldexButton = buttonCreate(gInterfaceBarWindow, 523 + gInterfaceBarContentOffset, 6, 22, 21, -1, -1, -1, KEY_LOWERCASE_S, _skilldexButtonNormalFrmImage.getData(), _skilldexButtonPressedFrmImage.getData(), nullptr, BUTTON_FLAG_TRANSPARENT);
    if (gSkilldexButton == -1) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    buttonSetMask(gSkilldexButton, _skilldexButtonMaskFrmImage.getData());
    buttonSetCallbacks(gSkilldexButton, _gsound_med_butt_press, _gsound_med_butt_release);

    fid = buildFid(OBJ_TYPE_INTERFACE, 13, 0, 0, 0);
    if (!_mapButtonNormalFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 10, 0, 0, 0);
    if (!_mapButtonPressedFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 13, 0, 0, 0);
    if (!_mapButtonMaskFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    gMapButton = buttonCreate(gInterfaceBarWindow, 526 + gInterfaceBarContentOffset, 39, 41, 19, -1, -1, -1, KEY_TAB, _mapButtonNormalFrmImage.getData(), _mapButtonPressedFrmImage.getData(), nullptr, BUTTON_FLAG_TRANSPARENT);
    if (gMapButton == -1) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    buttonSetMask(gMapButton, _mapButtonMaskFrmImage.getData());
    buttonSetCallbacks(gMapButton, _gsound_med_butt_press, _gsound_med_butt_release);

    fid = buildFid(OBJ_TYPE_INTERFACE, 59, 0, 0, 0);
    if (!_pipboyButtonNormalFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 58, 0, 0, 0);
    if (!_pipboyButtonPressedFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    gPipboyButton = buttonCreate(gInterfaceBarWindow, 526 + gInterfaceBarContentOffset, 77, 41, 19, -1, -1, -1, KEY_LOWERCASE_P, _pipboyButtonNormalFrmImage.getData(), _pipboyButtonPressedFrmImage.getData(), nullptr, 0);
    if (gPipboyButton == -1) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    buttonSetMask(gPipboyButton, _mapButtonMaskFrmImage.getData());
    buttonSetCallbacks(gPipboyButton, _gsound_med_butt_press, _gsound_med_butt_release);

    fid = buildFid(OBJ_TYPE_INTERFACE, 57, 0, 0, 0);
    if (!_characterButtonNormalFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 56, 0, 0, 0);
    if (!_characterButtonPressedFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    gCharacterButton = buttonCreate(gInterfaceBarWindow, 526 + gInterfaceBarContentOffset, 58, 41, 19, -1, -1, -1, KEY_LOWERCASE_C, _characterButtonNormalFrmImage.getData(), _characterButtonPressedFrmImage.getData(), nullptr, 0);
    if (gCharacterButton == -1) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    buttonSetMask(gCharacterButton, _mapButtonMaskFrmImage.getData());
    buttonSetCallbacks(gCharacterButton, _gsound_med_butt_press, _gsound_med_butt_release);

    fid = buildFid(OBJ_TYPE_INTERFACE, 32, 0, 0, 0);
    if (!_itemButtonNormalFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 31, 0, 0, 0);
    if (!_itemButtonPressedFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 73, 0, 0, 0);
    if (!_itemButtonDisabledFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    memcpy(_itemButtonUp, _itemButtonNormalFrmImage.getData(), sizeof(_itemButtonUp));
    memcpy(_itemButtonDown, _itemButtonPressedFrmImage.getData(), sizeof(_itemButtonDown));

    gSingleAttackButton = buttonCreate(gInterfaceBarWindow, 267 + gInterfaceBarContentOffset, 26, 188, 67, -1, -1, -1, -20, _itemButtonUp, _itemButtonDown, nullptr, BUTTON_FLAG_TRANSPARENT);
    if (gSingleAttackButton == -1) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    buttonSetRightMouseCallbacks(gSingleAttackButton, -1, KEY_LOWERCASE_N, nullptr, nullptr);
    buttonSetCallbacks(gSingleAttackButton, _gsound_lrg_butt_press, _gsound_lrg_butt_release);

    fid = buildFid(OBJ_TYPE_INTERFACE, 6, 0, 0, 0);
    if (!_changeHandsButtonNormalFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 7, 0, 0, 0);
    if (!_changeHandsButtonPressedFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 6, 0, 0, 0);
    if (!_changeHandsButtonMaskFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    // Swap hands button
    gChangeHandsButton = buttonCreate(gInterfaceBarWindow, 218 + gInterfaceBarContentOffset, 6, 22, 21, -1, -1, -1, KEY_LOWERCASE_B, _changeHandsButtonNormalFrmImage.getData(), _changeHandsButtonPressedFrmImage.getData(), nullptr, BUTTON_FLAG_TRANSPARENT);
    if (gChangeHandsButton == -1) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    buttonSetMask(gChangeHandsButton, _changeHandsButtonMaskFrmImage.getData());
    buttonSetCallbacks(gChangeHandsButton, _gsound_med_butt_press, _gsound_med_butt_release);

    fid = buildFid(OBJ_TYPE_INTERFACE, 82, 0, 0, 0);
    if (!_numbersFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 83, 0, 0, 0);
    if (!_greenLightFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 84, 0, 0, 0);
    if (!_yellowLightFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 85, 0, 0, 0);
    if (!_redLightFrmImage.lock(fid)) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    // The two-AP bulb, built from the green one (realtime's AP strip shows up to twenty points).
    intface_build_ap_lights();

    // MODS "Combat box sound" -- default OFF (his setting); 1 = the vanilla clunk and chime.
    {
        int boxSound = 0;
        configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_COMBAT_BOX_SOUND_KEY, &boxSound);
        intface_set_combat_box_sound(boxSound != 0);
    }

    blitBufferToBuffer(gInterfaceWindowBuffer + gInterfaceBarWidth * 14 + 316 + gInterfaceBarContentOffset, 90, 5, gInterfaceBarWidth, gInterfaceActionPointsBarBackground, 90);

    if (indicatorBarInit() == -1) {
        // NOTE: Uninline.
        return intface_fatal_error(-1);
    }

    gInterfaceCurrentHand = HAND_LEFT;

    // NOTE: Uninline.
    intface_init_items();

    displayMonitorInit();

    // SFALL
    sidePanelsInit();

    gInterfaceBarEnabled = true;
    gInterfaceBarInitialized = false;
    gInterfaceBarHidden = true;

    return 0;
}

// 0x45E3D0
void interfaceReset()
{
    interfaceBarEnable();

    // NOTE: Uninline.
    interfaceBarHide();

    indicatorBarRefresh();
    displayMonitorReset();

    // NOTE: Uninline a seemingly inlined routine.
    indicatorBarReset();

    gInterfaceCurrentHand = 0;

    // A new game or a load: no fight is live, so the combat box shuts without its animation.
    intface_rt_combat_box_reset();
}

// 0x45E440
void interfaceFree()
{
    if (gInterfaceBarWindow != -1) {
        // SFALL
        sidePanelsExit();

        displayMonitorExit();

        _redLightFrmImage.unlock();
        _yellowLightFrmImage.unlock();
        _greenLightFrmImage.unlock();

        _numbersFrmImage.unlock();

        if (gChangeHandsButton != -1) {
            buttonDestroy(gChangeHandsButton);
            gChangeHandsButton = -1;
        }

        _changeHandsButtonMaskFrmImage.unlock();
        _changeHandsButtonPressedFrmImage.unlock();
        _changeHandsButtonNormalFrmImage.unlock();

        if (gSingleAttackButton != -1) {
            buttonDestroy(gSingleAttackButton);
            gSingleAttackButton = -1;
        }

        _itemButtonDisabledFrmImage.unlock();
        _itemButtonPressedFrmImage.unlock();
        _itemButtonNormalFrmImage.unlock();

        if (gCharacterButton != -1) {
            buttonDestroy(gCharacterButton);
            gCharacterButton = -1;
        }

        _characterButtonPressedFrmImage.unlock();
        _characterButtonNormalFrmImage.unlock();

        if (gPipboyButton != -1) {
            buttonDestroy(gPipboyButton);
            gPipboyButton = -1;
        }

        _pipboyButtonPressedFrmImage.unlock();
        _pipboyButtonNormalFrmImage.unlock();

        if (gMapButton != -1) {
            buttonDestroy(gMapButton);
            gMapButton = -1;
        }

        _mapButtonMaskFrmImage.unlock();
        _mapButtonPressedFrmImage.unlock();
        _mapButtonNormalFrmImage.unlock();

        if (gSkilldexButton != -1) {
            buttonDestroy(gSkilldexButton);
            gSkilldexButton = -1;
        }

        _skilldexButtonMaskFrmImage.unlock();
        _skilldexButtonPressedFrmImage.unlock();
        _skilldexButtonNormalFrmImage.unlock();

        if (gOptionsButton != -1) {
            buttonDestroy(gOptionsButton);
            gOptionsButton = -1;
        }

        _optionsButtonPressedFrmImage.unlock();
        _optionsButtonNormalFrmImage.unlock();

        if (gInventoryButton != -1) {
            buttonDestroy(gInventoryButton);
            gInventoryButton = -1;
        }

        _inventoryButtonPressedFrmImage.unlock();
        _inventoryButtonNormalFrmImage.unlock();

        if (gInterfaceBarWindow != -1) {
            windowDestroy(gInterfaceBarWindow);
            gInterfaceBarWindow = -1;
        }
    }

    customInterfaceBarExit();

    interfaceBarFree();
}

// 0x45E860
int interfaceLoad(File* stream)
{
    if (gInterfaceBarWindow == -1) {
        if (interfaceInit() == -1) {
            return -1;
        }
    }

    bool interfaceBarEnabled;
    if (fileReadBool(stream, &interfaceBarEnabled) == -1) return -1;

    bool interfaceBarHidden;
    if (fileReadBool(stream, &interfaceBarHidden) == -1) return -1;

    int interfaceCurrentHand;
    if (fileReadInt32(stream, &interfaceCurrentHand) == -1) return -1;

    bool interfaceBarEndButtonsIsVisible;
    if (fileReadBool(stream, &interfaceBarEndButtonsIsVisible) == -1) return -1;

    if (!gInterfaceBarEnabled) {
        interfaceBarEnable();
    }

    if (interfaceBarHidden) {
        // NOTE: Uninline.
        interfaceBarHide();
    } else {
        interfaceBarShow();
    }

    interfaceRenderHitPoints(false);
    interfaceRenderArmorClass(false);

    gInterfaceCurrentHand = interfaceCurrentHand;

    interfaceUpdateItems(false, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);

    if (interfaceBarEndButtonsIsVisible != gInterfaceBarEndButtonsIsVisible) {
        if (interfaceBarEndButtonsIsVisible) {
            interfaceBarEndButtonsShow(false);
        } else {
            interfaceBarEndButtonsHide(false);
        }
    }

    if (!interfaceBarEnabled) {
        interfaceBarDisable();
    }

    indicatorBarRefresh();

    windowRefresh(gInterfaceBarWindow);

    return 0;
}

// 0x45E988
int interfaceSave(File* stream)
{
    if (gInterfaceBarWindow == -1) {
        return -1;
    }

    if (fileWriteBool(stream, gInterfaceBarEnabled) == -1) return -1;
    if (fileWriteBool(stream, gInterfaceBarHidden) == -1) return -1;
    if (fileWriteInt32(stream, gInterfaceCurrentHand) == -1) return -1;
    if (fileWriteBool(stream, gInterfaceBarEndButtonsIsVisible) == -1) return -1;

    return 0;
}

// NOTE: Inlined.
//
// 0x45E9E0
void interfaceBarHide()
{
    if (gInterfaceBarWindow != -1) {
        if (!gInterfaceBarHidden) {
            windowHide(gInterfaceBarWindow);
            gInterfaceBarHidden = true;
        }
    }

    // SFALL
    sidePanelsHide();

    indicatorBarRefresh();
}

// 0x45EA10
void interfaceBarShow()
{
    if (gInterfaceBarWindow != -1) {
        if (gInterfaceBarHidden) {
            interfaceUpdateItems(false, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
            interfaceRenderHitPoints(false);
            interfaceRenderArmorClass(false);
            windowShow(gInterfaceBarWindow);
            sidePanelsShow();
            gInterfaceBarHidden = false;
        }
    }

    // SFALL
    sidePanelsShow();

    indicatorBarRefresh();
}

// 0x45EA64
void interfaceBarEnable()
{
    if (!gInterfaceBarEnabled) {
        buttonEnable(gInventoryButton);
        buttonEnable(gOptionsButton);
        buttonEnable(gSkilldexButton);
        buttonEnable(gMapButton);
        buttonEnable(gPipboyButton);
        buttonEnable(gCharacterButton);

        if (gInterfaceItemStates[gInterfaceCurrentHand].isDisabled == 0) {
            buttonEnable(gSingleAttackButton);
        }

        buttonEnable(gEndTurnButton);
        buttonEnable(gEndCombatButton);
        displayMonitorEnable();

        gInterfaceBarEnabled = true;
    }
}

// 0x45EAFC
void interfaceBarDisable()
{
    if (gInterfaceBarEnabled) {
        displayMonitorDisable();
        buttonDisable(gInventoryButton);
        buttonDisable(gOptionsButton);
        buttonDisable(gSkilldexButton);
        buttonDisable(gMapButton);
        buttonDisable(gPipboyButton);
        buttonDisable(gCharacterButton);
        if (gInterfaceItemStates[gInterfaceCurrentHand].isDisabled == 0) {
            buttonDisable(gSingleAttackButton);
        }
        buttonDisable(gEndTurnButton);
        buttonDisable(gEndCombatButton);
        gInterfaceBarEnabled = false;
    }
}

// 0x45EB90
bool interfaceBarEnabled()
{
    return gInterfaceBarEnabled;
}

// --- REALTIME (fallout1-ce's intface.cc changes, ported) ---------------------------------------------

// THE COMBAT BOX IN REALTIME. Vanilla opens the end-turn/end-combat box (its curtains, endanim.frm)
// when a fight begins and shuts it when it ends; realtime never runs _combat_begin, so it stayed shut.
// combat_rt calls intface_rt_combat_box_tick every frame with whether a fight is ENGAGED; the box
// opens frame by frame, lights up green, and shuts RT_BOX_CLOSE_DELAY after the fight cools (so a
// one-frame lull never flaps it). Paint only -- the buttons themselves are turn-based and stay down.
#define RT_BOX_CLOSE_DELAY 0.75f

static int rt_box_frame = 0; // endanim frame painted now; 0 = closed, which is the bar's own art
static int rt_box_dir = 0; // +1 opening, -1 closing, 0 settled
static bool rt_box_want = false;
static bool rt_box_lit = false;
static bool rt_box_dirty = false; // a bar repaint may have wiped it: paint the current state again
static float rt_box_accum = 0.0f;
static float rt_box_cold_for = 0.0f;

// MODS "Combat box sound" (cfg combat_box_sound): the open/close clunk and the lights chime.
static bool rt_box_sound = true;

bool intface_get_combat_box_sound()
{
    return rt_box_sound;
}

void intface_set_combat_box_sound(bool on)
{
    rt_box_sound = on;
}

// Is there a strip right of the bar wide enough for the RB slot? (Fallout 1's SIDE_PANEL_MIN_WIDTH.)
// Without one the slot sits on the end-turn box, and the combat curtains stand down for it.
static bool intface_side_panel_ok()
{
    return gInterfaceSidePanelsTrailingWindow != -1 && windowGetWidth(gInterfaceSidePanelsTrailingWindow) >= 60;
}

static void intface_rt_box_paint(Art* art, int frame, bool lights)
{
    unsigned char* dest = gInterfaceWindowBuffer + gInterfaceBarWidth * 38 + 580 + gInterfaceBarContentOffset;
    unsigned char* src = artGetFrameData(art, frame, 0);
    if (src != nullptr) {
        blitBufferToBuffer(src, 57, 58, 57, dest, gInterfaceBarWidth);
    }

    if (lights) {
        // endltgrn.frm -- the green lights vanilla shows while it is your turn.
        FrmImage lightsFrmImage;
        if (lightsFrmImage.lock(buildFid(OBJ_TYPE_INTERFACE, 109, 0, 0, 0))) {
            blitBufferToBufferTrans(lightsFrmImage.getData(), 57, 58, 57, dest, gInterfaceBarWidth);
        }
    }

    windowRefreshRect(gInterfaceBarWindow, &gInterfaceBarEndButtonsRect);
}

void intface_rt_combat_box_tick(bool engaged, float dt)
{
    // ⚠️ ONLY WITH A SIDE PANEL: without one the RB quick slot lives on this box.
    if (gInterfaceBarWindow == -1 || !intface_side_panel_ok() || !combat_rt_enabled()) {
        return;
    }

    if (engaged) {
        rt_box_want = true;
        rt_box_cold_for = 0.0f;
    } else if (rt_box_want) {
        rt_box_cold_for += dt;
        if (rt_box_cold_for >= RT_BOX_CLOSE_DELAY) {
            rt_box_want = false;
        }
    }

    // Settled and untouched is every frame of the game but a few: leave before touching the art.
    bool settled = rt_box_dir == 0 && (rt_box_want ? rt_box_lit : rt_box_frame == 0);
    if (settled && !rt_box_dirty) {
        return;
    }

    CacheEntry* handle;
    Art* art = artLock(buildFid(OBJ_TYPE_INTERFACE, 104, 0, 0, 0), &handle);
    if (art == nullptr) {
        return;
    }

    int last = artGetFrameCount(art) - 1;
    int target = rt_box_want ? last : 0;

    if (rt_box_dirty) {
        rt_box_dirty = false;
        intface_rt_box_paint(art, rt_box_frame, rt_box_lit);
    }

    if (rt_box_frame != target) {
        int dir = target > rt_box_frame ? 1 : -1;
        if (dir != rt_box_dir) {
            rt_box_dir = dir;
            rt_box_accum = 0.0f;
            if (rt_box_sound) {
                soundPlayFile(dir > 0 ? "iciboxx1" : "icibcxx1");
            }
        }

        int fps = artGetFramesPerSecond(art);
        if (fps <= 0) {
            fps = 10;
        }

        rt_box_accum += dt;
        if (rt_box_accum >= 1.0f / (float)fps) {
            // One frame per tick at most: a long frame never skips the curtains.
            rt_box_accum = 0.0f;
            rt_box_frame += dir;
            rt_box_lit = false;
            intface_rt_box_paint(art, rt_box_frame, false);
        }
    } else {
        rt_box_dir = 0;
        if (rt_box_want && !rt_box_lit) {
            rt_box_lit = true;
            if (rt_box_sound) {
                soundPlayFile("icombat2");
            }
            intface_rt_box_paint(art, rt_box_frame, true);
        }
    }

    artUnlock(handle);
}

// A new game or a load: shut at once, no animation, no sound.
void intface_rt_combat_box_reset()
{
    bool painted = rt_box_frame != 0 || rt_box_lit;

    rt_box_frame = 0;
    rt_box_dir = 0;
    rt_box_want = false;
    rt_box_lit = false;
    rt_box_dirty = false;
    rt_box_accum = 0.0f;
    rt_box_cold_for = 0.0f;

    if (painted && gInterfaceBarWindow != -1) {
        CacheEntry* handle;
        Art* art = artLock(buildFid(OBJ_TYPE_INTERFACE, 104, 0, 0, 0), &handle);
        if (art != nullptr) {
            intface_rt_box_paint(art, 0, false);
            artUnlock(handle);
        }
    }
}

void intface_rt_combat_box_invalidate()
{
    if (rt_box_frame != 0 || rt_box_lit) {
        rt_box_dirty = true;
    }
}

// --- THE QUICK SLOTS BESIDE THE BAR (fallout1-ce's intface.cc, ported) ------------------------------
//
// The RB quick slot (the old heal slot) and the LB quick slot: an item icon, the key that uses it and
// how many are left, each in the end-turn box's closed frame. Fallout 2 already keeps a window on
// each side of the bar when the screen is wider than it (sidePanelsInit -- the HRP art, or plain
// black without it), so they live there: RB in a column hugging the bar on the right, LB its mirror
// on the left. On a screen only 640 wide there are no side windows; RB then sits ON the end-turn box,
// exactly as Fallout 1 does it, and the realtime combat curtains stand down for it.
#define HEAL_SLOT_X 580
#define HEAL_SLOT_Y 38
#define HEAL_SLOT_W 57
#define HEAL_SLOT_H 58

// What is currently PAINTED, so the per-frame poll is free when nothing changed. -2 = never drawn,
// which is also what a full bar repaint resets it to.
static int heal_slot_pid = -2;
static int heal_slot_count = -1;
static int heal_slot_pad = -1;

static int lb_slot_pid = -2;
static int lb_slot_count = -1;
static int lb_slot_armed = -1;

void intface_invalidate_heal_slot()
{
    heal_slot_pid = -2;
    heal_slot_count = -1;
    heal_slot_pad = -1;
    lb_slot_pid = -2;
    lb_slot_count = -1;
    lb_slot_armed = -1;
}

static void pad_badge_pill(unsigned char* buf, int pitch, int w, int h, int cx, int cy, const char* text, int color);

// Paints one slot at \`base\` (pitch \`pitch\`): the box frame when \`framed\`, a black plate, the icon,
// the key label (red while its item is drawn) and the count (green, red when empty).
static void intface_slot_paint(unsigned char* base, int pitch, bool framed, Object* item, int count, const char* label, bool armed)
{
    if (framed) {
        // Out in the side strip there is no bar art to frame it, so it brings its own: the end-turn
        // box's closed frame, the very art the RB slot sits on when there is no strip.
        CacheEntry* frameHandle;
        Art* frame = artLock(buildFid(OBJ_TYPE_INTERFACE, 104, 0, 0, 0), &frameHandle);
        if (frame != nullptr) {
            unsigned char* data = artGetFrameData(frame, 0, 0);
            if (data != nullptr) {
                blitBufferToBuffer(data, HEAL_SLOT_W, HEAL_SLOT_H, HEAL_SLOT_W, base, pitch);
            }
            artUnlock(frameHandle);
        }
    }

    // Flat black plate, inset 2px so the box's own bevel still frames it.
    bufferFill(base + pitch * 2 + 2, HEAL_SLOT_W - 4, HEAL_SLOT_H - 4, pitch, _colorTable[0]);

    int oldFont = fontGetCurrent();
    fontSetCurrent(101);

    int lineH = fontGetLineHeight();
    int iconH = HEAL_SLOT_H - 8 - lineH * 2;

    if (item != nullptr) {
        // Fits the art inside the box preserving aspect, index 0 left alone -- hence the plate first.
        artRender(itemGetInventoryFid(item), base + pitch * 4 + 4, HEAL_SLOT_W - 8, iconH, pitch);
    }

    int labelY = 4 + iconH;
    if (strcmp(label, "LB") == 0 || strcmp(label, "RB") == 0) {
        // The pad's shoulder in the bar badges' pill (intface_update_pad_badges); its label red while armed.
        pad_badge_pill(base, pitch, HEAL_SLOT_W, HEAL_SLOT_H, HEAL_SLOT_W / 2, labelY + lineH / 2 - 1, label,
            armed ? _colorTable[31744] : -1);
    } else {
        int labelX = (HEAL_SLOT_W - fontGetStringWidth(label)) / 2;
        fontDrawText(base + pitch * labelY + labelX, label, pitch, pitch, armed ? _colorTable[31744] : _colorTable[32747]);
    }

    char countText[8];
    snprintf(countText, sizeof(countText), "%d", count);
    int countY = labelY + lineH;
    int countX = (HEAL_SLOT_W - fontGetStringWidth(countText)) / 2;
    fontDrawText(base + pitch * countY + countX, countText, pitch, pitch, count > 0 ? _colorTable[992] : _colorTable[31744]);

    fontSetCurrent(oldFont);
}

// The column both side slots use: 78 px wide (the width the old aimed-part readout set), 4 px off the
// bar, the slot centred in it -- hugging the bar rather than drifting to the screen edge.
static int intface_slot_column_x(int stripW, bool rightOfBar)
{
    int columnW = 78 < stripW - 4 ? 78 : stripW - 4;
    int x;
    if (rightOfBar) {
        int columnX = stripW >= columnW + 8 ? 4 : (stripW - columnW) / 2;
        x = columnX + (columnW - HEAL_SLOT_W) / 2;
    } else {
        int columnRight = stripW - 4;
        x = columnRight - columnW + (columnW - HEAL_SLOT_W) / 2;
    }
    return x < 0 ? 0 : x;
}

void intface_update_heal_slot(Object* item, int count, bool armed)
{
    // Turn-based still owns the end-turn box, so stay off.
    if (gInterfaceBarWindow == -1 || !combat_rt_enabled()) {
        return;
    }

    // The label names the key: RB with a pad, T on the keyboard. Part of the change test, so plugging
    // a pad in or out repaints it, and so is whether its item is drawn (red label).
    int pad = gamepad_ui_get_controller() != nullptr ? 1 : 0;
    pad += armed ? 2 : 0;
    int pid = item != nullptr ? item->pid : -1;
    if (pid == heal_slot_pid && count == heal_slot_count && pad == heal_slot_pad) {
        return;
    }
    heal_slot_pid = pid;
    heal_slot_count = count;
    heal_slot_pad = pad;

    const char* label = (pad & 1) ? "RB" : "T";

    if (intface_side_panel_ok()) {
        int win = gInterfaceSidePanelsTrailingWindow;
        int pitch = windowGetWidth(win);
        unsigned char* buffer = windowGetBuffer(win);
        if (buffer == nullptr) {
            return;
        }
        int x = intface_slot_column_x(pitch, true);
        intface_slot_paint(buffer + pitch * HEAL_SLOT_Y + x, pitch, true, item, count, label, armed);
        Rect rect = { x, HEAL_SLOT_Y, x + HEAL_SLOT_W - 1, HEAL_SLOT_Y + HEAL_SLOT_H - 1 };
        windowRefreshRect(win, &rect);
    } else {
        int x = HEAL_SLOT_X + gInterfaceBarContentOffset;
        intface_slot_paint(gInterfaceWindowBuffer + gInterfaceBarWidth * HEAL_SLOT_Y + x, gInterfaceBarWidth, false, item, count, label, armed);
        windowRefreshRect(gInterfaceBarWindow, &gInterfaceBarEndButtonsRect);
    }
}

void intface_update_quick_slot_lb(Object* item, int count, bool armed)
{
    if (gInterfaceBarWindow == -1 || !combat_rt_enabled() || gInterfaceSidePanelsLeadingWindow == -1) {
        return;
    }

    int win = gInterfaceSidePanelsLeadingWindow;
    int pitch = windowGetWidth(win);
    unsigned char* buffer = windowGetBuffer(win);
    if (buffer == nullptr || pitch < HEAL_SLOT_W + 4) {
        return;
    }

    int pad = gamepad_ui_get_controller() != nullptr ? 1 : 0;
    int pid = item != nullptr ? item->pid : -1;
    int armedNow = (armed ? 1 : 0) + pad * 2;
    if (pid == lb_slot_pid && count == lb_slot_count && armedNow == lb_slot_armed) {
        return;
    }
    lb_slot_pid = pid;
    lb_slot_count = count;
    lb_slot_armed = armedNow;

    int x = intface_slot_column_x(pitch, false);
    intface_slot_paint(buffer + pitch * HEAL_SLOT_Y + x, pitch, true, item, count, pad ? "LB" : "H", armed);
    Rect rect = { x, HEAL_SLOT_Y, x + HEAL_SLOT_W - 1, HEAL_SLOT_Y + HEAL_SLOT_H - 1 };
    windowRefreshRect(win, &rect);
}

// --- PAD BUTTONS ON THE BAR -------------------------------------------------
//
// With a pad in, the bar names the buttons that work it (2026-11-03, his „jak lépe vylepšit UI, aby hráč
// věděl, co kdy jak kde mačkat“ and his picks on the mockups: Xbox symbols, the Steam Deck look -- a dark
// disc with a light letter -- Y and LS ON their red buttons, View and Menu to the RIGHT of INV and the
// options wheel, clear of the message screen). Y swaps hands (held: weapons), LS opens the skills, View the
// inventory, Menu the pause, the D-pad Map / Character / Pip-Boy. Without a pad the bar is vanilla's.
//
// Y and LS go into COPIES of their buttons' pictures, so a press or a disable redraws them with the badge;
// the others go onto parts of the bar art nothing repaints, and what was under them is kept to put back.

#define PAD_BADGE_R 7
#define PAD_BADGE_STATIC 5

static const char* pad_badge_glyph(char ch)
{
    switch (ch) {
    case 'A':
        return "01110100011000111111100011000110001";
    case 'Y':
        return "10001100010101000100001000010000100";
    case 'L':
        return "10000100001000010000100001000011111";
    case 'S':
        return "01111100001000001110000010000111110";
    case 'B':
        return "11110100011000111110100011000111110";
    case 'R':
        return "11110100011000111110101001001010001";
    case 'T':
        return "11111001000010000100001000010000100";
    case 'X':
        return "10001100010101000100010101000110001";
    }
    return nullptr;
}

// Every letter and icon in the gold of the weapon slot's "AP 3" and "PUNCH" (palette 60), on the Deck and
// a PC alike -- his pick over the light Deck letters and the Xbox colours ("Zkus použít tu žlutou jak má
// AP3", the dark disc kept).
static int pad_badge_gold()
{
    return _colorTable[(23 << 10) | (19 << 5) | 5]; // 184,156,40
}

// A palette index for a 0-255 grey, never 0 (the transparent index of a button picture).
static int pad_badge_grey(int v)
{
    int c = v >> 3;
    int index = _colorTable[(c << 10) | (c << 5) | c];
    return index != 0 ? index : _colorTable[(2 << 10) | (2 << 5) | 2];
}

static void pad_badge_put(unsigned char* buf, int pitch, int w, int h, int x, int y, int color)
{
    if (x >= 0 && y >= 0 && x < w && y < h) {
        buf[y * pitch + x] = (unsigned char)color;
    }
}

static void pad_badge_disc(unsigned char* buf, int pitch, int w, int h, int cx, int cy, int r)
{
    int fill = pad_badge_grey(44);
    int edge = pad_badge_grey(12);
    for (int y = -r - 1; y <= r + 1; y++) {
        for (int x = -r - 1; x <= r + 1; x++) {
            float d = sqrtf((float)(x * x + y * y));
            if (d <= r - 0.5f) {
                pad_badge_put(buf, pitch, w, h, cx + x, cy + y, fill);
            } else if (d <= r + 0.6f) {
                pad_badge_put(buf, pitch, w, h, cx + x, cy + y, edge);
            }
        }
    }
}

static void pad_badge_text(unsigned char* buf, int pitch, int w, int h, const char* text, int cx, int cy, int color = -1)
{
    int light = color != -1 ? color : pad_badge_gold();
    int width = (int)strlen(text) * 6 - 1;
    int x0 = cx - width / 2;
    int y0 = cy - 3;
    for (const char* p = text; *p != '\0'; p++, x0 += 6) {
        const char* g = pad_badge_glyph(*p);
        for (int i = 0; g != nullptr && i < 35; i++) {
            if (g[i] == '1') {
                pad_badge_put(buf, pitch, w, h, x0 + i % 5, y0 + i / 5, light);
            }
        }
    }
}

// A face button (Y, X): the dark disc, its letter gold.
static void pad_badge_face(unsigned char* buf, int pitch, int w, int h, int cx, int cy, char key)
{
    pad_badge_disc(buf, pitch, w, h, cx, cy, PAD_BADGE_R);
    char text[2] = { key, '\0' };
    pad_badge_text(buf, pitch, w, h, text, cx, cy);
}

// A shoulder or a trigger (LB, RB, RT): a dark pill, its label gold -- or `color` (an armed slot's red).
static void pad_badge_pill(unsigned char* buf, int pitch, int w, int h, int cx, int cy, const char* text, int color)
{
    int fill = pad_badge_grey(44);
    int edge = pad_badge_grey(12);
    int pillW = (int)strlen(text) * 6 + 5;
    int pillH = 11;
    int x0 = cx - pillW / 2;
    int y0 = cy - pillH / 2;
    for (int y = 0; y < pillH; y++) {
        for (int x = 0; x < pillW; x++) {
            bool cornerX = x < 2 || x >= pillW - 2;
            bool cornerY = y < 2 || y >= pillH - 2;
            bool outer = (x == 0 || x == pillW - 1) && (y == 0 || y == pillH - 1);
            if (outer || (cornerX && cornerY && (x == 0 || x == pillW - 1 || y == 0 || y == pillH - 1))) {
                continue; // rounded corners
            }
            bool border = x == 0 || y == 0 || x == pillW - 1 || y == pillH - 1 || (cornerX && cornerY);
            pad_badge_put(buf, pitch, w, h, x0 + x, y0 + y, border ? edge : fill);
        }
    }
    pad_badge_text(buf, pitch, w, h, text, cx, cy, color);
}

// For the character sheet's folder tabs (character_editor.cc).
void intface_pad_pill(unsigned char* buf, int pitch, int w, int h, int cx, int cy, const char* text)
{
    pad_badge_pill(buf, pitch, w, h, cx, cy, text, -1);
}

// For the A on the floating action icon (interact_prompt.cc): the same disc and gold letter as the bar's.
void intface_pad_face(unsigned char* buf, int pitch, int w, int h, int cx, int cy, char key)
{
    pad_badge_face(buf, pitch, w, h, cx, cy, key);
}

// The static ones: 0 View (inventory), 1 Menu (pause), 2-4 the D-pad left / down / up.
static void pad_badge_icon(unsigned char* buf, int pitch, int w, int h, int kind, int cx, int cy)
{
    pad_badge_disc(buf, pitch, w, h, cx, cy, PAD_BADGE_R);
    int light = pad_badge_gold();
    int dim = pad_badge_grey(110);
    if (kind == 0) {
        for (int i = 0; i < 5; i++) {
            pad_badge_put(buf, pitch, w, h, cx - 3 + i, cy - 3, light);
            pad_badge_put(buf, pitch, w, h, cx - 3 + i, cy + 1, light);
            pad_badge_put(buf, pitch, w, h, cx - 3, cy - 3 + i, light);
            pad_badge_put(buf, pitch, w, h, cx + 1, cy - 3 + i, light);
            pad_badge_put(buf, pitch, w, h, cx - 1 + i, cy + 3, light);
            pad_badge_put(buf, pitch, w, h, cx + 3, cy - 1 + i, light);
        }
    } else if (kind == 1) {
        for (int i = -3; i <= 3; i++) {
            pad_badge_put(buf, pitch, w, h, cx + i, cy - 3, light);
            pad_badge_put(buf, pitch, w, h, cx + i, cy, light);
            pad_badge_put(buf, pitch, w, h, cx + i, cy + 3, light);
        }
    } else {
        static const int arms[4][2] = { { -1, 0 }, { 0, 1 }, { 0, -1 }, { 1, 0 } }; // left, down, up, right
        pad_badge_put(buf, pitch, w, h, cx, cy, dim);
        for (int a = 0; a < 4; a++) {
            int color = a == kind - 2 ? light : dim;
            for (int t = 1; t <= 4; t++) {
                for (int s = -1; s <= 1; s++) {
                    int dx = arms[a][0];
                    int dy = arms[a][1];
                    pad_badge_put(buf, pitch, w, h, cx + dx * t + (dy != 0 ? s : 0), cy + dy * t + (dx != 0 ? s : 0), color);
                }
            }
        }
    }
}

static const int pad_badge_static_at[PAD_BADGE_STATIC][2] = { { 253, 50 }, { 253, 78 }, { 517, 48 }, { 517, 67 }, { 517, 86 } };
#define PAD_BADGE_SIZE (PAD_BADGE_R * 2 + 3)
static unsigned char pad_badge_under[PAD_BADGE_STATIC][PAD_BADGE_SIZE * PAD_BADGE_SIZE];
static unsigned char pad_badge_hands[2][22 * 21];
static unsigned char pad_badge_skilldex[2][22 * 21];
static int pad_badge_window = -1; // the bar they were painted on
static bool pad_badge_shown = false;
static int pad_badge_slot_state = -1; // the weapon slot's: 1 a pad, 2 the magazine empty

// The weapon in the active hand takes ammo and has none loaded: X (reload) shows in the slot.
static bool pad_badge_slot_empty()
{
    Object* item = gInterfaceItemStates[gInterfaceCurrentHand].item;
    return item != nullptr && itemGetType(item) == ITEM_TYPE_WEAPON && ammoGetCapacity(item) > 0
        && ammoGetQuantity(item) == 0;
}

// Called by interfaceBarRefreshMainAction on the slot's two pictures: RT top left, X beside it when empty.
static void pad_badge_slot_paint(unsigned char* picture)
{
    if (gamepad_ui_get_controller() == nullptr) {
        return;
    }
    pad_badge_pill(picture, 188, 188, 67, 14, 9, "RT", -1);
    if (pad_badge_slot_empty()) {
        pad_badge_face(picture, 188, 188, 67, 34, 9, 'X');
    }
}

// Once a frame from gamepad_ui_tick: on when a pad is in, off when it is not.
void intface_update_pad_badges()
{
    if (gInterfaceBarWindow == -1 || gInterfaceWindowBuffer == nullptr) {
        return;
    }
    if (pad_badge_window != gInterfaceBarWindow) {
        // A new bar: nothing of ours is on it.
        pad_badge_window = gInterfaceBarWindow;
        pad_badge_shown = false;
    }

    bool want = gamepad_ui_get_controller() != nullptr;

    // The weapon slot draws its own RT and X (interfaceBarRefreshMainAction): repaint it when the pad
    // comes or goes, or the magazine runs empty or is filled.
    int slotState = (want ? 1 : 0) | (pad_badge_slot_empty() ? 2 : 0);
    if (slotState != pad_badge_slot_state) {
        pad_badge_slot_state = slotState;
        interfaceBarRefreshMainAction();
    }

    if (want == pad_badge_shown) {
        return;
    }
    pad_badge_shown = want;

    unsigned char* bar = gInterfaceWindowBuffer;
    int pitch = gInterfaceBarWidth;
    int half = PAD_BADGE_SIZE / 2;
    for (int i = 0; i < PAD_BADGE_STATIC; i++) {
        int x = pad_badge_static_at[i][0] + gInterfaceBarContentOffset - half;
        int y = pad_badge_static_at[i][1] - half;
        unsigned char* at = bar + y * pitch + x;
        if (want) {
            blitBufferToBuffer(at, PAD_BADGE_SIZE, PAD_BADGE_SIZE, pitch, pad_badge_under[i], PAD_BADGE_SIZE);
            pad_badge_icon(at, pitch, PAD_BADGE_SIZE, PAD_BADGE_SIZE, i, half, half);
        } else {
            blitBufferToBuffer(pad_badge_under[i], PAD_BADGE_SIZE, PAD_BADGE_SIZE, PAD_BADGE_SIZE, at, pitch);
        }
    }

    if (want) {
        memcpy(pad_badge_hands[0], _changeHandsButtonNormalFrmImage.getData(), sizeof(pad_badge_hands[0]));
        memcpy(pad_badge_hands[1], _changeHandsButtonPressedFrmImage.getData(), sizeof(pad_badge_hands[1]));
        memcpy(pad_badge_skilldex[0], _skilldexButtonNormalFrmImage.getData(), sizeof(pad_badge_skilldex[0]));
        memcpy(pad_badge_skilldex[1], _skilldexButtonPressedFrmImage.getData(), sizeof(pad_badge_skilldex[1]));
        for (int i = 0; i < 2; i++) {
            pad_badge_face(pad_badge_hands[i], 22, 22, 21, 11, 10, 'Y');
            pad_badge_disc(pad_badge_skilldex[i], 22, 22, 21, 11, 10, PAD_BADGE_R + 1);
            pad_badge_text(pad_badge_skilldex[i], 22, 22, 21, "LS", 11, 10);
        }
        _win_register_button_image(gChangeHandsButton, pad_badge_hands[0], pad_badge_hands[1], nullptr, true);
        _win_register_button_image(gSkilldexButton, pad_badge_skilldex[0], pad_badge_skilldex[1], nullptr, true);
    } else {
        _win_register_button_image(gChangeHandsButton, _changeHandsButtonNormalFrmImage.getData(),
            _changeHandsButtonPressedFrmImage.getData(), nullptr, true);
        _win_register_button_image(gSkilldexButton, _skilldexButtonNormalFrmImage.getData(),
            _skilldexButtonPressedFrmImage.getData(), nullptr, true);
    }

    windowRefresh(gInterfaceBarWindow);
}

// --- AIMED-SHOT PART, IN THE WEAPON SLOT -----------------------------------
//
// Where an aimed RT goes: the part bound in the LB called-shot window (gpui_aim_last_part, default
// HEAD), printed in the WEAPON SLOT left of its bullseye while the slot is on its AIMED mode
// ("dej ty eyes a to kam mířím vedle toho terčíku… vlevo teda vedle toho terčíku", 2026-10-09).
//
// ⚠️ THE SIDE-PANEL READOUT IS GONE. From 2026-10-08 it printed "RB <part>" (later "AIM <part>")
// above the heal slot; once the part sat next to the bullseye that was the same fact twice, and he
// had it removed ("a zruš ho… tu duplicitu"). The slot is the only place it is shown now.

// The part the slot prints; UNCALLED = nothing. Fed by intface_update_aim_part from the pad tick.
static int aim_slot_weapon_part = HIT_LOCATION_UNCALLED;

// Short enough to sit between the AP cost and the bullseye in the worn letters below.
static const char* intface_aim_part_name(int hitLocation)
{
    switch (hitLocation) {
    case HIT_LOCATION_HEAD:
        return "HEAD";
    case HIT_LOCATION_EYES:
        return "EYES";
    case HIT_LOCATION_TORSO:
        return "TORSO";
    case HIT_LOCATION_GROIN:
        return "GROIN";
    case HIT_LOCATION_LEFT_ARM:
        return "L ARM";
    case HIT_LOCATION_RIGHT_ARM:
        return "R ARM";
    case HIT_LOCATION_LEFT_LEG:
        return "L LEG";
    case HIT_LOCATION_RIGHT_LEG:
        return "R LEG";
    default:
        return "BODY";
    }
}

void intface_update_aim_part(int hitLocation)
{
    // Turn-based asks for the part on every shot; there is nothing bound to print.
    if (gInterfaceBarWindow == -1 || !combat_rt_enabled()) {
        return;
    }

    // Repaint the slot when the part changes, but only while it is AIMED -- otherwise it shows no
    // part, and the next switch into the aimed mode repaints it anyway (intface_toggle_item_state /
    // intface_item_set_aiming).
    if (hitLocation != aim_slot_weapon_part) {
        aim_slot_weapon_part = hitLocation;
        int slotAction = gInterfaceItemStates[gInterfaceCurrentHand].action;
        if (slotAction == INTERFACE_ITEM_ACTION_PRIMARY_AIMING
            || slotAction == INTERFACE_ITEM_ACTION_SECONDARY_AIMING) {
            interfaceBarRefreshMainAction();
        }
    }
}

// --- THE WORN LETTERS ----------------------------------------------------------
//
// *"Můžeme to udělat, ať je to taky tak ošuntělé jako celej text ve hře?"* The words on the bar
// (AP, SINGLE, BURST, THROW...) are not a font: they are hand-drawn art with thick strokes, each
// stroke shaded darker at its edges and lighter down its middle, the pixels varying from stroke to
// stroke. This label is drawn the same way, and it took five rounds to get there (2026-10-09/10):
//   1. font 101 in the art's colour -- read as pasted on;
//   2. letters cut from the word art, colours picked at random -- "pixelated, jiný styl": the
//      bevel IS the style, noise destroys it;
//   3. letters cut from the art with their own colours -- right, but 11 px, as big as SINGLE;
//      then "ten text zmenši": shrunk to 3/4 with area averaging, which no pixel art survives --
//   4. "je to blurry, proč je to blurry": every output pixel was a mix of up to four;
//   5. NOW: small letters DRAWN AT SIZE, 8 rows with 2-px strokes in the bar's blocky hand, nothing
//      resampled, so every pixel is a whole palette entry and the edges stay hard.
//
// The shading still comes from the original: every stroke row takes the colours of a real 3-px
// vertical stroke of the word art at the matching row -- left edge / middle / right edge along a
// longer run, the MIDDLE for both pixels of a 2-px stroke. Seven real strokes are used, one per
// stroke of the label, so neighbouring strokes differ the way they do on the bar.
//
// ⚠️ ONLY BRIGHT STROKES, AND NO DARK EDGE ON A 2-PX STROKE (2026-10-10): *"torso poslední o vypadá
// zvláštně"*. Giving a 2-px stroke the middle + RIGHT EDGE made every round letter's right side
// darker than its left, and the sources first chosen included SINGLE's N right stem, ten of whose
// eleven pixels are grey -- whichever letter drew that stroke came out half grey (TORSO's last O,
// GROIN's O and I). Measured per column, the strokes kept are the ones with no grey middle. Then the
// colours are moved onto the AP digits' (intface_worn_build_remap), because the word art is dimmer
// than the AP cost the label sits next to: "moc tmavé oproti AP 4".
//
// Checked in renders from MASTER.DAT beside AP 4 under six guns before shipping
// (.claude/tools/sideart_preview/slotpreview.js, env SMALL=1 MULTI=1 BRIGHT=1 TWO_MID=1 REMAP=1).
//
// ⚠️ NO SHADOW: the black one tried under it on a gun was "strašná".
#define WORN_H 8
#define WORN_SPACE 3
#define WORN_GAP 1
#define WORN_MAX_W 64

typedef struct IntfaceWornGlyph {
    char ch;
    const char* rows[WORN_H];
} IntfaceWornGlyph;

static const IntfaceWornGlyph intface_worn_glyphs[] = {
    { 'H', { "##...##", "##...##", "##...##", "#######", "#######", "##...##", "##...##", "##...##" } },
    { 'E', { "######", "######", "##....", "#####.", "#####.", "##....", "######", "######" } },
    { 'A', { "..###..", ".#####.", "##...##", "##...##", "#######", "#######", "##...##", "##...##" } },
    { 'D', { "#####..", "######.", "##..###", "##...##", "##...##", "##..###", "######.", "#####.." } },
    { 'Y', { "##..##", "##..##", "##..##", ".####.", "..##..", "..##..", "..##..", "..##.." } },
    { 'S', { ".#####", "######", "##....", "#####.", ".#####", "....##", "######", "#####." } },
    { 'T', { "######", "######", "..##..", "..##..", "..##..", "..##..", "..##..", "..##.." } },
    { 'O', { ".#####.", "#######", "##...##", "##...##", "##...##", "##...##", "#######", ".#####." } },
    { 'R', { "######.", "#######", "##...##", "#######", "######.", "##.###.", "##..###", "##...##" } },
    { 'G', { ".#####.", "#######", "##.....", "##.....", "##..###", "##...##", "#######", ".#####." } },
    { 'I', { "##", "##", "##", "##", "##", "##", "##", "##" } },
    { 'N', { "##...##", "###..##", "###..##", "####.##", "##.####", "##..###", "##..###", "##...##" } },
    { 'L', { "##....", "##....", "##....", "##....", "##....", "##....", "######", "######" } },
    { 'M', { "##....##", "###..###", "########", "##.##.##", "##.##.##", "##....##", "##....##", "##....##" } },
};

// Real 3-px vertical strokes of the word art, 11 rows tall: { intrface.lst index, left column }.
// SINGLE (43): E's stem, L's stem, N's left stem. THRUST (45): H's two stems. BURST (40): B's stem,
// R's stem. (SINGLE's I at column 9 stays the fallback for a source row with a gap.)
#define WORN_STROKE_ROWS 11
#define WORN_STROKE_ARTS 3
static const int intface_worn_stroke_arts[WORN_STROKE_ARTS] = { 43, 45, 40 };
static const int intface_worn_strokes[][2] = {
    { 43, 44 },
    { 45, 10 },
    { 45, 18 },
    { 40, 0 },
    { 40, 22 },
    { 43, 36 },
    { 43, 13 },
};

static const IntfaceWornGlyph* intface_worn_glyph(char ch)
{
    for (size_t i = 0; i < sizeof(intface_worn_glyphs) / sizeof(intface_worn_glyphs[0]); i++) {
        if (intface_worn_glyphs[i].ch == ch) {
            return &(intface_worn_glyphs[i]);
        }
    }
    return nullptr;
}

// Width in pixels; -1 when the text has a letter the set does not carry (the caller draws nothing
// rather than a word with a hole in it).
static int intface_worn_text_width(const char* text)
{
    int width = 0;
    for (const char* p = text; *p != '\0'; p++) {
        if (p != text) {
            width += WORN_GAP;
        }
        if (*p == ' ') {
            width += WORN_SPACE;
            continue;
        }
        const IntfaceWornGlyph* glyph = intface_worn_glyph(*p);
        if (glyph == nullptr) {
            return -1;
        }
        width += (int)strlen(glyph->rows[0]);
    }
    return width;
}

// ⚠️ RECOLOURED ONTO THE AP DIGITS' COLOURS (2026-10-10): *"moc tmavé oproti AP 4"*. The word art
// the shading comes from (SINGLE, BURST...) is a dimmer mix with greys and browns in it; the digits
// are a brighter yellow. Every pixel keeps its place in the bevel but takes the digits' colour at the
// same BRIGHTNESS RANK: the darkest word-art pixels become the darkest digit pixels, the brightest the
// brightest. Measured from the art at first use, not hard-coded -- a replaced interface FRM brings its
// own colours along.
static unsigned char intface_worn_remap[256];
static bool intface_worn_remap_ready = false;

static float intface_worn_luma(int index)
{
    return 0.299f * _cmap[index * 3] + 0.587f * _cmap[index * 3 + 1] + 0.114f * _cmap[index * 3 + 2];
}

static void intface_worn_count(int artNum, unsigned int* counts)
{
    CacheEntry* handle;
    Art* art = artLock(buildFid(OBJ_TYPE_INTERFACE, artNum, 0, 0, 0), &handle);
    if (art == nullptr) {
        return;
    }
    int size = artGetWidth(art, 0, 0) * artGetHeight(art, 0, 0);
    unsigned char* data = artGetFrameData(art, 0, 0);
    for (int i = 0; i < size; i++) {
        if (data[i] != 0) {
            counts[data[i]]++;
        }
    }
    artUnlock(handle);
}

static void intface_worn_build_remap()
{
    for (int i = 0; i < 256; i++) {
        intface_worn_remap[i] = (unsigned char)i;
    }
    intface_worn_remap_ready = true;

    // Source: the bar's words (SINGLE, UNARMED, THRUST, BURST, THROW). Target: the ten AP digits
    // (art 290), the brightest text on the slot.
    unsigned int from[256] = { 0 };
    unsigned int to[256] = { 0 };
    static const int sourceArts[] = { 43, 287, 45, 40, 117 };
    for (size_t i = 0; i < sizeof(sourceArts) / sizeof(sourceArts[0]); i++) {
        intface_worn_count(sourceArts[i], from);
    }
    intface_worn_count(290, to);

    unsigned int fromTotal = 0;
    unsigned int toTotal = 0;
    for (int i = 0; i < 256; i++) {
        fromTotal += from[i];
        toTotal += to[i];
    }
    if (fromTotal == 0 || toTotal == 0) {
        return; // art missing: leave the colours as they are
    }

    // The target's palette entries from darkest to brightest (a 256-entry insertion sort, once).
    int order[256];
    int orderCount = 0;
    for (int i = 0; i < 256; i++) {
        if (to[i] == 0) {
            continue;
        }
        int at = orderCount++;
        while (at > 0 && intface_worn_luma(order[at - 1]) > intface_worn_luma(i)) {
            order[at] = order[at - 1];
            at--;
        }
        order[at] = i;
    }

    for (int v = 1; v < 256; v++) {
        if (from[v] == 0) {
            continue;
        }
        // v's rank among the source pixels: those darker, plus half of those exactly as bright.
        float luma = intface_worn_luma(v);
        unsigned int darker = 0;
        unsigned int same = 0;
        for (int u = 0; u < 256; u++) {
            if (from[u] == 0) {
                continue;
            }
            float l = intface_worn_luma(u);
            if (l < luma) {
                darker += from[u];
            } else if (l == luma) {
                same += from[u];
            }
        }
        float rank = (darker + same * 0.5f) / fromTotal;

        // ...and the target pixel at that rank.
        unsigned int want = (unsigned int)(rank * toTotal);
        if (want >= toTotal) {
            want = toTotal - 1;
        }
        unsigned int seen = 0;
        for (int k = 0; k < orderCount; k++) {
            seen += to[order[k]];
            if (seen > want) {
                intface_worn_remap[v] = (unsigned char)order[k];
                break;
            }
        }
    }
}

// Paints `text` into `dest` (width x WORN_H, pitch == width, already zeroed: 0 is transparent, the
// same convention as FRM data, so the result goes through trans_buf_to_buf like any art). False if
// the stroke art could not be loaded -- the caller then draws nothing.
static bool intface_worn_text_compose(const char* text, unsigned char* dest, int width)
{
    if (!intface_worn_remap_ready) {
        intface_worn_build_remap();
    }

    // The words the strokes come from, locked once for the whole label. [0] is SINGLE, the fallback.
    CacheEntry* handles[WORN_STROKE_ARTS];
    Art* arts[WORN_STROKE_ARTS];
    for (int i = 0; i < WORN_STROKE_ARTS; i++) {
        arts[i] = artLock(buildFid(OBJ_TYPE_INTERFACE, intface_worn_stroke_arts[i], 0, 0, 0), &(handles[i]));
        if (arts[i] == nullptr) {
            for (int j = 0; j < i; j++) {
                artUnlock(handles[j]);
            }
            return false;
        }
    }
    Art* single = arts[0];

    bool ok = true;
    int x = 0;
    for (const char* p = text; *p != '\0'; p++) {
        int index = (int)(p - text);
        if (p != text) {
            x += WORN_GAP;
        }
        if (*p == ' ') {
            x += WORN_SPACE;
            continue;
        }
        const IntfaceWornGlyph* glyph = intface_worn_glyph(*p);
        if (glyph == nullptr) {
            ok = false;
            break;
        }

        int glyphW = (int)strlen(glyph->rows[0]);
        for (int y = 0; y < WORN_H; y++) {
            const char* row = glyph->rows[y];
            int sy = y * WORN_STROKE_ROWS / WORN_H;
            for (int gx = 0; gx < glyphW && x + gx < width; gx++) {
                if (row[gx] != '#') {
                    continue;
                }

                // The run of stroke pixels this one sits in, and where in it.
                int runStart = gx;
                while (runStart > 0 && row[runStart - 1] == '#') {
                    runStart--;
                }
                int runEnd = gx;
                while (runEnd < glyphW - 1 && row[runEnd + 1] == '#') {
                    runEnd++;
                }
                int column;
                if (runEnd - runStart <= 1) {
                    column = 1; // a 1- or 2-px stroke is all middle (see above)
                } else {
                    column = gx == runStart ? 0 : (gx == runEnd ? 2 : 1);
                }

                // One real stroke per stroke of the label (letter index and where the run starts).
                const int* stroke = intface_worn_strokes[(index * 3 + runStart * 2) % (int)(sizeof(intface_worn_strokes) / sizeof(intface_worn_strokes[0]))];
                Art* art = single;
                for (int i = 0; i < WORN_STROKE_ARTS; i++) {
                    if (intface_worn_stroke_arts[i] == stroke[0]) {
                        art = arts[i];
                    }
                }
                int artW = artGetWidth(art, 0, 0);
                unsigned char* data = artGetFrameData(art, 0, 0);
                unsigned char v = data[sy * artW + stroke[1] + column];
                if (v == 0) {
                    // A gap in that stroke at this row: SINGLE's I is solid top to bottom.
                    unsigned char* iData = artGetFrameData(single, 0, 0);
                    v = iData[sy * artGetWidth(single, 0, 0) + 9 + column];
                }
                if (v != 0) {
                    dest[y * width + x + gx] = intface_worn_remap[v];
                }
            }
        }
        x += glyphW;
    }

    for (int i = 0; i < WORN_STROKE_ARTS; i++) {
        artUnlock(handles[i]);
    }
    return ok;
}


// NON-BLOCKING HIT-POINT COUNTER (realtime). Vanilla's animated counter rolls the digits inside
// interfaceRenderCounter's own loop, which never runs the game update -- so the whole world stopped
// for ~250 ms on every hit the dude took. This is the same odometer stepped from the frame loop: each
// point costs three drawn phases (the digit rolls OUT, the value steps, the digit rolls IN), at most
// one phase per tick, so a hitch never banks up a burst and nothing blocks.
//
// ⚠️ THE SPEED IS A SPECTRUM (fallout1-ce 2026-10-19, his "50HP ultra rychle, malé hp 5 se bude posouvat
// pomaleji", picked for hits and heals alike). Vanilla spends ~250 ms on any roll, and the
// one-phase-per-tick rule stretched a big one far past that: a 50-point change crawled for 2.5 s at
// 60 fps. Now the counter runs at a speed set by how far it still has to go -- HP_ROLL_TAU is the time
// the gap takes to shrink by about two thirds, HP_ROLL_MIN_RATE (points per second) the slowest it ever
// goes -- so a big change flies and settles, a scratch ticks over point by point. A heal coming in over
// time (combat_rt_heal_over_time) is followed the same way. When the spectrum asks for more points per
// second than three frames a point can show, one roll steps SEVERAL points (hp_roll_owed).
#define HP_ROLL_TAU (0.25f)
#define HP_ROLL_MIN_RATE (10.0f)
#define HP_ROLL_PHASE_OUT 0
#define HP_ROLL_PHASE_IN 1
#define HP_ROLL_PHASE_SETTLE 2
#define HP_ROLL_PHASES 3

static bool hp_roll_active = false;
static int hp_roll_target = 0;
static float hp_roll_accum = 0.0f; // time spent in the current phase
static float hp_roll_owed = 0.0f; // points the speed has earned and the counter not yet shown
static int hp_roll_step = 0; // the points this roll moves, signed (chosen at its OUT phase)
static int hp_roll_phase = HP_ROLL_PHASE_OUT;
// For the trace line at the end of a roll (intface_hp_roll_done).
static int hp_roll_from = 0;
static float hp_roll_took = 0.0f;
static int hp_roll_rolls = 0;
static int hp_roll_max_step = 0;

// [STATE/hp-roll]: how long the counter took and in how many rolls (biggest step), for a change of
// three points or more -- what the spectrum (HP_ROLL_TAU) is checked against.
static void intface_hp_roll_done()
{
    if (abs(gInterfaceLastRenderedHitPoints - hp_roll_from) >= 3) {
        trace_event(TRACE_STATE, "hp-roll", "from=%d to=%d took=%.2f rolls=%d maxstep=%d", hp_roll_from,
            gInterfaceLastRenderedHitPoints, hp_roll_took, hp_roll_rolls, hp_roll_max_step);
    }
}

// The color thresholds vanilla applies at 25% / 50% of max HP, per intermediate value.
static int intface_hp_points_color(int hp)
{
    int maxHp = critterGetStat(gDude, STAT_MAXIMUM_HIT_POINTS);

    if (hp < (int)((double)maxHp * 0.25)) {
        return INTERFACE_NUMBERS_COLOR_RED;
    } else if (hp < (int)((double)maxHp * 0.5)) {
        return INTERFACE_NUMBERS_COLOR_YELLOW;
    } else {
        return INTERFACE_NUMBERS_COLOR_WHITE;
    }
}

// Which decimal digits differ between two values: bit 0 ones, bit 1 tens, bit 2 hundreds.
static int intface_hp_digit_mask(int a, int b)
{
    int aa = abs(a);
    int bb = abs(b);
    int mask = 0;

    if (aa % 10 != bb % 10) {
        mask |= 1;
    }
    if ((aa / 10) % 10 != (bb / 10) % 10) {
        mask |= 2;
    }
    if (aa / 100 != bb / 100) {
        mask |= 4;
    }

    return mask;
}

// One frame of the roll, with interfaceRenderCounter's buffer arithmetic (9x17 digits in a 360-wide
// sheet, sign 6 wide, at 473,40). transition: 0 none, +1 rolling out, -1 rolling in.
static void intface_hp_roll_draw(int value, int digitMask, int transition)
{
    if (value > 999) {
        value = 999;
    } else if (value < -999) {
        value = -999;
    }

    unsigned char* numbers = _numbersFrmImage.getData() + gInterfaceLastRenderedHitPointsColor;
    unsigned char* dest = gInterfaceWindowBuffer + gInterfaceBarWidth * 40 + gInterfaceBarContentOffset;

    unsigned char* downSrc = numbers + 90;
    unsigned char* upSrc = numbers + 99;
    unsigned char* minusSrc = numbers + 108;
    unsigned char* plusSrc = numbers + 114;

    unsigned char* signDest = dest + 473;
    unsigned char* hundredsDest = dest + 473 + 6;
    unsigned char* tensDest = dest + 473 + 6 + 9;
    unsigned char* onesDest = dest + 473 + 6 + 9 * 2;

    int sign = value >= 0 ? 1 : -1;
    int magnitude = abs(value);

    int ones = magnitude % 10;
    int tens = (magnitude / 10) % 10;
    int hundreds = magnitude / 100;

    blitBufferToBuffer(numbers + 9 * hundreds, 9, 17, 360, hundredsDest, gInterfaceBarWidth);
    blitBufferToBuffer(numbers + 9 * tens, 9, 17, 360, tensDest, gInterfaceBarWidth);
    blitBufferToBuffer(numbers + 9 * ones, 9, 17, 360, onesDest, gInterfaceBarWidth);
    blitBufferToBuffer(sign >= 0 ? plusSrc : minusSrc, 6, 17, 360, signDest, gInterfaceBarWidth);

    if (transition != 0) {
        unsigned char* transitionSrc = transition > 0 ? upSrc : downSrc;

        if ((digitMask & 1) != 0) {
            blitBufferToBuffer(transitionSrc, 9, 17, 360, onesDest, gInterfaceBarWidth);
        }
        if ((digitMask & 2) != 0) {
            blitBufferToBuffer(transitionSrc, 9, 17, 360, tensDest, gInterfaceBarWidth);
        }
        if ((digitMask & 4) != 0) {
            blitBufferToBuffer(transitionSrc, 9, 17, 360, hundredsDest, gInterfaceBarWidth);
        }
    }

    Rect numbersRect = { 473 + gInterfaceBarContentOffset, 40, 473 + gInterfaceBarContentOffset + 33, 40 + 17 };
    windowRefreshRect(gInterfaceBarWindow, &numbersRect);
}

// Start -- or re-target -- the roll. Re-targeting mid-roll (a second hit while the first still
// rolls) must NOT restart from the old value, or the counter would visibly jump backwards.
static void intface_hp_roll_start()
{
    if (gInterfaceBarWindow == -1 || gDude == nullptr) {
        return;
    }

    hp_roll_target = critterGetHitPoints(gDude);

    if (hp_roll_target == gInterfaceLastRenderedHitPoints) {
        // Nothing to roll. A roll in flight finishes the point it is on and stops there (its OUT
        // phase finds no gap).
        return;
    }

    // The speed is not fixed here: the tick reads it off the gap every frame (HP_ROLL_TAU).
    if (!hp_roll_active) {
        hp_roll_accum = 0.0f;
        hp_roll_owed = 0.0f;
        hp_roll_phase = HP_ROLL_PHASE_OUT;
        hp_roll_active = true;
        hp_roll_from = gInterfaceLastRenderedHitPoints;
        hp_roll_took = 0.0f;
        hp_roll_rolls = 0;
        hp_roll_max_step = 0;
    }
}

// Frame-loop pump for the roll (combat_rt_update calls it, fight or not).
void intface_hp_roll_tick(float dt)
{
    if (!hp_roll_active) {
        return;
    }

    if (gInterfaceBarWindow == -1 || gDude == nullptr) {
        hp_roll_active = false;
        return;
    }

    // Points per second for the gap as it is now; one point = three phases.
    int gap = hp_roll_target - gInterfaceLastRenderedHitPoints;
    float rate = (float)abs(gap) / HP_ROLL_TAU;
    if (rate < HP_ROLL_MIN_RATE) {
        rate = HP_ROLL_MIN_RATE;
    }
    float phaseSeconds = 1.0f / (rate * (float)HP_ROLL_PHASES);

    hp_roll_owed += rate * dt;
    hp_roll_took += dt;
    if (hp_roll_owed > (float)abs(gap) + 1.0f) {
        hp_roll_owed = (float)abs(gap) + 1.0f; // a long frame or a pause banks nothing beyond the gap
    }

    hp_roll_accum += dt;

    if (hp_roll_accum < phaseSeconds) {
        return;
    }

    // Exactly one phase per tick.
    hp_roll_accum = 0.0f;

    switch (hp_roll_phase) {
    case HP_ROLL_PHASE_OUT: {
        if (gap == 0) {
            // Arrived (or the bar came back to the shown value mid-roll): settle and stop.
            gInterfaceLastRenderedHitPointsColor = intface_hp_points_color(gInterfaceLastRenderedHitPoints);
            intface_hp_roll_draw(gInterfaceLastRenderedHitPoints, 0, 0);
            hp_roll_active = false;
            hp_roll_owed = 0.0f;
            intface_hp_roll_done();
            break;
        }
        // How many points this roll moves: one while a point takes longer than a frame per phase,
        // otherwise as many as the speed has earned since the last one.
        int points = 1;
        if (phaseSeconds < dt) {
            points = (int)hp_roll_owed;
            if (points < 1) {
                points = 1;
            }
            if (points > abs(gap)) {
                points = abs(gap);
            }
            hp_roll_owed -= (float)points;
        } else {
            hp_roll_owed = 0.0f;
        }
        if (hp_roll_owed < 0.0f) {
            hp_roll_owed = 0.0f;
        }
        hp_roll_step = gap > 0 ? points : -points;
        hp_roll_rolls++;
        if (points > hp_roll_max_step) {
            hp_roll_max_step = points;
        }

        gInterfaceLastRenderedHitPointsColor = intface_hp_points_color(gInterfaceLastRenderedHitPoints);
        intface_hp_roll_draw(gInterfaceLastRenderedHitPoints, intface_hp_digit_mask(gInterfaceLastRenderedHitPoints, gInterfaceLastRenderedHitPoints + hp_roll_step), 1);
        hp_roll_phase = HP_ROLL_PHASE_IN;
        break;
    }

    case HP_ROLL_PHASE_IN: {
        // The target may have moved since the OUT phase: never step past it, and not at all if it is
        // now the other way.
        int step = hp_roll_step;
        if (gap * step <= 0) {
            step = 0;
        } else if (abs(step) > abs(gap)) {
            step = gap;
        }
        int previous = gInterfaceLastRenderedHitPoints;
        gInterfaceLastRenderedHitPoints += step;
        gInterfaceLastRenderedHitPointsColor = intface_hp_points_color(gInterfaceLastRenderedHitPoints);
        intface_hp_roll_draw(gInterfaceLastRenderedHitPoints, intface_hp_digit_mask(previous, gInterfaceLastRenderedHitPoints), -1);
        hp_roll_phase = HP_ROLL_PHASE_SETTLE;
        break;
    }

    case HP_ROLL_PHASE_SETTLE:
    default:
        gInterfaceLastRenderedHitPointsColor = intface_hp_points_color(gInterfaceLastRenderedHitPoints);
        intface_hp_roll_draw(gInterfaceLastRenderedHitPoints, 0, 0);
        hp_roll_phase = HP_ROLL_PHASE_OUT;

        if (gInterfaceLastRenderedHitPoints == hp_roll_target) {
            hp_roll_active = false;
            intface_hp_roll_done();
        }
        break;
    }
}

// Drop a roll in flight and snap the counter to the truth (a load, a map teardown, a new game).
void intface_hp_roll_cancel()
{
    if (!hp_roll_active) {
        return;
    }

    hp_roll_active = false;
    hp_roll_accum = 0.0f;
    hp_roll_phase = HP_ROLL_PHASE_OUT;

    if (gInterfaceBarWindow != -1 && gDude != nullptr) {
        interfaceRenderHitPoints(false);
    }
}

// The LIGHT-GREEN AP bulb: one bulb worth TWO action points, for the AP beyond the ten the strip can
// show (his pick: "udělej dvojku, světle zelená za dva AP"). Built from the green bulb, every channel
// lifted toward white, so it stays green but reads paler. Fallout 2's own yellow bulb is used as is.
static unsigned char moveLightGreenLight[5 * 5];

static void intface_build_ap_lights()
{
    unsigned char* green = _greenLightFrmImage.getData();
    for (int index = 0; index < 5 * 5; index++) {
        unsigned char pixel = green != nullptr ? green[index] : 0;
        if (pixel == 0) {
            moveLightGreenLight[index] = 0;
            continue;
        }

        int rgb = Color2RGB(pixel);
        int r = (rgb >> 10) & 0x1F;
        int g = (rgb >> 5) & 0x1F;
        int b = rgb & 0x1F;

        int lr = r + 12 > 0x1F ? 0x1F : r + 12;
        int lg = g + 8 > 0x1F ? 0x1F : g + 8;
        int lb = b + 12 > 0x1F ? 0x1F : b + 12;
        moveLightGreenLight[index] = _colorTable[(lr << 10) | (lg << 5) | lb];
    }
}

// SELF-HEALING WEAPON FID. The dude's sprite carries the weapon he holds in bits 12-15 of his fid,
// and the only thing that normally updates it is the swap animation's end callback. In realtime that
// sequence is not guaranteed to finish, or even to start, which left the hand holding one weapon and
// the SPRITE the previous one ("vystřelil jsem z pistole, ale přehrála se animace pušky"). So the
// fid is reconciled against the hand every frame -- only while the dude is idle, on his feet, and no
// swap is in flight.
static bool intface_dude_is_down()
{
    if (gDude == nullptr) {
        return false;
    }
    int anim = FID_ANIM_TYPE(gDude->fid);
    return _critter_is_prone(gDude) || anim == ANIM_BACK_TO_STANDING || anim == ANIM_PRONE_TO_STANDING;
}

void intface_reconcile_weapon_fid()
{
    if (gInterfaceBarWindow == -1 || gDude == nullptr || gInterfaceBarSwapHandsInProgress) {
        return;
    }

    // Never fight a running animation -- that is how the fid got out of step in the first place.
    if (animationIsBusy(gDude) != 0) {
        return;
    }

    if (intface_dude_is_down()) {
        return;
    }

    Object* item = gInterfaceItemStates[gInterfaceCurrentHand].item;
    if (item == (Object*)-1) {
        return; // hands not initialised yet
    }

    int wantCode = 0;
    if (item != nullptr && itemGetType(item) == ITEM_TYPE_WEAPON) {
        wantCode = weaponGetAnimationCode(item);
    }

    int haveCode = (gDude->fid & 0xF000) >> 12;
    if (haveCode == wantCode) {
        return;
    }

    // Keep the pose and facing the dude already has; only the weapon code changes.
    int fid = buildFid(OBJ_TYPE_CRITTER,
        gDude->fid & 0xFFF,
        FID_ANIM_TYPE(gDude->fid),
        wantCode,
        gDude->rotation + 1);

    Rect rect;
    if (objectSetFid(gDude, fid, &rect) == 0) {
        tileWindowRefreshRect(&rect, gDude->elevation);
    }
}

// Is `action` a mode this weapon can actually be switched INTO? ⚠️ Realtime drops the RELOAD entry:
// reloading has its own binding, and cycling onto it only meant a shot that did not fire.
static bool intface_item_action_available(InterfaceItemState* itemState, int action)
{
    bool realtime = combat_rt_enabled();

    switch (action) {
    case INTERFACE_ITEM_ACTION_PRIMARY:
        // Always reachable -- this is what makes the cycle terminate.
        return true;
    case INTERFACE_ITEM_ACTION_PRIMARY_AIMING:
        return critterCanAim(gDude, itemState->primaryHitMode);
    case INTERFACE_ITEM_ACTION_SECONDARY:
        return itemState->secondaryHitMode != HIT_MODE_PUNCH
            && itemState->secondaryHitMode != HIT_MODE_KICK
            && weaponGetAttackTypeForHitMode(itemState->item, itemState->secondaryHitMode) != ATTACK_TYPE_NONE;
    case INTERFACE_ITEM_ACTION_SECONDARY_AIMING:
        return itemState->secondaryHitMode != HIT_MODE_PUNCH
            && itemState->secondaryHitMode != HIT_MODE_KICK
            && weaponGetAttackTypeForHitMode(itemState->item, itemState->secondaryHitMode) != ATTACK_TYPE_NONE
            && critterCanAim(gDude, itemState->secondaryHitMode);
    case INTERFACE_ITEM_ACTION_RELOAD:
        return !realtime && ammoGetCapacity(itemState->item) != ammoGetQuantity(itemState->item);
    default:
        return false;
    }
}

// ⚠️ THE FIRING MODE IS REMEMBERED (fallout1-ce 2026-10-22, "když spawnu nebo loadnu save tak míření je
// vždycky defaultně na normal hit ... ať si to pamatuje to míření"). Vanilla puts a weapon slot back on
// its plain primary mode whenever a different item lands in it -- and after a load or a new game every
// item is a different object, so the aimed mode (or a burst) was gone every time. The bound PART was
// already remembered (aim_part, gamepad_ui); the MODE now is too: the one the player last picked is
// written to the cfg (fire_mode) on the spot, and every weapon that lands in a hand -- a load, a new
// game, a swap -- starts on it when it has it. Aimed falls back to plain for a weapon that cannot aim, a
// burst to plain for one with no burst.
static int intface_fire_mode_pref = -2; // -2: not read from the cfg yet

static int intface_fire_mode_get()
{
    if (intface_fire_mode_pref == -2) {
        int value = INTERFACE_ITEM_ACTION_PRIMARY;
        configGetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_FIRE_MODE_KEY, &value);
        intface_fire_mode_pref = value >= INTERFACE_ITEM_ACTION_PRIMARY && value <= INTERFACE_ITEM_ACTION_SECONDARY_AIMING
            ? value
            : INTERFACE_ITEM_ACTION_PRIMARY;
    }
    return intface_fire_mode_pref;
}

// Only the player's own choices come here: a cycle (LT+Y, N, a right-click) and binding a part.
static void intface_fire_mode_store(int action)
{
    if (action < INTERFACE_ITEM_ACTION_PRIMARY || action > INTERFACE_ITEM_ACTION_SECONDARY_AIMING
        || action == intface_fire_mode_get()) {
        return;
    }
    intface_fire_mode_pref = action;
    configSetInt(&gGameConfig, GAME_CONFIG_PREFERENCES_KEY, GAME_CONFIG_FIRE_MODE_KEY, action);
    gameConfigSave(); // at once: a crash must not lose it
    trace_event(TRACE_INPUT, "fire-mode-pref", "action=%d", action);
}

// The mode a weapon (or the bare fist / foot) that has just landed in `hand` starts on.
static int intface_fire_mode_for(InterfaceItemState* itemState, int hand)
{
    int want = intface_fire_mode_get();
    int got = INTERFACE_ITEM_ACTION_PRIMARY;
    if (intface_item_action_available(itemState, want)) {
        got = want;
    } else if (want == INTERFACE_ITEM_ACTION_SECONDARY_AIMING
        && intface_item_action_available(itemState, INTERFACE_ITEM_ACTION_PRIMARY_AIMING)) {
        got = INTERFACE_ITEM_ACTION_PRIMARY_AIMING;
    }
    if (got != INTERFACE_ITEM_ACTION_PRIMARY) {
        trace_event(TRACE_INPUT, "fire-mode-apply", "hand=%d pid=%d want=%d got=%d",
            hand, itemState->item != nullptr ? itemState->item->pid : -1, want, got);
    }
    return got;
}

// Would a toggle actually land on a DIFFERENT mode? The caller can refuse audibly instead.
bool intface_item_can_toggle_state()
{
    if (gInterfaceBarWindow == -1) {
        return false;
    }

    InterfaceItemState* itemState = &(gInterfaceItemStates[gInterfaceCurrentHand]);
    if (itemState->isWeapon == 0) {
        return false;
    }

    for (int action = INTERFACE_ITEM_ACTION_USE; action < INTERFACE_ITEM_ACTION_COUNT; action++) {
        if (action != itemState->action && intface_item_action_available(itemState, action)) {
            return true;
        }
    }

    return false;
}

// CE: put the active hand on its AIMED mode -- what binding a part in the called-shot window does,
// so the next RT shot goes there (rt_player_called_loc). True when the slot shows the bullseye.
bool intface_item_set_aiming()
{
    if (gInterfaceBarWindow == -1) {
        return false;
    }

    InterfaceItemState* itemState = &(gInterfaceItemStates[gInterfaceCurrentHand]);
    if (itemState->isWeapon == 0) {
        return false;
    }

    int want;
    switch (itemState->action) {
    case INTERFACE_ITEM_ACTION_PRIMARY_AIMING:
    case INTERFACE_ITEM_ACTION_SECONDARY_AIMING:
        intface_fire_mode_store(itemState->action); // binding a part is choosing to aim
        return true;
    case INTERFACE_ITEM_ACTION_PRIMARY:
        want = INTERFACE_ITEM_ACTION_PRIMARY_AIMING;
        break;
    case INTERFACE_ITEM_ACTION_SECONDARY:
        want = INTERFACE_ITEM_ACTION_SECONDARY_AIMING;
        break;
    default:
        return false;
    }

    if (!intface_item_action_available(itemState, want)) {
        return false;
    }

    itemState->action = want;
    interfaceBarRefreshMainAction();
    intface_fire_mode_store(want);
    return true;
}

// Would intface_reload_weapon do anything right now? A weapon in the active hand that takes ammo, a
// magazine that is not full, and a box in the pack that fits -- asked WITHOUT loading anything (the
// reload animation plays first and loads at the end).
bool intface_can_reload_weapon()
{
    if (gInterfaceBarWindow == -1 || gDude == nullptr) {
        return false;
    }

    Object* weapon = gInterfaceItemStates[gInterfaceCurrentHand].item;
    // ⚠️ The empty hand is (Object*)-1, not null, and a hand can hold a non-weapon.
    if (weapon == nullptr || weapon == (Object*)-1 || itemGetType(weapon) != ITEM_TYPE_WEAPON) {
        return false;
    }

    int capacity = ammoGetCapacity(weapon);
    if (capacity <= 0 || ammoGetQuantity(weapon) >= capacity) {
        return false;
    }

    int index = -1;
    Object* ammo;
    while ((ammo = _inven_find_type(gDude, ITEM_TYPE_AMMO, &index)) != nullptr) {
        if (weaponCanBeReloadedWith(weapon, ammo)) {
            return true;
        }
    }

    return false;
}

// The object intface_reload_weapon would reload (the active hand's), or null when that hand is empty.
Object* intface_reload_target()
{
    if (gInterfaceBarWindow == -1) {
        return nullptr;
    }

    Object* weapon = gInterfaceItemStates[gInterfaceCurrentHand].item;
    return weapon == (Object*)-1 ? nullptr : weapon;
}

// The controller reload: the active-hand weapon from inventory ammo. Unlike _intface_item_reload it
// does NOT cycle the item action -- a direct reload must not change the selected attack mode.
int intface_reload_weapon()
{
    if (gInterfaceBarWindow == -1) {
        return -1;
    }

    bool reloaded = false;
    while (weaponAttemptReload(gDude, gInterfaceItemStates[gInterfaceCurrentHand].item) != -1) {
        reloaded = true;
    }

    interfaceUpdateItems(false, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);

    if (!reloaded) {
        return -1;
    }

    const char* sfx = sfxBuildWeaponName(WEAPON_SOUND_EFFECT_READY, gInterfaceItemStates[gInterfaceCurrentHand].item, HIT_MODE_RIGHT_WEAPON_PRIMARY, nullptr);
    soundPlayFile(sfx);

    return 0;
}

// 0x45EB98
void interfaceBarRefresh()
{
    if (gInterfaceBarWindow != -1) {
        interfaceUpdateItems(false, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
        interfaceRenderHitPoints(false);
        interfaceRenderArmorClass(false);
        indicatorBarRefresh();
        // The repaint above wiped the realtime combat box (and the RB slot, when it sits on the box);
        // let the next tick put them back.
        intface_invalidate_heal_slot();
        intface_rt_combat_box_invalidate();
        windowRefresh(gInterfaceBarWindow);
    }
    indicatorBarRefresh();
}

// Render hit points.
//
// 0x45EBD8
void interfaceRenderHitPoints(bool animate)
{
    if (gInterfaceBarWindow == -1) {
        return;
    }

    // Realtime: hand the animation to the frame-loop roll instead of the blocking one below --
    // wherever the frame loop runs, fight or not. Only a screen with a loop of its own (the
    // inventory, a conversation) still gets the vanilla roll, where nothing moves while it plays.
    if (animate
        && (combat_rt_active() || (combat_rt_enabled() && !inven_any_open() && !GameMode::isInGameMode(GameMode::kDialog)))) {
        intface_hp_roll_start();
        return;
    }

    // Something authoritative is setting the value: whatever was in flight is stale.
    hp_roll_active = false;
    hp_roll_accum = 0.0f;
    hp_roll_phase = HP_ROLL_PHASE_OUT;

    int hp = critterGetHitPoints(gDude);
    int maxHp = critterGetStat(gDude, STAT_MAXIMUM_HIT_POINTS);

    int red = (int)((double)maxHp * 0.25);
    int yellow = (int)((double)maxHp * 0.5);

    int color;
    if (hp < red) {
        color = INTERFACE_NUMBERS_COLOR_RED;
    } else if (hp < yellow) {
        color = INTERFACE_NUMBERS_COLOR_YELLOW;
    } else {
        color = INTERFACE_NUMBERS_COLOR_WHITE;
    }

    int v1[4];
    int v2[3];
    int count = 1;

    v1[0] = gInterfaceLastRenderedHitPoints;
    v2[0] = gInterfaceLastRenderedHitPointsColor;

    if (gInterfaceLastRenderedHitPointsColor != color) {
        if (hp >= gInterfaceLastRenderedHitPoints) {
            if (gInterfaceLastRenderedHitPoints < red && hp >= red) {
                v1[count] = red;
                v2[count] = INTERFACE_NUMBERS_COLOR_YELLOW;
                count += 1;
            }

            if (gInterfaceLastRenderedHitPoints < yellow && hp >= yellow) {
                v1[count] = yellow;
                v2[count] = INTERFACE_NUMBERS_COLOR_WHITE;
                count += 1;
            }
        } else {
            if (gInterfaceLastRenderedHitPoints >= yellow && hp < yellow) {
                v1[count] = yellow;
                v2[count] = INTERFACE_NUMBERS_COLOR_YELLOW;
                count += 1;
            }

            if (gInterfaceLastRenderedHitPoints >= red && hp < red) {
                v1[count] = red;
                v2[count] = INTERFACE_NUMBERS_COLOR_RED;
                count += 1;
            }
        }
    }

    v1[count] = hp;

    if (animate) {
        int delay = 250 / (abs(gInterfaceLastRenderedHitPoints - hp) + 1);
        for (int index = 0; index < count; index++) {
            interfaceRenderCounter(473 + gInterfaceBarContentOffset, 40, v1[index], v1[index + 1], v2[index], delay);
        }
    } else {
        interfaceRenderCounter(473 + gInterfaceBarContentOffset, 40, gInterfaceLastRenderedHitPoints, hp, color, 0);
    }

    gInterfaceLastRenderedHitPoints = hp;
    gInterfaceLastRenderedHitPointsColor = color;
}

// Render armor class.
//
// 0x45EDA8
void interfaceRenderArmorClass(bool animate)
{
    // The guard the other indicators carry: combat_rt pushes this every frame the value moves.
    if (gInterfaceBarWindow == -1) {
        return;
    }

    int armorClass = critterGetStat(gDude, STAT_ARMOR_CLASS);

    int delay = 0;
    if (animate) {
        delay = 250 / (abs(gInterfaceLastRenderedArmorClass - armorClass) + 1);
    }

    interfaceRenderCounter(473 + gInterfaceBarContentOffset, 75, gInterfaceLastRenderedArmorClass, armorClass, 0, delay);

    gInterfaceLastRenderedArmorClass = armorClass;
}

// 0x45EE0C
void interfaceRenderActionPoints(int actionPointsLeft, int bonusActionPoints)
{
    unsigned char* frmData;

    if (gInterfaceBarWindow == -1) {
        return;
    }

    // Realtime holds the engine combat.ap maxed so movement stays free, so it can't drive the bar:
    // every caller gets the real RT pool, and the Bonus Move reserve as the yellow bulbs. Hidden while
    // the dude is stopped on an empty pool (it cannot move him until the red countdown ends).
    if (combat_rt_enabled()) {
        actionPointsLeft = combat_rt_player_ap();
        bonusActionPoints = combat_rt_move_blocked() ? 0 : combat_rt_bonus_move_ap();
    }

    blitBufferToBuffer(gInterfaceActionPointsBarBackground, 90, 5, 90, gInterfaceWindowBuffer + 14 * gInterfaceBarWidth + gInterfaceBarContentOffset + 316, gInterfaceBarWidth);

    if (actionPointsLeft == -1) {
        frmData = _redLightFrmImage.getData();
        actionPointsLeft = 10;
        bonusActionPoints = 0;
    } else {
        frmData = _greenLightFrmImage.getData();

        // Realtime: an empty pool STOPS the dude, and he may walk again only once AP climbs back to
        // the resume threshold -- for exactly that window the refilling points light RED, so watching
        // the reds go 1-2-3 IS the countdown to moving again.
        if (combat_rt_enabled() && combat_rt_move_blocked()) {
            frmData = _redLightFrmImage.getData();
        }
    }

    // ⚠️ TEN BULBS, UP TO TWENTY POINTS (fallout1-ce 2026-10-04, his rules). A + M <= 10: A green, then
    // M yellow, one point per bulb (vanilla's layout). Otherwise, from the front: M yellow worth TWO
    // (the AP underneath plus the movement point), A - 10 light green worth TWO, the rest green, one each.
    int apUnits = actionPointsLeft > 0 ? actionPointsLeft : 0;
    int moveUnits = bonusActionPoints > 0 ? bonusActionPoints : 0;

    int yellowDouble = 0;
    int lightDouble = 0;
    int greenSingle;
    int yellowSingle = 0;

    if (apUnits + moveUnits <= 10) {
        greenSingle = apUnits;
        yellowSingle = moveUnits;
    } else {
        yellowDouble = moveUnits;
        if (yellowDouble > apUnits) {
            yellowDouble = apUnits;
        }
        if (yellowDouble > 10) {
            yellowDouble = 10;
        }

        lightDouble = apUnits > 10 ? apUnits - 10 : 0;
        if (lightDouble > 10 - yellowDouble) {
            lightDouble = 10 - yellowDouble;
        }

        greenSingle = apUnits - yellowDouble - 2 * lightDouble;
        if (greenSingle > 10 - yellowDouble - lightDouble) {
            greenSingle = 10 - yellowDouble - lightDouble;
        }
        if (greenSingle < 0) {
            greenSingle = 0;
        }
    }

    unsigned char* bulbs = gInterfaceWindowBuffer + 14 * gInterfaceBarWidth + 316 + gInterfaceBarContentOffset;
    int slot = 0;
    for (int index = 0; index < yellowDouble && slot < 10; index++) {
        blitBufferToBuffer(_yellowLightFrmImage.getData(), 5, 5, 5, bulbs + 9 * slot++, gInterfaceBarWidth);
    }
    for (int index = 0; index < lightDouble && slot < 10; index++) {
        blitBufferToBuffer(moveLightGreenLight, 5, 5, 5, bulbs + 9 * slot++, gInterfaceBarWidth);
    }
    for (int index = 0; index < greenSingle && slot < 10; index++) {
        blitBufferToBuffer(frmData, 5, 5, 5, bulbs + 9 * slot++, gInterfaceBarWidth);
    }
    for (int index = 0; index < yellowSingle && slot < 10; index++) {
        blitBufferToBuffer(_yellowLightFrmImage.getData(), 5, 5, 5, bulbs + 9 * slot++, gInterfaceBarWidth);
    }

    if (!gInterfaceBarInitialized) {
        windowRefreshRect(gInterfaceBarWindow, &gInterfaceBarActionPointsBarRect);
    }
}

// 0x45EF6C
int interfaceGetCurrentHitMode(int* hitMode, bool* aiming)
{
    if (gInterfaceBarWindow == -1) {
        return -1;
    }

    *aiming = false;

    switch (gInterfaceItemStates[gInterfaceCurrentHand].action) {
    case INTERFACE_ITEM_ACTION_PRIMARY_AIMING:
        *aiming = true;
        // FALLTHROUGH
    case INTERFACE_ITEM_ACTION_PRIMARY:
        *hitMode = gInterfaceItemStates[gInterfaceCurrentHand].primaryHitMode;
        return 0;
    case INTERFACE_ITEM_ACTION_SECONDARY_AIMING:
        *aiming = true;
        // FALLTHROUGH
    case INTERFACE_ITEM_ACTION_SECONDARY:
        *hitMode = gInterfaceItemStates[gInterfaceCurrentHand].secondaryHitMode;
        return 0;
    }

    return -1;
}

// 0x45EFEC
int interfaceUpdateItems(bool animated, int leftItemAction, int rightItemAction)
{
    if (isoIsDisabled()) {
        animated = false;
    }

    if (gInterfaceBarWindow == -1) {
        return -1;
    }

    Object* oldCurrentItem = gInterfaceItemStates[gInterfaceCurrentHand].item;

    InterfaceItemState* leftItemState = &(gInterfaceItemStates[HAND_LEFT]);
    Object* item1 = critterGetItem1(gDude);
    if (item1 == leftItemState->item && leftItemState->item != nullptr) {
        if (leftItemState->item != nullptr) {
            leftItemState->isDisabled = dudeIsWeaponDisabled(item1);
            leftItemState->itemFid = itemGetInventoryFid(item1);
        }
    } else {
        Object* oldItem = leftItemState->item;
        int oldAction = leftItemState->action;

        leftItemState->item = item1;

        if (item1 != nullptr) {
            leftItemState->isDisabled = dudeIsWeaponDisabled(item1);
            leftItemState->primaryHitMode = HIT_MODE_LEFT_WEAPON_PRIMARY;
            leftItemState->secondaryHitMode = HIT_MODE_LEFT_WEAPON_SECONDARY;
            leftItemState->isWeapon = itemGetType(item1) == ITEM_TYPE_WEAPON;

            if (leftItemAction == INTERFACE_ITEM_ACTION_DEFAULT) {
                if (leftItemState->isWeapon != 0) {
                    leftItemState->action = intface_fire_mode_for(leftItemState, HAND_LEFT);
                } else {
                    leftItemState->action = INTERFACE_ITEM_ACTION_USE;
                }
            } else {
                leftItemState->action = leftItemAction;
            }

            leftItemState->itemFid = itemGetInventoryFid(item1);
        } else {
            leftItemState->isDisabled = 0;
            leftItemState->isWeapon = 1;
            leftItemState->action = INTERFACE_ITEM_ACTION_PRIMARY;
            leftItemState->itemFid = -1;

            // SFALL
            leftItemState->primaryHitMode = unarmedGetPunchHitMode(false);
            leftItemState->secondaryHitMode = unarmedGetPunchHitMode(true);
            leftItemState->action = intface_fire_mode_for(leftItemState, HAND_LEFT); // the remembered mode

            // SFALL: Keep selected attack mode.
            // CE: Implementation is different.
            if (oldItem == nullptr) {
                leftItemState->action = oldAction;
            }
        }
    }

    InterfaceItemState* rightItemState = &(gInterfaceItemStates[HAND_RIGHT]);

    Object* item2 = critterGetItem2(gDude);
    if (item2 == rightItemState->item && rightItemState->item != nullptr) {
        if (rightItemState->item != nullptr) {
            rightItemState->isDisabled = dudeIsWeaponDisabled(rightItemState->item);
            rightItemState->itemFid = itemGetInventoryFid(rightItemState->item);
        }
    } else {
        Object* oldItem = rightItemState->item;
        int oldAction = rightItemState->action;

        rightItemState->item = item2;

        if (item2 != nullptr) {
            rightItemState->isDisabled = dudeIsWeaponDisabled(item2);
            rightItemState->primaryHitMode = HIT_MODE_RIGHT_WEAPON_PRIMARY;
            rightItemState->secondaryHitMode = HIT_MODE_RIGHT_WEAPON_SECONDARY;
            rightItemState->isWeapon = itemGetType(item2) == ITEM_TYPE_WEAPON;

            if (rightItemAction == INTERFACE_ITEM_ACTION_DEFAULT) {
                if (rightItemState->isWeapon != 0) {
                    rightItemState->action = intface_fire_mode_for(rightItemState, HAND_RIGHT);
                } else {
                    rightItemState->action = INTERFACE_ITEM_ACTION_USE;
                }
            } else {
                rightItemState->action = rightItemAction;
            }
            rightItemState->itemFid = itemGetInventoryFid(item2);
        } else {
            rightItemState->isDisabled = 0;
            rightItemState->isWeapon = 1;
            rightItemState->action = INTERFACE_ITEM_ACTION_PRIMARY;
            rightItemState->itemFid = -1;

            // SFALL
            rightItemState->primaryHitMode = unarmedGetKickHitMode(false);
            rightItemState->secondaryHitMode = unarmedGetKickHitMode(true);
            rightItemState->action = intface_fire_mode_for(rightItemState, HAND_RIGHT); // the remembered mode

            // SFALL: Keep selected attack mode.
            // CE: Implementation is different.
            if (oldItem == nullptr) {
                rightItemState->action = oldAction;
            }
        }
    }

    if (animated) {
        Object* newCurrentItem = gInterfaceItemStates[gInterfaceCurrentHand].item;
        if (newCurrentItem != oldCurrentItem) {
            int animationCode = 0;
            if (newCurrentItem != nullptr) {
                if (itemGetType(newCurrentItem) == ITEM_TYPE_WEAPON) {
                    animationCode = weaponGetAnimationCode(newCurrentItem);
                }
            }

            interfaceBarSwapHandsAnimatePutAwayTakeOutSequence((gDude->fid & 0xF000) >> 12, animationCode);

            return 0;
        }
    }

    interfaceBarRefreshMainAction();

    return 0;
}

// 0x45F404
int interfaceBarSwapHands(bool animated)
{
    if (gInterfaceBarWindow == -1) {
        return -1;
    }

    gInterfaceCurrentHand = 1 - gInterfaceCurrentHand;

    if (animated) {
        Object* item = gInterfaceItemStates[gInterfaceCurrentHand].item;
        int animationCode = 0;
        if (item != nullptr) {
            if (itemGetType(item) == ITEM_TYPE_WEAPON) {
                animationCode = weaponGetAnimationCode(item);
            }
        }

        interfaceBarSwapHandsAnimatePutAwayTakeOutSequence((gDude->fid & 0xF000) >> 12, animationCode);
    } else {
        interfaceBarRefreshMainAction();
    }

    int mode = gameMouseGetMode();
    if (mode == GAME_MOUSE_MODE_CROSSHAIR || mode == GAME_MOUSE_MODE_USE_CROSSHAIR) {
        gameMouseSetMode(GAME_MOUSE_MODE_MOVE);
    }

    return 0;
}

// 0x45F4B4
int interfaceGetItemActions(int* leftItemAction, int* rightItemAction)
{
    *leftItemAction = gInterfaceItemStates[HAND_LEFT].action;
    *rightItemAction = gInterfaceItemStates[HAND_RIGHT].action;
    return 0;
}

// 0x45F4E0
int interfaceCycleItemAction()
{
    if (gInterfaceBarWindow == -1) {
        return -1;
    }

    InterfaceItemState* itemState = &(gInterfaceItemStates[gInterfaceCurrentHand]);

    int oldAction = itemState->action;
    if (itemState->isWeapon != 0) {
        bool done = false;
        while (!done) {
            itemState->action++;
            if (itemState->action == INTERFACE_ITEM_ACTION_COUNT) {
                // Wrap: ++ on the next pass takes USE to PRIMARY, which is always available.
                itemState->action = INTERFACE_ITEM_ACTION_USE;
                continue;
            }
            done = intface_item_action_available(itemState, itemState->action);
        }
    }

    if (oldAction != itemState->action) {
        interfaceBarRefreshMainAction();
        // Remembered (intface_fire_mode_store) -- except the step OFF reload that the turn-based
        // reload path takes, which is not a choice.
        if (itemState->isWeapon != 0 && oldAction != INTERFACE_ITEM_ACTION_RELOAD) {
            intface_fire_mode_store(itemState->action);
        }
    }

    return 0;
}

// 0x45F5EC
void _intface_use_item()
{
    if (gInterfaceBarWindow == -1) {
        return;
    }

    InterfaceItemState* ptr = &(gInterfaceItemStates[gInterfaceCurrentHand]);

    if (ptr->isWeapon != 0) {
        if (ptr->action == INTERFACE_ITEM_ACTION_RELOAD) {
            if (isInCombat()) {
                int hitMode = gInterfaceCurrentHand == HAND_LEFT
                    ? HIT_MODE_LEFT_WEAPON_RELOAD
                    : HIT_MODE_RIGHT_WEAPON_RELOAD;

                int actionPointsRequired = itemGetActionPointCost(gDude, hitMode, false);
                if (actionPointsRequired <= gDude->data.critter.combat.ap) {
                    if (_intface_item_reload() == 0) {
                        if (actionPointsRequired > gDude->data.critter.combat.ap) {
                            gDude->data.critter.combat.ap = 0;
                        } else {
                            gDude->data.critter.combat.ap -= actionPointsRequired;
                        }
                        interfaceRenderActionPoints(gDude->data.critter.combat.ap, _combat_free_move);
                    }
                }
            } else {
                _intface_item_reload();
            }
        } else {
            gameMouseSetCursor(MOUSE_CURSOR_CROSSHAIR);
            gameMouseSetMode(GAME_MOUSE_MODE_CROSSHAIR);
            if (!isInCombat()) {
                _combat(nullptr);
            }
        }
    } else if (_proto_action_can_use_on(ptr->item->pid)) {
        gameMouseSetCursor(MOUSE_CURSOR_USE_CROSSHAIR);
        gameMouseSetMode(GAME_MOUSE_MODE_USE_CROSSHAIR);
    } else if (_obj_action_can_use(ptr->item)) {
        if (isInCombat() && combat_rt_enabled()) {
            // Realtime holds the engine combat.ap maxed, so charge combat_rt's pool instead.
            int actionPointsRequired = itemGetActionPointCost(gDude, ptr->secondaryHitMode, false);
            if (combat_rt_spend_ap(actionPointsRequired)) {
                _obj_use_item(gDude, ptr->item);
                interfaceUpdateItems(false, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
            }
        } else if (isInCombat()) {
            int actionPointsRequired = itemGetActionPointCost(gDude, ptr->secondaryHitMode, false);
            if (actionPointsRequired <= gDude->data.critter.combat.ap) {
                _obj_use_item(gDude, ptr->item);
                interfaceUpdateItems(false, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
                if (actionPointsRequired > gDude->data.critter.combat.ap) {
                    gDude->data.critter.combat.ap = 0;
                } else {
                    gDude->data.critter.combat.ap -= actionPointsRequired;
                }

                interfaceRenderActionPoints(gDude->data.critter.combat.ap, _combat_free_move);
            }
        } else {
            _obj_use_item(gDude, ptr->item);
            interfaceUpdateItems(false, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
        }
    }
}

// 0x45F7FC
int interfaceGetCurrentHand()
{
    return gInterfaceCurrentHand;
}

// 0x45F804
int interfaceGetActiveItem(Object** itemPtr)
{
    if (gInterfaceBarWindow == -1) {
        return -1;
    }

    // Realtime build (ported from fallout1-ce intface.cc): ⚠️ THE "NO ITEM" SENTINEL HERE IS
    // (Object*)-1, NOT NULL. The interface seeds both hands with it, and vanilla only ever reaches
    // this getter in-game, after a real item or NULL has replaced it. The gamepad's mouse hook runs
    // in PRE-GAME loops too: A in the main menu got the sentinel, a NULL check waved it through, and
    // item->pid read 0xFFFF...FFFF + offsetof(Object, pid) = 0x77 -- the F1 credits crash, met again
    // in F2's main menu on the port's first pad test. Answer "no item": it fixes every caller at once.
    Object* item = gInterfaceItemStates[gInterfaceCurrentHand].item;
    if (item == (Object*)-1) {
        return -1;
    }

    *itemPtr = item;

    return 0;
}

// 0x45F838
int _intface_update_ammo_lights()
{
    if (gInterfaceBarWindow == -1) {
        return -1;
    }

    InterfaceItemState* p = &(gInterfaceItemStates[gInterfaceCurrentHand]);

    int ratio = 0;

    if (p->isWeapon != 0) {
        int maximum = ammoGetCapacity(p->item);
        if (maximum > 0) {
            int current = ammoGetQuantity(p->item);
            ratio = (int)((double)current / (double)maximum * 70.0);
        }
    } else {
        if (itemGetType(p->item) == ITEM_TYPE_MISC) {
            int maximum = miscItemGetMaxCharges(p->item);
            if (maximum > 0) {
                int current = miscItemGetCharges(p->item);
                ratio = (int)((double)current / (double)maximum * 70.0);
            }
        }
    }

    interfaceUpdateAmmoBar(463 + gInterfaceBarContentOffset, ratio);

    return 0;
}

// 0x45F96C
void interfaceBarEndButtonsShow(bool animated)
{
    if (gInterfaceBarWindow == -1) {
        return;
    }

    if (gInterfaceBarEndButtonsIsVisible) {
        return;
    }

    int fid = buildFid(OBJ_TYPE_INTERFACE, 104, 0, 0, 0);
    CacheEntry* handle;
    Art* art = artLock(fid, &handle);
    if (art == nullptr) {
        return;
    }

    int frameCount = artGetFrameCount(art);
    soundPlayFile("iciboxx1");

    if (animated) {
        unsigned int delay = 1000 / artGetFramesPerSecond(art);
        int time = 0;
        int frame = 0;
        while (frame < frameCount) {
            sharedFpsLimiter.mark();

            if (getTicksSince(time) >= delay) {
                unsigned char* src = artGetFrameData(art, frame, 0);
                if (src != nullptr) {
                    blitBufferToBuffer(src, 57, 58, 57, gInterfaceWindowBuffer + gInterfaceBarWidth * 38 + 580 + gInterfaceBarContentOffset, gInterfaceBarWidth);
                    windowRefreshRect(gInterfaceBarWindow, &gInterfaceBarEndButtonsRect);
                }

                time = getTicks();
                frame++;
            }
            gameMouseRefresh();

            renderPresent();
            sharedFpsLimiter.throttle();
        }
    } else {
        unsigned char* src = artGetFrameData(art, frameCount - 1, 0);
        blitBufferToBuffer(src, 57, 58, 57, gInterfaceWindowBuffer + gInterfaceBarWidth * 38 + 580 + gInterfaceBarContentOffset, gInterfaceBarWidth);
        windowRefreshRect(gInterfaceBarWindow, &gInterfaceBarEndButtonsRect);
    }

    artUnlock(handle);

    gInterfaceBarEndButtonsIsVisible = true;
    endTurnButtonInit();
    endCombatButtonInit();
    interfaceBarEndButtonsRenderRedLights();
}

// 0x45FAC0
void interfaceBarEndButtonsHide(bool animated)
{
    if (gInterfaceBarWindow == -1) {
        return;
    }

    if (!gInterfaceBarEndButtonsIsVisible) {
        return;
    }

    int fid = buildFid(OBJ_TYPE_INTERFACE, 104, 0, 0, 0);
    CacheEntry* handle;
    Art* art = artLock(fid, &handle);
    if (art == nullptr) {
        return;
    }

    endTurnButtonFree();
    endCombatButtonFree();
    soundPlayFile("icibcxx1");

    if (animated) {
        unsigned int delay = 1000 / artGetFramesPerSecond(art);
        unsigned int time = 0;
        int frame = artGetFrameCount(art);

        while (frame != 0) {
            sharedFpsLimiter.mark();

            if (getTicksSince(time) >= delay) {
                unsigned char* src = artGetFrameData(art, frame - 1, 0);
                unsigned char* dest = gInterfaceWindowBuffer + gInterfaceBarWidth * 38 + 580 + gInterfaceBarContentOffset;
                if (src != nullptr) {
                    blitBufferToBuffer(src, 57, 58, 57, dest, gInterfaceBarWidth);
                    windowRefreshRect(gInterfaceBarWindow, &gInterfaceBarEndButtonsRect);
                }

                time = getTicks();
                frame--;
            }
            gameMouseRefresh();

            renderPresent();
            sharedFpsLimiter.throttle();
        }
    } else {
        unsigned char* dest = gInterfaceWindowBuffer + gInterfaceBarWidth * 38 + 580 + gInterfaceBarContentOffset;
        unsigned char* src = artGetFrameData(art, 0, 0);
        blitBufferToBuffer(src, 57, 58, 57, dest, gInterfaceBarWidth);
        windowRefreshRect(gInterfaceBarWindow, &gInterfaceBarEndButtonsRect);
    }

    artUnlock(handle);
    gInterfaceBarEndButtonsIsVisible = false;
}

// 0x45FC04
void interfaceBarEndButtonsRenderGreenLights()
{
    if (gInterfaceBarEndButtonsIsVisible) {
        buttonEnable(gEndTurnButton);
        buttonEnable(gEndCombatButton);

        FrmImage lightsFrmImage;
        // endltgrn.frm - green lights around end turn/combat window
        int lightsFid = buildFid(OBJ_TYPE_INTERFACE, 109, 0, 0, 0);
        if (!lightsFrmImage.lock(lightsFid)) {
            return;
        }

        soundPlayFile("icombat2");
        blitBufferToBufferTrans(lightsFrmImage.getData(), 57, 58, 57, gInterfaceWindowBuffer + 38 * gInterfaceBarWidth + 580 + gInterfaceBarContentOffset, gInterfaceBarWidth);
        windowRefreshRect(gInterfaceBarWindow, &gInterfaceBarEndButtonsRect);
    }
}

// 0x45FC98
void interfaceBarEndButtonsRenderRedLights()
{
    if (gInterfaceBarEndButtonsIsVisible) {
        buttonDisable(gEndTurnButton);
        buttonDisable(gEndCombatButton);

        FrmImage lightsFrmImage;
        // endltred.frm - red lights around end turn/combat window
        int lightsFid = buildFid(OBJ_TYPE_INTERFACE, 110, 0, 0, 0);
        if (!lightsFrmImage.lock(lightsFid)) {
            return;
        }

        soundPlayFile("icombat1");
        blitBufferToBufferTrans(lightsFrmImage.getData(), 57, 58, 57, gInterfaceWindowBuffer + 38 * gInterfaceBarWidth + 580 + gInterfaceBarContentOffset, gInterfaceBarWidth);
        windowRefreshRect(gInterfaceBarWindow, &gInterfaceBarEndButtonsRect);
    }
}

// NOTE: Inlined.
//
// 0x45FD2C
static int intface_init_items()
{
    // FIXME: For unknown reason these values initialized with -1. It's never
    // checked for -1, so I have no explanation for this.
    gInterfaceItemStates[HAND_LEFT].item = (Object*)-1;
    gInterfaceItemStates[HAND_RIGHT].item = (Object*)-1;

    return 0;
}

// 0x45FD88
static int interfaceBarRefreshMainAction()
{
    if (gInterfaceBarWindow == -1) {
        return -1;
    }

    buttonEnable(gSingleAttackButton);

    InterfaceItemState* itemState = &(gInterfaceItemStates[gInterfaceCurrentHand]);
    int actionPoints = -1;
    int aimPartBullseyeW = -1; // set when the bullseye is drawn: the slot is on its AIMED mode
    int aimPartApRight = 7 + 17 + 7 + 10; // right edge of the AP cost's digit (MVEPNT.FRM + one digit)
    int aimPartApH = 12;

    if (itemState->isDisabled == 0) {
        memcpy(_itemButtonUp, _itemButtonNormalFrmImage.getData(), sizeof(_itemButtonUp));
        memcpy(_itemButtonDown, _itemButtonPressedFrmImage.getData(), sizeof(_itemButtonDown));

        if (itemState->isWeapon == 0) {
            int fid;
            if (_proto_action_can_use_on(itemState->item->pid)) {
                // USE ON
                fid = buildFid(OBJ_TYPE_INTERFACE, 294, 0, 0, 0);
            } else if (_obj_action_can_use(itemState->item)) {
                // USE
                fid = buildFid(OBJ_TYPE_INTERFACE, 292, 0, 0, 0);
            } else {
                fid = -1;
            }

            if (fid != -1) {
                FrmImage useTextFrmImage;
                if (useTextFrmImage.lock(fid)) {
                    int width = useTextFrmImage.getWidth();
                    int height = useTextFrmImage.getHeight();
                    unsigned char* data = useTextFrmImage.getData();
                    blitBufferToBufferTrans(data, width, height, width, _itemButtonUp + 188 * 7 + 181 - width, 188);
                    _dark_trans_buf_to_buf(data, width, height, width, _itemButtonDown, 181 - width + 1, 5, 188, 59641);
                }

                actionPoints = itemGetActionPointCost(gDude, itemState->primaryHitMode, false);
            }
        } else {
            int primaryFid = -1;
            int bullseyeFid = -1;
            int hitMode = -1;

            // NOTE: This value is decremented at 0x45FEAC, probably to build
            // jump table.
            switch (itemState->action) {
            case INTERFACE_ITEM_ACTION_PRIMARY_AIMING:
                bullseyeFid = buildFid(OBJ_TYPE_INTERFACE, 288, 0, 0, 0);
                // FALLTHROUGH
            case INTERFACE_ITEM_ACTION_PRIMARY:
                hitMode = itemState->primaryHitMode;
                break;
            case INTERFACE_ITEM_ACTION_SECONDARY_AIMING:
                bullseyeFid = buildFid(OBJ_TYPE_INTERFACE, 288, 0, 0, 0);
                // FALLTHROUGH
            case INTERFACE_ITEM_ACTION_SECONDARY:
                hitMode = itemState->secondaryHitMode;
                break;
            case INTERFACE_ITEM_ACTION_RELOAD:
                actionPoints = itemGetActionPointCost(gDude, gInterfaceCurrentHand == HAND_LEFT ? HIT_MODE_LEFT_WEAPON_RELOAD : HIT_MODE_RIGHT_WEAPON_RELOAD, false);
                primaryFid = buildFid(OBJ_TYPE_INTERFACE, 291, 0, 0, 0);
                break;
            }

            if (bullseyeFid != -1) {
                FrmImage bullseyeFrmImage;
                if (bullseyeFrmImage.lock(bullseyeFid)) {
                    int width = bullseyeFrmImage.getWidth();
                    int height = bullseyeFrmImage.getHeight();
                    unsigned char* data = bullseyeFrmImage.getData();
                    blitBufferToBufferTrans(data, width, height, width, _itemButtonUp + 188 * (60 - height) + (181 - width), 188);

                    int v9 = 60 - height - 2;
                    if (v9 < 0) {
                        v9 = 0;
                        height -= 2;
                    }

                    _dark_trans_buf_to_buf(data, width, height, width, _itemButtonDown, 181 - width + 1, v9, 188, 59641);

                    aimPartBullseyeW = width; // the part name goes up against its left edge, below
                }
            }

            if (hitMode != -1) {
                actionPoints = weaponGetActionPointCost(gDude, hitMode, bullseyeFid != -1);

                int id;
                int anim = critterGetAnimationForHitMode(gDude, hitMode);
                switch (anim) {
                case ANIM_THROW_PUNCH:
                    switch (hitMode) {
                    case HIT_MODE_STRONG_PUNCH:
                        id = 432; // strong punch
                        break;
                    case HIT_MODE_HAMMER_PUNCH:
                        id = 425; // hammer punch
                        break;
                    case HIT_MODE_HAYMAKER:
                        id = 428; // lightning punch
                        break;
                    case HIT_MODE_JAB:
                        id = 421; // chop punch
                        break;
                    case HIT_MODE_PALM_STRIKE:
                        id = 423; // dragon punch
                        break;
                    case HIT_MODE_PIERCING_STRIKE:
                        id = 424; // force punch
                        break;
                    default:
                        id = 42; // punch
                        break;
                    }
                    break;
                case ANIM_KICK_LEG:
                    switch (hitMode) {
                    case HIT_MODE_STRONG_KICK:
                        id = 430; // skick.frm - strong kick text
                        break;
                    case HIT_MODE_SNAP_KICK:
                        id = 431; // snapkick.frm - snap kick text
                        break;
                    case HIT_MODE_POWER_KICK:
                        id = 429; // cm_pwkck.frm - roundhouse kick text
                        break;
                    case HIT_MODE_HIP_KICK:
                        id = 426; // hipk.frm - kip kick text
                        break;
                    case HIT_MODE_HOOK_KICK:
                        id = 427; // cm_hookk.frm - jump kick text
                        break;
                    case HIT_MODE_PIERCING_KICK: // cm_prckk.frm - death blossom kick text
                        id = 422;
                        break;
                    default:
                        id = 41; // kick.frm - kick text
                        break;
                    }
                    break;
                case ANIM_THROW_ANIM:
                    id = 117; // throw
                    break;
                case ANIM_THRUST_ANIM:
                    id = 45; // thrust
                    break;
                case ANIM_SWING_ANIM:
                    id = 44; // swing
                    break;
                case ANIM_FIRE_SINGLE:
                    id = 43; // single
                    break;
                case ANIM_FIRE_BURST:
                case ANIM_FIRE_CONTINUOUS:
                    id = 40; // burst
                    break;
                }

                primaryFid = buildFid(OBJ_TYPE_INTERFACE, id, 0, 0, 0);
            }

            if (primaryFid != -1) {
                FrmImage primaryFrmImage;
                if (primaryFrmImage.lock(primaryFid)) {
                    int width = primaryFrmImage.getWidth();
                    int height = primaryFrmImage.getHeight();
                    unsigned char* data = primaryFrmImage.getData();
                    blitBufferToBufferTrans(data, width, height, width, _itemButtonUp + 188 * 7 + 181 - width, 188);
                    _dark_trans_buf_to_buf(data, width, height, width, _itemButtonDown, 181 - width + 1, 5, 188, 59641);
                }
            }
        }
    }

    if (actionPoints >= 0 && actionPoints < 10) {
        // movement point text
        int apFid = buildFid(OBJ_TYPE_INTERFACE, 289, 0, 0, 0);

        FrmImage apFrmImage;
        if (apFrmImage.lock(apFid)) {
            int width = apFrmImage.getWidth();
            int height = apFrmImage.getHeight();
            unsigned char* data = apFrmImage.getData();

            blitBufferToBufferTrans(data, width, height, width, _itemButtonUp + 188 * (60 - height) + 7, 188);
            aimPartApRight = 7 + width + 7 + 10; // "AP", the gap, one 10-px digit (see below)
            aimPartApH = height; // before the pressed frame trims it

            int v29 = 60 - height - 2;
            if (v29 < 0) {
                v29 = 0;
                height -= 2;
            }

            _dark_trans_buf_to_buf(data, width, height, width, _itemButtonDown, 7 + 1, v29, 188, 59641);

            int offset = width + 7;

            FrmImage apNumbersFrmImage;
            // movement point numbers - ten numbers 0 to 9, each 10 pixels wide.
            int apNumbersFid = buildFid(OBJ_TYPE_INTERFACE, 290, 0, 0, 0);
            if (apNumbersFrmImage.lock(apNumbersFid)) {
                int width = apNumbersFrmImage.getWidth();
                int height = apNumbersFrmImage.getHeight();
                unsigned char* data = apNumbersFrmImage.getData();

                blitBufferToBufferTrans(data + actionPoints * 10, 10, height, width, _itemButtonUp + 188 * (60 - height) + 7 + offset, 188);

                int v40 = 60 - height - 2;
                if (v40 < 0) {
                    v40 = 0;
                    height -= 2;
                }
                _dark_trans_buf_to_buf(data + actionPoints * 10, 10, height, width, _itemButtonDown, offset + 7 + 1, v40, 188, 59641);
            }
        }
    } else {
        memcpy(_itemButtonUp, _itemButtonDisabledFrmImage.getData(), sizeof(_itemButtonUp));
        memcpy(_itemButtonDown, _itemButtonDisabledFrmImage.getData(), sizeof(_itemButtonDown));
    }

    if (itemState->itemFid != -1) {
        FrmImage itemFrmImage;
        if (itemFrmImage.lock(itemState->itemFid)) {
            int width = itemFrmImage.getWidth();
            int height = itemFrmImage.getHeight();
            unsigned char* data = itemFrmImage.getData();

            int v46 = (188 - width) / 2;
            int v47 = (67 - height) / 2 - 2;

            blitBufferToBufferTrans(data, width, height, width, _itemButtonUp + 188 * ((67 - height) / 2) + v46, 188);

            if (v47 < 0) {
                v47 = 0;
                height -= 2;
            }

            _dark_trans_buf_to_buf(data, width, height, width, _itemButtonDown, v46 + 1, v47, 188, 63571);
        }
    }

    // CE: realtime names the part an AIMED shot goes to, in small worn letters drawn at size
    // (intface_worn_text_compose), only while the slot shows its bullseye -- RIGHT AFTER THE AP COST,
    // centred on its height, the corner no gun's art covers. Turn-based asks every shot, so nothing
    // is printed there. On top of the item, no shadow; the pressed frame the way every word on the bar
    // is drawn (+1, -2).
    if (aimPartBullseyeW >= 0 && combat_rt_enabled() && aim_slot_weapon_part != HIT_LOCATION_UNCALLED) {
        const char* partName = intface_aim_part_name(aim_slot_weapon_part);
        int labelW = intface_worn_text_width(partName);
        int labelX = aimPartApRight + 5;
        int labelY = (60 - aimPartApH) + (aimPartApH - WORN_H + 1) / 2; // centred; an odd half-pixel goes down
        bool fits = labelX + labelW <= 181 - aimPartBullseyeW - 3; // never under the bullseye
        if (labelW > 0 && labelW <= WORN_MAX_W && fits && labelY >= 2 && labelY + WORN_H <= 67) {
            unsigned char label[WORN_MAX_W * WORN_H];
            memset(label, 0, sizeof(label));
            if (intface_worn_text_compose(partName, label, labelW)) {
                blitBufferToBufferTrans(label, labelW, WORN_H, labelW, _itemButtonUp + 188 * labelY + labelX, 188);
                _dark_trans_buf_to_buf(label, labelW, WORN_H, labelW, _itemButtonDown, labelX + 1, labelY - 2, 188, 59641);
            }
        }
    }

    // With a pad: RT, and X while the magazine is empty (intface_update_pad_badges).
    if (itemState->isDisabled == 0) {
        pad_badge_slot_paint(_itemButtonUp);
        pad_badge_slot_paint(_itemButtonDown);
    }

    if (!gInterfaceBarInitialized) {
        _intface_update_ammo_lights();

        windowRefreshRect(gInterfaceBarWindow, &gInterfaceBarMainActionRect);

        if (itemState->isDisabled != 0) {
            buttonDisable(gSingleAttackButton);
        } else {
            buttonEnable(gSingleAttackButton);
        }
    }

    return 0;
}

// 0x460658
static int _intface_redraw_items_callback(Object* a1, Object* a2)
{
    interfaceBarRefreshMainAction();
    return 0;
}

// 0x460660
static int _intface_change_fid_callback(Object* a1, Object* a2)
{
    gInterfaceBarSwapHandsInProgress = false;
    return 0;
}

// 0x46066C
static void interfaceBarSwapHandsAnimatePutAwayTakeOutSequence(int previousWeaponAnimationCode, int weaponAnimationCode)
{
    // Down: the hand has already changed, show it, and leave the body alone (intface_dude_is_down);
    // the sprite catches up through intface_reconcile_weapon_fid once he is on his feet.
    if (combat_rt_enabled() && intface_dude_is_down()) {
        interfaceBarRefreshMainAction();
        return;
    }

    gInterfaceBarSwapHandsInProgress = true;

    reg_anim_clear(gDude);
    reg_anim_begin(ANIMATION_REQUEST_RESERVED);
    animationRegisterSetLightDistance(gDude, 4, 0);

    if (previousWeaponAnimationCode != 0) {
        const char* sfx = sfxBuildCharName(gDude, ANIM_PUT_AWAY, CHARACTER_SOUND_EFFECT_UNUSED);
        animationRegisterPlaySoundEffect(gDude, sfx, 0);
        animationRegisterAnimate(gDude, ANIM_PUT_AWAY, 0);
    }

    // TODO: Get rid of cast.
    animationRegisterCallbackForced(nullptr, nullptr, (AnimationCallback*)_intface_redraw_items_callback, -1);

    Object* item = gInterfaceItemStates[gInterfaceCurrentHand].item;
    if (item != nullptr && item->lightDistance > 4) {
        animationRegisterSetLightDistance(gDude, item->lightDistance, 0);
    }

    if (weaponAnimationCode != 0) {
        animationRegisterTakeOutWeapon(gDude, weaponAnimationCode, -1);
    } else {
        int fid = buildFid(OBJ_TYPE_CRITTER, gDude->fid & 0xFFF, ANIM_STAND, 0, gDude->rotation + 1);
        animationRegisterSetFid(gDude, fid, -1);
    }

    // TODO: Get rid of cast.
    animationRegisterCallbackForced(nullptr, nullptr, (AnimationCallback*)_intface_change_fid_callback, -1);

    if (reg_anim_end() == -1) {
        return;
    }

    // REALTIME: DO NOT BLOCK. The loop below pumps input and presents until the swap animation ends,
    // but never runs the game update -- the whole world froze for the swap. Returning leaves the
    // animation registered: the main loop plays it and its two callbacks still fire.
    if (combat_rt_enabled()) {
        return;
    }

    bool interfaceBarWasEnabled = gInterfaceBarEnabled;

    interfaceBarDisable();
    _gmouse_disable(0);

    gameMouseSetCursor(MOUSE_CURSOR_WAIT_WATCH);

    while (gInterfaceBarSwapHandsInProgress) {
        sharedFpsLimiter.mark();

        if (_game_user_wants_to_quit) {
            break;
        }

        inputGetInput();

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    gameMouseSetCursor(MOUSE_CURSOR_NONE);

    _gmouse_enable();

    if (interfaceBarWasEnabled) {
        interfaceBarEnable();
    }
}

// 0x4607E0
static int endTurnButtonInit()
{
    int fid;

    if (gInterfaceBarWindow == -1) {
        return -1;
    }

    if (!gInterfaceBarEndButtonsIsVisible) {
        return -1;
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 105, 0, 0, 0);
    if (!_endTurnButtonNormalFrmImage.lock(fid)) {
        return -1;
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 106, 0, 0, 0);
    if (!_endTurnButtonPressedFrmImage.lock(fid)) {
        return -1;
    }

    gEndTurnButton = buttonCreate(gInterfaceBarWindow, 590 + gInterfaceBarContentOffset, 43, 38, 22, -1, -1, -1, 32, _endTurnButtonNormalFrmImage.getData(), _endTurnButtonPressedFrmImage.getData(), nullptr, 0);
    if (gEndTurnButton == -1) {
        return -1;
    }

    _win_register_button_disable(gEndTurnButton, _endTurnButtonNormalFrmImage.getData(), _endTurnButtonNormalFrmImage.getData(), _endTurnButtonNormalFrmImage.getData());
    buttonSetCallbacks(gEndTurnButton, _gsound_med_butt_press, _gsound_med_butt_release);

    return 0;
}

// 0x4608C4
static int endTurnButtonFree()
{
    if (gInterfaceBarWindow == -1) {
        return -1;
    }

    if (gEndTurnButton != -1) {
        buttonDestroy(gEndTurnButton);
        gEndTurnButton = -1;
    }

    _endTurnButtonNormalFrmImage.unlock();
    _endTurnButtonPressedFrmImage.unlock();

    return 0;
}

// 0x460940
static int endCombatButtonInit()
{
    int fid;

    if (gInterfaceBarWindow == -1) {
        return -1;
    }

    if (!gInterfaceBarEndButtonsIsVisible) {
        return -1;
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 107, 0, 0, 0);
    if (!_endCombatButtonNormalFrmImage.lock(fid)) {
        return -1;
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 108, 0, 0, 0);
    if (!_endCombatButtonPressedFrmImage.lock(fid)) {
        return -1;
    }

    gEndCombatButton = buttonCreate(gInterfaceBarWindow, 590 + gInterfaceBarContentOffset, 65, 38, 22, -1, -1, -1, 13, _endCombatButtonNormalFrmImage.getData(), _endCombatButtonPressedFrmImage.getData(), nullptr, 0);
    if (gEndCombatButton == -1) {
        return -1;
    }

    _win_register_button_disable(gEndCombatButton, _endCombatButtonNormalFrmImage.getData(), _endCombatButtonNormalFrmImage.getData(), _endCombatButtonNormalFrmImage.getData());
    buttonSetCallbacks(gEndCombatButton, _gsound_med_butt_press, _gsound_med_butt_release);

    return 0;
}

// 0x460A24
static int endCombatButtonFree()
{
    if (gInterfaceBarWindow == -1) {
        return -1;
    }

    if (gEndCombatButton != -1) {
        buttonDestroy(gEndCombatButton);
        gEndCombatButton = -1;
    }

    _endCombatButtonNormalFrmImage.unlock();
    _endCombatButtonPressedFrmImage.unlock();

    return 0;
}

// 0x460AA0
static void interfaceUpdateAmmoBar(int x, int ratio)
{
    // The bar is 70 pixels tall: an out-of-range ratio (a stale weapon) wrote past the window buffer.
    if (ratio < 0) {
        ratio = 0;
    } else if (ratio > 70) {
        ratio = 70;
    }

    if ((ratio & 1) != 0) {
        ratio -= 1;
    }

    unsigned char* dest = gInterfaceWindowBuffer + gInterfaceBarWidth * 26 + x;

    for (int index = 70; index > ratio; index--) {
        *dest = 14;
        dest += gInterfaceBarWidth;
    }

    while (ratio > 0) {
        *dest = 196;
        dest += gInterfaceBarWidth;

        *dest = 14;
        dest += gInterfaceBarWidth;

        ratio -= 2;
    }

    if (!gInterfaceBarInitialized) {
        Rect rect;
        rect.left = x;
        rect.top = 26;
        rect.right = x + 1;
        rect.bottom = 26 + 70;
        windowRefreshRect(gInterfaceBarWindow, &rect);
    }
}

// 0x460B20
static int _intface_item_reload()
{
    if (gInterfaceBarWindow == -1) {
        return -1;
    }

    bool v0 = false;
    while (weaponAttemptReload(gDude, gInterfaceItemStates[gInterfaceCurrentHand].item) != -1) {
        v0 = true;
    }

    interfaceCycleItemAction();
    interfaceUpdateItems(false, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);

    if (!v0) {
        return -1;
    }

    const char* sfx = sfxBuildWeaponName(WEAPON_SOUND_EFFECT_READY, gInterfaceItemStates[gInterfaceCurrentHand].item, HIT_MODE_RIGHT_WEAPON_PRIMARY, nullptr);
    soundPlayFile(sfx);

    return 0;
}

// Renders hit points.
//
// [delay] is an animation delay.
// [previousValue] is only meaningful for animation.
// [offset] = 0 - grey, 120 - yellow, 240 - red.
//
// 0x460BA0
static void interfaceRenderCounter(int x, int y, int previousValue, int value, int offset, int delay)
{
    if (value > 999) {
        value = 999;
    } else if (value < -999) {
        value = -999;
    }

    unsigned char* numbers = _numbersFrmImage.getData() + offset;
    unsigned char* dest = gInterfaceWindowBuffer + gInterfaceBarWidth * y;

    unsigned char* downSrc = numbers + 90;
    unsigned char* upSrc = numbers + 99;
    unsigned char* minusSrc = numbers + 108;
    unsigned char* plusSrc = numbers + 114;

    unsigned char* signDest = dest + x;
    unsigned char* hundredsDest = dest + x + 6;
    unsigned char* tensDest = dest + x + 6 + 9;
    unsigned char* onesDest = dest + x + 6 + 9 * 2;

    int normalizedSign;
    int normalizedValue;
    if (gInterfaceBarInitialized || delay == 0) {
        normalizedSign = value >= 0 ? 1 : -1;
        normalizedValue = abs(value);
    } else {
        normalizedSign = previousValue >= 0 ? 1 : -1;
        normalizedValue = previousValue;
    }

    int ones = normalizedValue % 10;
    int tens = (normalizedValue / 10) % 10;
    int hundreds = normalizedValue / 100;

    blitBufferToBuffer(numbers + 9 * hundreds, 9, 17, 360, hundredsDest, gInterfaceBarWidth);
    blitBufferToBuffer(numbers + 9 * tens, 9, 17, 360, tensDest, gInterfaceBarWidth);
    blitBufferToBuffer(numbers + 9 * ones, 9, 17, 360, onesDest, gInterfaceBarWidth);
    blitBufferToBuffer(normalizedSign >= 0 ? plusSrc : minusSrc, 6, 17, 360, signDest, gInterfaceBarWidth);

    if (!gInterfaceBarInitialized) {
        Rect numbersRect = { x, y, x + 33, y + 17 };
        windowRefreshRect(gInterfaceBarWindow, &numbersRect);
        if (delay != 0) {
            int change = value - previousValue >= 0 ? 1 : -1;
            int v14 = previousValue >= 0 ? 1 : -1;
            int v49 = change * v14;
            while (previousValue != value) {
                if ((hundreds | tens | ones) == 0) {
                    v49 = 1;
                }

                blitBufferToBuffer(upSrc, 9, 17, 360, onesDest, gInterfaceBarWidth);
                _mouse_info();
                gameMouseRefresh();
                renderPresent();
                inputBlockForTocks(delay);
                windowRefreshRect(gInterfaceBarWindow, &numbersRect);

                ones += v49;

                if (ones > 9 || ones < 0) {
                    blitBufferToBuffer(upSrc, 9, 17, 360, tensDest, gInterfaceBarWidth);
                    _mouse_info();
                    gameMouseRefresh();
                    renderPresent();
                    inputBlockForTocks(delay);
                    windowRefreshRect(gInterfaceBarWindow, &numbersRect);

                    tens += v49;
                    ones -= 10 * v49;
                    if (tens == 10 || tens == -1) {
                        blitBufferToBuffer(upSrc, 9, 17, 360, hundredsDest, gInterfaceBarWidth);
                        _mouse_info();
                        gameMouseRefresh();
                        renderPresent();
                        inputBlockForTocks(delay);
                        windowRefreshRect(gInterfaceBarWindow, &numbersRect);

                        hundreds += v49;
                        tens -= 10 * v49;
                        if (hundreds == 10 || hundreds == -1) {
                            hundreds -= 10 * v49;
                        }

                        blitBufferToBuffer(downSrc, 9, 17, 360, hundredsDest, gInterfaceBarWidth);
                        _mouse_info();
                        gameMouseRefresh();
                        renderPresent();
                        inputBlockForTocks(delay);
                        windowRefreshRect(gInterfaceBarWindow, &numbersRect);
                    }

                    blitBufferToBuffer(downSrc, 9, 17, 360, tensDest, gInterfaceBarWidth);
                    renderPresent();
                    inputBlockForTocks(delay);
                    windowRefreshRect(gInterfaceBarWindow, &numbersRect);
                }

                blitBufferToBuffer(downSrc, 9, 17, 360, onesDest, gInterfaceBarWidth);
                _mouse_info();
                gameMouseRefresh();
                renderPresent();
                inputBlockForTocks(delay);
                windowRefreshRect(gInterfaceBarWindow, &numbersRect);

                previousValue += change;

                blitBufferToBuffer(numbers + 9 * hundreds, 9, 17, 360, hundredsDest, gInterfaceBarWidth);
                blitBufferToBuffer(numbers + 9 * tens, 9, 17, 360, tensDest, gInterfaceBarWidth);
                blitBufferToBuffer(numbers + 9 * ones, 9, 17, 360, onesDest, gInterfaceBarWidth);

                blitBufferToBuffer(previousValue >= 0 ? plusSrc : minusSrc, 6, 17, 360, signDest, gInterfaceBarWidth);
                _mouse_info();
                gameMouseRefresh();
                renderPresent();
                inputBlockForTocks(delay);
                windowRefreshRect(gInterfaceBarWindow, &numbersRect);
            }
        }
    }
}

// NOTE: Inlined.
//
// 0x461128
static int intface_fatal_error(int rc)
{
    interfaceFree();

    return rc;
}

// 0x461134
static int indicatorBarInit()
{
    int oldFont = fontGetCurrent();

    if (gIndicatorBarWindow != -1) {
        return 0;
    }

    MessageList messageList;
    MessageListItem messageListItem;
    int rc = 0;
    if (!messageListInit(&messageList)) {
        rc = -1;
    }

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "%s%s", asc_5186C8, "intrface.msg");

    if (rc != -1) {
        if (!messageListLoad(&messageList, path)) {
            rc = -1;
        }
    }

    if (rc == -1) {
        debugPrint("\nINTRFACE: Error indicator box messages! **\n");
        return -1;
    }

    FrmImage indicatorBoxFrmImage;
    int indicatorBoxFid = buildFid(OBJ_TYPE_INTERFACE, 126, 0, 0, 0);
    if (!indicatorBoxFrmImage.lock(indicatorBoxFid)) {
        debugPrint("\nINTRFACE: Error initializing indicator box graphics! **\n");
        messageListFree(&messageList);
        return -1;
    }

    for (int index = 0; index < INDICATOR_COUNT; index++) {
        IndicatorDescription* indicatorDescription = &(gIndicatorDescriptions[index]);

        indicatorDescription->data = (unsigned char*)internal_malloc(INDICATOR_BOX_WIDTH * INDICATOR_BOX_HEIGHT);
        if (indicatorDescription->data == nullptr) {
            debugPrint("\nINTRFACE: Error initializing indicator box graphics! **");

            while (--index >= 0) {
                internal_free(gIndicatorDescriptions[index].data);
            }

            messageListFree(&messageList);

            return -1;
        }
    }

    fontSetCurrent(101);

    for (int index = 0; index < INDICATOR_COUNT; index++) {
        IndicatorDescription* indicator = &(gIndicatorDescriptions[index]);

        char text[1024];
        strcpy(text, getmsg(&messageList, &messageListItem, indicator->title));

        int color = indicator->isBad ? _colorTable[31744] : _colorTable[992];

        memcpy(indicator->data, indicatorBoxFrmImage.getData(), INDICATOR_BOX_WIDTH * INDICATOR_BOX_HEIGHT);

        // NOTE: For unknown reason it uses 24 as a height of the box to center
        // the title. One explanation is that these boxes were redesigned, but
        // this value was not changed. On the other hand 24 is
        // [INDICATOR_BOX_HEIGHT] + [INDICATOR_BOX_CONNECTOR_WIDTH]. Maybe just
        // a coincidence. I guess we'll never find out.
        int y = (24 - fontGetLineHeight()) / 2;
        int x = (INDICATOR_BOX_WIDTH - fontGetStringWidth(text)) / 2;
        fontDrawText(indicator->data + INDICATOR_BOX_WIDTH * y + x, text, INDICATOR_BOX_WIDTH, INDICATOR_BOX_WIDTH, color);
    }

    gIndicatorBarIsVisible = true;
    indicatorBarRefresh();

    messageListFree(&messageList);
    fontSetCurrent(oldFont);

    return 0;
}

// 0x461454
static void interfaceBarFree()
{
    if (gIndicatorBarWindow != -1) {
        windowDestroy(gIndicatorBarWindow);
        gIndicatorBarWindow = -1;
    }

    for (int index = 0; index < INDICATOR_COUNT; index++) {
        IndicatorDescription* indicatorBoxDescription = &(gIndicatorDescriptions[index]);
        if (indicatorBoxDescription->data != nullptr) {
            internal_free(indicatorBoxDescription->data);
            indicatorBoxDescription->data = nullptr;
        }
    }
}

// NOTE: This function is not referenced in the original code.
//
// 0x4614A0
static void indicatorBarReset()
{
    if (gIndicatorBarWindow != -1) {
        windowDestroy(gIndicatorBarWindow);
        gIndicatorBarWindow = -1;
    }

    gIndicatorBarIsVisible = true;
}

// Updates indicator bar.
//
// 0x4614CC
int indicatorBarRefresh()
{
    if (gInterfaceBarWindow != -1 && gIndicatorBarIsVisible && !gInterfaceBarHidden) {
        for (int index = 0; index < INDICATOR_SLOTS_COUNT; index++) {
            gIndicatorSlots[index] = -1;
        }

        int count = 0;

        if (dudeHasState(DUDE_STATE_SNEAKING)) {
            if (indicatorBarAdd(INDICATOR_SNEAK)) {
                ++count;
            }
        }

        if (dudeHasState(DUDE_STATE_LEVEL_UP_AVAILABLE)) {
            if (indicatorBarAdd(INDICATOR_LEVEL)) {
                ++count;
            }
        }

        if (dudeHasState(DUDE_STATE_ADDICTED)) {
            if (indicatorBarAdd(INDICATOR_ADDICT)) {
                ++count;
            }
        }

        if (critterGetPoison(gDude) > POISON_INDICATOR_THRESHOLD) {
            if (indicatorBarAdd(INDICATOR_POISONED)) {
                ++count;
            }
        }

        if (critterGetRadiation(gDude) > RADATION_INDICATOR_THRESHOLD) {
            if (indicatorBarAdd(INDICATOR_RADIATED)) {
                ++count;
            }
        }

        if (count > 1) {
            qsort(gIndicatorSlots, count, sizeof(*gIndicatorSlots), indicatorBoxCompareByPosition);
        }

        if (gIndicatorBarWindow != -1) {
            windowDestroy(gIndicatorBarWindow);
            gIndicatorBarWindow = -1;
        }

        if (count != 0) {
            Rect interfaceBarWindowRect;
            windowGetRect(gInterfaceBarWindow, &interfaceBarWindowRect);

            gIndicatorBarWindow = windowCreate(interfaceBarWindowRect.left,
                screenGetHeight() - INTERFACE_BAR_HEIGHT - INDICATOR_BOX_HEIGHT,
                (INDICATOR_BOX_WIDTH - INDICATOR_BOX_CONNECTOR_WIDTH) * count,
                INDICATOR_BOX_HEIGHT,
                _colorTable[0],
                0);
            indicatorBarRender(count);
            windowRefresh(gIndicatorBarWindow);
        }

        return count;
    }

    if (gIndicatorBarWindow != -1) {
        windowDestroy(gIndicatorBarWindow);
        gIndicatorBarWindow = -1;
    }

    return 0;
}

// 0x461624
static int indicatorBoxCompareByPosition(const void* a, const void* b)
{
    int indicatorBox1 = *(int*)a;
    int indicatorBox2 = *(int*)b;

    if (indicatorBox1 == indicatorBox2) {
        return 0;
    } else if (indicatorBox1 < indicatorBox2) {
        return -1;
    } else {
        return 1;
    }
}

// Renders indicator boxes into the indicator bar window.
//
// 0x461648
static void indicatorBarRender(int count)
{
    if (gIndicatorBarWindow == -1) {
        return;
    }

    if (count == 0) {
        return;
    }

    int windowWidth = windowGetWidth(gIndicatorBarWindow);
    unsigned char* windowBuffer = windowGetBuffer(gIndicatorBarWindow);

    // The initial number of connections is 2 - one is first box to the screen
    // boundary, the other is female socket (initially empty). Every displayed
    // box adds one more connection (it is "plugged" into previous box and
    // exposes it's own empty female socket).
    int connections = 2;

    // The width of displayed indicator boxes as if there were no connections.
    int unconnectedIndicatorsWidth = 0;

    // The X offset to display next box.
    int x = 0;

    // The first box is connected to the screen boundary, so we have to clamp
    // male connectors on the left.
    int connectorWidthCompensation = INDICATOR_BOX_CONNECTOR_WIDTH;

    for (int index = 0; index < count; index++) {
        int indicator = gIndicatorSlots[index];
        IndicatorDescription* indicatorDescription = &(gIndicatorDescriptions[indicator]);

        blitBufferToBufferTrans(indicatorDescription->data + connectorWidthCompensation,
            INDICATOR_BOX_WIDTH - connectorWidthCompensation,
            INDICATOR_BOX_HEIGHT,
            INDICATOR_BOX_WIDTH,
            windowBuffer + x, windowWidth);

        connectorWidthCompensation = 0;

        unconnectedIndicatorsWidth += INDICATOR_BOX_WIDTH;
        x = unconnectedIndicatorsWidth - INDICATOR_BOX_CONNECTOR_WIDTH * connections;
        connections++;
    }
}

// Adds indicator to the indicator bar.
//
// Returns `true` if indicator was added, or `false` if there is no available
// space in the indicator bar.
//
// 0x4616F0
static bool indicatorBarAdd(int indicator)
{
    for (int index = 0; index < INDICATOR_SLOTS_COUNT; index++) {
        if (gIndicatorSlots[index] == -1) {
            gIndicatorSlots[index] = indicator;
            return true;
        }
    }

    debugPrint("\nINTRFACE: no free bar box slots!\n");

    return false;
}

// 0x461740
bool indicatorBarShow()
{
    bool oldIsVisible = gIndicatorBarIsVisible;
    gIndicatorBarIsVisible = true;

    indicatorBarRefresh();

    return oldIsVisible;
}

// 0x461760
bool indicatorBarHide()
{
    bool oldIsVisible = gIndicatorBarIsVisible;
    gIndicatorBarIsVisible = false;

    indicatorBarRefresh();

    return oldIsVisible;
}

// Realtime build: A BAR NARROWER THAN ITS ART (2026-10-23, his "pipboy obrazovka může být širší, když se
// tam najde místo pro lb a rb"). The Hi-Res patch ships the bar at 800 only, and on our 853-wide screen
// an 800 bar leaves no room beside it for the LB / RB slots. Its message screen is the 640 bar's drawn
// wider -- everything right of it is the 640 art column for column -- so the narrower bar is the wide
// one with columns taken out of the middle of that screen: nothing redrawn, nothing resampled. The cut
// sits where the columns that become neighbours differ least (measured on HR_IFACE_800: 80 columns
// from x 146, the joint about as rough as the art's own grain). Everything right of the screen then
// moves left with gInterfaceBarContentOffset, exactly as it moves right for the full wide bar.
// Returns the pixels (width x art height, malloc'd) or nullptr.
#define INTERFACE_BAR_CUT_CENTER 186

static unsigned char* interface_bar_cut(Art* wide, int width)
{
    int srcWidth = artGetWidth(wide, 0, 0);
    int height = artGetHeight(wide, 0, 0);
    unsigned char* src = artGetFrameData(wide, 0, 0);
    int remove = srcWidth - width;
    // Only out of the stretched part of the screen: never narrower than the 640 bar it was widened from.
    if (src == nullptr || remove <= 0 || width < 640 || remove > srcWidth - 640) {
        return nullptr;
    }

    // The screen's glass runs from DISPLAY_MONITOR_X (23) for 167 + (srcWidth - 640) columns; keep 40
    // columns of its rounded ends untouched on either side.
    int screenLeft = 23 + 40;
    int screenRight = 23 + 167 + (srcWidth - 640) - 40;
    int cut = INTERFACE_BAR_CUT_CENTER - remove / 2;
    if (cut + remove > screenRight) {
        cut = screenRight - remove;
    }
    if (cut < screenLeft) {
        cut = screenLeft;
    }

    unsigned char* pixels = (unsigned char*)internal_malloc(width * height);
    if (pixels == nullptr) {
        return nullptr;
    }
    for (int y = 0; y < height; y++) {
        memcpy(pixels + width * y, src + srcWidth * y, cut);
        memcpy(pixels + width * y + cut, src + srcWidth * y + cut + remove, width - cut);
    }
    debugPrint("interface: %d bar cut from the %d art (%d columns at x %d)\n", width, srcWidth, remove, cut);
    return pixels;
}

static void customInterfaceBarInit()
{
    gInterfaceBarContentOffset = gInterfaceBarWidth - 640;

    if (gInterfaceBarContentOffset > 0 && screenGetWidth() > 640) {
        char path[COMPAT_MAX_PATH];
        snprintf(path, sizeof(path), "art\\intrface\\HR_IFACE_%d.FRM", gInterfaceBarWidth);

        gCustomInterfaceBarBackground = artLoad(path);

        // Realtime build: no art at this width -- cut it out of the wider bar it was narrowed from.
        if (gCustomInterfaceBarBackground == nullptr && gInterfaceBarSourceWidth > gInterfaceBarWidth) {
            snprintf(path, sizeof(path), "art\\intrface\\HR_IFACE_%d.FRM", gInterfaceBarSourceWidth);
            Art* wide = artLoad(path);
            if (wide != nullptr) {
                gCustomInterfaceBarCut = interface_bar_cut(wide, gInterfaceBarWidth);
                internal_free(wide);
            }
        }
    }

    if (gCustomInterfaceBarBackground != nullptr || gCustomInterfaceBarCut != nullptr) {
        gInterfaceBarIsCustom = true;
    } else {
        gInterfaceBarContentOffset = 0;
        gInterfaceBarWidth = 640;
        gInterfaceBarIsCustom = false;
    }
}

static void customInterfaceBarExit()
{
    if (gCustomInterfaceBarBackground != nullptr) {
        internal_free(gCustomInterfaceBarBackground);
        gCustomInterfaceBarBackground = nullptr;
    }

    if (gCustomInterfaceBarCut != nullptr) {
        internal_free(gCustomInterfaceBarCut);
        gCustomInterfaceBarCut = nullptr;
    }
}

unsigned char* customInterfaceBarGetBackgroundImageData()
{
    if (!gInterfaceBarIsCustom) {
        return nullptr;
    }

    // Realtime build: the bar cut from a wider one (interface_bar_cut), same pitch as any custom bar.
    if (gCustomInterfaceBarCut != nullptr) {
        return gCustomInterfaceBarCut;
    }

    return artGetFrameData(gCustomInterfaceBarBackground, 0, 0);
}

static void sidePanelsInit()
{
    if (gInterfaceBarMode) {
        return;
    }

    if (gInterfaceSidePanelsImageId == 0) {
        return;
    }

    if (gInterfaceBarWidth >= screenGetWidth()) {
        return;
    }

    Rect windowRect;
    windowGetRect(gInterfaceBarWindow, &windowRect);

    gInterfaceSidePanelsLeadingWindow = windowCreate(0, windowRect.top, windowRect.left, windowRect.bottom - windowRect.top + 1, 0, WINDOW_HIDDEN | WINDOW_DONT_MOVE_TOP);
    gInterfaceSidePanelsTrailingWindow = windowCreate(windowRect.right + 1, windowRect.top, screenGetWidth() - windowRect.right - 1, windowRect.bottom - windowRect.top + 1, 0, WINDOW_HIDDEN | WINDOW_DONT_MOVE_TOP);

    char path[COMPAT_MAX_PATH];
    snprintf(path, sizeof(path), "art\\intrface\\HR_IFACELFT%d.frm", gInterfaceSidePanelsImageId);
    sidePanelsDraw(path, gInterfaceSidePanelsLeadingWindow, true);

    snprintf(path, sizeof(path), "art\\intrface\\HR_IFACERHT%d.frm", gInterfaceSidePanelsImageId);
    sidePanelsDraw(path, gInterfaceSidePanelsTrailingWindow, false);
}

static void sidePanelsExit()
{
    if (gInterfaceSidePanelsTrailingWindow != -1) {
        windowDestroy(gInterfaceSidePanelsTrailingWindow);
        gInterfaceSidePanelsTrailingWindow = -1;
    }

    if (gInterfaceSidePanelsLeadingWindow != -1) {
        windowDestroy(gInterfaceSidePanelsLeadingWindow);
        gInterfaceSidePanelsLeadingWindow = -1;
    }
}

static void sidePanelsHide()
{
    if (gInterfaceSidePanelsLeadingWindow != -1) {
        windowHide(gInterfaceSidePanelsLeadingWindow);
    }

    if (gInterfaceSidePanelsTrailingWindow != -1) {
        windowHide(gInterfaceSidePanelsTrailingWindow);
    }
}

static void sidePanelsShow()
{
    if (gInterfaceSidePanelsLeadingWindow != -1) {
        windowShow(gInterfaceSidePanelsLeadingWindow);
    }

    if (gInterfaceSidePanelsTrailingWindow != -1) {
        windowShow(gInterfaceSidePanelsTrailingWindow);
    }
}

static void sidePanelsDraw(const char* path, int win, bool isLeading)
{
    Art* image = artLoad(path);
    if (image == nullptr) {
        return;
    }

    unsigned char* imageData = artGetFrameData(image, 0, 0);

    int imageWidth = artGetWidth(image, 0, 0);
    int imageHeight = artGetHeight(image, 0, 0);

    int windowWidth = windowGetWidth(win);
    int windowHeight = windowGetHeight(win);

    int width = std::min(imageWidth, windowWidth);

    if (!gInterfaceSidePanelsExtendFromScreenEdge && isLeading) {
        imageData += imageWidth - width;
    }

    if (gInterfaceSidePanelsExtendFromScreenEdge && !isLeading) {
        imageData += imageWidth - width;
    }

    blitBufferToBufferStretch(imageData,
        width,
        imageHeight,
        imageWidth,
        windowGetBuffer(win),
        windowWidth,
        windowHeight,
        windowWidth);

    internal_free(image);
}

// NOTE: Follows Sfall implementation of `GetCurrentAttackMode`. It slightly
// differs from `interfaceGetCurrentHitMode` (can return one of `reload` hit
// modes, the default is `punch`).
//
// 0x45EF6C
bool interface_get_current_attack_mode(int* hit_mode)
{
    if (gInterfaceBarWindow == -1) {
        return false;
    }

    switch (gInterfaceItemStates[gInterfaceCurrentHand].action) {
    case INTERFACE_ITEM_ACTION_PRIMARY_AIMING:
    case INTERFACE_ITEM_ACTION_PRIMARY:
        *hit_mode = gInterfaceItemStates[gInterfaceCurrentHand].primaryHitMode;
        break;
    case INTERFACE_ITEM_ACTION_SECONDARY_AIMING:
    case INTERFACE_ITEM_ACTION_SECONDARY:
        *hit_mode = gInterfaceItemStates[gInterfaceCurrentHand].secondaryHitMode;
        break;
    case INTERFACE_ITEM_ACTION_RELOAD:
        *hit_mode = gInterfaceCurrentHand == HAND_LEFT
            ? HIT_MODE_LEFT_WEAPON_RELOAD
            : HIT_MODE_RIGHT_WEAPON_RELOAD;
        break;
    default:
        *hit_mode = HIT_MODE_PUNCH;
        break;
    }

    return true;
}

} // namespace fallout
