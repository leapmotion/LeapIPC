// Copyright (C) 2012-2017 Leap Motion, Inc. All rights reserved.
#pragma once
#include "IPCEndpoint.h"

namespace leap {
  namespace ipc {

    class RawIPCEndpoint :
      public IPCEndpoint
    {
    public:
      // IPCEndpoint overrides:
      std::streamsize ReadRaw(void* buffer, std::streamsize size) override = 0;
      bool WriteRaw(const void* pBuf, std::streamsize nBytes) override = 0;
    };

  }
}