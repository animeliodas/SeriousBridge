/* SeriousBridge core - the line to LiveSplit's server (TCP, one command a line), on its own thread: the game never
 * waits for it. Plain C++ (compiled without the engine's headers: winsock and the engine's Windows headers clash). */
#ifndef SBRIDGE_SPEEDRUNLINK_H
#define SBRIDGE_SPEEDRUNLINK_H

namespace lslink {

// Start the thread; it connects to host:port (tried again every few seconds) while enabled.
void Start(void);
void Stop(void);
void SetEnabled(bool bEnabled);
// "127.0.0.1:16834"
void SetAddress(const char *strHostPort);
// A command ("split"); dropped when not connected.
void Send(const char *strCommand);
bool Connected(void);
// Lines sent since the start (tests).
unsigned long Sent(void);

}; // namespace

#endif
