#include "stdafx.h"
#include "NamedPipeStatusBlockWin.h"
#include "DefaultSecurityDescriptor.h"

using namespace leap::ipc;

NamedPipeStatusBlockWin::NamedPipeStatusBlockWin(const char* pstrNamespace):
  m_hAbandon(CreateEvent(nullptr, false, false, nullptr))
{
  std::string lockName = "Global\\lk-";
  lockName += pstrNamespace;
  std::string eventName = "Global\\evt-";
  eventName += pstrNamespace;

  DefaultSecurityDescriptor sacl(EVENT_ALL_ACCESS, EVENT_ALL_ACCESS);
  m_hLock = CreateMutexA(&sacl, false, lockName.c_str());
  m_hEvent = CreateEventA(&sacl, true, false, eventName.c_str());
}

AutoLocker NamedPipeStatusBlockWin::Signal(void) {
  // Obtain the mutex no matter what
  HANDLE hObjs [] = {m_hAbandon, m_hLock};
  switch(WaitForMultipleObjects(2, hObjs, false, INFINITE)) {
  case WAIT_OBJECT_0:
    // Abandoned, thread terminating
    throw std::runtime_error("Wait abandoned");
  case WAIT_OBJECT_0 + 1:
  case WAIT_ABANDONED_0 + 1:
    // Managed to obtain the mutex, continue onwards
    break;
  default:
    throw std::runtime_error("Unexpected error condition while attempting to wait for signalling");
  }

  // We're open for business
  SetEvent(m_hEvent);

  return AutoLocker(m_hLock, m_hEvent);
}

bool NamedPipeStatusBlockWin::Wait(DWORD timeout) {
  HANDLE hObjs[] = {m_hAbandon, m_hEvent};

  for(;;) {
    switch(WaitForMultipleObjects(2, hObjs, false, timeout)) {
    case WAIT_FAILED:
      // Something went wrong, give up
      throw std::runtime_error("Error given while attempting to wait on an event");
    case WAIT_TIMEOUT:
      // Timed out, return control
      return false;
    case WAIT_OBJECT_0:
      // Wait abandoned, terminate here
      return false;
    case WAIT_OBJECT_0 + 1:
      // Event signalled, attempt to obtain the mutex and find out what's going on
      break;
    }

    // Poke mutex to ascertain server state
    switch(WaitForSingleObject(m_hLock, 0)) {
    case WAIT_OBJECT_0:
    case WAIT_ABANDONED_0:
      // Server went away, reset the event and try again
      ResetEvent(m_hEvent);
      ReleaseMutex(m_hLock);
      break;
    case WAIT_TIMEOUT:
      // This means that the server might be online and holding the lock.
      // Possible success--or maybe, the server is crashed, and another client is
      // in the process of resetting the event.
      return true;
    }
  }
}

void NamedPipeStatusBlockWin::Abandon(void) {
  SetEvent(m_hAbandon);
}
