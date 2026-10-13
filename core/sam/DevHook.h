/* SeriousBridge core - the dev hook: console commands from a file, so test tools can drive Serious Sam without the
 * player's mouse and keyboard (and with Sam's window in the background). On when the environment variable given to Init
 * is "1" (tools/dev.py sets it); commands are lines of %TEMP%\<SB_TEMP_DIR>\cmd.txt, taken away as they're read. */
#ifndef SBRIDGE_DEVHOOK_H
#define SBRIDGE_DEVHOOK_H

#include "StdH.h"

namespace devhook {

// Look at the environment (e.g. "SERIOUSBRIDGE_DEV"); TRUE if the hook is on.
BOOL Init(const char *strEnvironment);
BOOL On(void);
// Every frame: run what's waiting (at most four times a second).
void Poll(void);

}; // namespace

#endif
