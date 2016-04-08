#pragma once
#include <autowiring/CoreJob.h>
#include <autowiring/auto_signal.h>

class IPCClient;
class IPCEndpoint;

/// <summary>
/// A simple CoreJob type that will attempt to connect to a server
/// </summary>
class IPCClientConnector:
  public CoreJob
{
public:
  IPCClientConnector(IPCClient& client);
  ~IPCClientConnector(void);

  // Signal invoked when a connection is established
  autowiring::signal<void(const std::shared_ptr<IPCEndpoint>& ep)> onConnected;

private:
  IPCClient& client;

public:
  void TryConnect(void);
};

