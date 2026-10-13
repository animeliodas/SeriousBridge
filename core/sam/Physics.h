/* SeriousBridge core - physics of Serious Sam's player and monsters as other games' mechanics need it.
 *
 * Sam's own physics (Player.es, CMovableEntity) stays in charge; what a port adds goes through here:
 *   - outside forces on any movable entity (the player, a monster, a projectile), by entity ID so a world change in
 *     between can't leave a dangling pointer: an impulse (an explosion's knockback, a sword's), a displacement (a
 *     piston), a teleport (an ender pearl, a portal) - applied on the next simulation tick;
 *   - a velocity Driver: a port's movement mode (Minecraft's elytra, its water, a vehicle) sets the player's velocity
 *     every tick. Sam's gravity can't be switched off for the player (Player.es puts the flag back and levels the
 *     view), so the driver measures what Sam's physics adds in a tick when nothing was hit, and gives that much less;
 *   - riding: a vehicle a guest game owns (a Minecraft boat) carries the player - his position follows its seat.
 * docs/PHYSICS.md and docs/TRANSPORT.md have the model, units and ownership. */
#ifndef SBRIDGE_PHYSICS_H
#define SBRIDGE_PHYSICS_H

#include "StdH.h"

namespace physics {

// --- outside forces, applied on the next tick (Step); Sam coordinates, metres and m/s
void Push(ULONG ulEntity, const FLOAT3D &vSpeed);
void Shove(ULONG ulEntity, const FLOAT3D &vDelta);
void TeleportTo(ULONG ulEntity, const FLOAT3D &vPosition);

// --- the same now (guarded: FALSE if the engine faulted, and outside forces are off from then on)
BOOL PushNow(CEntity *pen, const FLOAT3D &vSpeed);
BOOL ShoveNow(CEntity *pen, const FLOAT3D &vDelta);
// To a placement, with this velocity (NULL: as it was).
BOOL PlaceNow(CEntity *pen, const CPlacement3D &pl, const FLOAT3D *pvVelocity);

// The world's movable entity with this ID, or NULL.
CEntity *FindEntity(ULONG ulEntity);

// --- velocity control against Sam's gravity, for one movement mode
class Driver {
public:
  Driver(void);
  // The velocity to go on from (m/s): the one set last tick if Sam's physics only added its gravity (then measured, if
  // bMeasure: not on the ground), else what it left (bBlocked: a wall, the ground stopped it).
  FLOAT3D From(CMovableEntity *pmen, BOOL bMeasure, BOOL &bBlocked);
  // Move at this velocity (m/s) this tick; bAgainstGravity: give Sam's measured pull that much less.
  void Set(CMovableEntity *pmen, const FLOAT3D &vVelocity, BOOL bAgainstGravity);
  // The mode let go (the next From starts from what Sam's physics has).
  void Release(void);
  // Sam's pull on the player per tick (m/s, negative), as measured.
  FLOAT Pull(void) const { return _fPull; };
private:
  FLOAT3D _vSet, _vSent;
  BOOL _bHave;
  FLOAT _fPull;
};

// --- riding a guest's vehicle
// From the guest, every tick it ticks: on or off, where the seat is (the rider's feet) and how fast it goes.
void Ride(BOOL bOn, const FLOAT3D &vSeat, const FLOAT3D &vVelocity, const char *strKind);
BOOL Riding(void);
const char *RideKind(void);

// Every simulation tick (IProcessingEvents::OnStep): forces, and the rider held at his seat.
void Step(CEntity *penPlayer);
// The world went (OnGameStop, a level change): forget everything queued.
void Reset(void);
CTString Status(void);

}; // namespace

#endif
