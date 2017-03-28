// Copyright (C) 2012-2017 Leap Motion, Inc. All rights reserved.
#pragma once
#include "IPCListener.h"
#include "NamedPipeStatusBlockWin.h"
#include "AutoHandle.h"
#include <autowiring/ObjectPool.h>

namespace leap {
namespace ipc {

class NamedPipeWin;

/// <summary>
/// Windows named pipe server implementation
/// </summary>
class IPCListenerWin:
  public IPCListener
{
public:
  IPCListenerWin(const char* pstrNamespace);
  virtual ~IPCListenerWin(void);

private:
  // Namespace where our named pipe server will go--adjusted with a \\pipe\ prefix for Windows
  std::wstring m_namespace;

  // Status block, provided so that clients have something they can wait on
  NamedPipeStatusBlockWin m_statusBlock;

  /// <returns>
  /// A new pipe instance
  /// </returns>
  NamedPipeWin* CreateNamedPipeWrapper(void) const;

  /// <summary>
  /// Notification routine called by resetter lambda when a named pipe instance returns to us
  /// </summary>
  /// <param name="namedPipe">The pipe that was just returned</param>
  void OnObjectReturned(NamedPipeWin& namedPipe);

protected:
  // CoreThread overrides:
  void OnStop(void) override;
  void Run(void) override;
};

}}
