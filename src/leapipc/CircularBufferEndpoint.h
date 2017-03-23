// Copyright (C) 2012-2017 Leap Motion, Inc. All rights reserved.
#pragma once
#include "RawIPCEndpoint.h"
#include <mutex>
#include <condition_variable>
#include <vector>


class CircularBufferEndpoint :
  public leap::ipc::RawIPCEndpoint
{
public:
  CircularBufferEndpoint(size_t bufferSize);
  ~CircularBufferEndpoint(void);

private:

  std::mutex m_dataMutex;
  std::condition_variable m_dataCV;

  size_t m_capacity;
  uint8_t* m_data;
  size_t m_writeIdx = 0;
  size_t m_readIdx = 0;

  /// <summary>
  /// Simulate EOF behavior when we are done.
  /// </summary>
  int Done(Reason reason);

  size_t readAvailable();

  size_t m_lastReadSize = 0;
  size_t m_lastWriteSize = 0;

  void ReadUnsafe(void* buffer, size_t size);
  void WriteUnsafe(const void* buffer, size_t size);

  void resize(size_t newCapacity);

public:
  // IPCEndpoint overrides:
  std::streamsize ReadRaw(void* buffer, std::streamsize size) override;
  bool WriteRaw(const void* pBuf, std::streamsize nBytes) override;
  bool Abort(Reason reason) override;
};