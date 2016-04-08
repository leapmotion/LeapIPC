#include "stdafx.h"
#include "IPCEndpointWin.h"
#include "NamedPipeWin.h"
#include <system_error>

IPCEndpointWin::IPCEndpointWin(const std::shared_ptr<NamedPipeWin>& namedPipe):
  m_namedPipe(namedPipe),
  m_namedPipeShared(namedPipe),
  m_eof(false)
{
  ULONG pid = 0;
  if (!NamedPipeWin::s_GetNamedPipeClientProcessId)
    throw std::runtime_error("Leap Service appears to be running on Windows XP or older");

  if(NamedPipeWin::s_GetNamedPipeClientProcessId(namedPipe->m_hPipe, &pid))
    m_pid = static_cast<pid_t>(pid);
}

IPCEndpointWin::~IPCEndpointWin(void)
{
}

std::streamsize IPCEndpointWin::ReadRaw(void* buffer, std::streamsize size) {
  auto namedPipe = m_namedPipe.lock();
  if(!namedPipe)
    return Done(Reason::WeakPointerLock);

  // Read up to the number of bytes requested:
  if(!ReadFile(namedPipe->m_hPipe, buffer, (DWORD) size, nullptr, &namedPipe->m_overlappedRead))
    switch(GetLastError()) {
    case ERROR_IO_PENDING:
      break;
    case ERROR_SUCCESS:
    case ERROR_MORE_DATA:
      // OK, we don't care about these conditions
      break;
    default:
      return Done(Reason::ReadFailure);
    }

  if(m_eof)
    return Done(Reason::UserClosed);

  // Ok, figure out what our results are
  DWORD dwTotalRead;
  if(!GetOverlappedResult(namedPipe->m_hPipe, &namedPipe->m_overlappedRead, &dwTotalRead, true))
    switch(GetLastError()) {
    case ERROR_MORE_DATA:
      // Yeah whatever, we'll get it next time
      break;
    case ERROR_PIPE_NOT_CONNECTED:
      return Done(Reason::ConnectionLost);
    default:
      // Something went wrong!
      return Done(Reason::Unspecified);
    }

  return (int)dwTotalRead;
}

bool IPCEndpointWin::WriteRaw(const void* pBuf, std::streamsize nBytes) {
  auto namedPipe = m_namedPipe.lock();
  if(!namedPipe)
    return false;

  if(!WriteFile(namedPipe->m_hPipe, pBuf, (DWORD) nBytes, nullptr, &namedPipe->m_overlappedWrite))
    switch(GetLastError()) {
    case ERROR_IO_PENDING:
      // Expected condition for overlapped IO
      break;
    default:
      // Something went wrong, abandon:
      return false;
    }

  DWORD dwTotalWritten;
  return GetOverlappedResult(namedPipe->m_hPipe, &namedPipe->m_overlappedWrite, &dwTotalWritten, true) == TRUE;
}

bool IPCEndpointWin::Abort(Reason reason) {
  bool wasEOF = false;
  if(!m_eof.compare_exchange_strong(wasEOF, true))
    // Already EOF, we don't need to release our shared pointer a second time
    return false;

  // Abandon any pipe operations that are presently underway:
  NamedPipeWin::s_CancelIoEx(m_namedPipeShared->m_hPipe, &m_namedPipeShared->m_overlappedWrite);
  NamedPipeWin::s_CancelIoEx(m_namedPipeShared->m_hPipe, &m_namedPipeShared->m_overlappedRead);

  // Release shared pointer.  We are guaranteed to be the only one here because of the interlocked
  // operation above.
  m_namedPipeShared.reset();

  Close(reason);
  return true;
}

// When we are done communicating over this pipe, so simulate EOF behavior. On
// the first call, return 0 (EOF). Subsequent calls, return -1 (error).
int IPCEndpointWin::Done(Reason reason) {
  return Abort(reason) ? 0 : -1;
}
