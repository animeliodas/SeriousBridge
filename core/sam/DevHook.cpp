/* SeriousBridge core - the dev hook. See DevHook.h. */
#include "StdH.h"
#include "SBridge.h"
#include "DevHook.h"

#include <stdio.h>

namespace devhook {

static BOOL _bOn = FALSE;
static DWORD _tmNextPoll = 0;

BOOL Init(const char *strEnvironment) {
  const char *str = getenv(strEnvironment);
  _bOn = str != NULL && str[0] == '1';
  if (_bOn) CPrintF(SB_TAG " dev hook on: commands from %%TEMP%%\\%s\\cmd.txt\n", SB_TEMP_DIR);
  return _bOn;
};

BOOL On(void) { return _bOn; };

void Poll(void) {
  if (!_bOn) return;
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

}; // namespace
