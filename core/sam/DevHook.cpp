/* SeriousBridge core - the dev hook. See DevHook.h. */
#include "StdH.h"
#include "SBridge.h"
#include "DevHook.h"
#include "Player.h"

#include <stdio.h>

namespace devhook {

static BOOL _bOn = FALSE;
static DWORD _tmNextPoll = 0;

// --- the watchdog: a game that stops says where
static volatile LONG _ctFrames = 0;
static volatile BOOL _bWatch = FALSE;
static HANDLE _hWatchdog = NULL;
static DWORD _idMainThread = 0;

// One sample of the main thread: its instruction and the return addresses up its frame chain, as module+offset.
static void SampleMainThread(FILE *f, HANDLE hThread) {
  if (SuspendThread(hThread) == (DWORD)-1) return;
  CONTEXT ctx;
  memset(&ctx, 0, sizeof(ctx));
  ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
  DWORD aulAddress[16];
  INDEX ctAddresses = 0;
  if (GetThreadContext(hThread, &ctx)) {
    aulAddress[ctAddresses++] = ctx.Eip;
    DWORD *pFrame = (DWORD *)ctx.Ebp;
    while (ctAddresses < 16 && pFrame != NULL && !IsBadReadPtr(pFrame, 8)) {
      aulAddress[ctAddresses++] = pFrame[1];
      DWORD *pNext = (DWORD *)pFrame[0];
      if (pNext <= pFrame) break;
      pFrame = pNext;
    }
  }
  ResumeThread(hThread);
  for (INDEX i = 0; i < ctAddresses; i++) {
    HMODULE hModule = NULL;
    char strModule[MAX_PATH] = "?";
    if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)aulAddress[i], &hModule)) {
      GetModuleFileNameA(hModule, strModule, MAX_PATH);
    }
    const char *strName = strrchr(strModule, '\\');
    fprintf(f, " %s+0x%lx", strName != NULL ? strName + 1 : strModule, (unsigned long)(aulAddress[i] - (DWORD)hModule));
  }
  fprintf(f, "\n");
};

static DWORD WINAPI Watchdog(LPVOID) {
  LONG ctLast = -1;
  DWORD tmSame = GetTickCount();
  BOOL bWritten = FALSE;
  while (_bWatch) {
    Sleep(500);
    const LONG ct = _ctFrames;
    if (ct != ctLast) {
      ctLast = ct;
      tmSame = GetTickCount();
      bWritten = FALSE;
      continue;
    }
    if (bWritten || GetTickCount() - tmSame < 5000) continue;
    bWritten = TRUE;
    char strTemp[MAX_PATH];
    GetTempPathA(MAX_PATH, strTemp);
    char strFile[MAX_PATH + 64];
    _snprintf(strFile, sizeof(strFile), "%s%s\\hang.txt", strTemp, SB_TEMP_DIR);
    FILE *f = fopen(strFile, "a");
    if (f == NULL) continue;
    fprintf(f, "no frame for 5 s; the main thread (module+offset, innermost first), three samples:\n");
    HANDLE hThread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT, FALSE, _idMainThread);
    if (hThread != NULL) {
      for (INDEX i = 0; i < 3; i++) {
        SampleMainThread(f, hThread);
        Sleep(200);
      }
      CloseHandle(hThread);
    }
    fclose(f);
  }
  return 0;
};

BOOL Init(const char *strEnvironment) {
  const char *str = getenv(strEnvironment);
  _bOn = str != NULL && str[0] == '1';
  if (_bOn) {
    CPrintF(SB_TAG " dev hook on: commands from %%TEMP%%\\%s\\cmd.txt (a frozen game writes hang.txt there)\n", SB_TEMP_DIR);
    // (a plugin starts on the game's main thread)
    _idMainThread = GetCurrentThreadId();
    _bWatch = TRUE;
    _hWatchdog = CreateThread(NULL, 0, &Watchdog, NULL, 0, NULL);
  }
  return _bOn;
};

void Shutdown(void) {
  if (_hWatchdog == NULL) return;
  _bWatch = FALSE;
  WaitForSingleObject(_hWatchdog, 2000);
  CloseHandle(_hWatchdog);
  _hWatchdog = NULL;
};

BOOL On(void) { return _bOn; };

void Poll(void) {
  if (!_bOn) return;
  InterlockedIncrement(&_ctFrames);
  const DWORD tmNow = GetTickCount();
  if ((LONG)(tmNow - _tmNextPoll) < 0) return;
  _tmNextPoll = tmNow + 250;

  char strTemp[MAX_PATH];
  GetTempPathA(MAX_PATH, strTemp);
  CTString strFile = CTString(strTemp) + SB_TEMP_DIR "\\cmd.txt";
  CTString strTaken = CTString(strTemp) + SB_TEMP_DIR "\\cmd.taken";
  // take the file away first, so a writer can't append to what's being read
  DeleteFileA(strTaken.str_String);
  if (!MoveFileA(strFile.str_String, strTaken.str_String)) return;
  FILE *f = fopen(strTaken.str_String, "rb");
  if (f == NULL) return;
  char strLine[1024];
  while (fgets(strLine, sizeof(strLine), f) != NULL) {
    size_t ct = strlen(strLine);
    while (ct > 0 && (strLine[ct - 1] == 10 || strLine[ct - 1] == 13)) strLine[--ct] = 0;
    if (ct == 0) continue;
    CPrintF(SB_TAG " dev> %s\n", strLine);
    _pShell->Execute(strLine);
  }
  fclose(f);
  DeleteFileA(strTaken.str_String);
};

// a level's intro (cinematic) playing: 1/0, -1 on a fault
static INDEX CinematicSEH(CEntity *penPlayer) {
  __try {
    return player::PropEntity(penPlayer, player::PROP_CAMERA) != NULL ? 1 : 0;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return -1;
  }
};

// Netricsa (CGame console state): 0 off, 1 on, 2 turning on, 3 turning off, 4 in background; -1 on a fault
static INDEX ComputerSEH(void) {
  __try {
    return GetGameAPI()->GetCompState();
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return -1;
  }
};

void Where(void) {
  CEntity *pen = player::Local();
  if (pen == NULL) {
    CPrintF(SB_TAG " where: no player\n");
    return;
  }
  const CPlacement3D &pl = pen->GetPlacement();
  const INDEX iCinematic = CinematicSEH(pen);
  const INDEX iComputer = ComputerSEH();
  const FLOAT fPitch = IWorld::GetViewpoint((CPlayerEntity *)pen, FALSE).pl_OrientationAngle(2);
  const INDEX iState = player::PropIndex(pen, player::PROP_STATE, player::PST_STAND);
  CPrintF(SB_TAG " where: %.3f %.3f %.3f heading %.1f pitch %.1f%s%s%s\n", pl.pl_PositionVector(1), pl.pl_PositionVector(2),
    pl.pl_PositionVector(3), (FLOAT)pl.pl_OrientationAngle(1), fPitch, iCinematic == 1 ? " cinematic" : "",
    (iComputer == 1 || iComputer == 2) ? " netricsa" : "", (iState == player::PST_SWIM || iState == player::PST_DIVE) ? " water" : "");
};

void CloseNetricsa(void) {
  const INDEX iComputer = ComputerSEH();
  const BOOL bUp = iComputer == 1 || iComputer == 2;
  if (bUp) GetGameAPI()->SetCompState(3);
  CPrintF(SB_TAG " netricsa: %s\n", bUp ? "closing" : "not open");
};

}; // namespace
