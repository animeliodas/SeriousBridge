/* SeriousBridge core - the line to LiveSplit's server. See SpeedrunLink.h. */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <deque>
#include <string>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "SpeedrunLink.h"

#pragma comment(lib, "ws2_32.lib")

namespace lslink {

static CRITICAL_SECTION _cs;
static bool _bInit = false;
static HANDLE _hThread = NULL;
static volatile bool _bRun = false, _bEnabled = true, _bConnected = false;
static std::deque<std::string> _aQueue;
static char _strHost[64] = "127.0.0.1";
static unsigned short _uPort = 16834;
static volatile bool _bAddressChanged = false;
static volatile unsigned long _ctSent = 0;

static SOCKET Connect(void) {
  char strHost[64];
  unsigned short uPort;
  EnterCriticalSection(&_cs);
  strcpy(strHost, _strHost);
  uPort = _uPort;
  _bAddressChanged = false;
  LeaveCriticalSection(&_cs);

  addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  char strPort[16];
  sprintf(strPort, "%u", (unsigned)uPort);
  addrinfo *pai = NULL;
  if (getaddrinfo(strHost, strPort, &hints, &pai) != 0 || pai == NULL) return INVALID_SOCKET;
  SOCKET s = socket(pai->ai_family, pai->ai_socktype, pai->ai_protocol);
  if (s == INVALID_SOCKET) {
    freeaddrinfo(pai);
    return INVALID_SOCKET;
  }
  // connect with a short timeout (nothing listening on 127.0.0.1 is refused at once anyway)
  u_long ulNonBlocking = 1;
  ioctlsocket(s, FIONBIO, &ulNonBlocking);
  connect(s, pai->ai_addr, (int)pai->ai_addrlen);
  freeaddrinfo(pai);
  fd_set fdsWrite, fdsError;
  FD_ZERO(&fdsWrite);
  FD_ZERO(&fdsError);
  FD_SET(s, &fdsWrite);
  FD_SET(s, &fdsError);
  timeval tv = { 1, 0 };
  if (select(0, NULL, &fdsWrite, &fdsError, &tv) <= 0 || FD_ISSET(s, &fdsError) || !FD_ISSET(s, &fdsWrite)) {
    closesocket(s);
    return INVALID_SOCKET;
  }
  ulNonBlocking = 0;
  ioctlsocket(s, FIONBIO, &ulNonBlocking);
  int iNoDelay = 1;
  setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&iNoDelay, sizeof(iNoDelay));
  return s;
};

static DWORD WINAPI Thread(LPVOID) {
  SOCKET s = INVALID_SOCKET;
  DWORD tmNextTry = 0;
  while (_bRun) {
    if (!_bEnabled || _bAddressChanged) {
      if (s != INVALID_SOCKET) {
        closesocket(s);
        s = INVALID_SOCKET;
      }
      _bConnected = false;
      if (!_bEnabled) {
        EnterCriticalSection(&_cs);
        _aQueue.clear();
        LeaveCriticalSection(&_cs);
        Sleep(200);
        continue;
      }
    }
    if (s == INVALID_SOCKET) {
      if ((LONG)(GetTickCount() - tmNextTry) < 0) {
        Sleep(100);
        continue;
      }
      s = Connect();
      tmNextTry = GetTickCount() + 3000;
      _bConnected = s != INVALID_SOCKET;
      // (what was asked while there was no LiveSplit is stale)
      EnterCriticalSection(&_cs);
      _aQueue.clear();
      LeaveCriticalSection(&_cs);
      continue;
    }
    std::string strLine;
    EnterCriticalSection(&_cs);
    if (!_aQueue.empty()) {
      strLine = _aQueue.front();
      _aQueue.pop_front();
    }
    LeaveCriticalSection(&_cs);
    if (strLine.empty()) {
      Sleep(5);
      continue;
    }
    strLine += "\r\n";
    if (send(s, strLine.c_str(), (int)strLine.size(), 0) != (int)strLine.size()) {
      closesocket(s);
      s = INVALID_SOCKET;
      _bConnected = false;
      tmNextTry = GetTickCount() + 3000;
      continue;
    }
    _ctSent++;
  }
  if (s != INVALID_SOCKET) closesocket(s);
  _bConnected = false;
  return 0;
};

void Start(void) {
  if (_bInit) return;
  _bInit = true;
  InitializeCriticalSection(&_cs);
  WSADATA wsa;
  WSAStartup(MAKEWORD(2, 2), &wsa);
  _bRun = true;
  _hThread = CreateThread(NULL, 0, &Thread, NULL, 0, NULL);
};

void Stop(void) {
  if (!_bInit) return;
  _bRun = false;
  if (_hThread != NULL) {
    WaitForSingleObject(_hThread, 3000);
    CloseHandle(_hThread);
    _hThread = NULL;
  }
  WSACleanup();
  DeleteCriticalSection(&_cs);
  _bInit = false;
};

void SetEnabled(bool bEnabled) { _bEnabled = bEnabled; };

void SetAddress(const char *strHostPort) {
  if (!_bInit || strHostPort == NULL) return;
  char strHost[64];
  unsigned uPort = 16834;
  const char *pchColon = strrchr(strHostPort, ':');
  size_t ctHost = pchColon != NULL ? (size_t)(pchColon - strHostPort) : strlen(strHostPort);
  if (ctHost == 0 || ctHost >= sizeof(strHost)) return;
  memcpy(strHost, strHostPort, ctHost);
  strHost[ctHost] = 0;
  if (pchColon != NULL) uPort = (unsigned)atoi(pchColon + 1);
  if (uPort == 0 || uPort > 65535) return;
  EnterCriticalSection(&_cs);
  if (strcmp(strHost, _strHost) != 0 || uPort != _uPort) {
    strcpy(_strHost, strHost);
    _uPort = (unsigned short)uPort;
    _bAddressChanged = true;
  }
  LeaveCriticalSection(&_cs);
};

void Send(const char *strCommand) {
  if (!_bInit || !_bConnected || strCommand == NULL) return;
  EnterCriticalSection(&_cs);
  if (_aQueue.size() < 256) _aQueue.push_back(strCommand);
  LeaveCriticalSection(&_cs);
};

bool Connected(void) { return _bConnected; };
unsigned long Sent(void) { return _ctSent; };

}; // namespace
