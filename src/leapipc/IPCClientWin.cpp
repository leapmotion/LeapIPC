// Copyright (C) 2012-2016 Leap Motion, Inc. All rights reserved.
#include "stdafx.h"
#include "IPCClientWin.h"
#include "IPCEndpointWin.h"
#include "NamedPipeWin.h"
#include <autowiring/autowiring.h>

using namespace leap::ipc;

IPCClientWin::IPCClientWin(const char* pstrNamespace):
  m_namespace(LR"(\\.\pipe\)"),
  m_statusBlock(pstrNamespace),
  m_hStatus(nullptr)
{
  for(auto* p = pstrNamespace; *p; p++)
    m_namespace.push_back(*p);

  std::string semaphoreName(R"(\\Global\)");
  semaphoreName += pstrNamespace;
  m_hStatus = CreateSemaphore(nullptr, 0, 0, semaphoreName.c_str());
}

IPCClient* IPCClient::New(const char* pstrScope, const char* pstrNamespace) {
  return new IPCClientWin(pstrNamespace);
}

void IPCClientWin::OnStop(bool graceful) {
  // Close status block unconditionally
  m_statusBlock.Abandon();
}

static std::shared_ptr<IPCEndpoint> ConfigureNamedPipe(const wchar_t* pwchNamespace) {
  // Open the pipe handle:
  HANDLE hPipe = CreateFileW(
    pwchNamespace,
    GENERIC_READ | GENERIC_WRITE,
    0,
    nullptr,
    OPEN_EXISTING,
    FILE_FLAG_OVERLAPPED,
    nullptr
  );
  if(hPipe == INVALID_HANDLE_VALUE) {
    switch(GetLastError()) {
    case ERROR_ACCESS_DENIED:
      throw std::runtime_error("Failed to open pipe, access control list incompatible with this client's request");
    }

    // Failed to open the handle the a reason of nonexistence, circle around
    return nullptr;
  }

  // Unconditionally bind into a pipe handle
  auto pipe = std::make_shared<NamedPipeWin>(hPipe);

  // Make sure we set our mode to message mode or we won't be able to segment
  DWORD mode = PIPE_READMODE_MESSAGE;
  if(!SetNamedPipeHandleState(hPipe, &mode, nullptr, nullptr))
    // Something went terribly wrong, circle around and try again
    return nullptr;

  // Create a client endpoint around this type:
  return std::make_shared<IPCEndpointWin>(pipe);
}

std::shared_ptr<IPCEndpoint> IPCClientWin::Connect(void) {
  while(!ShouldStop()) {
    // Wait for the status block to come back:
    if(!m_statusBlock.Wait(INFINITE))
      break;

    // Now wait for the pipe to become available:
    WaitNamedPipeW(m_namespace.c_str(), NMPWAIT_WAIT_FOREVER);

    // Try to configure, end here if we succeed
    auto rv = ConfigureNamedPipe(m_namespace.c_str());
    if (rv)
      return rv;
  }

  // Told to quit, terminate here
  return nullptr;
}

std::shared_ptr<IPCEndpoint> IPCClientWin::Connect(std::chrono::microseconds dt) {
  // Subtract one microsecond in order to ensure this loop always runs at least once
  for(
    auto now = std::chrono::profiling_clock::now() - std::chrono::microseconds(1),
    endTime = now + dt + std::chrono::microseconds(1);

    !ShouldStop() && now < endTime;
    now = std::chrono::profiling_clock::now()
  ) {
    auto maxTime = [=] {
      std::chrono::milliseconds dt = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - std::chrono::profiling_clock::now()
      );
      return
        std::chrono::milliseconds::zero() < dt ?
        (DWORD)dt.count() :
        0UL;
    };

    // Wait for the status block to come back:
    if (!m_statusBlock.Wait(maxTime()))
      return nullptr;

    // Now wait for the pipe to become available, up until the maximum allowed time, which changes on each iteration
    if (!WaitNamedPipeW(m_namespace.c_str(), maxTime()))
      continue;

    // Try to configure, end here if we succeed
    auto rv = ConfigureNamedPipe(m_namespace.c_str());
    if (rv)
      return rv;
  }

  // Told to quit, or timed out, terminate here
  return nullptr;
}
