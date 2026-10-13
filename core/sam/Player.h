/* SeriousBridge core - Serious Sam's local player: finding him, reading his Player.es state, and turning his view.
 *
 * Player.es (class 401) is the same in TFE and TSE for what's here; the powerups and serious bombs are TSE only (their
 * properties don't exist in TFE: reads give the default). */
#ifndef SBRIDGE_PLAYER_H
#define SBRIDGE_PLAYER_H

#include "StdH.h"

namespace player {

// Player.es properties (class 401)
static const ULONG PROP_ARMOR = (401UL << 8) | 3;
static const ULONG PROP_TOPHEALTH = (401UL << 8) | 6;       // m_fMaxHealth: the health a level starts with (100; 200 on easy)
static const ULONG PROP_VIEWSTATE = (401UL << 8) | 20;      // m_iViewState: PVT_3RDPERSONVIEW = 3
static const ULONG PROP_LAST_ROTATION = (401UL << 8) | 151; // what the last applied action carried
static const ULONG PROP_LOCAL_ROTATION = (401UL << 8) | 154; // the rotation actions are composed from (the mouse adds to it)
static const ULONG PROP_POWERUPS = (401UL << 8) | 160;      // m_tmInvisibility, ..Invulnerability, ..SeriousDamage, ..SeriousSpeed
static const ULONG PROP_SERIOUSBOMBS = (401UL << 8) | 190;  // m_iSeriousBombCount

enum Powerup { PU_INVISIBILITY = 0, PU_INVULNERABILITY = 1, PU_SERIOUSDAMAGE = 2, PU_SERIOUSSPEED = 3 };
static const INDEX VIEW_THIRD_PERSON = 3;

// The local player (the first one), NULL between levels or while the world loads.
CEntity *Local(void);

// A property of an entity (any class) by its id, or NULL if the class has none such.
void *Prop(CEntity *pen, ULONG ulID);
FLOAT PropFloat(CEntity *pen, ULONG ulID, FLOAT fDefault);
INDEX PropIndex(CEntity *pen, ULONG ulID, INDEX iDefault);
// Seconds left of a TSE powerup, 0 when none.
FLOAT PowerupLeft(CEntity *penPlayer, INDEX iPowerup);

// The player's view, turned by a port (a test hook, or a guest screen open over Sam that takes the mouse). Actions
// carry the accumulated rotation and Player.es applies their difference, so the offset stays added from then on; a new
// player entity (a level, a new game, a loaded save) has its own, read back from it, or its first action would jump the
// view by the difference.
// Every player action (IPacketEvents::OnPlayerAction): turn by what was asked, keep the view still while frozen.
void OnAction(CPlayerAction &pa, BOOL bFresh);
// Turn the view by this much on the next fresh action (degrees: heading left, pitch up).
void TurnView(FLOAT fHeading, FLOAT fPitch);
// While frozen, what the mouse turns is taken back out (it moves something else, e.g. a guest's cursor).
void FreezeView(BOOL bFrozen);
// A world was loaded or a save restored: read the offset back from the player entity.
void Resync(void);

}; // namespace

#endif
