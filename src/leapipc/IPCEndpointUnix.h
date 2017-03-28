// Copyright (C) 2012-2017 Leap Motion, Inc. All rights reserved.
#pragma once
#include "IPCEndpoint.h"
#include <atomic>
#include <string>

#include <sys/socket.h>
#if !defined(MSG_NOSIGNAL)
#define MSG_NOSIGNAL 0
#endif

namespace leap {
namespace ipc {

class IPCEndpointUnix:
  public IPCEndpoint
{
public:
  IPCEndpointUnix(int socket);
  ~IPCEndpointUnix(void);

  // IPCEndpoint overrides:
  std::streamsize ReadRaw(void* buffer, std::streamsize size) override;
  bool WriteRaw(const void* pBuf, std::streamsize nBytes) override;
  bool Abort(Reason reason) override;

  static void SetDefaultOptions(int socket);

private:
  // File descriptor of our socket
  std::atomic<int> m_socket;
};

}}
