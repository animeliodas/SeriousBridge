/* SeriousBridge core - the dev hook: console commands from a file, so test tools can drive Serious Sam without the
 * player's mouse and keyboard (and with Sam's window in the background). On when the environment variable given to Init
 * is "1" (tools/dev.py sets it); commands are lines of %TEMP%\<SB_TEMP_DIR>\cmd.txt, taken away as they're read. */
#ifndef SBRIDGE_DEVHOOK_H
#define SBRIDGE_DEVHOOK_H

#include "StdH.h"

namespace devhook {

// Look at the environment (e.g. "SERIOUSBRIDGE_DEV"); TRUE if the hook is on. From the plugin's startup (the game's
// main thread): with the hook on, a watchdog thread writes %TEMP%\<SB_TEMP_DIR>\hang.txt if frames stop for 5 s - where
// the main thread is, as module+offset (tools that drive the game should give up then, and kill it).
BOOL Init(const char *strEnvironment);
// The plugin's shutdown: the watchdog stops.
void Shutdown(void);
BOOL On(void);
// Every frame: run what's waiting (at most four times a second).
void Poll(void);

// For a port's test console commands (oracles start a level, get past its intro and Netricsa, then test):
// prints SB_TAG " where: x y z heading h pitch p[ cinematic][ netricsa][ water]" for the local player
void Where(void);
// Netricsa (the game's computer) closed if it's up; prints SB_TAG " netricsa: closing" or "not open"
void CloseNetricsa(void);

}; // namespace

#endif
