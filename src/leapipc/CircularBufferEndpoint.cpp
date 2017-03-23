// Copyright (C) 2012-2017 Leap Motion, Inc. All rights reserved.
#include "stdafx.h"
#include "CircularBufferEndpoint.h"
#include <system_error>

using namespace leap::ipc;

CircularBufferEndpoint::CircularBufferEndpoint(size_t bufferSize)
{
  m_data = new uint8_t[bufferSize];
  m_capacity = bufferSize;
}

CircularBufferEndpoint::~CircularBufferEndpoint(void)
{
  delete[] m_data;
}

size_t CircularBufferEndpoint::readAvailable() {
  if (m_writeIdx >= m_readIdx)
    return m_writeIdx - m_readIdx;
  else
    return m_capacity - (m_readIdx - m_writeIdx);
}

void CircularBufferEndpoint::resize(size_t newCapacity)
{
  size_t n = readAvailable();
  uint8_t* newbuf = new uint8_t[newCapacity];

  // copy to new buffer
  ReadUnsafe(newbuf, n);

  delete[] m_data;
  m_data = newbuf;
  m_readIdx = 0;
  m_writeIdx = n;
  m_capacity = newCapacity;
}

void CircularBufferEndpoint::clear()
{
  m_readIdx = 0;
  m_writeIdx = 0;
}

void CircularBufferEndpoint::ReadUnsafe(void* buffer, size_t size) {
  int wrap = (int)(m_readIdx + size) - (int)m_capacity;
  if (wrap > 0) {
    memcpy(buffer, (void*)(m_data + m_readIdx), size - wrap);
    memcpy((char*)buffer + (size - wrap), (void*)(m_data), wrap);
    m_readIdx = wrap;
  }
  else {
    memcpy(buffer, (void*)(m_data + m_readIdx), size);
    m_readIdx += size;
  }
}

void CircularBufferEndpoint::WriteUnsafe(const void* buffer, size_t size) {
  int wrap = (int)(m_writeIdx + size) - (int)m_capacity;
  if (wrap > 0) {
    memcpy((void*)(m_data + m_writeIdx), buffer, size - wrap);
    memcpy((void*)(m_data), (char*)buffer + size - wrap, wrap);
    m_writeIdx = wrap;
  }
  else {
    memcpy((void*)(m_data + m_writeIdx), buffer, size);
    m_writeIdx += size;
  }
}

std::streamsize CircularBufferEndpoint::ReadRaw(void* buffer, std::streamsize size)
{
  {
    std::unique_lock<std::mutex> lock(m_dataMutex);
    m_lastReadSize = size;
    m_dataCV.wait(lock, [this] {
      auto read = readAvailable();
      auto write = m_capacity - read;
      if (m_lastReadSize > read && m_lastWriteSize > write) {
        resize(std::max<size_t>(m_lastReadSize + m_lastWriteSize, m_capacity * 2));
        read = m_writeIdx;
      }
      return read >= m_lastReadSize;
    });

    // there are 'size' bytes available in the buffer
    ReadUnsafe(buffer, size);

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
      auto read = readAvailable();
      auto write = m_capacity - read;
      if (m_lastReadSize > read && m_lastWriteSize > write) {
        resize(std::max<size_t>(m_lastReadSize + m_lastWriteSize, m_capacity * 2));
        write = m_capacity - m_writeIdx;
      }
      return write > m_lastWriteSize; // must be strictly greater to avoid ambiguous m_writeIdx == m_readIdx state
    });

    // the data fits in the buffer so we copy it
    WriteUnsafe(pBuf, nBytes);

    m_lastWriteSize = 0;
  }
  m_dataCV.notify_one();

  return true;
}

bool CircularBufferEndpoint::Abort(Reason reason) {
  return true;
}

// On the first call, return 0 (EOF). Subsequent calls, return -1 (error).
int CircularBufferEndpoint::Done(Reason reason) {
  return Abort(reason) ? 0 : -1;
}
