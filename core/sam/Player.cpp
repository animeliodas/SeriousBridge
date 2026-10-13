/* SeriousBridge core - Serious Sam's local player. See Player.h. */
#include "StdH.h"
#include "SBridge.h"
#include "Player.h"

namespace player {

CEntity *Local(void) {
  CPlayerEntities cPlayers;
  IWorld::GetLocalPlayers(cPlayers);
  if (cPlayers.Count() == 0) return NULL;
  CEntity *pen = cPlayers.Pointer(0);
  if (pen == NULL || (pen->GetFlags() & ENF_DELETED)) return NULL;
  return pen;
};

void *Prop(CEntity *pen, ULONG ulID) {
  if (pen == NULL) return NULL;
  CEntityProperty *pep = IWorld::PropertyForId(LibClassHolder(pen), ulID);
  return pep != NULL ? (UBYTE *)pen + pep->ep_slOffset : NULL;
};

FLOAT PropFloat(CEntity *pen, ULONG ulID, FLOAT fDefault) {
  FLOAT *pf = (FLOAT *)Prop(pen, ulID);
  return pf != NULL ? *pf : fDefault;
};

INDEX PropIndex(CEntity *pen, ULONG ulID, INDEX iDefault) {
  INDEX *pi = (INDEX *)Prop(pen, ulID);
  return pi != NULL ? *pi : iDefault;
};

FLOAT PowerupLeft(CEntity *penPlayer, INDEX iPowerup) {
  const FLOAT tmUntil = PropFloat(penPlayer, PROP_POWERUPS + iPowerup, 0.0f);
  return Max(tmUntil - _pTimer->CurrentTick(), 0.0f);
};

// --- the view offset
static ANGLE3D _aOffset(0, 0, 0);
static ANGLE3D _aTurn(0, 0, 0);
static ANGLE3D _aLastRaw(0, 0, 0);
static BOOL _bHaveLastRaw = FALSE;
static BOOL _bResync = TRUE;
static BOOL _bFrozen = FALSE;
static ULONG _ulPlayer = 0;

static void ReadBack(CEntity *pen) {
  ANGLE3D *paLast = (ANGLE3D *)Prop(pen, PROP_LAST_ROTATION);
  ANGLE3D *paLocal = (ANGLE3D *)Prop(pen, PROP_LOCAL_ROTATION);
  _aOffset = (paLast != NULL && paLocal != NULL) ? *paLast - *paLocal : ANGLE3D(0, 0, 0);
  _ulPlayer = pen->en_ulID;
  _bResync = FALSE;
  _bHaveLastRaw = FALSE;
};

void Resync(void) { _bResync = TRUE; };
void FreezeView(BOOL bFrozen) { _bFrozen = bFrozen; };

void TurnView(FLOAT fHeading, FLOAT fPitch) {
  _aTurn(1) += fHeading;
  _aTurn(2) += fPitch;
};

void OnAction(CPlayerAction &pa, BOOL bFresh) {
  CEntity *pen = Local();
  if (pen != NULL && (_bResync || pen->en_ulID != _ulPlayer)) ReadBack(pen);
  if (bFresh) {
    _aOffset += _aTurn;
    _aTurn = ANGLE3D(0, 0, 0);
    if (_bFrozen && _bHaveLastRaw) _aOffset -= pa.pa_aRotation - _aLastRaw;
    _aLastRaw = pa.pa_aRotation;
    _bHaveLastRaw = TRUE;
  }
  pa.pa_aRotation += _aOffset;
};

}; // namespace
