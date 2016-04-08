#pragma once
#include "Overlapped.h"

namespace leap {
namespace ipc {

/// <summary>
/// Windows named pipe RAII class
/// </summary>
class NamedPipeWin
{
public:
  NamedPipeWin(HANDLE hPipe);
  ~NamedPipeWin(void);

  // Handle to the pipe proper
  const HANDLE m_hPipe;

  // Overlapped structure used for pipe read operations
  Overlapped m_overlappedRead;

  // Another structure used for pipe write operations
  Overlapped m_overlappedWrite;

  static const decltype(&GetNamedPipeClientProcessId) s_GetNamedPipeClientProcessId;
  static const decltype(&CancelIoEx) s_CancelIoEx;

  HANDLE operator*(void) const { return m_hPipe; }
};

}}
