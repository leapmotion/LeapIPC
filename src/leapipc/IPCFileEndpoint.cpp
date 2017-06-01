// Copyright (C) 2012-2017 Leap Motion, Inc. All rights reserved.
#include "stdafx.h"
#include "IPCFileEndpoint.h"
#include <system_error>

using namespace leap::ipc;

IPCFileEndpoint::IPCFileEndpoint(const std::string& fileName, bool read, bool write)
{
  std::ios_base::openmode mode = std::ios::binary;
  if (read) mode |= std::ios::in;
  if (write) mode |= std::ios::out;
  m_file.open(fileName, mode);
  if (!m_file.is_open())
    throw std::runtime_error("Failed to open file");
}

IPCFileEndpoint::~IPCFileEndpoint(void)
{
  Abort(Reason::Unspecified);
}

std::streamsize IPCFileEndpoint::ReadRaw(void* buffer, std::streamsize size) {
  if (m_file.eof()) {
    // On the first call, return 0 (EOF). Subsequent calls, return -1 (error).
    return Abort(Reason::UserClosed) ? 0 : -1;
  }
  m_file.read((char*)buffer, size);
  return m_file.gcount();
}

bool IPCFileEndpoint::WriteRaw(const void* pBuf, std::streamsize nBytes) {
  m_file.write((char*)pBuf, nBytes);
  if (m_file.bad())
    return false;
  return true;
}

bool IPCFileEndpoint::Abort(Reason reason) {
  if (!m_file.is_open()) {
    return false;
  }
  m_file.close();
  Close(reason);
  return true;
}
