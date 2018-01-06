// Copyright (C) 2012-2018 Leap Motion, Inc. All rights reserved.
#include "stdafx.h"
#include "CircularBufferEndpoint.h"
#include <system_error>

using namespace leap::ipc;

CircularBufferEndpoint::CircularBufferEndpoint(size_t bufferSize) :
  m_data{new uint8_t[bufferSize]},
  m_capacity{bufferSize}
{
}

CircularBufferEndpoint::~CircularBufferEndpoint(void) {
  Abort(Reason::Unspecified);
}

std::streamsize CircularBufferEndpoint::ReadRaw(void* buffer, std::streamsize size)
{
  {
    std::unique_lock<std::mutex> lock(m_dataMutex);
    m_lastReadSize = size;
    m_dataCV.wait(lock, [this] {
      auto read = readAvailable();
      const auto write = m_capacity - read;
      if (m_lastReadSize > read && m_lastWriteSize > write) {
        m_dataCV.notify_one(); // notify the writing thread so it can resize and write
      }
      return read >= m_lastReadSize || IsClosed();
    });
    if (IsClosed())
      return 0;

    // there are 'size' bytes available in the buffer
    readUnsafe(buffer, size);

    m_lastReadSize = 0;
  }
  m_dataCV.notify_one();

  return size;
}

bool CircularBufferEndpoint::WriteRaw(const void* pBuf, std::streamsize nBytes)
{
  {
    std::unique_lock<std::mutex> lock(m_dataMutex);
    m_lastWriteSize = nBytes;
    m_dataCV.wait(lock, [this] {
      const auto read = readAvailable();
      auto write = m_capacity - read;
      if (m_lastReadSize > read && m_lastWriteSize > write) {
        resizeUnsafe(std::max<size_t>(m_lastReadSize + m_lastWriteSize, m_capacity * 2));
        write = m_capacity - m_writeIdx;
      }
      return write > m_lastWriteSize || IsClosed(); // must be strictly greater to avoid ambiguous m_writeIdx == m_readIdx state
    });
    if (IsClosed())
      return false;

    // the data fits in the buffer so we copy it
    writeUnsafe(pBuf, nBytes);

    m_lastWriteSize = 0;
  }
  m_dataCV.notify_one();

  return true;
}

bool CircularBufferEndpoint::Abort(Reason reason) {
  Close(reason);
  m_dataCV.notify_all();
  return true;
}

void CircularBufferEndpoint::clear()
{
  std::lock_guard<std::mutex> lock(m_dataMutex);
  m_readIdx = 0;
  m_writeIdx = 0;
}

size_t CircularBufferEndpoint::readAvailable() {
  if (m_writeIdx >= m_readIdx)
    return m_writeIdx - m_readIdx;
  else
    return m_capacity - (m_readIdx - m_writeIdx);
}

void CircularBufferEndpoint::resizeUnsafe(size_t newCapacity)
{
  const size_t n = readAvailable();
  std::unique_ptr<uint8_t[]> newbuf{new uint8_t[newCapacity]};

  // copy to new buffer
  readUnsafe(newbuf.get(), n);

  m_data = std::move(newbuf);
  m_readIdx = 0;
  m_writeIdx = n;
  m_capacity = newCapacity;
}

void CircularBufferEndpoint::readUnsafe(void* buffer, size_t size) {
  const int wrap = static_cast<int>(m_readIdx + size) - static_cast<int>(m_capacity);
  if (wrap > 0) {
    memcpy(buffer, m_data.get() + m_readIdx, size - wrap);
    memcpy(reinterpret_cast<char*>(buffer) + (size - wrap), m_data.get(), wrap);
    m_readIdx = wrap;
  }
  else {
    memcpy(buffer, m_data.get() + m_readIdx, size);
    m_readIdx += size;
  }
}

void CircularBufferEndpoint::writeUnsafe(const void* buffer, size_t size) {
  const int wrap = static_cast<int>(m_writeIdx + size) - static_cast<int>(m_capacity);
  if (wrap > 0) {
    memcpy(m_data.get() + m_writeIdx, buffer, size - wrap);
    memcpy(m_data.get(), reinterpret_cast<const char*>(buffer) + size - wrap, wrap);
    m_writeIdx = wrap;
  }
  else {
    memcpy(m_data.get() + m_writeIdx, buffer, size);
    m_writeIdx += size;
  }
}
