// Copyright (C) 2012-2018 Leap Motion, Inc. All rights reserved.
#pragma once
#include "RawIPCEndpoint.h"
#include <memory>
#include <mutex>
#include <condition_variable>
#include <vector>

namespace leap { 
namespace ipc {

class CircularBufferEndpoint :
  public leap::ipc::RawIPCEndpoint
{
public:
  explicit CircularBufferEndpoint(size_t bufferSize);
  ~CircularBufferEndpoint(void);

  // IPCEndpoint overrides:
  std::streamsize ReadRaw(void* buffer, std::streamsize size) override;
  bool WriteRaw(const void* pBuf, std::streamsize nBytes) override;
  bool Abort(Reason reason) override;

  void clear();

private:
  std::mutex m_dataMutex;
  std::condition_variable m_dataCV;

  std::unique_ptr<uint8_t[]> m_data;
  size_t m_capacity = 0;
  size_t m_writeIdx = 0;
  size_t m_readIdx = 0;
  size_t m_lastReadSize = 0;
  size_t m_lastWriteSize = 0;

  size_t readAvailable();
  void resizeUnsafe(size_t newCapacity);

  void readUnsafe(void* buffer, size_t size);
  void writeUnsafe(const void* buffer, size_t size);
};

}
}
