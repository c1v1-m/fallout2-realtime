#include "version.h"
#include "sfall_config.h"

#include <stdio.h>

// Realtime build: our own version after the game's (fallout1-ce's getverstr).
#include "rt/rt_version.h"

namespace fallout {

// 0x4B4580
void versionGetVersion(char* dest, size_t size)
{
    // SFALL: custom version string.
    char* versionString = nullptr;
    if (configGetString(&gSfallConfig, SFALL_CONFIG_MISC_KEY, SFALL_CONFIG_VERSION_STRING, &versionString)) {
        if (*versionString == '\0') {
            versionString = nullptr;
        }
    }

    // Realtime build: the main menu's corner (and CTRL+V) says which Realtime this is, as Fallout 1
    // Realtime's does -- "FALLOUT II 1.02  RT 0.1.0", DEV on a development build (2026-10-23, "neukazovalo
    // vpravo dole ani RT"). The vanilla numbers in front stay as they were.
    char game[VERSION_MAX];
    snprintf(game, sizeof(game), (versionString != nullptr ? versionString : "FALLOUT II %d.%02d"), VERSION_MAJOR, VERSION_MINOR);
#ifdef FALLOUT_RELEASE
    snprintf(dest, size, "%s  RT %s", game, REALTIME_VERSION);
#else
    snprintf(dest, size, "%s  RT %s DEV", game, REALTIME_VERSION);
#endif
}

} // namespace fallout
