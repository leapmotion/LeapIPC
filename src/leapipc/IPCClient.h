// Copyright (C) 2012-2016 Leap Motion, Inc. All rights reserved.
#pragma once
#include <autowiring/CoreObject.h>
#include <autowiring/CoreRunnable.h>
#include <chrono>
#include <memory>

namespace leap {
namespace ipc {

class IPCEndpoint;

/// <summary>
/// Represents the client end of a server-client connection
/// </summary>
/// <remarks>
/// Clients implement the CoreRunnable interface so that they will honor vigilance changes of any context
/// that they are injected into.  They are not required to be injected into any context.
/// </remarks>
class IPCClient:
  public CoreObject,
  public CoreRunnable
{
public:
  /// <summary>
  /// Creates a new IPC client in the specified scope and namespace
  /// </summary>
  /// <param name="pstrScope">The scope where the server exists</param>
  /// <param name="pstrNamespace">The namespace where the server exists</param>
  static IPCClient* New(const char* pstrScope, const char* pstrNamespace);

  /// <summary>
  /// Creates a new IPC client in the default scope and specified namespace
  /// </summary>
  /// <param name="pstrNamespace">The namespace where the server exists</param>
  static IPCClient* New(const char* pstrNamespace) {
    return New(nullptr, pstrNamespace);
  }

  /// <summary>
  /// Attemps to connect to the server, returns the endpoint if we are successful
  /// </summary>
  /// <returns>
  /// A connected endpoint, or in the event of failure, an exception is thrown
  /// </returns>
  virtual std::shared_ptr<IPCEndpoint> Connect(void) = 0;

  /// <summary>
  /// Timed version of Connect
  /// </summary>
  virtual std::shared_ptr<IPCEndpoint> Connect(std::chrono::microseconds dt) = 0;

  // CoreRunnable overrides:
  bool OnStart(void) override { return true; }
};

}}