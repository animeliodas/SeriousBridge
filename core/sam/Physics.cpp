/* SeriousBridge core - physics of Sam's player and monsters as other games' mechanics need it. See Physics.h. */
#include "StdH.h"
#include "SBridge.h"
#include "Physics.h"

#include <deque>

namespace physics {

enum ForceKind { FK_PUSH, FK_SHOVE, FK_TELEPORT };
struct Force {
  ULONG ulEntity;
  INDEX iKind;
  FLOAT3D v;
};

static std::deque<Force> _aForces;
static BOOL _bBroken = FALSE;
static ULONG _ctApplied = 0;

// riding
static BOOL _bRide = FALSE, _bRiding = FALSE;
static FLOAT3D _vSeat(0, 0, 0), _vSeatVelocity(0, 0, 0);
static ULONG _ulRideTick = 0, _ctTicks = 0, _ctRideTicks = 0;
static char _strKind[32] = "";
static Driver _drRide;

static void Broken(const char *strWhat) {
  if (_bBroken) return;
  _bBroken = TRUE;
  CPrintF("^cff0000" SB_TAG "^r %s crashed inside the game; outside forces on Sam's entities are off for this session\n", strWhat);
};

static void Queue(ULONG ulEntity, INDEX iKind, const FLOAT3D &v) {
  if (_aForces.size() >= 256) return;
  Force f = { ulEntity, iKind, v };
  _aForces.push_back(f);
};

void Push(ULONG ulEntity, const FLOAT3D &vSpeed) { Queue(ulEntity, FK_PUSH, vSpeed); };
void Shove(ULONG ulEntity, const FLOAT3D &vDelta) { Queue(ulEntity, FK_SHOVE, vDelta); };
void TeleportTo(ULONG ulEntity, const FLOAT3D &vPosition) { Queue(ulEntity, FK_TELEPORT, vPosition); };

static BOOL PushSEH(CEntity *pen, const FLOAT3D &vSpeed) {
  __try {
    ((CMovableEntity *)pen)->GiveImpulseTranslationAbsolute(vSpeed);
    return TRUE;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return FALSE;
  }
};

static BOOL ShoveSEH(CEntity *pen, const FLOAT3D &vDelta) {
  __try {
    CPlacement3D pl = pen->GetPlacement();
    pl.pl_PositionVector += vDelta;
    pen->SetPlacement(pl);
    return TRUE;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return FALSE;
  }
};

static BOOL PlaceSEH(CEntity *pen, const CPlacement3D &pl, const FLOAT3D *pvVelocity) {
  __try {
    const FLOAT3D vKeep = ((CMovableEntity *)pen)->en_vCurrentTranslationAbsolute;
    pen->Teleport(pl, FALSE);
    CMovableEntity *pmen = (CMovableEntity *)pen;
    pmen->en_vCurrentTranslationAbsolute = pvVelocity != NULL ? *pvVelocity : vKeep;
    pmen->AddToMovers();
    return TRUE;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return FALSE;
  }
};

static BOOL Movable(CEntity *pen) {
  return pen != NULL && !(pen->GetFlags() & ENF_DELETED) && (pen->en_ulPhysicsFlags & EPF_MOVABLE);
};

BOOL PushNow(CEntity *pen, const FLOAT3D &vSpeed) {
  if (_bBroken || !Movable(pen)) return FALSE;
  if (!PushSEH(pen, vSpeed)) { Broken("pushing an entity"); return FALSE; }
  return TRUE;
};

BOOL ShoveNow(CEntity *pen, const FLOAT3D &vDelta) {
  if (_bBroken || pen == NULL || (pen->GetFlags() & ENF_DELETED)) return FALSE;
  if (!ShoveSEH(pen, vDelta)) { Broken("moving an entity"); return FALSE; }
  return TRUE;
};

BOOL PlaceNow(CEntity *pen, const CPlacement3D &pl, const FLOAT3D *pvVelocity) {
  if (_bBroken || !Movable(pen)) return FALSE;
  if (!PlaceSEH(pen, pl, pvVelocity)) { Broken("teleporting an entity"); return FALSE; }
  return TRUE;
};

CEntity *FindEntity(ULONG ulEntity) {
  CWorld *pwo = IWorld::GetWorld();
  if (pwo == NULL) return NULL;
  FOREACHINDYNAMICCONTAINER(pwo->wo_cenEntities, CEntity, iten) {
    CEntity *pen = &*iten;
    if (pen->en_ulID == ulEntity && !(pen->GetFlags() & ENF_DELETED) && !pen->IsPredictor()) return pen;
  }
  return NULL;
};

// --- Driver
Driver::Driver(void) : _vSet(0, 0, 0), _vSent(0, 0, 0), _bHave(FALSE), _fPull(-1.5f) {};

FLOAT3D Driver::From(CMovableEntity *pmen, BOOL bMeasure, BOOL &bBlocked) {
  const FLOAT3D vNow = pmen->en_vCurrentTranslationAbsolute;
  bBlocked = FALSE;
  if (!_bHave) return vNow;
  // nothing hit: the horizontal velocity is what was given, and Sam's physics only added its gravity
  if (Abs(vNow(1) - _vSent(1)) < 0.04f && Abs(vNow(3) - _vSent(3)) < 0.04f) {
    if (bMeasure) _fPull = vNow(2) - _vSent(2);
    return _vSet;
  }
  bBlocked = TRUE;
  return vNow;
};

void Driver::Set(CMovableEntity *pmen, const FLOAT3D &vVelocity, BOOL bAgainstGravity) {
  _vSet = vVelocity;
  _vSent = vVelocity;
  if (bAgainstGravity) _vSent(2) -= _fPull;
  _bHave = TRUE;
  pmen->en_vCurrentTranslationAbsolute = _vSent;
  // Sam's physics steers a player for a moment after a jump (towards the input): not against the mode
  pmen->en_tmJumped = -1000.0f;
  pmen->AddToMovers();
};

void Driver::Release(void) { _bHave = FALSE; };

// --- riding
void Ride(BOOL bOn, const FLOAT3D &vSeat, const FLOAT3D &vVelocity, const char *strKind) {
  _bRide = bOn;
  _vSeat = vSeat;
  _vSeatVelocity = vVelocity;
  _ulRideTick = _ctTicks;
  if (strKind != NULL) {
    strncpy(_strKind, strKind, sizeof(_strKind) - 1);
    _strKind[sizeof(_strKind) - 1] = 0;
  }
};

BOOL Riding(void) { return _bRiding; };
const char *RideKind(void) { return _strKind; };

static void RideStep(CEntity *penPlayer) {
  // the guest's news must be fresh (a vehicle gone quiet, a guest gone: off)
  const BOOL bOn = _bRide && _ctTicks - _ulRideTick <= 10 && Movable(penPlayer) && (penPlayer->GetFlags() & ENF_ALIVE);
  if (!bOn) {
    if (_bRiding) {
      _bRiding = FALSE;
      _drRide.Release();
      CPrintF(SB_TAG " off the %s\n", _strKind);
    }
    return;
  }
  if (!_bRiding) {
    _bRiding = TRUE;
    _ctRideTicks = 0;
    CPrintF(SB_TAG " riding a %s\n", _strKind);
  }
  _ctRideTicks++;

  CMovableEntity *pmen = (CMovableEntity *)penPlayer;
  // where the seat is now (the news is a tick or two old)
  const FLOAT3D vSeat = _vSeat + _vSeatVelocity * (_pTimer->TickQuantum * (FLOAT)(_ctTicks - _ulRideTick));
  const FLOAT3D vPos = penPlayer->GetPlacement().pl_PositionVector;
  const FLOAT3D vOff = vSeat - vPos;
  if (vOff.Length() > 6.0f) {
    // too far to catch up with (got on, or a jump of the guest's): put him there
    CPlacement3D pl = penPlayer->GetPlacement();
    pl.pl_PositionVector = vSeat;
    PlaceNow(penPlayer, pl, &_vSeatVelocity);
    _drRide.Release();
    return;
  }
  // the seat's velocity, and a pull towards it
  FLOAT3D v = _vSeatVelocity + vOff * 10.0f;
  if (v.Length() > 60.0f) v = v * (60.0f / v.Length());
  BOOL bBlocked;
  _drRide.From(pmen, pmen->en_penReference == NULL, bBlocked);
  _drRide.Set(pmen, v, pmen->en_penReference == NULL);
};

void Step(CEntity *penPlayer) {
  _ctTicks++;
  while (!_aForces.empty()) {
    const Force f = _aForces.front();
    _aForces.pop_front();
    if (_bBroken) continue;
    CEntity *pen = FindEntity(f.ulEntity);
    if (pen == NULL) continue;
    BOOL bDone = FALSE;
    if (f.iKind == FK_PUSH) {
      bDone = PushNow(pen, f.v);
    } else if (f.iKind == FK_SHOVE) {
      bDone = ShoveNow(pen, f.v);
    } else {
      CPlacement3D pl = pen->GetPlacement();
      pl.pl_PositionVector = f.v;
      bDone = PlaceNow(pen, pl, NULL);
    }
    if (bDone) _ctApplied++;
  }
  RideStep(penPlayer);
};

void Reset(void) {
  _aForces.clear();
  _bRide = FALSE;
  _bRiding = FALSE;
  _drRide.Release();
};

CTString Status(void) {
  CTString str;
  str.PrintF("physics: %u outside forces applied%s%s", _ctApplied, _bBroken ? " (off: the engine faulted)" : "",
    _bRiding ? (CTString(", riding a ") + _strKind).str_String : "");
  return str;
};

}; // namespace
