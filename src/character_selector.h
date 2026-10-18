#ifndef CHARACTER_SELECTOR_H
#define CHARACTER_SELECTOR_H

namespace fallout {

// Realtime build: the New Game selection window, -1 while it is closed (read by the gamepad layer).
extern int gCharacterSelectorWindow;

int characterSelectorOpen();

void premadeCharactersInit();
void premadeCharactersExit();

} // namespace fallout

#endif /* CHARACTER_SELECTOR_H */
