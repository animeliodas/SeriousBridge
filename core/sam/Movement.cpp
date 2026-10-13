/* SeriousBridge core - another game's movement rules for Serious Sam's player. See Movement.h. */
#include "StdH.h"
#include "SBridge.h"
#include "Movement.h"
#include "Player.h"

#include <Extras/XGizmo/Objects/StructPtr.h>

namespace movement {

// Sam's running speeds (Player.es plr_fSpeedForward, plr_fSpeedSide: what his keys give, m/s)
static const FLOAT SAM_RUN = 10.0f;

static Rules _pRules = NULL;
static BOOL _bHooked = FALSE, _bBroken = FALSE;
static CEntity *_penTracked = NULL;
static ULONG _ulTracked = 0;

static BOOL _bTaken = FALSE; // the last tick was the port's
static BOOL _bHave = FALSE;  // ...and its result is there to go on from
static TIME _tmEnd = -1.0f;  // the tick it was for
static FLOAT3D _vExpected(0, 0, 0), _vEnd(0, 0, 0);
static BOOL _bGround = TRUE;
static ANGLE _aLastHeading = 0.0f;
static BOOL _bLastHeading = FALSE;
static ULONG _ctTicks = 0, _ctGiven = 0;
static const char *_strWhy = "not started";

// the latest player action
static Keys _keys = { 0.0f, 0.0f, FALSE, FALSE };

static void GiveToSam(const char *strWhy) {
  _strWhy = strWhy;
  _bHave = FALSE;
  _bLastHeading = FALSE;
  if (!_bTaken) return;
  _bTaken = FALSE;
  _ctGiven++;
};

// As Player.es decides the player swims.
static BOOL InWater(CMovableEntity *pmen) {
  CWorld *pwo = pmen->GetWorld();
  const CContentType &ctUp = pwo->wo_actContentTypes[pmen->en_iUpContent];
  const CContentType &ctDn = pwo->wo_actContentTypes[pmen->en_iDnContent];
  return ((ctUp.ct_ulFlags & CTF_SWIMABLE) && pmen->en_fImmersionFactor <= 0.99f)
      || ((ctDn.ct_ulFlags & CTF_SWIMABLE) && pmen->en_fImmersionFactor >= 0.5f);
};

// Why Sam keeps the movement now, NULL if he doesn't.
static const char *SamsNow(CEntity *pen) {
  if (!(pen->GetFlags() & ENF_ALIVE)) return "dead";
  if (!(pen->en_ulPhysicsFlags & EPF_TRANSLATEDBYGRAVITY)) return "flying";
  if (player::PropEntity(pen, player::PROP_ACTION_MARKER) != NULL) return "auto action";
  if (player::PropEntity(pen, player::PROP_CAMERA) != NULL) return "cinematic";
  const INDEX iState = player::PropIndex(pen, player::PROP_STATE, player::PST_STAND);
  if (iState == player::PST_SWIM || iState == player::PST_DIVE || InWater((CMovableEntity *)pen)) return "water";
  return NULL;
};

// The view's heading as it will be after this tick: the body's, the head's (relative to it), and what Player.es has
// asked the body to turn by this tick (it turns the body to the view while moving).
static FLOAT ViewRelative(CMovableEntity *pmen) {
  return ((CPlayerEntity *)pmen)->en_plViewpoint.pl_OrientationAngle(1) + pmen->en_aDesiredRotationRelative(1) * _pTimer->TickQuantum;
};

static void FillState(CMovableEntity *pmen, State &st) {
  const TIME tmNow = _pTimer->CurrentTick();
  const BOOL bGoOn = _bTaken && _bHave && tmNow - _tmEnd < _pTimer->TickQuantum * 1.5f;

  // what the world did: the difference between how he really moved and how he was meant to
  const FLOAT3D vNow = pmen->en_vCurrentTranslationAbsolute;
  st.vVelocity = bGoOn ? _vEnd + (vNow - _vExpected) : vNow;
  st.vUp = -pmen->en_vGravityDir;
  st.fGravity = pmen->en_fGravityA;
  st.bGround = pmen->en_penReference.ep_pen != NULL && pmen->en_vReferencePlane.Length() > 0.5f;
  st.vNormal = st.bGround ? pmen->en_vReferencePlane : st.vUp;
  st.fFriction = 1.0f;
  if (st.bGround) {
    CWorld *pwo = pmen->GetWorld();
    const INDEX iSurface = pmen->en_iReferenceSurface;
    if (iSurface >= 0 && iSurface < pwo->wo_astSurfaceTypes.Count()) st.fFriction = pwo->wo_astSurfaceTypes[iSurface].st_fFriction;
  }
  st.fLanding = (bGoOn && st.bGround && !_bGround) ? Max(-(_vEnd % st.vUp), 0.0f) : 0.0f;
  st.bDucked = player::PropIndex(pmen, player::PROP_STATE, player::PST_STAND) == player::PST_CROUCH;
  st.keys = _keys;
  st.fView = ViewRelative(pmen);
  const ANGLE aHeading = pmen->GetPlacement().pl_OrientationAngle(1) + st.fView;
  st.fTurn = _bLastHeading ? NormalizeAngle(aHeading - _aLastHeading) : 0.0f;
  _aLastHeading = aHeading;
  _bLastHeading = TRUE;
};

// Sam's movement out of the way for this tick, and the rules' velocity in.
static void Apply(CMovableEntity *pmen, const Result &res) {
  const FLOAT tmTick = _pTimer->TickQuantum;
  // no acceleration of Sam's (Player.es set it from the action), no jump of his, no stopping at ledges
  pmen->en_fAcceleration = 0.0f;
  pmen->en_fDeceleration = 0.0f;
  if (pmen->en_vDesiredTranslationRelative(2) > 0.0f) pmen->en_vDesiredTranslationRelative(2) = 0.0f;
  pmen->en_fStepDnHeight = -1.0f;
  if (res.bJumped) {
    // off the ground, as Sam's own jump leaves it (Player.es plays the jump sound next tick)
    pmen->en_penReference = NULL;
    pmen->en_pbpoStandOn = NULL;
    pmen->en_tmJumped = _pTimer->CurrentTick();
  }
  CEntity *penReference = pmen->en_penReference.ep_pen;

  // Sam's gravity this tick (the rest of PreMoving): in the air, full - unless he'd fall faster than the level's top
  // falling speed, then none; on the ground, only what pulls along its plane (none on a flat floor); on something
  // moving up or down, full. It goes out of what he's given, so his speed is the rules'.
  const FLOAT3D vGDir = pmen->en_vGravityDir;
  const FLOAT fGA = pmen->en_fGravityA;
  FLOAT3D vBase(0, 0, 0);
  BOOL bFull = penReference == NULL;
  if (penReference != NULL && (penReference->en_ulPhysicsFlags & EPF_MOVABLE)) {
    vBase = ((CMovableEntity *)penReference)->en_vCurrentTranslationAbsolute;
    if (Abs(vBase % vGDir) > 0.0f) bFull = TRUE;
  }
  FLOAT3D vGravity(0, 0, 0);
  if (fGA > 0.0f) {
    if (bFull) {
      if ((res.vMove % vGDir) < pmen->en_fGravityV) vGravity = vGDir * (fGA * tmTick);
    } else {
      const FLOAT3D &vN = pmen->en_vReferencePlane;
      const FLOAT3D vAlong = vGDir - vN * (vGDir % vN);
      if (vAlong.Length() > 0.001f) vGravity = vAlong * (fGA * tmTick);
    }
  }
  pmen->en_vCurrentTranslationAbsolute = res.vMove - vGravity;
  // Sam's impact damage (speed lost to a wall or the floor) is not the rules'
  pmen->en_ulPhysicsFlags |= EPF_NOIMPACTTHISTICK;
  pmen->AddToMovers();

  // (how he'd go: with what he stands on carrying him, the engine adds that itself)
  _vExpected = res.vMove + vBase;
  _vEnd = res.vEnd;
  _bHave = TRUE;
  _tmEnd = _pTimer->CurrentTick();
  _ctTicks++;
  _bGround = res.bJumped ? FALSE : (penReference != NULL);
};

static void Tick(CMovableEntity *pmen) {
  const char *strWhy = SamsNow(pmen);
  if (strWhy != NULL) {
    GiveToSam(strWhy);
    return;
  }
  if (_bTaken && _bHave && _pTimer->CurrentTick() == _tmEnd) return; // (done this tick)
  State st;
  FillState(pmen, st);
  Result res = { FLOAT3D(0, 0, 0), FLOAT3D(0, 0, 0), FALSE };
  if (!_pRules(pmen, st, res)) {
    GiveToSam("the port's rules are off");
    return;
  }
  if (!_bTaken) {
    _bTaken = TRUE;
    _bHave = FALSE;
  }
  Apply(pmen, res);
};

static void Broken(void) {
  if (_bBroken) return;
  _bBroken = TRUE;
  _bTaken = FALSE;
  CPrintF("^cff0000" SB_TAG "^r the player's movement rules crashed inside the game; Sam moves him by his own for this session\n");
};

static BOOL TickSEH(CMovableEntity *pmen) {
  __try {
    Tick(pmen);
    return TRUE;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return FALSE;
  }
};

// --- the engine hook
typedef void (CMovableEntity::*CPreMoving)(void);
static CPreMoving _pPreMoving = NULL;

class CMovableEntityHook : public CMovableEntity {
public:
  void P_PreMoving(void) {
    // (the tracked player, still the same entity)
    if (this == _penTracked && en_ulID == _ulTracked && !_bBroken && !IsPredictor()) {
      if (!TickSEH(this)) Broken();
    }
    (this->*_pPreMoving)();
  };
};

BOOL Init(Rules pRules) {
  _pRules = pRules;
  StructPtr ptr(ClassicsCore_GetEngineSymbol("?PreMoving@CMovableEntity@@UAEXXZ"));
  if (ptr.iAddress == NULL) {
    CPrintF("^cff0000" SB_TAG "^r CMovableEntity::PreMoving isn't in the engine: Sam keeps his own movement\n");
    return FALSE;
  }
  _pPreMoving = ptr(CPreMoving());
  HFuncPatch hPatch = CreatePatch(_pPreMoving, &CMovableEntityHook::P_PreMoving, SB_NAME ": CMovableEntity::PreMoving()");
  _bHooked = hPatch != NULL && hPatch->IsPatched();
  if (!_bHooked) CPrintF("^cff0000" SB_TAG "^r couldn't hook CMovableEntity::PreMoving: Sam keeps his own movement\n");
  return _bHooked;
};

void Track(CEntity *pen) {
  if (pen != NULL && ((pen->GetFlags() & ENF_DELETED) || !(pen->en_ulPhysicsFlags & EPF_MOVABLE))) pen = NULL;
  if (pen != _penTracked || (pen != NULL && pen->en_ulID != _ulTracked)) GiveToSam(pen != NULL ? "a new player" : "no player");
  _penTracked = pen;
  _ulTracked = pen != NULL ? pen->en_ulID : 0;
};

void OnAction(const CPlayerAction &pa, BOOL bFresh) {
  if (!bFresh) return;
  const FLOAT3D &vKeys = pa.pa_vTranslation;
  _keys.fForward = Clamp(-vKeys(3) / SAM_RUN, -1.0f, 1.0f);
  _keys.fSide = Clamp(vKeys(1) / SAM_RUN, -1.0f, 1.0f);
  _keys.bJump = vKeys(2) > 0.1f;
  _keys.bDuck = vKeys(2) < -0.01f;
};

FLOAT3D Wish(CEntity *pen, const State &st, FLOAT fForward, FLOAT fSide, FLOAT fAt) {
  // as Player.es turns its keys by the view (relative to the body), and the engine the body's to absolute
  FLOATmatrix3D mView;
  MakeRotationMatrixFast(mView, ANGLE3D(st.fView - st.fTurn * (1.0f - fAt), 0.0f, 0.0f));
  const FLOAT3D vRelative = FLOAT3D(fSide, 0.0f, -fForward) * mView;
  return vRelative * pen->GetRotationMatrix();
};

void Release(void) {
  GiveToSam("released");
  _penTracked = NULL;
  _ulTracked = 0;
};

BOOL Active(void) { return _bTaken; };

CTString Status(void) {
  CTString str;
  if (!_bHooked) {
    str = "movement Sam's (the engine isn't hooked)";
  } else if (_bTaken) {
    str.PrintF("movement taken (%u ticks; given back to Sam %u times)", _ctTicks, _ctGiven);
  } else {
    str.PrintF("movement Sam's (%s; %u ticks taken, given back %u times)", _strWhy, _ctTicks, _ctGiven);
  }
  return str;
};

}; // namespace
