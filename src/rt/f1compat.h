#ifndef FALLOUT_RT_F1COMPAT_H_
#define FALLOUT_RT_F1COMPAT_H_

// THE TRANSLATION LAYER. Our realtime modules (src/rt/game/*) are fallout1-ce files, kept as close to
// byte-identical as possible so a fix made in one game can be copied to the other. They speak the
// ORIGINAL engine names fallout1-ce uses (obj_dude, stat_level, win_add ...); fallout2-ce renamed
// almost all of them (gDude, critterGetStat, windowCreate ...). This header gives every F1 name the
// modules use its F2 meaning:
//
//   - functions: f1compat_gen.h, one inline wrapper per name with the F1 signature, generated from the
//     reviewed pairing table (session tool gencompat.js); a wrong pairing fails to compile;
//   - globals, types, struct fields and functions whose shape changed: written out below by hand.
//
// ⚠️ The F2 headers are included with <angle brackets> on purpose. MSVC looks for a "quoted" header in
// the directory of EVERY file on the include stack, and src/rt/compat/game/ holds forwarding headers
// with the same names (stat.h, combat.h ...): a quoted include found the forwarder, not fallout2-ce's.
//
// It is reached through the forwarding headers in src/rt/compat/<F1 path>.h, which is how an
// unmodified `#include "game/anim.h"` in a module lands here.

#include <SDL.h>

#include <actions.h>
#include <animation.h>
#include <art.h>
#include <automap.h>
#include <character_editor.h>
#include <character_selector.h>
#include <color.h>
#include <combat.h>
#include <combat_ai.h>
#include <combat_defs.h>
#include <config.h>
#include <critter.h>
#include <cycle.h>
#include <db.h>
#include <debug.h>
#include <display_monitor.h>
#include <draw.h>
#include <elevator.h>
#include <game.h>
#include <game_config.h>
#include <game_dialog.h>
#include <game_mouse.h>
#include <game_movie.h>
#include <game_sound.h>
#include <geometry.h>
#include <input.h>
#include <interface.h>
#include <inventory.h>
#include <item.h>
#include <kb.h>
#include <loadsave.h>
#include <mainmenu.h>
#include <map.h>
#include <memory.h>
#include <message.h>
#include <mouse.h>
#include <obj_types.h>
#include <object.h>
#include <options.h>
#include <palette.h>
#include <party_member.h>
#include <perk.h>
#include <pipboy.h>
#include <preferences.h>
#include <proto.h>
#include <proto_instance.h>
#include <proto_types.h>
#include <random.h>
#include <scripts.h>
#include <skill.h>
#include <stat.h>
#include <svga.h>
#include <text_font.h>
#include <tile.h>
#include <trait.h>
#include <version.h>
#include <win32.h>
#include <window_manager.h>
#include <window_manager_private.h>
#include <worldmap.h>

#include "rt/rt_hooks.h"

// <windows.h> (pulled in by win32.h) defines near and far as nothing; our modules use them as names.
#undef near
#undef far

namespace fallout {

// --- Globals --------------------------------------------------------------------------------------
#define obj_dude gDude
#define game_config gGameConfig
#define map_elevation gElevation
// F1 map_data = F2 gMapHeader (name, index, ... the same fields under the same names).
#define map_data gMapHeader
// ⚠️ F1's display_win is the WORLD window (map.cc: 0,0, screen width, screen height minus the bar) --
// F2's gIsoWindow. It was mapped to the interface BAR for a day (2026-10-20, "mířím na něco, tak je
// terč úplně mimo"): the lock-on cursor was placed against the bar, a locked target counted as off
// screen 0.4 s after every lock, and the target highlight drew into the bar's buffer.
#define display_win gIsoWindow
#define colorTable _colorTable
#define cmap _cmap
// F1 keeps the hex step offsets as off_tile[2][6] (x row, y row); F2 splits them into _off_tile and
// dword_51D984. rt_off_tile (rt_f2_hooks.cc) is the F1-shaped copy -- the same twelve numbers.
extern const int rt_off_tile[2][6];
#define off_tile rt_off_tile
#define game_user_wants_to_quit _game_user_wants_to_quit
#define in_main_menu _in_main_menu
#define select_window_id gCharacterSelectorWindow
#define combat_state gCombatState
#define gcsd _gcsd
#define combat_exps _combat_exps
// F1's "the game window has the focus" (plib/gnw/winmain.h). Both go false on SDL's FOCUS_LOST, and neither does
// in a self-played run (input.cc ignores the event there).
#define GNW95_isActive gProgramIsActive
// F1's start-of-combat data is F2's CombatStartData.
typedef CombatStartData STRUCT_664980;

// --- Types and constants --------------------------------------------------------------------------
typedef File DB_FILE;
#define PC_FLAG_SNEAKING DUDE_STATE_SNEAKING
// Same bit (0x2000): "an event was ever queued for this object".
#define OBJECT_USED OBJECT_QUEUED

// F1's party.h name for it.
static inline bool isPartyMember(Object* object) { return objectIsPartyMember(object); }

// --- Struct fields: F1's Rect is {ulx, uly, lrx, lry}, F2's is {left, top, right, bottom} ---------
#define ulx left
#define uly top
#define lrx right
#define lry bottom

// --- Text: function-pointer globals in both trees, under other names ------------------------------
#define text_to_buf fontDrawText
#define text_width fontGetStringWidth
#define text_height fontGetLineHeight
#define debug_printf debugPrint

// --- Functions whose shape changed (not a plain forward) ------------------------------------------

// F1 perks belong to the player only; F2 asks whose.
static inline int perk_level(int perk) { return perkGetRank(gDude, perk); }

// F2 lets the caller pick each hand's action; F1 always meant "the default".
static inline int intface_update_items(bool animated)
{
    return interfaceUpdateItems(animated, INTERFACE_ITEM_ACTION_DEFAULT, INTERFACE_ITEM_ACTION_DEFAULT);
}

// F2 falls back to the critter's own damage type when the weapon is NULL; F1 took only the weapon.
static inline int item_w_damage_type(Object* weapon) { return weaponGetDamageType(nullptr, weapon); }

// F2 adds an optional defender (to prefer weapons good against it); F1 had none.
static inline Object* ai_search_inven(Object* critter, int check_action_points)
{
    return _ai_search_inven_weap(critter, check_action_points != 0, nullptr);
}

// F2 takes an extra out-parameter it never writes; F1 had none.
static inline int determine_to_hit_no_range(Object* a1, Object* a2, int hitLocation, int hitMode)
{
    return _determine_to_hit_no_range(a1, a2, hitLocation, hitMode, nullptr);
}


// F1 background processes carry a label for the frame-time meter; F2's tickers do not.
static inline void add_bk_process(TickerProc* proc, const char* label)
{
    (void)label;
    tickersAdd(proc);
}

#include "rt/f1compat_gen.h"

// F1 asks "is one of these carried" as an int; F2 only has the object lookup.
static inline int inven_pid_is_carried(Object* obj, int pid) { return objectGetCarriedObjectByPid(obj, pid) != nullptr ? 1 : 0; }

} // namespace fallout

#endif /* FALLOUT_RT_F1COMPAT_H_ */
