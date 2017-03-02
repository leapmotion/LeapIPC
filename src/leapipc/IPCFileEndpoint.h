// Copyright (C) 2012-2016 Leap Motion, Inc. All rights reserved.
#pragma once
#include "IPCEndpoint.h"
#include "AutoHandle.h"
#include <fstream>

namespace leap {
namespace ipc {

class IPCFileEndpoint:
  public IPCEndpoint
{
public:
  IPCFileEndpoint(const std::string & fileName, bool read, bool write);
  ~IPCFileEndpoint(void);

private:
  std::fstream m_file;

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