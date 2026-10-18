#ifndef FALLOUT_MAINMENU_H_
#define FALLOUT_MAINMENU_H_

namespace fallout {

typedef enum MainMenuOption {
    MAIN_MENU_INTRO,
    MAIN_MENU_NEW_GAME,
    MAIN_MENU_LOAD_GAME,
    MAIN_MENU_SCREENSAVER,
    MAIN_MENU_TIMEOUT,
    MAIN_MENU_CREDITS,
    MAIN_MENU_QUOTES,
    MAIN_MENU_EXIT,
    MAIN_MENU_SELFRUN,
    MAIN_MENU_OPTIONS,
} MainMenuOption;

// Realtime build: true while the main menu runs (read by the gamepad layer).
extern bool _in_main_menu;

int mainMenuWindowInit();
void mainMenuWindowFree();
void mainMenuWindowHide(bool animate);
void mainMenuWindowUnhide(bool animate);
int _main_menu_is_enabled();
int mainMenuWindowHandleEvents();

} // namespace fallout

#endif /* FALLOUT_MAINMENU_H_ */
