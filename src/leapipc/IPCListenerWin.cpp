// Copyright (C) 2012-2016 Leap Motion, Inc. All rights reserved.
#include "stdafx.h"
#include "IPCListenerWin.h"
#include "DefaultSecurityDescriptor.h"
#include "IPCEndpointWin.h"
#include "NamedPipeWin.h"
#include <autowiring/Autowired.h>

using namespace leap::ipc;

const char* leap::ipc::IPCTestScope(void) { return "session"; }

IPCListenerWin::IPCListenerWin(const char* pstrNamespace) :
  m_namespace(LR"(\\.\pipe\)"),
  m_statusBlock(pstrNamespace)
{
  // Append namespace portion
  for(const char* p = pstrNamespace; *p; p++)
    m_namespace.push_back(*p);
}

IPCListenerWin::~IPCListenerWin()
{
}

IPCListener* IPCListener::New(const char* pstrScope, const char* pstrNamespace) {
  return new IPCListenerWin(pstrNamespace);
}

NamedPipeWin* IPCListenerWin::CreateNamedPipeWrapper(void) const {
  DefaultSecurityDescriptor sacl(GENERIC_READ | GENERIC_WRITE, GENERIC_ALL);

  HANDLE hPipe =
    CreateNamedPipeW(
      m_namespace.c_str(),

      PIPE_ACCESS_DUPLEX |
      FILE_FLAG_OVERLAPPED,

      PIPE_TYPE_MESSAGE |
      PIPE_READMODE_MESSAGE,

      PIPE_UNLIMITED_INSTANCES,
      0x1000,
      0x1000,
      0,
      &sacl
    );
  if(!hPipe)
    throw std::runtime_error("Failed to create a pipe with a DACL");
  return new NamedPipeWin(hPipe);
}

void IPCListenerWin::OnStop(void) {
  // This APC does nothing, it's just here to wake up the main thread
  ::QueueUserAPC(
    [](ULONG_PTR) {},
    this->m_state->m_thisThread.native_handle(),
    101
  );
}

void IPCListenerWin::OnObjectReturned(NamedPipeWin& namedPipe) {
  // Cancel all outstanding IO operations on these pipes:
  NamedPipeWin::s_CancelIoEx(namedPipe.m_hPipe, &namedPipe.m_overlappedRead);
  NamedPipeWin::s_CancelIoEx(namedPipe.m_hPipe, &namedPipe.m_overlappedWrite);

  // Now complete the disconnection
  DisconnectNamedPipe(namedPipe.m_hPipe);

  // Queue an APC, which will cause any alertable waits in IPCListenerWin::Run to return and circle around
  ::QueueUserAPC(
    [](ULONG_PTR) {},
    this->m_state->m_thisThread.native_handle(),
    102
  );
}

void IPCListenerWin::Run(void) {
  // Cached instances that we'll be waiting on:
  std::shared_ptr<NamedPipeWin> namedPipe;

  // Open for business:
  auto signal = m_statusBlock.Signal();

  while(!ShouldStop()) {
    if(!namedPipe) {
      namedPipe = std::shared_ptr<NamedPipeWin>(CreateNamedPipeWrapper(), [this] (NamedPipeWin* pipe) {
        this->OnObjectReturned(*pipe);
        delete pipe;
      });

      // Asynchronously start the connect operation:
      if (!ConnectNamedPipe(namedPipe->m_hPipe, &namedPipe->m_overlappedRead) &&
          GetLastError() == ERROR_PIPE_CONNECTED)
        // Client is already connected, so signal an event
        SetEvent(namedPipe->m_overlappedRead.hEvent);
    }

    // Delay until someone connects:
    DWORD rs = WaitForSingleObjectEx(namedPipe->m_overlappedRead.hEvent, INFINITE, true);

    switch(rs) {
    case WAIT_ABANDONED:
      break;
    case WAIT_IO_COMPLETION:
      // Something woke us up--just circle around, if we need to quit then so be it
      break;
    case WAIT_OBJECT_0:
      // Handoff
      onClientConnected(std::make_shared<IPCEndpointWin>(namedPipe));
      namedPipe.reset();
      break;
    default:
      throw std::runtime_error("Unexpected return value from WaitForSingleObjectEx");
    }
  }
}
