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

// F1's hex step offsets, off_tile[2][6]: x row then y row (F2: _off_tile + dword_51D984).
const int rt_off_tile[2][6] = {
    { 16, 32, 16, -16, -32, -16 },
    { -12, 0, 12, 12, 0, -12 },
};

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
    return GameMode::isInGameMode(GameMode::kInventory | GameMode::kUseOn | GameMode::kLoot | GameMode::kBarter);
}

// The inventory window on top. As in fallout1-ce, the quantity box is NOT one of them: it reads the pad
// itself (move_timer_gamepad_keycode), and the hook keeps treating the screen under it as what it is.
int inven_current_window_type()
{
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
// (anim.cc's hooks are real now: animation.cc IS fallout1-ce's anim.cc, translated.)


// inventry.cc: inven_wield without the ceremony -- no put-away, no take-out, the stance changes at once
// (the quick slots' grenade draw). Fallout 2 already has that switch in _invenWieldFunc.
int inven_wield_instant(Object* critter, Object* item, int a3) { return _invenWieldFunc(critter, item, a3, false); }

// --- THE PAD TEST ---------------------------------------------------------------------------------
//
// F1's autotest (rt/game/autotest.cc) plays whole FIGHTS through a virtual pad. This is the smaller
// tool for the SCREENS: with FALLOUT_RT_PADTEST=<script> in the environment, a virtual pad is
// created before gamepad_ui opens a controller and a background thread presses it on a timeline,
// so the pad layer can be checked without anyone holding a controller. With a real pad plugged in
// gamepad_ui takes the real one (it opens the first controller it finds). Script lines (seconds
// since start, then an action):
//
//   7.0  btn A 1          press / release a button (A B X Y BACK GUIDE START L3 R3 LB RB UP DOWN LEFT RIGHT)
//   7.5  tap A            down for 120 ms
//   8.0  axis RX 20000    set an axis (LX LY RX RY LT RT), -32768..32767
//   # comment
//
// Unset: nothing happens.

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

int rt_padtest_controller_index()
{
    return padtest_index >= 0 ? padtest_index : -1;
}

void rt_padtest_init()
{
    if (padtest_index != -2) {
        return;
    }
    padtest_index = -1;

    const char* path = SDL_getenv("FALLOUT_RT_PADTEST");
    if (path == nullptr || *path == '\0') {
        return;
    }

    FILE* stream = fopen(path, "rt");
    if (stream == nullptr) {
        trace_event(TRACE_ERR, "padtest", "cannot open %s", path);
        return;
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

    // gamepad_ui_init sets this too, but only AFTER us -- and it must precede the subsystem init.
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
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
        return;
    }

    padtest_index = index;
    trace_event(TRACE_FLOW, "padtest", "script=%s steps=%d pad=%d", path, padtest_count, index);
    SDL_DetachThread(SDL_CreateThread(padtest_thread, "padtest", nullptr));
}

} // namespace fallout
