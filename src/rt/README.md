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
| `src/rt/rt_f2_hooks.cc` | The F2 side of those hooks, and **stubs** (original-game behaviour) for every feature not ported yet — including the whole `combat_rt` API. Delete a stub when its feature lands; the linker refuses a duplicate. |
| `src/rt/rt_gconfig_keys.h` | Our cfg keys, copied from fallout1-ce `gconfig.h`. Defaults live in `game_config.cc`. |

Every edit to a vanilla fallout2-ce file is marked `Realtime build`.

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
2. Realtime combat: `combat_rt.cc` + its hooks in combat / animation / actions / combat_ai.
3. Pad screens: wheels, inventory, quick slots, Pip-Boy, dialogue, world map, character editor.
4. Fallout 2 only: party members' combat control, the car, turn-start scripts.
5. Installer (Steam app 38410 runs `fallout2launcher.exe`; GOG: next to `fallout2.exe`) and release.

## Testing without a controller

`FALLOUT_RT_PADTEST=<script>` creates a **virtual pad** and presses it on a timeline (see
`autotest_controller_index` in `rt_f2_hooks.cc` for the format). Scripts and the driver are in
`.claude/tools/padtest/`:

```
SDL_WINDOW_NO_ACTIVATION_WHEN_SHOWN=1   # the window never takes focus; a focus LOSS idles the game
FALLOUT_RT_PADTEST=.claude/tools/padtest/full-menus-to-world.txt
powershell -File .claude/tools/padtest/drive.ps1 -Exe build/Release/fallout2-ce.exe -Dir build/Release -OutPrefix shot_ -Steps "wait:13;shot:prefs"
```

⚠️ `PrintWindow` screenshots of a window that is not active can lag a second or two behind — trust the
trace (`[FLOW]` lines are written at once) over a single picture.
