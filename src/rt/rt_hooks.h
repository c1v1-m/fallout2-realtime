#ifndef FALLOUT_RT_HOOKS_H_
#define FALLOUT_RT_HOOKS_H_

// The engine hooks our realtime modules call that fallout1-ce ADDED to vanilla files (anim.cc,
// inventry.cc, intface.cc ...). Same names and signatures as there. Each one is either implemented in
// the matching fallout2-ce file (search the name) or, while the feature behind it is not ported yet,
// stubbed with the vanilla behaviour in src/rt/rt_stubs.cc -- that file lists which.
//
// Hooks that F2 already has a natural header for (map_is_loading in map.h, mouse_set_input_hook in
// mouse.h, win_get_top_modal in window_manager.h, svga_set_vsync in svga.h) are declared there.

namespace fallout {

typedef struct Object Object;

// anim.cc (F1)
float anim_get_travel_speed();
bool anim_get_reload_anim();
bool anim_get_smooth_motion();
void anim_set_travel_speed(float scale);
void anim_set_smooth_motion(bool on);
void anim_set_reload_anim(bool on);
void anim_set_injection_timing(Object* critter);
void anim_quick_draw_begin(Object* obj);
void anim_quick_draw_end();
int anim_clear_pending(Object* a1);
int register_object_reload(Object* critter);
void anim_set_reload_timing(Object* critter);
int anim_busy_blocking(Object* a1);

// critter.cc (F1)
bool critter_get_radaway_cancels();
void critter_set_radaway_cancels(bool enabled);

// intface.cc (F1)
bool intface_get_combat_box_sound();
void intface_set_combat_box_sound(bool on);
bool intface_can_reload_weapon();
int intface_reload_weapon();
bool intface_item_can_toggle_state();
Object* intface_reload_target();
bool intface_item_set_aiming();
void intface_update_heal_slot(Object* item, int count, bool armed = false);
void intface_update_quick_slot_lb(Object* item, int count, bool armed);
void intface_update_aim_part(int hitLocation);

// gdialog.cc (F1)
bool gdialog_get_script_float();
void gdialog_set_script_float(bool on);

// inventry.cc (F1)
bool inven_any_open();
int inven_current_window_type();
bool inven_normal_cursor_over_list();
bool inven_normal_cursor_over_armor();
bool inven_loot_cursor_over_panel();
bool inven_is_open();
bool inven_equip_under_cursor(int hand);
Object* inven_list_item_under_cursor();
int inven_wield_instant(Object* critter, Object* item, int a3);
void inven_explosive_dropped_notice();
bool inven_set_timer_place_requested();

// gmouse.cc (F1)
bool gmouse_move_cursor_is_hidden();
void gmouse_set_use_on_item(int pid);
void gmouse_set_cursor_suppressed(bool suppressed);
void gmouse_set_move_cursor_hidden(bool hidden);
int gmouse_3d_modes_are_enabled();

// options.cc (F1)
bool options_is_open();

// combat.cc (F1)
char* combat_get_loc_name(Object* critter, int hitLocation);
int combat_called_shot_pick(Object* critter, int hitMode, int initialLocation);

// scripts.cc (F1)
void scr_load_before_first_event(Object* obj);

// Fallout 2 only (rt_f2_hooks.cc): one press of a pad button (an SDL_GameControllerButton) inside a
// vanilla screen's own loop, armed on release. `armed` must be a local of that loop, false at entry.
bool rt_pad_pressed(int button, bool* armed);

} // namespace fallout

#endif /* FALLOUT_RT_HOOKS_H_ */
