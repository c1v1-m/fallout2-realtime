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
- `gamepad_ui.cc`: `inWorld` needs `combat_rt_enabled()`; `gpui_acquire` prefers the padtest pad (`rt_padtest_controller_index`).
- `mods_page.cc`: `FALLOUT_RT_GAME` res ini name and `mods_row_visible`.
- `combat_rt.cc`: `rt_plan_attack` passes the target to `combatai_pick_hit_mode` (F2's AI weighs it);
  the corpse that must stay standing is read off the proto (`CRITTER_FLAT`, `rt_settle_corpse_flat` and
  the dying reaction) — fallout1-ce names three F1 pids, which are other critters in F2; the Master's
  boss fight (guards, horde, psychic scream: found by F1 pids and maps) is `#if FALLOUT_RT_GAME == 1`,
  with an empty `rt_horde_reset` for F2.
- `autotest.cc`: the `worldmap_script_jump` step is `#if FALLOUT_RT_GAME == 1` (F1's world map); two
  scenario keywords fallout1-ce does not have yet: `census` (every critter on every floor, traced at the
  start) and `party` on a spawn (`partyMemberAdd`, a real follower).

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

The game is identified by its window title, `RT_GAME_TITLE` in `rt/rt_version.h` ("Fallout 2
Realtime"); `REALTIME_VERSION` there is the installer's version. The whole build links the C/C++
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
