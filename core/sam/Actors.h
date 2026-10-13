/* SeriousBridge core - Serious Sam's monsters in the guest.
 *
 * Living monsters near the player are listed for the guest ten times a second; the guest keeps an invisible stand-in
 * ("proxy") for each, which its hostile mobs hunt and its weapons hit. The hits come back here and are applied to
 * the real monster. */
#ifndef SBRIDGE_ACTORS_H
#define SBRIDGE_ACTORS_H

#include "StdH.h"
#include <string>

namespace actors {

// Every simulation tick: apply the guest's hits, and every other tick send the monsters near the player.
void Step(CEntity *penPlayer, void (*pfnSend)(const std::string &));
// The guest hit monster ulId for fDamage Sam hit points (applied on the next tick).
void OnHit(ULONG ulId, FLOAT fDamage, BOOL bExplosion);
// A guest piston moved monster ulId's stand-in by vDelta metres (the monster moves on the next tick).
void OnPush(ULONG ulId, const FLOAT3D &vDelta);
// The world was replaced under the plugin (a level change: no OnGameStop first): the entities held here belong to the
// old world, which is gone. Their references are dropped without releasing them (releasing would touch freed memory
// and crash the engine); the old entities just stay allocated.
void Abandon(void);
void Reset(void);
CTString Status(void);

}; // namespace

#endif
