#ifndef DISPLAY_MONITOR_H
#define DISPLAY_MONITOR_H

namespace fallout {

int displayMonitorInit();
int displayMonitorReset();
void displayMonitorExit();
void displayMonitorAddMessage(char* string);
void displayMonitorDisable();
void displayMonitorEnable();

// Realtime build: no longer static -- the gamepad layer (src/rt) calls these.
void displayMonitorScrollUpOnMouseDown(int btn, int keyCode);
void displayMonitorScrollDownOnMouseDown(int btn, int keyCode);

} // namespace fallout

#endif /* DISPLAY_MONITOR_H */
