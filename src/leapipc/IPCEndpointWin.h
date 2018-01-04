// Copyright (C) 2012-2018 Leap Motion, Inc. All rights reserved.
#pragma once
#include "IPCEndpoint.h"
#include "AutoHandle.h"
#include <atomic>
#include <memory>

namespace leap {
namespace ipc {

class NamedPipeWin;

class IPCEndpointWin:
  public IPCEndpoint
{
public:
  IPCEndpointWin(const std::shared_ptr<NamedPipeWin>& namedPipe);
  ~IPCEndpointWin(void);

private:
  const std::weak_ptr<NamedPipeWin> m_namedPipe;
  std::atomic<bool> m_eof;

  // Strong hold on the shared pointer, held until Abort or destruction
  std::shared_ptr<NamedPipeWin> m_namedPipeShared;

  /// <summary>
  /// Simulate EOF behavior when we are done.
  /// </summary>
  int Done(Reason reason);

public:
  // IPCEndpoint overrides:
  std::streamsize ReadRaw(void* buffer, std::streamsize size) override;
  bool WriteRaw(const void* pBuf, std::streamsize nBytes) override;
  bool Abort(Reason reason) override;
};

}}