/* SeriousBridge core - another game's movement rules for Serious Sam's player (Half-Life's in SeriousHL).
 *
 * Sam's own movement doesn't let another's add to it: on the ground the engine pulls the velocity towards what the keys
 * ask with 100 m/s² (60 to slow down) every tick, after a jump it steers him for half a second, it jumps only after
 * 0.25 s without vertical movement, and a walking player stops at ledges - anything a port adds on top is eaten, and
 * bunny hops die on landing. Taken over, every tick right before the engine moves him (CMovableEntity::PreMoving,
 * hooked through Classics Patch's function patches - Player.es has applied the tick's action by then):
 *   - Sam's acceleration and deceleration are zero, his jump is out of the desired movement, he never stops at ledges;
 *   - the port's rules get what the world really did since the last tick (walls, floors, ceilings, explosions, pushes,
 *     teleports, a moving floor) and the keys, and give the velocity to move with; the engine still does the collision
 *     (sliding, steps);
 *   - the level's gravity, which the engine still adds, is taken back out of it (the port has its own);
 *   - Sam's impact damage is off (fast movers hit walls; a port's rules have their own fall damage).
 * Sam keeps the movement while the player isn't alive, swims or dives (unless the port takes the water too:
 * TakeWater), plays an auto action or a cinematic (a level's intro or ending), flies by a cheat, or the port's rules say
 * so; it comes back to the port by itself after that. In water the engine's own fluid physics (buoyancy by the player's
 * density, the pull to a sinking or floating speed while swimming) is taken back out as the gravity is.
 *
 * A port: movement::Init(rules) in its startup; movement::Track(player::Local()) every tick (OnStep); movement::OnAction
 * for every player action (OnPlayerAction). One port at a time may take the player's movement.
 */
#ifndef SBRIDGE_MOVEMENT_H
#define SBRIDGE_MOVEMENT_H

#include "StdH.h"

namespace movement {

// The movement keys of the latest player action.
struct Keys {
  FLOAT fForward; // -1..1: forward / back (Sam's walk key gives about half)
  FLOAT fSide;    // -1..1: right / left
  BOOL bJump;     // the jump key held
  BOOL bDuck;     // the crouch key held
};

struct State {
  FLOAT3D vVelocity; // m/s: what the port gave last tick, with what the world did to it since
  FLOAT3D vUp;       // against the level's gravity here
  FLOAT fGravity;    // the level's gravity here, m/s² (Sam's usual: 30; 0 in zero gravity)
  BOOL bGround;      // standing on something (the engine's reference)
  FLOAT3D vNormal;   // ...its plane (pointing up from it)
  FLOAT fFriction;   // ...its surface's friction (1; less on ice)
  FLOAT fLanding;    // landed since the last tick: how fast he was falling, m/s (else 0)
  BOOL bDucked;      // Sam crouches (his collision box is the crouching one)
  Keys keys;         // the latest action's
  FLOAT fTurn;       // degrees the view turned since the last tick (heading, left positive)
  FLOAT fView;       // (the view's heading relative to the body, for Wish)
  FLOAT fPitch;      // the view's pitch, degrees (up positive)
  FLOAT fWater;      // metres above the bottom of his collision box the water reaches (0: none there; WATER_DEEP: deeper
                     // than tested) - Sam's boxes differ: standing it's from his feet up, swimming around his middle
  FLOAT fHeight;     // his collision box's height, metres
};
static const FLOAT WATER_DEEP = 1000.0f;

// What the port's rules give for the tick.
struct Result {
  FLOAT3D vMove;     // m/s, the average over the tick: Sam moves him by vMove * tick (and the engine's collision)
  FLOAT3D vEnd;      // m/s at the tick's end: the next tick's State.vVelocity starts from it
  BOOL bJumped;      // he left the ground by a jump (Sam's ground contact is dropped, the jump sound plays)
  BOOL bLifted;      // he left the ground otherwise (dropped, no sound)
  BOOL bHoldCrouch;  // let Sam crouch in the air however long he's been off the ground (Player.es: only the first 0.5 s)
};

// The rules: FALSE - not now, Sam moves him this tick by his own.
typedef BOOL (*Rules)(CEntity *penPlayer, const State &st, Result &res);

// Plugin startup: hook the engine and run the rules for the tracked player every tick. FALSE if the engine couldn't be
// hooked (then Sam moves him by his own rules).
BOOL Init(Rules pRules);
// Every tick (OnStep): the player whose movement the rules take (the local one; NULL: none).
void Track(CEntity *penPlayer);
// A port's look at every action Player.es is about to apply for the tracked player: it may change it (take its own
// buttons out - SeriousHL's weapons take fire and reload). Needs the applied action hooked (Init).
typedef void (*ActionFilter)(CEntity *penPlayer, CPlayerAction &pa);
void SetActionFilter(ActionFilter pFilter);
// Every player action (from the network): jump and duck held together are put into it (ACTION_JUMP_WITH_DUCK: Sam's
// controls add the two into one number, so both held came out as neither); its keys, if the applied action
// (CPlayer::ApplyAction in the Entities library) couldn't be hooked.
void OnAction(CPlayerAction &pa, BOOL bFresh);
// In an action: the jump held while ducking (the action's up-down is the duck's). Taken out before Player.es sees it.
static const ULONG ACTION_JUMP_WITH_DUCK = 1UL << 31;
// Keys relative to the view (fForward ahead, fSide to the right), as the view was at fAt of the tick (0 its start, 1
// now): an absolute horizontal vector as long as the input.
FLOAT3D Wish(CEntity *penPlayer, const State &st, FLOAT fForward, FLOAT fSide, FLOAT fAt);
// The port's rules move him in water too (default: Sam swims by his own).
void TakeWater(BOOL bTake);
// Whether a point is inside a swimmable sector the player is in (as Half-Life's PM_CheckWater tests points).
BOOL PointInWater(CEntity *penPlayer, const FLOAT3D &vPoint);
// How high above the bottom of his collision box the water reaches, up to fReach metres (WATER_DEEP beyond; 0 if
// it's not at the bottom).
FLOAT WaterAbove(CEntity *penPlayer, FLOAT fReach);
// Start from scratch (a level ends: a new player entity comes).
void Release(void);
BOOL Active(void);
// The player Track() was given (NULL: none).
CEntity *Tracked(void);
CTString Status(void);

}; // namespace

#endif
