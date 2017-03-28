// Copyright (C) 2012-2017 Leap Motion, Inc. All rights reserved.
#include "stdafx.h"
#include "NamedPipeWin.h"

using namespace leap::ipc;

static const HMODULE hKernel32 = LoadLibrary("kernel32.dll");
const decltype(&GetNamedPipeClientProcessId) NamedPipeWin::s_GetNamedPipeClientProcessId = (decltype(&GetNamedPipeClientProcessId))GetProcAddress(hKernel32, "GetNamedPipeClientProcessId");
const decltype(&CancelIoEx) NamedPipeWin::s_CancelIoEx = (decltype(&CancelIoEx)) GetProcAddress(hKernel32, "CancelIoEx");

NamedPipeWin::NamedPipeWin(HANDLE hPipe):
  m_hPipe(hPipe)
{
}

NamedPipeWin::~NamedPipeWin(void)
{
  if(!m_hPipe)
    // Nothing to do
    return;

  // Cancel all outstanding IO operations on these pipes:
  NamedPipeWin::s_CancelIoEx(m_hPipe, &m_overlappedRead);
  NamedPipeWin::s_CancelIoEx(m_hPipe, &m_overlappedWrite);

  // Now we can close the actual handle
  CloseHandle(m_hPipe);
}
