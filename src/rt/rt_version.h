#ifndef FALLOUT_RT_VERSION_H_
#define FALLOUT_RT_VERSION_H_

// Fallout 2 Realtime's own version and name, apart from the vanilla VERSION_* in version.h (those
// go into saved games and stay untouched).
//
// RT_GAME_TITLE is the window title, and it doubles as the installer's MARKER: every build of the
// game carries the string, Steam's Fallout2Launcher.exe does not, so installer/setup.cc tells "our
// exe in the launcher's place" from "Steam's launcher" by looking for it. Keep the two in step
// (and install-steamdeck.sh's MARKER / VERSION).
#define REALTIME_VERSION "0.1.0"
#define RT_GAME_TITLE "Fallout 2 Realtime"

#endif /* FALLOUT_RT_VERSION_H_ */
