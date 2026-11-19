# Fallout 2 Realtime

**Fallout 2 in realtime for Steam Deck and PC.**

<!-- TODO: a screenshot or a short GIF of a fight goes here -->

Fallout 2 where nobody waits for their turn. You walk with the stick, hold the left trigger to lock onto an enemy and pull the right one to shoot, while the geckos, raiders and mutants come at you at the same time. Action points are still there. They refill as you fight, faster when an enemy is close, and walking in a fight spends them, so kiting does not pay.

It is made for a controller, from the first screen on. It builds on [Fallout 1 Realtime](https://github.com/c1v1-m/fallout1-realtime) and is a modified version of Alexander Batalov's [Fallout 2 Community Edition](https://github.com/alexbatalov/fallout2-ce).

You need your own copy of Fallout 2 from [Steam](https://store.steampowered.com/app/38410) or [GOG](https://www.gog.com/game/fallout_2).

> [!CAUTION]
> **BACK UP YOUR SAVES BEFORE YOU PLAY.** The game uses save slots 1 to 5 for its quick save and autosaves and **overwrites whatever is in them**. If you have your own saves there, save them again into slot 6 or later, or copy the `SAVEGAME` folder somewhere outside the game folder. Steam Cloud syncs that folder, so the overwritten slots also reach your other PCs and your Steam Deck: it is not a backup. See [Saves](#saves).

## What is new

- Realtime combat. Everyone moves and attacks at the same time, and you can shoot and keep moving.
- Every shot flies. Bullets leave tracers in a burst. Rockets and plasma you can see coming and step out of, lasers hit at once.
- Enemies fight smarter. They wait for each other and come round a corner together, cover the door you are holding, shoot from where they can see you, reload out of your sight, jump clear of grenades and step out of your line of fire. They see you only in the open, lose you when you get out of sight and search where they saw you last.
- At most two enemies attack you at the same time, and two each of your followers. The rest wait their turn.
- Your stats decide how fast you swing, aim and reload. Agility brings your action points back faster.
- A stimpack or Healing Powder heals over the next few seconds in a fight, so hits still hurt while it works. Hold the button and you take one after another.
- Aiming at someone shows how hurt they are each time it gets worse. With the Awareness perk you see their hit points, and red dots on the ground show how far a grenade or rocket will blast.
- The whole game plays on a gamepad: fighting, looting, barter, dialogue, the world map, the Pip-Boy, character selection, the character screen and your followers' combat settings.
- With a pad in, the screens show its buttons: the interface bar, the inventory and loot windows, dialogue and barter, the timer on explosives, character selection and the character screen. The action icon carries an A, and arrows on it mark when the D-pad has more to offer.
- Two quick slots beside the interface bar: LB for a stimpack or any drug or tool, RB for a grenade. Radial menus for skills, weapons, ammo and drugs.
- Hold BACK and you run to the nearest exit. Where the exits lead to more than one place, a wheel lets you pick where to go.
- Squad orders on LT + D-pad: your followers go for whoever you aim at, follow you, hold their ground or charge. Point them at someone peaceful and they start the fight. In a fight they keep to the Distance from their combat settings.
- LT + A slows the world to a stop and opens the squad command, which stays open until you press B. Pick who with LB and RB, then send them to a spot to hold it, at anyone not on your side or to heal you. Point at a follower (R3 jumps to the next one) to choose their weapon or have them put on their best armour. X tells them to hold fire, L3 to keep their distance, and Y switches off their healing of you.
- Your followers heal you with their stimpacks or Healing Powder when you are hurt, and run for cover when they are badly hurt.
- The pad rumbles when you are hit, harder for a bigger hit, and when something blows up near you.
- Autosaves when you go to another map or a quest moves on, and a quick save on the controller.
- On the Steam Deck it starts straight from the Steam library, in Game Mode.
- Widescreen at the original scale, smooth movement and VSync.
- A MODS page in the options, where most of this can be switched back toward the original, and where you can speed up travel on foot and on the world map.

## Install

Download the latest zip from [Releases](../../releases) and unzip it.

Before it changes anything, the installer copies `fallout2.cfg` and your whole `SAVEGAME` folder into a new dated folder in `Fallout2-Realtime-backup`, inside the game folder. It does this on every install and every update, and if the copy fails, nothing is installed. To get your saves back, copy `SAVEGAME` from the oldest dated folder into the game's `data` folder.

### Steam on a PC

1. Run `Fallout2-Realtime-Setup.exe`, click **Install** (**Update** if it is already installed) and answer **Yes**. It finds your Steam copy of Fallout 2 by itself.
2. Press **Play** on Fallout 2 in Steam.

### Steam Deck

1. Switch to Desktop Mode and unzip the download.
2. Open the `steamdeck` folder, right-click inside it, choose **Open Terminal Here** and type `bash install-steamdeck.sh`.
3. Go back to Game Mode. On Fallout 2's page open the controller settings, pick **Templates** and choose **Gamepad with Mouse Trackpad**. Without it Steam can turn the buttons into keyboard keys, and the game then never sees a controller.
4. Press **Play** on Fallout 2.

### GOG or a CD copy

Run `Fallout2-Realtime-Setup.exe`, click **Browse...**, choose your Fallout 2 folder, click **Install** and answer **Yes**. Then start `fallout2-realtime.exe` from that folder. It has not been tried on a GOG copy yet.

Steam's Fallout 2 includes the High Resolution Patch, whose files give the wider interface bar and stop the view at the edge of the map. A copy without it still plays, with the original interface bar, and the view can go past the edge of the map.

### Uninstall

Run the installer again and click **Uninstall**. On the Deck it is `bash install-steamdeck.sh uninstall`. Steam's **Verify integrity of game files** brings the original back as well. Your saves, `fallout2.cfg` and the backups stay. **Screen resolution** and **Internal resolution** on the MODS page write to the High Resolution Patch's `f2_res.ini`, which the installer does not back up, so copy that file somewhere safe before you change them and put it back after you uninstall.

## Controls

| Button | What it does |
|---|---|
| Left stick | Walk |
| Right stick | Look, aim |
| D-pad | Next action, scroll |
| Hold Up | Pip-Boy |
| Hold Down | Character |
| Hold Left | Automap |
| Hold Right | End combat |
| LB / RB | Quick slots |
| LT | Aim |
| RT | Shoot |
| L3 | Skills |
| R3 | Called shot |
| A | Confirm, action |
| B | Cancel |
| B hold | Drugs, items |
| X hold | Run |
| Y | Swap hands |
| Y hold | Weapons, ammo |
| LT + X | Reload |
| LT + Y | Firing mode |
| LT + D-pad | Squad orders |
| LT + A | Squad command |
| BACK | Inventory |
| BACK hold | Run to exit |
| START | Pause |
| START hold | Quick save |

The same list is in the game: press Y on the pause menu (START). On a Steam Deck that list calls the buttons L1, R1, L2, R2, VIEW and MENU.

## Saves

Saves from the original Fallout 2 and from Fallout 2 Community Edition should load, so you can carry on where you left off. Saves from a game with a big content mod may not load, the same as in Fallout 2 Community Edition.

The first five slots on the save screen are used by the game itself:

| Slot | What goes there |
|---|---|
| 1 | Quick save, when you hold START |
| 2, 3, 4 | Autosave each time you go to another map, taking turns between the three |
| 5 | Autosave each time a quest moves on |

An autosave waits until the fight is over. Holding START quick-saves whenever you are out in the world, in a fight too, and the fight is still on when you load it.

> [!CAUTION]
> **Your own saves in slots 1 to 5 WILL BE OVERWRITTEN.** Save them again into slot 6 or later before you start, or back up the `SAVEGAME` folder outside the game folder.

If you play on more than one machine, Steam Cloud can deliver a slot only partly when the other machine syncs while an upload is still running. The game checks for that: Continue offers the last complete save instead, and the load screen asks before it loads one. Saves made by this version also carry your companions, so one that a half-copied slot would have lost comes back. With every slot full, Steam's file limit can keep the last slots from syncing at all, so use the lower ones if you switch machines.

## Status

Version 0.1.0, the first public release, so expect rough edges. So far it has been played from a new character through the Temple of Trials, Klamath, the Den and Modoc into Vault City and Gecko. The rest of the game has not been played yet, and the car has not been worked on. If something breaks, please open an [issue](../../issues). It helps a lot if you put `trace_log=1` under `[preferences]` in `fallout2.cfg`, play until it happens again and attach the newest `trace_*.log` from the game folder.

Known:

- On some PCs (seen with an AMD graphics card) the Steam overlay may not show.
- If the game shows a small 4:3 picture with black bars and no LB slot (seen on a Steam Deck), step **Internal resolution** on the MODS page to another value and back to **Original**, then restart the game. It may already say Original, but only a change is saved.
- Fallout 2's own calibres all fly at the same speed for now.
- Some help texts on the MODS page still talk about Fallout 1.
- In a New Reno boxing match the opponent can keep coming during the break between rounds.

## What's next

- The rest of the game after Vault City and Gecko, and polish everywhere it still feels rough.
- New hand-made animations, starting with reloading.
- The Steam overlay on the PCs where it does not show yet.

## Building

Windows, with Visual Studio 2026 and CMake:

```
cmake -S . -B build-release -G "Visual Studio 18 2026" -A x64 -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DFALLOUT_RELEASE=ON
cmake --build build-release --config Release --target fallout2-realtime-setup
```

That builds `fallout2-realtime.exe` and the installer. Leave out `-DFALLOUT_RELEASE=ON` for the development build (`fallout2-ce.exe`), which keeps every diagnostic on.

## Credits

Fallout 2 Realtime is a **modified version** of [Fallout 2 Community Edition](https://github.com/alexbatalov/fallout2-ce) by Alexander Batalov, which re-implements the original engine. Everything changed after upstream commit `e97087b` is the work of c1v1-m, apart from the Fallout Community Edition code it carries (see below).

It builds on [Fallout 1 Realtime](https://github.com/c1v1-m/fallout1-realtime), and through it carries code from [Fallout Community Edition](https://github.com/alexbatalov/fallout1-ce), also by Alexander Batalov.

It uses [SDL2](https://www.libsdl.org) and [zlib](https://zlib.net) under the zlib license, the YUV conversion code in SDL2 under a BSD license, and fpattern by David R Tribble under the MIT License. Their notices are in `NOTICE.txt` in the release zip.

Fallout is a trademark of Bethesda Softworks. This project contains no game data. The High Resolution Patch by Mash, whose files it reads, comes with Steam's copy of the game.

## License

The source code is available under the [Sustainable Use License](LICENSE.md), the same as Fallout 2 Community Edition: free, non-commercial use and distribution only.
