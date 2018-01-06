// Copyright (C) 2012-2018 Leap Motion, Inc. All rights reserved.
#pragma once
#include "AutoHandle.h"

namespace leap {
namespace ipc {

struct AutoLocker {
  AutoLocker(HANDLE hEvent, HANDLE hMutex) :
    m_hEvent(hEvent),
    m_hMutex(hMutex)
  {
  }

  AutoLocker(AutoLocker&& rhs):
    m_hEvent(rhs.m_hEvent)
  {
    rhs.m_hEvent = nullptr;
  }

  ~AutoLocker(void) { Release(); }

private:
  HANDLE m_hMutex;
  HANDLE m_hEvent;

public:
  // Manual release function
  void Release(void) {
    if(m_hEvent)
      ResetEvent(m_hEvent);
    if(m_hMutex)
      ReleaseMutex(m_hMutex);

    m_hMutex = nullptr;
    m_hEvent = nullptr;
  }

  // No movement off of the stack:
  void operator=(AutoLocker&& rhs) = delete;
};

/// <summary>
/// Represents a named pipe status block, which may be used to wait until a named pipe is created
/// </summary>
class NamedPipeStatusBlockWin
{
public:
  NamedPipeStatusBlockWin(const char* pstrNamespace);

private:
  // Local event, used to block the wait operation
  CAutoHandle<HANDLE> m_hAbandon;

  // Lock, held while altering the state of the event, and also held for as long as the server is
  // online.  This lock is therefore either released or abandoned when the server goes away.
  CAutoHandle<HANDLE> m_hLock;

  // Event, signalled by the server when a pipe is created
  CAutoHandle<HANDLE> m_hEvent;

public:
  AutoLocker Signal(void);

  /// <summary>
  /// Blocks until the desired named pipe has been created, or the specified object is signalled
  /// </summary>
  /// <param name="timeout">The timeout value, or INFINITE to block indefinitely</param>
  /// <returns>
  /// False if aborted, true if there is a possibility that the server end of the link might be online
  /// </returns>
  bool Wait(DWORD timeout = INFINITE);

  /// <summary>
  /// Wakes up any existing wait operations
  /// </summary>
  void Abandon(void);
};

}}
