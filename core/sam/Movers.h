/* SeriousBridge core - Serious Sam's moving brushes (doors, lifts, platforms) followed in the guest (docs/PROTOCOL.md). */
#ifndef SBRIDGE_MOVERS_H
#define SBRIDGE_MOVERS_H

#include "StdH.h"
#include <string>

namespace movers {

// A level was exported: forget the old movers; Add() each moving brush in the order of the .mov file.
void Reset(void);
void Add(CEntity *pen);
// Send every mover's placement again soon (reconnect, the level announced again).
void ResendAll(void);
// Every simulation tick: brushes that moved go to the guest ({"t":"movers"}).
void Step(void (*pfnSend)(const std::string &));
CTString Status(void);
// Send mover i an ETrigger (a door or lift starts towards its next marker), as if penCaused did it.
BOOL Trigger(INDEX i, CEntity *penCaused);
// Send any entity an ETrigger from penCaused.
BOOL TriggerEntity(CEntity *pen, CEntity *penCaused);
// Mover i's brush entity in the current world, or NULL.
CEntity *FindMover(INDEX i);
// Dev: every mover and where it was last sent.
void List(void);

}; // namespace

#endif
