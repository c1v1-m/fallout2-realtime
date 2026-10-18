// The Fallout 2 side of the engine hooks our realtime modules call (declared in rt_hooks.h), plus
// STUBS for every feature that is not ported yet.
//
// ⚠️ A STUB HERE IS A FEATURE THAT DOES NOT EXIST YET IN FALLOUT 2, not a finished decision. Each one
// returns what the ORIGINAL game would do (no reload animation, no quick slots, no realtime combat...),
// so the modules that call it behave as if the switch were off. When a feature is ported, delete its
// stub here and implement it in the matching fallout2-ce file -- the linker then refuses a duplicate,
// so a stub cannot silently shadow the real thing.
//
// Phase 1 of the port (2026-10-18): trace, MODS, 480-line resolution, VSync, the gamepad layer.

#include "rt/f1compat.h"

#include <stdio.h>

#include "rt/game/autotest.h"
#include "rt/game/combat_rt.h"
#include "rt/game/game_trace.h"
#include "rt/game/gamepad_ui.h"

namespace fallout {

// --- Implemented for real ---------------------------------------------------------------------------
// Fallout 2 CE already tracks which screen is up (GameMode, a port of sfall's), so the "is X open"
// questions F1 answered with flags of its own are read from there.

bool options_is_open()
{
    return GameMode::isInGameMode(GameMode::kOptions | GameMode::kPreferences);
}

bool inven_is_open()
{
    return GameMode::isInGameMode(GameMode::kInventory);
}

bool inven_any_open()
{
    return GameMode::isInGameMode(GameMode::kInventory | GameMode::kUseOn | GameMode::kLoot | GameMode::kBarter | GameMode::kCounter);
}

// The window on TOP: a quantity box opened from the barter screen is the quantity box.
int inven_current_window_type()
{
    if (GameMode::isInGameMode(GameMode::kCounter)) {
        return INVENTORY_WINDOW_TYPE_MOVE_ITEMS;
    }
    if (GameMode::isInGameMode(GameMode::kBarter)) {
        return INVENTORY_WINDOW_TYPE_TRADE;
    }
    if (GameMode::isInGameMode(GameMode::kLoot)) {
        return INVENTORY_WINDOW_TYPE_LOOT;
    }
    if (GameMode::isInGameMode(GameMode::kUseOn)) {
        return INVENTORY_WINDOW_TYPE_USE_ITEM_ON;
    }
    if (GameMode::isInGameMode(GameMode::kInventory)) {
        return INVENTORY_WINDOW_TYPE_NORMAL;
    }
    return -1;
}

// A script's FIRST event runs only its start body. The mouse used to pre-load it on hover; the pad
// has no hover, so the interact prompt calls this before it acts (F1: the roped shaft said "You see").
void scr_load_before_first_event(Object* obj)
{
    if (obj == nullptr || obj->sid == -1) {
        return;
    }

    Script* script;
    if (scriptGetScript(obj->sid, &script) == -1) {
        return;
    }

    // Loaded already: running START again would re-run the body, which is not what this is for.
    if ((script->flags & SCRIPT_FLAG_0x01) != 0) {
        return;
    }

    scriptExecProc(obj->sid, SCRIPT_PROC_START);
}

// One press of a pad button, for the vanilla screens' own loops (gamepad_ui_tick does not run inside
// them). ⚠️ The button must be seen RELEASED before it counts: the press that opened the screen is
// often still down. `armed` is the caller's -- a LOCAL of its loop, so every visit starts disarmed.
bool rt_pad_pressed(int button, bool* armed)
{
    SDL_GameController* pad = gamepad_ui_get_controller();
    if (pad == nullptr) {
        return false;
    }
    bool down = SDL_GameControllerGetButton(pad, (SDL_GameControllerButton)button) != 0;
    if (!down) {
        *armed = true;
        return false;
    }
    if (*armed) {
        *armed = false;
        return true;
    }
    return false;
}

// --- Stubs: not ported yet, the original game's behaviour -------------------------------------------

// anim.cc: travel speed, smooth motion, reload animation, drug injection timing, quick draw, and the
// pending-ownership split -- all phase 2 (they live in F1's anim.cc rewrite).
float anim_get_travel_speed() { return 1.0f; }
bool anim_get_reload_anim() { return false; }
bool anim_get_smooth_motion() { return false; }
void anim_set_travel_speed(float scale) { (void)scale; }
void anim_set_smooth_motion(bool on) { (void)on; }
void anim_set_reload_anim(bool on) { (void)on; }
void anim_set_injection_timing(Object* critter) { (void)critter; }
void anim_quick_draw_begin(Object* obj) { (void)obj; }
void anim_quick_draw_end() { }
// Vanilla has no pending-but-not-started split: clearing pending = nothing to clear.
int anim_clear_pending(Object* a1)
{
    (void)a1;
    return 0;
}
int register_object_reload(Object* critter)
{
    (void)critter;
    return -1;
}
void anim_set_reload_timing(Object* critter) { (void)critter; }
// Vanilla's own notion of busy.
int anim_busy_blocking(Object* a1) { return animationIsBusy(a1); }

// critter.cc: RadAway clearing a queued dose -- a design change, phase 2+.
bool critter_get_radaway_cancels() { return false; }
void critter_set_radaway_cancels(bool enabled) { (void)enabled; }

// intface.cc: the heal / quick slots beside the bar, the aimed-part readout, the pad reload -- phase 3.
bool intface_get_combat_box_sound() { return true; }
void intface_set_combat_box_sound(bool on) { (void)on; }
bool intface_can_reload_weapon() { return false; }
int intface_reload_weapon() { return -1; }
bool intface_item_can_toggle_state() { return true; }
Object* intface_reload_target() { return nullptr; }
bool intface_item_set_aiming() { return false; }
void intface_update_heal_slot(Object* item, int count, bool armed)
{
    (void)item;
    (void)count;
    (void)armed;
}
void intface_update_quick_slot_lb(Object* item, int count, bool armed)
{
    (void)item;
    (void)count;
    (void)armed;
}
void intface_update_aim_part(int hitLocation) { (void)hitLocation; }

// gdialog.cc: script lines as floating text -- phase 3.
bool gdialog_get_script_float() { return false; }
void gdialog_set_script_float(bool on) { (void)on; }

// inventry.cc: the pad's inventory navigation -- phase 3. Until then the inventory is a mouse screen.
bool inven_normal_cursor_over_list() { return false; }
bool inven_normal_cursor_over_armor() { return false; }
bool inven_loot_cursor_over_panel() { return false; }
bool inven_equip_under_cursor(int hand)
{
    (void)hand;
    return false;
}
Object* inven_list_item_under_cursor() { return nullptr; }
int inven_wield_instant(Object* critter, Object* item, int a3) { return _inven_wield(critter, item, a3); }
void inven_explosive_dropped_notice() { }
bool inven_set_timer_place_requested() { return false; }

// gmouse.cc: hiding the hex cursor while the stick walks, use-on for an item not in the hand -- phase 2/3.
bool gmouse_move_cursor_is_hidden() { return false; }
void gmouse_set_use_on_item(int pid) { (void)pid; }
void gmouse_set_cursor_suppressed(bool suppressed) { (void)suppressed; }
void gmouse_set_move_cursor_hidden(bool hidden) { (void)hidden; }

// combat.cc: the called-shot window the pad opens -- phase 2.
char* combat_get_loc_name(Object* critter, int hitLocation)
{
    (void)critter;
    (void)hitLocation;
    static char empty[] = "";
    return empty;
}
int combat_called_shot_pick(Object* critter, int hitMode, int initialLocation)
{
    (void)critter;
    (void)hitMode;
    return initialLocation;
}

// --- combat_rt: THE REALTIME COMBAT, phase 2 -------------------------------------------------------
// Until it is ported the fights are the original turn-based ones, so every question the pad asks the
// realtime layer gets "not running" / "nothing". The MODS rows that set these levers are hidden.

float combat_rt_get_door_magnet() { return 0.0f; }
bool combat_rt_get_npc_slide() { return false; }
float combat_rt_get_knockdown_agility() { return 0.0f; }
float combat_rt_get_cam_smooth() { return 0.0f; }
bool combat_rt_get_free_move() { return false; }
bool combat_rt_get_look_around() { return false; }
bool combat_rt_get_script_turns() { return true; }
bool combat_rt_get_cam_lean() { return false; }
float combat_rt_get_flinch_chance() { return 0.0f; }
bool combat_rt_get_mouse_hold_walk() { return false; }
bool combat_rt_get_forgive_stray() { return false; }
bool combat_rt_get_crowd_make_room() { return false; }
bool combat_rt_get_tactical_ai() { return false; }
void combat_rt_set_door_magnet(float strength) { (void)strength; }
void combat_rt_set_npc_slide(bool enabled) { (void)enabled; }
void combat_rt_set_knockdown_agility(float factor) { (void)factor; }
void combat_rt_set_cam_smooth(float k) { (void)k; }
void combat_rt_set_free_move(bool enabled) { (void)enabled; }
void combat_rt_set_look_around(bool enabled) { (void)enabled; }
void combat_rt_set_script_turns(bool enabled) { (void)enabled; }
void combat_rt_set_cam_lean(bool on) { (void)on; }
void combat_rt_set_mouse_hold_walk(bool enabled) { (void)enabled; }
void combat_rt_set_forgive_stray(bool enabled) { (void)enabled; }
void combat_rt_set_crowd_make_room(bool enabled) { (void)enabled; }
void combat_rt_set_tactical_ai(bool enabled) { (void)enabled; }
void combat_rt_set_flinch_chance(float factor) { (void)factor; }

bool combat_rt_has_lock() { return false; }
bool combat_rt_try_world_loot() { return false; }
bool combat_rt_player_attack_busy() { return false; }
// Turn-based: the dude's action points are the vanilla counter (shown on the bar in combat).
int combat_rt_player_ap() { return gDude != nullptr ? gDude->data.critter.combat.ap : 0; }
bool combat_rt_is_reloading(Object* critter)
{
    (void)critter;
    return false;
}
// Out of a fight nothing costs AP; in a turn-based fight the engine charges its own costs.
bool combat_rt_spend_ap(int cost)
{
    (void)cost;
    return true;
}
bool combat_rt_end_combat() { return false; }
bool combat_rt_cancel_player_action() { return false; }
void combat_rt_inject_end(Object* critter) { (void)critter; }
void combat_rt_refund_ap(int amount) { (void)amount; }
bool combat_rt_active() { return false; }
void combat_rt_inject_begin(Object* critter) { (void)critter; }
bool combat_rt_is_injecting(Object* critter)
{
    (void)critter;
    return false;
}
bool combat_rt_enabled() { return false; }
bool combat_rt_player_attack_pending() { return false; }
Object* combat_rt_quick_throw_target() { return nullptr; }
bool combat_rt_request_throw(Object* target)
{
    (void)target;
    return false;
}
void combat_rt_reload_end(Object* critter) { (void)critter; }
bool combat_rt_stick_walking() { return false; }
void combat_rt_reload_begin(Object* critter) { (void)critter; }
int combat_rt_aim_chance(int hitLocation)
{
    (void)hitLocation;
    return 0;
}
Object* combat_rt_aim_target() { return nullptr; }
bool combat_rt_can_aim() { return false; }
bool combat_rt_aimed_attack(int hitLocation)
{
    (void)hitLocation;
    return false;
}
bool combat_rt_weapon_can_aim() { return false; }
const char* combat_rt_aim_refusal() { return "Realtime combat is not in this build yet."; }
int combat_rt_player_hit_mode() { return HIT_MODE_PUNCH; }
Object* combat_rt_mouse_target() { return nullptr; }
void combat_rt_request_lock_step(int dir) { (void)dir; }
bool combat_rt_hostile_in_attack_range() { return false; }
bool combat_rt_is_engaging(Object* obj)
{
    (void)obj;
    return false;
}
bool combat_rt_fight_in_view() { return isInCombat(); }

// --- autotest: the self-played fights arrive in phase 2. Until then: THE PAD TEST ------------------
//
// F1's autotest plays a fight through a VIRTUAL pad; gamepad_ui opens whatever controller index this
// function names instead of a real one. The fights need combat_rt, so for now the same door is used
// for something smaller: with FALLOUT_RT_PADTEST=<script> in the environment, a virtual pad is
// created and a background thread presses it on a timeline, so the pad layer can be checked without
// anyone holding a controller. Script lines (seconds since start, then an action):
//
//   7.0  btn A 1          press / release a button (A B X Y BACK GUIDE START L3 R3 LB RB UP DOWN LEFT RIGHT)
//   7.5  tap A            down for 120 ms
//   8.0  axis RX 20000    set an axis (LX LY RX RY LT RT), -32768..32767
//   # comment
//
// Unset, it is exactly the stub it replaces: no virtual pad, real controllers only.

struct PadTestStep {
    double at;
    int kind; // 0 = button, 1 = axis, 2 = tap
    int index;
    int value;
};

static int padtest_index = -2; // -2 = not decided yet, -1 = off
static SDL_Joystick* padtest_joy = nullptr;
static PadTestStep padtest_steps[512];
static int padtest_count = 0;

static int padtest_button(const char* name)
{
    static const struct {
        const char* name;
        int button;
    } names[] = {
        { "A", SDL_CONTROLLER_BUTTON_A }, { "B", SDL_CONTROLLER_BUTTON_B }, { "X", SDL_CONTROLLER_BUTTON_X },
        { "Y", SDL_CONTROLLER_BUTTON_Y }, { "BACK", SDL_CONTROLLER_BUTTON_BACK }, { "GUIDE", SDL_CONTROLLER_BUTTON_GUIDE },
        { "START", SDL_CONTROLLER_BUTTON_START }, { "L3", SDL_CONTROLLER_BUTTON_LEFTSTICK },
        { "R3", SDL_CONTROLLER_BUTTON_RIGHTSTICK }, { "LB", SDL_CONTROLLER_BUTTON_LEFTSHOULDER },
        { "RB", SDL_CONTROLLER_BUTTON_RIGHTSHOULDER }, { "UP", SDL_CONTROLLER_BUTTON_DPAD_UP },
        { "DOWN", SDL_CONTROLLER_BUTTON_DPAD_DOWN }, { "LEFT", SDL_CONTROLLER_BUTTON_DPAD_LEFT },
        { "RIGHT", SDL_CONTROLLER_BUTTON_DPAD_RIGHT },
    };
    for (const auto& n : names) {
        if (SDL_strcasecmp(n.name, name) == 0) {
            return n.button;
        }
    }
    return -1;
}

static int padtest_axis(const char* name)
{
    static const struct {
        const char* name;
        int axis;
    } names[] = {
        { "LX", SDL_CONTROLLER_AXIS_LEFTX }, { "LY", SDL_CONTROLLER_AXIS_LEFTY }, { "RX", SDL_CONTROLLER_AXIS_RIGHTX },
        { "RY", SDL_CONTROLLER_AXIS_RIGHTY }, { "LT", SDL_CONTROLLER_AXIS_TRIGGERLEFT }, { "RT", SDL_CONTROLLER_AXIS_TRIGGERRIGHT },
    };
    for (const auto& n : names) {
        if (SDL_strcasecmp(n.name, name) == 0) {
            return n.axis;
        }
    }
    return -1;
}

static int padtest_thread(void* data)
{
    (void)data;
    Uint64 start = SDL_GetPerformanceCounter();
    double freq = (double)SDL_GetPerformanceFrequency();
    for (int i = 0; i < padtest_count; i++) {
        const PadTestStep& s = padtest_steps[i];
        for (;;) {
            double now = (double)(SDL_GetPerformanceCounter() - start) / freq;
            if (now >= s.at) {
                break;
            }
            SDL_Delay(5);
        }
        if (s.kind == 1) {
            SDL_JoystickSetVirtualAxis(padtest_joy, s.index, (Sint16)s.value);
        } else if (s.kind == 0) {
            SDL_JoystickSetVirtualButton(padtest_joy, s.index, (Uint8)s.value);
        } else {
            SDL_JoystickSetVirtualButton(padtest_joy, s.index, 1);
            SDL_Delay(120);
            SDL_JoystickSetVirtualButton(padtest_joy, s.index, 0);
        }
    }
    return 0;
}

int autotest_controller_index()
{
    if (padtest_index != -2) {
        return padtest_index;
    }
    padtest_index = -1;

    const char* path = SDL_getenv("FALLOUT_RT_PADTEST");
    if (path == nullptr || *path == '\0') {
        return -1;
    }

    FILE* stream = fopen(path, "rt");
    if (stream == nullptr) {
        trace_event(TRACE_ERR, "padtest", "cannot open %s", path);
        return -1;
    }
    char line[256];
    while (padtest_count < 512 && fgets(line, sizeof(line), stream) != nullptr) {
        double at;
        char verb[16];
        char name[16];
        int value = 0;
        int fields = sscanf(line, "%lf %15s %15s %d", &at, verb, name, &value);
        if (fields < 3) {
            continue; // blank line or # comment
        }
        PadTestStep step = { at, 0, -1, value };
        if (SDL_strcasecmp(verb, "axis") == 0) {
            step.kind = 1;
            step.index = padtest_axis(name);
        } else {
            step.kind = SDL_strcasecmp(verb, "tap") == 0 ? 2 : 0;
            step.index = padtest_button(name);
        }
        if (step.index < 0) {
            trace_event(TRACE_WARN, "padtest", "unknown control in: %s", line);
            continue;
        }
        padtest_steps[padtest_count++] = step;
    }
    fclose(stream);

    SDL_InitSubSystem(SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);
    SDL_VirtualJoystickDesc desc;
    SDL_zero(desc);
    desc.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
    desc.type = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
    desc.naxes = SDL_CONTROLLER_AXIS_MAX;
    desc.nbuttons = SDL_CONTROLLER_BUTTON_DPAD_RIGHT + 1;
    desc.name = "Pad test";
    int index = SDL_JoystickAttachVirtualEx(&desc);
    padtest_joy = index >= 0 ? SDL_JoystickOpen(index) : nullptr;
    if (padtest_joy == nullptr) {
        trace_event(TRACE_ERR, "padtest", "virtual pad failed: %s", SDL_GetError());
        return -1;
    }

    padtest_index = index;
    trace_event(TRACE_FLOW, "padtest", "script=%s steps=%d pad=%d", path, padtest_count, index);
    SDL_DetachThread(SDL_CreateThread(padtest_thread, "padtest", nullptr));
    return padtest_index;
}

} // namespace fallout
