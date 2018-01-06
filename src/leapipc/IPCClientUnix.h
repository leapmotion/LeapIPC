// Copyright (C) 2012-2018 Leap Motion, Inc. All rights reserved.
#pragma once
#include "IPCClient.h"
#include <atomic>
#include <string>

namespace leap {
namespace ipc {

class IPCClientUnix:
  public IPCClient
{
public:
  IPCClientUnix(const char* pstrScope, const char* pstrNamespace);

protected:
  // Namespace of our domain socket file
  std::string m_namespace;

public:
  std::shared_ptr<IPCEndpoint> Connect(void) override {
    return Connect(std::chrono::microseconds{ -1 });
  }
  std::shared_ptr<IPCEndpoint> Connect(std::chrono::microseconds dt) override;
};

}}
