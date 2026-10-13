/* SeriousBridge core - what a port tells the core about itself (define before including the core, e.g. in the port's
 * StdH.h), with defaults:
 *   SB_NAME          the port's name: log lines are tagged "[SB_NAME]", files go to %TEMP%\SB_NAME
 *   SB_FRAME_MEMORY  the guest's frame shared memory (wide string), docs/PROTOCOL.md "Frames"
 * Every core .cpp includes the port's precompiled header "StdH.h" first, then this. */
#ifndef SBRIDGE_SBRIDGE_H
#define SBRIDGE_SBRIDGE_H

#ifndef SB_NAME
#define SB_NAME "SeriousBridge"
#endif

#ifndef SB_TAG
#define SB_TAG "[" SB_NAME "]"
#endif

#ifndef SB_TEMP_DIR
#define SB_TEMP_DIR SB_NAME
#endif

#ifndef SB_FRAME_MEMORY
#define SB_FRAME_MEMORY L"Local\SeriousBridgeFrame"
#endif

#endif
