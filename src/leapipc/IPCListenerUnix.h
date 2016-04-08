// Copyright (C) 2012-2016 Leap Motion, Inc. All rights reserved.
#pragma once
#include "IPCListener.h"
#include <autowiring/autowiring.h>
#include <boost/filesystem/path.hpp>

namespace leap {
namespace ipc {

class FileMonitor;
class FileWatch;

/// <summary>
/// UNIX Domain Socket server implementation
/// </summary>
class IPCListenerUnix:
  public IPCListener
{
public:
  IPCListenerUnix(const char* pstrScope, const char* pstrNamespace);
  virtual ~IPCListenerUnix(void);

private:
  // Namespace of our domain socket file
  boost::filesystem::path m_namespace;
  Autowired<FileMonitor> m_fileMonitor;

  // Pipe used to wake up the connection loop
  int m_sendFd;
  int m_recvFd;

  struct IPCNamespace {
    IPCNamespace(FileMonitor* m_fileMonitor, const boost::filesystem::path& ns, const int& sendFd);
    ~IPCNamespace(void);

    bool ok{ false };
    const boost::filesystem::path ns;
    const int m_socket;
    const int& m_sendFd;
    std::shared_ptr<FileWatch> m_watcher;

    operator bool(void) const { return ok; }
  };

  void OnStop(void) override;

protected:
  // CoreThread overrides:
  void Run(void) override;
};

}}
