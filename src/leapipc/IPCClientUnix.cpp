#include <iostream>

// Copyright (C) 2012-2016 Leap Motion, Inc. All rights reserved.
#include "stdafx.h"
#include "IPCClientUnix.h"
#include "IPCEndpointUnix.h"
#include <autowiring/autowiring.h>
#include <autowiring/ContextEnumerator.h>
#include <algorithm>

#include <unistd.h>
#if USE_NETWORK_SOCKETS
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#else
#include <sys/un.h>
#endif

using namespace leap::ipc;

IPCClientUnix::IPCClientUnix(const char* pstrScope, const char* pstrNamespace) :
  m_namespace{ pstrScope ? pstrScope : "" }
{
  m_namespace += pstrNamespace;
}

IPCClient* IPCClient::New(const char* pstrScope, const char* pstrNamespace) {
  return new IPCClientUnix(pstrScope, pstrNamespace);
}

std::shared_ptr<IPCEndpoint> IPCClientUnix::Connect(std::chrono::microseconds dt) {
#if USE_NETWORK_SOCKETS
  const int domain = PF_INET;
#else
  const int domain = PF_LOCAL;
#endif

#if USE_NETWORK_SOCKETS
  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr("127.0.0.1");
  addr.sin_port = htons(46438);
#else
  struct sockaddr_un addr = {0};
  addr.sun_family = AF_LOCAL;
  addr.sun_path[0] = '\0';
  m_namespace.copy(addr.sun_path+1, m_namespace.length());
#endif

  auto limit = std::chrono::steady_clock::now() + dt;
  while (!ShouldStop()) {
    int socket = ::socket(domain, SOCK_STREAM, 0);

    std::cout << "Calling connect w/ namespace: " << m_namespace << std::endl;
    if (::connect(socket, (struct sockaddr*)&addr, sizeof(addr.sun_family) + m_namespace.length() + 1) != -1) {

     std::cout << "Connect SUCCEEDED" << std::endl;
      if (socket >= 0)
        // Success, break out here
        return std::make_shared<IPCEndpointUnix>(socket);
    }
    std::cout << "Unsuccessful, calling ::shutdown" << std::endl;
    // Unsuccessful, kill this socket, we will need to regenerate it
    ::shutdown(socket, SHUT_RDWR);
    ::close(socket);

    // Connection failed. Sleep for up to 250 msec before trying again -- FIXME
    std::chrono::nanoseconds sleepTime;
    if (std::chrono::microseconds::zero() <= dt) {
      auto now = std::chrono::steady_clock::now();
      if (limit < now)
        break;
      sleepTime =
        std::min<std::chrono::nanoseconds>(
          std::chrono::milliseconds(250),
          limit - now
        );
    }
    else
      sleepTime = std::chrono::milliseconds(250);

    // Use the CoreRunnable condition variable to implement ThreadSleep
    // TODO:  Autowiring 0.7.4 pulls ThreadSleep into CoreRunnable, we should change this
    // to call that method once 0.7.4 is released.
    std::unique_lock<std::mutex> lk(m_lock);
    m_cv.wait_for(
      lk,
      sleepTime,
      [this] {
        return ShouldStop();
      }
    );
  }

  return nullptr;
}
