#include "inventory.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "actions.h"
#include "animation.h"
#include "art.h"
#include "color.h"
#include "combat.h"
#include "combat_ai.h"
#include "critter.h"
#include "dbox.h"
#include "debug.h"
#include "dialog.h"
#include "display_monitor.h"
#include "draw.h"
#include "game.h"
#include "game_dialog.h"
#include "game_mouse.h"
#include "game_sound.h"
#include "input.h"
#include "interface.h"
#include "item.h"
#include "kb.h"
#include "light.h"
#include "map.h"
#include "message.h"
#include "mouse.h"
#include "object.h"
#include "party_member.h"
#include "perk.h"
#include "platform_compat.h"
#include "proto.h"
#include "proto_instance.h"
#include "random.h"
#include "reaction.h"
#include "scripts.h"
#include "skill.h"
#include "stat.h"
#include "svga.h"
#include "text_font.h"
#include "tile.h"
#include "window_manager.h"

#include <SDL.h>

// Realtime / controller (fallout1-ce's inventry.cc changes, ported).
#include "rt/game/combat_rt.h"
#include "rt/game/game_trace.h"
#include "rt/game/gamepad_ui.h"
#include "rt/game/interact_prompt.h"
#include "rt/rt_hooks.h"

namespace fallout {

#define INVENTORY_WINDOW_X 80
#define INVENTORY_WINDOW_Y 0

#define INVENTORY_TRADE_WINDOW_X 80
#define INVENTORY_TRADE_WINDOW_Y 290
#define INVENTORY_TRADE_WINDOW_WIDTH 480
#define INVENTORY_TRADE_WINDOW_HEIGHT 180

#define INVENTORY_LARGE_SLOT_WIDTH 90
#define INVENTORY_LARGE_SLOT_HEIGHT 61

#define INVENTORY_SLOT_WIDTH 64
#define INVENTORY_SLOT_HEIGHT 48

#define INVENTORY_LEFT_HAND_SLOT_X 154
#define INVENTORY_LEFT_HAND_SLOT_Y 286
#define INVENTORY_LEFT_HAND_SLOT_MAX_X (INVENTORY_LEFT_HAND_SLOT_X + INVENTORY_LARGE_SLOT_WIDTH)
#define INVENTORY_LEFT_HAND_SLOT_MAX_Y (INVENTORY_LEFT_HAND_SLOT_Y + INVENTORY_LARGE_SLOT_HEIGHT)

#define INVENTORY_RIGHT_HAND_SLOT_X 245
#define INVENTORY_RIGHT_HAND_SLOT_Y 286
#define INVENTORY_RIGHT_HAND_SLOT_MAX_X (INVENTORY_RIGHT_HAND_SLOT_X + INVENTORY_LARGE_SLOT_WIDTH)
#define INVENTORY_RIGHT_HAND_SLOT_MAX_Y (INVENTORY_RIGHT_HAND_SLOT_Y + INVENTORY_LARGE_SLOT_HEIGHT)

#define INVENTORY_ARMOR_SLOT_X 154
#define INVENTORY_ARMOR_SLOT_Y 183
#define INVENTORY_ARMOR_SLOT_MAX_X (INVENTORY_ARMOR_SLOT_X + INVENTORY_LARGE_SLOT_WIDTH)
#define INVENTORY_ARMOR_SLOT_MAX_Y (INVENTORY_ARMOR_SLOT_Y + INVENTORY_LARGE_SLOT_HEIGHT)

#define INVENTORY_TRADE_SCROLLER_Y 30
#define INVENTORY_TRADE_INNER_SCROLLER_Y 20

#define INVENTORY_TRADE_LEFT_SCROLLER_X 29
#define INVENTORY_TRADE_LEFT_SCROLLER_Y INVENTORY_TRADE_SCROLLER_Y

#define INVENTORY_TRADE_RIGHT_SCROLLER_X 388
#define INVENTORY_TRADE_RIGHT_SCROLLER_Y INVENTORY_TRADE_SCROLLER_Y

#define INVENTORY_TRADE_INNER_LEFT_SCROLLER_X 165
#define INVENTORY_TRADE_INNER_LEFT_SCROLLER_Y INVENTORY_TRADE_INNER_SCROLLER_Y

#define INVENTORY_TRADE_INNER_RIGHT_SCROLLER_X 250
#define INVENTORY_TRADE_INNER_RIGHT_SCROLLER_Y INVENTORY_TRADE_INNER_SCROLLER_Y

#define INVENTORY_TRADE_LEFT_SCROLLER_TRACKING_X 0
#define INVENTORY_TRADE_LEFT_SCROLLER_TRACKING_Y 10
#define INVENTORY_TRADE_LEFT_SCROLLER_TRACKING_MAX_X (INVENTORY_TRADE_LEFT_SCROLLER_TRACKING_X + INVENTORY_SLOT_WIDTH)

#define INVENTORY_TRADE_INNER_LEFT_SCROLLER_TRACKING_X 165
#define INVENTORY_TRADE_INNER_LEFT_SCROLLER_TRACKING_Y 10
#define INVENTORY_TRADE_INNER_LEFT_SCROLLER_TRACKING_MAX_X (INVENTORY_TRADE_INNER_LEFT_SCROLLER_TRACKING_X + INVENTORY_SLOT_WIDTH)

#define INVENTORY_TRADE_INNER_RIGHT_SCROLLER_TRACKING_X 250
#define INVENTORY_TRADE_INNER_RIGHT_SCROLLER_TRACKING_Y 10
#define INVENTORY_TRADE_INNER_RIGHT_SCROLLER_TRACKING_MAX_X (INVENTORY_TRADE_INNER_RIGHT_SCROLLER_TRACKING_X + INVENTORY_SLOT_WIDTH)

#define INVENTORY_TRADE_RIGHT_SCROLLER_TRACKING_X 395
#define INVENTORY_TRADE_RIGHT_SCROLLER_TRACKING_Y 10
#define INVENTORY_TRADE_RIGHT_SCROLLER_TRACKING_MAX_X (INVENTORY_TRADE_RIGHT_SCROLLER_TRACKING_X + INVENTORY_SLOT_WIDTH)

#define INVENTORY_LOOT_LEFT_SCROLLER_X 180
#define INVENTORY_LOOT_LEFT_SCROLLER_Y 37
#define INVENTORY_LOOT_LEFT_SCROLLER_MAX_X (INVENTORY_LOOT_LEFT_SCROLLER_X + INVENTORY_SLOT_WIDTH)

#define INVENTORY_LOOT_RIGHT_SCROLLER_X 297
#define INVENTORY_LOOT_RIGHT_SCROLLER_Y 37
#define INVENTORY_LOOT_RIGHT_SCROLLER_MAX_X (INVENTORY_LOOT_RIGHT_SCROLLER_X + INVENTORY_SLOT_WIDTH)

#define INVENTORY_SCROLLER_X 46
#define INVENTORY_SCROLLER_Y 35
#define INVENTORY_SCROLLER_MAX_X (INVENTORY_SCROLLER_X + INVENTORY_SLOT_WIDTH)

#define INVENTORY_BODY_VIEW_WIDTH 60
#define INVENTORY_BODY_VIEW_HEIGHT 100

#define INVENTORY_PC_BODY_VIEW_X 176
#define INVENTORY_PC_BODY_VIEW_Y 37
#define INVENTORY_PC_BODY_VIEW_MAX_X (INVENTORY_PC_BODY_VIEW_X + INVENTORY_BODY_VIEW_WIDTH)
#define INVENTORY_PC_BODY_VIEW_MAX_Y (INVENTORY_PC_BODY_VIEW_Y + INVENTORY_BODY_VIEW_HEIGHT)

#define INVENTORY_LOOT_RIGHT_BODY_VIEW_X 422
#define INVENTORY_LOOT_RIGHT_BODY_VIEW_Y 35

#define INVENTORY_LOOT_LEFT_BODY_VIEW_X 44
#define INVENTORY_LOOT_LEFT_BODY_VIEW_Y 35

#define INVENTORY_SUMMARY_X 297
#define INVENTORY_SUMMARY_Y 44
#define INVENTORY_SUMMARY_MAX_X 440

#define INVENTORY_WINDOW_WIDTH 499
#define INVENTORY_USE_ON_WINDOW_WIDTH 292
#define INVENTORY_LOOT_WINDOW_WIDTH 537
#define INVENTORY_TRADE_WINDOW_WIDTH 480
#define INVENTORY_TIMER_WINDOW_WIDTH 259

#define INVENTORY_TRADE_BACKGROUND_WINDOW_WIDTH 640
#define INVENTORY_TRADE_BACKGROUND_WINDOW_HEIGHT 480
#define INVENTORY_TRADE_WINDOW_OFFSET ((INVENTORY_TRADE_BACKGROUND_WINDOW_WIDTH - INVENTORY_TRADE_WINDOW_WIDTH) / 2)

#define INVENTORY_SLOT_PADDING 4

#define INVENTORY_SCROLLER_X_PAD (INVENTORY_SCROLLER_X + INVENTORY_SLOT_PADDING)
#define INVENTORY_SCROLLER_Y_PAD (INVENTORY_SCROLLER_Y + INVENTORY_SLOT_PADDING)

#define INVENTORY_LOOT_LEFT_SCROLLER_X_PAD (INVENTORY_LOOT_LEFT_SCROLLER_X + INVENTORY_SLOT_PADDING)
#define INVENTORY_LOOT_LEFT_SCROLLER_Y_PAD (INVENTORY_LOOT_LEFT_SCROLLER_Y + INVENTORY_SLOT_PADDING)

#define INVENTORY_LOOT_RIGHT_SCROLLER_X_PAD (INVENTORY_LOOT_RIGHT_SCROLLER_X + INVENTORY_SLOT_PADDING)
#define INVENTORY_LOOT_RIGHT_SCROLLER_Y_PAD (INVENTORY_LOOT_RIGHT_SCROLLER_Y + INVENTORY_SLOT_PADDING)

#define INVENTORY_TRADE_LEFT_SCROLLER_X_PAD (INVENTORY_TRADE_LEFT_SCROLLER_Y + INVENTORY_SLOT_PADDING)
#define INVENTORY_TRADE_LEFT_SCROLLER_Y_PAD (INVENTORY_TRADE_LEFT_SCROLLER_Y + INVENTORY_SLOT_PADDING)

#define INVENTORY_TRADE_RIGHT_SCROLLER_X_PAD (INVENTORY_TRADE_RIGHT_SCROLLER_X + INVENTORY_SLOT_PADDING)
#define INVENTORY_TRADE_RIGHT_SCROLLER_Y_PAD (INVENTORY_TRADE_RIGHT_SCROLLER_Y + INVENTORY_SLOT_PADDING)

#define INVENTORY_TRADE_INNER_LEFT_SCROLLER_X_PAD (INVENTORY_TRADE_INNER_LEFT_SCROLLER_X + INVENTORY_SLOT_PADDING)
#define INVENTORY_TRADE_INNER_LEFT_SCROLLER_Y_PAD (INVENTORY_TRADE_INNER_LEFT_SCROLLER_Y + INVENTORY_SLOT_PADDING)

#define INVENTORY_TRADE_INNER_RIGHT_SCROLLER_X_PAD (INVENTORY_TRADE_INNER_RIGHT_SCROLLER_X + INVENTORY_SLOT_PADDING)
#define INVENTORY_TRADE_INNER_RIGHT_SCROLLER_Y_PAD (INVENTORY_TRADE_INNER_RIGHT_SCROLLER_Y + INVENTORY_SLOT_PADDING)

#define INVENTORY_SLOT_WIDTH_PAD (INVENTORY_SLOT_WIDTH - INVENTORY_SLOT_PADDING * 2)
#define INVENTORY_SLOT_HEIGHT_PAD (INVENTORY_SLOT_HEIGHT - INVENTORY_SLOT_PADDING * 2)

#define INVENTORY_NORMAL_WINDOW_PC_ROTATION_DELAY (1000U / ROTATION_COUNT)
#define INVENTORY_FRM_COUNT 12

typedef enum InventoryArrowFrm {
    INVENTORY_ARROW_FRM_LEFT_ARROW_UP,
    INVENTORY_ARROW_FRM_LEFT_ARROW_DOWN,
    INVENTORY_ARROW_FRM_RIGHT_ARROW_UP,
    INVENTORY_ARROW_FRM_RIGHT_ARROW_DOWN,
    INVENTORY_ARROW_FRM_COUNT,
} InventoryArrowFrm;

// Realtime build: InventoryWindowCursor and InventoryWindowType moved to inventory.h (the gamepad
// layer sets the cursor and asks which window is up).

typedef struct InventoryWindowConfiguration {
    int frmId; // artId
    int width;
    int height;
    int x;
    int y;
} InventoryWindowDescription;

typedef struct InventoryCursorData {
    Art* frm;
    unsigned char* frmData;
    int width;
    int height;
    int offsetX;
    int offsetY;
    CacheEntry* frmHandle;
} InventoryCursorData;

typedef enum InventoryMoveResult {
    INVENTORY_MOVE_RESULT_FAILED,
    INVENTORY_MOVE_RESULT_CAUGHT_STEALING,
    INVENTORY_MOVE_RESULT_SUCCESS,
} InventoryMoveResult;

static int inventoryMessageListInit();
static int inventoryMessageListFree();
static bool _setup_inventory(int inventoryWindowType);
static void _exit_inventory(bool shouldEnableIso);
static void _display_inventory(int stackOffset, int draggedSlotIndex, int inventoryWindowType);
static void _display_target_inventory(int stackOffset, int dragSlotIndex, Inventory* inventory, int inventoryWindowType);
static void _display_inventory_info(Object* item, int quantity, unsigned char* dest, int pitch, bool isDragged);
static void _display_body(int fid, int inventoryWindowType);
static int inventoryCommonInit();
static void inventoryCommonFree();
static void inventoryItemSlotOnMouseEnter(int btn, int keyCode);
static void inventoryItemSlotOnMouseExit(int btn, int keyCode);
static void _inven_update_lighting(Object* activeItem);
static void _inven_pickup(int keyCode, int indexOffset);
static void _switch_hand(Object* sourceItem, Object** targetSlot, Object** sourceSlot, int itemIndex);
static void _adjust_fid();
static void inventoryRenderSummary();
static int _inven_from_button(int keyCode, Object** outItem, Object*** outItemSlot, Object** outOwner);
static void inventoryExamineItem(Object* critter, Object* item);
static void inventoryWindowOpenContextMenu(int eventCode, int inventoryWindowType);
static InventoryMoveResult _move_inventory(Object* item, int slotIndex, Object* targetObj, bool isPlanting, int gpQuantity = 0);
static int _barter_compute_value(Object* dude, Object* npc);
static int _barter_attempt_transaction(Object* dude, Object* offerTable, Object* npc, Object* barterTable);
static void _barter_move_inventory(Object* item, int quantity, int slotIndex, int indexOffset, Object* npc, Object* sourceTable, bool fromDude);
static void _barter_move_from_table_inventory(Object* item, int quantity, int slotIndex, Object* npc, Object* sourceTable, bool fromDude);
static void inventoryWindowRenderInnerInventories(int win, Object* leftTable, Object* rightTable, int draggedSlotIndex);
static void _container_enter(int keyCode, int inventoryWindowType);
static void _container_exit(int keyCode, int inventoryWindowType);
static int _drop_into_container(Object* container, Object* item, int sourceIndex, Object** itemSlot, int quantity);
static int _drop_ammo_into_weapon(Object* weapon, Object* ammo, Object** ammoItemSlot, int quantity, int keyCode);
static void _draw_amount(int value, int inventoryWindowType);
static int inventoryQuantitySelect(int inventoryWindowType, Object* item, int maximum);
static int inventoryQuantityWindowInit(int inventoryWindowType, Object* item);
static int inventoryQuantityWindowFree(int inventoryWindowType);

// --- REALTIME / CONTROLLER (fallout1-ce's inventry.cc changes, ported) ------------------------------
//
// ⚠️ FALLOUT 2 LISTS ARE SHOWN NEWEST FIRST: visible row r of a list is items[length - (r + offset + 1)],
// where fallout1-ce's is items[r + offset]. Every helper below that turns a row into an item uses the
// Fallout 2 mapping (the same one _inven_from_button uses).

// How long A must be held on a barter slot before it describes the item instead of moving it.
// Same 220 ms the pad wheels use, so every tap-vs-hold on the controller feels identical.
#define BARTER_LOOK_HOLD_MS 220

// How long a D-pad direction must be held before the inventory list starts auto-scrolling, and how
// fast it steps once it does.
#define INVEN_PAD_REPEAT_DELAY_MS 350
#define INVEN_PAD_REPEAT_RATE_MS 70

// ⚠️ THE TIMER WINDOW CAN PUT THE EXPLOSIVE DOWN TOO (fallout1-ce 2026-10-16, "nebo do artu přidat
// tlačítko nastavit čas a položit?"). The window's PLACE (the plate under the clock, X on the pad)
// sets the timer exactly as DONE does and also asks for the explosive to go on the ground; the one who
// armed it reads the request back (inven_set_timer_place_requested) and drops it. The plate is the ALL
// button's art from the move-items mode of this same window, with the label drawn into a COPY.
#define MT_KEY_PLACE 5001
#define MT_PLACE_W 94
#define MT_PLACE_H 33
static bool mt_place_requested = false;
static unsigned char mt_place_up[MT_PLACE_W * MT_PLACE_H];
static unsigned char mt_place_down[MT_PLACE_W * MT_PLACE_H];

// True only while the normal inventory screen (inventoryOpen) is up: the controller's equip/close
// shortcuts must not misfire inside loot, trade, or use-on screens that reuse the same window statics.
static bool normal_inventory_open = false;

static void inven_place_if_requested(Object* item, Object* owner, Object** handSlot);
static void barter_warp_cursor(int sx, int sy);
static void barter_draw_deal();
void inven_explosive_dropped_notice();


// 0x46E6D0
static const int gSummaryStats[7] = {
    STAT_CURRENT_HIT_POINTS,
    STAT_ARMOR_CLASS,
    STAT_DAMAGE_THRESHOLD,
    STAT_DAMAGE_THRESHOLD_LASER,
    STAT_DAMAGE_THRESHOLD_FIRE,
    STAT_DAMAGE_THRESHOLD_PLASMA,
    STAT_DAMAGE_THRESHOLD_EXPLOSION,
};

// 0x46E6EC
static const int gSummaryStats2[7] = {
    STAT_MAXIMUM_HIT_POINTS,
    -1,
    STAT_DAMAGE_RESISTANCE,
    STAT_DAMAGE_RESISTANCE_LASER,
    STAT_DAMAGE_RESISTANCE_FIRE,
    STAT_DAMAGE_RESISTANCE_PLASMA,
    STAT_DAMAGE_RESISTANCE_EXPLOSION,
};

// 0x46E708
static const int gInventoryArrowFrmIds[INVENTORY_ARROW_FRM_COUNT] = {
    122, // left arrow up
    123, // left arrow down
    124, // right arrow up
    125, // right arrow down
};

// The number of items to show in scroller.
//
// 0x519054
static int gInventorySlotsCount = 6;

// 0x519058
static Object* _inven_dude = nullptr;

// Probably fid of armor to display in inventory dialog.
//
// 0x51905C
static int _inven_pid = -1;

// 0x519060
static bool _inven_is_initialized = false;

// 0x519064
static int _inven_display_msg_line = 1;

// 0x519068
static const InventoryWindowDescription gInventoryWindowDescriptions[INVENTORY_WINDOW_TYPE_COUNT] = {
    { 48, INVENTORY_WINDOW_WIDTH, 377, 80, 0 },
    { 113, INVENTORY_USE_ON_WINDOW_WIDTH, 376, 80, 0 },
    { 114, INVENTORY_LOOT_WINDOW_WIDTH, 376, 80, 0 },
    { 111, INVENTORY_TRADE_WINDOW_WIDTH, 180, 80, 290 },
    { 305, INVENTORY_TIMER_WINDOW_WIDTH, 162, 140, 80 },
    { 305, INVENTORY_TIMER_WINDOW_WIDTH, 162, 140, 80 },
};

// 0x5190E0
static bool _dropped_explosive = false;

// 0x5190E4
static int gInventoryScrollUpButton = -1;

// 0x5190E8
static int gInventoryScrollDownButton = -1;

// 0x5190EC
static int gSecondaryInventoryScrollUpButton = -1;

// 0x5190F0
static int gSecondaryInventoryScrollDownButton = -1;

// 0x5190F4
static unsigned int gInventoryWindowDudeRotationTimestamp = 0;

// 0x5190F8
static int gInventoryWindowDudeRotation = 0;

// 0x5190FC
static const int gInventoryWindowCursorFrmIds[INVENTORY_WINDOW_CURSOR_COUNT] = {
    286, // pointing hand
    250, // action arrow
    282, // action pick
    283, // action menu
    266, // blank
};

// 0x519110
static Object* _last_target = nullptr;

// 0x519114
static const int _act_use[4] = {
    GAME_MOUSE_ACTION_MENU_ITEM_LOOK,
    GAME_MOUSE_ACTION_MENU_ITEM_USE,
    GAME_MOUSE_ACTION_MENU_ITEM_DROP,
    GAME_MOUSE_ACTION_MENU_ITEM_CANCEL,
};

// 0x519124
static const int _act_no_use[3] = {
    GAME_MOUSE_ACTION_MENU_ITEM_LOOK,
    GAME_MOUSE_ACTION_MENU_ITEM_DROP,
    GAME_MOUSE_ACTION_MENU_ITEM_CANCEL,
};

// 0x519130
static const int _act_just_use[3] = {
    GAME_MOUSE_ACTION_MENU_ITEM_LOOK,
    GAME_MOUSE_ACTION_MENU_ITEM_USE,
    GAME_MOUSE_ACTION_MENU_ITEM_CANCEL,
};

// 0x51913C
static const int _act_nothing[2] = {
    GAME_MOUSE_ACTION_MENU_ITEM_LOOK,
    GAME_MOUSE_ACTION_MENU_ITEM_CANCEL,
};

// 0x519144
static const int _act_weap[4] = {
    GAME_MOUSE_ACTION_MENU_ITEM_LOOK,
    GAME_MOUSE_ACTION_MENU_ITEM_UNLOAD,
    GAME_MOUSE_ACTION_MENU_ITEM_DROP,
    GAME_MOUSE_ACTION_MENU_ITEM_CANCEL,
};

// 0x519154
static const int _act_weap2[3] = {
    GAME_MOUSE_ACTION_MENU_ITEM_LOOK,
    GAME_MOUSE_ACTION_MENU_ITEM_UNLOAD,
    GAME_MOUSE_ACTION_MENU_ITEM_CANCEL,
};

// Scroll offsets to target inventory for every container nesting level (stack).
// 0x59E7EC
static int _target_stack_offset[10];

// inventory.msg
//
// 0x59E814
static MessageList gInventoryMessageList;

// Current target critter or container for every nesting level (stack).
// 0x59E81C
static Object* _target_stack[10];

// Scroll offsets to main inventory for every container nesting level (stack).
// 0x59E844
static int _stack_offset[10];

// Current critter or container for every nesting level (stack).
// 0x59E86C
static Object* _stack[10];

// 0x59E894
static int _mt_wid;

// Current barter price modifier, set from scripts.
// 0x59E898
static int _barter_mod;

// 0x59E89C
static int _btable_offset;

// 0x59E8A0
static int _ptable_offset;

// 0x59E8A4
static Inventory* _ptable_pud;

// 0x59E8A8
static InventoryCursorData gInventoryCursorData[INVENTORY_WINDOW_CURSOR_COUNT];

// 0x59E934
static Object* _ptable;

// 0x59E938
static InventoryPrintItemDescriptionHandler* gInventoryPrintItemDescriptionHandler;

// 0x59E93C
static int _im_value;

// 0x59E940
static int gInventoryCursor;

// 0x59E944
static Object* _btable;

// Current nesting level for viewing target's bag/backpack contents.
// 0x59E948
static int _target_curr_stack;

// 0x59E94C
static Inventory* _btable_pud;

// 0x59E950
static bool _inven_ui_was_disabled;

// 0x59E954
static Object* gInventoryArmor;

// 0x59E958
static Object* gInventoryLeftHandItem;

// Rotating character's fid.
//
// 0x59E95C
static int gInventoryWindowDudeFid;

// 0x59E960
static Inventory* _pud;

// 0x59E964
static int gInventoryWindow;

// item2
// 0x59E968
static Object* gInventoryRightHandItem;

// Current nesting level for viewing bag/backpack contents.
// 0x59E96C
static int _curr_stack;

// 0x59E970
static int gInventoryWindowMaxY;

// 0x59E974
static int gInventoryWindowMaxX;

// 0x59E978
static Inventory* _target_pud;

// 0x59E97C
static int _barter_back_win;

static FrmImage _inventoryFrmImages[INVENTORY_FRM_COUNT];
static FrmImage _moveFrmImages[8];

// 0x46E724
void _inven_reset_dude()
{
    _inven_dude = gDude;
    _inven_pid = 0x1000000;
}

// inventory_msg_init
// 0x46E73C
static int inventoryMessageListInit()
{
    char path[COMPAT_MAX_PATH];

    if (!messageListInit(&gInventoryMessageList))
        return -1;

    snprintf(path, sizeof(path), "%s%s", asc_5186C8, "inventry.msg");
    if (!messageListLoad(&gInventoryMessageList, path))
        return -1;

    return 0;
}

// inventory_msg_free
// 0x46E7A0
static int inventoryMessageListFree()
{
    messageListFree(&gInventoryMessageList);
    return 0;
}

// True if the mouse cursor is over either loot item panel (the player's or the
// container's). The gamepad layer uses this so that tap-A over a panel does a
// quick-take, while tap-A elsewhere (Done and other GNW buttons) does a normal
// click instead.
bool inven_loot_cursor_over_panel()
{
    if (gInventoryWindow == -1) {
        return false;
    }

    if (mouseHitTestInWindow(gInventoryWindow, INVENTORY_LOOT_LEFT_SCROLLER_X, INVENTORY_LOOT_LEFT_SCROLLER_Y, INVENTORY_LOOT_LEFT_SCROLLER_MAX_X, INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + INVENTORY_LOOT_LEFT_SCROLLER_Y)) {
        return true;
    }

    if (mouseHitTestInWindow(gInventoryWindow, INVENTORY_LOOT_RIGHT_SCROLLER_X, INVENTORY_LOOT_RIGHT_SCROLLER_Y, INVENTORY_LOOT_RIGHT_SCROLLER_MAX_X, INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + INVENTORY_LOOT_RIGHT_SCROLLER_Y)) {
        return true;
    }

    return false;
}

// True if the cursor is over the normal/use-on inventory item list. The gamepad
// layer uses this so tap-A over the list does a quick equip/use, while tap-A
// elsewhere (Done and other GNW buttons) does a normal click instead.
bool inven_normal_cursor_over_list()
{
    if (gInventoryWindow == -1) {
        return false;
    }

    return mouseHitTestInWindow(gInventoryWindow, INVENTORY_SCROLLER_X, INVENTORY_SCROLLER_Y, INVENTORY_SCROLLER_MAX_X, INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + INVENTORY_SCROLLER_Y);
}

// True if the cursor is over the body-armor slot of the normal inventory. The gamepad layer routes a
// tap of A there to inven_gamepad_quick_normal, which takes the armor off (2026-10-10).
bool inven_normal_cursor_over_armor()
{
    if (gInventoryWindow == -1) {
        return false;
    }

    return mouseHitTestInWindow(gInventoryWindow, INVENTORY_ARMOR_SLOT_X, INVENTORY_ARMOR_SLOT_Y, INVENTORY_ARMOR_SLOT_MAX_X, INVENTORY_ARMOR_SLOT_MAX_Y);
}

// Gamepad D-pad up/down -> inventory list scroll. One step on the press, then HOLD TO REPEAT;
// returns KEY_ARROW_UP / KEY_ARROW_DOWN (which the inventory loop already handles as scroll) or -1.
// D-pad ONLY on purpose: the LEFT STICK already drives the cursor (gpui_menu_snap) in this modal
// window, so using it here too would fight that. Read directly because gamepad_ui_tick doesn't run
// inside this modal loop (same pattern as the load/save slot list).
static int inven_gamepad_scroll_key()
{
    static bool prevUp = false;
    static bool prevDown = false;

    SDL_GameController* pad = gamepad_ui_get_controller();
    if (pad == NULL) {
        prevUp = false;
        prevDown = false;
        return -1;
    }

    SDL_GameControllerUpdate();
    bool upNow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0;
    bool downNow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0;

    // HOLD TO REPEAT. It used to be edge-only, so a long inventory meant one press per row --
    // "zatÃ­m jde pouze klikat". Now a held direction keeps stepping: one row on the press, then a
    // pause, then a steady stream, which is how every list on a controller behaves.
    static unsigned int heldSince = 0;
    static unsigned int lastRepeat = 0;

    int keyCode = -1;
    bool edge = (upNow && !prevUp) || (downNow && !prevDown);
    unsigned int now = getTicks();

    if (edge) {
        heldSince = now;
        lastRepeat = now;
        keyCode = upNow && !prevUp ? KEY_ARROW_UP : KEY_ARROW_DOWN;
    } else if (upNow || downNow) {
        // Delay before the stream starts, so a deliberate single step never runs away, then a fixed
        // interval between repeats.
        if (now - heldSince >= INVEN_PAD_REPEAT_DELAY_MS
            && now - lastRepeat >= INVEN_PAD_REPEAT_RATE_MS) {
            lastRepeat = now;
            keyCode = upNow ? KEY_ARROW_UP : KEY_ARROW_DOWN;
        }
    }

    prevUp = upNow;
    prevDown = downNow;
    return keyCode;
}

// 0x46E7B0
void inventoryOpen()
{
    // In realtime there is no turn scheduler (the combat "warm" bit is set but _combat_whose_turn()
    // never becomes the dude), so the vanilla turn-owner gate would refuse the inventory for the whole
    // fight. Realtime is always "the dude's turn"; the AP gate below still applies.
    if (isInCombat() && !combat_rt_enabled()) {
        if (_combat_whose_turn() != _inven_dude) {
            return;
        }
    }

    ScopedGameMode gm(GameMode::kInventory);

    if (inventoryCommonInit() == -1) {
        return;
    }

    if (isInCombat()) {
        if (_inven_dude == gDude) {
            int actionPointsRequired = 4 - 2 * perkGetRank(_inven_dude, PERK_QUICK_POCKETS);

            // Realtime keeps the engine combat.ap maxed (movement is free), so charge combat_rt's own
            // pool instead: opening the inventory mid-fight costs AP just like vanilla.
            bool affordable;
            if (combat_rt_enabled()) {
                affordable = actionPointsRequired <= 0 || combat_rt_spend_ap(actionPointsRequired);
            } else {
                affordable = actionPointsRequired <= 0 || actionPointsRequired <= gDude->data.critter.combat.ap;
            }

            if (!affordable) {
                // You don't have enough action points to use inventory
                MessageListItem messageListItem;
                messageListItem.num = 19;
                if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
                    displayMonitorAddMessage(messageListItem.text);
                }

                // NOTE: Uninline.
                inventoryCommonFree();

                return;
            }

            if (!combat_rt_enabled() && actionPointsRequired > 0) {
                if (actionPointsRequired > gDude->data.critter.combat.ap) {
                    gDude->data.critter.combat.ap = 0;
                } else {
                    gDude->data.critter.combat.ap -= actionPointsRequired;
                }
                interfaceRenderActionPoints(gDude->data.critter.combat.ap, _combat_free_move);
            }
        }
    }

    Object* oldArmor = critterGetArmor(_inven_dude);
    bool isoWasEnabled = _setup_inventory(INVENTORY_WINDOW_TYPE_NORMAL);
    reg_anim_clear(_inven_dude);
    inventoryRenderSummary();
    _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_NORMAL);
    inventorySetCursor(INVENTORY_WINDOW_CURSOR_HAND);

    normal_inventory_open = true;

    for (;;) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();

        // The pad's D-pad scrolls the item list (arrow keys the loop below already handles).
        if (keyCode == -1) {
            int padKey = inven_gamepad_scroll_key();
            if (padKey != -1) {
                keyCode = padKey;
            }
        }

        // SFALL: Close with 'I'.
        if (keyCode == KEY_ESCAPE || keyCode == KEY_UPPERCASE_I || keyCode == KEY_LOWERCASE_I) {
            break;
        }

        if (_game_user_wants_to_quit != 0) {
            break;
        }

        // Controller tap-A quick action: the gamepad layer sets a pending request from the input hook;
        // it runs here (outside the hook) where the use/equip pipeline's redraws are safe.
        if (gamepad_ui_take_quick_normal()) {
            inven_gamepad_quick_normal();
        }
        gamepad_ui_inventory_tick(); // the LB / RB quick slots, kept in step

        _display_body(-1, INVENTORY_WINDOW_TYPE_NORMAL);

        if (gameGetState() == GAME_STATE_5) {
            break;
        }

        if (keyCode == KEY_CTRL_Q || keyCode == KEY_CTRL_X) {
            showQuitConfirmationDialog();
        } else if (keyCode == KEY_HOME) {
            _stack_offset[_curr_stack] = 0;
            _display_inventory(0, -1, INVENTORY_WINDOW_TYPE_NORMAL);
        } else if (keyCode == KEY_ARROW_UP) {
            if (_stack_offset[_curr_stack] > 0) {
                _stack_offset[_curr_stack] -= 1;
                _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_NORMAL);
            }
        } else if (keyCode == KEY_PAGE_UP) {
            _stack_offset[_curr_stack] -= gInventorySlotsCount;
            if (_stack_offset[_curr_stack] < 0) {
                _stack_offset[_curr_stack] = 0;
            }
            _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_NORMAL);
        } else if (keyCode == KEY_END) {
            _stack_offset[_curr_stack] = _pud->length - gInventorySlotsCount;
            if (_stack_offset[_curr_stack] < 0) {
                _stack_offset[_curr_stack] = 0;
            }
            _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_NORMAL);
        } else if (keyCode == KEY_ARROW_DOWN) {
            if (gInventorySlotsCount + _stack_offset[_curr_stack] < _pud->length) {
                _stack_offset[_curr_stack] += 1;
                _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_NORMAL);
            }
        } else if (keyCode == KEY_PAGE_DOWN) {
            int v12 = gInventorySlotsCount + _stack_offset[_curr_stack];
            int v13 = v12 + gInventorySlotsCount;
            _stack_offset[_curr_stack] = v12;
            int v14 = _pud->length;
            if (v13 >= _pud->length) {
                int v15 = v14 - gInventorySlotsCount;
                _stack_offset[_curr_stack] = v14 - gInventorySlotsCount;
                if (v15 < 0) {
                    _stack_offset[_curr_stack] = 0;
                }
            }
            _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_NORMAL);
        } else if (keyCode == 2500) {
            _container_exit(keyCode, INVENTORY_WINDOW_TYPE_NORMAL);
        } else {
            if ((mouseGetEvent() & MOUSE_EVENT_RIGHT_BUTTON_DOWN) != 0) {
                if (gInventoryCursor == INVENTORY_WINDOW_CURSOR_HAND) {
                    inventorySetCursor(INVENTORY_WINDOW_CURSOR_ARROW);
                } else if (gInventoryCursor == INVENTORY_WINDOW_CURSOR_ARROW) {
                    inventorySetCursor(INVENTORY_WINDOW_CURSOR_HAND);
                    inventoryRenderSummary();
                    windowRefresh(gInventoryWindow);
                }
            } else if ((mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0) {
                if (keyCode >= 1000 && keyCode <= 1008) {
                    if (gInventoryCursor == INVENTORY_WINDOW_CURSOR_ARROW) {
                        inventoryWindowOpenContextMenu(keyCode, INVENTORY_WINDOW_TYPE_NORMAL);
                    } else {
                        _inven_pickup(keyCode, _stack_offset[_curr_stack]);
                    }
                }
            } else if ((mouseGetEvent() & MOUSE_EVENT_WHEEL) != 0) {
                if (mouseHitTestInWindow(gInventoryWindow, INVENTORY_SCROLLER_X, INVENTORY_SCROLLER_Y, INVENTORY_SCROLLER_MAX_X, INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + INVENTORY_SCROLLER_Y)) {
                    int wheelX;
                    int wheelY;
                    mouseGetWheel(&wheelX, &wheelY);
                    if (wheelY > 0) {
                        if (_stack_offset[_curr_stack] > 0) {
                            _stack_offset[_curr_stack] -= 1;
                            _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_NORMAL);
                        }
                    } else if (wheelY < 0) {
                        if (gInventorySlotsCount + _stack_offset[_curr_stack] < _pud->length) {
                            _stack_offset[_curr_stack] += 1;
                            _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_NORMAL);
                        }
                    }
                }
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    normal_inventory_open = false;

    _inven_dude = _stack[0];
    _adjust_fid();

    if (_inven_dude == gDude) {
        Rect rect;
        objectSetFid(_inven_dude, gInventoryWindowDudeFid, &rect);
        tileWindowRefreshRect(&rect, _inven_dude->elevation);
    }

    Object* newArmor = critterGetArmor(_inven_dude);
    if (_inven_dude == gDude) {
        if (oldArmor != newArmor) {
            interfaceRenderArmorClass(true);
        }
    }

    _exit_inventory(isoWasEnabled);

    // NOTE: Uninline.
    inventoryCommonFree();

    if (_inven_dude == gDude) {
        interfaceUpdateItems(false, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
    }
}

// 0x46EC90
static bool _setup_inventory(int inventoryWindowType)
{
    _dropped_explosive = 0;
    _curr_stack = 0;
    _stack_offset[0] = 0;
    gInventorySlotsCount = 6;
    _pud = &(_inven_dude->data.inventory);
    _stack[0] = _inven_dude;

    if (inventoryWindowType <= INVENTORY_WINDOW_TYPE_LOOT) {
        const InventoryWindowDescription* windowDescription = &(gInventoryWindowDescriptions[inventoryWindowType]);

        // Maintain original position in original resolution, otherwise center it.
        int inventoryWindowX = screenGetWidth() != 640
            ? (screenGetWidth() - windowDescription->width) / 2
            : INVENTORY_WINDOW_X;
        int inventoryWindowY = screenGetHeight() != 480
            ? (screenGetHeight() - windowDescription->height) / 2
            : INVENTORY_WINDOW_Y;
        gInventoryWindow = windowCreate(inventoryWindowX,
            inventoryWindowY,
            windowDescription->width,
            windowDescription->height,
            257,
            WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
        gInventoryWindowMaxX = windowDescription->width + inventoryWindowX;
        gInventoryWindowMaxY = windowDescription->height + inventoryWindowY;

        unsigned char* dest = windowGetBuffer(gInventoryWindow);

        FrmImage backgroundFrmImage;
        int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, windowDescription->frmId, 0, 0, 0);
        if (backgroundFrmImage.lock(backgroundFid)) {
            blitBufferToBuffer(backgroundFrmImage.getData(), windowDescription->width, windowDescription->height, windowDescription->width, dest, windowDescription->width);
        }

        gInventoryPrintItemDescriptionHandler = displayMonitorAddMessage;
    } else if (inventoryWindowType == INVENTORY_WINDOW_TYPE_TRADE) {
        if (_barter_back_win == -1) {
            exit(1);
        }

        gInventorySlotsCount = 3;

        // Trade inventory window is a part of game dialog, which is 640x480.
        int tradeWindowX = (screenGetWidth() - INVENTORY_TRADE_BACKGROUND_WINDOW_WIDTH) / 2 + INVENTORY_TRADE_WINDOW_X;
        int tradeWindowY = (screenGetHeight() - INVENTORY_TRADE_BACKGROUND_WINDOW_HEIGHT) / 2 + INVENTORY_TRADE_WINDOW_Y;
        gInventoryWindow = windowCreate(tradeWindowX, tradeWindowY, INVENTORY_TRADE_WINDOW_WIDTH, INVENTORY_TRADE_WINDOW_HEIGHT, 257, 0);
        gInventoryWindowMaxX = tradeWindowX + INVENTORY_TRADE_WINDOW_WIDTH;
        gInventoryWindowMaxY = tradeWindowY + INVENTORY_TRADE_WINDOW_HEIGHT;

        unsigned char* dest = windowGetBuffer(gInventoryWindow);
        unsigned char* src = windowGetBuffer(_barter_back_win);
        blitBufferToBuffer(src + INVENTORY_TRADE_WINDOW_X, INVENTORY_TRADE_WINDOW_WIDTH, INVENTORY_TRADE_WINDOW_HEIGHT, INVENTORY_TRADE_BACKGROUND_WINDOW_WIDTH, dest, INVENTORY_TRADE_WINDOW_WIDTH);

        gInventoryPrintItemDescriptionHandler = gameDialogRenderSupplementaryMessage;
    }

    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
        // Create invsibile buttons representing character's inventory item
        // slots.
        for (int index = 0; index < gInventorySlotsCount; index++) {
            int btn = buttonCreate(gInventoryWindow,
                INVENTORY_LOOT_LEFT_SCROLLER_X,
                INVENTORY_SLOT_HEIGHT * (gInventorySlotsCount - index - 1) + INVENTORY_LOOT_LEFT_SCROLLER_Y,
                INVENTORY_SLOT_WIDTH,
                INVENTORY_SLOT_HEIGHT,
                999 + gInventorySlotsCount - index,
                -1,
                999 + gInventorySlotsCount - index,
                -1,
                nullptr,
                nullptr,
                nullptr,
                0);
            if (btn != -1) {
                buttonSetMouseCallbacks(btn, inventoryItemSlotOnMouseEnter, inventoryItemSlotOnMouseExit, nullptr, nullptr);
            }
        }

        int eventCode = 2005;
        int y = INVENTORY_SLOT_HEIGHT * 5 + INVENTORY_LOOT_LEFT_SCROLLER_Y;

        // Create invisible buttons representing container's inventory item
        // slots. For unknown reason it loops backwards and it's size is
        // hardcoded at 6 items.
        //
        // Original code is slightly different. It loops until y reaches -11,
        // which is a bit awkward for a loop. Probably result of some
        // optimization.
        for (int index = 0; index < 6; index++) {
            int btn = buttonCreate(gInventoryWindow,
                INVENTORY_LOOT_RIGHT_SCROLLER_X,
                y,
                INVENTORY_SLOT_WIDTH,
                INVENTORY_SLOT_HEIGHT,
                eventCode,
                -1,
                eventCode,
                -1,
                nullptr,
                nullptr,
                nullptr,
                0);
            if (btn != -1) {
                buttonSetMouseCallbacks(btn, inventoryItemSlotOnMouseEnter, inventoryItemSlotOnMouseExit, nullptr, nullptr);
            }

            eventCode -= 1;
            y -= INVENTORY_SLOT_HEIGHT;
        }
    } else if (inventoryWindowType == INVENTORY_WINDOW_TYPE_TRADE) {
        int y1 = INVENTORY_TRADE_SCROLLER_Y;
        int y2 = INVENTORY_TRADE_INNER_SCROLLER_Y;

        for (int index = 0; index < gInventorySlotsCount; index++) {
            int btn;

            // Invsibile button representing left inventory slot.
            btn = buttonCreate(gInventoryWindow,
                INVENTORY_TRADE_LEFT_SCROLLER_X,
                y1,
                INVENTORY_SLOT_WIDTH,
                INVENTORY_SLOT_HEIGHT,
                1000 + index,
                -1,
                1000 + index,
                -1,
                nullptr,
                nullptr,
                nullptr,
                0);
            if (btn != -1) {
                buttonSetMouseCallbacks(btn, inventoryItemSlotOnMouseEnter, inventoryItemSlotOnMouseExit, nullptr, nullptr);
            }

            // Invisible button representing right inventory slot.
            btn = buttonCreate(gInventoryWindow,
                INVENTORY_TRADE_RIGHT_SCROLLER_X,
                y1,
                INVENTORY_SLOT_WIDTH,
                INVENTORY_SLOT_HEIGHT,
                2000 + index,
                -1,
                2000 + index,
                -1,
                nullptr,
                nullptr,
                nullptr,
                0);
            if (btn != -1) {
                buttonSetMouseCallbacks(btn, inventoryItemSlotOnMouseEnter, inventoryItemSlotOnMouseExit, nullptr, nullptr);
            }

            // Invisible button representing left suggested slot.
            btn = buttonCreate(gInventoryWindow,
                INVENTORY_TRADE_INNER_LEFT_SCROLLER_X,
                y2,
                INVENTORY_SLOT_WIDTH,
                INVENTORY_SLOT_HEIGHT,
                2300 + index,
                -1,
                2300 + index,
                -1,
                nullptr,
                nullptr,
                nullptr,
                0);
            if (btn != -1) {
                buttonSetMouseCallbacks(btn, inventoryItemSlotOnMouseEnter, inventoryItemSlotOnMouseExit, nullptr, nullptr);
            }

            // Invisible button representing right suggested slot.
            btn = buttonCreate(gInventoryWindow,
                INVENTORY_TRADE_INNER_RIGHT_SCROLLER_X,
                y2,
                INVENTORY_SLOT_WIDTH,
                INVENTORY_SLOT_HEIGHT,
                2400 + index,
                -1,
                2400 + index,
                -1,
                nullptr,
                nullptr,
                nullptr,
                0);
            if (btn != -1) {
                buttonSetMouseCallbacks(btn, inventoryItemSlotOnMouseEnter, inventoryItemSlotOnMouseExit, nullptr, nullptr);
            }

            y1 += INVENTORY_SLOT_HEIGHT;
            y2 += INVENTORY_SLOT_HEIGHT;
        }
    } else {
        // Create invisible buttons representing item slots.
        for (int index = 0; index < gInventorySlotsCount; index++) {
            int btn = buttonCreate(gInventoryWindow,
                INVENTORY_SCROLLER_X,
                INVENTORY_SLOT_HEIGHT * (gInventorySlotsCount - index - 1) + INVENTORY_SCROLLER_Y,
                INVENTORY_SLOT_WIDTH,
                INVENTORY_SLOT_HEIGHT,
                999 + gInventorySlotsCount - index,
                -1,
                999 + gInventorySlotsCount - index,
                -1,
                nullptr,
                nullptr,
                nullptr,
                0);
            if (btn != -1) {
                buttonSetMouseCallbacks(btn, inventoryItemSlotOnMouseEnter, inventoryItemSlotOnMouseExit, nullptr, nullptr);
            }
        }
    }

    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_NORMAL) {
        int btn;

        // Item2 slot
        btn = buttonCreate(gInventoryWindow,
            INVENTORY_RIGHT_HAND_SLOT_X,
            INVENTORY_RIGHT_HAND_SLOT_Y,
            INVENTORY_LARGE_SLOT_WIDTH,
            INVENTORY_LARGE_SLOT_HEIGHT,
            1006,
            -1,
            1006,
            -1,
            nullptr,
            nullptr,
            nullptr,
            0);
        if (btn != -1) {
            buttonSetMouseCallbacks(btn, inventoryItemSlotOnMouseEnter, inventoryItemSlotOnMouseExit, nullptr, nullptr);
        }

        // Item1 slot
        btn = buttonCreate(gInventoryWindow,
            INVENTORY_LEFT_HAND_SLOT_X,
            INVENTORY_LEFT_HAND_SLOT_Y,
            INVENTORY_LARGE_SLOT_WIDTH,
            INVENTORY_LARGE_SLOT_HEIGHT,
            1007,
            -1,
            1007,
            -1,
            nullptr,
            nullptr,
            nullptr,
            0);
        if (btn != -1) {
            buttonSetMouseCallbacks(btn, inventoryItemSlotOnMouseEnter, inventoryItemSlotOnMouseExit, nullptr, nullptr);
        }

        // Armor slot
        btn = buttonCreate(gInventoryWindow,
            INVENTORY_ARMOR_SLOT_X,
            INVENTORY_ARMOR_SLOT_Y,
            INVENTORY_LARGE_SLOT_WIDTH,
            INVENTORY_LARGE_SLOT_HEIGHT,
            1008,
            -1,
            1008,
            -1,
            nullptr,
            nullptr,
            nullptr,
            0);
        if (btn != -1) {
            buttonSetMouseCallbacks(btn, inventoryItemSlotOnMouseEnter, inventoryItemSlotOnMouseExit, nullptr, nullptr);
        }
    }

    int fid;
    int btn;

    fid = buildFid(OBJ_TYPE_INTERFACE, 8, 0, 0, 0);
    _inventoryFrmImages[0].lock(fid);

    fid = buildFid(OBJ_TYPE_INTERFACE, 9, 0, 0, 0);
    _inventoryFrmImages[1].lock(fid);

    if (_inventoryFrmImages[0].isLocked() && _inventoryFrmImages[1].isLocked()) {
        btn = -1;
        switch (inventoryWindowType) {
        case INVENTORY_WINDOW_TYPE_NORMAL:
            // Done button
            btn = buttonCreate(gInventoryWindow,
                437,
                329,
                15,
                16,
                -1,
                -1,
                -1,
                KEY_ESCAPE,
                _inventoryFrmImages[0].getData(),
                _inventoryFrmImages[1].getData(),
                nullptr,
                BUTTON_FLAG_TRANSPARENT);
            break;
        case INVENTORY_WINDOW_TYPE_USE_ITEM_ON:
            // Cancel button
            btn = buttonCreate(gInventoryWindow,
                233,
                328,
                15,
                16,
                -1,
                -1,
                -1,
                KEY_ESCAPE,
                _inventoryFrmImages[0].getData(),
                _inventoryFrmImages[1].getData(),
                nullptr,
                BUTTON_FLAG_TRANSPARENT);
            break;
        case INVENTORY_WINDOW_TYPE_LOOT:
            // Done button
            btn = buttonCreate(gInventoryWindow,
                476,
                331,
                15,
                16,
                -1,
                -1,
                -1,
                KEY_ESCAPE,
                _inventoryFrmImages[0].getData(),
                _inventoryFrmImages[1].getData(),
                nullptr,
                BUTTON_FLAG_TRANSPARENT);
            break;
        }

        if (btn != -1) {
            buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
        }
    }

    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_TRADE) {
        // Large arrow up (normal).
        fid = buildFid(OBJ_TYPE_INTERFACE, 100, 0, 0, 0);
        _inventoryFrmImages[2].lock(fid);

        // Large arrow up (pressed).
        fid = buildFid(OBJ_TYPE_INTERFACE, 101, 0, 0, 0);
        _inventoryFrmImages[3].lock(fid);

        if (_inventoryFrmImages[2].isLocked() && _inventoryFrmImages[3].isLocked()) {
            // Left inventory up button.
            btn = buttonCreate(gInventoryWindow,
                109,
                56,
                23,
                24,
                -1,
                -1,
                KEY_ARROW_UP,
                -1,
                _inventoryFrmImages[2].getData(),
                _inventoryFrmImages[3].getData(),
                nullptr,
                0);
            if (btn != -1) {
                buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
            }

            // Right inventory up button.
            btn = buttonCreate(gInventoryWindow,
                342,
                56,
                23,
                24,
                -1,
                -1,
                KEY_CTRL_ARROW_UP,
                -1,
                _inventoryFrmImages[2].getData(),
                _inventoryFrmImages[3].getData(),
                nullptr,
                0);
            if (btn != -1) {
                buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
            }
        }
    } else {
        // Large up arrow (normal).
        fid = buildFid(OBJ_TYPE_INTERFACE, 49, 0, 0, 0);
        _inventoryFrmImages[2].lock(fid);

        // Large up arrow (pressed).
        fid = buildFid(OBJ_TYPE_INTERFACE, 50, 0, 0, 0);
        _inventoryFrmImages[3].lock(fid);

        // Large up arrow (disabled).
        fid = buildFid(OBJ_TYPE_INTERFACE, 53, 0, 0, 0);
        _inventoryFrmImages[4].lock(fid);

        if (_inventoryFrmImages[2].isLocked() && _inventoryFrmImages[3].isLocked() && _inventoryFrmImages[4].isLocked()) {
            if (inventoryWindowType != INVENTORY_WINDOW_TYPE_TRADE) {
                // Left inventory up button.
                gInventoryScrollUpButton = buttonCreate(gInventoryWindow,
                    128,
                    39,
                    22,
                    23,
                    -1,
                    -1,
                    KEY_ARROW_UP,
                    -1,
                    _inventoryFrmImages[2].getData(),
                    _inventoryFrmImages[3].getData(),
                    nullptr,
                    0);
                if (gInventoryScrollUpButton != -1) {
                    _win_register_button_disable(gInventoryScrollUpButton, _inventoryFrmImages[4].getData(), _inventoryFrmImages[4].getData(), _inventoryFrmImages[4].getData());
                    buttonSetCallbacks(gInventoryScrollUpButton, _gsound_red_butt_press, _gsound_red_butt_release);
                    buttonDisable(gInventoryScrollUpButton);
                }
            }

            if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
                // Right inventory up button.
                gSecondaryInventoryScrollUpButton = buttonCreate(gInventoryWindow,
                    379,
                    39,
                    22,
                    23,
                    -1,
                    -1,
                    KEY_CTRL_ARROW_UP,
                    -1,
                    _inventoryFrmImages[2].getData(),
                    _inventoryFrmImages[3].getData(),
                    nullptr,
                    0);
                if (gSecondaryInventoryScrollUpButton != -1) {
                    _win_register_button_disable(gSecondaryInventoryScrollUpButton, _inventoryFrmImages[4].getData(), _inventoryFrmImages[4].getData(), _inventoryFrmImages[4].getData());
                    buttonSetCallbacks(gSecondaryInventoryScrollUpButton, _gsound_red_butt_press, _gsound_red_butt_release);
                    buttonDisable(gSecondaryInventoryScrollUpButton);
                }
            }
        }
    }

    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_TRADE) {
        // Large dialog down button (normal)
        fid = buildFid(OBJ_TYPE_INTERFACE, 93, 0, 0, 0);
        _inventoryFrmImages[5].lock(fid);

        // Dialog down button (pressed)
        fid = buildFid(OBJ_TYPE_INTERFACE, 94, 0, 0, 0);
        _inventoryFrmImages[6].lock(fid);

        if (_inventoryFrmImages[5].isLocked() && _inventoryFrmImages[6].isLocked()) {
            // Left inventory down button.
            btn = buttonCreate(gInventoryWindow,
                109,
                82,
                24,
                25,
                -1,
                -1,
                KEY_ARROW_DOWN,
                -1,
                _inventoryFrmImages[5].getData(),
                _inventoryFrmImages[6].getData(),
                nullptr,
                0);
            if (btn != -1) {
                buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
            }

            // Right inventory down button
            btn = buttonCreate(gInventoryWindow,
                342,
                82,
                24,
                25,
                -1,
                -1,
                KEY_CTRL_ARROW_DOWN,
                -1,
                _inventoryFrmImages[5].getData(),
                _inventoryFrmImages[6].getData(),
                nullptr,
                0);
            if (btn != -1) {
                buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
            }

            // Invisible button representing left character.
            buttonCreate(_barter_back_win,
                15,
                25,
                INVENTORY_BODY_VIEW_WIDTH,
                INVENTORY_BODY_VIEW_HEIGHT,
                -1,
                -1,
                2500,
                -1,
                nullptr,
                nullptr,
                nullptr,
                0);

            // Invisible button representing right character.
            buttonCreate(_barter_back_win,
                560,
                25,
                INVENTORY_BODY_VIEW_WIDTH,
                INVENTORY_BODY_VIEW_HEIGHT,
                -1,
                -1,
                2501,
                -1,
                nullptr,
                nullptr,
                nullptr,
                0);
        }
    } else {
        // Large arrow down (normal).
        fid = buildFid(OBJ_TYPE_INTERFACE, 51, 0, 0, 0);
        _inventoryFrmImages[5].lock(fid);

        // Large arrow down (pressed).
        fid = buildFid(OBJ_TYPE_INTERFACE, 52, 0, 0, 0);
        _inventoryFrmImages[6].lock(fid);

        // Large arrow down (disabled).
        fid = buildFid(OBJ_TYPE_INTERFACE, 54, 0, 0, 0);
        _inventoryFrmImages[7].lock(fid);

        if (_inventoryFrmImages[5].isLocked() && _inventoryFrmImages[6].isLocked() && _inventoryFrmImages[7].isLocked()) {
            // Left inventory down button.
            gInventoryScrollDownButton = buttonCreate(gInventoryWindow,
                128,
                62,
                22,
                23,
                -1,
                -1,
                KEY_ARROW_DOWN,
                -1,
                _inventoryFrmImages[5].getData(),
                _inventoryFrmImages[6].getData(),
                nullptr,
                0);
            buttonSetCallbacks(gInventoryScrollDownButton, _gsound_red_butt_press, _gsound_red_butt_release);
            _win_register_button_disable(gInventoryScrollDownButton, _inventoryFrmImages[7].getData(), _inventoryFrmImages[7].getData(), _inventoryFrmImages[7].getData());
            buttonDisable(gInventoryScrollDownButton);

            if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
                // Invisible button representing left character.
                buttonCreate(gInventoryWindow,
                    INVENTORY_LOOT_LEFT_BODY_VIEW_X,
                    INVENTORY_LOOT_LEFT_BODY_VIEW_Y,
                    INVENTORY_BODY_VIEW_WIDTH,
                    INVENTORY_BODY_VIEW_HEIGHT,
                    -1,
                    -1,
                    2500,
                    -1,
                    nullptr,
                    nullptr,
                    nullptr,
                    0);

                // Right inventory down button.
                gSecondaryInventoryScrollDownButton = buttonCreate(gInventoryWindow,
                    379,
                    62,
                    22,
                    23,
                    -1,
                    -1,
                    KEY_CTRL_ARROW_DOWN,
                    -1,
                    _inventoryFrmImages[5].getData(),
                    _inventoryFrmImages[6].getData(),
                    nullptr,
                    0);
                if (gSecondaryInventoryScrollDownButton != -1) {
                    buttonSetCallbacks(gSecondaryInventoryScrollDownButton, _gsound_red_butt_press, _gsound_red_butt_release);
                    _win_register_button_disable(gSecondaryInventoryScrollDownButton, _inventoryFrmImages[7].getData(), _inventoryFrmImages[7].getData(), _inventoryFrmImages[7].getData());
                    buttonDisable(gSecondaryInventoryScrollDownButton);
                }

                // Invisible button representing right character.
                buttonCreate(gInventoryWindow,
                    INVENTORY_LOOT_RIGHT_BODY_VIEW_X,
                    INVENTORY_LOOT_RIGHT_BODY_VIEW_Y,
                    INVENTORY_BODY_VIEW_WIDTH,
                    INVENTORY_BODY_VIEW_HEIGHT,
                    -1,
                    -1,
                    2501,
                    -1,
                    nullptr,
                    nullptr,
                    nullptr,
                    0);
            } else {
                // Invisible button representing character (in inventory and use on dialogs).
                buttonCreate(gInventoryWindow,
                    INVENTORY_PC_BODY_VIEW_X,
                    INVENTORY_PC_BODY_VIEW_Y,
                    INVENTORY_BODY_VIEW_WIDTH,
                    INVENTORY_BODY_VIEW_HEIGHT,
                    -1,
                    -1,
                    2500,
                    -1,
                    nullptr,
                    nullptr,
                    nullptr,
                    0);
            }
        }
    }

    if (inventoryWindowType != INVENTORY_WINDOW_TYPE_TRADE) {
        if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
            if (!_gIsSteal) {
                // Take all button (normal)
                fid = buildFid(OBJ_TYPE_INTERFACE, 436, 0, 0, 0);
                _inventoryFrmImages[8].lock(fid);

                // Take all button (pressed)
                fid = buildFid(OBJ_TYPE_INTERFACE, 437, 0, 0, 0);
                _inventoryFrmImages[9].lock(fid);

                if (_inventoryFrmImages[8].isLocked() && _inventoryFrmImages[9].isLocked()) {
                    // Take all button.
                    btn = buttonCreate(gInventoryWindow,
                        432,
                        204,
                        39,
                        41,
                        -1,
                        -1,
                        KEY_UPPERCASE_A,
                        -1,
                        _inventoryFrmImages[8].getData(),
                        _inventoryFrmImages[9].getData(),
                        nullptr,
                        0);
                    if (btn != -1) {
                        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
                    }
                }
            }
        }
    } else {
        // Inventory button up (normal)
        fid = buildFid(OBJ_TYPE_INTERFACE, 49, 0, 0, 0);
        _inventoryFrmImages[8].lock(fid);

        // Inventory button up (pressed)
        fid = buildFid(OBJ_TYPE_INTERFACE, 50, 0, 0, 0);
        _inventoryFrmImages[9].lock(fid);

        if (_inventoryFrmImages[8].isLocked() && _inventoryFrmImages[9].isLocked()) {
            // Left offered inventory up button.
            btn = buttonCreate(gInventoryWindow,
                128,
                113,
                22,
                23,
                -1,
                -1,
                KEY_PAGE_UP,
                -1,
                _inventoryFrmImages[8].getData(),
                _inventoryFrmImages[9].getData(),
                nullptr,
                0);
            if (btn != -1) {
                buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
            }

            // Right offered inventory up button.
            btn = buttonCreate(gInventoryWindow,
                333,
                113,
                22,
                23,
                -1,
                -1,
                KEY_CTRL_PAGE_UP,
                -1,
                _inventoryFrmImages[8].getData(),
                _inventoryFrmImages[9].getData(),
                nullptr,
                0);
            if (btn != -1) {
                buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
            }
        }

        // Inventory button down (normal)
        fid = buildFid(OBJ_TYPE_INTERFACE, 51, 0, 0, 0);
        _inventoryFrmImages[10].lock(fid);

        // Inventory button down (pressed).
        fid = buildFid(OBJ_TYPE_INTERFACE, 52, 0, 0, 0);
        _inventoryFrmImages[11].lock(fid);

        if (_inventoryFrmImages[10].isLocked() && _inventoryFrmImages[11].isLocked()) {
            // Left offered inventory down button.
            btn = buttonCreate(gInventoryWindow,
                128,
                136,
                22,
                23,
                -1,
                -1,
                KEY_PAGE_DOWN,
                -1,
                _inventoryFrmImages[10].getData(),
                _inventoryFrmImages[11].getData(),
                nullptr,
                0);
            if (btn != -1) {
                buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
            }

            // Right offered inventory down button.
            btn = buttonCreate(gInventoryWindow,
                333,
                136,
                22,
                23,
                -1,
                -1,
                KEY_CTRL_PAGE_DOWN,
                -1,
                _inventoryFrmImages[10].getData(),
                _inventoryFrmImages[11].getData(),
                nullptr,
                0);
            if (btn != -1) {
                buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
            }
        }
    }

    gInventoryRightHandItem = nullptr;
    gInventoryArmor = nullptr;
    gInventoryLeftHandItem = nullptr;

    for (int index = 0; index < _pud->length; index++) {
        InventoryItem* inventoryItem = &(_pud->items[index]);
        Object* item = inventoryItem->item;
        if ((item->flags & OBJECT_IN_LEFT_HAND) != 0) {
            if ((item->flags & OBJECT_IN_RIGHT_HAND) != 0) {
                gInventoryRightHandItem = item;
            }
            gInventoryLeftHandItem = item;
        } else if ((item->flags & OBJECT_IN_RIGHT_HAND) != 0) {
            gInventoryRightHandItem = item;
        } else if ((item->flags & OBJECT_WORN) != 0) {
            gInventoryArmor = item;
        }
    }

    if (gInventoryLeftHandItem != nullptr) {
        itemRemove(_inven_dude, gInventoryLeftHandItem, 1);
    }

    if (gInventoryRightHandItem != nullptr && gInventoryRightHandItem != gInventoryLeftHandItem) {
        itemRemove(_inven_dude, gInventoryRightHandItem, 1);
    }

    if (gInventoryArmor != nullptr) {
        itemRemove(_inven_dude, gInventoryArmor, 1);
    }

    _adjust_fid();

    bool isoWasEnabled = isoDisable();

    _gmouse_disable(0);

    return isoWasEnabled;
}

// 0x46FBD8
static void _exit_inventory(bool shouldEnableIso)
{
    _inven_dude = _stack[0];

    if (gInventoryLeftHandItem != nullptr) {
        gInventoryLeftHandItem->flags |= OBJECT_IN_LEFT_HAND;
        if (gInventoryLeftHandItem == gInventoryRightHandItem) {
            gInventoryLeftHandItem->flags |= OBJECT_IN_RIGHT_HAND;
        }

        itemAdd(_inven_dude, gInventoryLeftHandItem, 1);
    }

    if (gInventoryRightHandItem != nullptr && gInventoryRightHandItem != gInventoryLeftHandItem) {
        gInventoryRightHandItem->flags |= OBJECT_IN_RIGHT_HAND;
        itemAdd(_inven_dude, gInventoryRightHandItem, 1);
    }

    if (gInventoryArmor != nullptr) {
        gInventoryArmor->flags |= OBJECT_WORN;
        itemAdd(_inven_dude, gInventoryArmor, 1);
    }

    gInventoryRightHandItem = nullptr;
    gInventoryArmor = nullptr;
    gInventoryLeftHandItem = nullptr;

    for (int index = 0; index < INVENTORY_FRM_COUNT; index++) {
        _inventoryFrmImages[index].unlock();
    }

    if (shouldEnableIso) {
        isoEnable();
    }

    windowDestroy(gInventoryWindow);

    _gmouse_enable();

    if (_dropped_explosive) {
        inven_explosive_dropped_notice();
        _dropped_explosive = false;
    }

    // ⚠️ THE BUMP LIVES HERE, NOT AT THE CALL SITES (fallout1-ce). Every inventory-family modal screen
    // runs its own input loop while the pad's button edges sit in statics, so the button that closed
    // the screen is still DOWN when gameplay resumes and its edge fires there (closing loot with B
    // cycled the firing mode). Every inventory screen passes through here exactly once on the way out.
    gamepad_ui_bump_input_epoch();
}

// Vanilla's answer to an armed explosive put down in front of people -- it runs as the inventory
// closes after one was dropped: everyone of another team inside the blast who notices it takes it as
// an attack by the dude. Out here so the pad's quick menu "put it down" answers the same way.
void inven_explosive_dropped_notice()
{
    Attack attack;
    attackInit(&attack, gDude, nullptr, HIT_MODE_PUNCH, HIT_LOCATION_TORSO);
    attack.attackerFlags = DAM_HIT;
    attack.tile = gDude->tile;
    _compute_explosion_on_extras(&attack, 0, 0, 1);

    Object* watcher = nullptr;
    for (int index = 0; index < attack.extrasLength; index++) {
        Object* critter = attack.extras[index];
        if (critter != gDude
            && critter->data.critter.combat.team != gDude->data.critter.combat.team
            && statRoll(critter, STAT_PERCEPTION, 0, nullptr) >= ROLL_SUCCESS) {
            _critter_set_who_hit_me(critter, gDude);

            if (watcher == nullptr) {
                watcher = critter;
            }
        }
    }

    if (watcher != nullptr) {
        if (!isInCombat()) {
            CombatStartData combat;
            combat.attacker = watcher;
            combat.defender = gDude;
            combat.actionPointsBonus = 0;
            combat.accuracyBonus = 0;
            combat.damageBonus = 0;
            combat.minDamage = 0;
            combat.maxDamage = INT_MAX;
            combat.overrideAttackResults = 0;
            scriptsRequestCombat(&combat);
        }
    }

}

// 0x46FDF4
static void _display_inventory(int stackOffset, int dragSlotIndex, int inventoryWindowType)
{
    unsigned char* windowBuffer = windowGetBuffer(gInventoryWindow);
    int pitch;

    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_NORMAL) {
        pitch = INVENTORY_WINDOW_WIDTH;

        FrmImage backgroundFrmImage;
        int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, 48, 0, 0, 0);
        if (backgroundFrmImage.lock(backgroundFid)) {
            // Clear scroll view background.
            blitBufferToBuffer(backgroundFrmImage.getData() + pitch * INVENTORY_SCROLLER_Y + INVENTORY_SCROLLER_X,
                INVENTORY_SLOT_WIDTH,
                gInventorySlotsCount * INVENTORY_SLOT_HEIGHT,
                pitch,
                windowBuffer + pitch * INVENTORY_SCROLLER_Y + INVENTORY_SCROLLER_X,
                pitch);

            // Clear armor button background.
            blitBufferToBuffer(backgroundFrmImage.getData() + pitch * INVENTORY_ARMOR_SLOT_Y + INVENTORY_ARMOR_SLOT_X,
                INVENTORY_LARGE_SLOT_WIDTH,
                INVENTORY_LARGE_SLOT_HEIGHT,
                pitch,
                windowBuffer + pitch * INVENTORY_ARMOR_SLOT_Y + INVENTORY_ARMOR_SLOT_X,
                pitch);

            if (gInventoryLeftHandItem != nullptr && gInventoryLeftHandItem == gInventoryRightHandItem) {
                // Clear item1.
                FrmImage itemBackgroundFrmImage;
                int itemBackgroundFid = buildFid(OBJ_TYPE_INTERFACE, 32, 0, 0, 0);
                if (itemBackgroundFrmImage.lock(itemBackgroundFid)) {
                    unsigned char* data = itemBackgroundFrmImage.getData();
                    int width = itemBackgroundFrmImage.getWidth();
                    int height = itemBackgroundFrmImage.getHeight();
                    blitBufferToBuffer(data, width, height, width, windowBuffer + pitch * 284 + 152, pitch);
                }
            } else {
                // Clear both items in one go.
                blitBufferToBuffer(backgroundFrmImage.getData() + pitch * INVENTORY_LEFT_HAND_SLOT_Y + INVENTORY_LEFT_HAND_SLOT_X,
                    INVENTORY_LARGE_SLOT_WIDTH * 2,
                    INVENTORY_LARGE_SLOT_HEIGHT,
                    pitch,
                    windowBuffer + pitch * INVENTORY_LEFT_HAND_SLOT_Y + INVENTORY_LEFT_HAND_SLOT_X,
                    pitch);
            }
        }
    } else if (inventoryWindowType == INVENTORY_WINDOW_TYPE_USE_ITEM_ON) {
        pitch = INVENTORY_USE_ON_WINDOW_WIDTH;

        FrmImage backgroundFrmImage;
        int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, 113, 0, 0, 0);
        if (backgroundFrmImage.lock(backgroundFid)) {
            // Clear scroll view background.
            blitBufferToBuffer(backgroundFrmImage.getData() + pitch * INVENTORY_SCROLLER_Y + INVENTORY_SCROLLER_X,
                INVENTORY_SLOT_WIDTH,
                gInventorySlotsCount * INVENTORY_SLOT_HEIGHT,
                pitch,
                windowBuffer + pitch * INVENTORY_SCROLLER_Y + INVENTORY_SCROLLER_X,
                pitch);
        }
    } else if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
        pitch = INVENTORY_LOOT_WINDOW_WIDTH;

        FrmImage backgroundFrmImage;
        int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, 114, 0, 0, 0);
        if (backgroundFrmImage.lock(backgroundFid)) {
            // Clear scroll view background.
            blitBufferToBuffer(backgroundFrmImage.getData() + pitch * INVENTORY_LOOT_LEFT_SCROLLER_Y + INVENTORY_LOOT_LEFT_SCROLLER_X,
                INVENTORY_SLOT_WIDTH,
                gInventorySlotsCount * INVENTORY_SLOT_HEIGHT,
                pitch,
                windowBuffer + pitch * INVENTORY_LOOT_LEFT_SCROLLER_Y + INVENTORY_LOOT_LEFT_SCROLLER_X,
                pitch);
        }
    } else if (inventoryWindowType == INVENTORY_WINDOW_TYPE_TRADE) {
        pitch = INVENTORY_TRADE_WINDOW_WIDTH;

        windowBuffer = windowGetBuffer(gInventoryWindow);

        blitBufferToBuffer(windowGetBuffer(_barter_back_win) + INVENTORY_TRADE_LEFT_SCROLLER_Y * INVENTORY_TRADE_BACKGROUND_WINDOW_WIDTH + INVENTORY_TRADE_LEFT_SCROLLER_X + INVENTORY_TRADE_WINDOW_OFFSET, INVENTORY_SLOT_WIDTH, INVENTORY_SLOT_HEIGHT * gInventorySlotsCount, INVENTORY_TRADE_BACKGROUND_WINDOW_WIDTH, windowBuffer + pitch * INVENTORY_TRADE_LEFT_SCROLLER_Y + INVENTORY_TRADE_LEFT_SCROLLER_X, pitch);
    } else {
        assert(false && "Should be unreachable");
    }

    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_NORMAL
        || inventoryWindowType == INVENTORY_WINDOW_TYPE_USE_ITEM_ON
        || inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
        if (gInventoryScrollUpButton != -1) {
            if (stackOffset <= 0) {
                buttonDisable(gInventoryScrollUpButton);
            } else {
                buttonEnable(gInventoryScrollUpButton);
            }
        }

        if (gInventoryScrollDownButton != -1) {
            if (_pud->length - stackOffset <= gInventorySlotsCount) {
                buttonDisable(gInventoryScrollDownButton);
            } else {
                buttonEnable(gInventoryScrollDownButton);
            }
        }
    }

    int y = 0;
    for (int slotIndex = 0; slotIndex + stackOffset < _pud->length && slotIndex < gInventorySlotsCount; slotIndex += 1) {
        int itemIndex = slotIndex + stackOffset + 1;

        int offset;
        if (inventoryWindowType == INVENTORY_WINDOW_TYPE_TRADE) {
            offset = pitch * (y + INVENTORY_TRADE_LEFT_SCROLLER_Y_PAD) + INVENTORY_TRADE_LEFT_SCROLLER_X_PAD;
        } else {
            if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
                offset = pitch * (y + INVENTORY_LOOT_LEFT_SCROLLER_Y_PAD) + INVENTORY_LOOT_LEFT_SCROLLER_X_PAD;
            } else {
                offset = pitch * (y + INVENTORY_SCROLLER_Y_PAD) + INVENTORY_SCROLLER_X_PAD;
            }
        }

        InventoryItem* inventoryItem = &(_pud->items[_pud->length - itemIndex]);

        int inventoryFid = itemGetInventoryFid(inventoryItem->item);
        artRender(inventoryFid, windowBuffer + offset, INVENTORY_SLOT_WIDTH_PAD, INVENTORY_SLOT_HEIGHT_PAD, pitch);

        if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
            offset = pitch * (y + INVENTORY_LOOT_LEFT_SCROLLER_Y_PAD) + INVENTORY_LOOT_LEFT_SCROLLER_X_PAD;
        } else if (inventoryWindowType == INVENTORY_WINDOW_TYPE_TRADE) {
            offset = pitch * (y + INVENTORY_TRADE_LEFT_SCROLLER_Y_PAD) + INVENTORY_TRADE_LEFT_SCROLLER_X_PAD;
        } else {
            offset = pitch * (y + INVENTORY_SCROLLER_Y_PAD) + INVENTORY_SCROLLER_X_PAD;
        }

        _display_inventory_info(inventoryItem->item, inventoryItem->quantity, windowBuffer + offset, pitch, slotIndex == dragSlotIndex);

        y += INVENTORY_SLOT_HEIGHT;
    }

    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_NORMAL) {
        if (gInventoryRightHandItem != nullptr) {
            int width = gInventoryRightHandItem == gInventoryLeftHandItem ? INVENTORY_LARGE_SLOT_WIDTH * 2 : INVENTORY_LARGE_SLOT_WIDTH;
            int inventoryFid = itemGetInventoryFid(gInventoryRightHandItem);
            artRender(inventoryFid, windowBuffer + INVENTORY_WINDOW_WIDTH * INVENTORY_RIGHT_HAND_SLOT_Y + INVENTORY_RIGHT_HAND_SLOT_X, width, INVENTORY_LARGE_SLOT_HEIGHT, INVENTORY_WINDOW_WIDTH);
        }

        if (gInventoryLeftHandItem != nullptr && gInventoryLeftHandItem != gInventoryRightHandItem) {
            int inventoryFid = itemGetInventoryFid(gInventoryLeftHandItem);
            artRender(inventoryFid, windowBuffer + INVENTORY_WINDOW_WIDTH * INVENTORY_LEFT_HAND_SLOT_Y + INVENTORY_LEFT_HAND_SLOT_X, INVENTORY_LARGE_SLOT_WIDTH, INVENTORY_LARGE_SLOT_HEIGHT, INVENTORY_WINDOW_WIDTH);
        }

        if (gInventoryArmor != nullptr) {
            int inventoryFid = itemGetInventoryFid(gInventoryArmor);
            artRender(inventoryFid, windowBuffer + INVENTORY_WINDOW_WIDTH * INVENTORY_ARMOR_SLOT_Y + INVENTORY_ARMOR_SLOT_X, INVENTORY_LARGE_SLOT_WIDTH, INVENTORY_LARGE_SLOT_HEIGHT, INVENTORY_WINDOW_WIDTH);
        }

        // CE: the pad's button hints in each hand box -- A into the LEFT hand, B into the RIGHT.
        // ⚠️ These letters are a PROMISE: inven_gamepad_quick_normal must put an A-tapped item in the A box.
        if (gamepad_ui_get_controller() != nullptr) {
            int oldFont = fontGetCurrent();
            fontSetCurrent(101);

            int hintY = INVENTORY_LEFT_HAND_SLOT_Y + INVENTORY_LARGE_SLOT_HEIGHT - fontGetLineHeight() - 2;

            fontDrawText(windowBuffer + INVENTORY_WINDOW_WIDTH * hintY + (INVENTORY_LEFT_HAND_SLOT_X + 3),
                "A", INVENTORY_LARGE_SLOT_WIDTH, INVENTORY_WINDOW_WIDTH, _colorTable[32747]);
            fontDrawText(windowBuffer + INVENTORY_WINDOW_WIDTH * hintY + (INVENTORY_RIGHT_HAND_SLOT_X + 3),
                "B", INVENTORY_LARGE_SLOT_WIDTH, INVENTORY_WINDOW_WIDTH, _colorTable[32747]);

            fontSetCurrent(oldFont);
        }
    }

    // CE: Show items weight.
    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
        char formattedText[20];

        int oldFont = fontGetCurrent();
        fontSetCurrent(101);

        FrmImage backgroundFrm;
        int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, 114, 0, 0, 0);
        if (backgroundFrm.lock(backgroundFid)) {
            int x = INVENTORY_LOOT_LEFT_SCROLLER_X;
            int y = INVENTORY_LOOT_LEFT_SCROLLER_Y + gInventorySlotsCount * INVENTORY_SLOT_HEIGHT + 2;
            blitBufferToBuffer(backgroundFrm.getData() + pitch * y + x, INVENTORY_SLOT_WIDTH, fontGetLineHeight(), pitch, windowBuffer + pitch * y + x, pitch);
        }

        Object* object = _stack[0];

        int color = _colorTable[992];
        if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
            int carryWeight = critterGetStat(object, STAT_CARRY_WEIGHT);
            int inventoryWeight = objectGetInventoryWeight(object);
            snprintf(formattedText, sizeof(formattedText), "%d/%d", inventoryWeight, carryWeight);

            if (critterIsEncumbered(object)) {
                color = _colorTable[31744];
            }
        } else {
            int inventoryWeight = objectGetInventoryWeight(object);
            snprintf(formattedText, sizeof(formattedText), "%d", inventoryWeight);
        }

        int width = fontGetStringWidth(formattedText);
        int x = INVENTORY_LOOT_LEFT_SCROLLER_X + INVENTORY_SLOT_WIDTH / 2 - width / 2;
        int y = INVENTORY_LOOT_LEFT_SCROLLER_Y + INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + 2;
        fontDrawText(windowBuffer + pitch * y + x, formattedText, width, pitch, color);

        fontSetCurrent(oldFont);
    }

    windowRefresh(gInventoryWindow);
}

// Render inventory item.
//
// [stackOffset] is an index of the first visible item in the scrolling view.
// [dragSlotIndex] is an index of item being dragged (it decreases displayed number of items in inner functions).
//
// 0x47036C
static void _display_target_inventory(int stackOffset, int dragSlotIndex, Inventory* inventory, int inventoryWindowType)
{
    unsigned char* windowBuffer = windowGetBuffer(gInventoryWindow);

    int pitch;
    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
        pitch = INVENTORY_LOOT_WINDOW_WIDTH;

        FrmImage backgroundFrmImage;
        int fid = buildFid(OBJ_TYPE_INTERFACE, 114, 0, 0, 0);
        if (backgroundFrmImage.lock(fid)) {
            blitBufferToBuffer(backgroundFrmImage.getData() + pitch * INVENTORY_LOOT_RIGHT_SCROLLER_Y + INVENTORY_LOOT_RIGHT_SCROLLER_X,
                INVENTORY_SLOT_WIDTH,
                INVENTORY_SLOT_HEIGHT * gInventorySlotsCount,
                pitch,
                windowBuffer + pitch * INVENTORY_LOOT_RIGHT_SCROLLER_Y + INVENTORY_LOOT_RIGHT_SCROLLER_X,
                pitch);
        }
    } else if (inventoryWindowType == INVENTORY_WINDOW_TYPE_TRADE) {
        pitch = INVENTORY_TRADE_WINDOW_WIDTH;

        unsigned char* src = windowGetBuffer(_barter_back_win);
        blitBufferToBuffer(src + INVENTORY_TRADE_BACKGROUND_WINDOW_WIDTH * INVENTORY_TRADE_RIGHT_SCROLLER_Y + INVENTORY_TRADE_RIGHT_SCROLLER_X + INVENTORY_TRADE_WINDOW_OFFSET, INVENTORY_SLOT_WIDTH, INVENTORY_SLOT_HEIGHT * gInventorySlotsCount, INVENTORY_TRADE_BACKGROUND_WINDOW_WIDTH, windowBuffer + INVENTORY_TRADE_WINDOW_WIDTH * INVENTORY_TRADE_RIGHT_SCROLLER_Y + INVENTORY_TRADE_RIGHT_SCROLLER_X, INVENTORY_TRADE_WINDOW_WIDTH);
    } else {
        assert(false && "Should be unreachable");
    }

    int y = 0;
    for (int slotIndex = 0; slotIndex < gInventorySlotsCount; slotIndex++) {
        int itemIndex = stackOffset + slotIndex;
        if (itemIndex >= inventory->length) {
            break;
        }

        int offset;
        if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
            offset = pitch * (y + INVENTORY_LOOT_RIGHT_SCROLLER_Y_PAD) + INVENTORY_LOOT_RIGHT_SCROLLER_X_PAD;
        } else if (inventoryWindowType == INVENTORY_WINDOW_TYPE_TRADE) {
            offset = pitch * (y + INVENTORY_TRADE_RIGHT_SCROLLER_Y_PAD) + INVENTORY_TRADE_RIGHT_SCROLLER_X_PAD;
        } else {
            assert(false && "Should be unreachable");
        }

        InventoryItem* inventoryItem = &(inventory->items[inventory->length - (itemIndex + 1)]);
        int inventoryFid = itemGetInventoryFid(inventoryItem->item);
        artRender(inventoryFid, windowBuffer + offset, INVENTORY_SLOT_WIDTH_PAD, INVENTORY_SLOT_HEIGHT_PAD, pitch);
        _display_inventory_info(inventoryItem->item, inventoryItem->quantity, windowBuffer + offset, pitch, slotIndex == dragSlotIndex);

        y += INVENTORY_SLOT_HEIGHT;
    }

    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
        if (gSecondaryInventoryScrollUpButton != -1) {
            if (stackOffset <= 0) {
                buttonDisable(gSecondaryInventoryScrollUpButton);
            } else {
                buttonEnable(gSecondaryInventoryScrollUpButton);
            }
        }

        if (gSecondaryInventoryScrollDownButton != -1) {
            if (inventory->length - stackOffset <= gInventorySlotsCount) {
                buttonDisable(gSecondaryInventoryScrollDownButton);
            } else {
                buttonEnable(gSecondaryInventoryScrollDownButton);
            }
        }
    }

    // CE: Show items weight.
    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
        char formattedText[20];
        formattedText[0] = '\0';

        int oldFont = fontGetCurrent();
        fontSetCurrent(101);

        FrmImage backgroundFrmImage;
        int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, 114, 0, 0, 0);
        if (backgroundFrmImage.lock(backgroundFid)) {
            int x = INVENTORY_LOOT_RIGHT_SCROLLER_X;
            int y = INVENTORY_LOOT_RIGHT_SCROLLER_Y + INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + 2;
            blitBufferToBuffer(backgroundFrmImage.getData() + pitch * y + x,
                INVENTORY_SLOT_WIDTH,
                fontGetLineHeight(),
                pitch,
                windowBuffer + pitch * y + x,
                pitch);
        }

        Object* object = _target_stack[_target_curr_stack];

        int color = _colorTable[992];
        if (PID_TYPE(object->pid) == OBJ_TYPE_CRITTER) {
            int currentWeight = objectGetInventoryWeight(object);
            int maxWeight = critterGetStat(object, STAT_CARRY_WEIGHT);
            snprintf(formattedText, sizeof(formattedText), "%d/%d", currentWeight, maxWeight);

            if (critterIsEncumbered(object)) {
                color = _colorTable[31744];
            }
        } else if (PID_TYPE(object->pid) == OBJ_TYPE_ITEM) {
            if (itemGetType(object) == ITEM_TYPE_CONTAINER) {
                int currentSize = containerGetTotalSize(object);
                int maxSize = containerGetMaxSize(object);
                snprintf(formattedText, sizeof(formattedText), "%d/%d", currentSize, maxSize);
            }
        } else {
            int inventoryWeight = objectGetInventoryWeight(object);
            snprintf(formattedText, sizeof(formattedText), "%d", inventoryWeight);
        }

        int width = fontGetStringWidth(formattedText);
        int x = INVENTORY_LOOT_RIGHT_SCROLLER_X + INVENTORY_SLOT_WIDTH / 2 - width / 2;
        int y = INVENTORY_LOOT_RIGHT_SCROLLER_Y + INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + 2;
        fontDrawText(windowBuffer + pitch * y + x, formattedText, width, pitch, color);

        fontSetCurrent(oldFont);
    }
}

// Renders inventory item quantity.
//
// 0x4705A0
static void _display_inventory_info(Object* item, int quantity, unsigned char* dest, int pitch, bool isDragged)
{
    int oldFont = fontGetCurrent();
    fontSetCurrent(101);

    char formattedText[12];

    // NOTE: Original code is slightly different and probably used goto.
    bool draw = false;

    if (itemGetType(item) == ITEM_TYPE_AMMO) {
        int ammoQuantity = ammoGetCapacity(item) * (quantity - 1);

        if (!isDragged) {
            ammoQuantity += ammoGetQuantity(item);
        }

        if (ammoQuantity > 99999) {
            ammoQuantity = 99999;
        }

        snprintf(formattedText, sizeof(formattedText), "x%d", ammoQuantity);
        draw = true;
    } else {
        if (quantity > 1) {
            int v9 = quantity;
            if (isDragged) {
                v9 -= 1;
            }

            if (quantity > 1) {
                if (v9 > 99999) {
                    v9 = 99999;
                }

                snprintf(formattedText, sizeof(formattedText), "x%d", v9);
                draw = true;
            }
        }
    }

    if (draw) {
        fontDrawText(dest, formattedText, 80, pitch, _colorTable[32767]);
    }

    fontSetCurrent(oldFont);
}

// 0x470650
// CE: set to draw the next _display_body at once, past its rotation throttle -- the loot window's
// switch to another body must show that body even when the last one was drawn a moment ago.
static bool display_body_force = false;

static void _display_body(int fid, int inventoryWindowType)
{
    if (!display_body_force && getTicksSince(gInventoryWindowDudeRotationTimestamp) < INVENTORY_NORMAL_WINDOW_PC_ROTATION_DELAY) {
        return;
    }
    display_body_force = false;

    gInventoryWindowDudeRotation += 1;

    if (gInventoryWindowDudeRotation == ROTATION_COUNT) {
        gInventoryWindowDudeRotation = 0;
    }

    int rotations[2];
    if (fid == -1) {
        rotations[0] = gInventoryWindowDudeRotation;
        rotations[1] = ROTATION_SE;
    } else {
        rotations[0] = ROTATION_SW;
        rotations[1] = _target_stack[_target_curr_stack]->rotation;
    }

    int fids[2] = {
        gInventoryWindowDudeFid,
        fid,
    };

    for (int index = 0; index < 2; index += 1) {
        int fid = fids[index];
        if (fid == -1) {
            continue;
        }

        CacheEntry* handle;
        Art* art = artLock(fid, &handle);
        if (art == nullptr) {
            continue;
        }

        int frame = 0;
        if (index == 1) {
            frame = artGetFrameCount(art) - 1;
        }

        int rotation = rotations[index];

        unsigned char* frameData = artGetFrameData(art, frame, rotation);

        int framePitch = artGetWidth(art, frame, rotation);
        int frameWidth = std::min(framePitch, INVENTORY_BODY_VIEW_WIDTH);

        int frameHeight = artGetHeight(art, frame, rotation);
        if (frameHeight > INVENTORY_BODY_VIEW_HEIGHT) {
            frameHeight = INVENTORY_BODY_VIEW_HEIGHT;
        }

        int win;
        Rect rect;
        if (inventoryWindowType == INVENTORY_WINDOW_TYPE_TRADE) {
            unsigned char* windowBuffer = windowGetBuffer(_barter_back_win);
            int windowPitch = windowGetWidth(_barter_back_win);

            if (index == 1) {
                rect.left = 560;
                rect.top = 25;
            } else {
                rect.left = 15;
                rect.top = 25;
            }

            rect.right = rect.left + INVENTORY_BODY_VIEW_WIDTH - 1;
            rect.bottom = rect.top + INVENTORY_BODY_VIEW_HEIGHT - 1;

            FrmImage backgroundFrmImage;
            int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, gGameDialogSpeakerIsPartyMember ? 420 : 111, 0, 0, 0);
            if (backgroundFrmImage.lock(backgroundFid)) {
                blitBufferToBuffer(backgroundFrmImage.getData() + rect.top * 640 + rect.left,
                    INVENTORY_BODY_VIEW_WIDTH,
                    INVENTORY_BODY_VIEW_HEIGHT,
                    640,
                    windowBuffer + windowPitch * rect.top + rect.left,
                    windowPitch);
            }

            blitBufferToBufferTrans(frameData, frameWidth, frameHeight, framePitch,
                windowBuffer + windowPitch * (rect.top + (INVENTORY_BODY_VIEW_HEIGHT - frameHeight) / 2) + (INVENTORY_BODY_VIEW_WIDTH - frameWidth) / 2 + rect.left,
                windowPitch);

            win = _barter_back_win;
        } else {
            unsigned char* windowBuffer = windowGetBuffer(gInventoryWindow);
            int windowPitch = windowGetWidth(gInventoryWindow);

            if (index == 1) {
                if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
                    rect.left = 426;
                    rect.top = 39;
                } else {
                    rect.left = 297;
                    rect.top = 37;
                }
            } else {
                if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT) {
                    rect.left = 48;
                    rect.top = 39;
                } else {
                    rect.left = 176;
                    rect.top = 37;
                }
            }

            rect.right = rect.left + INVENTORY_BODY_VIEW_WIDTH - 1;
            rect.bottom = rect.top + INVENTORY_BODY_VIEW_HEIGHT - 1;

            FrmImage backgroundFrmImage;
            int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, 114, 0, 0, 0);
            if (backgroundFrmImage.lock(backgroundFid)) {
                blitBufferToBuffer(backgroundFrmImage.getData() + INVENTORY_LOOT_WINDOW_WIDTH * rect.top + rect.left,
                    INVENTORY_BODY_VIEW_WIDTH,
                    INVENTORY_BODY_VIEW_HEIGHT,
                    INVENTORY_LOOT_WINDOW_WIDTH,
                    windowBuffer + windowPitch * rect.top + rect.left,
                    windowPitch);
            }

            blitBufferToBufferTrans(frameData, frameWidth, frameHeight, framePitch,
                windowBuffer + windowPitch * (rect.top + (INVENTORY_BODY_VIEW_HEIGHT - frameHeight) / 2) + (INVENTORY_BODY_VIEW_WIDTH - frameWidth) / 2 + rect.left,
                windowPitch);

            win = gInventoryWindow;
        }
        windowRefreshRect(win, &rect);

        artUnlock(handle);
    }

    gInventoryWindowDudeRotationTimestamp = getTicks();
}

// 0x470A2C
static int inventoryCommonInit()
{
    if (inventoryMessageListInit() == -1) {
        return -1;
    }

    _inven_ui_was_disabled = gameUiIsDisabled();

    if (_inven_ui_was_disabled) {
        gameUiEnable();
    }

    gameMouseObjectsHide();

    gameMouseSetCursor(MOUSE_CURSOR_ARROW);

    int index;
    for (index = 0; index < INVENTORY_WINDOW_CURSOR_COUNT; index++) {
        InventoryCursorData* cursorData = &(gInventoryCursorData[index]);

        int fid = buildFid(OBJ_TYPE_INTERFACE, gInventoryWindowCursorFrmIds[index], 0, 0, 0);
        Art* frm = artLock(fid, &(cursorData->frmHandle));
        if (frm == nullptr) {
            break;
        }

        cursorData->frm = frm;
        cursorData->frmData = artGetFrameData(frm, 0, 0);
        cursorData->width = artGetWidth(frm, 0, 0);
        cursorData->height = artGetHeight(frm, 0, 0);
        artGetFrameOffsets(frm, 0, 0, &(cursorData->offsetX), &(cursorData->offsetY));
    }

    if (index != INVENTORY_WINDOW_CURSOR_COUNT) {
        for (; index >= 0; index--) {
            artUnlock(gInventoryCursorData[index].frmHandle);
        }

        if (_inven_ui_was_disabled) {
            gameUiDisable(0);
        }

        messageListFree(&gInventoryMessageList);

        return -1;
    }

    _inven_is_initialized = true;
    _im_value = -1;

    return 0;
}

// NOTE: Inlined.
//
// 0x470B8C
static void inventoryCommonFree()
{
    for (int index = 0; index < INVENTORY_WINDOW_CURSOR_COUNT; index++) {
        artUnlock(gInventoryCursorData[index].frmHandle);
    }

    if (_inven_ui_was_disabled) {
        gameUiDisable(0);
    }

    // NOTE: Uninline.
    inventoryMessageListFree();

    _inven_is_initialized = 0;
}

// 0x470BCC
void inventorySetCursor(int cursor)
{
    gInventoryCursor = cursor;

    if (cursor != INVENTORY_WINDOW_CURSOR_ARROW || _im_value == -1) {
        InventoryCursorData* cursorData = &(gInventoryCursorData[cursor]);
        mouseSetFrame(cursorData->frmData, cursorData->width, cursorData->height, cursorData->width, cursorData->offsetX, cursorData->offsetY, 0);
    } else {
        inventoryItemSlotOnMouseEnter(-1, _im_value);
    }
}

// 0x470C2C
static void inventoryItemSlotOnMouseEnter(int btn, int keyCode)
{
    if (gInventoryCursor == INVENTORY_WINDOW_CURSOR_ARROW) {
        int x;
        int y;
        mouseGetPositionInWindow(gInventoryWindow, &x, &y);

        Object* item = nullptr;
        if (_inven_from_button(keyCode, &item, nullptr, nullptr) != 0) {
            gameMouseRenderPrimaryAction(x, y, 3, gInventoryWindowMaxX, gInventoryWindowMaxY);

            int v5 = 0;
            int v6 = 0;
            _gmouse_3d_pick_frame_hot(&v5, &v6);

            InventoryCursorData* cursorData = &(gInventoryCursorData[INVENTORY_WINDOW_CURSOR_PICK]);
            mouseSetFrame(cursorData->frmData, cursorData->width, cursorData->height, cursorData->width, v5, v6, 0);

            if (item != _last_target) {
                _obj_look_at_func(_stack[0], item, gInventoryPrintItemDescriptionHandler);
            }
        } else {
            InventoryCursorData* cursorData = &(gInventoryCursorData[INVENTORY_WINDOW_CURSOR_ARROW]);
            mouseSetFrame(cursorData->frmData, cursorData->width, cursorData->height, cursorData->width, cursorData->offsetX, cursorData->offsetY, 0);
        }

        _last_target = item;
    }

    _im_value = keyCode;
}

// 0x470D1C
static void inventoryItemSlotOnMouseExit(int btn, int keyCode)
{
    if (gInventoryCursor == INVENTORY_WINDOW_CURSOR_ARROW) {
        InventoryCursorData* cursorData = &(gInventoryCursorData[INVENTORY_WINDOW_CURSOR_ARROW]);
        mouseSetFrame(cursorData->frmData, cursorData->width, cursorData->height, cursorData->width, cursorData->offsetX, cursorData->offsetY, 0);
    }

    _im_value = -1;
}

// 0x470D5C
static void _inven_update_lighting(Object* activeItem)
{
    if (gDude == _inven_dude) {
        int lightDistance;
        if (activeItem != nullptr && activeItem->lightDistance > 4) {
            lightDistance = activeItem->lightDistance;
        } else {
            lightDistance = 4;
        }

        Rect rect;
        objectSetLight(_inven_dude, lightDistance, 0x10000, &rect);
        tileWindowRefreshRect(&rect, gElevation);
    }
}

// 0x470DB8
static void _inven_pickup(int buttonCode, int indexOffset)
{
    Object* item;
    Object** itemSlot = nullptr;
    int count = _inven_from_button(buttonCode, &item, &itemSlot, nullptr);
    if (count == 0) {
        return;
    }

    int itemIndex = -1;
    Object* itemInHand = nullptr;
    Rect rect;

    switch (buttonCode) {
    case 1006:
        rect.left = 245;
        rect.top = 286;
        if (_inven_dude == gDude && interfaceGetCurrentHand() != HAND_LEFT) {
            itemInHand = item;
        }
        break;
    case 1007:
        rect.left = 154;
        rect.top = 286;
        if (_inven_dude == gDude && interfaceGetCurrentHand() == HAND_LEFT) {
            itemInHand = item;
        }
        break;
    case 1008:
        rect.left = 154;
        rect.top = 183;
        break;
    default:
        // NOTE: Original code a little bit different, this code path
        // is only for key codes below 1006.
        itemIndex = buttonCode - 1000;
        rect.left = INVENTORY_SCROLLER_X;
        rect.top = INVENTORY_SLOT_HEIGHT * itemIndex + INVENTORY_SCROLLER_Y;
        break;
    }

    if (itemIndex == -1 || _pud->items[indexOffset + itemIndex].quantity <= 1) {
        unsigned char* windowBuffer = windowGetBuffer(gInventoryWindow);
        if (gInventoryRightHandItem != gInventoryLeftHandItem || item != gInventoryLeftHandItem) {
            int height;
            int width;
            if (itemIndex == -1) {
                height = INVENTORY_LARGE_SLOT_HEIGHT;
                width = INVENTORY_LARGE_SLOT_WIDTH;
            } else {
                height = INVENTORY_SLOT_HEIGHT;
                width = INVENTORY_SLOT_WIDTH;
            }

            FrmImage backgroundFrmImage;
            int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, 48, 0, 0, 0);
            if (backgroundFrmImage.lock(backgroundFid)) {
                blitBufferToBuffer(backgroundFrmImage.getData() + INVENTORY_WINDOW_WIDTH * rect.top + rect.left,
                    width,
                    height,
                    INVENTORY_WINDOW_WIDTH,
                    windowBuffer + INVENTORY_WINDOW_WIDTH * rect.top + rect.left,
                    INVENTORY_WINDOW_WIDTH);
            }

            rect.right = rect.left + width - 1;
            rect.bottom = rect.top + height - 1;
        } else {
            FrmImage backgroundFrmImage;
            int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, 48, 0, 0, 0);
            if (backgroundFrmImage.lock(backgroundFid)) {
                blitBufferToBuffer(backgroundFrmImage.getData() + INVENTORY_WINDOW_WIDTH * 286 + 154,
                    180,
                    61,
                    INVENTORY_WINDOW_WIDTH,
                    windowBuffer + INVENTORY_WINDOW_WIDTH * 286 + 154,
                    INVENTORY_WINDOW_WIDTH);
            }

            rect.left = 154;
            rect.top = 286;
            rect.right = rect.left + 180 - 1;
            rect.bottom = rect.top + 61 - 1;
        }
        windowRefreshRect(gInventoryWindow, &rect);
    } else {
        _display_inventory(indexOffset, itemIndex, INVENTORY_WINDOW_TYPE_NORMAL);
    }

    FrmImage itemInventoryFrmImage;
    int itemInventoryFid = itemGetInventoryFid(item);
    if (itemInventoryFrmImage.lock(itemInventoryFid)) {
        int width = itemInventoryFrmImage.getWidth();
        int height = itemInventoryFrmImage.getHeight();
        unsigned char* data = itemInventoryFrmImage.getData();
        mouseSetFrame(data, width, height, width, width / 2, height / 2, 0);
        soundPlayFile("ipickup1");
    }

    if (itemInHand != nullptr) {
        _inven_update_lighting(nullptr);
    }

    do {
        sharedFpsLimiter.mark();

        inputGetInput();
        _display_body(-1, INVENTORY_WINDOW_TYPE_NORMAL);

        renderPresent();
        sharedFpsLimiter.throttle();
    } while ((mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_REPEAT) != 0);

    if (itemInventoryFrmImage.isLocked()) {
        itemInventoryFrmImage.unlock();
        soundPlayFile("iputdown");
    }

    if (mouseHitTestInWindow(gInventoryWindow, INVENTORY_SCROLLER_X, INVENTORY_SCROLLER_Y, INVENTORY_SCROLLER_MAX_X, INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + INVENTORY_SCROLLER_Y)) {
        int x;
        int y;
        mouseGetPositionInWindow(gInventoryWindow, &x, &y);

        int targetIndex = (y - 39) / INVENTORY_SLOT_HEIGHT + indexOffset;
        if (targetIndex < _pud->length) {
            Object* targetItem = _pud->items[targetIndex].item;
            if (targetItem != item) {
                // Dropping item on top of another item.
                if (itemGetType(targetItem) == ITEM_TYPE_CONTAINER) {
                    if (_drop_into_container(targetItem, item, itemIndex, itemSlot, count) == 0) {
                        itemIndex = 0;
                    }
                } else {
                    if (_drop_ammo_into_weapon(targetItem, item, itemSlot, count, buttonCode) == 0) {
                        itemIndex = 0;
                    }
                }
            }
        }

        if (itemIndex == -1) {
            // TODO: Holy shit, needs refactoring.
            *itemSlot = nullptr;
            if (itemAdd(_inven_dude, item, 1)) {
                *itemSlot = item;
            } else if (itemSlot == &gInventoryArmor) {
                _adjust_ac(_stack[0], item, nullptr);
            } else if (gInventoryRightHandItem == gInventoryLeftHandItem) {
                gInventoryLeftHandItem = nullptr;
                gInventoryRightHandItem = nullptr;
            }
        }
    } else if (mouseHitTestInWindow(gInventoryWindow, INVENTORY_LEFT_HAND_SLOT_X, INVENTORY_LEFT_HAND_SLOT_Y, INVENTORY_LEFT_HAND_SLOT_MAX_X, INVENTORY_LEFT_HAND_SLOT_MAX_Y)) {
        if (gInventoryLeftHandItem != nullptr && itemGetType(gInventoryLeftHandItem) == ITEM_TYPE_CONTAINER && gInventoryLeftHandItem != item) {
            _drop_into_container(gInventoryLeftHandItem, item, itemIndex, itemSlot, count);
        } else if (gInventoryLeftHandItem == nullptr || _drop_ammo_into_weapon(gInventoryLeftHandItem, item, itemSlot, count, buttonCode)) {
            _switch_hand(item, &gInventoryLeftHandItem, itemSlot, buttonCode);
        }
    } else if (mouseHitTestInWindow(gInventoryWindow, INVENTORY_RIGHT_HAND_SLOT_X, INVENTORY_RIGHT_HAND_SLOT_Y, INVENTORY_RIGHT_HAND_SLOT_MAX_X, INVENTORY_RIGHT_HAND_SLOT_MAX_Y)) {
        if (gInventoryRightHandItem != nullptr && itemGetType(gInventoryRightHandItem) == ITEM_TYPE_CONTAINER && gInventoryRightHandItem != item) {
            _drop_into_container(gInventoryRightHandItem, item, itemIndex, itemSlot, count);
        } else if (gInventoryRightHandItem == nullptr || _drop_ammo_into_weapon(gInventoryRightHandItem, item, itemSlot, count, buttonCode)) {
            _switch_hand(item, &gInventoryRightHandItem, itemSlot, itemIndex);
        }
    } else if (mouseHitTestInWindow(gInventoryWindow, INVENTORY_ARMOR_SLOT_X, INVENTORY_ARMOR_SLOT_Y, INVENTORY_ARMOR_SLOT_MAX_X, INVENTORY_ARMOR_SLOT_MAX_Y)) {
        if (itemGetType(item) == ITEM_TYPE_ARMOR) {
            Object* currentArmor = gInventoryArmor;
            int itemAddResult = 0;
            if (itemIndex != -1) {
                itemRemove(_inven_dude, item, 1);
            }

            if (gInventoryArmor != nullptr) {
                if (itemSlot != nullptr) {
                    *itemSlot = gInventoryArmor;
                } else {
                    gInventoryArmor = nullptr;
                    itemAddResult = itemAdd(_inven_dude, currentArmor, 1);
                }
            } else {
                if (itemSlot != nullptr) {
                    *itemSlot = gInventoryArmor;
                }
            }

            if (itemAddResult != 0) {
                gInventoryArmor = currentArmor;
                if (itemIndex != -1) {
                    itemAdd(_inven_dude, item, 1);
                }
            } else {
                _adjust_ac(_stack[0], currentArmor, item);
                gInventoryArmor = item;
            }
        }
    } else if (mouseHitTestInWindow(gInventoryWindow, INVENTORY_PC_BODY_VIEW_X, INVENTORY_PC_BODY_VIEW_Y, INVENTORY_PC_BODY_VIEW_MAX_X, INVENTORY_PC_BODY_VIEW_MAX_Y)) {
        if (_curr_stack != 0) {
            // If we are looking inside nested inventory (such as backpack item), we see this item in the PC Body View instead of the player.
            // So we drop item into it.
            _drop_into_container(_stack[_curr_stack - 1], item, itemIndex, itemSlot, count);
        }
    }

    _adjust_fid();
    inventoryRenderSummary();
    _display_inventory(indexOffset, -1, INVENTORY_WINDOW_TYPE_NORMAL);
    inventorySetCursor(INVENTORY_WINDOW_CURSOR_HAND);
    if (_inven_dude == gDude) {
        Object* item;
        if (interfaceGetCurrentHand() == HAND_LEFT) {
            item = critterGetItem1(_inven_dude);
        } else {
            item = critterGetItem2(_inven_dude);
        }

        if (item != nullptr) {
            _inven_update_lighting(item);
        }
    }
}

// Controller shortcut: equip the inventory-list item under the cursor into a
// hand slot (hand == HAND_LEFT or HAND_RIGHT). Mirrors the list-item -> hand-slot
// drop in _inven_pickup, but is driven by a button press instead of a mouse drag.
// Returns false (no-op) unless the normal inventory is open and the cursor is
// over a populated list slot.
bool inven_equip_under_cursor(int hand)
{
    if (!normal_inventory_open || gInventoryWindow == -1) {
        return false;
    }

    // OVER A HAND SLOT = TAKE THAT HAND'S ITEM BACK OUT, rather than equip. Without this there was
    // no way to empty a hand from the inventory at all: X/B only ever worked over the item LIST, so
    // the slots were write-only and you could swap weapons forever but never end up empty-handed
    // ("v inventory mi nejdou odebrat itemy z A a B"). Which hand is decided by the SLOT under the
    // cursor, not by which button was pressed -- pointing at a thing and pressing either button
    // should do what you are pointing at.
    bool overLeft = mouseHitTestInWindow(gInventoryWindow, INVENTORY_LEFT_HAND_SLOT_X, INVENTORY_LEFT_HAND_SLOT_Y,
        INVENTORY_LEFT_HAND_SLOT_MAX_X, INVENTORY_LEFT_HAND_SLOT_MAX_Y);
    bool overRight = mouseHitTestInWindow(gInventoryWindow, INVENTORY_RIGHT_HAND_SLOT_X, INVENTORY_RIGHT_HAND_SLOT_Y,
        INVENTORY_RIGHT_HAND_SLOT_MAX_X, INVENTORY_RIGHT_HAND_SLOT_MAX_Y);

    if (overLeft || overRight) {
        Object** handSlot = overLeft ? &gInventoryLeftHandItem : &gInventoryRightHandItem;
        if (*handSlot == NULL) {
            return false; // already empty -- let the caller refuse audibly
        }

        // Back into the inventory working copy, then clear the slot. itemAdd is what
        // _switch_hand uses to return a displaced hand item, so the bookkeeping matches the
        // engine's own path and the OK button commits it normally.
        if (itemAdd(_inven_dude, *handSlot, 1) != 0) {
            return false;
        }
        *handSlot = NULL;

        _adjust_fid();
        inventoryRenderSummary();
        _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_NORMAL);
        windowRefresh(gInventoryWindow);
        soundPlayFile("iputdown");
        return true;
    }

    // Same list region _inven_pickup uses to decide a drop targets the list.
    if (!mouseHitTestInWindow(gInventoryWindow, INVENTORY_SCROLLER_X, INVENTORY_SCROLLER_Y, INVENTORY_SCROLLER_MAX_X, INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + INVENTORY_SCROLLER_Y)) {
        return false;
    }

    int x;
    int y;
    mouseGetPositionInWindow(gInventoryWindow, &x, &y);

    // Row under the cursor (matches _inven_pickup's (y - 39) / 48 mapping).
    int displayIndex = (y - 39) / INVENTORY_SLOT_HEIGHT;
    if (displayIndex < 0 || displayIndex >= gInventorySlotsCount) {
        return false;
    }

    int itemIndex = _stack_offset[_curr_stack] + displayIndex;
    if (itemIndex >= _pud->length) {
        return false;
    }

    Object* item = _pud->items[_pud->length - (itemIndex + 1)].item;
    if (item == NULL) {
        return false;
    }

    // a3 (source slot) is NULL for a list item; a4 != -1 makes _switch_hand pull
    // the item out of the inventory and return any displaced hand item to it.
    Object** handSlot = (hand == HAND_RIGHT) ? &gInventoryRightHandItem : &gInventoryLeftHandItem;
    _switch_hand(item, handSlot, NULL, 1000 + displayIndex);

    _adjust_fid();
    inventoryRenderSummary();
    _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_NORMAL);
    windowRefresh(gInventoryWindow);
    soundPlayFile("iputdown");
    return true;
}

// Controller shortcut: WEAR the inventory-list armor under the cursor (into the body
// armor slot), instead of dropping it into a hand. Mirrors the list-item -> armor-slot
// drop in _inven_pickup (the ITEM_TYPE_ARMOR branch over INVENTORY_ARMOR_SLOT), driven
// by a button press rather than a mouse drag: pull the item from the list, return any
// currently-worn armor to the list, run _adjust_ac, and set it as worn. If the displaced
// armor can't fit back in the inventory the swap is rolled back (item stays in the list).
// Returns false (no-op) unless the normal inventory is open and ARMOR is under the cursor.
Object* inven_list_item_under_cursor()
{
    if (!normal_inventory_open || gInventoryWindow == -1) {
        return NULL;
    }
    // Same list region and row mapping as inven_equip_under_cursor.
    if (!mouseHitTestInWindow(gInventoryWindow, INVENTORY_SCROLLER_X, INVENTORY_SCROLLER_Y, INVENTORY_SCROLLER_MAX_X, INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + INVENTORY_SCROLLER_Y)) {
        return NULL;
    }
    int x;
    int y;
    mouseGetPositionInWindow(gInventoryWindow, &x, &y);
    int displayIndex = (y - 39) / INVENTORY_SLOT_HEIGHT;
    if (displayIndex < 0 || displayIndex >= gInventorySlotsCount) {
        return NULL;
    }
    int itemIndex = _stack_offset[_curr_stack] + displayIndex;
    if (itemIndex >= _pud->length) {
        return NULL;
    }
    return _pud->items[_pud->length - (itemIndex + 1)].item;
}

// A DRUG TAKEN INSIDE THE INVENTORY IS FREE, AS IN VANILLA (fallout1-ce 2026-10-19). For a few minutes it
// cost 2 AP (1 with Quick Pockets) -- the screen stops the world, and opening it for 4 AP bought any
// number of stimpaks -- and then he took it back ("tak asi to nemusí stát už 2 AP", pick "Zdarma, jako
// ve vanille"): a stimpak heals over time in a fight now (combat_rt_heal_over_time), and while the
// screen is open nothing comes in at all. What you pay is the opening.

bool inven_wear_under_cursor()
{
    if (!normal_inventory_open || gInventoryWindow == -1) {
        return false;
    }

    if (!mouseHitTestInWindow(gInventoryWindow, INVENTORY_SCROLLER_X, INVENTORY_SCROLLER_Y, INVENTORY_SCROLLER_MAX_X, INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + INVENTORY_SCROLLER_Y)) {
        return false;
    }

    int x;
    int y;
    mouseGetPositionInWindow(gInventoryWindow, &x, &y);

    int displayIndex = (y - 39) / INVENTORY_SLOT_HEIGHT;
    if (displayIndex < 0 || displayIndex >= gInventorySlotsCount) {
        return false;
    }

    int itemIndex = _stack_offset[_curr_stack] + displayIndex;
    if (itemIndex >= _pud->length) {
        return false;
    }

    Object* item = _pud->items[_pud->length - (itemIndex + 1)].item;
    if (item == NULL || itemGetType(item) != ITEM_TYPE_ARMOR) {
        return false;
    }

    // Already worn (cursor over the same object) -- nothing to do.
    if (item == gInventoryArmor) {
        return false;
    }

    Object* oldArmor = gInventoryArmor;

    // Pull the to-wear item out of the inventory list.
    itemRemove(_inven_dude, item, 1);

    // Return any currently-worn armor to the inventory list. If it won't fit, roll back.
    int addFailed = 0;
    if (oldArmor != NULL) {
        gInventoryArmor = NULL;
        addFailed = itemAdd(_inven_dude, oldArmor, 1);
    }

    if (addFailed != 0) {
        gInventoryArmor = oldArmor;
        itemAdd(_inven_dude, item, 1);
        return false;
    }

    _adjust_ac(_stack[0], oldArmor, item);
    gInventoryArmor = item;

    _adjust_fid();
    inventoryRenderSummary();
    _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_NORMAL);
    windowRefresh(gInventoryWindow);
    soundPlayFile("iputdown");
    return true;
}

// Controller quick-action for the player's own (normal) inventory list: a tap of
// A on a list item does the most useful one-shot action without any popup --
// drugs and use-on-self items are used, everything else is equipped into the
// hand box labelled "A" (the left hand). Mirrors inventoryWindowOpenContextMenu's NORMAL-window
// USE branch for the use case and inven_equip_under_cursor for the equip case,
// so it respects the inventory screen's working-copy bookkeeping. Hold A / RB->A
// still opens the full Use/Drop/Look popup. Returns true if it acted.
bool inven_gamepad_quick_normal()
{
    if (!normal_inventory_open || gInventoryWindow == -1) {
        return false;
    }

    // ⚠️ A ON THE ARMOR SLOT TAKES THE ARMOR OFF (2026-10-10): *"body armour mi nejde sundat"*. Vanilla
    // only undresses by DRAGGING the armor from its slot onto the list, and the pad's A is one click --
    // the drag ended on the slot it started from, so the armor went straight back on and the pad had
    // no way to take it off at all (A on the list already PUTS it on: inven_wear_under_cursor). This
    // mirrors the drag's drop-onto-list branch in _inven_pickup: back into the bag, _adjust_ac, and if
    // the bag cannot take it the armor stays worn.
    if (inven_normal_cursor_over_armor()) {
        if (gInventoryArmor == NULL) {
            return false;
        }

        Object* armor = gInventoryArmor;
        gInventoryArmor = NULL;
        if (itemAdd(_inven_dude, armor, 1) != 0) {
            gInventoryArmor = armor;
            soundPlayFile("iisxxxx1");
            return true;
        }

        _adjust_ac(_stack[0], armor, NULL);
        _adjust_fid();
        inventoryRenderSummary();
        _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_NORMAL);
        windowRefresh(gInventoryWindow);
        soundPlayFile("iputdown");
        return true;
    }

    // Same list region _inven_pickup/inven_equip_under_cursor use.
    if (!mouseHitTestInWindow(gInventoryWindow, INVENTORY_SCROLLER_X, INVENTORY_SCROLLER_Y, INVENTORY_SCROLLER_MAX_X, INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + INVENTORY_SCROLLER_Y)) {
        return false;
    }

    int x;
    int y;
    mouseGetPositionInWindow(gInventoryWindow, &x, &y);

    int displayIndex = (y - 39) / INVENTORY_SLOT_HEIGHT;
    if (displayIndex < 0 || displayIndex >= gInventorySlotsCount) {
        return false;
    }

    int itemIndex = _stack_offset[_curr_stack] + displayIndex;
    if (itemIndex >= _pud->length) {
        return false;
    }

    int keyCode = 1000 + displayIndex;
    Object* item;
    Object** v43;
    Object* v41;
    if (_inven_from_button(keyCode, &item, &v43, &v41) == 0) {
        return false;
    }

    int itemType = itemGetType(item);

    // Ammo loads into a compatible equipped weapon (right hand preferred) rather
    // than going to a hand slot, reusing the engine's drag-ammo-onto-weapon path
    // (loads one ammo object, no quantity prompt). weaponCanBeReloadedWith also rejects
    // a weapon already holding a DIFFERENT ammo type (it must be unloaded first
    // via the popup), in which case no weapon matches and this is a no-op.
    if (itemType == ITEM_TYPE_AMMO) {
        // Same "active hand first" rule as the equip fallback below: reload the weapon the
        // interface bar is showing, and only fall back to the other hand when that one cannot
        // take this ammo. Only reachable with a reloadable weapon in BOTH hands, but the old
        // hardcoded right-hand preference would then load the gun you are not holding.
        int ammoActive = interfaceGetCurrentHand() != 0 ? HAND_RIGHT : HAND_LEFT;
        Object* first = ammoActive == HAND_RIGHT ? gInventoryRightHandItem : gInventoryLeftHandItem;
        Object* second = ammoActive == HAND_RIGHT ? gInventoryLeftHandItem : gInventoryRightHandItem;

        Object* weapon = NULL;
        if (first != NULL && weaponCanBeReloadedWith(first, item)) {
            weapon = first;
        } else if (second != NULL && weaponCanBeReloadedWith(second, item)) {
            weapon = second;
        }

        if (weapon != NULL) {
            _drop_ammo_into_weapon(weapon, item, v43, 1, keyCode);
            _adjust_fid();
            inventoryRenderSummary();
            _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_NORMAL);
            windowRefresh(gInventoryWindow);
        }
        return true;
    }

    // Armor is WORN (body slot), not dropped into a hand -- a tap of A on armor should
    // put it on. (The hand-equip fallback below is for weapons/everything else.)
    if (itemType == ITEM_TYPE_ARMOR) {
        return inven_wear_under_cursor();
    }

    // Drugs and items you can use ON YOURSELF get used; weapons (and anything else) are equipped
    // into a free hand.
    //
    // ⚠️ proto_action_can_use_ON is deliberately NOT part of this test, though it used to be. A
    // use-ON item -- the rope, a lockpick, a doctor's bag -- cannot do anything from an inventory
    // slot: it needs a TARGET, and the only way to give it one is to hold it and aim the engine's
    // targeting cursor at something. Treating it as "use" meant A on the rope tried to use it on
    // nothing instead of putting it in hand, so there was no way to arm it at all ("provaz se
    // nechce dÃ¡t do Ã¡Äka, protoÅ¾e to bere jako pouÅ¾Ã­t"). can_use (use on SELF) still gets used --
    // that one genuinely works from the list.
    bool useIt = itemType == ITEM_TYPE_DRUG
        || (itemType != ITEM_TYPE_WEAPON && _proto_action_can_use(item->pid));

    // HISTORY (report 1 below). A's target used to be "right if it is free, else left", so with
    // BOTH hands empty the item always landed in the right one -- the slot B equips -- no matter
    // which hand the interface bar was actually showing ("mÃ¡m prÃ¡zdnÃ© ruce a kdyÅ¾ kliknu Ã¡Äkem na
    // item, tak se dÃ¡ do BÄka slotu"). It only showed up with empty hands because with one hand
    // occupied the fallback happened to pick the free one, which is what you wanted anyway.
    //
    // ⚠️ WHICH HAND A FILLS: THE ONE WITH "A" PRINTED ON IT. The two hand boxes carry the pad's own
    // hints -- "A" on the LEFT hand, "B" on the RIGHT (_display_inventory, the CE hint block) -- and
    // B already equips into the B box. A has to do the same with its box, always, replacing what is
    // in it (that item goes back to the list).
    //
    // Every one of his three reports about this was the same thing, and it took three rounds to see
    // it because the second one was misread:
    //   1. empty hands, A on an item -> it landed in B: the rule was "right if free" (the paragraph
    //      above, kept for its quote);
    //   2. "mám zbraň v A a kliknu na jinou zbraň A a hodí se B! ať nahradí A" -- which I took to mean
    //      "the hand the interface bar shows", and A was made to fill the ACTIVE hand;
    //   3. 2026-10-04, with the bar showing the right hand: "kliknu na zbraň áčkem a dá se mi do
    //      Béčka a to nemám v áčku žádné zbraně".
    // A = the A box. Which hand is ACTIVE is Y's business on the interface bar, not A's.
    if (!useIt) {
        return inven_equip_under_cursor(HAND_LEFT);
    }

    if (itemType == ITEM_TYPE_DRUG) {
        if (_item_d_take_drug(_stack[0], item)) {
            if (v43 != NULL) {
                *v43 = NULL;
            } else {
                itemRemove(v41, item, 1);
            }

            _obj_connect(item, gDude->tile, gDude->elevation, NULL);
            _obj_destroy(item);
        }
        interfaceRenderHitPoints(true);
    } else {
        if (v43 == NULL) {
            itemRemove(v41, item, 1);
        }

        int rc;
        if (_proto_action_can_use(item->pid)) {
            rc = _protinst_use_item(_stack[0], item);
        } else {
            rc = _protinst_use_item_on(_stack[0], _stack[0], item);
        }

        if (rc == 1) {
            if (v43 != NULL) {
                *v43 = NULL;
            }

            _obj_connect(item, gDude->tile, gDude->elevation, NULL);
            _obj_destroy(item);
        } else {
            if (v43 == NULL) {
                itemAdd(v41, item, 1);
            }
            inven_place_if_requested(item, v41, v43);
        }
    }

    _adjust_fid();
    inventoryRenderSummary();
    _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_NORMAL);
    windowRefresh(gInventoryWindow);
    return true;
}

// 0x4714E0
static void _switch_hand(Object* sourceItem, Object** targetSlot, Object** sourceSlot, int itemIndex)
{
    if (*targetSlot != nullptr) {
        if (itemGetType(*targetSlot) == ITEM_TYPE_WEAPON && itemGetType(sourceItem) == ITEM_TYPE_AMMO) {
            return;
        }

        if (sourceSlot != nullptr && (sourceSlot != &gInventoryArmor || itemGetType(*targetSlot) == ITEM_TYPE_ARMOR)) {
            if (sourceSlot == &gInventoryArmor) {
                _adjust_ac(_stack[0], gInventoryArmor, *targetSlot);
            }
            *sourceSlot = *targetSlot;
        } else {
            if (itemIndex != -1) {
                itemRemove(_inven_dude, sourceItem, 1);
            }

            Object* existingItem = *targetSlot;
            *targetSlot = nullptr;
            if (itemAdd(_inven_dude, existingItem, 1) != 0) {
                itemAdd(_inven_dude, sourceItem, 1);
                return;
            }

            itemIndex = -1;

            if (sourceSlot != nullptr) {
                if (sourceSlot == &gInventoryArmor) {
                    _adjust_ac(_stack[0], gInventoryArmor, nullptr);
                }
                *sourceSlot = nullptr;
            }
        }
    } else {
        if (sourceSlot != nullptr) {
            if (sourceSlot == &gInventoryArmor) {
                _adjust_ac(_stack[0], gInventoryArmor, nullptr);
            }
            *sourceSlot = nullptr;
        }
    }

    *targetSlot = sourceItem;

    if (itemIndex != -1) {
        itemRemove(_inven_dude, sourceItem, 1);
    }
}

// This function removes armor bonuses and effects granted by [oldArmor] and
// adds appropriate bonuses and effects granted by [newArmor]. Both [oldArmor]
// and [newArmor] can be NULL.
//
// 0x4715F8
void _adjust_ac(Object* critter, Object* oldArmor, Object* newArmor)
{
    int armorClassBonus = critterGetBonusStat(critter, STAT_ARMOR_CLASS);
    int oldArmorClass = armorGetArmorClass(oldArmor);
    int newArmorClass = armorGetArmorClass(newArmor);
    critterSetBonusStat(critter, STAT_ARMOR_CLASS, armorClassBonus - oldArmorClass + newArmorClass);

    int damageResistanceStat = STAT_DAMAGE_RESISTANCE;
    int damageThresholdStat = STAT_DAMAGE_THRESHOLD;
    for (int damageType = 0; damageType < DAMAGE_TYPE_COUNT; damageType += 1) {
        int damageResistanceBonus = critterGetBonusStat(critter, damageResistanceStat);
        int oldArmorDamageResistance = armorGetDamageResistance(oldArmor, damageType);
        int newArmorDamageResistance = armorGetDamageResistance(newArmor, damageType);
        critterSetBonusStat(critter, damageResistanceStat, damageResistanceBonus - oldArmorDamageResistance + newArmorDamageResistance);

        int damageThresholdBonus = critterGetBonusStat(critter, damageThresholdStat);
        int oldArmorDamageThreshold = armorGetDamageThreshold(oldArmor, damageType);
        int newArmorDamageThreshold = armorGetDamageThreshold(newArmor, damageType);
        critterSetBonusStat(critter, damageThresholdStat, damageThresholdBonus - oldArmorDamageThreshold + newArmorDamageThreshold);

        damageResistanceStat += 1;
        damageThresholdStat += 1;
    }

    if (objectIsPartyMember(critter)) {
        if (oldArmor != nullptr) {
            int perk = armorGetPerk(oldArmor);
            perkRemoveEffect(critter, perk);
        }

        if (newArmor != nullptr) {
            int perk = armorGetPerk(newArmor);
            perkAddEffect(critter, perk);
        }
    }
}

// 0x4716E8
static void _adjust_fid()
{
    int fid;
    if (FID_TYPE(_inven_dude->fid) == OBJ_TYPE_CRITTER) {
        Proto* proto;

        int v0 = _art_vault_guy_num;

        if (protoGetProto(_inven_pid, &proto) == -1) {
            v0 = proto->fid & 0xFFF;
        }

        if (gInventoryArmor != nullptr) {
            protoGetProto(gInventoryArmor->pid, &proto);
            if (critterGetStat(_inven_dude, STAT_GENDER) == GENDER_FEMALE) {
                v0 = proto->item.data.armor.femaleFid;
            } else {
                v0 = proto->item.data.armor.maleFid;
            }

            if (v0 == -1) {
                v0 = _art_vault_guy_num;
            }
        }

        int animationCode = 0;
        if (interfaceGetCurrentHand()) {
            if (gInventoryRightHandItem != nullptr) {
                protoGetProto(gInventoryRightHandItem->pid, &proto);
                if (proto->item.type == ITEM_TYPE_WEAPON) {
                    animationCode = proto->item.data.weapon.animationCode;
                }
            }
        } else {
            if (gInventoryLeftHandItem != nullptr) {
                protoGetProto(gInventoryLeftHandItem->pid, &proto);
                if (proto->item.type == ITEM_TYPE_WEAPON) {
                    animationCode = proto->item.data.weapon.animationCode;
                }
            }
        }

        fid = buildFid(OBJ_TYPE_CRITTER, v0, 0, animationCode, 0);
    } else {
        fid = _inven_dude->fid;
    }

    gInventoryWindowDudeFid = fid;
}

// 0x4717E4
void inventoryOpenUseItemOn(Object* targetObj)
{
    ScopedGameMode gm(GameMode::kUseOn);

    if (inventoryCommonInit() == -1) {
        return;
    }

    bool isoWasEnabled = _setup_inventory(INVENTORY_WINDOW_TYPE_USE_ITEM_ON);
    _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_USE_ITEM_ON);
    inventorySetCursor(INVENTORY_WINDOW_CURSOR_HAND);
    for (;;) {
        sharedFpsLimiter.mark();

        if (_game_user_wants_to_quit != 0) {
            break;
        }

        _display_body(-1, INVENTORY_WINDOW_TYPE_USE_ITEM_ON);

        int keyCode = inputGetInput();
        switch (keyCode) {
        case KEY_HOME:
            _stack_offset[_curr_stack] = 0;
            _display_inventory(0, -1, INVENTORY_WINDOW_TYPE_USE_ITEM_ON);
            break;
        case KEY_ARROW_UP:
            if (_stack_offset[_curr_stack] > 0) {
                _stack_offset[_curr_stack] -= 1;
                _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_USE_ITEM_ON);
            }
            break;
        case KEY_PAGE_UP:
            _stack_offset[_curr_stack] -= gInventorySlotsCount;
            if (_stack_offset[_curr_stack] < 0) {
                _stack_offset[_curr_stack] = 0;
                _display_inventory(_stack_offset[_curr_stack], -1, 1);
            }
            break;
        case KEY_END:
            _stack_offset[_curr_stack] = _pud->length - gInventorySlotsCount;
            if (_stack_offset[_curr_stack] < 0) {
                _stack_offset[_curr_stack] = 0;
            }
            _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_USE_ITEM_ON);
            break;
        case KEY_ARROW_DOWN:
            if (_stack_offset[_curr_stack] + gInventorySlotsCount < _pud->length) {
                _stack_offset[_curr_stack] += 1;
                _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_USE_ITEM_ON);
            }
            break;
        case KEY_PAGE_DOWN:
            _stack_offset[_curr_stack] += gInventorySlotsCount;
            if (_stack_offset[_curr_stack] + gInventorySlotsCount >= _pud->length) {
                _stack_offset[_curr_stack] = _pud->length - gInventorySlotsCount;
                if (_stack_offset[_curr_stack] < 0) {
                    _stack_offset[_curr_stack] = 0;
                }
            }
            _display_inventory(_stack_offset[_curr_stack], -1, 1);
            break;
        case 2500:
            _container_exit(keyCode, INVENTORY_WINDOW_TYPE_USE_ITEM_ON);
            break;
        default:
            if ((mouseGetEvent() & MOUSE_EVENT_RIGHT_BUTTON_DOWN) != 0) {
                if (gInventoryCursor == INVENTORY_WINDOW_CURSOR_HAND) {
                    inventorySetCursor(INVENTORY_WINDOW_CURSOR_ARROW);
                } else {
                    inventorySetCursor(INVENTORY_WINDOW_CURSOR_HAND);
                }
            } else if ((mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0) {
                if (keyCode >= 1000 && keyCode < 1000 + gInventorySlotsCount) {
                    if (gInventoryCursor == INVENTORY_WINDOW_CURSOR_ARROW) {
                        inventoryWindowOpenContextMenu(keyCode, INVENTORY_WINDOW_TYPE_USE_ITEM_ON);
                    } else {
                        int inventoryItemIndex = _pud->length - (_stack_offset[_curr_stack] + keyCode - 1000 + 1);
                        // SFALL: Fix crash when clicking on empty space in the inventory list
                        // opened by "Use Inventory Item On" (backpack) action icon
                        if (inventoryItemIndex < _pud->length && inventoryItemIndex >= 0) {
                            InventoryItem* inventoryItem = &(_pud->items[inventoryItemIndex]);
                            if (isInCombat()) {
                                if (gDude->data.critter.combat.ap >= 2) {
                                    if (_action_use_an_item_on_object(gDude, targetObj, inventoryItem->item) != -1) {
                                        int actionPoints = gDude->data.critter.combat.ap;
                                        if (actionPoints < 2) {
                                            gDude->data.critter.combat.ap = 0;
                                        } else {
                                            gDude->data.critter.combat.ap = actionPoints - 2;
                                        }
                                        interfaceRenderActionPoints(gDude->data.critter.combat.ap, _combat_free_move);
                                    }
                                }
                            } else {
                                _action_use_an_item_on_object(gDude, targetObj, inventoryItem->item);
                            }
                            keyCode = KEY_ESCAPE;
                        } else {
                            keyCode = -1;
                        }
                    }
                }
            } else if ((mouseGetEvent() & MOUSE_EVENT_WHEEL) != 0) {
                if (mouseHitTestInWindow(gInventoryWindow, INVENTORY_SCROLLER_X, INVENTORY_SCROLLER_Y, INVENTORY_SCROLLER_MAX_X, INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + INVENTORY_SCROLLER_Y)) {
                    int wheelX;
                    int wheelY;
                    mouseGetWheel(&wheelX, &wheelY);
                    if (wheelY > 0) {
                        if (_stack_offset[_curr_stack] > 0) {
                            _stack_offset[_curr_stack] -= 1;
                            _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_USE_ITEM_ON);
                        }
                    } else if (wheelY < 0) {
                        if (_stack_offset[_curr_stack] + gInventorySlotsCount < _pud->length) {
                            _stack_offset[_curr_stack] += 1;
                            _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_USE_ITEM_ON);
                        }
                    }
                }
            }
        }

        if (keyCode == KEY_ESCAPE) {
            break;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    _exit_inventory(isoWasEnabled);

    // NOTE: Uninline.
    inventoryCommonFree();
}

// 0x471B70
Object* critterGetItem2(Object* critter)
{
    int i;
    Inventory* inventory;
    Object* item;

    if (gInventoryRightHandItem != nullptr && critter == _inven_dude) {
        return gInventoryRightHandItem;
    }

    inventory = &(critter->data.inventory);
    for (i = 0; i < inventory->length; i++) {
        item = inventory->items[i].item;
        if (item->flags & OBJECT_IN_RIGHT_HAND) {
            return item;
        }
    }

    return nullptr;
}

// 0x471BBC
Object* critterGetItem1(Object* critter)
{
    int i;
    Inventory* inventory;
    Object* item;

    if (gInventoryLeftHandItem != nullptr && critter == _inven_dude) {
        return gInventoryLeftHandItem;
    }

    inventory = &(critter->data.inventory);
    for (i = 0; i < inventory->length; i++) {
        item = inventory->items[i].item;
        if (item->flags & OBJECT_IN_LEFT_HAND) {
            return item;
        }
    }

    return nullptr;
}

// 0x471C08
Object* critterGetArmor(Object* critter)
{
    int i;
    Inventory* inventory;
    Object* item;

    if (gInventoryArmor != nullptr && critter == _inven_dude) {
        return gInventoryArmor;
    }

    inventory = &(critter->data.inventory);
    for (i = 0; i < inventory->length; i++) {
        item = inventory->items[i].item;
        if (item->flags & OBJECT_WORN) {
            return item;
        }
    }

    return nullptr;
}

// 0x471CA0
Object* objectGetCarriedObjectByPid(Object* obj, int pid)
{
    Inventory* inventory = &(obj->data.inventory);

    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        if (inventoryItem->item->pid == pid) {
            return inventoryItem->item;
        }

        Object* found = objectGetCarriedObjectByPid(inventoryItem->item, pid);
        if (found != nullptr) {
            return found;
        }
    }

    return nullptr;
}

// 0x471CDC
int objectGetCarriedQuantityByPid(Object* object, int pid)
{
    int quantity = 0;

    Inventory* inventory = &(object->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        if (inventoryItem->item->pid == pid) {
            quantity += inventoryItem->quantity;
        }

        quantity += objectGetCarriedQuantityByPid(inventoryItem->item, pid);
    }

    return quantity;
}

// Renders character's summary of SPECIAL stats, equipped armor bonuses,
// and weapon's damage/range.
//
// 0x471D5C
static void inventoryRenderSummary()
{
    int summaryStats[7];
    memcpy(summaryStats, gSummaryStats, sizeof(summaryStats));

    int summaryStats2[7];
    memcpy(summaryStats2, gSummaryStats2, sizeof(summaryStats2));

    char formattedText[80];

    int oldFont = fontGetCurrent();
    fontSetCurrent(101);

    unsigned char* windowBuffer = windowGetBuffer(gInventoryWindow);

    FrmImage backgroundFrmImage;
    int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, 48, 0, 0, 0);
    if (backgroundFrmImage.lock(backgroundFid)) {
        blitBufferToBuffer(backgroundFrmImage.getData() + INVENTORY_WINDOW_WIDTH * INVENTORY_SUMMARY_Y + INVENTORY_SUMMARY_X,
            152,
            188,
            INVENTORY_WINDOW_WIDTH,
            windowBuffer + INVENTORY_WINDOW_WIDTH * INVENTORY_SUMMARY_Y + INVENTORY_SUMMARY_X,
            INVENTORY_WINDOW_WIDTH);
    }

    // Render character name.
    const char* critterName = critterGetName(_stack[0]);
    fontDrawText(windowBuffer + INVENTORY_WINDOW_WIDTH * INVENTORY_SUMMARY_Y + INVENTORY_SUMMARY_X, critterName, 80, INVENTORY_WINDOW_WIDTH, _colorTable[992]);

    bufferDrawLine(windowBuffer,
        INVENTORY_WINDOW_WIDTH,
        INVENTORY_SUMMARY_X,
        3 * fontGetLineHeight() / 2 + INVENTORY_SUMMARY_Y,
        INVENTORY_SUMMARY_MAX_X,
        3 * fontGetLineHeight() / 2 + INVENTORY_SUMMARY_Y,
        _colorTable[992]);

    MessageListItem messageListItem;

    int offset = INVENTORY_WINDOW_WIDTH * 2 * fontGetLineHeight() + INVENTORY_WINDOW_WIDTH * INVENTORY_SUMMARY_Y + INVENTORY_SUMMARY_X;
    for (int stat = 0; stat < PRIMARY_STAT_COUNT; stat++) {
        messageListItem.num = stat;
        if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
            fontDrawText(windowBuffer + offset, messageListItem.text, 80, INVENTORY_WINDOW_WIDTH, _colorTable[992]);
        }

        int value = critterGetStat(_stack[0], stat);
        snprintf(formattedText, sizeof(formattedText), "%d", value);
        fontDrawText(windowBuffer + offset + 24, formattedText, 80, INVENTORY_WINDOW_WIDTH, _colorTable[992]);

        offset += INVENTORY_WINDOW_WIDTH * fontGetLineHeight();
    }

    offset -= INVENTORY_WINDOW_WIDTH * 7 * fontGetLineHeight();

    for (int index = 0; index < 7; index += 1) {
        messageListItem.num = 7 + index;
        if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
            fontDrawText(windowBuffer + offset + 40, messageListItem.text, 80, INVENTORY_WINDOW_WIDTH, _colorTable[992]);
        }

        if (summaryStats2[index] == -1) {
            int value = critterGetStat(_stack[0], summaryStats[index]);
            snprintf(formattedText, sizeof(formattedText), "   %d", value);
        } else {
            int value1 = critterGetStat(_stack[0], summaryStats[index]);
            int value2 = critterGetStat(_stack[0], summaryStats2[index]);
            const char* format = index != 0 ? "%d/%d%%" : "%d/%d";
            snprintf(formattedText, sizeof(formattedText), format, value1, value2);
        }

        fontDrawText(windowBuffer + offset + 104, formattedText, 80, INVENTORY_WINDOW_WIDTH, _colorTable[992]);

        offset += INVENTORY_WINDOW_WIDTH * fontGetLineHeight();
    }

    bufferDrawLine(windowBuffer, INVENTORY_WINDOW_WIDTH, INVENTORY_SUMMARY_X, 18 * fontGetLineHeight() / 2 + 48, INVENTORY_SUMMARY_MAX_X, 18 * fontGetLineHeight() / 2 + 48, _colorTable[992]);
    bufferDrawLine(windowBuffer, INVENTORY_WINDOW_WIDTH, INVENTORY_SUMMARY_X, 26 * fontGetLineHeight() / 2 + 48, INVENTORY_SUMMARY_MAX_X, 26 * fontGetLineHeight() / 2 + 48, _colorTable[992]);

    Object* itemsInHands[2] = {
        gInventoryLeftHandItem,
        gInventoryRightHandItem,
    };

    const int hitModes[2] = {
        HIT_MODE_LEFT_WEAPON_PRIMARY,
        HIT_MODE_RIGHT_WEAPON_PRIMARY,
    };

    const int secondaryHitModes[2] = {
        HIT_MODE_LEFT_WEAPON_SECONDARY,
        HIT_MODE_RIGHT_WEAPON_SECONDARY,
    };

    const int unarmedHitModes[2] = {
        HIT_MODE_PUNCH,
        HIT_MODE_KICK,
    };

    offset += INVENTORY_WINDOW_WIDTH * fontGetLineHeight();

    for (int index = 0; index < 2; index += 1) {
        Object* item = itemsInHands[index];
        if (item == nullptr) {
            formattedText[0] = '\0';

            // No item
            messageListItem.num = 14;
            if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
                fontDrawText(windowBuffer + offset, messageListItem.text, 120, INVENTORY_WINDOW_WIDTH, _colorTable[992]);
            }

            offset += INVENTORY_WINDOW_WIDTH * fontGetLineHeight();

            // Unarmed dmg:
            messageListItem.num = 24;
            if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
                // SFALL: Display the actual damage values of unarmed attacks.
                // CE: Implementation is different.
                int hitMode = unarmedHitModes[index];
                if (_stack[0] == gDude) {
                    int actions[2];
                    interfaceGetItemActions(&(actions[0]), &(actions[1]));

                    bool isSecondary = actions[index] == INTERFACE_ITEM_ACTION_SECONDARY
                        || actions[index] == INTERFACE_ITEM_ACTION_SECONDARY_AIMING;

                    if (index == HAND_LEFT) {
                        hitMode = unarmedGetPunchHitMode(isSecondary);
                    } else {
                        hitMode = unarmedGetKickHitMode(isSecondary);
                    }
                }

                // Formula is the same as in `weaponGetDamage`.
                int minDamage;
                int maxDamage;
                int bonusDamage = unarmedGetDamage(hitMode, &minDamage, &maxDamage);
                int meleeDamage = critterGetStat(_stack[0], STAT_MELEE_DAMAGE);
                // TODO: Localize unarmed attack names.
                snprintf(formattedText, sizeof(formattedText), "%s %d-%d",
                    messageListItem.text,
                    bonusDamage + minDamage,
                    bonusDamage + meleeDamage + maxDamage);
            }

            fontDrawText(windowBuffer + offset, formattedText, 120, INVENTORY_WINDOW_WIDTH, _colorTable[992]);

            offset += 3 * INVENTORY_WINDOW_WIDTH * fontGetLineHeight();
            continue;
        }

        const char* itemName = itemGetName(item);
        fontDrawText(windowBuffer + offset, itemName, 140, INVENTORY_WINDOW_WIDTH, _colorTable[992]);

        offset += INVENTORY_WINDOW_WIDTH * fontGetLineHeight();

        int itemType = itemGetType(item);
        if (itemType != ITEM_TYPE_WEAPON) {
            if (itemType == ITEM_TYPE_ARMOR) {
                // (Not worn)
                messageListItem.num = 18;
                if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
                    fontDrawText(windowBuffer + offset, messageListItem.text, 120, INVENTORY_WINDOW_WIDTH, _colorTable[992]);
                }
            }

            offset += 3 * INVENTORY_WINDOW_WIDTH * fontGetLineHeight();
            continue;
        }

        // SFALL: Fix displaying secondary mode weapon range.
        int hitMode = hitModes[index];
        if (_stack[0] == gDude) {
            int actions[2];
            interfaceGetItemActions(&(actions[0]), &(actions[1]));

            bool isSecondary = actions[index] == INTERFACE_ITEM_ACTION_SECONDARY
                || actions[index] == INTERFACE_ITEM_ACTION_SECONDARY_AIMING;

            if (isSecondary) {
                hitMode = secondaryHitModes[index];
            }
        }

        int range = weaponGetRange(_stack[0], hitMode);

        int damageMin;
        int damageMax;
        weaponGetDamageMinMax(item, &damageMin, &damageMax);

        // CE: Fix displaying secondary mode weapon damage (affects throwable
        // melee weapons - knifes, spears, etc.).
        int attackType = weaponGetAttackTypeForHitMode(item, hitMode);

        formattedText[0] = '\0';

        int meleeDamage;
        if (attackType == ATTACK_TYPE_MELEE || attackType == ATTACK_TYPE_UNARMED) {
            meleeDamage = critterGetStat(_stack[0], STAT_MELEE_DAMAGE);

            // SFALL: Display melee damage without "Bonus HtH Damage" bonus.
            if (damageModGetBonusHthDamageFix() && !damageModGetDisplayBonusDamage()) {
                meleeDamage -= 2 * perkGetRank(gDude, PERK_BONUS_HTH_DAMAGE);
            }
        } else {
            meleeDamage = 0;
        }

        messageListItem.num = 15; // Dmg:
        if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
            if (attackType != 4 && range <= 1) {
                // SFALL: Display bonus damage.
                if (damageModGetBonusHthDamageFix() && damageModGetDisplayBonusDamage()) {
                    // CE: Just in case check for attack type, however it looks
                    // like we cannot be here with anything besides melee or
                    // unarmed.
                    if (_stack[0] == gDude && (attackType == ATTACK_TYPE_MELEE || attackType == ATTACK_TYPE_UNARMED)) {
                        // See explanation in `weaponGetDamage`.
                        damageMin += 2 * perkGetRank(gDude, PERK_BONUS_HTH_DAMAGE);
                    }
                }
                snprintf(formattedText, sizeof(formattedText), "%s %d-%d", messageListItem.text, damageMin, damageMax + meleeDamage);
            } else {
                MessageListItem rangeMessageListItem;
                rangeMessageListItem.num = 16; // Rng:
                if (messageListGetItem(&gInventoryMessageList, &rangeMessageListItem)) {
                    // SFALL: Display bonus damage.
                    if (damageModGetDisplayBonusDamage()) {
                        // CE: There is a bug in Sfall diplaying wrong damage
                        // bonus for melee weapons with range > 1 (spears,
                        // sledgehammers) and throwables (secondary mode).
                        if (_stack[0] == gDude && attackType == ATTACK_TYPE_RANGED) {
                            int damageBonus = 2 * perkGetRank(gDude, PERK_BONUS_RANGED_DAMAGE);
                            damageMin += damageBonus;
                            damageMax += damageBonus;
                        }
                    }

                    snprintf(formattedText, sizeof(formattedText), "%s %d-%d   %s %d", messageListItem.text, damageMin, damageMax + meleeDamage, rangeMessageListItem.text, range);
                }
            }

            fontDrawText(windowBuffer + offset, formattedText, 140, INVENTORY_WINDOW_WIDTH, _colorTable[992]);
        }

        offset += INVENTORY_WINDOW_WIDTH * fontGetLineHeight();

        if (ammoGetCapacity(item) > 0) {
            int ammoTypePid = weaponGetAmmoTypePid(item);

            formattedText[0] = '\0';

            messageListItem.num = 17; // Ammo:
            if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
                if (ammoTypePid != -1) {
                    if (ammoGetQuantity(item) != 0) {
                        const char* ammoName = protoGetName(ammoTypePid);
                        int capacity = ammoGetCapacity(item);
                        int quantity = ammoGetQuantity(item);
                        snprintf(formattedText, sizeof(formattedText), "%s %d/%d %s", messageListItem.text, quantity, capacity, ammoName);
                    } else {
                        int capacity = ammoGetCapacity(item);
                        int quantity = ammoGetQuantity(item);
                        snprintf(formattedText, sizeof(formattedText), "%s %d/%d", messageListItem.text, quantity, capacity);
                    }
                }
            } else {
                int capacity = ammoGetCapacity(item);
                int quantity = ammoGetQuantity(item);
                snprintf(formattedText, sizeof(formattedText), "%s %d/%d", messageListItem.text, quantity, capacity);
            }

            fontDrawText(windowBuffer + offset, formattedText, 140, INVENTORY_WINDOW_WIDTH, _colorTable[992]);
        }

        offset += 2 * INVENTORY_WINDOW_WIDTH * fontGetLineHeight();
    }

    // Total wt:
    messageListItem.num = 20;
    if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
        if (PID_TYPE(_stack[0]->pid) == OBJ_TYPE_CRITTER) {
            int carryWeight = critterGetStat(_stack[0], STAT_CARRY_WEIGHT);
            int inventoryWeight = objectGetInventoryWeight(_stack[0]);
            snprintf(formattedText, sizeof(formattedText), "%s %d/%d", messageListItem.text, inventoryWeight, carryWeight);

            int color = _colorTable[992];
            if (critterIsEncumbered(_stack[0])) {
                color = _colorTable[31744];
            }

            fontDrawText(windowBuffer + offset + 15, formattedText, 120, INVENTORY_WINDOW_WIDTH, color);
        } else {
            int inventoryWeight = objectGetInventoryWeight(_stack[0]);
            snprintf(formattedText, sizeof(formattedText), "%s %d", messageListItem.text, inventoryWeight);

            fontDrawText(windowBuffer + offset + 30, formattedText, 80, INVENTORY_WINDOW_WIDTH, _colorTable[992]);
        }
    }

    fontSetCurrent(oldFont);
}

// Finds next item of given [itemType] (can be -1 which means any type of
// item).
//
// The [index] is used to control where to continue the search from, -1 - from
// the beginning.
//
// 0x472698
Object* _inven_find_type(Object* obj, int itemType, int* indexPtr)
{
    int dummy = -1;
    if (indexPtr == nullptr) {
        indexPtr = &dummy;
    }

    *indexPtr += 1;

    Inventory* inventory = &(obj->data.inventory);

    // TODO: Refactor with for loop.
    if (*indexPtr >= inventory->length) {
        return nullptr;
    }

    while (itemType != -1 && itemGetType(inventory->items[*indexPtr].item) != itemType) {
        *indexPtr += 1;

        if (*indexPtr >= inventory->length) {
            return nullptr;
        }
    }

    return inventory->items[*indexPtr].item;
}

// Searches for an item with a given id inside given obj's inventory.
//
// 0x4726EC
Object* _inven_find_id(Object* obj, int id)
{
    if (obj->id == id) {
        return obj;
    }

    Inventory* inventory = &(obj->data.inventory);
    for (int index = 0; index < inventory->length; index++) {
        InventoryItem* inventoryItem = &(inventory->items[index]);
        Object* item = inventoryItem->item;
        if (item->id == id) {
            return item;
        }

        if (itemGetType(item) == ITEM_TYPE_CONTAINER) {
            item = _inven_find_id(item, id);
            if (item != nullptr) {
                return item;
            }
        }
    }

    return nullptr;
}

// Returns inventory item at a given index.
//
// 0x472740
Object* _inven_index_ptr(Object* obj, int index)
{
    Inventory* inventory;

    inventory = &(obj->data.inventory);

    if (index < 0 || index >= inventory->length) {
        return nullptr;
    }

    return inventory->items[index].item;
}

// inven_wield
// 0x472758
int _inven_wield(Object* critter, Object* item, int hand)
{
    return _invenWieldFunc(critter, item, hand, true);
}

// 0x472768
int _invenWieldFunc(Object* critter, Object* item, int handIndex, bool animate)
{
    if (animate) {
        if (!isoIsDisabled()) {
            reg_anim_begin(ANIMATION_REQUEST_RESERVED);
        }
    }

    int itemType = itemGetType(item);
    if (itemType == ITEM_TYPE_ARMOR) {
        Object* armor = critterGetArmor(critter);
        if (armor != nullptr) {
            armor->flags &= ~OBJECT_WORN;
        }

        item->flags |= OBJECT_WORN;

        int baseFrmId;
        if (critterGetStat(critter, STAT_GENDER) == GENDER_FEMALE) {
            baseFrmId = armorGetFemaleFid(item);
        } else {
            baseFrmId = armorGetMaleFid(item);
        }

        if (baseFrmId == -1) {
            baseFrmId = 1;
        }

        if (critter == gDude) {
            if (!isoIsDisabled()) {
                int fid = buildFid(OBJ_TYPE_CRITTER, baseFrmId, 0, (critter->fid & 0xF000) >> 12, critter->rotation + 1);
                animationRegisterSetFid(critter, fid, 0);
            }
        } else {
            _adjust_ac(critter, armor, item);
        }
    } else {
        int hand;
        if (critter == gDude) {
            hand = interfaceGetCurrentHand();
        } else {
            hand = HAND_RIGHT;
        }

        int weaponAnimationCode = weaponGetAnimationCode(item);
        int hitModeAnimationCode = weaponGetAnimationForHitMode(item, HIT_MODE_RIGHT_WEAPON_PRIMARY);
        int fid = buildFid(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, hitModeAnimationCode, weaponAnimationCode, critter->rotation + 1);
        if (!artExists(fid)) {
            debugPrint("\ninven_wield failed!  ERROR ERROR ERROR!");
            return -1;
        }

        Object* v17;
        if (handIndex) {
            v17 = critterGetItem2(critter);
            item->flags |= OBJECT_IN_RIGHT_HAND;
        } else {
            v17 = critterGetItem1(critter);
            item->flags |= OBJECT_IN_LEFT_HAND;
        }

        Rect rect;
        if (v17 != nullptr) {
            v17->flags &= ~OBJECT_IN_ANY_HAND;

            if (v17->pid == PROTO_ID_LIT_FLARE) {
                int lightIntensity;
                int lightDistance;
                if (critter == gDude) {
                    lightIntensity = LIGHT_INTENSITY_MAX;
                    lightDistance = 4;
                } else {
                    Proto* proto;
                    if (protoGetProto(critter->pid, &proto) == -1) {
                        return -1;
                    }

                    lightDistance = proto->lightDistance;
                    lightIntensity = proto->lightIntensity;
                }

                objectSetLight(critter, lightDistance, lightIntensity, &rect);
            }
        }

        if (item->pid == PROTO_ID_LIT_FLARE) {
            int lightDistance = item->lightDistance;
            if (lightDistance < critter->lightDistance) {
                lightDistance = critter->lightDistance;
            }

            int lightIntensity = item->lightIntensity;
            if (lightIntensity < critter->lightIntensity) {
                lightIntensity = critter->lightIntensity;
            }

            objectSetLight(critter, lightDistance, lightIntensity, &rect);
            tileWindowRefreshRect(&rect, gElevation);
        }

        if (itemGetType(item) == ITEM_TYPE_WEAPON) {
            weaponAnimationCode = weaponGetAnimationCode(item);
        } else {
            weaponAnimationCode = 0;
        }

        if (hand == handIndex) {
            if ((critter->fid & 0xF000) >> 12 != 0) {
                if (animate) {
                    if (!isoIsDisabled()) {
                        const char* soundEffectName = sfxBuildCharName(critter, ANIM_PUT_AWAY, CHARACTER_SOUND_EFFECT_UNUSED);
                        animationRegisterPlaySoundEffect(critter, soundEffectName, 0);
                        animationRegisterAnimate(critter, ANIM_PUT_AWAY, 0);
                    }
                }
            }

            if (animate && !isoIsDisabled()) {
                if (weaponAnimationCode != 0) {
                    animationRegisterTakeOutWeapon(critter, weaponAnimationCode, -1);
                } else {
                    int fid = buildFid(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, 0, 0, critter->rotation + 1);
                    animationRegisterSetFid(critter, fid, -1);
                }
            } else {
                int fid = buildFid(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, 0, weaponAnimationCode, critter->rotation + 1);
                _dude_stand(critter, critter->rotation, fid);
            }
        }
    }

    if (animate) {
        if (!isoIsDisabled()) {
            return reg_anim_end();
        }
    }

    return 0;
}

// inven_unwield
// 0x472A54
int _inven_unwield(Object* critter_obj, int hand)
{
    return _invenUnwieldFunc(critter_obj, hand, true);
}

// 0x472A64
int _invenUnwieldFunc(Object* critter, int hand, bool animate)
{
    int activeHand;
    Object* item;
    int fid;

    if (critter == gDude) {
        activeHand = interfaceGetCurrentHand();
    } else {
        activeHand = HAND_RIGHT; // NPC's only ever use right slot
    }

    if (hand) {
        item = critterGetItem2(critter);
    } else {
        item = critterGetItem1(critter);
    }

    if (item) {
        item->flags &= ~OBJECT_IN_ANY_HAND;
    }

    if (activeHand == hand && ((critter->fid & 0xF000) >> 12) != 0) {
        if (animate && !isoIsDisabled()) {
            reg_anim_begin(ANIMATION_REQUEST_RESERVED);

            const char* sfx = sfxBuildCharName(critter, ANIM_PUT_AWAY, CHARACTER_SOUND_EFFECT_UNUSED);
            animationRegisterPlaySoundEffect(critter, sfx, 0);

            animationRegisterAnimate(critter, ANIM_PUT_AWAY, 0);

            fid = buildFid(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, 0, 0, critter->rotation + 1);
            animationRegisterSetFid(critter, fid, -1);

            return reg_anim_end();
        }

        fid = buildFid(OBJ_TYPE_CRITTER, critter->fid & 0xFFF, 0, 0, critter->rotation + 1);
        _dude_stand(critter, critter->rotation, fid);
    }

    return 0;
}

// 0x472B54
static int _inven_from_button(int keyCode, Object** outItem, Object*** outItemSlot, Object** outOwner)
{
    Object** itemSlot;
    Object* owner;
    Object* item;
    int quantity = 0;

    switch (keyCode) {
    case 1006:
        itemSlot = &gInventoryRightHandItem;
        owner = _stack[0];
        item = gInventoryRightHandItem;
        break;
    case 1007:
        itemSlot = &gInventoryLeftHandItem;
        owner = _stack[0];
        item = gInventoryLeftHandItem;
        break;
    case 1008:
        itemSlot = &gInventoryArmor;
        owner = _stack[0];
        item = gInventoryArmor;
        break;
    default:
        itemSlot = nullptr;
        owner = nullptr;
        item = nullptr;

        InventoryItem* inventoryItem;
        if (keyCode < 2000) {
            int index = _stack_offset[_curr_stack] + keyCode - 1000;
            if (index >= _pud->length) {
                break;
            }

            inventoryItem = &(_pud->items[_pud->length - (index + 1)]);
            item = inventoryItem->item;
            owner = _stack[_curr_stack];
        } else if (keyCode < 2300) {
            int index = _target_stack_offset[_target_curr_stack] + keyCode - 2000;
            if (index >= _target_pud->length) {
                break;
            }

            inventoryItem = &(_target_pud->items[_target_pud->length - (index + 1)]);
            item = inventoryItem->item;
            owner = _target_stack[_target_curr_stack];
        } else if (keyCode < 2400) {
            int index = _ptable_offset + keyCode - 2300;
            if (index >= _ptable_pud->length) {
                break;
            }

            inventoryItem = &(_ptable_pud->items[_ptable_pud->length - (index + 1)]);
            item = inventoryItem->item;
            owner = _ptable;
        } else {
            int index = _btable_offset + keyCode - 2400;
            if (index >= _btable_pud->length) {
                break;
            }

            inventoryItem = &(_btable_pud->items[_btable_pud->length - (index + 1)]);
            item = inventoryItem->item;
            owner = _btable;
        }

        quantity = inventoryItem->quantity;
    }

    if (outItemSlot != nullptr) {
        *outItemSlot = itemSlot;
    }

    if (outItem != nullptr) {
        *outItem = item;
    }

    if (outOwner != nullptr) {
        *outOwner = owner;
    }

    if (quantity == 0 && item != nullptr) {
        quantity = 1;
    }

    return quantity;
}

// Displays item description.
//
// The [string] is mutated in the process replacing spaces back and forth
// for word wrapping purposes.
//
// inven_display_msg
// 0x472D24
void inventoryRenderItemDescription(char* string)
{
    // Realtime: a copy -- the word wrapping below writes into the text, and the realtime modules pass
    // string literals (inven_display_msg). See displayMonitorAddMessage, the same crash on the Deck.
    char copy[1024];
    if (string != nullptr) {
        strncpy(copy, string, sizeof(copy) - 1);
        copy[sizeof(copy) - 1] = '\0';
        string = copy;
    }

    int oldFont = fontGetCurrent();
    fontSetCurrent(101);

    unsigned char* windowBuffer = windowGetBuffer(gInventoryWindow);
    windowBuffer += INVENTORY_WINDOW_WIDTH * INVENTORY_SUMMARY_Y + INVENTORY_SUMMARY_X;

    char* c = string;
    while (c != nullptr && *c != '\0') {
        _inven_display_msg_line += 1;
        if (_inven_display_msg_line > 17) {
            debugPrint("\nError: inven_display_msg: out of bounds!");
            return;
        }

        char* space = nullptr;
        if (fontGetStringWidth(c) > 152) {
            // Look for next space.
            space = c + 1;
            while (*space != '\0' && *space != ' ') {
                space += 1;
            }

            if (*space == '\0') {
                // This was the last line containing very long word. Text
                // drawing routine will silently truncate it after reaching
                // desired length.
                fontDrawText(windowBuffer + INVENTORY_WINDOW_WIDTH * _inven_display_msg_line * fontGetLineHeight(), c, 152, INVENTORY_WINDOW_WIDTH, _colorTable[992]);
                return;
            }

            char* nextSpace = space + 1;
            while (true) {
                while (*nextSpace != '\0' && *nextSpace != ' ') {
                    nextSpace += 1;
                }

                if (*nextSpace == '\0') {
                    break;
                }

                // Break string and measure it.
                *nextSpace = '\0';
                if (fontGetStringWidth(c) >= 152) {
                    // Next space is too far to fit in one line. Restore next
                    // space's character and stop.
                    *nextSpace = ' ';
                    break;
                }

                space = nextSpace;

                // Restore next space's character and continue looping from the
                // next character.
                *nextSpace = ' ';
                nextSpace += 1;
            }

            if (*space == ' ') {
                *space = '\0';
            }
        }

        if (fontGetStringWidth(c) > 152) {
            debugPrint("\nError: inven_display_msg: word too long!");
            return;
        }

        fontDrawText(windowBuffer + INVENTORY_WINDOW_WIDTH * _inven_display_msg_line * fontGetLineHeight(), c, 152, INVENTORY_WINDOW_WIDTH, _colorTable[992]);

        if (space != nullptr) {
            c = space + 1;
            if (*space == '\0') {
                *space = ' ';
            }
        } else {
            c = nullptr;
        }
    }

    fontSetCurrent(oldFont);
}

// Examines inventory item.
//
// 0x472EB8
static void inventoryExamineItem(Object* critter, Object* item)
{
    int oldFont = fontGetCurrent();
    fontSetCurrent(101);

    unsigned char* windowBuffer = windowGetBuffer(gInventoryWindow);

    // Clear item description area.
    FrmImage backgroundFrmImage;
    int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, 48, 0, 0, 0);
    if (backgroundFrmImage.lock(backgroundFid)) {
        blitBufferToBuffer(backgroundFrmImage.getData() + INVENTORY_WINDOW_WIDTH * INVENTORY_SUMMARY_Y + INVENTORY_SUMMARY_X,
            152,
            188,
            INVENTORY_WINDOW_WIDTH,
            windowBuffer + INVENTORY_WINDOW_WIDTH * INVENTORY_SUMMARY_Y + INVENTORY_SUMMARY_X,
            INVENTORY_WINDOW_WIDTH);
    }

    // Reset item description lines counter.
    _inven_display_msg_line = 0;

    // Render item's name.
    char* itemName = objectGetName(item);
    inventoryRenderItemDescription(itemName);

    // Increment line counter to accomodate separator below.
    _inven_display_msg_line += 1;

    int lineHeight = fontGetLineHeight();

    // Draw separator.
    // SFALL: Fix separator position when item name is longer than one line.
    bufferDrawLine(windowBuffer,
        INVENTORY_WINDOW_WIDTH,
        INVENTORY_SUMMARY_X,
        (_inven_display_msg_line - 1) * lineHeight + lineHeight / 2 + 49,
        INVENTORY_SUMMARY_MAX_X,
        (_inven_display_msg_line - 1) * lineHeight + lineHeight / 2 + 49,
        _colorTable[992]);

    // Examine item.
    _obj_examine_func(critter, item, inventoryRenderItemDescription);

    // Add weight if neccessary.
    int weight = itemGetWeight(item);
    if (weight != 0) {
        MessageListItem messageListItem;
        messageListItem.num = 540;

        if (weight == 1) {
            messageListItem.num = 541;
        }

        if (!messageListGetItem(&gProtoMessageList, &messageListItem)) {
            debugPrint("\nError: Couldn't find message!");
        }

        char formattedText[40];
        snprintf(formattedText, sizeof(formattedText), messageListItem.text, weight);
        inventoryRenderItemDescription(formattedText);
    }

    fontSetCurrent(oldFont);
}

// 0x47304C
static void inventoryWindowOpenContextMenu(int keyCode, int inventoryWindowType)
{
    Object* item;
    Object** itemSlot;
    Object* owner;

    int quantity = _inven_from_button(keyCode, &item, &itemSlot, &owner);
    if (quantity == 0) {
        return;
    }

    int itemType = itemGetType(item);

    int mouseState;
    do {
        sharedFpsLimiter.mark();

        inputGetInput();

        if (inventoryWindowType == INVENTORY_WINDOW_TYPE_NORMAL) {
            _display_body(-1, INVENTORY_WINDOW_TYPE_NORMAL);
        }

        mouseState = mouseGetEvent();
        if ((mouseState & MOUSE_EVENT_LEFT_BUTTON_UP) != 0) {
            if (inventoryWindowType != INVENTORY_WINDOW_TYPE_NORMAL) {
                _obj_look_at_func(_stack[0], item, gInventoryPrintItemDescriptionHandler);
            } else {
                inventoryExamineItem(_stack[0], item);
            }
            windowRefresh(gInventoryWindow);
            return;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    } while ((mouseState & MOUSE_EVENT_LEFT_BUTTON_DOWN_REPEAT) != MOUSE_EVENT_LEFT_BUTTON_DOWN_REPEAT);

    inventorySetCursor(INVENTORY_WINDOW_CURSOR_BLANK);

    unsigned char* windowBuffer = windowGetBuffer(gInventoryWindow);

    int x;
    int y;
    mouseGetPosition(&x, &y);

    int actionMenuItemsLength;
    const int* actionMenuItems;
    if (itemType == ITEM_TYPE_WEAPON && weaponCanBeUnloaded(item)) {
        if (inventoryWindowType != INVENTORY_WINDOW_TYPE_NORMAL && objectGetOwner(item) != gDude) {
            actionMenuItemsLength = 3;
            actionMenuItems = _act_weap2;
        } else {
            actionMenuItemsLength = 4;
            actionMenuItems = _act_weap;
        }
    } else {
        if (inventoryWindowType != INVENTORY_WINDOW_TYPE_NORMAL) {
            // SFALL: Fix crash when trying to open bag/backpack on the table
            // in the bartering interface.
            Object* owner = objectGetOwner(item);
            if (owner != gDude) {
                if (itemType == ITEM_TYPE_CONTAINER && (owner == _stack[_curr_stack] || owner == _target_stack[_target_curr_stack])) {
                    actionMenuItemsLength = 3;
                    actionMenuItems = _act_just_use;
                } else {
                    actionMenuItemsLength = 2;
                    actionMenuItems = _act_nothing;
                }
            } else {
                if (itemType == ITEM_TYPE_CONTAINER) {
                    actionMenuItemsLength = 4;
                    actionMenuItems = _act_use;
                } else {
                    actionMenuItemsLength = 3;
                    actionMenuItems = _act_no_use;
                }
            }
        } else {
            if (itemType == ITEM_TYPE_CONTAINER && itemSlot != nullptr) {
                actionMenuItemsLength = 3;
                actionMenuItems = _act_no_use;
            } else {
                if (_obj_action_can_use(item) || _proto_action_can_use_on(item->pid)) {
                    actionMenuItemsLength = 4;
                    actionMenuItems = _act_use;
                } else {
                    actionMenuItemsLength = 3;
                    actionMenuItems = _act_no_use;
                }
            }
        }
    }

    const InventoryWindowDescription* windowDescription = &(gInventoryWindowDescriptions[inventoryWindowType]);

    Rect windowRect;
    windowGetRect(gInventoryWindow, &windowRect);
    int inventoryWindowX = windowRect.left;
    int inventoryWindowY = windowRect.top;

    gameMouseRenderActionMenuItems(x, y, actionMenuItems, actionMenuItemsLength,
        windowDescription->width + inventoryWindowX,
        windowDescription->height + inventoryWindowY);

    InventoryCursorData* cursorData = &(gInventoryCursorData[INVENTORY_WINDOW_CURSOR_MENU]);

    int offsetX;
    int offsetY;
    artGetRotationOffsets(cursorData->frm, 0, &offsetX, &offsetY);

    Rect rect;
    rect.left = x - inventoryWindowX - cursorData->width / 2 + offsetX;
    rect.top = y - inventoryWindowY - cursorData->height + 1 + offsetY;
    rect.right = rect.left + cursorData->width - 1;
    rect.bottom = rect.top + cursorData->height - 1;

    int menuButtonHeight = cursorData->height;
    if (rect.top + menuButtonHeight > windowDescription->height) {
        menuButtonHeight = windowDescription->height - rect.top;
    }

    int btn = buttonCreate(gInventoryWindow,
        rect.left,
        rect.top,
        cursorData->width,
        menuButtonHeight,
        -1,
        -1,
        -1,
        -1,
        cursorData->frmData,
        cursorData->frmData,
        nullptr,
        BUTTON_FLAG_TRANSPARENT);
    windowRefreshRect(gInventoryWindow, &rect);

    int menuItemIndex = 0;
    int previousMouseY = y;
    // The gamepad layer feeds this hold-drag popup (the left stick moves mouse Y).
    gamepad_ui_set_action_popup_active(true);
    while ((mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_UP) == 0) {
        sharedFpsLimiter.mark();

        inputGetInput();

        if (inventoryWindowType == INVENTORY_WINDOW_TYPE_NORMAL) {
            _display_body(-1, INVENTORY_WINDOW_TYPE_NORMAL);
        }

        // D-pad up/down steps the highlighted option directly; A (left-button up) confirms.
        int dpad = gamepad_ui_action_popup_dpad();
        if (dpad < 0 && menuItemIndex > 0) {
            menuItemIndex--;
            gameMouseHighlightActionMenuItemAtIndex(menuItemIndex);
            windowRefreshRect(gInventoryWindow, &rect);
        } else if (dpad > 0 && menuItemIndex < actionMenuItemsLength - 1) {
            menuItemIndex++;
            gameMouseHighlightActionMenuItemAtIndex(menuItemIndex);
            windowRefreshRect(gInventoryWindow, &rect);
        }

        int x;
        int y;
        mouseGetPosition(&x, &y);
        if (y - previousMouseY > 10 || previousMouseY - y > 10) {
            if (y >= previousMouseY || menuItemIndex <= 0) {
                if (previousMouseY < y && menuItemIndex < actionMenuItemsLength - 1) {
                    menuItemIndex++;
                }
            } else {
                menuItemIndex--;
            }
            gameMouseHighlightActionMenuItemAtIndex(menuItemIndex);
            windowRefreshRect(gInventoryWindow, &rect);
            previousMouseY = y;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }
    gamepad_ui_set_action_popup_active(false);

    buttonDestroy(btn);

    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_TRADE) {
        unsigned char* src = windowGetBuffer(_barter_back_win);
        int pitch = INVENTORY_TRADE_BACKGROUND_WINDOW_WIDTH;
        blitBufferToBuffer(src + pitch * rect.top + rect.left + INVENTORY_TRADE_WINDOW_OFFSET,
            cursorData->width,
            menuButtonHeight,
            pitch,
            windowBuffer + windowDescription->width * rect.top + rect.left,
            windowDescription->width);
    } else {
        FrmImage backgroundFrmImage;
        int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, windowDescription->frmId, 0, 0, 0);
        if (backgroundFrmImage.lock(backgroundFid)) {
            blitBufferToBuffer(backgroundFrmImage.getData() + windowDescription->width * rect.top + rect.left,
                cursorData->width,
                menuButtonHeight,
                windowDescription->width,
                windowBuffer + windowDescription->width * rect.top + rect.left,
                windowDescription->width);
        }
    }

    _mouse_set_position(x, y);

    _display_inventory(_stack_offset[_curr_stack], -1, inventoryWindowType);

    int actionMenuItem = actionMenuItems[menuItemIndex];
    switch (actionMenuItem) {
    case GAME_MOUSE_ACTION_MENU_ITEM_DROP:
        if (itemSlot != nullptr) {
            if (itemSlot == &gInventoryArmor) {
                _adjust_ac(_stack[0], item, nullptr);
            }
            itemAdd(owner, item, 1);
            quantity = 1;
            *itemSlot = nullptr;
        }

        if (item->pid == PROTO_ID_MONEY) {
            if (quantity > 1) {
                quantity = inventoryQuantitySelect(INVENTORY_WINDOW_TYPE_MOVE_ITEMS, item, quantity);
            } else {
                quantity = 1;
            }

            if (quantity > 0) {
                if (quantity == 1) {
                    itemSetMoney(item, 1);
                    _obj_drop(owner, item);
                } else {
                    if (itemRemove(owner, item, quantity - 1) == 0) {
                        Object* item2;
                        if (_inven_from_button(keyCode, &item2, &itemSlot, &owner) != 0) {
                            itemSetMoney(item2, quantity);
                            _obj_drop(owner, item2);
                        } else {
                            itemAdd(owner, item, quantity - 1);
                        }
                    }
                }
            }
        } else if (explosiveIsActiveExplosive(item->pid)) {
            _dropped_explosive = 1;
            _obj_drop(owner, item);
        } else {
            if (quantity > 1) {
                quantity = inventoryQuantitySelect(INVENTORY_WINDOW_TYPE_MOVE_ITEMS, item, quantity);

                for (int index = 0; index < quantity; index++) {
                    if (_inven_from_button(keyCode, &item, &itemSlot, &owner) != 0) {
                        _obj_drop(owner, item);
                    }
                }
            } else {
                _obj_drop(owner, item);
            }
        }
        break;
    case GAME_MOUSE_ACTION_MENU_ITEM_LOOK:
        if (inventoryWindowType != INVENTORY_WINDOW_TYPE_NORMAL) {
            _obj_examine_func(_stack[0], item, gInventoryPrintItemDescriptionHandler);
        } else {
            inventoryExamineItem(_stack[0], item);
        }
        break;
    case GAME_MOUSE_ACTION_MENU_ITEM_USE:
        switch (itemType) {
        case ITEM_TYPE_CONTAINER:
            _container_enter(keyCode, inventoryWindowType);
            break;
        case ITEM_TYPE_DRUG:
            if (_item_d_take_drug(_stack[0], item)) {
                if (itemSlot != nullptr) {
                    *itemSlot = nullptr;
                } else {
                    itemRemove(owner, item, 1);
                }

                _obj_connect(item, gDude->tile, gDude->elevation, nullptr);
                _obj_destroy(item);
            }
            interfaceRenderHitPoints(true);
            break;
        case ITEM_TYPE_WEAPON:
        case ITEM_TYPE_MISC:
            if (itemSlot == nullptr) {
                itemRemove(owner, item, 1);
            }

            int useResult;
            if (_obj_action_can_use(item)) {
                useResult = _protinst_use_item(_stack[0], item);
            } else {
                useResult = _protinst_use_item_on(_stack[0], _stack[0], item);
            }

            if (useResult == 1) {
                if (itemSlot != nullptr) {
                    *itemSlot = nullptr;
                }

                _obj_connect(item, gDude->tile, gDude->elevation, nullptr);
                _obj_destroy(item);
            } else {
                if (itemSlot == nullptr) {
                    itemAdd(owner, item, 1);
                }
                inven_place_if_requested(item, owner, itemSlot);
            }
        }
        break;
    case GAME_MOUSE_ACTION_MENU_ITEM_UNLOAD:
        if (itemSlot == nullptr) {
            itemRemove(owner, item, 1);
        }

        for (;;) {
            Object* ammo = weaponUnload(item);
            if (ammo == nullptr) {
                break;
            }

            Rect rect;
            _obj_disconnect(ammo, &rect);
            itemAdd(owner, ammo, 1);
        }

        if (itemSlot == nullptr) {
            itemAdd(owner, item, 1);
        }
        break;
    default:
        break;
    }

    inventorySetCursor(INVENTORY_WINDOW_CURSOR_ARROW);

    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_NORMAL && actionMenuItem != GAME_MOUSE_ACTION_MENU_ITEM_LOOK) {
        inventoryRenderSummary();
    }

    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_LOOT
        || inventoryWindowType == INVENTORY_WINDOW_TYPE_TRADE) {
        _display_target_inventory(_target_stack_offset[_target_curr_stack], -1, _target_pud, inventoryWindowType);
    }

    _display_inventory(_stack_offset[_curr_stack], -1, inventoryWindowType);

    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_TRADE) {
        inventoryWindowRenderInnerInventories(_barter_back_win, _ptable, _btable, -1);
    }

    _adjust_fid();
}

// CE: loot-window panel jump. Pushing the left stick / D-pad LEFT snaps the pointer to the
// first (top) slot of YOUR inventory (left panel); RIGHT snaps it to the first slot of the
// container/corpse (right panel). Up/down within a panel is still the shared cursor snap
// (gpui_menu_snap); this only overrides the left/right cross-panel jump so entering a side
// always lands on its first item instead of whatever row was nearest. Read SDL directly --
// gamepad_ui_tick doesn't run inside inventoryOpenLooting's modal loop. Runs after inputGetInput, so its
// warp wins over gpui's same-frame horizontal snap.
static void loot_gamepad_panel_jump(int gInventoryWindow)
{
    static bool prevL = false;
    static bool prevR = false;

    SDL_GameController* pad = gamepad_ui_get_controller();
    if (pad == NULL) {
        prevL = false;
        prevR = false;
        return;
    }

    SDL_GameControllerUpdate();
    Sint16 lx = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
    bool lNow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0 || lx < -16000;
    bool rNow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0 || lx > 16000;

    Rect winRect;
    windowGetRect(gInventoryWindow, &winRect);

    if (lNow && !prevL) {
        barter_warp_cursor(winRect.left + INVENTORY_LOOT_LEFT_SCROLLER_X + INVENTORY_SLOT_WIDTH / 2,
            winRect.top + INVENTORY_LOOT_LEFT_SCROLLER_Y + INVENTORY_SLOT_HEIGHT / 2);
    } else if (rNow && !prevR) {
        barter_warp_cursor(winRect.left + INVENTORY_LOOT_RIGHT_SCROLLER_X + INVENTORY_SLOT_WIDTH / 2,
            winRect.top + INVENTORY_LOOT_RIGHT_SCROLLER_Y + INVENTORY_SLOT_HEIGHT / 2);
    }

    prevL = lNow;
    prevR = rNow;
}

// 0x473904
// CE: A PILE OF BODIES IS LOOTED FROM ONE WINDOW (fallout1-ce 2026-10-22, "jak vymyslíme lootování mnoho
// mrtvých postav na jednom místě?" -> his pick "vzít vše + další mrtvola"). Vanilla's arrows beside the
// body (KEY_PAGE_UP / KEY_PAGE_DOWN, LB / RB on the pad) only stepped through critters dead or out cold
// on the SAME TILE as the one opened. They now also reach every dead body that still carries something
// within INVENTORY_LOOT_NEARBY_TILES of the dude and in his sight (interact_prompt_sees: nothing behind a
// wall), nearest first, and skip the ones already emptied. Under the body, "2/5" says where you are.
// Each body is still its own inventory, as in vanilla.
#define INVENTORY_LOOT_NEARBY_TILES 3
#define INVENTORY_LOOT_BODIES_MAX 16
static int loot_body_index = 0;
static int loot_body_count = 0; // under 2: no counter

// The body opened first, then vanilla's same-tile ones and the nearby ones, nearest first.
static int loot_collect_bodies(Object* first, Object** out, int max)
{
    int count = 0;
    out[count++] = first;

    for (Object* obj = objectFindFirstAtElevation(first->elevation); obj != nullptr && count < max; obj = objectFindNextAtElevation()) {
        if (obj == first || PID_TYPE(obj->pid) != OBJ_TYPE_CRITTER || (obj->flags & OBJECT_HIDDEN) != 0) {
            continue;
        }
        int results = obj->data.critter.combat.results;
        bool sameTile = obj->tile == first->tile && (results & (DAM_DEAD | DAM_KNOCKED_OUT)) != 0;
        bool nearby = (results & DAM_DEAD) != 0
            && obj->data.inventory.length > 0
            && objectGetDistanceBetween(_inven_dude, obj) <= INVENTORY_LOOT_NEARBY_TILES
            && interact_prompt_sees(obj);
        if (sameTile || nearby) {
            out[count++] = obj;
        }
    }

    for (int i = 2; i < count; i++) {
        Object* body = out[i];
        int dist = objectGetDistanceBetween(_inven_dude, body);
        int j = i;
        while (j > 1 && objectGetDistanceBetween(_inven_dude, out[j - 1]) > dist) {
            out[j] = out[j - 1];
            j--;
        }
        out[j] = body;
    }

    return count;
}

// From `from` toward `dir`, the next body that still carries something. The first time a body is met
// its pickup procedure runs, as opening it would have run it, and one whose script overrides that is
// passed over from then on (state 2). Returns `from` when there is no other.
static int loot_next_body(Object** bodies, signed char* state, int count, int from, int dir)
{
    for (int step = 1; step < count; step++) {
        int index = ((from + dir * step) % count + count) % count;
        Object* body = bodies[index];
        if (state[index] == 0) {
            state[index] = 1;
            int sid;
            if (_obj_sid(body, &sid) != -1) {
                scriptSetObjects(sid, _inven_dude, nullptr);
                scriptExecProc(sid, SCRIPT_PROC_PICKUP);
                Script* script;
                if (scriptGetScript(sid, &script) != -1 && script->scriptOverrides) {
                    state[index] = 2;
                }
            }
        }
        if (state[index] == 1 && body->data.inventory.length > 0) {
            return index;
        }
    }
    return from;
}

// "2/5" along the bottom of the body's picture (where a lying body never reaches). Fallout 2 draws the
// looted body at 426,39 (_display_body).
static void loot_draw_body_counter()
{
    if (loot_body_count < 2) {
        return;
    }

    unsigned char* windowBuffer = windowGetBuffer(gInventoryWindow);
    int pitch = windowGetWidth(gInventoryWindow);

    int oldFont = fontGetCurrent();
    fontSetCurrent(101);

    int x = 426;
    int y = 39 + INVENTORY_BODY_VIEW_HEIGHT - fontGetLineHeight() - 1;

    FrmImage backgroundFrmImage;
    if (backgroundFrmImage.lock(buildFid(OBJ_TYPE_INTERFACE, 114, 0, 0, 0))) {
        blitBufferToBuffer(backgroundFrmImage.getData() + INVENTORY_LOOT_WINDOW_WIDTH * y + x,
            INVENTORY_BODY_VIEW_WIDTH,
            fontGetLineHeight(),
            INVENTORY_LOOT_WINDOW_WIDTH,
            windowBuffer + pitch * y + x,
            pitch);
    }

    char text[16];
    snprintf(text, sizeof(text), "%d/%d", loot_body_index + 1, loot_body_count);
    int width = fontGetStringWidth(text);
    fontDrawText(windowBuffer + pitch * y + x + (INVENTORY_BODY_VIEW_WIDTH - width) / 2, text, width, pitch, _colorTable[992]);

    Rect rect;
    rect.left = x;
    rect.top = y;
    rect.right = x + INVENTORY_BODY_VIEW_WIDTH - 1;
    rect.bottom = y + fontGetLineHeight() - 1;
    windowRefreshRect(gInventoryWindow, &rect);

    fontSetCurrent(oldFont);
}

int inventoryOpenLooting(Object* looter, Object* target)
{
    int arrowFrmIds[INVENTORY_ARROW_FRM_COUNT];
    FrmImage arrowFrmImages[INVENTORY_ARROW_FRM_COUNT];
    MessageListItem messageListItem;

    memcpy(arrowFrmIds, gInventoryArrowFrmIds, sizeof(gInventoryArrowFrmIds));

    if (looter != _inven_dude) {
        return 0;
    }

    ScopedGameMode gm(GameMode::kLoot);

    if (FID_TYPE(target->fid) == OBJ_TYPE_CRITTER) {
        if (_critter_flag_check(target->pid, CRITTER_NO_STEAL)) {
            // You can't find anything to take from that.
            messageListItem.num = 50;
            if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
                displayMonitorAddMessage(messageListItem.text);
            }
            return 0;
        }
    }

    if (FID_TYPE(target->fid) == OBJ_TYPE_ITEM) {
        if (itemGetType(target) == ITEM_TYPE_CONTAINER) {
            if (target->frame == 0) {
                CacheEntry* handle;
                Art* frm = artLock(target->fid, &handle);
                if (frm != nullptr) {
                    int frameCount = artGetFrameCount(frm);
                    artUnlock(handle);
                    if (frameCount > 1) {
                        return 0;
                    }
                }
            }
        }
    }

    int sid = -1;
    if (!_gIsSteal) {
        if (_obj_sid(target, &sid) != -1) {
            scriptSetObjects(sid, looter, nullptr);
            scriptExecProc(sid, SCRIPT_PROC_PICKUP);

            Script* script;
            if (scriptGetScript(sid, &script) != -1) {
                if (script->scriptOverrides) {
                    return 0;
                }
            }
        }
    }

    if (inventoryCommonInit() == -1) {
        return 0;
    }

    _target_pud = &(target->data.inventory);
    _target_curr_stack = 0;
    _target_stack_offset[0] = 0;
    _target_stack[0] = target;

    Object* hiddenBox = nullptr;
    if (objectCreateWithFidPid(&hiddenBox, 0, PROTO_ID_JESSE_CONTAINER) == -1) {
        return 0;
    }

    itemMoveAllHidden(target, hiddenBox);

    Object* item1 = nullptr;
    Object* item2 = nullptr;
    Object* armor = nullptr;

    if (_gIsSteal) {
        item1 = critterGetItem1(target);
        if (item1 != nullptr) {
            itemRemove(target, item1, 1);
        }

        item2 = critterGetItem2(target);
        if (item2 != nullptr) {
            itemRemove(target, item2, 1);
        }

        armor = critterGetArmor(target);
        if (armor != nullptr) {
            itemRemove(target, armor, 1);
        }
    }

    bool isoWasEnabled = _setup_inventory(INVENTORY_WINDOW_TYPE_LOOT);

    // CE: the bodies the arrows step through -- see loot_collect_bodies. The one opened is first.
    Object* critters[INVENTORY_LOOT_BODIES_MAX];
    signed char critterState[INVENTORY_LOOT_BODIES_MAX]; // see loot_next_body
    int critterCount = 0;
    int critterIndex = 0;
    if (!_gIsSteal) {
        if (FID_TYPE(target->fid) == OBJ_TYPE_CRITTER) {
            critterCount = loot_collect_bodies(target, critters, INVENTORY_LOOT_BODIES_MAX);
            for (int index = 0; index < critterCount; index++) {
                critterState[index] = index == 0 ? 1 : 0; // its pickup procedure ran above
            }

            if (critterCount == 1) {
                critterCount = 0;
            }

            if (critterCount > 1) {
                int fid;
                int btn;

                // Setup left arrow button.
                fid = buildFid(OBJ_TYPE_INTERFACE, arrowFrmIds[INVENTORY_ARROW_FRM_LEFT_ARROW_UP], 0, 0, 0);
                arrowFrmImages[INVENTORY_ARROW_FRM_LEFT_ARROW_UP].lock(fid);

                fid = buildFid(OBJ_TYPE_INTERFACE, arrowFrmIds[INVENTORY_ARROW_FRM_LEFT_ARROW_DOWN], 0, 0, 0);
                arrowFrmImages[INVENTORY_ARROW_FRM_LEFT_ARROW_DOWN].lock(fid);

                if (arrowFrmImages[INVENTORY_ARROW_FRM_LEFT_ARROW_UP].isLocked() && arrowFrmImages[INVENTORY_ARROW_FRM_LEFT_ARROW_DOWN].isLocked()) {
                    btn = buttonCreate(gInventoryWindow,
                        436,
                        162,
                        20,
                        18,
                        -1,
                        -1,
                        KEY_PAGE_UP,
                        -1,
                        arrowFrmImages[INVENTORY_ARROW_FRM_LEFT_ARROW_UP].getData(),
                        arrowFrmImages[INVENTORY_ARROW_FRM_LEFT_ARROW_DOWN].getData(),
                        nullptr,
                        0);
                    if (btn != -1) {
                        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
                    }
                }

                // Setup right arrow button.
                fid = buildFid(OBJ_TYPE_INTERFACE, arrowFrmIds[INVENTORY_ARROW_FRM_RIGHT_ARROW_UP], 0, 0, 0);
                arrowFrmImages[INVENTORY_ARROW_FRM_RIGHT_ARROW_UP].lock(fid);

                fid = buildFid(OBJ_TYPE_INTERFACE, arrowFrmIds[INVENTORY_ARROW_FRM_RIGHT_ARROW_DOWN], 0, 0, 0);
                arrowFrmImages[INVENTORY_ARROW_FRM_RIGHT_ARROW_DOWN].lock(fid);

                if (arrowFrmImages[INVENTORY_ARROW_FRM_RIGHT_ARROW_UP].isLocked() && arrowFrmImages[INVENTORY_ARROW_FRM_RIGHT_ARROW_DOWN].isLocked()) {
                    btn = buttonCreate(gInventoryWindow,
                        456,
                        162,
                        20,
                        18,
                        -1,
                        -1,
                        KEY_PAGE_DOWN,
                        -1,
                        arrowFrmImages[INVENTORY_ARROW_FRM_RIGHT_ARROW_UP].getData(),
                        arrowFrmImages[INVENTORY_ARROW_FRM_RIGHT_ARROW_DOWN].getData(),
                        nullptr,
                        0);
                    if (btn != -1) {
                        buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
                    }
                }

            }
        }
    }

    _display_target_inventory(_target_stack_offset[_target_curr_stack], -1, _target_pud, INVENTORY_WINDOW_TYPE_LOOT);
    _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_LOOT);
    display_body_force = true;
    _display_body(target->fid, INVENTORY_WINDOW_TYPE_LOOT);
    loot_body_index = 0;
    loot_body_count = critterCount;
    loot_draw_body_counter();
    trace_event(TRACE_ACTION, "loot-open", "id=%d pid=%d bodies=%d items=%d", target->id, target->pid, critterCount, _target_pud->length);
    inventorySetCursor(INVENTORY_WINDOW_CURSOR_HAND);

    // CE: ON A CONTROLLER, START ON THE LOOT ITSELF -- the first slot of the CONTAINER side is the only
    // thing anyone opens this window for. Never under a mouse player's hand.
    if (gamepad_ui_get_controller() != nullptr) {
        Rect lootRect;
        windowGetRect(gInventoryWindow, &lootRect);
        barter_warp_cursor(lootRect.left + INVENTORY_LOOT_RIGHT_SCROLLER_X + INVENTORY_SLOT_WIDTH / 2,
            lootRect.top + INVENTORY_LOOT_RIGHT_SCROLLER_Y + INVENTORY_SLOT_HEIGHT / 2);
    }

    bool isCaughtStealing = false;
    int stealingXp = 0;
    int stealingXpBonus = 10;
    for (;;) {
        sharedFpsLimiter.mark();

        if (_game_user_wants_to_quit != 0) {
            break;
        }

        if (isCaughtStealing) {
            break;
        }

        int keyCode = inputGetInput();

        // The pad's D-pad in the LOOT window: it STEPS THE ITEMS of the panel the cursor is over first,
        // and scrolls only at the ends ("začne se na prvním, a pak postupně druhý třetí čtvrtý, a pak
        // d-pad posouvá na další"). The cursor IS the selection here, so stepping parks it one slot on.
        if (keyCode == -1) {
            int padKey = inven_gamepad_scroll_key();
            if (padKey != -1) {
                bool overTarget = mouseHitTestInWindow(gInventoryWindow,
                    INVENTORY_LOOT_RIGHT_SCROLLER_X, INVENTORY_LOOT_RIGHT_SCROLLER_Y,
                    INVENTORY_LOOT_RIGHT_SCROLLER_MAX_X,
                    INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + INVENTORY_LOOT_RIGHT_SCROLLER_Y);

                int panelX = overTarget ? INVENTORY_LOOT_RIGHT_SCROLLER_X : INVENTORY_LOOT_LEFT_SCROLLER_X;
                int panelY = overTarget ? INVENTORY_LOOT_RIGHT_SCROLLER_Y : INVENTORY_LOOT_LEFT_SCROLLER_Y;

                int listLength = overTarget ? _target_pud->length : _pud->length;
                int listOffset = overTarget ? _target_stack_offset[_target_curr_stack] : _stack_offset[_curr_stack];

                int shown = listLength - listOffset;
                if (shown > gInventorySlotsCount) {
                    shown = gInventorySlotsCount;
                }

                int mx;
                int my;
                mouseGetPositionInWindow(gInventoryWindow, &mx, &my);

                int sel = (my - panelY) / INVENTORY_SLOT_HEIGHT;
                if (sel < 0) {
                    sel = 0;
                } else if (sel > gInventorySlotsCount - 1) {
                    sel = gInventorySlotsCount - 1;
                }

                int next = sel + (padKey == KEY_ARROW_UP ? -1 : 1);

                if (next >= 0 && next < shown) {
                    Rect lootRect;
                    windowGetRect(gInventoryWindow, &lootRect);
                    barter_warp_cursor(lootRect.left + panelX + INVENTORY_SLOT_WIDTH / 2,
                        lootRect.top + panelY + next * INVENTORY_SLOT_HEIGHT + INVENTORY_SLOT_HEIGHT / 2);
                } else if (overTarget) {
                    keyCode = padKey == KEY_ARROW_UP ? KEY_CTRL_ARROW_UP : KEY_CTRL_ARROW_DOWN;
                } else {
                    keyCode = padKey;
                }
            }
        }

        if (keyCode == KEY_CTRL_Q || keyCode == KEY_CTRL_X || keyCode == KEY_F10) {
            showQuitConfirmationDialog();
        }

        if (_game_user_wants_to_quit != 0) {
            break;
        }

        // Controller: left/right jumps the pointer to the first item of your / the container's panel.
        loot_gamepad_panel_jump(gInventoryWindow);

        // Controller quick-take: the gamepad layer signals 1 (A = one unit) or 2 (Y = whole stack) for
        // the item under the cursor, moved in _move_inventory's quick (no-drag) mode, steal bookkeeping
        // exactly as the mouse path.
        int lootQuick = gamepad_ui_take_loot_quick();
        if (lootQuick != 0) {
            int mx;
            int my;
            mouseGetPositionInWindow(gInventoryWindow, &mx, &my);

            if (mouseHitTestInWindow(gInventoryWindow, INVENTORY_LOOT_LEFT_SCROLLER_X, INVENTORY_LOOT_LEFT_SCROLLER_Y, INVENTORY_LOOT_LEFT_SCROLLER_MAX_X, INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + INVENTORY_LOOT_LEFT_SCROLLER_Y)) {
                int slotIndex = (my - INVENTORY_LOOT_LEFT_SCROLLER_Y) / INVENTORY_SLOT_HEIGHT;
                if (slotIndex >= 0 && slotIndex + _stack_offset[_curr_stack] < _pud->length) {
                    InventoryItem* inventoryItem = &(_pud->items[_pud->length - (slotIndex + _stack_offset[_curr_stack] + 1)]);
                    int amount = lootQuick == 2 ? inventoryItem->quantity : 1;
                    _gStealCount += 1;
                    _gStealSize += itemGetSize(inventoryItem->item);
                    InventoryMoveResult rc = _move_inventory(inventoryItem->item, slotIndex, _target_stack[_target_curr_stack], true, amount);
                    if (rc == INVENTORY_MOVE_RESULT_CAUGHT_STEALING) {
                        isCaughtStealing = true;
                    } else if (rc == INVENTORY_MOVE_RESULT_SUCCESS) {
                        stealingXp += stealingXpBonus;
                        stealingXpBonus += 10;
                    }
                    _display_target_inventory(_target_stack_offset[_target_curr_stack], -1, _target_pud, INVENTORY_WINDOW_TYPE_LOOT);
                    _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_LOOT);
                }
            } else if (mouseHitTestInWindow(gInventoryWindow, INVENTORY_LOOT_RIGHT_SCROLLER_X, INVENTORY_LOOT_RIGHT_SCROLLER_Y, INVENTORY_LOOT_RIGHT_SCROLLER_MAX_X, INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + INVENTORY_LOOT_RIGHT_SCROLLER_Y)) {
                int slotIndex = (my - INVENTORY_LOOT_RIGHT_SCROLLER_Y) / INVENTORY_SLOT_HEIGHT;
                if (slotIndex >= 0 && slotIndex + _target_stack_offset[_target_curr_stack] < _target_pud->length) {
                    InventoryItem* inventoryItem = &(_target_pud->items[_target_pud->length - (slotIndex + _target_stack_offset[_target_curr_stack] + 1)]);
                    int amount = lootQuick == 2 ? inventoryItem->quantity : 1;
                    _gStealCount += 1;
                    _gStealSize += itemGetSize(inventoryItem->item);
                    InventoryMoveResult rc = _move_inventory(inventoryItem->item, slotIndex, _target_stack[_target_curr_stack], false, amount);
                    if (rc == INVENTORY_MOVE_RESULT_CAUGHT_STEALING) {
                        isCaughtStealing = true;
                    } else if (rc == INVENTORY_MOVE_RESULT_SUCCESS) {
                        stealingXp += stealingXpBonus;
                        stealingXpBonus += 10;
                    }
                    _display_target_inventory(_target_stack_offset[_target_curr_stack], -1, _target_pud, INVENTORY_WINDOW_TYPE_LOOT);
                    _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_LOOT);
                }
            }

            keyCode = -1;
        }

        if (keyCode == KEY_UPPERCASE_A) {
            if (!_gIsSteal) {
                int maxCarryWeight = critterGetStat(looter, STAT_CARRY_WEIGHT);
                int currentWeight = objectGetInventoryWeight(looter);
                int newInventoryWeight = objectGetInventoryWeight(target);
                if (newInventoryWeight <= maxCarryWeight - currentWeight) {
                    itemMoveAll(target, looter);
                    _display_target_inventory(_target_stack_offset[_target_curr_stack], -1, _target_pud, INVENTORY_WINDOW_TYPE_LOOT);
                    _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_LOOT);
                } else {
                    // CE: TAKE WHAT FITS (X on the pad, see loot_collect_bodies). Vanilla took all or
                    // nothing, so one heavy gun left the caps and the stimpaks behind as well. Down the
                    // list, as many of each as the weight left allows; then the same message.
                    int room = maxCarryWeight - currentWeight;
                    int index = 0;
                    while (index < _target_pud->length) {
                        InventoryItem* inventoryItem = &(_target_pud->items[index]);
                        int unitWeight = itemGetWeight(inventoryItem->item);
                        int quantity = inventoryItem->quantity;
                        if (unitWeight > 0 && quantity > room / unitWeight) {
                            quantity = room / unitWeight;
                        }
                        int lengthBefore = _target_pud->length;
                        if (quantity <= 0 || itemMove(target, looter, inventoryItem->item, quantity) != 0) {
                            index++;
                            continue;
                        }
                        room -= unitWeight * quantity;
                        if (_target_pud->length == lengthBefore) {
                            index++; // part of a stack stayed where it was
                        }
                    }
                    _display_target_inventory(_target_stack_offset[_target_curr_stack], -1, _target_pud, INVENTORY_WINDOW_TYPE_LOOT);
                    _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_LOOT);

                    // Sorry, you cannot carry that much.
                    messageListItem.num = 31;
                    if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
                        showDialogBox(messageListItem.text, nullptr, 0, 169, 117, _colorTable[32328], nullptr, _colorTable[32328], 0);
                    }
                }
                trace_event(TRACE_ACTION, "loot-take-all", "id=%d left=%d weight=%d/%d",
                    target->id, _target_pud->length, objectGetInventoryWeight(looter), maxCarryWeight);
            }
        } else if (keyCode == KEY_ARROW_UP) {
            if (_stack_offset[_curr_stack] > 0) {
                _stack_offset[_curr_stack] -= 1;
                _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_LOOT);
            }
        } else if (keyCode == KEY_PAGE_UP || keyCode == KEY_PAGE_DOWN) {
            // CE: vanilla stepped to the next index whatever it held; now to the next body with
            // something on it (loot_next_body), and the counter follows.
            int next = critterCount != 0
                ? loot_next_body(critters, critterState, critterCount, critterIndex, keyCode == KEY_PAGE_UP ? -1 : 1)
                : critterIndex;
            if (next != critterIndex) {
                critterIndex = next;

                target = critters[critterIndex];
                _target_pud = &(target->data.inventory);
                _target_stack[0] = target;
                _target_curr_stack = 0;
                _target_stack_offset[0] = 0;
                _display_target_inventory(0, -1, _target_pud, INVENTORY_WINDOW_TYPE_LOOT);
                _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_LOOT);
                display_body_force = true;
                _display_body(target->fid, INVENTORY_WINDOW_TYPE_LOOT);
                loot_body_index = critterIndex;
                loot_draw_body_counter();
                trace_event(TRACE_ACTION, "loot-body", "id=%d pid=%d index=%d of=%d items=%d",
                    target->id, target->pid, critterIndex + 1, critterCount, _target_pud->length);
            }
        } else if (keyCode == KEY_ARROW_DOWN) {
            if (_stack_offset[_curr_stack] + gInventorySlotsCount < _pud->length) {
                _stack_offset[_curr_stack] += 1;
                _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_LOOT);
            }
        } else if (keyCode == KEY_CTRL_ARROW_UP) {
            if (_target_stack_offset[_target_curr_stack] > 0) {
                _target_stack_offset[_target_curr_stack] -= 1;
                _display_target_inventory(_target_stack_offset[_target_curr_stack], -1, _target_pud, INVENTORY_WINDOW_TYPE_LOOT);
                windowRefresh(gInventoryWindow);
            }
        } else if (keyCode == KEY_CTRL_ARROW_DOWN) {
            if (_target_stack_offset[_target_curr_stack] + gInventorySlotsCount < _target_pud->length) {
                _target_stack_offset[_target_curr_stack] += 1;
                _display_target_inventory(_target_stack_offset[_target_curr_stack], -1, _target_pud, INVENTORY_WINDOW_TYPE_LOOT);
                windowRefresh(gInventoryWindow);
            }
        } else if (keyCode >= 2500 && keyCode <= 2501) {
            _container_exit(keyCode, INVENTORY_WINDOW_TYPE_LOOT);
        } else {
            if ((mouseGetEvent() & MOUSE_EVENT_RIGHT_BUTTON_DOWN) != 0) {
                if (gInventoryCursor == INVENTORY_WINDOW_CURSOR_HAND) {
                    inventorySetCursor(INVENTORY_WINDOW_CURSOR_ARROW);
                } else {
                    inventorySetCursor(INVENTORY_WINDOW_CURSOR_HAND);
                }
            } else if ((mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0) {
                if (keyCode >= 1000 && keyCode <= 1000 + gInventorySlotsCount) {
                    if (gInventoryCursor == INVENTORY_WINDOW_CURSOR_ARROW) {
                        inventoryWindowOpenContextMenu(keyCode, INVENTORY_WINDOW_TYPE_LOOT);
                    } else {
                        int slotIndex = keyCode - 1000;
                        if (slotIndex + _stack_offset[_curr_stack] < _pud->length) {
                            _gStealCount += 1;
                            _gStealSize += itemGetSize(_stack[_curr_stack]);

                            InventoryItem* inventoryItem = &(_pud->items[_pud->length - (slotIndex + _stack_offset[_curr_stack] + 1)]);
                            InventoryMoveResult rc = _move_inventory(inventoryItem->item, slotIndex, _target_stack[_target_curr_stack], true);
                            if (rc == INVENTORY_MOVE_RESULT_CAUGHT_STEALING) {
                                isCaughtStealing = true;
                            } else if (rc == INVENTORY_MOVE_RESULT_SUCCESS) {
                                stealingXp += stealingXpBonus;
                                stealingXpBonus += 10;
                            }

                            _display_target_inventory(_target_stack_offset[_target_curr_stack], -1, _target_pud, INVENTORY_WINDOW_TYPE_LOOT);
                            _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_LOOT);
                        }

                        keyCode = -1;
                    }
                } else if (keyCode >= 2000 && keyCode <= 2000 + gInventorySlotsCount) {
                    if (gInventoryCursor == INVENTORY_WINDOW_CURSOR_ARROW) {
                        inventoryWindowOpenContextMenu(keyCode, INVENTORY_WINDOW_TYPE_LOOT);
                    } else {
                        int slotIndex = keyCode - 2000;
                        if (slotIndex + _target_stack_offset[_target_curr_stack] < _target_pud->length) {
                            _gStealCount += 1;
                            _gStealSize += itemGetSize(_stack[_curr_stack]);

                            InventoryItem* inventoryItem = &(_target_pud->items[_target_pud->length - (slotIndex + _target_stack_offset[_target_curr_stack] + 1)]);
                            InventoryMoveResult rc = _move_inventory(inventoryItem->item, slotIndex, _target_stack[_target_curr_stack], false);
                            if (rc == INVENTORY_MOVE_RESULT_CAUGHT_STEALING) {
                                isCaughtStealing = true;
                            } else if (rc == INVENTORY_MOVE_RESULT_SUCCESS) {
                                stealingXp += stealingXpBonus;
                                stealingXpBonus += 10;
                            }

                            _display_target_inventory(_target_stack_offset[_target_curr_stack], -1, _target_pud, INVENTORY_WINDOW_TYPE_LOOT);
                            _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_LOOT);
                        }
                    }
                }
            } else if ((mouseGetEvent() & MOUSE_EVENT_WHEEL) != 0) {
                if (mouseHitTestInWindow(gInventoryWindow, INVENTORY_LOOT_LEFT_SCROLLER_X, INVENTORY_LOOT_LEFT_SCROLLER_Y, INVENTORY_LOOT_LEFT_SCROLLER_MAX_X, INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + INVENTORY_LOOT_LEFT_SCROLLER_Y)) {
                    int wheelX;
                    int wheelY;
                    mouseGetWheel(&wheelX, &wheelY);
                    if (wheelY > 0) {
                        if (_stack_offset[_curr_stack] > 0) {
                            _stack_offset[_curr_stack] -= 1;
                            _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_LOOT);
                        }
                    } else if (wheelY < 0) {
                        if (_stack_offset[_curr_stack] + gInventorySlotsCount < _pud->length) {
                            _stack_offset[_curr_stack] += 1;
                            _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_LOOT);
                        }
                    }
                } else if (mouseHitTestInWindow(gInventoryWindow, INVENTORY_LOOT_RIGHT_SCROLLER_X, INVENTORY_LOOT_RIGHT_SCROLLER_Y, INVENTORY_LOOT_RIGHT_SCROLLER_MAX_X, INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + INVENTORY_LOOT_RIGHT_SCROLLER_Y)) {
                    int wheelX;
                    int wheelY;
                    mouseGetWheel(&wheelX, &wheelY);
                    if (wheelY > 0) {
                        if (_target_stack_offset[_target_curr_stack] > 0) {
                            _target_stack_offset[_target_curr_stack] -= 1;
                            _display_target_inventory(_target_stack_offset[_target_curr_stack], -1, _target_pud, INVENTORY_WINDOW_TYPE_LOOT);
                            windowRefresh(gInventoryWindow);
                        }
                    } else if (wheelY < 0) {
                        if (_target_stack_offset[_target_curr_stack] + gInventorySlotsCount < _target_pud->length) {
                            _target_stack_offset[_target_curr_stack] += 1;
                            _display_target_inventory(_target_stack_offset[_target_curr_stack], -1, _target_pud, INVENTORY_WINDOW_TYPE_LOOT);
                            windowRefresh(gInventoryWindow);
                        }
                    }
                }
            }
        }

        if (keyCode == KEY_ESCAPE) {
            break;
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    loot_body_count = 0;

    if (_gIsSteal) {
        if (item1 != nullptr) {
            item1->flags |= OBJECT_IN_LEFT_HAND;
            itemAdd(target, item1, 1);
        }

        if (item2 != nullptr) {
            item2->flags |= OBJECT_IN_RIGHT_HAND;
            itemAdd(target, item2, 1);
        }

        if (armor != nullptr) {
            armor->flags |= OBJECT_WORN;
            itemAdd(target, armor, 1);
        }
    }

    itemMoveAll(hiddenBox, target);
    objectDestroy(hiddenBox, nullptr);

    if (_gIsSteal && !isCaughtStealing && stealingXp > 0 && !objectIsPartyMember(target)) {
        stealingXp = std::min(300 - skillGetValue(looter, SKILL_STEAL), stealingXp);
        debugPrint("\n[[[%d]]]", 300 - skillGetValue(looter, SKILL_STEAL));

        // SFALL: Display actual xp received.
        int xpGained;
        pcAddExperience(stealingXp, &xpGained);

        // You gain %d experience points for successfully using your Steal skill.
        messageListItem.num = 29;
        if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
            char formattedText[200];
            snprintf(formattedText, sizeof(formattedText), messageListItem.text, xpGained);
            displayMonitorAddMessage(formattedText);
        }
    }

    _exit_inventory(isoWasEnabled);

    // NOTE: Uninline.
    inventoryCommonFree();

    if (_gIsSteal && isCaughtStealing && _gStealCount > 0 && _obj_sid(target, &sid) != -1) {
        scriptSetObjects(sid, looter, nullptr);
        scriptExecProc(sid, SCRIPT_PROC_PICKUP);

        // TODO: Looks like inlining, script is not used.
        Script* script;
        scriptGetScript(sid, &script);
    }

    return 0;
}

// 0x4746A0
int inventoryOpenStealing(Object* thief, Object* target)
{
    if (thief == target) {
        return -1;
    }

    _gIsSteal = PID_TYPE(thief->pid) == OBJ_TYPE_CRITTER && critterIsActive(target);
    _gStealCount = 0;
    _gStealSize = 0;

    int rc = inventoryOpenLooting(thief, target);

    _gIsSteal = 0;
    _gStealCount = 0;
    _gStealSize = 0;

    return rc;
}

// 0x474708
// gpQuantity: 0 = the mouse drag-and-drop transfer. > 0 = the controller's quick transfer of exactly
// that many units to the opposite panel, with no drag gesture (the pad's loot quick-take).
static InventoryMoveResult _move_inventory(Object* item, int slotIndex, Object* targetObj, bool isPlanting, int gpQuantity)
{
    bool gp = gpQuantity != 0;
    bool needRefresh = true;

    Rect rect;

    int quantity;
    if (isPlanting) {
        rect.left = INVENTORY_LOOT_LEFT_SCROLLER_X;
        rect.top = INVENTORY_SLOT_HEIGHT * slotIndex + INVENTORY_LOOT_LEFT_SCROLLER_Y;

        InventoryItem* inventoryItem = &(_pud->items[_pud->length - (slotIndex + _stack_offset[_curr_stack] + 1)]);
        quantity = inventoryItem->quantity;
        if (quantity > 1) {
            _display_inventory(_stack_offset[_curr_stack], slotIndex, INVENTORY_WINDOW_TYPE_LOOT);
            needRefresh = false;
        }
    } else {
        rect.left = INVENTORY_LOOT_RIGHT_SCROLLER_X;
        rect.top = INVENTORY_SLOT_HEIGHT * slotIndex + INVENTORY_LOOT_RIGHT_SCROLLER_Y;

        InventoryItem* inventoryItem = &(_target_pud->items[_target_pud->length - (slotIndex + _target_stack_offset[_target_curr_stack] + 1)]);
        quantity = inventoryItem->quantity;
        if (quantity > 1) {
            _display_target_inventory(_target_stack_offset[_target_curr_stack], slotIndex, _target_pud, INVENTORY_WINDOW_TYPE_LOOT);
            windowRefresh(gInventoryWindow);
            needRefresh = false;
        }
    }

    if (needRefresh && !gp) {
        unsigned char* windowBuffer = windowGetBuffer(gInventoryWindow);

        FrmImage backgroundFrmImage;
        int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, 114, 0, 0, 0);
        if (backgroundFrmImage.lock(backgroundFid)) {
            blitBufferToBuffer(backgroundFrmImage.getData() + INVENTORY_LOOT_WINDOW_WIDTH * rect.top + rect.left,
                INVENTORY_SLOT_WIDTH,
                INVENTORY_SLOT_HEIGHT,
                INVENTORY_LOOT_WINDOW_WIDTH,
                windowBuffer + INVENTORY_LOOT_WINDOW_WIDTH * rect.top + rect.left,
                INVENTORY_LOOT_WINDOW_WIDTH);
        }

        rect.right = rect.left + INVENTORY_SLOT_WIDTH - 1;
        rect.bottom = rect.top + INVENTORY_SLOT_HEIGHT - 1;
        windowRefreshRect(gInventoryWindow, &rect);
    }

    FrmImage itemInventoryFrmImage;
    if (!gp) {
        int itemInventoryFid = itemGetInventoryFid(item);
        if (itemInventoryFrmImage.lock(itemInventoryFid)) {
            int width = itemInventoryFrmImage.getWidth();
            int height = itemInventoryFrmImage.getHeight();
            unsigned char* data = itemInventoryFrmImage.getData();
            mouseSetFrame(data, width, height, width, width / 2, height / 2, 0);
            soundPlayFile("ipickup1");
        }

        do {
            sharedFpsLimiter.mark();

            inputGetInput();

            renderPresent();
            sharedFpsLimiter.throttle();
        } while ((mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_REPEAT) != 0);

        if (itemInventoryFrmImage.isLocked()) {
            itemInventoryFrmImage.unlock();
            soundPlayFile("iputdown");
        }
    } else {
        soundPlayFile("iputdown");
    }

    InventoryMoveResult result = INVENTORY_MOVE_RESULT_FAILED;
    MessageListItem messageListItem;

    if (isPlanting) {
        if (gp || mouseHitTestInWindow(gInventoryWindow, INVENTORY_LOOT_RIGHT_SCROLLER_X, INVENTORY_LOOT_RIGHT_SCROLLER_Y, INVENTORY_LOOT_RIGHT_SCROLLER_MAX_X, INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + INVENTORY_LOOT_RIGHT_SCROLLER_Y)) {
            int quantityToMove;
            if (gp) {
                quantityToMove = gpQuantity;
            } else if (quantity > 1) {
                quantityToMove = inventoryQuantitySelect(INVENTORY_WINDOW_TYPE_MOVE_ITEMS, item, quantity);
            } else {
                quantityToMove = 1;
            }

            if (quantityToMove != -1) {
                if (_gIsSteal) {
                    if (skillsPerformStealing(_inven_dude, targetObj, item, true) == 0) {
                        result = INVENTORY_MOVE_RESULT_CAUGHT_STEALING;
                    }
                }

                if (result != INVENTORY_MOVE_RESULT_CAUGHT_STEALING) {
                    if (itemMove(_inven_dude, targetObj, item, quantityToMove) != -1) {
                        result = INVENTORY_MOVE_RESULT_SUCCESS;
                    } else {
                        // There is no space left for that item.
                        messageListItem.num = 26;
                        if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
                            displayMonitorAddMessage(messageListItem.text);
                        }
                    }
                }
            }
        }
    } else {
        if (gp || mouseHitTestInWindow(gInventoryWindow, INVENTORY_LOOT_LEFT_SCROLLER_X, INVENTORY_LOOT_LEFT_SCROLLER_Y, INVENTORY_LOOT_LEFT_SCROLLER_MAX_X, INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + INVENTORY_LOOT_LEFT_SCROLLER_Y)) {
            int quantityToMove;
            if (gp) {
                quantityToMove = gpQuantity;
            } else if (quantity > 1) {
                quantityToMove = inventoryQuantitySelect(INVENTORY_WINDOW_TYPE_MOVE_ITEMS, item, quantity);
            } else {
                quantityToMove = 1;
            }

            if (quantityToMove != -1) {
                if (_gIsSteal) {
                    if (skillsPerformStealing(_inven_dude, targetObj, item, false) == 0) {
                        result = INVENTORY_MOVE_RESULT_CAUGHT_STEALING;
                    }
                }

                if (result != INVENTORY_MOVE_RESULT_CAUGHT_STEALING) {
                    if (itemMove(targetObj, _inven_dude, item, quantityToMove) == 0) {
                        if ((item->flags & OBJECT_IN_RIGHT_HAND) != 0) {
                            targetObj->fid = buildFid(FID_TYPE(targetObj->fid), targetObj->fid & 0xFFF, FID_ANIM_TYPE(targetObj->fid), 0, targetObj->rotation + 1);
                        }

                        targetObj->flags &= ~OBJECT_EQUIPPED;

                        result = INVENTORY_MOVE_RESULT_SUCCESS;
                    } else {
                        // You cannot pick that up. You are at your maximum weight capacity.
                        messageListItem.num = 25;
                        if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
                            displayMonitorAddMessage(messageListItem.text);
                        }
                    }
                }
            }
        }
    }

    inventorySetCursor(INVENTORY_WINDOW_CURSOR_HAND);

    return result;
}

// 0x474B2C
static int _barter_compute_value(Object* dude, Object* npc)
{
    if (gGameDialogSpeakerIsPartyMember) {
        return objectGetInventoryWeight(_btable);
    }

    int cost = objectGetCost(_btable);
    int caps = itemGetTotalCaps(_btable);
    int costWithoutCaps = cost - caps;

    double perkBonus = 0.0;
    if (dude == gDude) {
        if (perkHasRank(gDude, PERK_MASTER_TRADER)) {
            perkBonus = 25.0;
        }
    }

    int partyBarter = partyGetBestSkillValue(SKILL_BARTER);
    int npcBarter = skillGetValue(npc, SKILL_BARTER);

    // TODO: Check in debugger, complex math, probably uses floats, not doubles.
    double barterModMult = (_barter_mod + 100.0 - perkBonus) * 0.01;
    double balancedCost = (160.0 + npcBarter) / (160.0 + partyBarter) * (costWithoutCaps * 2.0);
    if (barterModMult < 0) {
        // TODO: Probably 0.01 as float.
        barterModMult = 0.0099999998;
    }

    int rounded = (int)(barterModMult * balancedCost + caps);
    return rounded;
}

// 0x474C50
static int _barter_attempt_transaction(Object* dude, Object* offerTable, Object* npc, Object* barterTable)
{
    MessageListItem messageListItem;

    int weightAvailable = critterGetStat(dude, STAT_CARRY_WEIGHT) - objectGetInventoryWeight(dude);
    if (objectGetInventoryWeight(barterTable) > weightAvailable) {
        // Sorry, you cannot carry that much.
        messageListItem.num = 31;
        if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
            gameDialogRenderSupplementaryMessage(messageListItem.text);
        }
        return -1;
    }

    if (gGameDialogSpeakerIsPartyMember) {
        int npcWeightAvailable = critterGetStat(npc, STAT_CARRY_WEIGHT) - objectGetInventoryWeight(npc);
        if (objectGetInventoryWeight(offerTable) > npcWeightAvailable) {
            // Sorry, that's too much to carry.
            messageListItem.num = 32;
            if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
                gameDialogRenderSupplementaryMessage(messageListItem.text);
            }
            return -1;
        }
    } else {
        bool badOffer = false;
        if (offerTable->data.inventory.length == 0) {
            badOffer = true;
        } else {
            if (itemIsQueued(offerTable)) {
                if (offerTable->pid != PROTO_ID_GEIGER_COUNTER_I || miscItemTurnOff(offerTable) == -1) {
                    badOffer = true;
                }
            }
        }

        if (!badOffer) {
            int cost = objectGetCost(offerTable);
            if (_barter_compute_value(dude, npc) > cost) {
                badOffer = true;
            }
        }

        if (badOffer) {
            // No, your offer is not good enough.
            messageListItem.num = 28;
            if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
                gameDialogRenderSupplementaryMessage(messageListItem.text);
            }
            return -1;
        }
    }

    itemMoveAll(barterTable, dude);
    itemMoveAll(offerTable, npc);
    return 0;
}

// 0x474DAC
static void _barter_move_inventory(Object* item, int quantity, int slotIndex, int indexOffset, Object* npc, Object* sourceTable, bool fromDude)
{
    Rect rect;
    if (fromDude) {
        rect.left = 23;
        rect.top = INVENTORY_SLOT_HEIGHT * slotIndex + 34;
    } else {
        rect.left = 395;
        rect.top = INVENTORY_SLOT_HEIGHT * slotIndex + 31;
    }

    if (quantity > 1) {
        if (fromDude) {
            _display_inventory(indexOffset, slotIndex, INVENTORY_WINDOW_TYPE_TRADE);
        } else {
            _display_target_inventory(indexOffset, slotIndex, _target_pud, INVENTORY_WINDOW_TYPE_TRADE);
        }
    } else {
        unsigned char* dest = windowGetBuffer(gInventoryWindow);
        unsigned char* src = windowGetBuffer(_barter_back_win);

        int pitch = INVENTORY_TRADE_BACKGROUND_WINDOW_WIDTH;
        blitBufferToBuffer(src + pitch * rect.top + rect.left + INVENTORY_TRADE_WINDOW_OFFSET, INVENTORY_SLOT_WIDTH, INVENTORY_SLOT_HEIGHT, pitch, dest + INVENTORY_TRADE_WINDOW_WIDTH * rect.top + rect.left, INVENTORY_TRADE_WINDOW_WIDTH);

        rect.right = rect.left + INVENTORY_SLOT_WIDTH - 1;
        rect.bottom = rect.top + INVENTORY_SLOT_HEIGHT - 1;
        windowRefreshRect(gInventoryWindow, &rect);
    }

    FrmImage itemInventoryFrmImage;
    int itemInventoryFid = itemGetInventoryFid(item);
    if (itemInventoryFrmImage.lock(itemInventoryFid)) {
        int width = itemInventoryFrmImage.getWidth();
        int height = itemInventoryFrmImage.getHeight();
        unsigned char* data = itemInventoryFrmImage.getData();
        mouseSetFrame(data, width, height, width, width / 2, height / 2, 0);
        soundPlayFile("ipickup1");
    }

    do {
        sharedFpsLimiter.mark();

        inputGetInput();

        renderPresent();
        sharedFpsLimiter.throttle();
    } while ((mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_REPEAT) != 0);

    if (itemInventoryFrmImage.isLocked()) {
        itemInventoryFrmImage.unlock();
        soundPlayFile("iputdown");
    }

    MessageListItem messageListItem;

    if (fromDude) {
        if (mouseHitTestInWindow(gInventoryWindow, INVENTORY_TRADE_INNER_LEFT_SCROLLER_TRACKING_X, INVENTORY_TRADE_INNER_LEFT_SCROLLER_TRACKING_Y, INVENTORY_TRADE_INNER_LEFT_SCROLLER_TRACKING_MAX_X, INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + INVENTORY_TRADE_INNER_LEFT_SCROLLER_TRACKING_Y)) {
            int quantityToMove = quantity > 1 ? inventoryQuantitySelect(INVENTORY_WINDOW_TYPE_MOVE_ITEMS, item, quantity) : 1;
            if (quantityToMove != -1) {
                if (itemMoveForce(_inven_dude, sourceTable, item, quantityToMove) == -1) {
                    // There is no space left for that item.
                    messageListItem.num = 26;
                    if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
                        displayMonitorAddMessage(messageListItem.text);
                    }
                }
            }
        }
    } else {
        if (mouseHitTestInWindow(gInventoryWindow, INVENTORY_TRADE_INNER_RIGHT_SCROLLER_TRACKING_X, INVENTORY_TRADE_INNER_RIGHT_SCROLLER_TRACKING_Y, INVENTORY_TRADE_INNER_RIGHT_SCROLLER_TRACKING_MAX_X, INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + INVENTORY_TRADE_INNER_RIGHT_SCROLLER_TRACKING_Y)) {
            int quantityToMove = quantity > 1 ? inventoryQuantitySelect(INVENTORY_WINDOW_TYPE_MOVE_ITEMS, item, quantity) : 1;
            if (quantityToMove != -1) {
                if (itemMoveForce(npc, sourceTable, item, quantityToMove) == -1) {
                    // You cannot pick that up. You are at your maximum weight capacity.
                    messageListItem.num = 25;
                    if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
                        displayMonitorAddMessage(messageListItem.text);
                    }
                }
            }
        }
    }

    inventorySetCursor(INVENTORY_WINDOW_CURSOR_HAND);
}

// 0x475070
static void _barter_move_from_table_inventory(Object* item, int quantity, int slotIndex, Object* npc, Object* sourceTable, bool fromDude)
{
    Rect rect;
    if (fromDude) {
        rect.left = INVENTORY_TRADE_INNER_LEFT_SCROLLER_X_PAD;
        rect.top = INVENTORY_SLOT_HEIGHT * slotIndex + INVENTORY_TRADE_INNER_LEFT_SCROLLER_Y_PAD;
    } else {
        rect.left = INVENTORY_TRADE_INNER_RIGHT_SCROLLER_X_PAD;
        rect.top = INVENTORY_SLOT_HEIGHT * slotIndex + INVENTORY_TRADE_INNER_RIGHT_SCROLLER_Y_PAD;
    }

    if (quantity > 1) {
        if (fromDude) {
            inventoryWindowRenderInnerInventories(_barter_back_win, sourceTable, nullptr, slotIndex);
        } else {
            inventoryWindowRenderInnerInventories(_barter_back_win, nullptr, sourceTable, slotIndex);
        }
    } else {
        unsigned char* dest = windowGetBuffer(gInventoryWindow);
        unsigned char* src = windowGetBuffer(_barter_back_win);

        int pitch = INVENTORY_TRADE_BACKGROUND_WINDOW_WIDTH;
        blitBufferToBuffer(src + pitch * rect.top + rect.left + INVENTORY_TRADE_WINDOW_OFFSET, INVENTORY_SLOT_WIDTH, INVENTORY_SLOT_HEIGHT, pitch, dest + INVENTORY_TRADE_WINDOW_WIDTH * rect.top + rect.left, INVENTORY_TRADE_WINDOW_WIDTH);

        rect.right = rect.left + INVENTORY_SLOT_WIDTH - 1;
        rect.bottom = rect.top + INVENTORY_SLOT_HEIGHT - 1;
        windowRefreshRect(gInventoryWindow, &rect);
    }

    FrmImage itemInventoryFrmImage;
    int itemInventoryFid = itemGetInventoryFid(item);
    if (itemInventoryFrmImage.lock(itemInventoryFid)) {
        int width = itemInventoryFrmImage.getWidth();
        int height = itemInventoryFrmImage.getHeight();
        unsigned char* data = itemInventoryFrmImage.getData();
        mouseSetFrame(data, width, height, width, width / 2, height / 2, 0);
        soundPlayFile("ipickup1");
    }

    do {
        sharedFpsLimiter.mark();

        inputGetInput();

        renderPresent();
        sharedFpsLimiter.throttle();
    } while ((mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_REPEAT) != 0);

    if (itemInventoryFrmImage.isLocked()) {
        itemInventoryFrmImage.unlock();
        soundPlayFile("iputdown");
    }

    MessageListItem messageListItem;

    if (fromDude) {
        if (mouseHitTestInWindow(gInventoryWindow, INVENTORY_TRADE_LEFT_SCROLLER_TRACKING_X, INVENTORY_TRADE_LEFT_SCROLLER_TRACKING_Y, INVENTORY_TRADE_LEFT_SCROLLER_TRACKING_MAX_X, INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + INVENTORY_TRADE_LEFT_SCROLLER_TRACKING_Y)) {
            int quantityToMove = quantity > 1 ? inventoryQuantitySelect(INVENTORY_WINDOW_TYPE_MOVE_ITEMS, item, quantity) : 1;
            if (quantityToMove != -1) {
                if (itemMoveForce(sourceTable, _inven_dude, item, quantityToMove) == -1) {
                    // There is no space left for that item.
                    messageListItem.num = 26;
                    if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
                        displayMonitorAddMessage(messageListItem.text);
                    }
                }
            }
        }
    } else {
        if (mouseHitTestInWindow(gInventoryWindow, INVENTORY_TRADE_RIGHT_SCROLLER_TRACKING_X, INVENTORY_TRADE_RIGHT_SCROLLER_TRACKING_Y, INVENTORY_TRADE_RIGHT_SCROLLER_TRACKING_MAX_X, INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + INVENTORY_TRADE_RIGHT_SCROLLER_TRACKING_Y)) {
            int quantityToMove = quantity > 1 ? inventoryQuantitySelect(INVENTORY_WINDOW_TYPE_MOVE_ITEMS, item, quantity) : 1;
            if (quantityToMove != -1) {
                if (itemMoveForce(sourceTable, npc, item, quantityToMove) == -1) {
                    // You cannot pick that up. You are at your maximum weight capacity.
                    messageListItem.num = 25;
                    if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
                        displayMonitorAddMessage(messageListItem.text);
                    }
                }
            }
        }
    }

    inventorySetCursor(INVENTORY_WINDOW_CURSOR_HAND);
}

// 0x475334
static void inventoryWindowRenderInnerInventories(int win, Object* leftTable, Object* rightTable, int draggedSlotIndex)
{
    unsigned char* windowBuffer = windowGetBuffer(gInventoryWindow);

    int oldFont = fontGetCurrent();
    fontSetCurrent(101);

    char formattedText[80];
    int rectHeight = fontGetLineHeight() + INVENTORY_SLOT_HEIGHT * gInventorySlotsCount;

    if (leftTable != nullptr) {
        unsigned char* src = windowGetBuffer(win);
        blitBufferToBuffer(src + INVENTORY_TRADE_BACKGROUND_WINDOW_WIDTH * INVENTORY_TRADE_INNER_LEFT_SCROLLER_Y + INVENTORY_TRADE_INNER_LEFT_SCROLLER_X_PAD + INVENTORY_TRADE_WINDOW_OFFSET, INVENTORY_SLOT_WIDTH, rectHeight + 1, INVENTORY_TRADE_BACKGROUND_WINDOW_WIDTH, windowBuffer + INVENTORY_TRADE_WINDOW_WIDTH * INVENTORY_TRADE_INNER_LEFT_SCROLLER_Y + INVENTORY_TRADE_INNER_LEFT_SCROLLER_X_PAD, INVENTORY_TRADE_WINDOW_WIDTH);

        unsigned char* dest = windowBuffer + INVENTORY_TRADE_WINDOW_WIDTH * INVENTORY_TRADE_INNER_LEFT_SCROLLER_Y_PAD + INVENTORY_TRADE_INNER_LEFT_SCROLLER_X_PAD;
        Inventory* inventory = &(leftTable->data.inventory);
        for (int index = 0; index < gInventorySlotsCount && index + _ptable_offset < inventory->length; index++) {
            InventoryItem* inventoryItem = &(inventory->items[inventory->length - (index + _ptable_offset + 1)]);
            int inventoryFid = itemGetInventoryFid(inventoryItem->item);
            artRender(inventoryFid, dest, INVENTORY_SLOT_WIDTH_PAD, INVENTORY_SLOT_HEIGHT_PAD, INVENTORY_TRADE_WINDOW_WIDTH);
            _display_inventory_info(inventoryItem->item, inventoryItem->quantity, dest, INVENTORY_TRADE_WINDOW_WIDTH, index == draggedSlotIndex);

            dest += INVENTORY_TRADE_WINDOW_WIDTH * INVENTORY_SLOT_HEIGHT;
        }

        if (gGameDialogSpeakerIsPartyMember) {
            MessageListItem messageListItem;
            messageListItem.num = 30;

            if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
                int weight = objectGetInventoryWeight(leftTable);
                snprintf(formattedText, sizeof(formattedText), "%s %d", messageListItem.text, weight);
            }
        } else {
            int cost = objectGetCost(leftTable);
            snprintf(formattedText, sizeof(formattedText), "$%d", cost);
        }

        fontDrawText(windowBuffer + INVENTORY_TRADE_WINDOW_WIDTH * (INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + INVENTORY_TRADE_INNER_LEFT_SCROLLER_Y_PAD) + INVENTORY_TRADE_INNER_LEFT_SCROLLER_X_PAD, formattedText, 80, INVENTORY_TRADE_WINDOW_WIDTH, _colorTable[32767]);

        Rect rect;
        rect.left = INVENTORY_TRADE_INNER_LEFT_SCROLLER_X_PAD;
        rect.top = INVENTORY_TRADE_INNER_LEFT_SCROLLER_Y_PAD;
        // NOTE: Odd math, the only way to get 223 is to subtract 2.
        rect.right = INVENTORY_TRADE_INNER_LEFT_SCROLLER_X_PAD + INVENTORY_SLOT_WIDTH_PAD - 2;
        rect.bottom = rect.top + rectHeight;
        windowRefreshRect(gInventoryWindow, &rect);
    }

    if (rightTable != nullptr) {
        unsigned char* src = windowGetBuffer(win);
        blitBufferToBuffer(src + INVENTORY_TRADE_BACKGROUND_WINDOW_WIDTH * INVENTORY_TRADE_INNER_RIGHT_SCROLLER_Y + INVENTORY_TRADE_INNER_RIGHT_SCROLLER_X_PAD + INVENTORY_TRADE_WINDOW_OFFSET, INVENTORY_SLOT_WIDTH, rectHeight + 1, INVENTORY_TRADE_BACKGROUND_WINDOW_WIDTH, windowBuffer + INVENTORY_TRADE_WINDOW_WIDTH * INVENTORY_TRADE_INNER_RIGHT_SCROLLER_Y + INVENTORY_TRADE_INNER_RIGHT_SCROLLER_X_PAD, INVENTORY_TRADE_WINDOW_WIDTH);

        unsigned char* dest = windowBuffer + INVENTORY_TRADE_WINDOW_WIDTH * INVENTORY_TRADE_INNER_RIGHT_SCROLLER_Y_PAD + INVENTORY_TRADE_INNER_RIGHT_SCROLLER_X_PAD;
        Inventory* inventory = &(rightTable->data.inventory);
        for (int index = 0; index < gInventorySlotsCount && index + _btable_offset < inventory->length; index++) {
            InventoryItem* inventoryItem = &(inventory->items[inventory->length - (index + _btable_offset + 1)]);
            int inventoryFid = itemGetInventoryFid(inventoryItem->item);
            artRender(inventoryFid, dest, INVENTORY_SLOT_WIDTH_PAD, INVENTORY_SLOT_HEIGHT_PAD, INVENTORY_TRADE_WINDOW_WIDTH);
            _display_inventory_info(inventoryItem->item, inventoryItem->quantity, dest, INVENTORY_TRADE_WINDOW_WIDTH, index == draggedSlotIndex);

            dest += INVENTORY_TRADE_WINDOW_WIDTH * INVENTORY_SLOT_HEIGHT;
        }

        if (gGameDialogSpeakerIsPartyMember) {
            MessageListItem messageListItem;
            messageListItem.num = 30;

            if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
                int weight = _barter_compute_value(gDude, _target_stack[0]);
                snprintf(formattedText, sizeof(formattedText), "%s %d", messageListItem.text, weight);
            }
        } else {
            int cost = _barter_compute_value(gDude, _target_stack[0]);
            snprintf(formattedText, sizeof(formattedText), "$%d", cost);
        }

        fontDrawText(windowBuffer + INVENTORY_TRADE_WINDOW_WIDTH * (INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + INVENTORY_TRADE_INNER_RIGHT_SCROLLER_Y_PAD) + INVENTORY_TRADE_INNER_RIGHT_SCROLLER_X_PAD, formattedText, 80, INVENTORY_TRADE_WINDOW_WIDTH, _colorTable[32767]);

        Rect rect;
        rect.left = INVENTORY_TRADE_INNER_RIGHT_SCROLLER_X_PAD;
        rect.top = INVENTORY_TRADE_INNER_RIGHT_SCROLLER_Y_PAD;
        // NOTE: Odd math, likely should be `INVENTORY_SLOT_WIDTH_PAD`.
        rect.right = INVENTORY_TRADE_INNER_RIGHT_SCROLLER_X_PAD + INVENTORY_SLOT_WIDTH;
        rect.bottom = rect.top + rectHeight;
        windowRefreshRect(gInventoryWindow, &rect);
    }

    fontSetCurrent(oldFont);

    // The deal readout rides along with every table repaint (more than twenty call sites -- hooking
    // here means none is missed). Self-guarding: it returns at once when no barter is in progress.
    barter_draw_deal();
}

// One occupied, visible barter slot: its screen rect + which list (0..3) and visible row it maps to.
struct BarterSlot {
    int x0; // screen top-left
    int y0;
    int listId; // 0 player inv, 1 player offer, 2 NPC offer, 3 NPC inv
    int row; // visible row within the list
};

// How much of the stack under the cursor one press moves.
enum BarterAmount {
    BARTER_AMOUNT_ONE, // A
    BARTER_AMOUNT_ALL, // LT + A
    BARTER_AMOUNT_PICK, // RT -- the quantity picker
};

// Stick deflection that counts as a direction, and how fast a held direction repeats.
#define BARTER_STICK_THRESHOLD 0.5f
#define BARTER_NAV_REPEAT_MS 160

// Rebuilds the flat left-to-right list of the four trade lists' occupied visible slots
// (player inventory -> player offer -> NPC offer -> NPC inventory). Returns the count;
// fills slots[] (cap 4*gInventorySlotsCount) and the trade window's screen rect.
static int barter_collect_slots(BarterSlot* slots, int cap, Rect* winRect)
{
    if (windowGetRect(gInventoryWindow, winRect) != 0) {
        return 0;
    }

    struct {
        int x;
        int y0;
        int count;
    } cols[4] = {
        { INVENTORY_TRADE_LEFT_SCROLLER_X, INVENTORY_TRADE_SCROLLER_Y, _pud->length - _stack_offset[_curr_stack] },
        { INVENTORY_TRADE_INNER_LEFT_SCROLLER_X, INVENTORY_TRADE_INNER_SCROLLER_Y, _ptable_pud->length - _ptable_offset },
        { INVENTORY_TRADE_INNER_RIGHT_SCROLLER_X, INVENTORY_TRADE_INNER_SCROLLER_Y, _btable_pud->length - _btable_offset },
        { INVENTORY_TRADE_RIGHT_SCROLLER_X, INVENTORY_TRADE_SCROLLER_Y, _target_pud->length - _target_stack_offset[_target_curr_stack] },
    };

    int n = 0;
    for (int c = 0; c < 4; c++) {
        int count = cols[c].count;
        if (count > gInventorySlotsCount) {
            count = gInventorySlotsCount;
        }
        for (int i = 0; i < count && n < cap; i++) {
            slots[n].x0 = winRect->left + cols[c].x;
            slots[n].y0 = winRect->top + cols[c].y0 + i * INVENTORY_SLOT_HEIGHT;
            slots[n].listId = c;
            slots[n].row = i;
            n++;
        }
    }
    return n;
}

// Moves the mouse cursor to (sx, sy) AND redraws it: _mouse_set_position alone only
// updates coordinates (mouse.cc:744) -- it does NOT erase the old cursor sprite or
// blit the new one, which leaves a ghost trail. Replicate what mouse_simulate_input
// does on a real move: refresh the window under the old rect, then re-show at the new.
static void barter_warp_cursor(int sx, int sy)
{
    int curX;
    int curY;
    mouseGetPosition(&curX, &curY);
    if (sx == curX && sy == curY) {
        return;
    }

    Rect oldRect;
    mouseGetRect(&oldRect);
    _mouse_set_position(sx, sy);
    windowRefreshAll(&oldRect);
    mouseShowCursor();
}

// Transfers the item in (listId, row): player inventory / NPC inventory slots move the
// item into that side's offer table; an offer-table slot moves it back.
//
// This calls itemMoveForce DIRECTLY rather than the mouse handlers
// _barter_move_inventory / _barter_move_from_table_inventory: those are DRAG-and-DROP
// (they pick the item up, wait for the mouse button to release, then require the cursor
// to be over the DESTINATION offer-table drop zone). Driven from a controller the cursor
// stays on the SOURCE slot, so that drop-zone hit-test failed and nothing moved -- the
// "A does nothing" bug. Moving the WHOLE stack in one press also kills the "click caps 60
// times" grind for stacked items. The src/dst pairs mirror those handlers' final
// itemMoveForce calls exactly.
static void barter_redraw_tables()
{
    _display_target_inventory(_target_stack_offset[_target_curr_stack], -1, _target_pud, INVENTORY_WINDOW_TYPE_TRADE);
    _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_TRADE);
    inventoryWindowRenderInnerInventories(_barter_back_win, _ptable, _btable, -1);
}

// The item shown in (listId, visible row), or NULL if that slot is empty. Same four-list mapping
// barter_transfer_slot uses -- kept beside it so the two can never drift about which list is which.
static Object* barter_slot_item(int listId, int row)
{
    switch (listId) {
    case 0: { // player inventory
        int idx = row + _stack_offset[_curr_stack];
        return idx < _pud->length ? _pud->items[_pud->length - (idx + 1)].item : NULL;
    }
    case 1: { // player's offer table
        int idx = row + _ptable_offset;
        return idx < _ptable_pud->length ? _ptable_pud->items[_ptable_pud->length - (idx + 1)].item : NULL;
    }
    case 2: { // NPC's offer table
        int idx = row + _btable_offset;
        return idx < _btable_pud->length ? _btable_pud->items[_btable_pud->length - (idx + 1)].item : NULL;
    }
    case 3: { // NPC inventory
        int idx = row + _target_stack_offset[_target_curr_stack];
        return idx < _target_pud->length ? _target_pud->items[_target_pud->length - (idx + 1)].item : NULL;
    }
    default:
        return NULL;
    }
}

// Put the cursor back on the nearest occupied slot after an item has moved lists, so repeated
// A / LB / RB keep flowing instead of stranding it on a slot that is now empty.
static void barter_reanchor_cursor(int mouseX, int mouseY)
{
    Rect winRect;
    BarterSlot after[4 * 3];
    int nAfter = barter_collect_slots(after, 4 * 3, &winRect);
    if (nAfter <= 0) {
        return;
    }

    int nearest = 0;
    long bestDist = LONG_MAX;
    for (int i = 0; i < nAfter; i++) {
        long dx = after[i].x0 + INVENTORY_SLOT_WIDTH / 2 - mouseX;
        long dy = after[i].y0 + INVENTORY_SLOT_HEIGHT / 2 - mouseY;
        long d = dx * dx + dy * dy;
        if (d < bestDist) {
            bestDist = d;
            nearest = i;
        }
    }

    barter_warp_cursor(after[nearest].x0 + INVENTORY_SLOT_WIDTH / 2,
        after[nearest].y0 + INVENTORY_SLOT_HEIGHT / 2);
}

static void barter_transfer_slot(int listId, int row, BarterAmount amount)
{
    Object* npc = _target_stack[0]; // the NPC we are trading with

    Object* item = NULL;
    int qty = 0;
    Object* src = NULL;
    Object* dst = NULL;

    switch (listId) {
    case 0: { // player inventory -> player's offer
        int idx = row + _stack_offset[_curr_stack];
        if (idx >= _pud->length) {
            return;
        }
        item = _pud->items[_pud->length - (idx + 1)].item;
        qty = _pud->items[_pud->length - (idx + 1)].quantity;
        src = _inven_dude;
        dst = _ptable;
        break;
    }
    case 1: { // player's offer -> back to player inventory
        int idx = row + _ptable_offset;
        if (idx >= _ptable_pud->length) {
            return;
        }
        item = _ptable_pud->items[_ptable_pud->length - (idx + 1)].item;
        qty = _ptable_pud->items[_ptable_pud->length - (idx + 1)].quantity;
        src = _ptable;
        dst = _inven_dude;
        break;
    }
    case 2: { // NPC's offer -> back to NPC inventory
        int idx = row + _btable_offset;
        if (idx >= _btable_pud->length) {
            return;
        }
        item = _btable_pud->items[_btable_pud->length - (idx + 1)].item;
        qty = _btable_pud->items[_btable_pud->length - (idx + 1)].quantity;
        src = _btable;
        dst = npc;
        break;
    }
    case 3: { // NPC inventory -> NPC's offer
        int idx = row + _target_stack_offset[_target_curr_stack];
        if (idx >= _target_pud->length) {
            return;
        }
        item = _target_pud->items[_target_pud->length - (idx + 1)].item;
        qty = _target_pud->items[_target_pud->length - (idx + 1)].quantity;
        src = npc;
        dst = _btable;
        break;
    }
    default:
        return;
    }

    if (item == NULL || qty <= 0) {
        return;
    }

    // ⚠️ A MOVES ONE, LT+A MOVES THE LOT, LT+A HELD ASKS. His call: "Ačko přidá jeden item a LT
    // všechny", then "a jak přidám some, třeba kdybych chtěl přidat 3 stimpacks?".
    //
    // Three stimpaks are three taps, which is fewer presses than the picker used to cost to open,
    // fill in and confirm -- and that is why the picker is no longer on the tap. But thirty out of
    // fifty is not thirty taps, so it did not simply go away either: it moved onto the HOLD, where
    // it is there when you ask for it and never in the way when you do not. ⚠️ The same shape the
    // rest of this screen uses: LT decides how much, tap versus hold decides how you say it.
    if (qty > 1) {
        if (amount == BARTER_AMOUNT_ONE) {
            qty = 1;
        } else if (amount == BARTER_AMOUNT_PICK) {
            int chosen = inventoryQuantitySelect(INVENTORY_WINDOW_TYPE_MOVE_ITEMS, item, qty);
            if (chosen < 1) {
                return; // cancelled
            }
            qty = chosen;
        }
    }

    if (itemMoveForce(src, dst, item, qty) == -1) {
        MessageListItem messageListItem;
        messageListItem.num = 25; // at maximum weight / no space
        if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
            displayMonitorAddMessage(messageListItem.text);
        }
        return;
    }

    soundPlayFile("iputdown");
    barter_redraw_tables();
}

// CE: "Auto-match offer" (gamepad X). Tops up the PLAYER's offer table with exactly
// enough caps to meet the NPC's current asking price for whatever is on their offer
// table -- so buying an item priced at 60 is one button, not 60 cap clicks. The price
// is _barter_compute_value (the same value _barter_attempt_transaction checks, so after
// this the trade will be accepted); the shortfall is filled from the player's spare
// caps (capped at what they actually carry).
// Finds the top-level caps (money) stack in obj's inventory. Returns the item (or NULL) and
// its quantity via *qtyOut.
static Object* barter_find_money(Object* obj, int* qtyOut)
{
    Inventory* inv = &(obj->data.inventory);
    for (int i = 0; i < inv->length; i++) {
        if (inv->items[i].item->pid == PROTO_ID_MONEY) {
            *qtyOut = inv->items[i].quantity;
            return inv->items[i].item;
        }
    }
    *qtyOut = 0;
    return NULL;
}

// X in the barter window: even the deal out with caps, so a single press finishes it instead of
// clicking caps one by one. Bidirectional (see barter_auto_offer for the scale it measures on):
//   SHORT    -- the player owes for the NPC's offer -> the trader's caps come off ITS table first,
//               then the DUDE's caps go onto the player's table.
//   OVERPAY  -- the player's offer is worth more than the trader's -> the player's own caps come
//               back off the player's table first, then the TRADER's caps go onto its table.
// Each side is capped at the caps that side actually carries.
// THE DEAL READOUT -- the number the game decides on, finally on screen.
//
// ⚠️ VANILLA SHOWS YOU TWO NUMBERS AND JUDGES YOU ON A THIRD. The $N under each table is the RAW
// objectGetCost of that table, but _barter_attempt_transaction refuses unless
// _barter_compute_value(...) <= objectGetCost(ptable) -- and _barter_compute_value applies the
// Barter-skill / Master Trader / reputation modifier to the NPC's side. So two piles reading $100
// and $100 are routinely rejected, because what the trader actually wants is $130, and nothing
// anywhere told you that. That is the whole "trading is bad" experience: not a control problem, an
// information problem.
//
// This draws what the check really compares -- what you are GIVING against what they WANT after the
// markup -- plus the exact shortfall, which is also precisely the number X (barter_auto_offer)
// covers with caps. The two features finally describe the same transaction.
static void barter_draw_deal()
{
    if (_ptable == NULL || _btable == NULL || _target_stack[0] == NULL) {
        return;
    }

    unsigned char* windowBuffer = windowGetBuffer(gInventoryWindow);
    if (windowBuffer == NULL) {
        return;
    }

    int give = objectGetCost(_ptable);                           // raw value of the player's pile
    int want = _barter_compute_value(_inven_dude, _target_stack[0]);  // adjusted price of the NPC's pile
    int shortBy = want - give;

    int oldFont = fontGetCurrent();
    fontSetCurrent(101);
    int th = fontGetLineHeight();

    // ⚠️ THE LAYOUT IS DICTATED BY FOUR 64px COLUMNS AT x 29 / 165 / 250 / 388. The first attempt
    // put this in the gap between the two inner columns, which measures 250 - (165+64) = 17 PIXELS --
    // enough for nothing. The usable space is the strip to the RIGHT of the NPC's $total (drawn at
    // x 254) and before the NPC's inventory column starts at 388.
    const int dealX = 300;
    const int dealW = INVENTORY_TRADE_RIGHT_SCROLLER_X - dealX - 2;
    const int dealY = INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + 24;

    // Both strips are FILLED first: this is background art underneath, not a flat panel, so text
    // drawn straight onto it would sit on whatever the frame happens to show there.
    if (dealW > 8) {
        bufferFill(windowBuffer + INVENTORY_TRADE_WINDOW_WIDTH * dealY + dealX,
            dealW, th + 2, INVENTORY_TRADE_WINDOW_WIDTH, _colorTable[0]);

        // OVERPAY is a deal the trader accepts but you are losing on -- exactly what X evens out, so
        // it is shown rather than hidden under "DEAL OK" (which is how a lopsided trade looked fine).
        char text[32];
        int color;
        if (shortBy > 0) {
            snprintf(text, sizeof(text), "SHORT %d", shortBy);
            color = _colorTable[31744];
        } else if (shortBy < 0) {
            snprintf(text, sizeof(text), "OVERPAY %d", -shortBy);
            color = _colorTable[32747];
        } else {
            snprintf(text, sizeof(text), "DEAL OK");
            color = _colorTable[992];
        }

        int tw = fontGetStringWidth(text);
        int tx = dealX + (dealW - tw) / 2;
        if (tx < dealX) {
            tx = dealX;
        }
        fontDrawText(windowBuffer + INVENTORY_TRADE_WINDOW_WIDTH * (dealY + 1) + tx,
            text, dealW, INVENTORY_TRADE_WINDOW_WIDTH, color);
    }

    // THE CONTROL LEGEND, on screen where the controls are actually used. The Pip-Boy CONTROLS page
    // lists these too, but a page you have to go looking for is how "even out with caps" and the
    // split-stack picker sat unused for weeks -- if a control is not visible where it applies, it
    // does not exist. The top strip is free: the inner item columns start at y 20 and the outer at
    // y 30, so nothing is drawn across this band.
    // Full window width, not just the gap between the outer columns: at y 4 NOTHING is drawn
    // yet -- the outer item columns start at y 30 and the inner ones at y 20 -- so the whole
    // 480px strip is free, and the legend needs it once the navigation keys are in there too.
    const int legX = 6;
    const int legW = INVENTORY_TRADE_WINDOW_WIDTH - 12;
    const int legY = 4;

    if (legW > 40) {
        bufferFill(windowBuffer + INVENTORY_TRADE_WINDOW_WIDTH * legY + legX,
            legW, th + 2, INVENTORY_TRADE_WINDOW_WIDTH, _colorTable[0]);

        // ⚠️ THE LEGEND MUST NAME THE ACTUAL AXES. It read "LB/RB item" back when the shoulders
        // walked the flat left-to-right list of every occupied slot; they now step COLUMN (empty
        // ones skipped) and the D-pad steps ITEM within one column. Wording and binding change
        // together -- a legend promising a jump the buttons do not make is worse than none.
        const char* legend = "LB/RB column   D-pad item   A move   LT+A some   X even   Y offer   B exit";
        int lw = fontGetStringWidth(legend);
        int lx = legX + (legW - lw) / 2;
        if (lx < legX) {
            lx = legX;
        }
        fontDrawText(windowBuffer + INVENTORY_TRADE_WINDOW_WIDTH * (legY + 1) + lx,
            legend, legW, INVENTORY_TRADE_WINDOW_WIDTH, _colorTable[32767]);
    }

    fontSetCurrent(oldFont);

    Rect r;
    r.left = legX < dealX ? legX : dealX;
    r.top = legY;
    r.right = (legX + legW) > (dealX + dealW) ? (legX + legW) : (dealX + dealW);
    r.bottom = dealY + th + 2;
    windowRefreshRect(gInventoryWindow, &r);
}

// ⚠️ "EVEN" IS MEASURED ON THE SCALE THE TRADER JUDGES WITH, AND THAT SCALE IS NOT THE PRICE TAG.
//
// The deal check is want <= give, where want = _barter_compute_value (the trader's pile with the
// Barter-skill markup -- or markdown: a good trader pays well under the tag) and give = the RAW value
// of the player's pile. The SELL side used to balance against the trader's RAW total instead, which
// only agrees with `want` while the trader's pile is nothing but caps. Put a few items on BOTH sides
// and the two scales part: measured in trace_2026-10-04_153226, want 2131 against give 5022 -- the
// player overpaying by 2891 -- was "evened" with 822 caps, and every X after that refused as
// "already even" while the deal was still 2065 in the trader's favour ("even nefunguje správně s
// více itemy").
//
// Caps are the one thing BOTH scales count 1:1 (_barter_compute_value takes them out before the
// modifier), so moving N caps moves the gap by exactly N. Each direction first takes back caps
// from the side that is ahead -- the player's own caps off the player's table, or the trader's off
// the trader's -- and only then adds new ones, so a press never shuffles caps both ways at once.
static void barter_auto_offer()
{
    Object* npc = _target_stack[0];

    int want = _barter_compute_value(_inven_dude, npc); // what the trader asks for its pile
    int give = objectGetCost(_ptable);               // what the player's pile is worth to it
    int gap = want - give;                            // > 0 player is short, < 0 player overpays

    int dudeCaps = 0;
    barter_find_money(_inven_dude, &dudeCaps);
    int npcCaps = 0;
    barter_find_money(npc, &npcCaps);
    int ptableCaps = 0;
    barter_find_money(_ptable, &ptableCaps);
    int btableCaps = 0;
    barter_find_money(_btable, &btableCaps);

    // TEMP DIAG (strip after diagnosing barter auto-caps): flight-recorder ONLY.
    // NOTE: displayMonitorAddMessage CRASHES inside the barter/dialog modal (message monitor buffer
    // invalid), so diagnostics MUST go to the trace, never the screen.
    trace_event(TRACE_FLOW, "barter-auto",
        "want=%d give=%d gap=%d dudeCaps=%d npcCaps=%d ptableCaps=%d btableCaps=%d",
        want, give, gap, dudeCaps, npcCaps, ptableCaps, btableCaps);

    // Move up to `amount` caps from `from` to `to`; returns how many actually went. The money object
    // is looked up again for every move -- a move can merge or empty a stack, and a pointer fetched
    // before it may no longer be the stack it was.
    auto moveCaps = [](Object* from, Object* to, int amount) -> int {
        int have = 0;
        Object* money = barter_find_money(from, &have);
        if (money == NULL || have <= 0 || amount <= 0) {
            return 0;
        }
        int n = amount < have ? amount : have;
        return itemMoveForce(from, to, money, n) == 0 ? n : 0;
    };

    const char* why = NULL;
    int moved = 0;

    if (gap > 0) {
        // SHORT: the trader's own caps off its table first (each one off its pile is one off the
        // price), then the rest out of the player's pocket.
        int left = gap;
        int n = moveCaps(_btable, npc, left);
        moved += n;
        left -= n;
        if (left > 0) {
            n = moveCaps(_inven_dude, _ptable, left);
            moved += n;
            left -= n;
        }
        if (moved == 0) {
            why = dudeCaps <= 0 ? "no-caps-dude" : "move-failed";
        }
    } else if (gap < 0) {
        // OVERPAYING: the player's own caps back off the player's table first, then the trader pays
        // the rest into its offer.
        int left = -gap;
        int n = moveCaps(_ptable, _inven_dude, left);
        moved += n;
        left -= n;
        if (left > 0) {
            n = moveCaps(npc, _btable, left);
            moved += n;
            left -= n;
        }
        if (moved == 0) {
            why = npcCaps <= 0 ? "no-caps-npc" : "move-failed";
        }
    } else {
        why = "already-even";
    }

    if (moved > 0) {
        soundPlayFile("iputdown");
        barter_redraw_tables();
        trace_event(TRACE_INPUT, "barter-auto-moved", "caps=%d gap=%d", moved, gap);
        return;
    }

    // ⚠️ IT MUST NEVER REFUSE IN SILENCE. "even xko mi někdy nefungovalo" -- and it had not failed,
    // it had DECLINED, three different ways, with no sound and no message: nobody is short of
    // anything (the deal is already even), the buyer has no caps, the seller has no caps. A button
    // that does nothing and says nothing reads as broken every time. The refusal buzzer is the same
    // one the reload and every other refused action on this pad uses.
    //
    // ⚠️ And it has to be a SOUND, not text: displayMonitorAddMessage crashes inside the barter/dialog modal
    // (invalid message-monitor buffer), which is why the reason goes to the trace instead.
    soundPlayFile("iisxxxx1");
    trace_event(TRACE_INPUT, "barter-auto-refused", "why=%s gap=%d dudeCaps=%d npcCaps=%d",
        why != NULL ? why : "?", gap, dudeCaps, npcCaps);
}

// Total item count and current scroll offset of trade column c (0 player inv, 1 player
// offer, 2 NPC offer, 3 NPC inv). Used by the D-pad navigation to know how many items are
// below/above the visible window so it can scroll only at the edge.
static void barter_col_range(int c, int* length, int* offset)
{
    switch (c) {
    case 0:
        *length = _pud->length;
        *offset = _stack_offset[_curr_stack];
        break;
    case 1:
        *length = _ptable_pud->length;
        *offset = _ptable_offset;
        break;
    case 2:
        *length = _btable_pud->length;
        *offset = _btable_offset;
        break;
    default:
        *length = _target_pud->length;
        *offset = _target_stack_offset[_target_curr_stack];
        break;
    }
}

// Which of the four lists is the cursor nearest, by x alone? Used when the cursor is not over
// an occupied slot, so the shoulders and the D-pad anchor onto the same column.
static int barter_nearest_column(int rx)
{
    static const int colsX[4] = {
        INVENTORY_TRADE_LEFT_SCROLLER_X, INVENTORY_TRADE_INNER_LEFT_SCROLLER_X,
        INVENTORY_TRADE_INNER_RIGHT_SCROLLER_X, INVENTORY_TRADE_RIGHT_SCROLLER_X
    };

    int c = 0;
    int bestDx = INT_MAX;
    for (int i = 0; i < 4; i++) {
        int cx = colsX[i] + INVENTORY_SLOT_WIDTH / 2;
        int dx = rx < cx ? cx - rx : rx - cx;
        if (dx < bestDx) {
            bestDx = dx;
            c = i;
        }
    }
    return c;
}

// How many occupied visible rows does column c have in slots[]? barter_collect_slots fills each
// column top-down from row 0, so this doubles as "the row past the last one".
static int barter_col_rows(const BarterSlot* slots, int nSlots, int c)
{
    int rows = 0;
    for (int i = 0; i < nSlots; i++) {
        if (slots[i].listId == c) {
            rows++;
        }
    }
    return rows;
}

// Warp the cursor onto column c's visible row `row`, if that slot is present in slots[].
static void barter_warp_to_slot(const BarterSlot* slots, int nSlots, int c, int row)
{
    for (int i = 0; i < nSlots; i++) {
        if (slots[i].listId == c && slots[i].row == row) {
            barter_warp_cursor(slots[i].x0 + INVENTORY_SLOT_WIDTH / 2,
                slots[i].y0 + INVENTORY_SLOT_HEIGHT / 2);
            return;
        }
    }
}

static int barter_gamepad_input()
{
    static bool prevLB = false;
    static bool prevRB = false;
    static bool prevRT = false;
    static bool prevA = false;
    static bool prevX = false;
    static bool prevY = false;
    static bool prevB = false;
    static bool prevBack = false;
    static bool prevUp = false;
    static bool prevDown = false;

    SDL_GameController* pad = gamepad_ui_get_controller();
    if (pad == NULL) {
        prevLB = prevRB = prevRT = prevA = prevX = prevY = prevB = prevBack = prevUp = prevDown = false;
        return -1;
    }

    SDL_GameControllerUpdate();

    bool lbNow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER) != 0;
    bool rbNow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) != 0;
    bool aNow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A) != 0;
    bool xNow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_X) != 0;
    bool yNow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_Y) != 0;
    bool bNow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B) != 0;
    bool backNow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK) != 0;
    // RT asks how many. ⚠️ Read through gamepad_ui_split_trigger_held's sibling rule -- a held
    // trigger intermittently reports a hard 0 (measured: 14 of 33 readings), so a bare threshold
    // drops presses. Here it is only an EDGE on a press, so the Schmitt low bound is enough.
    bool rtNow = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > 12000;
    bool upNow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0;
    bool downNow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0;

    // ⚠️ SAMPLED EVERY FRAME, AND THAT IS NOT OPTIONAL. The split modifier is filtered by agreement
    // across readings (gamepad_ui_split_trigger_held), because this axis reports garbage in both
    // directions; asking it once, at the moment A is released, hands the filter a single sample and
    // it can then neither confirm a real pull nor out-vote a spurious one. This loop runs every
    // frame, so it is what feeds it.
    bool splitNow = gamepad_ui_split_trigger_held();

    // ⚠️ THE LEFT STICK STEPS TOO, AND EVERYTHING REPEATS WHEN HELD. His two asks in one breath:
    // *"Proč nepoužíváme i joystick? Můžu držet dpad aby se rychleji posouvalo?"*. The left stick is
    // free on this screen -- inventoryOpenTrade suppresses the cursor snap for its whole length -- so
    // it drives exactly the same four steps the D-pad does, and both of them now act on the first
    // frame and then repeat while held. The right stick stays the free pointer it always was.
    float navX = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX) / 32767.0f;
    float navY = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY) / 32767.0f;
    float navAbsX = navX < 0.0f ? -navX : navX;
    float navAbsY = navY < 0.0f ? -navY : navY;

    // The dominant axis wins outright: a diagonal must not step a row AND a column at once.
    bool stickUp = navAbsY >= navAbsX && navY < -BARTER_STICK_THRESHOLD;
    bool stickDown = navAbsY >= navAbsX && navY > BARTER_STICK_THRESHOLD;
    bool stickLeft = navAbsX > navAbsY && navX < -BARTER_STICK_THRESHOLD;
    bool stickRight = navAbsX > navAbsY && navX > BARTER_STICK_THRESHOLD;

    upNow = upNow || stickUp;
    downNow = downNow || stickDown;
    bool colLeftNow = lbNow || stickLeft;
    bool colRightNow = rbNow || stickRight;

    // One repeat clock for all four directions: the newest direction owns it, so sliding the stick
    // from down to left does not inherit the old timer and fire instantly.
    static int navHeld = 0; // 0 none, 1 up, 2 down, 3 left, 4 right
    static unsigned int navAt = 0;
    int navNow = upNow ? 1 : (downNow ? 2 : (colLeftNow ? 3 : (colRightNow ? 4 : 0)));
    bool navRepeat = false;
    if (navNow == 0) {
        navHeld = 0;
    } else if (navNow != navHeld) {
        navHeld = navNow;
        navAt = getTicks();
    } else if (getTicksSince(navAt) >= BARTER_NAV_REPEAT_MS) {
        navAt = getTicks();
        navRepeat = true;
    }

    bool lbEdge = (colLeftNow && !prevLB) || (navRepeat && navNow == 3);
    bool rbEdge = (colRightNow && !prevRB) || (navRepeat && navNow == 4);
    bool aEdge = aNow && !prevA;
    bool aReleased = !aNow && prevA;
    bool xEdge = xNow && !prevX;
    bool yEdge = yNow && !prevY;
    bool bEdge = bNow && !prevB;
    bool backEdge = backNow && !prevBack;
    bool rtEdge = rtNow && !prevRT;
    bool upEdge = (upNow && !prevUp) || (navRepeat && navNow == 1);
    bool downEdge = (downNow && !prevDown) || (navRepeat && navNow == 2);

    prevLB = colLeftNow;
    prevRB = colRightNow;
    prevRT = rtNow;
    prevA = aNow;
    prevX = xNow;
    prevY = yNow;
    prevB = bNow;
    prevBack = backNow;
    prevUp = upNow;
    prevDown = downNow;

    // X: auto-fill the player's offer with exactly the caps the NPC is asking for.
    if (xEdge) {
        barter_auto_offer();
        return -1;
    }

    Rect winRect;
    BarterSlot slots[4 * 3];
    int nSlots = barter_collect_slots(slots, 4 * 3, &winRect);

    int mouseX;
    int mouseY;
    mouseGetPosition(&mouseX, &mouseY);

    // Index of the occupied slot the cursor is over (fully inside its 64x48 rect), or -1.
    int over = -1;
    for (int i = 0; i < nSlots; i++) {
        if (mouseX >= slots[i].x0 && mouseX < slots[i].x0 + INVENTORY_SLOT_WIDTH
            && mouseY >= slots[i].y0 && mouseY < slots[i].y0 + INVENTORY_SLOT_HEIGHT) {
            over = i;
            break;
        }
    }

    // ⚠️ HOLD A TO LOOK AT AN ITEM. A trade screen without a description is guesswork -- you cannot
    // tell two similar guns apart, and there was no way at all to read one on a controller ("potřebuji
    // se podívat na item v shopu, nevím jak toho na controlleru dosáhnout"). The mouse reaches it
    // through the hold-drag action menu; the pad now reaches it the same way the INVENTORY already
    // does, by holding A, so it is one habit and not two.
    //
    // Printed with _obj_examine_func(..., gInventoryPrintItemDescriptionHandler) -- exactly what vanilla's own LOOK does in
    // this window (see GAME_MOUSE_ACTION_MENU_ITEM_LOOK: every non-NORMAL window uses gInventoryPrintItemDescriptionHandler,
    // because the inventory's description panel does not exist here).
    // DIAG (the "I could not get at the items in my own offer" report, 2026-09-25): one line per
    // navigation press, saying which slot the cursor is on, how many slots were collected at all,
    // and the length + scroll offset of all four lists. Between them those numbers say whether a
    // column was never collected, never scrolled, or simply never pointed at -- three different
    // bugs that I could not tell apart by reading the code. Strip with `move-diag`.
    if (trace_enabled() && (upEdge || downEdge || lbEdge || rbEdge)) {
        trace_event(TRACE_INPUT, "barter-nav",
            "over=%d col=%d row=%d slots=%d mine=%d/%d offer=%d/%d theirs=%d/%d npc=%d/%d m=%d,%d",
            over, over >= 0 ? slots[over].listId : -1, over >= 0 ? slots[over].row : -1, nSlots,
            _pud->length, _stack_offset[_curr_stack],
            _ptable_pud->length, _ptable_offset,
            _btable_pud->length, _btable_offset,
            _target_pud->length, _target_stack_offset[_target_curr_stack],
            mouseX - winRect.left, mouseY - winRect.top);
    }

    static unsigned int barterADownTime = 0;
    static bool barterAHandled = false;
    if (aEdge) {
        barterADownTime = getTicks();
        barterAHandled = false;
    }
    if (aNow && !barterAHandled && over >= 0
        && getTicksSince(barterADownTime) >= BARTER_LOOK_HOLD_MS) {
        barterAHandled = true;
        Object* looked = barter_slot_item(slots[over].listId, slots[over].row);
        if (looked != NULL) {
            _obj_examine_func(_inven_dude, looked, gInventoryPrintItemDescriptionHandler);
        }
        return -1;
    }

    // RT: HOW MANY? Opens the quantity picker for the item under the cursor.
    //
    // ⚠️ IT IS ITS OWN BUTTON BECAUSE THE COMBINATION WAS NOT WORTH IT. It lived on LT+A-held for
    // about ten minutes and he was right about it: "není to zbytečně komplikované? prostě přidej
    // jiný button". Three amounts on one button meant a tap, a modifier, and a hold length all
    // carrying meaning at once, and you had to get the order right. RT had no job on this screen at
    // all, and it sits beside LT, which is the other amount key. A = one, LT+A = all, RT = ask.
    if (rtEdge && over >= 0) {
        barter_transfer_slot(slots[over].listId, slots[over].row, BARTER_AMOUNT_PICK);
        barter_reanchor_cursor(mouseX, mouseY);
        return -1;
    }

    // A transfers the item under the cursor: ONE of it, or the WHOLE stack with LT held. On the
    // RELEASE edge, because the hold above owns the press: a tap moves, a hold describes.
    //
    // ⚠️ THE SPLIT STAYS ON LT WHILE EVERY OTHER COMBO MOVED TO LB (2026-08-29). The tempting move
    // is consistency -- LB is the modifier everywhere else now -- but LB is NOT free on this screen:
    // lbEdge steps the cursor to the previous column (see the rbEdge || lbEdge branch below).
    // Holding LB to split would jump the cursor off the item you were pointing at before A was ever
    // pressed, which breaks the gesture outright. LT has no job in a barter window, so it keeps it.
    if (aReleased && !barterAHandled && over >= 0) {
        // ⚠️ NEVER A BARE THRESHOLD ON A TRIGGER. A held trigger intermittently reports a hard 0
        // (measured: 14 of 33 readings), so `axis > 8000` dropped the split request about four
        // times in ten and moved the whole stack. gamepad_ui_split_trigger_held is the Schmitt
        // trigger + 60 ms release debounce every other analog read in this codebase goes through.
        int listId = slots[over].listId;
        int row = slots[over].row;

        // DIAG (the "A keeps moving the whole stack" report, 2026-09-24): one line per press,
        // naming the slot, what the trigger ACTUALLY read, what the filter made of it, and which
        // amount that chose. Reported three times and never once measured -- this is what tells a
        // stuck modifier apart from a stuck stack. Strip with `move-diag`.
        if (trace_enabled()) {
            int rawLt = 0;
            int level = 0;
            gamepad_ui_split_trigger_debug(&rawLt, &level);
            Object* it = barter_slot_item(listId, row);
            trace_event(TRACE_INPUT, "barter-a", "list=%d row=%d pid=%d lt=%d lvl=%d amount=%s",
                listId, row, it != NULL ? it->pid : -1, rawLt, level, splitNow ? "ALL" : "ONE");
        }

        barter_transfer_slot(listId, row, splitNow ? BARTER_AMOUNT_ALL : BARTER_AMOUNT_ONE);

        barter_reanchor_cursor(mouseX, mouseY);
        return -1;
    }

    // LB/RB: step to the previous/next COLUMN. The shoulders are the column axis, the D-pad is
    // the item axis -- one gesture, one meaning. They used to walk the flat left-to-right list of
    // every occupied slot, crossing into the next column only once the current one ran out, so
    // "go to the other pile" cost as many presses as that pile had items.
    //
    // Empty columns are SKIPPED, not landed on: with no occupied slot there is nothing to point
    // at and A would do nothing, so stopping there would just strand the cursor. The row carries
    // across (clamped to what the target column has), so a sideways step lands level.
    if ((rbEdge || lbEdge) && nSlots > 0) {
        int c;
        int row;
        if (over >= 0) {
            c = slots[over].listId;
            row = slots[over].row;
        } else {
            c = barter_nearest_column(mouseX - winRect.left);
            row = 0;
        }

        // ⚠️ STEP INTO THE NEXT COLUMN EVEN WHEN IT IS EMPTY. Empty columns used to be skipped --
        // "there is nothing to point at and A would do nothing there" -- and that reasoning was
        // wrong in the hand: an offer table is empty exactly at the start of every trade, so the
        // shoulder button silently jumped PAST the table you were trying to reach and the player
        // could not tell whether he had pressed it at all ("když nejsou itemy v columnu, tak na to
        // neskáče LB nebo RB"). A cursor sitting on an empty slot is honest -- it shows where you
        // are -- and the actions there simply do nothing, which is the same answer the mouse gives.
        //
        // One step, one column, always: no searching, no wrapping past anything.
        int dir = rbEdge ? 1 : -1;
        int target = (c + dir + 4) % 4;
        int rows = barter_col_rows(slots, nSlots, target);
        if (rows > 0) {
            barter_warp_to_slot(slots, nSlots, target, row < rows ? row : rows - 1);
        } else {
            // No slots collected for that column -- it is empty, so put the cursor on where its
            // first row WOULD be. The geometry is fixed by the window art, not by the contents.
            static const int colsX[4] = {
                INVENTORY_TRADE_LEFT_SCROLLER_X, INVENTORY_TRADE_INNER_LEFT_SCROLLER_X,
                INVENTORY_TRADE_INNER_RIGHT_SCROLLER_X, INVENTORY_TRADE_RIGHT_SCROLLER_X
            };
            barter_warp_cursor(winRect.left + colsX[target] + INVENTORY_SLOT_WIDTH / 2,
                winRect.top + INVENTORY_TRADE_SCROLLER_Y + INVENTORY_SLOT_HEIGHT / 2);
        }
        return -1;
    }

    // Y offers the trade; B / BACK leave.
    if (yEdge) {
        return KEY_LOWERCASE_M;
    }
    if (bEdge || backEdge) {
        return KEY_ESCAPE;
    }

    // D-pad up/down: move the cursor between items in one column. It advances the SELECTION
    // through the visible slots first and only SCROLLS the list once the cursor is already on
    // the top/bottom visible slot and there are more items off-screen. This is what lets you
    // reach the middle and last items -- the old "scroll only" behaviour left the cursor stuck
    // on the first visible slot, so the 2nd/3rd items were unreachable once scrolling maxed out.
    if (upEdge || downEdge) {
        static const int upKeys[4] = { KEY_ARROW_UP, KEY_PAGE_UP, KEY_CTRL_PAGE_UP, KEY_CTRL_ARROW_UP };
        static const int downKeys[4] = { KEY_ARROW_DOWN, KEY_PAGE_DOWN, KEY_CTRL_PAGE_DOWN, KEY_CTRL_ARROW_DOWN };

        // Which column, and which visible row within it, is the cursor on? If it isn't over a
        // slot yet, anchor onto the nearest column (first slot on a down press, last on up).
        int c;
        int row;
        if (over >= 0) {
            c = slots[over].listId;
            row = slots[over].row;
        } else {
            c = barter_nearest_column(mouseX - winRect.left);
            row = -1;
        }

        int length;
        int offset;
        barter_col_range(c, &length, &offset);
        int remaining = length - offset;
        int visible = remaining < gInventorySlotsCount ? remaining : gInventorySlotsCount;
        if (visible <= 0) {
            return -1;
        }

        if (row < 0) {
            // Not anchored yet: land on the first (down) / last (up) visible item.
            barter_warp_to_slot(slots, nSlots, c, downEdge ? 0 : visible - 1);
            return -1;
        }

        if (downEdge) {
            if (row + 1 < visible) {
                barter_warp_to_slot(slots, nSlots, c, row + 1); // next item down
                return -1;
            }
            if (remaining > gInventorySlotsCount) {
                return downKeys[c]; // at bottom edge, more below -> scroll (cursor stays on last row)
            }
            return -1; // already on the last item
        }

        // upEdge
        if (row - 1 >= 0) {
            barter_warp_to_slot(slots, nSlots, c, row - 1); // next item up
            return -1;
        }
        if (offset > 0) {
            return upKeys[c]; // at top edge, more above -> scroll (cursor stays on first row)
        }
        return -1; // already on the first item
    }

    return -1;
}

// 0x4757F0
void inventoryOpenTrade(int win, Object* barterer, Object* playerTable, Object* bartererTable, int barterMod)
{
    ScopedGameMode gm(GameMode::kBarter);

    _barter_mod = barterMod;

    if (inventoryCommonInit() == -1) {
        return;
    }

    Object* armor = critterGetArmor(barterer);
    if (armor != nullptr) {
        itemRemove(barterer, armor, 1);
    }

    Object* item1 = nullptr;
    Object* item2 = critterGetItem2(barterer);
    if (item2 != nullptr) {
        itemRemove(barterer, item2, 1);
    } else {
        if (!gGameDialogSpeakerIsPartyMember) {
            item1 = _inven_find_type(barterer, ITEM_TYPE_WEAPON, nullptr);
            if (item1 != nullptr) {
                itemRemove(barterer, item1, 1);
            }
        }
    }

    Object* hiddenBox = nullptr;
    if (objectCreateWithFidPid(&hiddenBox, 0, PROTO_ID_JESSE_CONTAINER) == -1) {
        return;
    }

    _pud = &(_inven_dude->data.inventory);
    _btable = bartererTable;
    _ptable = playerTable;

    _ptable_offset = 0;
    _btable_offset = 0;

    _ptable_pud = &(playerTable->data.inventory);
    _btable_pud = &(bartererTable->data.inventory);

    _barter_back_win = win;
    _target_curr_stack = 0;
    _target_pud = &(barterer->data.inventory);

    _target_stack[0] = barterer;
    _target_stack_offset[0] = 0;

    bool isoWasEnabled = _setup_inventory(INVENTORY_WINDOW_TYPE_TRADE);
    _display_target_inventory(_target_stack_offset[_target_curr_stack], -1, _target_pud, INVENTORY_WINDOW_TYPE_TRADE);
    _display_inventory(_stack_offset[0], -1, INVENTORY_WINDOW_TYPE_TRADE);
    _display_body(barterer->fid, INVENTORY_WINDOW_TYPE_TRADE);
    windowRefresh(_barter_back_win);
    inventoryWindowRenderInnerInventories(win, playerTable, bartererTable, -1);

    inventorySetCursor(INVENTORY_WINDOW_CURSOR_HAND);

    int modifier;
    int npcReactionValue = reactionGetValue(barterer);
    int npcReactionType = reactionTranslateValue(npcReactionValue);
    switch (npcReactionType) {
    case NPC_REACTION_BAD:
        modifier = 25;
        break;
    case NPC_REACTION_NEUTRAL:
        modifier = 0;
        break;
    case NPC_REACTION_GOOD:
        modifier = -15;
        break;
    default:
        assert(false && "Should be unreachable");
    }

    // CE: while the barter window owns the screen the left-stick item snap stands down (it would grab
    // the slot buttons and fight the LB/RB stepping). Cleared right after the loop.
    gamepad_ui_set_suppress_snap(true);

    // CE: START ON YOUR OWN FIRST ITEM, on a controller (never under a mouse player's hand).
    if (gamepad_ui_get_controller() != nullptr) {
        Rect bWinRect;
        BarterSlot bSlots[4 * 3];
        int bCount = barter_collect_slots(bSlots, 4 * 3, &bWinRect);
        if (bCount > 0) {
            barter_warp_to_slot(bSlots, bCount, 0, 0);
        }
    }

    int keyCode = -1;
    for (;;) {
        sharedFpsLimiter.mark();

        if (keyCode == KEY_ESCAPE || _game_user_wants_to_quit != 0) {
            break;
        }

        keyCode = inputGetInput();

        // CE: the pad in the trade window. Runs every frame (LB/RB move the cursor as a side effect);
        // its injected keyCode only applies when nothing else did.
        int padKey = barter_gamepad_input();
        if (keyCode == -1 && padKey != -1) {
            keyCode = padKey;
        }

        if (keyCode == KEY_CTRL_Q || keyCode == KEY_CTRL_X || keyCode == KEY_F10) {
            showQuitConfirmationDialog();
        }

        if (_game_user_wants_to_quit != 0) {
            break;
        }

        _barter_mod = barterMod + modifier;

        if (keyCode == KEY_LOWERCASE_T || modifier <= -30) {
            itemMoveAll(bartererTable, barterer);
            itemMoveAll(playerTable, gDude);
            _barter_end_to_talk_to();
            break;
        } else if (keyCode == KEY_LOWERCASE_M) {
            if (playerTable->data.inventory.length != 0 || _btable->data.inventory.length != 0) {
                if (_barter_attempt_transaction(_inven_dude, playerTable, barterer, bartererTable) == 0) {
                    _display_target_inventory(_target_stack_offset[_target_curr_stack], -1, _target_pud, INVENTORY_WINDOW_TYPE_TRADE);
                    _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_TRADE);
                    inventoryWindowRenderInnerInventories(win, playerTable, bartererTable, -1);

                    // Ok, that's a good trade.
                    MessageListItem messageListItem;
                    messageListItem.num = 27;
                    if (!gGameDialogSpeakerIsPartyMember) {
                        if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
                            gameDialogRenderSupplementaryMessage(messageListItem.text);
                        }
                    }
                }
            }
        } else if (keyCode == KEY_ARROW_UP) {
            if (_stack_offset[_curr_stack] > 0) {
                _stack_offset[_curr_stack] -= 1;
                _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_TRADE);
            }
        } else if (keyCode == KEY_PAGE_UP) {
            if (_ptable_offset > 0) {
                _ptable_offset -= 1;
                inventoryWindowRenderInnerInventories(win, playerTable, bartererTable, -1);
            }
        } else if (keyCode == KEY_ARROW_DOWN) {
            if (_stack_offset[_curr_stack] + gInventorySlotsCount < _pud->length) {
                _stack_offset[_curr_stack] += 1;
                _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_TRADE);
            }
        } else if (keyCode == KEY_PAGE_DOWN) {
            if (_ptable_offset + gInventorySlotsCount < _ptable_pud->length) {
                _ptable_offset += 1;
                inventoryWindowRenderInnerInventories(win, playerTable, bartererTable, -1);
            }
        } else if (keyCode == KEY_CTRL_PAGE_DOWN) {
            if (_btable_offset + gInventorySlotsCount < _btable_pud->length) {
                _btable_offset++;
                inventoryWindowRenderInnerInventories(win, playerTable, bartererTable, -1);
            }
        } else if (keyCode == KEY_CTRL_PAGE_UP) {
            if (_btable_offset > 0) {
                _btable_offset -= 1;
                inventoryWindowRenderInnerInventories(win, playerTable, bartererTable, -1);
            }
        } else if (keyCode == KEY_CTRL_ARROW_UP) {
            if (_target_stack_offset[_target_curr_stack] > 0) {
                _target_stack_offset[_target_curr_stack] -= 1;
                _display_target_inventory(_target_stack_offset[_target_curr_stack], -1, _target_pud, INVENTORY_WINDOW_TYPE_TRADE);
                windowRefresh(gInventoryWindow);
            }
        } else if (keyCode == KEY_CTRL_ARROW_DOWN) {
            if (_target_stack_offset[_target_curr_stack] + gInventorySlotsCount < _target_pud->length) {
                _target_stack_offset[_target_curr_stack] += 1;
                _display_target_inventory(_target_stack_offset[_target_curr_stack], -1, _target_pud, INVENTORY_WINDOW_TYPE_TRADE);
                windowRefresh(gInventoryWindow);
            }
        } else if (keyCode >= 2500 && keyCode <= 2501) {
            _container_exit(keyCode, INVENTORY_WINDOW_TYPE_TRADE);
        } else {
            if ((mouseGetEvent() & MOUSE_EVENT_RIGHT_BUTTON_DOWN) != 0) {
                if (gInventoryCursor == INVENTORY_WINDOW_CURSOR_HAND) {
                    inventorySetCursor(INVENTORY_WINDOW_CURSOR_ARROW);
                } else {
                    inventorySetCursor(INVENTORY_WINDOW_CURSOR_HAND);
                }
            } else if ((mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_DOWN) != 0) {
                if (keyCode >= 1000 && keyCode <= 1000 + gInventorySlotsCount) {
                    if (gInventoryCursor == INVENTORY_WINDOW_CURSOR_ARROW) {
                        inventoryWindowOpenContextMenu(keyCode, INVENTORY_WINDOW_TYPE_TRADE);
                        inventoryWindowRenderInnerInventories(win, playerTable, nullptr, -1);
                    } else {
                        int slotIndex = keyCode - 1000;
                        if (slotIndex + _stack_offset[_curr_stack] < _pud->length) {
                            int offset = _stack_offset[_curr_stack];
                            InventoryItem* inventoryItem = &(_pud->items[_pud->length - (slotIndex + offset + 1)]);
                            _barter_move_inventory(inventoryItem->item, inventoryItem->quantity, slotIndex, offset, barterer, playerTable, true);
                            _display_target_inventory(_target_stack_offset[_target_curr_stack], -1, _target_pud, INVENTORY_WINDOW_TYPE_TRADE);
                            _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_TRADE);
                            inventoryWindowRenderInnerInventories(win, playerTable, nullptr, -1);
                        }
                    }

                    keyCode = -1;
                } else if (keyCode >= 2000 && keyCode <= 2000 + gInventorySlotsCount) {
                    if (gInventoryCursor == INVENTORY_WINDOW_CURSOR_ARROW) {
                        inventoryWindowOpenContextMenu(keyCode, INVENTORY_WINDOW_TYPE_TRADE);
                        inventoryWindowRenderInnerInventories(win, nullptr, bartererTable, -1);
                    } else {
                        int slotIndex = keyCode - 2000;
                        if (slotIndex + _target_stack_offset[_target_curr_stack] < _target_pud->length) {
                            int stackOffset = _target_stack_offset[_target_curr_stack];
                            InventoryItem* inventoryItem = &(_target_pud->items[_target_pud->length - (slotIndex + stackOffset + 1)]);
                            _barter_move_inventory(inventoryItem->item, inventoryItem->quantity, slotIndex, stackOffset, barterer, bartererTable, false);
                            _display_target_inventory(_target_stack_offset[_target_curr_stack], -1, _target_pud, INVENTORY_WINDOW_TYPE_TRADE);
                            _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_TRADE);
                            inventoryWindowRenderInnerInventories(win, nullptr, bartererTable, -1);
                        }
                    }

                    keyCode = -1;
                } else if (keyCode >= 2300 && keyCode <= 2300 + gInventorySlotsCount) {
                    if (gInventoryCursor == INVENTORY_WINDOW_CURSOR_ARROW) {
                        inventoryWindowOpenContextMenu(keyCode, INVENTORY_WINDOW_TYPE_TRADE);
                        inventoryWindowRenderInnerInventories(win, playerTable, nullptr, -1);
                    } else {
                        int slotIndex = keyCode - 2300;
                        if (slotIndex < _ptable_pud->length) {
                            InventoryItem* inventoryItem = &(_ptable_pud->items[_ptable_pud->length - (slotIndex + _ptable_offset + 1)]);
                            _barter_move_from_table_inventory(inventoryItem->item, inventoryItem->quantity, slotIndex, barterer, playerTable, true);
                            _display_target_inventory(_target_stack_offset[_target_curr_stack], -1, _target_pud, INVENTORY_WINDOW_TYPE_TRADE);
                            _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_TRADE);
                            inventoryWindowRenderInnerInventories(win, playerTable, nullptr, -1);
                        }
                    }

                    keyCode = -1;
                } else if (keyCode >= 2400 && keyCode <= 2400 + gInventorySlotsCount) {
                    if (gInventoryCursor == INVENTORY_WINDOW_CURSOR_ARROW) {
                        inventoryWindowOpenContextMenu(keyCode, INVENTORY_WINDOW_TYPE_TRADE);
                        inventoryWindowRenderInnerInventories(win, nullptr, bartererTable, -1);
                    } else {
                        int slotIndex = keyCode - 2400;
                        if (slotIndex < _btable_pud->length) {
                            InventoryItem* inventoryItem = &(_btable_pud->items[_btable_pud->length - (slotIndex + _btable_offset + 1)]);
                            _barter_move_from_table_inventory(inventoryItem->item, inventoryItem->quantity, slotIndex, barterer, bartererTable, false);
                            _display_target_inventory(_target_stack_offset[_target_curr_stack], -1, _target_pud, INVENTORY_WINDOW_TYPE_TRADE);
                            _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_TRADE);
                            inventoryWindowRenderInnerInventories(win, nullptr, bartererTable, -1);
                        }
                    }

                    keyCode = -1;
                }
            } else if ((mouseGetEvent() & MOUSE_EVENT_WHEEL) != 0) {
                if (mouseHitTestInWindow(gInventoryWindow, INVENTORY_TRADE_LEFT_SCROLLER_TRACKING_X, INVENTORY_TRADE_LEFT_SCROLLER_TRACKING_Y, INVENTORY_TRADE_LEFT_SCROLLER_TRACKING_MAX_X, INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + INVENTORY_TRADE_LEFT_SCROLLER_TRACKING_Y)) {
                    int wheelX;
                    int wheelY;
                    mouseGetWheel(&wheelX, &wheelY);
                    if (wheelY > 0) {
                        if (_stack_offset[_curr_stack] > 0) {
                            _stack_offset[_curr_stack] -= 1;
                            _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_TRADE);
                        }
                    } else if (wheelY < 0) {
                        if (_stack_offset[_curr_stack] + gInventorySlotsCount < _pud->length) {
                            _stack_offset[_curr_stack] += 1;
                            _display_inventory(_stack_offset[_curr_stack], -1, INVENTORY_WINDOW_TYPE_TRADE);
                        }
                    }
                } else if (mouseHitTestInWindow(gInventoryWindow, INVENTORY_TRADE_INNER_LEFT_SCROLLER_TRACKING_X, INVENTORY_TRADE_INNER_LEFT_SCROLLER_TRACKING_Y, INVENTORY_TRADE_INNER_LEFT_SCROLLER_TRACKING_MAX_X, INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + INVENTORY_TRADE_INNER_LEFT_SCROLLER_TRACKING_Y)) {
                    int wheelX;
                    int wheelY;
                    mouseGetWheel(&wheelX, &wheelY);
                    if (wheelY > 0) {
                        if (_ptable_offset > 0) {
                            _ptable_offset -= 1;
                            inventoryWindowRenderInnerInventories(win, playerTable, bartererTable, -1);
                        }
                    } else if (wheelY < 0) {
                        if (_ptable_offset + gInventorySlotsCount < _ptable_pud->length) {
                            _ptable_offset += 1;
                            inventoryWindowRenderInnerInventories(win, playerTable, bartererTable, -1);
                        }
                    }
                } else if (mouseHitTestInWindow(gInventoryWindow, INVENTORY_TRADE_RIGHT_SCROLLER_TRACKING_X, INVENTORY_TRADE_RIGHT_SCROLLER_TRACKING_Y, INVENTORY_TRADE_RIGHT_SCROLLER_TRACKING_MAX_X, INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + INVENTORY_TRADE_RIGHT_SCROLLER_TRACKING_Y)) {
                    int wheelX;
                    int wheelY;
                    mouseGetWheel(&wheelX, &wheelY);
                    if (wheelY > 0) {
                        if (_target_stack_offset[_target_curr_stack] > 0) {
                            _target_stack_offset[_target_curr_stack] -= 1;
                            _display_target_inventory(_target_stack_offset[_target_curr_stack], -1, _target_pud, INVENTORY_WINDOW_TYPE_TRADE);
                            windowRefresh(gInventoryWindow);
                        }
                    } else if (wheelY < 0) {
                        if (_target_stack_offset[_target_curr_stack] + gInventorySlotsCount < _target_pud->length) {
                            _target_stack_offset[_target_curr_stack] += 1;
                            _display_target_inventory(_target_stack_offset[_target_curr_stack], -1, _target_pud, INVENTORY_WINDOW_TYPE_TRADE);
                            windowRefresh(gInventoryWindow);
                        }
                    }
                } else if (mouseHitTestInWindow(gInventoryWindow, INVENTORY_TRADE_INNER_RIGHT_SCROLLER_TRACKING_X, INVENTORY_TRADE_INNER_RIGHT_SCROLLER_TRACKING_Y, INVENTORY_TRADE_INNER_RIGHT_SCROLLER_TRACKING_MAX_X, INVENTORY_SLOT_HEIGHT * gInventorySlotsCount + INVENTORY_TRADE_INNER_RIGHT_SCROLLER_TRACKING_Y)) {
                    int wheelX;
                    int wheelY;
                    mouseGetWheel(&wheelX, &wheelY);
                    if (wheelY > 0) {
                        if (_btable_offset > 0) {
                            _btable_offset -= 1;
                            inventoryWindowRenderInnerInventories(win, playerTable, bartererTable, -1);
                        }
                    } else if (wheelY < 0) {
                        if (_btable_offset + gInventorySlotsCount < _btable_pud->length) {
                            _btable_offset++;
                            inventoryWindowRenderInnerInventories(win, playerTable, bartererTable, -1);
                        }
                    }
                }
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    gamepad_ui_set_suppress_snap(false);

    itemMoveAll(hiddenBox, barterer);
    objectDestroy(hiddenBox, nullptr);

    if (armor != nullptr) {
        armor->flags |= OBJECT_WORN;
        itemAdd(barterer, armor, 1);
    }

    if (item2 != nullptr) {
        item2->flags |= OBJECT_IN_RIGHT_HAND;
        itemAdd(barterer, item2, 1);
    }

    if (item1 != nullptr) {
        itemAdd(barterer, item1, 1);
    }

    _exit_inventory(isoWasEnabled);

    // NOTE: Uninline.
    inventoryCommonFree();
}

// 0x47620C
static void _container_enter(int keyCode, int inventoryWindowType)
{
    if (keyCode >= 2000) {
        int index = _target_pud->length - (_target_stack_offset[_target_curr_stack] + keyCode - 2000 + 1);
        if (index < _target_pud->length && _target_curr_stack < 9) {
            InventoryItem* inventoryItem = &(_target_pud->items[index]);
            Object* item = inventoryItem->item;
            if (itemGetType(item) == ITEM_TYPE_CONTAINER) {
                _target_curr_stack += 1;
                _target_stack[_target_curr_stack] = item;
                _target_stack_offset[_target_curr_stack] = 0;

                _target_pud = &(item->data.inventory);

                _display_body(item->fid, inventoryWindowType);
                _display_target_inventory(_target_stack_offset[_target_curr_stack], -1, _target_pud, inventoryWindowType);
                windowRefresh(gInventoryWindow);
            }
        }
    } else {
        int index = _pud->length - (_stack_offset[_curr_stack] + keyCode - 1000 + 1);
        if (index < _pud->length && _curr_stack < 9) {
            InventoryItem* inventoryItem = &(_pud->items[index]);
            Object* item = inventoryItem->item;
            if (itemGetType(item) == ITEM_TYPE_CONTAINER) {
                _curr_stack += 1;

                _stack[_curr_stack] = item;
                _stack_offset[_curr_stack] = 0;

                _inven_dude = _stack[_curr_stack];
                _pud = &(item->data.inventory);

                _adjust_fid();
                _display_body(-1, inventoryWindowType);
                _display_inventory(_stack_offset[_curr_stack], -1, inventoryWindowType);
            }
        }
    }
}

// 0x476394
static void _container_exit(int keyCode, int inventoryWindowType)
{
    if (keyCode == 2500) {
        if (_curr_stack > 0) {
            _curr_stack -= 1;
            _inven_dude = _stack[_curr_stack];
            _pud = &_inven_dude->data.inventory;
            _adjust_fid();
            _display_body(-1, inventoryWindowType);
            _display_inventory(_stack_offset[_curr_stack], -1, inventoryWindowType);
        }
    } else if (keyCode == 2501) {
        if (_target_curr_stack > 0) {
            _target_curr_stack -= 1;
            Object* v5 = _target_stack[_target_curr_stack];
            _target_pud = &(v5->data.inventory);
            _display_body(v5->fid, inventoryWindowType);
            _display_target_inventory(_target_stack_offset[_target_curr_stack], -1, _target_pud, inventoryWindowType);
            windowRefresh(gInventoryWindow);
        }
    }
}

// Drop item inside a container item (bag, backpack, etc.).
// 0x476464
static int _drop_into_container(Object* container, Object* item, int sourceIndex, Object** itemSlot, int quantity)
{
    int quantityToMove;
    if (quantity > 1) {
        quantityToMove = inventoryQuantitySelect(INVENTORY_WINDOW_TYPE_MOVE_ITEMS, item, quantity);
    } else {
        quantityToMove = 1;
    }

    if (quantityToMove == -1) {
        return -1;
    }

    if (sourceIndex != -1) {
        if (itemRemove(_inven_dude, item, quantityToMove) == -1) {
            return -1;
        }
    }

    int rc = itemAttemptAdd(container, item, quantityToMove);
    if (rc != 0) {
        if (sourceIndex != -1) {
            // SFALL: Fix for items disappearing from inventory when you try to
            // drag them to bag/backpack in the inventory list and are
            // overloaded.
            itemAdd(_inven_dude, item, quantityToMove);
        }
    } else {
        if (itemSlot != nullptr) {
            if (itemSlot == &gInventoryArmor) {
                _adjust_ac(_stack[0], gInventoryArmor, nullptr);
            }
            *itemSlot = nullptr;
        }
    }

    return rc;
}

// 0x47650C
static int _drop_ammo_into_weapon(Object* weapon, Object* ammo, Object** ammoItemSlot, int quantity, int keyCode)
{
    if (itemGetType(weapon) != ITEM_TYPE_WEAPON) {
        return -1;
    }

    if (itemGetType(ammo) != ITEM_TYPE_AMMO) {
        return -1;
    }

    if (!weaponCanBeReloadedWith(weapon, ammo)) {
        return -1;
    }

    int quantityToMove;
    if (quantity > 1) {
        quantityToMove = inventoryQuantitySelect(INVENTORY_WINDOW_TYPE_MOVE_ITEMS, ammo, quantity);
    } else {
        quantityToMove = 1;
    }

    if (quantityToMove == -1) {
        return -1;
    }

    Object* sourceItem = ammo;
    bool isReloaded = false;
    int rc = itemRemove(_inven_dude, weapon, 1);
    for (int index = 0; index < quantityToMove; index++) {
        int rcReload = weaponReload(weapon, sourceItem);
        if (rcReload == 0) {
            if (ammoItemSlot != nullptr) {
                *ammoItemSlot = nullptr;
            }

            _obj_destroy(sourceItem);

            isReloaded = true;
            if (_inven_from_button(keyCode, &sourceItem, nullptr, nullptr) == 0) {
                break;
            }
        }
        if (rcReload != -1) {
            isReloaded = true;
        }
        if (rcReload != 0) {
            break;
        }
    }

    if (rc != -1) {
        itemAdd(_inven_dude, weapon, 1);
    }

    if (!isReloaded) {
        return -1;
    }

    const char* sfx = sfxBuildWeaponName(WEAPON_SOUND_EFFECT_READY, weapon, HIT_MODE_RIGHT_WEAPON_PRIMARY, nullptr);
    soundPlayFile(sfx);

    return 0;
}

// 0x47664C
static void _draw_amount(int value, int inventoryWindowType)
{
    // BIGNUM.frm
    FrmImage numbersFrmImage;
    int numbersFid = buildFid(OBJ_TYPE_INTERFACE, 170, 0, 0, 0);
    if (!numbersFrmImage.lock(numbersFid)) {
        return;
    }

    Rect rect;

    int windowWidth = windowGetWidth(_mt_wid);
    unsigned char* windowBuffer = windowGetBuffer(_mt_wid);

    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_MOVE_ITEMS) {
        rect.left = 125;
        rect.top = 45;
        rect.right = 195;
        rect.bottom = 69;

        int ranks[5];
        ranks[4] = value % 10;
        ranks[3] = value / 10 % 10;
        ranks[2] = value / 100 % 10;
        ranks[1] = value / 1000 % 10;
        ranks[0] = value / 10000 % 10;

        windowBuffer += rect.top * windowWidth + rect.left;

        for (int index = 0; index < 5; index++) {
            unsigned char* src = numbersFrmImage.getData() + 14 * ranks[index];
            blitBufferToBuffer(src, 14, 24, 336, windowBuffer, windowWidth);
            windowBuffer += 14;
        }
    } else {
        rect.left = 133;
        rect.top = 64;
        rect.right = 189;
        rect.bottom = 88;

        windowBuffer += windowWidth * rect.top + rect.left;
        blitBufferToBuffer(numbersFrmImage.getData() + 14 * (value / 60), 14, 24, 336, windowBuffer, windowWidth);
        blitBufferToBuffer(numbersFrmImage.getData() + 14 * (value % 60 / 10), 14, 24, 336, windowBuffer + 14 * 2, windowWidth);
        blitBufferToBuffer(numbersFrmImage.getData() + 14 * (value % 10), 14, 24, 336, windowBuffer + 14 * 3, windowWidth);
    }

    windowRefreshRect(_mt_wid, &rect);
}

// 0x47688C
static bool mt_pad_arm = true;

// ⚠️ A PRESSES WHAT THE CURSOR IS ON, once the cursor has been moved (fallout1-ce 2026-10-17). With
// the cursor where it was when the window opened, A is the quick DONE; once it has been moved onto a
// button, A presses that button -- PLACE, DONE, CANCEL, + or -.
//
// ⚠️ ...BY ITS OWN KEY, NOT BY A CLICK (Fallout 2 Realtime 2026-10-24). It used to leave the press to
// the mouse hook's A-as-left-click, and that click is what leaked: opened from the WORLD (the wheel, a
// quick slot) the window is no self-nav screen, so the hook's world A router clicked through it, the
// window closed on its own A edge with the button still down, and the release walked the dude to the
// cursor -- off the explosive he was putting down (f2_trace_2026-10-24_113000 s=315.91, make-path to
// the next tile; autotest screen-explosive-place, 4 steps). Now the window owns A
// (gamepad_ui_set_suppress_a_click for its whole life, a real trackpad click still passes) and
// answers it with the button's own code; its close bumps the input epoch so the still-held A is
// nobody's press.
static int mt_window_type = -1;
static int mt_open_mx = -1;
static int mt_open_my = -1;

// The key of the button under the cursor, -1 when it is on none.
static int mt_button_under_cursor()
{
    if (_mt_wid == -1) {
        return -1;
    }
    bool timer = mt_window_type == INVENTORY_WINDOW_TYPE_SET_TIMER;
    int px = timer ? 194 : 200;
    int py = timer ? 64 : 46;
    if (mouseHitTestInWindow(_mt_wid, px, py, px + 16 - 1, py + 12 - 1)) {
        return 6000; // +
    }
    if (mouseHitTestInWindow(_mt_wid, px, py + 12, px + 17 - 1, py + 24 - 1)) {
        return 7000; // -
    }
    if (mouseHitTestInWindow(_mt_wid, 98, 128, 98 + 15 - 1, 128 + 16 - 1)) {
        return KEY_RETURN; // DONE
    }
    if (mouseHitTestInWindow(_mt_wid, 148, 128, 148 + 15 - 1, 128 + 16 - 1)) {
        return KEY_ESCAPE; // CANCEL
    }
    if (timer) {
        return mouseHitTestInWindow(_mt_wid, 120, 86, 120 + MT_PLACE_W - 1, 86 + MT_PLACE_H - 1) ? MT_KEY_PLACE : -1;
    }
    return mouseHitTestInWindow(_mt_wid, 120, 80, 120 + 94 - 1, 80 + 33 - 1) ? 5000 : -1; // ALL
}

// The window's hold on A, for as long as it is up (see mt_button_under_cursor).
struct MtPadOwner {
    bool previous;
    MtPadOwner()
        : previous(gamepad_ui_set_suppress_a_click(true))
    {
    }
    ~MtPadOwner()
    {
        gamepad_ui_set_suppress_a_click(previous);
        gamepad_ui_bump_input_epoch();
    }
};

// Re-arm the picker's gamepad edge detection so a button still held from whatever opened
// the picker (e.g. LT+A in barter) doesn't instantly fire on the first frame.
static void move_timer_gamepad_reset()
{
    mt_pad_arm = true;
}

// CE: Gamepad control for the quantity picker (inventoryQuantitySelect). A = confirm (KEY_RETURN),
// B = cancel (KEY_ESCAPE), Y = set to max (5000), D-pad Up/Right = +1 (6000), D-pad
// Down/Left = -1 (7000). The +/- steps auto-repeat while held so you can ramp the count
// up quickly. Read SDL directly -- gamepad_ui_tick doesn't run in this modal loop.
static int move_timer_gamepad_keycode()
{
    static bool prevA = false;
    static bool prevB = false;
    static bool prevY = false;
    static bool prevX = false;
    static bool prevInc = false;
    static bool prevDec = false;
    static Uint32 lastStep = 0;

    SDL_GameController* pad = gamepad_ui_get_controller();
    if (pad == NULL) {
        prevA = prevB = prevY = prevX = prevInc = prevDec = false;
        return -1;
    }

    SDL_GameControllerUpdate();
    bool aNow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A) != 0;
    bool bNow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B) != 0;
    bool yNow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_Y) != 0;
    bool xNow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_X) != 0;
    bool incNow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP) != 0
        || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;
    bool decNow = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN) != 0
        || SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT) != 0;

    // First frame of a picker session: adopt the current button states as the baseline so
    // a held A/LT (used to OPEN the picker) isn't read as a confirm edge.
    if (mt_pad_arm) {
        prevA = aNow;
        prevB = bNow;
        prevY = yNow;
        prevX = xNow;
        prevInc = incNow;
        prevDec = decNow;
        mt_pad_arm = false;
        mouseGetPosition(&mt_open_mx, &mt_open_my);
        return -1;
    }

    int keyCode = -1;
    Uint32 now = SDL_GetTicks();
    if (aNow && !prevA) {
        int mx;
        int my;
        mouseGetPosition(&mx, &my);
        bool moved = mx != mt_open_mx || my != mt_open_my;
        // On a button A presses it (see mt_open_mx); anywhere else A is DONE.
        keyCode = moved ? mt_button_under_cursor() : -1;
        if (keyCode == -1) {
            keyCode = KEY_RETURN;
        }
    } else if (bNow && !prevB) {
        keyCode = KEY_ESCAPE;
    } else if (yNow && !prevY) {
        keyCode = 5000; // set to max
    } else if (xNow && !prevX) {
        keyCode = MT_KEY_PLACE; // set the timer and put it down (SET TIMER only)
    } else if (incNow && (!prevInc || now - lastStep >= 120)) {
        keyCode = 6000; // +1
        lastStep = now;
    } else if (decNow && (!prevDec || now - lastStep >= 120)) {
        keyCode = 7000; // -1
        lastStep = now;
    }

    prevA = aNow;
    prevB = bNow;
    prevY = yNow;
    prevX = xNow;
    prevInc = incNow;
    prevDec = decNow;
    return keyCode;
}

static int inventoryQuantitySelect(int inventoryWindowType, Object* item, int max)
{
    ScopedGameMode gm(GameMode::kCounter);
    MtPadOwner padOwner;

    mt_window_type = inventoryWindowType;
    inventoryQuantityWindowInit(inventoryWindowType, item);
    move_timer_gamepad_reset();
    mt_place_requested = false;

    int value;
    int min;
    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_MOVE_ITEMS) {
        value = 1;
        if (max > 99999) {
            max = 99999;
        }
        min = 1;
    } else {
        value = 60;
        min = 10;
    }

    _draw_amount(value, inventoryWindowType);

    bool isTyping = false;
    for (;;) {
        sharedFpsLimiter.mark();

        int keyCode = inputGetInput();

        // CE: the pad (A/B/Y/X + D-pad) when no real key fired.
        if (keyCode == -1) {
            keyCode = move_timer_gamepad_keycode();
        }

        if (keyCode == KEY_ESCAPE) {
            inventoryQuantityWindowFree(inventoryWindowType);
            return -1;
        }

        // P = PLACE for the keyboard.
        if (keyCode == KEY_LOWERCASE_P || keyCode == KEY_UPPERCASE_P) {
            keyCode = MT_KEY_PLACE;
        }
        bool place = keyCode == MT_KEY_PLACE && inventoryWindowType == INVENTORY_WINDOW_TYPE_SET_TIMER;
        if (keyCode == KEY_RETURN || place) {
            if (value >= min && value <= max) {
                if (inventoryWindowType != INVENTORY_WINDOW_TYPE_SET_TIMER || value % 10 == 0) {
                    soundPlayFile("ib1p1xx1");
                    mt_place_requested = place;
                    break;
                }
            }

            soundPlayFile("iisxxxx1");
        } else if (keyCode == 5000) {
            isTyping = false;
            value = max;
            _draw_amount(value, inventoryWindowType);
        } else if (keyCode == 6000) {
            isTyping = false;
            if (value < max) {
                if (inventoryWindowType == INVENTORY_WINDOW_TYPE_MOVE_ITEMS) {
                    if ((mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_REPEAT) != 0) {
                        getTicks();

                        unsigned int delay = 100;
                        while ((mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_REPEAT) != 0) {
                            sharedFpsLimiter.mark();

                            if (value < max) {
                                value++;
                            }

                            _draw_amount(value, inventoryWindowType);
                            inputGetInput();

                            if (delay > 1) {
                                delay--;
                                inputPauseForTocks(delay);
                            }

                            renderPresent();
                            sharedFpsLimiter.throttle();
                        }
                    } else {
                        if (value < max) {
                            value++;
                        }
                    }
                } else {
                    value += 10;
                }

                _draw_amount(value, inventoryWindowType);
                continue;
            }
        } else if (keyCode == 7000) {
            isTyping = false;
            if (value > min) {
                if (inventoryWindowType == INVENTORY_WINDOW_TYPE_MOVE_ITEMS) {
                    if ((mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_REPEAT) != 0) {
                        getTicks();

                        unsigned int delay = 100;
                        while ((mouseGetEvent() & MOUSE_EVENT_LEFT_BUTTON_REPEAT) != 0) {
                            sharedFpsLimiter.mark();

                            if (value > min) {
                                value--;
                            }

                            _draw_amount(value, inventoryWindowType);
                            inputGetInput();

                            if (delay > 1) {
                                delay--;
                                inputPauseForTocks(delay);
                            }

                            renderPresent();
                            sharedFpsLimiter.throttle();
                        }
                    } else {
                        if (value > min) {
                            value--;
                        }
                    }
                } else {
                    value -= 10;
                }

                _draw_amount(value, inventoryWindowType);
                continue;
            }
        }

        if (inventoryWindowType == INVENTORY_WINDOW_TYPE_MOVE_ITEMS) {
            if (keyCode >= KEY_0 && keyCode <= KEY_9) {
                int number = keyCode - KEY_0;
                if (!isTyping) {
                    value = 0;
                }

                value = 10 * value % 100000 + number;
                isTyping = true;

                _draw_amount(value, inventoryWindowType);
                continue;
            } else if (keyCode == KEY_BACKSPACE) {
                if (!isTyping) {
                    value = 0;
                }

                value /= 10;
                isTyping = true;

                _draw_amount(value, inventoryWindowType);
                continue;
            }
        }

        renderPresent();
        sharedFpsLimiter.throttle();
    }

    inventoryQuantityWindowFree(inventoryWindowType);

    return value;
}

// Creates move items/set timer interface.
//
// 0x476AB8
static int inventoryQuantityWindowInit(int inventoryWindowType, Object* item)
{
    const int oldFont = fontGetCurrent();
    fontSetCurrent(103);

    const InventoryWindowDescription* windowDescription = &(gInventoryWindowDescriptions[inventoryWindowType]);

    // Maintain original position in original resolution, otherwise center it.
    int quantityWindowX = screenGetWidth() != 640
        ? (screenGetWidth() - windowDescription->width) / 2
        : windowDescription->x;
    int quantityWindowY = screenGetHeight() != 480
        ? (screenGetHeight() - windowDescription->height) / 2
        : windowDescription->y;
    _mt_wid = windowCreate(quantityWindowX, quantityWindowY, windowDescription->width, windowDescription->height, 257, WINDOW_MODAL | WINDOW_MOVE_ON_TOP);
    unsigned char* windowBuffer = windowGetBuffer(_mt_wid);

    FrmImage backgroundFrmImage;
    int backgroundFid = buildFid(OBJ_TYPE_INTERFACE, windowDescription->frmId, 0, 0, 0);
    if (backgroundFrmImage.lock(backgroundFid)) {
        blitBufferToBuffer(backgroundFrmImage.getData(),
            windowDescription->width,
            windowDescription->height,
            windowDescription->width,
            windowBuffer,
            windowDescription->width);
    }

    MessageListItem messageListItem;
    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_MOVE_ITEMS) {
        // MOVE ITEMS
        messageListItem.num = 21;
        if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
            int length = fontGetStringWidth(messageListItem.text);
            fontDrawText(windowBuffer + windowDescription->width * 9 + (windowDescription->width - length) / 2, messageListItem.text, 200, windowDescription->width, _colorTable[21091]);
        }
    } else if (inventoryWindowType == INVENTORY_WINDOW_TYPE_SET_TIMER) {
        // SET TIMER
        messageListItem.num = 23;
        if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
            int length = fontGetStringWidth(messageListItem.text);
            fontDrawText(windowBuffer + windowDescription->width * 9 + (windowDescription->width - length) / 2, messageListItem.text, 200, windowDescription->width, _colorTable[21091]);
        }

        // Timer overlay
        FrmImage overlayFrmImage;
        int overlayFid = buildFid(OBJ_TYPE_INTERFACE, 306, 0, 0, 0);
        if (overlayFrmImage.lock(overlayFid)) {
            blitBufferToBuffer(overlayFrmImage.getData(),
                105,
                81,
                105,
                windowBuffer + 34 * windowDescription->width + 113,
                windowDescription->width);
        }

        // PLACE -- the ALL plate, 6 px lower than ALL sits in the move-items mode (the clock reaches
        // further down than the counter). Its label is drawn into a COPY of the art.
        FrmImage plateUpFrm;
        FrmImage plateDownFrm;
        if (plateUpFrm.lock(buildFid(OBJ_TYPE_INTERFACE, 307, 0, 0, 0)) && plateDownFrm.lock(buildFid(OBJ_TYPE_INTERFACE, 308, 0, 0, 0))) {
            memcpy(mt_place_up, plateUpFrm.getData(), sizeof(mt_place_up));
            memcpy(mt_place_down, plateDownFrm.getData(), sizeof(mt_place_down));

            const char* label = "PLACE";
            int length = fontGetStringWidth(label);
            fontDrawText(mt_place_up + (MT_PLACE_W - length) / 2 + 4 * MT_PLACE_W, label, 200, MT_PLACE_W, _colorTable[21091]);
            fontDrawText(mt_place_down + (MT_PLACE_W - length) / 2 + 4 * MT_PLACE_W, label, 200, MT_PLACE_W, _colorTable[18977]);

            int placeBtn = buttonCreate(_mt_wid, 120, 86, MT_PLACE_W, MT_PLACE_H, -1, -1, -1, MT_KEY_PLACE,
                mt_place_up, mt_place_down, nullptr, BUTTON_FLAG_TRANSPARENT);
            if (placeBtn != -1) {
                buttonSetCallbacks(placeBtn, _gsound_red_butt_press, _gsound_red_butt_release);
            }
        }
    }

    int inventoryFid = itemGetInventoryFid(item);
    artRender(inventoryFid, windowBuffer + windowDescription->width * 46 + 16, INVENTORY_LARGE_SLOT_WIDTH, INVENTORY_LARGE_SLOT_HEIGHT, windowDescription->width);

    int x;
    int y;
    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_MOVE_ITEMS) {
        x = 200;
        y = 46;
    } else {
        x = 194;
        y = 64;
    }

    int fid;
    int btn;

    // Plus button
    fid = buildFid(OBJ_TYPE_INTERFACE, 193, 0, 0, 0);
    _moveFrmImages[0].lock(fid);

    fid = buildFid(OBJ_TYPE_INTERFACE, 194, 0, 0, 0);
    _moveFrmImages[1].lock(fid);

    if (_moveFrmImages[0].isLocked() && _moveFrmImages[1].isLocked()) {
        btn = buttonCreate(_mt_wid,
            x,
            y,
            16,
            12,
            -1,
            -1,
            6000,
            -1,
            _moveFrmImages[0].getData(),
            _moveFrmImages[1].getData(),
            nullptr,
            BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
        }
    }

    // Minus button
    fid = buildFid(OBJ_TYPE_INTERFACE, 191, 0, 0, 0);
    _moveFrmImages[2].lock(fid);

    fid = buildFid(OBJ_TYPE_INTERFACE, 192, 0, 0, 0);
    _moveFrmImages[3].lock(fid);

    if (_moveFrmImages[2].isLocked() && _moveFrmImages[3].isLocked()) {
        btn = buttonCreate(_mt_wid,
            x,
            y + 12,
            17,
            12,
            -1,
            -1,
            7000,
            -1,
            _moveFrmImages[2].getData(),
            _moveFrmImages[3].getData(),
            nullptr,
            BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
        }
    }

    fid = buildFid(OBJ_TYPE_INTERFACE, 8, 0, 0, 0);
    _moveFrmImages[4].lock(fid);

    fid = buildFid(OBJ_TYPE_INTERFACE, 9, 0, 0, 0);
    _moveFrmImages[5].lock(fid);

    if (_moveFrmImages[4].isLocked() && _moveFrmImages[5].isLocked()) {
        // Done
        btn = buttonCreate(_mt_wid,
            98,
            128,
            15,
            16,
            -1,
            -1,
            -1,
            KEY_RETURN,
            _moveFrmImages[4].getData(),
            _moveFrmImages[5].getData(),
            nullptr,
            BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
        }

        // Cancel
        btn = buttonCreate(_mt_wid,
            148,
            128,
            15,
            16,
            -1,
            -1,
            -1,
            KEY_ESCAPE,
            _moveFrmImages[4].getData(),
            _moveFrmImages[5].getData(),
            nullptr,
            BUTTON_FLAG_TRANSPARENT);
        if (btn != -1) {
            buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
        }
    }

    if (inventoryWindowType == INVENTORY_WINDOW_TYPE_MOVE_ITEMS) {
        fid = buildFid(OBJ_TYPE_INTERFACE, 307, 0, 0, 0);
        _moveFrmImages[6].lock(fid);

        fid = buildFid(OBJ_TYPE_INTERFACE, 308, 0, 0, 0);
        _moveFrmImages[7].lock(fid);

        if (_moveFrmImages[6].isLocked() && _moveFrmImages[7].isLocked()) {
            // ALL
            messageListItem.num = 22;
            if (messageListGetItem(&gInventoryMessageList, &messageListItem)) {
                int length = fontGetStringWidth(messageListItem.text);

                // TODO: Where is y? Is it hardcoded in to 376?
                fontDrawText(_moveFrmImages[6].getData() + (94 - length) / 2 + 376, messageListItem.text, 200, 94, _colorTable[21091]);
                fontDrawText(_moveFrmImages[7].getData() + (94 - length) / 2 + 376, messageListItem.text, 200, 94, _colorTable[18977]);

                btn = buttonCreate(_mt_wid,
                    120,
                    80,
                    94,
                    33,
                    -1,
                    -1,
                    -1,
                    5000,
                    _moveFrmImages[6].getData(),
                    _moveFrmImages[7].getData(),
                    nullptr,
                    BUTTON_FLAG_TRANSPARENT);
                if (btn != -1) {
                    buttonSetCallbacks(btn, _gsound_red_butt_press, _gsound_red_butt_release);
                }
            }
        }
    }

    windowRefresh(_mt_wid);
    inventorySetCursor(INVENTORY_WINDOW_CURSOR_ARROW);
    fontSetCurrent(oldFont);

    return 0;
}

// 0x477030
static int inventoryQuantityWindowFree(int inventoryWindowType)
{
    int count = inventoryWindowType == INVENTORY_WINDOW_TYPE_MOVE_ITEMS ? 8 : 6;

    for (int index = 0; index < count; index++) {
        _moveFrmImages[index].unlock();
    }

    windowDestroy(_mt_wid);

    return 0;
}

// 0x477074
int _inven_set_timer(Object* item)
{
    bool isInitialized = _inven_is_initialized;

    if (!isInitialized) {
        if (inventoryCommonInit() == -1) {
            return -1;
        }
    }

    int seconds = inventoryQuantitySelect(INVENTORY_WINDOW_TYPE_SET_TIMER, item, 180);

    if (!isInitialized) {
        // NOTE: Uninline.
        inventoryCommonFree();
    }

    return seconds;
}

// Did the last timer window end on PLACE? Read once -- the answer is cleared.
bool inven_set_timer_place_requested()
{
    bool requested = mt_place_requested;
    mt_place_requested = false;
    return requested;
}

// PLACE from the inventory's own Use (both paths): the freshly armed explosive goes on the ground the
// way the Drop action puts it there -- back from a hand box into the pack first -- and the screen's
// close runs vanilla's "they saw that" (_dropped_explosive).
static void inven_place_if_requested(Object* item, Object* owner, Object** handSlot)
{
    if (!inven_set_timer_place_requested()) {
        return;
    }
    if (!explosiveIsActiveExplosive(item->pid)) {
        return;
    }
    if (handSlot != NULL) {
        itemAdd(owner, item, 1);
        *handSlot = NULL;
    }
    _obj_drop(owner, item);
    _dropped_explosive = true;
}

Object* inven_get_current_target_obj()
{
    return _target_stack[_target_curr_stack];
}

} // namespace fallout
