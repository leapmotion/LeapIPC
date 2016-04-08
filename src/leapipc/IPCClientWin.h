// Copyright (C) 2012-2016 Leap Motion, Inc. All rights reserved.
#pragma once
#include "IPCClient.h"
#include "NamedPipeStatusBlockWin.h"
#include <autowiring/CoreRunnable.h>

namespace leap {
namespace ipc {

class IPCClientWin :
  public IPCClient
{
public:
  IPCClientWin(const char* pstrNamespace);

protected:
  // Namespace where we will open the named pipe
  std::wstring m_namespace;

  // Named pipe status block, used in preference of a poll:
  NamedPipeStatusBlockWin m_statusBlock;

  // Status semaphore
  HANDLE m_hStatus;

  // CoreThread overrides:
  void OnStop(bool graceful) override;

public:
  // IPCClient overrides:
  std::shared_ptr<IPCEndpoint> Connect(void) override;
  std::shared_ptr<IPCEndpoint> Connect(std::chrono::microseconds dt) override;
};

}}
