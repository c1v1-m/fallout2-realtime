# Fallout 2 Realtime — the port

The realtime work of **Fallout 1 Realtime** (fallout1-ce, branch `rt-combat-wip`) carried onto
fallout2-ce. Same engine family, different names: fallout2-ce renamed almost every engine function
(`obj_dude` → `gDude`, `stat_level` → `critterGetStat`, `win_add` → `windowCreate` …).

## Layout

| Path | What |
|---|---|
| `src/rt/game/*.cc/.h` | Our modules, **copied from fallout1-ce `src/game/`** and kept as close to byte-identical as possible, so a fix can be carried across by copying the file. |
| `src/rt/f1compat.h` | The translation layer: F1 names → F2. Globals, types, Rect fields (`ulx` → `left`), and the functions whose shape changed. |
| `src/rt/f1compat_gen.h` | One inline wrapper per F1 function (F1 signature, F2 call). **Generated** by `.claude/tools/gencompat.js` from a reviewed pairing table — edit the table, not this file. A wrong pairing fails to compile. |
| `src/rt/compat/<F1 path>.h` | One forwarding header per F1 header path the modules include (`game/anim.h`, `plib/gnw/gnw.h` …), so `#include "game/anim.h"` needs no edit. |
| `src/rt/rt_hooks.h` | Engine hooks fallout1-ce ADDED to vanilla files (F1 names). |
| `src/rt/rt_f2_hooks.cc` | The F2 side of those hooks, and **stubs** (original-game behaviour) for every feature not ported yet (RadAway, the side-panel heal/quick slots, floating script lines, the pad inventory). Delete a stub when its feature lands; the linker refuses a duplicate. |
| `src/rt/rt_gconfig_keys.h` | Our cfg keys, copied from fallout1-ce `gconfig.h`. Defaults live in `game_config.cc`. |

Every edit to a vanilla fallout2-ce file is marked `Realtime build` or cites fallout1-ce.

**`src/animation.cc` is fallout1-ce's `anim.cc`, translated whole** (the realtime work in it is 3,000
lines), with Fallout 2's own additions put back and marked "Fallout 2:" (light intensity, the
walk-through portal for doors, encumbrance, goo avoidance in the pathfinder, the woodsman fidget).
The other vanilla files (combat, actions, combat_ai, interface, game_mouse, map, object, tile) got
fallout1-ce's hunks re-applied by hand, keeping Fallout 2's own rules (sfall explosions, F2 critical
tables, its AI packets).

**Module deltas** — the places where a `src/rt/game` file differs from fallout1-ce (re-apply after a resync):
- `gamepad_ui.cc`: `inWorld` needs `combat_rt_enabled()`; `gpui_acquire` prefers the padtest pad (`rt_padtest_controller_index`);
  LT + D-pad squad orders and the LT + A squad command mode (`FALLOUT_RT_GAME == 2`, see "Squad orders" and "Squad
  command" below; the mode's flag exists in both games so the wheels and the hook can stand aside for it).
- `mods_page.cc`: `FALLOUT_RT_GAME` res ini name and `mods_row_visible`.
- `combat_rt.cc`: `rt_plan_attack` passes the target to `combatai_pick_hit_mode` (F2's AI weighs it);
  the corpse that must stay standing is read off the proto (`CRITTER_FLAT`, `rt_settle_corpse_flat` and
  the dying reaction) — fallout1-ce names three F1 pids, which are other critters in F2; the Master's
  boss fight (guards, horde, psychic scream: found by F1 pids and maps) is `#if FALLOUT_RT_GAME == 1`,
  with an empty `rt_horde_reset` for F2; the squad orders read and write COMBAT CONTROL's Distance
  (`aiGetDistance` / `aiSetDistance`, `FALLOUT_RT_GAME == 2`; Fallout 1 reads no Distance and keeps today's fight);
  the squad command (see "Squad command" below: its orders, the `RTSQ` save section and the Healing Powder / Fallout 2
  kits are `FALLOUT_RT_GAME == 2`; Fallout 1's tables stay empty, so none of it runs there).
- `autotest.cc`: the `worldmap_script_jump` step is `#if FALLOUT_RT_GAME == 1` (F1's world map); two
  scenario keywords fallout1-ce does not have yet: `census` (every critter on every floor, traced at the
  start) and `party` on a spawn (`partyMemberAdd`, a real follower); the `squad` timeline step (F2); the `hostile
  <ms> <pid>` timeline step (a fight that starts when the scenario says) and `script <scripts.lst line>` on a spawn
  (F2: a companion's own party script, attached before `party` so the party id holds).

**Fallout 1 Realtime's saves (2026-10-24):** `loadsave.cc` `SaveGameSlot` (vanilla's quick save body with no
boxes, refuses a dead/KO dude), `QuickSaveGame` (START held → SLOT01 "Quick Save"), `AutoSaveLocation`
(a ring in SLOT02..04, "Autosave 1/2/3"), `AutoSaveQuest` (SLOT05), `LoadGameNewest` (newest by the file's
mtime, `compat_file_mtime`); the triggers sit in `main.cc` `mainLoop` after `mapHandleTransition` (location
change, `pipboy_quest_state_hash` change; latched, never in a fight, during a death fall or an autotest);
`mainmenu.cc` has a seventh button, CONTINUE (second row; the seventh plate is a copy of the second laid
in the window buffer). `padtest/continue-autosave.txt` + `play/m-quicksave-temple.txt` test all three.

**The throw (2026-10-24):** `animation.cc` `anim_frame_ticks` — every `ANIM_THROW_ANIM` holds its frames
before the art's action frame ×1.4 and runs the rest ×0.6 (`RT_THROW_WINDUP` / `RT_THROW_RELEASE`); the
release callback counts frames, so the weapon still leaves the hand on the release. `gamepad_ui.cc`
(module delta, `FALLOUT_RT_GAME == 2`): the RB quick throw no longer plays the item's take-out.
While a quick throw is held its chance shows over whom the release throws at, with or without the LT
lock, and it is the THROW's (a spear's secondary, not its thrust): `combat_rt_quick_throw_hit_mode`,
asked by `combat.cc` `_combat_to_hit`; `gamepad_ui_quick_throw_held`. Not in fallout1-ce yet.

**Explosives (2026-10-24, from the Deck):** `item.cc` `explosiveGetDamage` also knows the ARMED pids
(`DYNAMITE_II`, `PLASTIC_EXPLOSIVES_II`, sfall's `activePid`) — the queued blast carries the armed one,
so every real detonation read two uninitialized ints in `queue.cc` `_queue_do_explosion_` (now 0-initialized,
plastic's damage for an unknown pid); a botched timer killed him for 9855647 HP. `gamepad_ui.cc` (module
delta, `FALLOUT_RT_GAME == 2`): an explosive armed from the wheel or a quick slot goes on the ground on
DONE too — vanilla Fallout 2's Use from the hand (`_obj_use_item`, rc 2) puts it down; the inventory's Use
still keeps it (planting). `inventory.cc`: the timer / quantity window owns A for its life
(`MtPadOwner`: suppressed A-click, an input-epoch bump on close) and presses the button under a moved
cursor by its own key (`mt_button_under_cursor`) — the hook's A-as-click used to leak out as a world
walk-click and pull him off the explosive. `screen-explosive-place.txt`, `screen-throw-pct.txt`. The
window and the quick throw are fallout1-ce's code too, so it very likely has the same leak and no throw %
without a lock (not checked there, not ported).

**Scripts under realtime (2026-10-24, fallout1-ce's vanilla-file hunks, carried late):** Cameron's
`terminate_combat` at the end of the Temple of Trials closed the game — vanilla's `_game_user_wants_to_quit
= 1` means "end this fight" only to the turn-based loop realtime never runs. `interpreter_extra.cc`:
`terminate_combat` → `combat_rt_terminate_from_script` (F2's own self part kept), `attack` →
`combat_rt_notify_attack` in a warm fight, `start_gdialog` / `dialogue_system_enter` ask
`combat_rt_under_threat` (+ dialog-request / dialog-refused traces), the guards' weapon amnesty in
`critter_inven_obj`, `reg_anim_func` BEGIN/END and `reg_anim_animate_forever` pass in realtime, the
duplicate-spawn guard in `create_object_sid`, a `script-injure` trace. `scripts.cc`: the critter
heartbeat and the timed events wait only for a fight that is after the player (`combat_rt_under_threat`,
no heartbeat for a fighter), the game clock runs through a realtime fight and keeps pace with travel
speed (`[FLOW/clock]`), and a script's world map / town map request no longer quits in a fight.
`play/m-cameron-fight.txt` (the old exe ends with `loop-ended`), `screen-explosive-fight.txt` (the old
exe never set the bomb off during the fight). `critter.cc` (same day): poison cannot kill during a
script's `game_time_advance` (`critter_set_poison_spares_life`, set around it in `interpreter_extra.cc`),
the RadAway lever is real (cfg `radaway_cancels`, the MODS row is shown now — module delta in
`mods_page.cc`; the `rt_f2_hooks.cc` stub is gone), `rads` / `rad-check` traces and the FATAL+1
radiation level clamp (an out-of-bounds read in vanilla), and `critterDeathFid` — a death frame that
really has art, so no invisible corpses. F2's Sneak already rolls (F1's fix not needed).
Then, from his Deck play the same day: `combat_rt_terminate_from_script` (module change, fallout1-ce
has the same gap) now asks for vanilla's `combat_over` map-script pass (`rt_map_update_on_exit` —
the door out of Cameron's room, `AITemDor.int` at 13528, unlocks itself in `map_update_p_proc` once he
is beaten; it used to wait up to 30 s) and ends the player's side of the fight: lock and intent
dropped, RT dead until let go and 1 s has passed (`rt_script_end_rt_gate`; a mashed RT used to beat
Cameron to death after "You have defeated me"). `interact_prompt.cc` (module change): FLAT generic
scenery gets no A prompt — the Temple's trap plates (`screen-plates.txt`, ARCAVES floor 1).
`game_mouse.cc`: with a pad the loading planet (`MOUSE_CURSOR_WAIT_PLANET`) is centred on the screen
when it comes up (not photographed). `interpreter_extra.cc`: `[FLOW/script-door]` for every script
lock / unlock / open / close, a null object included. Encounters of exactly TWO groups (F2 counts
"Player" as a group, so "A AND B AMBUSH Player" is three and stays peaceful between A and B; F2 never
reads the AND / FIGHTING words): `wmSetupRandomEncounter` sets them against each other, and the
whoHitMe for every critter of both teams came from `_caiTeamCombatInit` in `_combat_begin`, which
realtime never runs — only the one pair named in the request fought. `combat.cc` `_combat` (realtime
branch) now calls `aiTeamCombatInitAtElevation` (`combat_ai.cc`, `[FLOW/team-combat]`) first.
`autotest.cc` (module delta, F2 only): `teamfight <teamA> <teamB>` sets two spawned teams up exactly
that way (`team-fight.txt`: three raiders a side, all six fight).
Ambushers that only watched (his "Sm_Scorpions AND Spore_Plants AMBUSH Player"): the radscorpions attack
by script (ECScorp: wander, attack the dude on sight), but the plants' fight stopped every heartbeat
(`combat_rt_under_threat`) and — the deeper half — F2's `animate_move_obj_to_tile` refuses a walk while
`isInCombat()`, which realtime's warm combat bit made "always": no scripted walk anywhere on the map
for as long as two immobile plants stayed hostile. `scripts.cc`: on a random encounter map (maps.txt
`saved=No`, `!wmMapIsSaveable()`) the bystanders' heartbeat runs through a fight; `interpreter_extra.cc`:
a critter combat_rt is not fighting walks as its script says (fallout1-ce has the same gate). Tested on
his own save (`play/m-encounter-scorpions.txt`, `loadslot 4` = his Deck SLOT04 copied into the test
folder; the old local SLOT04 is `SLOT04.before-deck-2026-10-24`).
A crash only a 640 bar could find (the Deck, holding BACK in a fight): `displayMonitorAddMessage`
(`display_monitor.cc`) and `inventoryRenderItemDescription` (`inventory.cc`, the inventory's
`inven_display_msg`) break lines by writing a `'\0'` into the text they were given and putting the space
back — an access violation on a string literal, and the realtime modules print literals everywhere. Both
work on a 1 KB copy now (`screen-back-in-fight.txt`: the old exe dies with 0xC0000005).

**Torr's pasture (KLAGRAZ), HIS DESIGN, not vanilla** ("Torr bojuje se štíry" + "Štíři jdou po
brahmínech", and on softening it: "ne, ať je šance že se quest posere"): `combat_rt.cc`
`rt_klamath_pasture_tick` (module delta, `FALLOUT_RT_GAME == 2`, beside F1's Master block; F2 pids, the
map by `gMapHeader.name`). A bugman (radscorpion 16777221) fighting nobody goes for the nearest brahmin
or Torr it sees; one after the dude turns to a brahmin or Torr 2 tiles nearer than he is, and one Torr
hits answers Torr ("škorpioni nejdou po něm jen po mně") — each turn at most every 8 s, its combat_rt
target and hostility turned with it (`[FLOW/bugman-turn]`); Torr (16777244) goes for the nearest bugman
within 10 tiles that is fighting somebody, never the dude. A bugman on Torr still runs its own heartbeat
(an NPC-vs-NPC fight is not `combat_rt_critter_fighting` while the dude is out of it), so its script's
attack-on-sight pulls it back to the dude now and then — left as is. Left alone, a bugman kills Torr
(~35-40 s in `m-pasture`). A dead brahmin fails the quest as in vanilla (GVAR 202,
`Klagraz.int` map exit). `[FLOW/bugman-brahmin]`, `[FLOW/torr-defends]`; `play/m-pasture.txt`,
`play/m-pasture-run.txt` on his Deck save (SLOT03; the old local one is `SLOT03.before-deck-2026-10-24`). Not carried
yet: the smaller `scripts.cc` pieces (load serial around the world map, elevator traces, `scr_load`
extent init).

**Fallout 2 only, in vanilla files:** `combat.cc` `aiInfoHas` — realtime sets the combat bit without
`_combat_begin`, so `_aiInfoList` is never allocated, while a loaded game brings back saved `cid`s; every
AI-info accessor now checks the list holds that cid (2026-10-24 crash: a failed load, then the first
attack wrote through the null list). `loadsave.cc` traces each failure point of `_SlotMap2Game`
(`[ERR/slotmap2game] returning N`). Saves from another data version (more global variables, e.g. a
modded install) fail there with `returning 2` — vanilla Fallout 2 CE cannot load them either.

**The realtime save chunk** (2026-10-27, his "Oprav to": a mid-fight save came back with every enemy
neutral, because `map.cc` NULLs each `whoHitMe` on a load): `combat_rt_medic_optout_save/_load` grew
from the medic opt-out list into a tagged chunk — handler 26 of 28 in `loadsave.cc` (`RTFH` the fight:
who fought the dude, by id, map and elevation; `RTFM` the fled-enemy memory; `RTND` the end). Loading
puts each recorded fighter back on the dude (`[FLOW/fight-load]`, `[FLOW/fight-restore]`). Older saves
end after the opt-out list: the reader stops at EOF (`rt_save_read_int` — F2's `fileReadInt32` does not
report it) and keeps the defaults (`[FLOW/rt-chunk] end=optout`). An NPC-vs-NPC fight is not recorded.
`rtchunk-*.txt`.

**Fled enemies stay hostile** (HIS DESIGN 2026-10-26, "Zůstane nepřítel": he fought a robot, ran off
the map, came back and it was neutral): leaving a SAVED map mid-fight (`combat_rt_end_for_map_exit`)
remembers everyone who fought the dude (`[FLOW/fled-remember]`, at most 64, never on a random
encounter map — its critters are erased and the ids reused); back on that map within 24 game hours,
each one that SEES him turns on him again (`[FLOW/fled-reaggro]`), one already fighting him is
dropped (`fled-forget why=hostile`), older ones are forgotten (`fled-expired`). Travels through saves
in `RTFM`. `rtfled-*.txt` (leave and return, expired, random map, save away + load and return).

**Map edges** (2026-10-27, "při hraní F2 vidím za mapu"): Steam's Fallout 2 ships the High Resolution
Patch, and its `f2_res.dat` holds `maps\<map>.edg` for 155 maps — per elevation the tiles at the
left / top / right / bottom extremes of what the player may see. fallout2-ce never read them, so our
853-wide view showed the void past the art. `tile.cc` now loads them (`[FLOW/map-edges]`), clamps the
FINAL view into that area after every centring (`tile_set_center_offset` clamps after its pixel shift,
so the follow camera slides up to the edge), centres it where it is narrower than the window
(ARTEMPLE: 704 px) and paints the rest black. Never off the dude: the clamp stops while he is 48 px
inside the window (an area does not always cover every walkable hex), and no black while he stands
outside it. `mapScroll` (mouse at the screen edge) undoes a step the edge stopped, like a scroll
blocker, and redraws in full a partial one (its strip blit assumes a whole step). f2_res.ini `[MAPS]`
keeps the HRP's switches (`IGNORE_MAP_EDGES`, `EDGE_CLIPPING_ON`). No `.edg` = vanilla behaviour.
`screen-edges-temple.txt`, `screen-edges-pasture.txt` (screenshots; the test folder needs Steam's
`f2_res.dat`).

**AddressSanitizer build** (2026-10-27, fallout1-ce's recipe): `option(ASAN)` in CMakeLists.txt;
configure `build-asan` with `-DASAN=ON -DCMAKE_POLICY_VERSION_MINIMUM=3.5` and the three
`FETCHCONTENT_SOURCE_DIR_*` pointing into `build/_deps`, copy the exe to
`build/Release/fallout2-ce-asan.exe` beside `clang_rt.asan_dynamic-x86_64.dll`, run with
`ASAN_OPTIONS=log_path=asan.log:halt_on_error=1` (a RELATIVE path; the report is `asan.log.<pid>`).
It found the 0xC0000374 crash of every scripted mid-fight map exit in the self-played runs — the
harness's own `at_give` left a given item in the map's object list as well as in the inventory, so the
next map change freed it twice (fixed: `obj_disconnect`, as vanilla `add_obj_to_inven`) — plus two
vanilla tolls it halts on: `fileNameListInit`'s memmove read one entry past the list (`db.cc`), and
`backgroundSoundLoad` strcpy'd its file name onto itself (`game_sound.cc`, fixed as in fallout1-ce).

**The blast ring** (2026-11-08, his „Ano, jednoznačně ano“, then „Ten prstenec bych dal součástí awareness“;
MODS row "Blast ring" Off / Awareness / Always, cfg `blast_ring` 0 / 1 / 2, default 1 = only with the Awareness
perk, `rt_blast_ring_shown`): `combat_rt.cc` `rt_blast_rings_tick` puts a 3 x 3 red dot on every hex at a blast's
radius — the F11 debug ring's look, his pick of seven mock-ups (vanilla's radius, `rt_blast_radius_of`: a grenade
2, a rocket 3) — round where a held quick throw would land (`rt_blast_ring_aim`:
the target, or where a far throw falls short, `combat_forced_throw_tile`'s rule) and round where each grenade or
rocket in the air flies to. Drawn by the tile overlay (`rt_world_overlay`: the rings, then the tracers), repainted
only when a ring comes, goes or moves with the view. The tile in the air is the flight's own: `actions.cc`
`action_rt_launch_flight` reports it (`combat_rt_flight_launched` → `RtInFlight.landTile`), because `rt_reaim`
reads the target after that frame's moves and can be a hex out. `[COMBAT/blast-ring] on|off` traces, harness
`blastring on|off|awareness` and `dude perk <n>`, `screen-blast-ring.txt`, `screen-blast-ring-far.txt`,
`blast-ring-rocket.txt`, `blast-ring-aware-on.txt` / `-off.txt`, `screen-mods-blast.txt` (the row; 25 rows at
480 lines, pitch 12 = the `lineH + 2` floor: the next MODS row needs a new layout). Not in fallout1-ce yet
(module delta in `combat_rt.cc` / `.h`, `mods_page.cc`, `autotest.cc`).

**The exit wheel** (2026-11-10, his „pokud není možné vybrat východ, tak to tlačítko docela ztrácí smysl“):
BACK held, when the exit grids on the dude's floor lead to two places or more, opens a radial wheel of them
(`gamepad_ui.cc` `gpui_run_to_exit` / `gpui_exit_wheel`): "World map" for every grid with a map <= 0, else
that map and floor's name from map.msg (`mapGetName` -- the Pip-Boy automap list's and the save screen's;
`FALLOUT_RT_GAME == 2`), the segments in the order the places lie round the dude on screen. The right stick
(or the left) aims; letting go of BACK, or A, runs for that place's nearest grid; nothing aimed = the
nearest exit; B closes it with no run. One place = the old straight run, and G (keyboard) always is.
`radial_wheel.cc`: the plain text look widens to fit its labels and no longer inherits the last icon
wheel's description band. `[INPUT/exit-wheel]` (the layout), `[INPUT/exit-pick]`, `[ACTION/exit-nav]`
(+ `map=` `elev=`), `screen-exit-wheel-pick.txt`, `screen-exit-wheel-nearest.txt`. Not in fallout1-ce yet
(module delta in `gamepad_ui.cc`, `radial_wheel.cc`).
**How hurt the target is** (2026-11-10, his „to wounded nebo almost dead je taky ve vanille?“; no MODS row): a
second line under the to-hit chance, wherever the chance is drawn (LT lock, held quick throw, mouse), in its
font, colour and outline — the Examine's own word (proto.msg 500-504, Dead … Unhurt, banded by
`proto_instance.cc` `objectExamineHealthBand`, taken out of `_obj_examine_func`), or with Awareness the hit
points as `cur/max` (`game_mouse.cc` `gameMouseToHitHealth`, `gameMouseRenderAccuracy`'s `line2`). A critter
only, never the dude. The to-hit canvas (ACTTOHIT.FRM, 50 x 21) is widened to 140 when it loads (`art.cc`
`artWidenToHitCanvas`, fallout1-ce's trick for its Awareness line), the crosshair where it was. Awareness's
perk card now says so (fallout1-ce's note). `[COMBAT/aim-health]` on a change, `screen-aim-health.txt` /
`screen-aim-health-aware.txt`.

**Squad orders** (2026-11-11, his design "B", „Obojí, nejdřív B“): LT held + a D-pad press orders every companion at
once — Up ATTACK, Down FOLLOW, Left HOLD, Right CHARGE (`gamepad_ui.cc`, module delta, `FALLOUT_RT_GAME == 2`: taken on
the PRESS like LT+Y, spent so no tap or hold follows, only from a D-pad at rest; a D-pad hold opens no screen while LT
is down; given on the safe tick, a click or the error buzz). `combat_rt.cc` `combat_rt_squad_order`: ATTACK makes the
LT-locked enemy the squad's focus (`rt_squad_focus`; no lock or a neutral one is refused), which every companion that
can get at it goes for first (`rt_focus_wanted` — a shot from where it stands or a route, asked once a second) until it
dies, leaves the floor, the map changes or a load comes, another ATTACK replaces it, or the fight is ended (END COMBAT,
a script's `terminate_combat`) or the target joins his side, with a red chevron over its
head (the prompt arrows' 5 x 3, through the tile overlay, `rt_squad_mark_tick`). FOLLOW / HOLD / CHARGE write COMBAT
CONTROL's Distance (Stay close / Stay / Charge) the way the dialogue's panel does: Custom first (`aiSetDisposition` 0,
the packet `aiSave` keeps), then `aiSetDistance` — only a value party.txt allows (Vic will not charge), never on a
generic packet. The nearest companion that took it floats "On it." / "Coming." / "Holding." / "Charging!". And the
fight now keeps to every companion's Distance (it ignored it; module delta, Fallout 1 reads `RT_DIST_NONE` = no rule):
vanilla's regroup distances (5 / 7 / 7 / 7 / never), Stay close fights from within 5 tiles of the dude (pulled back
past 5 + 2, `rt_party_leash`), Stay takes no step for the fight and turns toward what comes (`rt_party_hold_look`,
and `combat_rt_critter_fighting` keeps its party script from walking it), Charge closes to the next tile before it
attacks, Snipe backs off by vanilla's rule (`rt_party_snipe_step`), and a companion whose Distance will not let it
go after its target lets the target go (`rt_party_chase_holds`), so no fight is kept on for nobody. Traces:
`[ACTION/squad-order]`, `[FLOW/squad-focus] on|take|off`, `[FLOW/squad-mark]`, `[FLOW/party-distance]`,
`[FLOW/party-hold]`, `[FLOW/party-drop]`, `[FLOW/party-leash]`, `[FLOW/party-look]`, `[FLOW/party-charge]`,
`[FLOW/party-snipe]`, `follow-step ... limit= why=`. "LT + D-pad Squad orders" is a CONTROLS row (`pipboy.cc`).
`squad-hold.txt`, `squad-follow.txt`, `squad-attack.txt`, `squad-charge.txt`. Not in fallout1-ce (it has no COMBAT
CONTROL).

**Squad command** (2026-11-12, his design "A", „přidej nestřílej a vyléčit mě, změň zbraň, můžu mu i říkat kam má
jít?“) — the behaviours and their API in `combat_rt.cc` / `combat_rt.h` (the LT + A mode that gives the orders lives in
`gamepad_ui.cc`). Its last part, GO — A on free ground: a tile they walked to and held, the POST (Distance "Stay" there
in a fight and out of one, All spread round it, the pathfinder's reach under the cursor) — was TAKEN OUT again on
2026-11-13 at his ask, „dej pryč ovládání a přikazování companions kam mají přesně chodit … nehodí se to do této
action oriented verze“ (it does not fit the action version); Y's healing switch came instead, and GO as it was, code
and tests, lives on the branch `shelf/squad-orders-with-go` (ad1fe7a). One function
gives every order, to All (`who` NULL: the party on the dude's floor, alive and awake) or to one companion:
`combat_rt_squad_command(who, order, target)`; design B's `combat_rt_squad_order` is now `who` = All with ATTACK on the
LT lock. New orders: HOLD FIRE / OPEN FIRE (`rt_squad_holds_fire`: a fight it is in is dropped at the top of
`rt_update_actor`, its idle pick takes nobody, the arbiter skips it, END COMBAT does not wait on it; it still follows,
flees, heals), HEAL ME (the chosen one, or the nearest with supplies, walks to him with `rt_party_step_to_dude` and doses
him with `rt_medic_dose` — the medic beat's dose, past its threshold, cooldown and opt-out; his own heal order Stimpak /
Super Stimpak / Healing Powder, then a kit; "No meds.", "You're fine.", "Can't reach you."), NO HEALING / HEAL AGAIN
(2026-11-13, Y in the mode: the medic opt-out the party dialogue line "(Stop patching me up. Save your supplies.)"
toggles — `combat_rt_medic_set_treats_player` for each of who, the very state the dialogue reads and the realtime
chunk's opt-out list saves; `combat_rt_squad_medic_off(who)`, every one of who refusing, makes Y HEAL AGAIN; "Saving
supplies." / "I'll patch you up."; only the medic beat asks it, HEAL ME goes past it; no Distance, no hold fire, a HEAL
ME on its way goes on; refused why=medic-off while there is no medic beat), WEAPON (`combat_rt_squad_set_weapon`: the
pick is taken out at the next free beat and `rt_ensure_weapon_drawn` keeps to it while it is carried and can fire; dry
or gone, the AI chooses and it says "Out of ammo." once; hopeless by min_to_hit, the AI chooses for the rest of that
fight instead of the flight; COMBAT CONTROL's "Use best weapon" forgets it). ATTACK to one companion is its OWN focus
(`RtSquadOrder.focus`, before the squad's); FOLLOW to one ignores the squad's focus till its next ATTACK; every live
focus has a chevron (`rt_marks[]`). ATTACK / CHARGE lift hold fire, FOLLOW ends own foci — for B's LT + D-pad too. Two
tables keyed by object id: `RtSquadPref` (hold fire, the weapon pid; SAVED as the chunk's `RTSQ` section, before RTPB;
cleared on load / new game) and `RtSquadOrder` (own focus, heal on its way; this map only, `rt_abandon_encounter`). Out
of a fight a heal on its way keeps the party script's follow off by running that heartbeat with `GVAR_PARTY_NO_FOLLOW`
set (`scripts.cc`, `combat_rt_party_follow_held` — vanilla's own switch, every companion's follow asks it first). The
mode draws on top of the world overlay and is told about freed objects (`combat_rt_set_squad_mode_hooks`). Engine
hunks: `scripts.cc` (the GVAR around the heartbeat; the game clock's rate), `game_dialog.cc` ("Use best weapon" forgets
the pick), `animation.cc` (the animation clock, below). Traces: `[ACTION/squad-order] ... who=` (at the END of B's
line), `[FLOW/squad-follow-held]`, `[FLOW/squad-fire] hold|open|drop|refuse`, `[FLOW/squad-heal] order|wait|dose|end`,
`[FLOW/squad-weapon] pin|auto|take|keep|lapse|resume|hopeless`, `[FLOW/squad-focus] own-on|own-off`, `[FLOW/squad-mode]
open|slow|freeze|close`, `[STATE/squad-save|squad-load]`, `[FLOW/medic] ... why=beat|heal-me`, `[FLOW/medic-optout] id=
treats= count=` (NO HEALING / HEAL AGAIN write the dialogue toggle's own line). The harness's `squad <ms> <order> <who>
[arg]` timeline step (`autotest.cc`, F2: attack, follow, hold, charge, holdfire, openfire, healme, noheal, healagain,
weapon, auto) gives an order straight to combat_rt: `squad-holdfire.txt`, `squad-noheal.txt` (the medic beat's own
dose of the dude, held off and back), `squad-heal.txt`, `squad-heal-script.txt` (HEAL ME against the companion's own
party script, `script 50` = DCVic: the follow held), `squad-weapon.txt`, `squad-attack-one.txt`.

**The squad command mode** (`gamepad_ui.cc` `gpui_squad_*`, F2): LT + A in the world (a companion on the floor, no
menu, conversation, inventory, wheel, armed cursor or script scene) opens it on the safe tick; the A is spent (no
interaction, no loot, no use-on hold). WHO starts at All, LB / RB step All → each companion nearest first → All (a
white outline round and a green name over every one chosen). The CURSOR is a point moved by the right stick (the left
one too once it has been back in the centre: it is the walk stick) on the pointer's curve, kept in the view, starting on
the LT-locked enemy or 3 tiles in front of the dude; on a critter's sprite it takes that critter and follows it, else
the tile under it — corner brackets frame which. A gives the order for what is under it (red ATTACK + red brackets on
an enemy, green or grey HEAL ME on the dude, WEAPON on a companion: a radial wheel of what his art can hold plus Auto,
B back to the mode; anything else — free ground too, since GO went — no word, grey brackets, and A buzzes); X hold
fire / open fire and Y no healing / heal me again for who (the legend says which: "X Hold fire" or "X Open fire", "Y
No healing" or "Y Heal me again" — his preview „A Heal me X Hold fire Y No healing B Back“; no Y at all while the
medic beat is off, cfg `medic_enabled=0`, as the dialogue then has no line either); the D-pad design B's four for who
(a compass of four words beside the cursor); B closes. Every order closes it, a refusal buzzes and leaves it open. A
Y held on after its order is spent like X (the close bumps the input epoch; the Y-hold weapon wheel stands aside while
the mode is up): no hand swap, no LT + Y firing mode, no wheel. The marks ride on combat_rt's world overlay
(`gpui_squad_overlay`): the dotted lines, over them the OUTLINES (his pick 2026-11-12 „Obrys postavy“, after „nelíbí se
mi zelené kruhy“ about the green hexes they replaced), on top the brackets. An outline is 1 px round the sprite's
current frame, the engine's own outline shape (every see-through pixel next to a solid one, up, down, left or right),
traced by the overlay from what the tick measured — fid, frame, facing and the place the renderer draws it, the
smooth-movement glide included (`gpui_squad_sprite_of`, as `_obj_render_object` places it) — with the art locked by
fid: the overlay reads no object. Never the object's own `outline` field (vanilla's combat outlines and the LT lock own
it, a fight starting in the slowdown rewrites it, a save keeps it); `objectDrawOutline` is not reused for the same
reason (it takes its colour from that field and writes the object's sx / sy). White, `GPUI_SQUAD_OUTLINE_RGB`: no
outline of the game's is white (team green, enemy red, the yellow of an item / a neutral under the lock / a friendly
the stick points at, vanilla's ochre for a critter out of the line of fire; the badges' gold, the other candidate, sits
by that ochre and that yellow and fades on desert sand). A walk frame or a glide that moves on changes what the tick
measured, so the old outline's box and the new one's are repainted. The words are small MOVE_ON_TOP windows copying
the world under them (the prompt word's way), all erased on every way out (the input epoch is bumped so the buttons it
took are spent; a close after the hold repaints the whole view once, since an order given then cuts a walk while the
smooth-movement glides stand still). **First a slowdown, then the stop** (his pick 2026-11-12: „ten efekt
bude nejdříve zpomalení a pak zastavení … 1 nebo 2“): from the opening the whole world eases from full speed to a
standstill over `RT_SQUAD_SLOW_SECONDS` 1.5 s of real time (smoothstep, floored at 0.05 like the limb picker's slow
motion) and only then holds still as under a radial wheel (`rt_world_frozen`; gamepad_ui suspends the background
processes when `combat_rt_squad_mode_frozen`); the mode answers the pad from its first frame and an order given during
the slowdown is given at once; its close lets everything run at full speed at once. One rate, `combat_rt_world_rate`:
combat_rt_update's dt (after `combat_rt_time_scale`), the ANIMATION CLOCK (`animation.cc` `anim_clock_now`: frame
waits, the smooth-movement glides, stride and cadence holds all read it — it is getTicks() until the first slowdown, so
nothing changes otherwise) and the game clock (`scripts.cc`); a free walk the stick had going coasts to the stop
(`rt_fm_last_angle`). Not folded into `compute_tpf`: an animation takes its ticksPerFrame when it starts (only walk and
run steps re-time), so a swing in the air would keep full speed and one started in the slowdown would stay slow
after it. LT + D-pad (design B) opens no mode and slows nothing. Traces: `[INPUT/squad-press|squad-mode|squad-who|
squad-cursor|squad-a|squad-x|squad-y|squad-dpad|squad-b|squad-weapon-wheel]`, `[FLOW/squad-mode] open why= slow=
probe= | slow f= after= rt=+ probe= moved= | freeze after= rtSlowed= | close why= slowed= frozen= heldMs= rtHeld=
probeHeld=` (the probe is the critter on the move nearest the dude at the opening, measured from a fixed hex),
`[FLOW/world-hold] on|off why=squad`. CONTROLS row "LT + A Squad command" (`pipboy.cc`; Y is the mode's own, no row).
Tests through the real pad: `squad-mode-slow.txt` (the slowdown — its walker a HEAL ME on its way —, the hold, an
order in the slowdown, B's D-pad slowing nothing), `squad-mode-coast.txt` (the dude's walk coasting to the stop),
`squad-mode-none.txt` (no companion: LT + A is A), `squad-mode-pad.txt` (A on free ground, B, X, Y, LB / RB, the
D-pad), `squad-save.txt` + `squad-load.txt` (the RTSQ section round trip; they overwrite the test folder's SLOT01),
`squad-pad-holdfire.txt`, `squad-pad-noheal.txt` (Y with LT still down and held on past the close, shots of the legend
and the two lines), `squad-pad-heal.txt`, `squad-pad-weapon.txt`, `squad-pad-attack.txt` (with shots),
`screen-squad-mode.txt` (shots: free ground — no word, grey brackets —, who, HEAL ME grey and green under his legend
„A Heal me X Hold fire Y No healing B Back“, WEAPON, the wheel, the erase), `screen-squad-walk.txt` (shots: the outline
going with a HEAL ME walk through the slowdown, standing at the hold, gone after a close at the hold and after one in
the slowdown), `screen-squad-y.txt` (shots: his legend with "Y No healing", "Saving supplies.", the legend again with
"Y Heal me again", "I'll patch you up.").

**Resyncing from fallout1-ce** (last: 99edef7 → 0be4c88, v0.1.0, 2026-10-23): the modules by a three-way
merge — `git merge-file -L F2 -L F1-old -L F1-new <ours> <F1 old> <F1 new>`, the two F1 copies taken with
`git -C ../fallout1-ce show <rev>:src/game/<file>` — then `git -C ../fallout1-ce diff <old> <new> -- src/game src/int`
for the vanilla files, carried by hand to their F2 names (the list of F1 → F2 files is in the
commits: anim → animation, combatai → combat_ai, gmouse → game_mouse, intface → interface, inventry →
inventory, protinst → proto_instance, display → display_monitor, gdialog → game_dialog, intextra →
interpreter_extra, editor → character_editor). A new F1 engine name the modules call goes into
`gencompat.js`'s table; a new F1 hook into `rt_hooks.h`; a new cfg key into `rt_gconfig_keys.h`.

⚠️ **The F2 headers in `f1compat.h` are included with `<angle brackets>` on purpose.** MSVC looks for a
quoted header in the directory of every file on the include stack, and `src/rt/compat/game/` holds
forwarders with the same names (`stat.h`, `combat.h` …).

`FALLOUT_RT_GAME=2` (CMake) tells a shared module which game it is built for (`mods_page.cc`: the res
ini name and which MODS rows exist yet).

## Phases

1. ✅ **Foundation** (2026-10-18): translation layer, trace (flight recorder + crash/hang digest),
   480-line internal resolution (`INTERNAL_HEIGHT`, fullscreen = desktop size), VSync + the
   `[ANIM/pace]` meter, MODS page (Preferences → X / M; rows: VSync, Screen and Internal
   resolution), the gamepad layer (cursor, clicks, menu snapping, START/BACK, movie skip, B backs out
   of the game menu and Preferences). Without combat_rt the world is a click screen on the pad:
   right stick = pointer, RT = click.
2. ✅ **Realtime combat — playtest 0.1** (2026-10-19, modules from fallout1-ce 99edef7): `combat_rt`
   wired into the main loop; animation.cc = F1's anim.cc; the attack snapshot/commit pipeline, the
   defender reaction at the commit, projectile flights as sequences of their own, Luck dodge, the
   realtime knockout, the AI's flee/min-to-hit/called-shot accessors, the HUD (non-blocking HP roll,
   AP bulbs from the RT pool, the combat box), smooth movement, the follow-cam offset, bullet tracers,
   the pad's called-shot window, keyboard-as-controller, `--autotest`. Self-played fights pass:
   pistols, knives, grenades, rockets.
3. ✅ **Pad screens** (2026-10-19, uncommitted until played): every screen drives itself the way
   fallout1-ce's does — inventory / loot / barter, dialogue, Pip-Boy (+ the CONTROLS page),
   world map and town map, automap, the character editor (creation and sheet: focus frame, points,
   folders, Age / Gender / Options / name / perk pick, the rate-of-fire card footers), the character
   selector, load / save, the yes-no boxes and file dialogs, the elevator, the main menu, the pause
   menu and Preferences. Fallout 2 only, in the same style: the party member COMBAT CONTROL panel,
   its CUSTOMIZATION panel and the value picker. The quick slots sit in Fallout 2's own side windows
   (LB left, RB right; on a 640-wide screen RB falls back onto the end-turn box), and an aimed shot's
   part is printed in the weapon slot in the worn letters (intrface.lst 40/43/45/290 are the same
   art in both games). Not ported yet: the main menu's CONTINUE, autosaves, the Awareness HP readout.
3b. ✅ **fallout1-ce v0.1.0 combat** (2026-10-23, modules resynced 99edef7 → 0be4c88, uncommitted until
   played): a drug's heal comes in over time in a fight (`item.cc`), scenery / doors / pick-ups cost
   the 3 AP from the realtime pool (`proto_instance.cc`), a quick-slot item on an object costs the pack
   price, a lockpick re-arms after a try, the HP counter's speed follows the size of the change, the
   firing mode is remembered (cfg `fire_mode`), a pile of bodies is looted from one window (arrows reach
   dead bodies within 3 tiles, X takes what fits, "2/5"), ONE CONTROLS page with his list (Deck button
   names on a Deck), NPCs pick the weapon they are skilled with, plasma 1.045, the start-frame bracket
   for the held heal chain, `LoadGameSlot` for the `loadslot` autotest, and trace lines for messages,
   dialogue options, script attacks and floats. Fallout 2 only: the main menu's corner says
   "FALLOUT II 1.02  RT 0.1.0" (`version.cc`), and an `IFACE_BAR_WIDTH` that leaves under 66 px each side
   for the LB / RB slots (Steam's 800 on our 853-wide screen) becomes the widest bar that does (`svga.cc`,
   a multiple of 8: 720), cut out of the Hi-Res 800 art at load — 80 columns out of the middle of its
   message screen (`interface.cc`, `interface_bar_cut`); 640 where nothing wider fits (a Deck's 768).
   `hud-wide-bar.txt` photographs it (with Steam's `f2_res.dat` and `[IFACE]` lines in the test folder).
4. Fallout 2 only: the car, turn-start scripts.
5. ⏳ **Installer** built (2026-10-20), release still to do — see below.

## Installing into Fallout 2

`cmake --build build --config Release --target fallout2-realtime-setup` →
`build/Release/Fallout2-Realtime-Setup.exe`: one exe with the game inside (fallout1-ce's installer,
for Fallout 2). It finds the Steam copy (app 38410, `appmanifest_38410.acf`, any library), or takes a
folder. On Steam it backs up `Fallout2Launcher.exe` as `Fallout2Launcher.original.exe` and puts the
game there, so Steam's **Play** starts Fallout 2 Realtime (Proton on a Deck too); elsewhere it writes
`fallout2-realtime.exe`. ⚠️ **Before anything is written** it copies `fallout2.cfg` and the whole
`<master_patches>\SAVEGAME` into `Fallout2-Realtime-backup\<date time>\` — the game writes both
while it runs (every setting back into the cfg on exit, a quick save into the selected slot, and
Steam Cloud syncs that folder), and they are the original game's own files. A failed backup stops the
install. Uninstall restores the launcher and keeps the backups. CLI for tests:
`Fallout2-Realtime-Setup.exe --target <folder> --install | --uninstall | --detect`.
⚠️ Test with the CLI and a FAKE folder only: the window finds the real Steam copy by itself, and it
once got installed there by a stray Enter — Install is no longer a default button and asks first.
`installer/install-steamdeck.sh` does the same on a Deck (next to it: `fallout2-realtime.exe`).

**The public build** is its own tree (fallout1-ce's switch):
`cmake -S . -B build-release -G "Visual Studio 18 2026" -A x64 -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DFALLOUT_RELEASE=ON`
(the `FETCHCONTENT_SOURCE_DIR_*` of `build/_deps` save the download), then
`cmake --build build-release --config Release --target fallout2-realtime-setup` →
`build-release/Release/fallout2-realtime.exe` + `Fallout2-Realtime-Setup.exe` with it inside. It
records nothing unless `trace_log=1` stands in `fallout2.cfg` (no `rt_walk.log` either), shows no DEV
in the main menu corner, has no F11 ring, and its window title has no " (dev)". ⚠️ Package only from
`build-release` — the dev tree's Setup carries the dev exe (that one is for his own Steam / Deck). The
upstream GitHub Actions are removed: `release.yml` built the plain configuration on every published
release and would have attached a dev build to ours.

The game is identified by the text "Fallout 2 Realtime" in the exe, `RT_GAME_TITLE` in
`rt/rt_version.h` (`RT_WINDOW_TITLE` adds " (dev)" on the dev build); `REALTIME_VERSION` there is the
installer's version. The whole build links the C/C++
runtime statically (top of CMakeLists.txt), so the exe needs no Visual C++ redistributable.

## Self-played fights

fallout1-ce's harness plays a scenario through a virtual pad in a hidden window and reports on the
trace. Fallout 2 scenarios (F2 maps and pids — `.claude/tools/dat2.js` lists the DATs) are in
`.claude/tools/autotest/`; the runner and the report are fallout1-ce's:

```
node ../fallout1-ce/.claude/tools/autotest/run.js .claude/tools/autotest/open-raiders.txt 1 --exe build/Release/fallout2-ce.exe
```

The `screen-*` scenarios press the pad inside the screens (pad timeline) and check `expect` lines:
CONTROLS, the loot pile, take-what-fits, the sheet, the age window, the quit box. ⚠️ A new Fallout 2
dude has no Pip-Boy until the vault-suit movie, so the Pip-Boy ones need `PipBoyAvailableAtGameStart=2`
under `[Misc]` in `build/Release/ddraw.ini` for the run (take it out again). For an A/B, copy an older
exe next to the new one and pass it with `--exe`.

## Testing without a controller

`FALLOUT_RT_PADTEST=<script>` creates a **virtual pad** and presses it on a timeline (see
`rt_padtest_init` in `rt_f2_hooks.cc` for the format; it wins over a real pad plugged in). Scripts and the driver are in
`.claude/tools/padtest/`:

```
SDL_WINDOW_NO_ACTIVATION_WHEN_SHOWN=1   # the window never takes focus; a focus LOSS idles the game
FALLOUT_RT_PADTEST=.claude/tools/padtest/full-menus-to-world.txt
powershell -File .claude/tools/padtest/drive.ps1 -Exe build/Release/fallout2-ce.exe -Dir build/Release -OutPrefix shot_ -Steps "wait:13;shot:prefs"
```

⚠️ `PrintWindow` screenshots of a window that is not active can lag a second or two behind — trust the
trace (`[FLOW]` lines are written at once) over a single picture.

The pad timeline starts when the game initialises SDL, about 1.4 s after `drive.ps1` starts the
process — the trace's `s=` matches the script's seconds, the screenshot waits do not. `screens.txt`
(menus → Preferences → selector → editor → world → pause → sheet → Pip-Boy box → inventory) and
`inv-save.txt` (inventory, save and load screens) walk every screen a fresh game can reach.
