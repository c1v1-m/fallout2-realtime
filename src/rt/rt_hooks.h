#ifndef FALLOUT_RT_HOOKS_H_
#define FALLOUT_RT_HOOKS_H_

// The engine hooks our realtime modules call that fallout1-ce ADDED to vanilla files (anim.cc,
// combat.cc, actions.cc, intface.cc ...). Same names and signatures as there, grouped by the F1 file
// that owns them. Each one is implemented in the matching fallout2-ce file (search the name) or,
// where the feature behind it is not ported yet, stubbed with the original game's behaviour in
// src/rt/rt_f2_hooks.cc -- that file says which.
//
// Hooks F2 already has a natural header for (map_is_loading in map.h, mouse_set_input_hook in
// mouse.h, win_get_top_modal in window_manager.h, svga_set_vsync in svga.h, FpsLimiter) live there.
//
// ⚠️ <angle brackets> on purpose: src/rt/compat/game/ holds forwarders named like F2's headers.

#include <stddef.h>

#include <combat_defs.h>
#include <db.h>
#include <geometry.h>
#include <obj_types.h>

namespace fallout {

// --- anim.cc ----------------------------------------------------------------------------------------
// ANIMATION_REQUEST_DEFER_STAND is added to F2's AnimationRequestOptions in animation.h.
void anim_debug_table(int* occupied, int* nonReserved, int* curr);
bool anim_discard_open_transaction();
void anim_debug_gate(int* openTransaction, int* inAnimStop);
void anim_debug_attack_scope(bool on);
int anim_force_clear(Object* a1);
void anim_notify_object_freed(Object* obj);
int anim_clear_pending(Object* a1);
int anim_cut_pending(Object* a1);
int anim_end_lone_stand(Object* obj);
void anim_quick_draw_begin(Object* obj);
void anim_quick_draw_end();
int anim_end_moves(Object* obj);
int anim_busy_blocking(Object* a1);
bool anim_owned_elsewhere(Object* obj);
void anim_debug_blocking_dump(Object* a1, char* buf, size_t size);
int make_path_capped(Object* object, int from, int to, unsigned char* rotations, int a5, int maxClosed);
float anim_get_travel_speed();
float anim_travel_speed_in_effect();
void anim_set_travel_speed(float scale);
void anim_set_projectile_speedup(float speedup);
bool anim_get_reload_anim();
void anim_set_reload_anim(bool on);
void anim_set_reload_timing(Object* critter);
void anim_set_injection_timing(Object* critter);
void anim_set_start_frame(Object* critter, int frame);
int register_object_reload(Object* critter);
bool anim_is_reload_pose(Object* critter);
void anim_reset_last_path();
int anim_last_path_steps(Object* obj);
int anim_forever_snapshot(Object** owners, int* anims, int max);
void anim_forever_restore(Object** owners, int* anims, int count);
void anim_set_speed_scale(float scale);
void anim_set_move_speed_scale(float scale);
void anim_set_weapon_swap_stat_factor(float factor);
int anim_extend_move(Object* obj, int rotation, int count);
int anim_move_pending_steps(Object* obj);
int anim_move_repoint(Object* obj, const int* rots, int count, int* newDest);
void anim_move_speed(Object* object, int anim, float* tilesPerSec, float* framesPerSec);
bool anim_smooth_offset(Object* obj, int* dx, int* dy);
bool anim_get_smooth_motion();
void anim_set_smooth_motion(bool on);
void anim_smooth_tick();
void anim_smooth_reset();
unsigned int anim_dude_reg_serial();
typedef struct AnimSmoothStats {
    int pathSearches;
    double pathMs;
    double pathMaxMs;
    int glides;
    int glideSnaps;
    int liveGlides;
    int strideHolds;
    int strideStands;
    int cadenceHolds;
} AnimSmoothStats;
void anim_smooth_take_stats(AnimSmoothStats* out);

// --- actions.cc -------------------------------------------------------------------------------------
bool action_defender_reaction(Attack* attack);
void action_extras_reaction(Attack* attack);
Object* action_take_explosive_projectile(int* explosionFid);
#define ACTION_RT_LANDS_ON_ERASE (-2)
void action_notify_object_freed(Object* obj);

// --- combat.cc --------------------------------------------------------------------------------------
void combat_display_cant_end_message();
bool combat_is_attack_pending();
void combat_clear_pending_attack();
bool combat_commit_pending_attack();
void combat_take_pending_snapshot(Attack* out, bool* outCallDisplay);
bool combat_commit_snapshot(const Attack* snap, bool callDisplay);
bool combat_late_lethal_upgrade(Attack* attack);
bool combat_attack_is_slave_capture(const Attack* attack);
int combat_called_shot_pick(Object* critter, int hitMode, int initialLocation);
extern int combat_forced_throw_tile;
char* combat_get_loc_name(Object* critter, int hitLocation);

// --- combatai.cc ------------------------------------------------------------------------------------
int combatai_aggression(Object* critter);
Object* ai_best_weapon(Object* weapon1, Object* weapon2);
Object* ai_best_weapon_for(Object* critter, Object* weapon1, Object* weapon2);
int combatai_flee_reason(Object* critter);
int combatai_flee_tile(Object* critter, int actionPoints);
int combatai_max_dist(Object* critter);
int combatai_min_to_hit(Object* critter);
// Fallout 2: takes the target too (F2's ai_pick_hit_mode weighs it); combat_rt.cc's call is the one delta.
int combatai_pick_hit_mode(Object* critter, Object* weapon, Object* defender);
int combatai_called_shot(Object* critter, Object* target, int hitMode);
int combatai_msg_at(Object* critter, int hitLocation, int message_type, int delay);
bool combatai_msg_text(int num, char* out, size_t size);
int combatai_float_line(Object* critter, const char* text);
bool is_within_perception_unsneaked(Object* critter1, Object* critter2);

// --- critter.cc -------------------------------------------------------------------------------------
void critter_set_poison_spares_life(bool spares);
bool critter_get_radaway_cancels();
void critter_set_radaway_cancels(bool enabled);

// --- gdialog.cc -------------------------------------------------------------------------------------
bool gdialog_get_script_float();
void gdialog_set_script_float(bool on);
bool dialog_is_bartering();

// --- gmouse.cc --------------------------------------------------------------------------------------
int gmouse_3d_modes_are_enabled();
void gmouse_set_use_on_item(int pid);
void gmouse_3d_show_to_hit(Object* target);
void gmouse_set_move_cursor_hidden(bool hidden);
bool gmouse_move_cursor_is_hidden();
void gmouse_set_cursor_suppressed(bool suppressed);

// --- gsound.cc, cache.cc, fontmgr.cc, message.cc (trace meters and reports) -------------------------
void gsound_take_load_stats(int* count, double* ms);
void cache_take_miss_stats(int* count, double* ms);
const char* FMFontSource();
const char* message_language();

// --- intface.cc -------------------------------------------------------------------------------------
void intface_hp_roll_tick(float dt);
void intface_hp_roll_cancel();
void intface_update_heal_slot(Object* item, int count, bool armed = false);
void intface_invalidate_heal_slot();
void intface_update_quick_slot_lb(Object* item, int count, bool armed);
void intface_update_pad_badges();
void intface_pad_pill(unsigned char* buf, int pitch, int w, int h, int cx, int cy, const char* text);
void intface_update_aim_part(int hitLocation);
void intface_rt_combat_box_tick(bool engaged, float dt);
void intface_rt_combat_box_reset();
void intface_rt_combat_box_invalidate();
bool intface_get_combat_box_sound();
void intface_set_combat_box_sound(bool on);
void intface_reconcile_weapon_fid();
int intface_reload_weapon();
bool intface_can_reload_weapon();
Object* intface_reload_target();
bool intface_item_can_toggle_state();
bool intface_item_set_aiming();

// --- inventry.cc ------------------------------------------------------------------------------------
bool inven_is_open();
bool inven_any_open();
int inven_current_window_type();
bool inven_loot_cursor_over_panel();
bool inven_normal_cursor_over_list();
bool inven_normal_cursor_over_armor();
bool inven_equip_under_cursor(int hand);
bool inven_wear_under_cursor();
Object* inven_list_item_under_cursor();
bool inven_gamepad_quick_normal();
int inven_wield_instant(Object* critter, Object* item, int a3);
void inven_explosive_dropped_notice();
bool inven_set_timer_place_requested();

// --- loadsave.cc ------------------------------------------------------------------------------------
int SaveGameSlot(int slot, const char* description);
int QuickSaveGame();
int LoadGameSlot(int slot);
int LoadGameNewest();
int AutoSaveLocation();
int AutoSaveQuest();
bool LoadSaveConsumeJustLoaded();
unsigned int LoadGameSerial();

// --- map.cc, object.cc, tile.cc ---------------------------------------------------------------------
bool map_tile_is_background(int tile, int elevation);
void map_clear_pending_transition();
void map_request_world_map();
bool map_has_pending_transition();
bool obj_dude_on_exit_grid();
Object* obj_wall_blocking_at(Object* a1, int tile, int elevation);
void tile_take_refresh_stats(int* count, long long* pixels, double* ms);
int tile_take_full_refreshes();
int tile_set_center_offset(int tile, int flags, int offsetX, int offsetY);
void tile_view_save();
void tile_view_restore();
void tile_debug_ring_set(int radius);
int tile_debug_ring_get();
typedef void (*TileOverlayFn)(Rect* rect, int elevation);
void tile_set_overlay(TileOverlayFn fn);
void tile_overlay_line(Rect* clip, int x0, int y0, int x1, int y1, int color);

// --- options.cc, perk.cc, pipboy.cc, scripts.cc -----------------------------------------------------
bool options_is_open();
const char* perk_realtime_note(int perk);
unsigned int pipboy_quest_state_hash();
void scr_load_before_first_event(Object* obj);

// --- plib: input.cc (bk process timing), winmain.cc (the headless autotest window) -----------------
void bk_take_times(char* out, size_t size);
extern bool gnw_headless;
extern bool gnw_visible_override;

// --- Fallout 2 only (rt_f2_hooks.cc) ----------------------------------------------------------------
// One press of a pad button (an SDL_GameControllerButton) inside a vanilla screen's own loop, armed on
// release. `armed` must be a local of that loop, false at entry.
bool rt_pad_pressed(int button, bool* armed);
// FALLOUT_RT_PADTEST=<script> creates a virtual pad and presses it on a timeline. Call before
// gamepad_ui_init. No-op without the variable.
void rt_padtest_init();
// The virtual pad's joystick index while FALLOUT_RT_PADTEST runs, else -1 (gamepad_ui prefers it).
int rt_padtest_controller_index();

} // namespace fallout

#endif /* FALLOUT_RT_HOOKS_H_ */
