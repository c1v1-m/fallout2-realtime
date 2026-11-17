#ifndef FALLOUT_GAME_COMBAT_RT_H_
#define FALLOUT_GAME_COMBAT_RT_H_

#include "game/object_types.h"
#include "plib/db/db.h" // DB_FILE, for the medic opt-out save chunk

namespace fallout {

// Realtime combat system.
//
// Replaces the turn-based combat() interrupt with a per-frame, per-actor state
// machine. There is no "combat mode" the player enters or leaves: enemies aggro
// when the player comes within their perception range, act on cooldowns derived
// from their action points (Agility), and the player moves/attacks freely in
// realtime. See combat_rt.cc for the design notes.
//
// This system REUSES the existing damage pipeline (combat_attack, apply_damage,
// roll.cc, stat_level) rather than reimplementing it. It keeps the combat
// subsystem "warm" (the COMBAT_STATE_0x01 in-combat bit set while hostiles are
// near) so the shared attack/animation/damage code engages, but it never runs
// the turn scheduler, action-point UI, or end-turn flow.

// Lifecycle.
void combat_rt_init();
void combat_rt_exit();
void combat_rt_reset();

// Per-frame tick. dt is wall-clock seconds since the previous frame. Drives
// aggro, the NPC state machines, and cooldowns. Safe to call every frame even
// when no hostiles are present (it early-outs cheaply).
void combat_rt_update(float dt);

// Routes a player input event (a get_input() key code) to the realtime system:
// WASD tile movement and mouse-click-to-attack. Returns true if the event was
// consumed by combat_rt and should not be processed further.
bool combat_rt_handle_input(int keyCode);

// Polls the gamepad (SDL_GameController) once per frame: left stick -> tile
// movement, right trigger / A -> attack nearest hostile in range, B -> cancel.
// Called from combat_rt_update.
void combat_rt_poll_gamepad();

// True while at least one hostile is engaged (for music/UI hooks later).
bool combat_rt_active();

// "Is something actually coming for me right now?" -- an engaged, live hostile that is after the
// player's side AND can perceive him. Use this INSTEAD of isInCombat() for any vanilla rule that
// meant "you are in a turn-based encounter" (no talking, no script timers, ...): realtime keeps the
// warm combat bit set for as long as anything anywhere is chasing, which turns those short,
// bounded restrictions into permanent ones. Falls back to isInCombat() outside realtime.
bool combat_rt_under_threat();
// True while a fight is going on within sight of the player, whoever it is between -- including a
// brawl he is only a bystander to. Restores vanilla's "no chatting mid-fight" for the PLAYER-INITIATED
// talk gates only, without the map-wide lockout that a raw isInCombat() brings back. Cleared at once
// by combat_rt_end_combat(). See the note above the definition before widening its use.
bool combat_rt_fight_in_view();

// True while any critter on the dude's elevation is still playing its death animation. Saving calls
// anim_stop(), which force-ends every sequence and slams a mid-fall body flat, so an autosave must
// wait for this to clear.
bool combat_rt_death_anim_pending();

// Per-critter version of the above: is THIS critter actually fighting the player right now?
// Use it instead of combat_rt_is_hostile() wherever the question is "should I treat this as an
// enemy in front of me" -- the hostile flag itself is STICKY and never cleared, so a bystander
// caught by faction aggro carries it for the rest of the map's life even if it never acts.
bool combat_rt_is_engaging(Object* obj);

// SQUAD ORDERS (Fallout 2 Realtime, 2026-11-11): LT held + a D-pad tap orders every companion at once (gamepad_ui.cc).
// ATTACK = the LT-locked target -- an enemy, or since 2026-11-14 a neutral, whose fight they then start -- is the
// squad's focus, attacked first while it lives; FOLLOW / HOLD / CHARGE = COMBAT CONTROL's Distance "Stay close" /
// "Stay" / "Charge", written as the dialogue's panel writes it (saved, shown there).
// The realtime fight keeps to every Distance from now on. True when a companion took the order (the nearest says so
// over its head); false = refused, no lock or nobody who can take it -- the caller buzzes. Fallout 1: always false.
// Since design A it is combat_rt_squad_command(NULL, order, <the LT lock for ATTACK>, -1): the order names below. No
// order NUMBER is saved anywhere (RTSQ keeps hold fire and the weapon pid, the traces print the names), so the list may
// change, but every table indexed by it must follow: combat_rt.cc's three assert their length, gamepad_ui.cc's
// gpui_squad_order_name keeps its own copy of the names.
enum {
    COMBAT_RT_ORDER_ATTACK, // design B, Up
    COMBAT_RT_ORDER_FOLLOW, // design B, Down
    COMBAT_RT_ORDER_HOLD, // design B, Left
    COMBAT_RT_ORDER_CHARGE, // design B, Right
    COMBAT_RT_ORDER_GO, // design A: walk to a tile and hold it (the post)
    COMBAT_RT_ORDER_HOLD_FIRE, // design A: start no attack
    COMBAT_RT_ORDER_OPEN_FIRE, // design A: hold fire lifted
    COMBAT_RT_ORDER_HEAL_ME, // design A: come and dose the dude
    COMBAT_RT_ORDER_NO_HEALING, // design A: Y, the medic opt-out -- they keep their supplies from him
    COMBAT_RT_ORDER_HEAL_AGAIN, // design A: Y, the opt-out lifted
    COMBAT_RT_ORDER_KEEP_DISTANCE, // design A: R3, Distance "Snipe" -- back off from what comes near (2026-11-17)
    COMBAT_RT_ORDER_COUNT,
};
bool combat_rt_squad_order(int order);

// --- SQUAD COMMAND (Fallout 2 Realtime, 2026-11-12, his design "A": „přidej nestřílej a vyléčit mě, změň zbraň, můžu
// mu i říkat kam má jít?“). LT + A opens the squad command mode (gamepad_ui.cc); everything it orders lands here.
// GO -- a tile they walk to and hold, the post -- was taken out on 2026-11-13 („nehodí se to do této action oriented
// verze“; Y's healing switch, NO HEALING / HEAL AGAIN, came in then and stays) and came back on 2026-11-14, his „no to
// jsem myslel že vrátíš“.
// `who` everywhere: NULL = every companion on the dude's map and floor, alive and not knocked out ("All"), else that
// one party member. Nothing here draws or plays a sound -- the mode clicks on true and buzzes on false. Every order
// floats one line over the nearest companion of `who` that took it ("On it.", "Coming.", "Holding.", "Charging!",
// "Moving.", "Holding fire.", "Weapons free.", "Hold still.", "Saving supplies.", "I'll patch you up.", "Got it."), a
// refusal with words to it over the nearest one asked ("No meds.", "You're fine.", "Can't get there.", "Out of
// ammo."; "Can't reach you." comes later, from a HEAL ME that gives up). Traced as [ACTION/squad-order] order= target=
// companions= skipped= why= speaker= t= who= tile=. Fallout 1: every order false, every query empty.
//
// Hold fire and the weapon pick are SAVED (the realtime chunk's RTSQ section, by object id) and kept across maps, and
// so is NO HEALING (it is the dialogue's opt-out, saved in the chunk's own list); the posts, every focus and a HEAL ME
// on its way end with a map change or a load. A load or a new game resets all of it before the save's own are read.

// The order to `who`. Returns true when at least one companion took it.
//   ATTACK     `target` must be an enemy or a neutral, alive on this floor and not on his side (the mode offers it where
//              combat_rt_squad_kind says ENEMY or NEUTRAL -- a neutral only in his sight): NULL / dead / gone ->
//              false why=no-target, his side -> why=friend. A neutral is taken since 2026-11-14 (his „Ano, i na
//              neutrály“), and the order does not make it hostile: the companions go for it, and their first blow that
//              reaches it (a hit, or a miss with nothing behind it to take the round) turns it and its team on them, by
//              vanilla's retaliation. For All it becomes the squad's focus (design B: a red chevron over it, everyone's
//              own focus cleared); for one, that one's own focus (a chevron too). Lifts hold fire and ends the post of
//              whoever took it.
//   FOLLOW     Distance "Stay close" (design B) plus the regroup: posts and own foci end; for All the squad's focus
//              too; for one, it ignores the squad's focus till its next ATTACK.
//   HOLD       Distance "Stay" (design B). Posts stay.
//   CHARGE     Distance "Charge" (design B); lifts hold fire and ends the post. party.txt-refused ones are skipped
//              (Vic will not charge): skipped=, and false when nobody could.
//   GO         `tile` (combat_rt_squad_tile_free): they walk there and hold it -- "Stay" at that tile, in a fight and
//              out of one, until FOLLOW, another GO, CHARGE, ATTACK, a floor or map change, or a load. All: the
//              nearest the tile itself, the others the nearest free tiles round it, one each. A tile nothing may stand
//              on -> false why=tile; nobody can reach it -> false why=no-route; both "Can't get there." (a pathfinder
//              search per companion: on A, never per frame).
//   HOLD FIRE / OPEN FIRE   no new attack / attacks again. Ask combat_rt_squad_holds_fire(who) which one X gives.
//   HEAL ME    the one chosen, or for All the nearest that carries a stimpak, super stimpak, healing powder or a kit
//              (and is a biped), comes and doses the dude past the medic's threshold, cooldown and opt-out. Refused:
//              dude unhurt -> why=fine "You're fine."; nobody with supplies -> why=no-meds "No meds.".
//   NO HEALING / HEAL AGAIN   the party dialogue's "(Stop patching me up. Save your supplies.)" / "(You can patch me
//              up again.)" for every one of `who`: the very state that line toggles
//              (combat_rt_medic_set_treats_player), saved with it. Only the medic beat asks it -- HEAL ME goes past it
//              -- and nothing else changes (no Distance, no hold fire; a HEAL ME on its way goes on). One already so
//              counts as taken, as with X. Its line comes from the nearest one that carries something to heal with
//              (else the nearest). Ask combat_rt_squad_medic_off(who) which one Y gives. Refused without a
//              word: no medic beat at all (combat_rt_medic_beat_enabled false) -> why=medic-off; the opt-out list full
//              (RT_MEDIC_OPTOUT_MAX ids, one per companion: no real party gets there) -> that one skipped, why=full
//              when nobody took it.
// `target` is read for ATTACK only, `tile` for GO only.
bool combat_rt_squad_command(Object* who, int order, Object* target, int tile);

// The companions the mode steps through with LB / RB after "All": every one on the dude's floor, alive, awake, nearest
// the dude first. Fills out[] (up to max) and returns how many. Pointers are valid until the next frame of the world
// (or a combat_rt_set_squad_mode_hooks forget call names one).
int combat_rt_squad_members(Object** out, int max);

// Whether `who` holds its fire -- for All, true only when every one of them does (the legend's "X Open fire"); false
// with nobody there. X then gives HOLD_FIRE when false, OPEN_FIRE when true.
bool combat_rt_squad_holds_fire(Object* who);

// Whether `who` keeps its supplies from him -- the medic opt-out the party dialogue line toggles, which NO HEALING sets
// (combat_rt_medic_treats_player false) -- for All, true only when every one of them does (the legend's "Y Heal me
// again"); false with nobody there. Y then gives NO_HEALING when false, HEAL_AGAIN when true. The same list as the
// order's, so the legend turns with a press. While combat_rt_medic_beat_enabled() is false (the medic opt-out block
// below; the dialogue hides its line on that test) the mode has no Y at all. Fallout 1: always false.
bool combat_rt_squad_medic_off(Object* who);

// What the critter under the cursor is, for the label over it: ENEMY "ATTACK" (red), NEUTRAL "ATTACK" (yellow), DUDE
// "HEAL ME", COMPANION "WEAPON", NONE (no label, A buzzes -- one on his side who is no companion, a neutral he cannot
// see, anything else; free ground is asked separately: combat_rt_squad_tile_free, GO). ENEMY and NEUTRAL are ATTACK's
// rule: on this floor, alive, not on his side. ENEMY is the red outline the LT lock uses -- fighting him or hostile by
// nature; NEUTRAL is everyone else there whom the lock would paint yellow: in his sight, or the one he is locked on
// (2026-11-14, his „Ano, i na neutrály“: ATTACK on one starts that fight; one a roof or a wall hides reads NONE).
// Fallout 1 (no squad): never NEUTRAL.
enum {
    COMBAT_RT_SQUAD_KIND_NONE,
    COMBAT_RT_SQUAD_KIND_ENEMY,
    COMBAT_RT_SQUAD_KIND_DUDE,
    COMBAT_RT_SQUAD_KIND_COMPANION,
    COMBAT_RT_SQUAD_KIND_NEUTRAL,
};
int combat_rt_squad_kind(Object* obj);

// Who would answer HEAL ME from `who` now, or NULL -- then `why` (may be NULL) is "fine" (he is unhurt: his HP plus a
// heal still coming in reaches his maximum), "no-meds" (nobody of `who` carries anything to heal with, or only
// non-bipeds do) or "no-companions". The label greys out on NULL. Cheap (inventory scans).
Object* combat_rt_squad_healer(Object* who, const char** why);

// GO's tile test, cheap enough for every frame: a valid tile on the dude's floor, nothing standing on it (wall,
// scenery, critter -- the dude too), not the black past the map's edge.
bool combat_rt_squad_tile_free(int tile);
// Can somebody of `who` get there? A pathfinder search (up to 12000 nodes, the nearest few companions): ask on a
// cursor tile change, not every frame; the last answer is kept. The order itself searches again when given.
bool combat_rt_squad_can_reach(Object* who, int tile);

// WEAPON. The weapons companion `c` carries that its body has the art to hold (one per kind: the one in hand, else the
// fullest), for the radial wheel titled with object_name(c), plus the mode's own "Auto" segment. Returns how many.
int combat_rt_squad_weapons(Object* c, Object** out, int max);
// Its current pick: the weapon's pid, -1 = Auto (the AI's own choice).
int combat_rt_squad_weapon_pick(Object* c);
// Can it fire that one now (rounds in it, or rounds for it in its pack; melee and thrown always)? For greying a dry gun.
bool combat_rt_squad_weapon_ready(Object* c, Object* item);
// The pick: `item` (one of combat_rt_squad_weapons) or NULL = Auto. The weapon goes in its hand at its next free
// beat -- after the mode, the world moving again -- and the realtime AI keeps to it while it carries it and can fire
// it; without either it falls back to the AI's choice and says "Out of ammo." once. False = refused: not its own, no
// art for it, or dry with nothing to load ("Out of ammo."). Says "Got it." on true. Logged order=weapon.
bool combat_rt_squad_set_weapon(Object* c, Object* item);
// The pick forgotten without a word -- COMBAT CONTROL's "Use best weapon" (game_dialog.cc) chooses for it instead.
void combat_rt_squad_forget_weapon(Object* c, const char* why);

// ARMOR (2026-11-17): COMBAT CONTROL's "Use best armor" from the companion's wheel. Can `c` wear armour at all (a party
// member, a biped, not the one vanilla's button leaves out)? The armour it would put on -- the best in its pack, better
// than what it wears -- or NULL. And the putting on: says "Suiting up." (or "Nothing better." and false).
bool combat_rt_squad_can_wear_armor(Object* c);
Object* combat_rt_squad_best_armor(Object* c);
bool combat_rt_squad_wear_best_armor(Object* c);

// Who said the last order's line ("Keeping my distance.", "Got it.", "No meds." ...), or NULL: the mode, open after an
// order since 2026-11-17, moves his name aside for it.
Object* combat_rt_squad_last_speaker();

// Its post's tile (GO), or -1 -- for drawing the posts while the mode is up.
int combat_rt_squad_post_of(Object* c);
// Its focus now -- its own (ATTACK to it alone), else the squad's -- or NULL; `c` NULL = the squad's. Every live focus
// has a red chevron over it, drawn by combat_rt.
Object* combat_rt_squad_focus_of(Object* c);

// The mode is up (true) or gone (false). From the opening the whole world SLOWS DOWN, eased, over 1.5 s of real time
// (his pick 2026-11-12: first a slowdown, then the stop) and then holds still exactly as under a radial wheel (the
// combat model, AP, cooldowns; gamepad_ui stops the engine's own processes with map_disable_bk_processes once
// combat_rt_squad_mode_frozen says so). The pad is the mode's from the first frame (the stick walks nobody). On the
// close everything runs at full speed at once. Call on every open and close edge; `why` is traced ([FLOW/squad-mode]
// open|close why=; slow and freeze lines in between). A load or a new game closes it here too (the mode must reset
// itself then: its forget hook is called with NULL).
void combat_rt_squad_mode_set_open(bool open, const char* why);
bool combat_rt_squad_mode_is_open();
// The mode is up and its slowdown has run out: the world holds still (gamepad_ui's hold of the background processes).
bool combat_rt_squad_mode_frozen();

// How fast the world runs against real time, for the clocks outside combat_rt -- the animation scheduler's
// (animation.cc) and the game clock's (scripts.cc): 1 normally, the squad command mode's eased slowdown while it runs
// (never under 0.05), 0 once the mode holds the world still. Fallout 1: always 1.
float combat_rt_world_rate();

// The mode's two hooks into combat_rt, both optional (NULL = none). `overlay` draws on top of the world overlay (after
// the blast rings, the tracers and the chevrons) for every repainted rect: window coordinates, tile_overlay_line into
// `rect`, nothing that reads objects without checking them. `forget(obj)` is called right before `obj` is freed (a
// script's destroy, a corpse removed), and forget(NULL) before every object goes (a map change, a load, a new game,
// the game's end) -- clear every pointer the mode keeps. It may be called while the mode is closed, or before it
// was ever opened.
struct Rect;
typedef void (*CombatRtSquadOverlayFn)(struct Rect* rect, int elevation);
typedef void (*CombatRtSquadForgetFn)(Object* obj);
void combat_rt_set_squad_mode_hooks(CombatRtSquadOverlayFn overlay, CombatRtSquadForgetFn forget);

// scripts.cc, around a companion's critter_p_proc heartbeat: true while a post (GO) or a HEAL ME must not be undone by
// its party script's follow. The heartbeat then runs with GVAR_PARTY_NO_FOLLOW set -- vanilla's own switch, which
// every companion's follow asks first.
bool combat_rt_party_follow_held(Object* critter);

// Manual END COMBAT -- the realtime replacement for vanilla's Enter key / the interface bar's
// END COMBAT button, both of which live in the dormant turn-based path and are unreachable here.
// It is NOT an escape hatch: exactly like vanilla's combat_end(), it only succeeds when every
// live hostile already wants to stop (disengaging, fleeing, or perceiving nobody to fight), and
// otherwise refuses and prints combat.msg #103 so the player is told WHY. Returns true if combat
// ended (or was not running).
bool combat_rt_end_combat();

// True when a hostile the player could hit is within current-weapon range. The
// gamepad layer uses this to arbitrate the A button: A attacks when this is true,
// otherwise A is a world/UI interact (left click). Cheap; safe every frame.
bool combat_rt_hostile_in_attack_range();

// True if this specific critter is currently flagged hostile to the dude.
bool combat_rt_is_hostile(Object* obj);

// True while a right-stick lock-on target is set. The gamepad layer also treats
// A as a committed attack (never a UI interact) whenever this is true, so a lock
// on an out-of-range target still consumes A (it just doesn't fire) instead of
// falling through to a world click.
bool combat_rt_has_lock();
void combat_rt_set_quick_throw_cost(int cost);
Object* combat_rt_lock_target();
// True while the player is walking under left-stick control (not standing, not walking out a
// click-to-move path).
bool combat_rt_stick_walking();
// A script's game_ui_disable (called BEFORE the UI goes off) / game_ui_enable (op_game_ui_*), and
// whether that lock holds now: the pad, the stick, the use prompt and the follow camera stand aside
// while it does. A script's gfade_out / gfade_in, so a lock ended from our side can give the screen back.
void combat_rt_note_script_ui_lock(bool on);
void combat_rt_note_script_fade(bool black);
bool combat_rt_script_ui_locked();
// A script's timed event ran or a script floated a line (the lock's idle watchdog).
void combat_rt_note_scene_progress();
// A scene whose lock a fight ended early is still running (the autosave waits for it).
bool combat_rt_scene_pending();
// The pad set the dude off on a walk-then-act of the player's own; `serialBefore` is
// anim_dude_reg_serial() from before it. A script's lock that begins while it is still his latest
// sequence cuts it.
void combat_rt_note_player_walk(unsigned int serialBefore);

// MODS page: live read/write of the two gamepad movement assists. The cfg is still where they are
// LOADED from at init; these are how the settings screen changes them without a re-init.
void combat_rt_set_cam_smooth(float k);
// MODS "Camera leans to target": ease the view toward an aimed target that is off the screen.
void combat_rt_set_cam_lean(bool on);
bool combat_rt_get_cam_lean();
// MODS "Hits stagger": the stagger chance factor (0 = every hit through armor staggers).
void combat_rt_set_flinch_chance(float factor);
float combat_rt_get_flinch_chance();
float combat_rt_get_cam_smooth();
void combat_rt_set_knockdown_agility(float factor);
float combat_rt_get_knockdown_agility();
void combat_rt_set_door_magnet(float strength);
float combat_rt_get_door_magnet();
void combat_rt_set_npc_slide(bool enabled);
bool combat_rt_get_npc_slide();
void combat_rt_set_mouse_hold_walk(bool enabled);
bool combat_rt_get_mouse_hold_walk();
void combat_rt_set_forgive_stray(bool enabled);
bool combat_rt_get_forgive_stray();
void combat_rt_set_crowd_make_room(bool enabled);
bool combat_rt_get_crowd_make_room();
void combat_rt_set_tactical_ai(bool enabled);
bool combat_rt_get_tactical_ai();
// MODS "Attackers at once": how many enemies attack the dude at one time, the rest wait their turn. 0 = no limit.
void combat_rt_set_max_attackers(int count);
int combat_rt_get_max_attackers();
// MODS "Bonus Move": how fast the perk's movement reserve comes back while the dude stands. 0 Fast (the AP regen
// itself), 1 Half turn (default), 2 Turn (vanilla's once per turn: the time the whole AP pool takes at the base rate).
void combat_rt_set_bonus_move_refill(int mode);
int combat_rt_get_bonus_move_refill();
void combat_rt_set_projectiles(bool enabled);
bool combat_rt_get_projectiles();
int combat_rt_tracers_in_air();
void combat_rt_set_tracer_look(float lengthHexes, int r, int g, int b, bool underline);
// MODS "Blast ring" (cfg blast_ring): a ring of red dots at the blast radius round where a held quick throw of a
// grenade would land and round where each grenade or rocket in the air goes off. 0 = off, 1 = with the
// Awareness perk (the default), 2 = always.
void combat_rt_set_blast_ring(int mode);
int combat_rt_get_blast_ring();
// actions.cc, as a projectile's flight is launched: the tile it flies to (where its blast ring sits).
void combat_rt_flight_launched(Object* projectile, int tile);
bool combat_rt_weapon_amnesty();
bool combat_rt_critter_fighting(Object* critter);
bool combat_rt_fm_foreign_touch();
bool combat_rt_forgive_stray(Object* attacker, Object* bystander, Object* intended);
// MODS "Enemies look around" (cfg enemies_look_around): idle hostiles turn toward gunshots and
// scan around, so a back turned to you is not a permanent blind spot. See rt_idle_look_around.
void combat_rt_set_look_around(bool enabled);
bool combat_rt_get_look_around();

// MODS "Script combat turns" (cfg script_turns, default on = vanilla): every scripted critter on the
// map gets the combat event vanilla gives it each turn -- the start of its turn while it fights, the
// "do you join?" check while it does not. See rt_script_turn_tick.
void combat_rt_set_script_turns(bool enabled);
bool combat_rt_get_script_turns();
void combat_rt_set_free_move(bool enabled);
bool combat_rt_get_free_move();
// Free movement: the offset the dude stands at on his tile, for dude_stand to keep instead of
// re-centring him. False = re-centre as vanilla (free movement off, or he is on another tile).
bool combat_rt_dude_rest_offset(int* x, int* y);

// --- L3 aimed-shot wheel support (gamepad_ui drives the body-part overlay). ---

// Whether an aimed shot is possible right now: the dude is in combat, the active
// weapon supports called shots, and there is a target (the lock-on, else the nearest
// hostile in range). The gamepad layer gates the body-part wheel on this.
bool combat_rt_can_aim();
// The weapon side of combat_rt_can_aim only: the dude can make a called shot with what he holds,
// target or not. R3 / V open the called-shot window on this, to bind a part ahead of the fight.
bool combat_rt_weapon_can_aim();

// The critter an aimed shot would currently hit (the lock-on target, else the nearest
// hostile in range), or NULL. The gamepad layer uses it to show the target's body in the
// wheel hub.
Object* combat_rt_aim_target();

// Keyboard & mouse (2026-10-10): the hostile the mouse is aiming at, or NULL (combat_rt.cc), and the
// keyboard's Q / E lock step (-1 left, +1 right).
Object* combat_rt_mouse_target();
void combat_rt_request_lock_step(int dir);

// To-hit % for a called shot at `hitLocation` (a HIT_LOCATION_* specific value)
// against the current aim target, clamped to 0..999. 0 if there is no target. Used to
// label each wheel segment.
int combat_rt_aim_chance(int hitLocation);

// Queue an aimed shot at the current target hitting `hitLocation`. Returns true if it
// was latched (the arbiter fires it on the next eligible frame, paying the aimed AP
// cost), false if no valid target / the weapon can't aim / the location is invalid.
bool combat_rt_aimed_attack(int hitLocation);

// The hit mode the player's next attack would use. The called-shot window needs it to print the
// right to-hit percentage per body part.
int combat_rt_player_hit_mode();

// Why an aimed shot is impossible right now, as a short player-facing phrase -- or NULL when it is
// possible. Several of the reasons are VANILLA rules the player cannot see from the interface bar
// (Fast Shot forbids called shots outright; burst and continuous modes cannot be aimed; neither can
// flame, explosive or EMP damage), so a silent refusal is indistinguishable from a broken button.
const char* combat_rt_aim_refusal();

// Green-highlight (Fallout 3 VATS style) the current aim target while the body-part wheel
// is up, replacing its combat red outline; the off call restores the exact prior outline.
// Called by the gamepad layer on aim-wheel open/close. No-op if there is no target.
void combat_rt_aim_highlight_on();
void combat_rt_aim_highlight_off();

// Per-companion "stop patching me up", set from the dialogue -- and since 2026-11-13 by Y in the
// squad command mode too (NO HEALING / HEAL AGAIN, the same state). Keyed by object id so it survives
// map changes and loads. Only the PLAYER is refused -- the medic still doses itself and other
// allies. combat_rt_medic_optout_load treats a missing chunk as "not in this save" and defaults
// to everyone healing you, which is what makes the save format change backward compatible.
// "Give me some room": the follower WALKS a few tiles away from the dude. Queued rather than done
// on the spot, because the dialogue that asks for it is modal -- it fires on the first update after
// the conversation closes. Pass NULL to cancel a standing request.
void combat_rt_request_step_back(Object* npc);

// Is there a medic beat at all (cfg medic_enabled, realtime on)? Without one the dialogue offers no
// "stop patching me up" line and the squad command mode no Y.
bool combat_rt_medic_beat_enabled();
bool combat_rt_medic_treats_player(Object* npc);
void combat_rt_medic_set_treats_player(Object* npc, bool treats);
// The realtime SAVE CHUNK (load/save handler #26, second to last): the medic opt-outs, the AP pool (RTAP), the
// quick slots (RTQS), then tagged sections -- RTFH (who was fighting the dude, restored on the same map) and
// RTFM (the fled-enemy memory, Fallout 2). combat_rt_medic_optout_load treats any missing part as "not in this
// save" and keeps the defaults; it never fails the load. Every read is EOF-safe (rt_save_read_int).
void combat_rt_medic_optout_clear();
int combat_rt_medic_optout_save(DB_FILE* stream);
int combat_rt_medic_optout_load(DB_FILE* stream);

// Rate of fire, for the character sheet to PRINT. Both go through the same helpers the fight
// uses, so the card can never disagree with the combat model.
//
// combat_rt_skill_attack_speed: seconds between attacks for one of the six combat skills, at
//   `skillLevel`, with `critter`'s current SPECIAL folded in -- plus the two multipliers that
//   made it, so the card can say how much of the number is the skill's doing. The attack
//   category comes from the skill, so no weapon needs to be in hand.
// combat_rt_stat_attack_speed: the multiplier a SPECIAL level buys, for the one attack category
//   that stat owns (STR melee, PE ranged, AGI thrown).
//
// Both return false when there is nothing true to print: turn-based mode, the cooldown disabled
// by config, or a stat/skill that owns no attack category.
bool combat_rt_skill_attack_speed(Object* critter, int skill, int skillLevel,
    float* seconds, float* statMult, float* skillMult);
bool combat_rt_stat_attack_speed(int stat, int statLevel, float* mult, int* attackType);
float combat_rt_attack_cooldown_base();

// Tap-A world loot: if an adjacent corpse or container is next to the dude, stash
// it (opened by combat_rt_update) and return true so the gamepad layer routes A to
// looting instead of a walk-click. Safe to call from the input hook (detection
// only; the loot window is opened later from the main loop).
bool combat_rt_try_world_loot();

// Hook from combat(): when a critter's script routes an attack on the dude
// through the turn-based entry point (op_attack -> scripts_request_combat ->
// combat()), combat() forwards the attacker/defender here before refusing to
// run. combat_rt uses it as the authoritative "this critter is hostile to the
// player" signal (genuine hostiles request combat; peaceful NPCs never do).
void combat_rt_notify_attack(Object* attacker, Object* defender);

// Hook from the realtime damage committer (combat_commit_pending_attack) for a critter that the
// hit KILLED or knocked out. Vanilla's combatai_notify_onlookers ran on dying defenders too;
// combat_rt's aggro spread cannot, because it needs a live seed hostile -- so without this a
// one-shot kill was invisible to the critter standing right next to the victim.
void combat_rt_notify_victim(Object* attacker, Object* victim);

// Hook from _damage_object (combat.cc), where vanilla adds to damageLastTurn: `critter` took damage
// from `attacker`'s attack. combat_rt remembers it until the fight ends, because damageLastTurn itself
// is zeroed at every script turn; a critter's blow on the dude also ends a script's scene lock.
void combat_rt_note_hurt(Object* critter, Object* attacker);

// Fallout 2: an encounter's two groups are set on each other (_caiSetupTeamCombat, from wmSetupRandomEncounter).
// combat_rt keeps the two teams for this map, writes them into the save, and combat_rt_rearm_team_fight -- run
// once a load is complete -- sets them on each other again, since nothing else in a save keeps that set-up.
void combat_rt_note_team_fight(Object* attackerTeam, Object* defenderTeam);
void combat_rt_rearm_team_fight();

// Hook from obj_remove_all() (object.cc) right before a map teardown frees every object.
// Abandons any in-flight realtime attack and drops all object pointers combat_rt holds
// (rt_resolving, the actor registry, lock/intent/loot, the engine's pending main_ctd) so a
// stale attack can't commit against freed memory after a mid-combat map exit. Idempotent.
void combat_rt_notify_objects_freed();
// Cancel a queued walk-there-then-act the player started with A. True if something was cancelled.
bool combat_rt_cancel_player_action();

// Hook from obj_erase_object() (object.cc) right before a SINGLE object is freed (a script
// destroy, a gibbed/cleaned-up corpse, etc.). If a critter is being freed mid-realtime-combat,
// clears every dangling whoHitMe pointer that references it (so apply_damage's retaliation
// check can't dereference freed memory) and drops combat_rt's own pointers to it. No-op outside
// realtime combat / for non-critter objects.
void combat_rt_notify_critter_removed(Object* removed);

// Total damage from attacks that are already IN FLIGHT against `defender` (computed and
// snapshotted, but not yet committed), plus whether one of them is already flagged lethal.
// combat.cc's check_for_death() calls this so that an attack's DAM_DEAD flag -- which is decided
// at FIRE time -- accounts for the damage that will land before this attack does. Without it,
// concurrent attacks each compute "not lethal" against the same stale HP, nobody registers a
// death animation, and the kill falls into critter_adjust_hits' emergency critter_kill() path.
// `killClaimed` (may be NULL) reports that another in-flight attack already owns the death, so
// this one must not register a second death animation. Returns 0 outside realtime.
int combat_rt_pending_damage_on(Object* defender, bool* killClaimed);

// Defensive guard called from combat_commit_pending_attack() right before apply_damage(): clears
// a dangling whoHitMe on either attack party so apply_damage's retaliation check
// (combatai_check_retaliation -> combatai_rating(defender->whoHitMe)) can't dereference a freed
// Object. Validates the whoHitMe pointer by membership in the live critter list (a scan that
// never dereferences the candidate) and NULLs it if the referenced critter is gone. Covers any
// free path that the obj_erase_object hook misses. No-op outside realtime combat.
void combat_rt_validate_who_hit_me(Object* a, Object* b);

// Called from map_leave_map() / the elevation-change guard when the player leaves a map
// MID-COMBAT in realtime. Ends realtime combat cleanly (clears the engine warm bit so
// isInCombat() becomes false and the map transition proceeds) instead of the vanilla
// "in combat -> game_user_wants_to_quit = 1" path, which boots the player to the main menu.
void combat_rt_end_for_map_exit();
// The dude was just put down elsewhere on the map (map.cc mapHandleTransition): he stands, free movement starts afresh.
void combat_rt_dude_teleported();

// op_terminate_combat in realtime: end the engagement unconditionally (the script decided), instead
// of vanilla's game_user_wants_to_quit = 1, which only the dormant turn-based loop ever consumed --
// under combat_rt it dropped the player to the main menu as if they had died.
void combat_rt_terminate_from_script();

// Called from map_leave_map() when the player steps onto a map-exit grid IN realtime combat.
// Arms the exit: on its next frame combat_rt ends the fight so the queued transition fires --
// immediately (the 10 s hold-position delay was removed 2026-10-06 at his request).
// scripted = the transition did not come from stepping onto an exit grid (stairs, ladders and
// elevators are scripted): it goes on the next frame instead of waiting for the dude to stand on one.
void combat_rt_arm_map_exit(bool scripted = false);

// Route a legacy player-attack request (combat_attack_this / the crosshair mouse path)
// into the realtime intent system. Calling combat_attack() directly in realtime sets the
// engine's pending flag without combat_rt's rt_resolving slot, wedging the attack lock
// until the 2.5s watchdog (a frozen, damage-less swing); this resolves the attack through
// rt_do_attack instead. Only a hostile combatant in weapon range is attacked (else no-op).
// Returns true if attack intent was set.
bool combat_rt_player_attack(Object* target);

// game_trace heartbeat callback: emits one TRACE_STATE snapshot of the realtime combat
// world (actors, hostiles, dude HP/AP, lock, pending/resolving, anim-table occupancy)
// plus invariant health-check TRACE_WARNs. Registered with trace_set_snapshot_fn.
void combat_rt_trace_snapshot();

// True once combat_rt owns combat (between combat_rt_init and combat_rt_exit).
// The turn-based combat() scheduler checks this and refuses to activate while
// it is set, so the old combat mode can never take over.
bool combat_rt_enabled();

// True while the player is actively feeding movement input (within a short grace
// window of the last WASD/stick step). actions.cc uses this to suppress the dude's
// enemy-miss dodge while he's moving, so a miss never interrupts his movement.
bool combat_rt_dude_moving();

// True while the critter is in its anti-stunlock flinch-immunity window (see combat_rt.cc).
bool combat_rt_flinch_immune(Object* obj);

// True when this attacker's weapon still counts as SHOULDERED from a recent shot, so action_ranged
// should SKIP the ANIM_POINT raise. The raise is a per-shot 0.43 s tax when you fire continuously,
// so it is paid once per burst rather than once per round -- see the long note in combat_rt.cc.
// Governed by cfg `ranged_point_hold` (< 0 never raises, 0 raises every shot, > 0 = the hold).
bool combat_rt_weapon_is_up(Object* attacker);

// The player's realtime action-point pool and its cap (the dude's max AP). The
// interface bar's AP indicator reads these so the bulbs reflect the real pool
// (which paces attacks) instead of the engine combat.ap that combat_rt keeps
// maxed for free movement. Both clamp safely before the first update frame.
int combat_rt_player_ap();
// The REAL remaining action points of any critter in a realtime fight, or -1 when combat_rt is not
// the authority (disabled / untracked). The engine's combat.critter.ap is pinned to max and is NOT
// this number -- see the definition in combat_rt.cc.
int combat_rt_critter_ap(Object* critter);
int combat_rt_player_max_ap();
bool combat_rt_spend_ap(int cost);
// Return points an action took up front and then never did (a cut reload). No-op out of a fight.
void combat_rt_refund_ap(int amount);
// MODS "Reload animation": who is in the middle of a reload loop. begin/end bracket it; the query
// also drops a body whose loop was replaced by something else (knockdown, walk, attack).
void combat_rt_reload_begin(Object* critter);
void combat_rt_reload_end(Object* critter);
bool combat_rt_is_reloading(Object* critter);
// The dude's stimpak jab (gamepad_ui): spared by an enemy's trigger pull like a reload, ended only
// by a stagger or a knockdown. The query drops a body that has left the use pose.
void combat_rt_inject_begin(Object* critter);
void combat_rt_inject_end(Object* critter);
bool combat_rt_is_injecting(Object* critter);

// A drug's healing in a fight comes in over time, the dude's and every fighter's (see rt_heal_left in
// combat_rt.cc). False = not pooled (not in the fight's table); the caller leaves the heal as it was.
bool combat_rt_heal_over_time(Object* critter, int hp, int pid);
int combat_rt_heal_left();
void combat_rt_heal_lend_for_save();
void combat_rt_heal_return_after_save();
// A body in a knockdown or a death -- falling, lying, getting up, dying (the reaction layer's view).
// An enemy's trigger pull must not cut it (action_attack); only the commit decides what a new hit does.
bool combat_rt_in_heavy_reaction(Object* critter);

// Self-played tests only (autotest `sethp ... knock`): knock `critter` down the way a knockdown hit's commit
// does -- the reaction layer's FALLING, DAM_KNOCKED_DOWN, the fall in its own sequence -- with no damage.
void combat_rt_test_knockdown(Object* critter, bool knockout);
void combat_rt_test_stagger(Object* critter);

// Empty a critter's realtime pool. The realtime stand-in for vanilla's "lose your turn" critical
// failure: there are no turns, so the cost is paid as points plus the wait for them to come back.
// Safe on anyone -- the dude, a tracked NPC, or a critter the registry has never heard of.
void combat_rt_zero_ap(Object* critter);

// Is something adjacent lootable RIGHT NOW (a corpse or a container)? Pure query -- unlike
// combat_rt_try_world_loot it queues nothing. The pad asks this the instant A goes down, to tell a
// press that MEANT to loot from the press that merely happened to kill something.
bool combat_rt_world_loot_available();

// True while the dude's own attack cooldown is still running -- he shot or swung a moment ago.
// Distinguishes "mashing A at an enemy" from "deliberately double-tapping to loot".
bool combat_rt_player_attack_busy();

// True while movement is STOPPED on an empty pool and has not yet earned its way back: the
// latch runs from the moment walking/running drains the pool to 0 until AP has climbed back
// to the reserve threshold (cfg `resume_ap`, clamped to the dude's max). False out of a
// fight, where nothing drains the pool and no threshold is enforced. The interface bar
// paints the AP indicator RED for exactly this window, so the "you stop at 0, you move
// again at 3" rule is on screen instead of being a silent loss of control.
bool combat_rt_move_blocked();

// The Bonus Move perk's movement-only reserve (2 points per rank): spent by walking and running
// before the regular pool, refilled while the dude stands, never touched by anything else. 0
// without the perk and 0 out of a fight. The interface bar draws it as the vanilla yellow bulbs.
int combat_rt_bonus_move_ap();

// How fast the world is currently running, as a fraction of normal. 1.0 nearly always; less than
// that while the on-body limb picker is up, which slows the fight instead of pausing it.
//
// Read by anim.cc's compute_tpf as well as by combat_rt itself, so the animation clock and the
// combat model slow together -- a critter that decides at quarter speed but walks at full speed
// looks broken. Never returns 0: this is a slow-down, never a stop.
float combat_rt_time_scale();
float combat_rt_flinch_seconds();

// QUICK THROW (the LB / RB quick slots, gamepad_ui): whom a throw from the slot goes at -- the locked
// target while LT holds a lock (if it is in throwing range), else whom RT without a lock would hit;
// NULL when there is nobody. combat_rt_request_throw fires the active hand at it like an RT press;
// combat_rt_player_attack_pending says whether that attack has not happened yet.
Object* combat_rt_quick_throw_target();
bool combat_rt_request_throw(Object* target);
// The THROW mode a held quick throw will use (a spear's secondary), for the to-hit % over its target
// (_combat_to_hit); false when no quick throw is held or the hand does not throw.
bool combat_rt_quick_throw_hit_mode(int* hitMode);
bool combat_rt_player_attack_pending();
bool combat_rt_dude_targeted();
bool combat_rt_dude_attacking();

} // namespace fallout

#endif /* FALLOUT_GAME_COMBAT_RT_H_ */
