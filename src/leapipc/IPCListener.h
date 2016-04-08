// Copyright (C) 2012-2016 Leap Motion, Inc. All rights reserved.
#pragma once
#include <autowiring/CoreThread.h>
#include <autowiring/auto_signal.h>

class CoreContext;

namespace leap {
namespace ipc {

class IPCEndpoint;

/// <summary>
/// Retrieves a scope that is known to be valid in test scenarios
/// </summary>
/// <remarks>
/// Test contexts have slightly different requirements from system contexts.  In particular, they must be unique
/// and ephemeral, and it must be possible to create them without worrying about concurrent access issues.  The
/// returned handle will be one that is unique for the lifetime of the process, and which the caller may use in
/// a call to `IPCListener::New` or `IPCClient::New`.
/// </remarks>
const char* IPCTestScope(void);

/// <summary>
/// Generates a random namespace for string with testing purposes
/// </summary>
std::string IPCTestNamespace(void);

/// <summary>
/// Native stream-based interprocess communication listener
/// </summary>
class IPCListener:
  public CoreThread
{
public:
  IPCListener(void);
  virtual ~IPCListener(void);

  // Handler invoked when a new client endpoint has been connected
  // At this point the context will not yet have been started.  It is an error for listeners on this routine
  // to initiate the passed context.  Listeners may inject anything they want into the passed context.  The
  // first argument, the IPCEndpoint, is guaranteed to exist in the context.
  autowiring::signal<void(const std::shared_ptr<IPCEndpoint>&)> onClientConnected;

  /// <summary>
  /// Creates a new IPC listener in the specified scope and namespace
  /// </summary>
  /// <param name="pstrScope">The scope where the namespace will be created, or nullptr to use the default</param>
  /// <param name="pstrNamespace">The namespace where this listener will listen for connections</param>
  /// <remarks>
  /// The "scope" term has a platform-dependent meaning.  On Linux, this is a file path where the domain
  /// socket file handle lock will be created.  The caller must have permissions to write to the specified
  /// path.  On Windows, this parameter refers to whether the created named pipe will be at session or global
  /// scope.
  /// </remarks>
  static IPCListener* New(const char* pstrScope, const char* pstrName);

  /// <summary>
  /// Creates a new IPC listener in the default scope and specified namespace
  /// </summary>
  /// <param name="pstrNamespace">The namespace where the server exists</param>
  static IPCListener* New(const char* pstrNamespace) {
    return New(nullptr, pstrNamespace);
  }
};

}}
