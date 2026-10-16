/* SeriousBridge core - a speedrun timer for Serious Sam, and LiveSplit kept in step with it. See Speedrun.h. */
#include "StdH.h"
#include "SBridge.h"
#include "Speedrun.h"
#include "SpeedrunLink.h"
#include "Player.h"

namespace speedrun {

INDEX sr_bShowTimer = TRUE;
INDEX sr_bLiveSplit = TRUE;
INDEX sr_iStartOn = 1; // 0: the new game itself; 1: the player's first control
CTString sr_strLiveSplit = "127.0.0.1:16834";

static const ULONG PROP_END_OF_LEVEL = (401UL << 8) | 96; // Player.es m_bEndOfLevel: the level's stats recorded
static const ULONG PROP_END_OF_GAME = (401UL << 8) | 97;  // m_bEndOfGame

static BOOL _bInit = FALSE;
static RunState _rs = RS_IDLE;
static LARGE_INTEGER _liFreq, _liStart, _liEnd;
static DOUBLE _tmGame = 0.0;
static TIME _tmLastTick = -1.0f;
static INDEX _ctSplits = 0;
static BOOL _bLevelSplit = FALSE, _bWasEndOfLevel = FALSE, _bWasEndOfGame = FALSE;
static DWORD _tmNextSync = 0;
static DOUBLE _tmLastSplitRTA = 0.0, _tmLastSplitIGT = 0.0;
static INDEX _bLastLiveSplit = -1;
static CTString _strLastAddress;
static CSymbolPtr _symStartMode;

static DOUBLE Seconds(const LARGE_INTEGER &liFrom, const LARGE_INTEGER &liTo) {
  return (DOUBLE)(liTo.QuadPart - liFrom.QuadPart) / (DOUBLE)_liFreq.QuadPart;
};

CTString FormatTime(DOUBLE tm) {
  if (tm < 0.0) tm = 0.0;
  const INDEX iCenti = (INDEX)floor(tm * 100.0 + 0.0001);
  const INDEX iH = iCenti / 360000, iM = (iCenti / 6000) % 60, iS = (iCenti / 100) % 60, iC = iCenti % 100;
  CTString str;
  if (iH > 0) str.PrintF("%d:%02d:%02d.%02d", iH, iM, iS, iC);
  else str.PrintF("%d:%02d.%02d", iM, iS, iC);
  return str;
};

// LiveSplit's TimeSpan: h:mm:ss.fff
static void SendGameTime(void) {
  const INDEX iMilli = (INDEX)floor(_tmGame * 1000.0 + 0.0001);
  char str[64];
  _snprintf(str, sizeof(str), "setgametime %d:%02d:%02d.%03d", iMilli / 3600000, (iMilli / 60000) % 60, (iMilli / 1000) % 60, iMilli % 1000);
  lslink::Send(str);
};

DOUBLE RealTime(void) {
  if (_rs == RS_IDLE || _rs == RS_ARMED) return 0.0;
  LARGE_INTEGER li = _liEnd;
  if (_rs == RS_RUNNING) QueryPerformanceCounter(&li);
  return Seconds(_liStart, li);
};

DOUBLE GameTime(void) { return (_rs == RS_RUNNING || _rs == RS_DONE) ? _tmGame : 0.0; };
RunState State(void) { return _rs; };
INDEX Splits(void) { return _ctSplits; };
BOOL LiveSplitConnected(void) { return lslink::Connected(); };

static void StartRun(void) {
  _rs = RS_RUNNING;
  QueryPerformanceCounter(&_liStart);
  _tmGame = 0.0;
  _ctSplits = 0;
  _tmLastSplitRTA = _tmLastSplitIGT = 0.0;
  _bLevelSplit = FALSE;
  lslink::Send("reset");
  lslink::Send("starttimer");
  lslink::Send("initgametime");
  lslink::Send("pausegametime");
  SendGameTime();
  CPrintF(SB_TAG " run started\n");
};

static void Split(const char *strWhy) {
  _ctSplits++;
  _bLevelSplit = TRUE;
  _tmLastSplitRTA = RealTime();
  _tmLastSplitIGT = _tmGame;
  SendGameTime();
  lslink::Send("split");
  CPrintF(SB_TAG " split %d (%s): RTA %s, IGT %s\n", _ctSplits, strWhy, FormatTime(_tmLastSplitRTA).str_String, FormatTime(_tmGame).str_String);
};

static void Finish(void) {
  Split("the end of the game");
  QueryPerformanceCounter(&_liEnd);
  _rs = RS_DONE;
  CPrintF(SB_TAG " run finished: RTA %s, IGT %s\n", FormatTime(RealTime()).str_String, FormatTime(_tmGame).str_String);
};

static BOOL InControl(CEntity *pen) {
  if (!(pen->GetFlags() & ENF_ALIVE)) return FALSE;
  if (player::PropEntity(pen, player::PROP_CAMERA) != NULL) return FALSE;
  if (player::PropEntity(pen, player::PROP_ACTION_MARKER) != NULL) return FALSE;
  return GetGameAPI()->GetCompState() == 0;
};

void Init(void) {
  if (_bInit) return;
  _bInit = TRUE;
  QueryPerformanceFrequency(&_liFreq);
  _pShell->DeclareSymbol("persistent user INDEX sr_bShowTimer;", &sr_bShowTimer);
  _pShell->DeclareSymbol("persistent user INDEX sr_bLiveSplit;", &sr_bLiveSplit);
  _pShell->DeclareSymbol("persistent user INDEX sr_iStartOn;", &sr_iStartOn);
  _pShell->DeclareSymbol("persistent user CTString sr_strLiveSplit;", &sr_strLiveSplit);
  lslink::Start();
};

void Shutdown(void) {
  if (!_bInit) return;
  lslink::Stop();
  _bInit = FALSE;
};

void OnGameStart(void) {
  if (!_bInit) return;
  // (the menu's intro plays as a "flyover" game; demos aren't runs)
  if (!_symStartMode.Exists()) _symStartMode.Find("gam_iStartMode");
  const BOOL bFlyover = _symStartMode.Exists() && _symStartMode.GetIndex() < 0;
  if (bFlyover || _pNetwork->IsPlayingDemo()) return;
  if (_rs == RS_RUNNING) CPrintF(SB_TAG " run reset (a new game)\n");
  _rs = RS_ARMED;
  _tmGame = 0.0;
  _ctSplits = 0;
  _bWasEndOfLevel = _bWasEndOfGame = FALSE;
  _bLevelSplit = FALSE;
  _tmLastTick = -1.0f;
  lslink::Send("reset");
  if (sr_iStartOn == 0) StartRun();
};

void OnChangeLevel(void) {
  if (_rs == RS_RUNNING && !_bLevelSplit) Split("the level changed");
  _bLevelSplit = FALSE;
  _bWasEndOfLevel = FALSE;
};

void OnGameLoad(void) {
  // a loaded save: the game time goes on from where it was, the level's split state from the save
  _tmLastTick = -1.0f;
};

void Step(CEntity *pen) {
  if (!_bInit) return;
  const TIME tmNow = _pTimer->CurrentTick();
  if (tmNow == _tmLastTick) return; // (OnStep comes with every game stream block)
  const FLOAT tmDelta = tmNow - _tmLastTick;
  const BOOL bFirst = _tmLastTick < 0.0f;
  _tmLastTick = tmNow;
  if (pen == NULL) return;

  if (_rs == RS_ARMED) {
    if (InControl(pen)) StartRun();
    return;
  }
  if (_rs != RS_RUNNING) return;
  // a tick of game time (a level's or a save's start jumps the clock: one tick then)
  const FLOAT tmTick = _pTimer->TickQuantum;
  _tmGame += (!bFirst && tmDelta > 0.0f && tmDelta <= tmTick * 3.0f) ? tmDelta : tmTick;

  const BOOL bEndOfLevel = player::PropIndex(pen, PROP_END_OF_LEVEL, FALSE) != 0;
  const BOOL bEndOfGame = player::PropIndex(pen, PROP_END_OF_GAME, FALSE) != 0;
  if (bEndOfGame && !_bWasEndOfGame) {
    Finish();
  } else if (bEndOfLevel && !_bWasEndOfLevel && !_bLevelSplit) {
    Split("the level's end");
  }
  _bWasEndOfLevel = bEndOfLevel;
  _bWasEndOfGame = bEndOfGame;
};

void Frame(void) {
  if (!_bInit) return;
  if (sr_bLiveSplit != _bLastLiveSplit) {
    _bLastLiveSplit = sr_bLiveSplit;
    lslink::SetEnabled(sr_bLiveSplit != 0);
  }
  if (sr_strLiveSplit != _strLastAddress) {
    _strLastAddress = sr_strLiveSplit;
    lslink::SetAddress(sr_strLiveSplit.str_String);
  }
  // LiveSplit's game time follows the IGT ten times a second
  const DWORD tmNow = GetTickCount();
  if (_rs == RS_RUNNING && (LONG)(tmNow - _tmNextSync) >= 0) {
    _tmNextSync = tmNow + 100;
    SendGameTime();
  }
};

void Draw(CDrawPort *pdp) {
  if (!_bInit || !sr_bShowTimer || pdp == NULL || _rs == RS_IDLE) return;
  const FLOAT fScale = pdp->GetHeight() / 720.0f;
  pdp->SetFont(_pfdConsoleFont);
  pdp->SetTextAspect(1.0f);
  pdp->SetTextScaling(fScale * 1.5f);
  const COLOR col = _rs == RS_DONE ? 0x60FF60E0 : _rs == RS_ARMED ? 0xA0A0A0C0 : 0xFFFFFFE0;
  CTString strRTA = CTString("RTA ") + (_rs == RS_ARMED ? CTString("ready") : FormatTime(RealTime()));
  CTString strIGT = CTString("IGT ") + (_rs == RS_ARMED ? CTString("ready") : FormatTime(_tmGame));
  if (_ctSplits > 0) strIGT += CTString(0, "  #%d", _ctSplits);
  if (lslink::Connected()) strRTA += "  LS";
  const PIX pixX = pdp->GetWidth() - (PIX)(190 * fScale);
  const PIX pixY = (PIX)(46 * fScale);
  const PIX pixLine = (PIX)(16 * fScale);
  pdp->PutText(strRTA, pixX + 1, pixY + 1, 0x000000A0);
  pdp->PutText(strRTA, pixX, pixY, col);
  pdp->PutText(strIGT, pixX + 1, pixY + pixLine + 1, 0x000000A0);
  pdp->PutText(strIGT, pixX, pixY + pixLine, col);
};

CTString Status(void) {
  static const char *astrState[] = { "idle", "ready (starts on control)", "running", "finished" };
  CTString str;
  str.PrintF("run %s; RTA %s, IGT %s, %d splits; LiveSplit %s (%s), %lu commands sent", astrState[_rs],
    FormatTime(RealTime()).str_String, FormatTime(GameTime()).str_String, _ctSplits,
    !sr_bLiveSplit ? "off" : lslink::Connected() ? "connected" : "not connected", sr_strLiveSplit.str_String, lslink::Sent());
  return str;
};

}; // namespace
