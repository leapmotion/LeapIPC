// Copyright (C) 2012-2017 Leap Motion, Inc. All rights reserved.
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
  m_namespace.copy(addr.sun_path, sizeof(addr.sun_path) - 1);
#endif

  const auto limit = std::chrono::steady_clock::now() + dt;
  int fn_2 = 1, fn_1 = 0, delay = 0;
  while (!ShouldStop()) {
    int socket = ::socket(domain, SOCK_STREAM, 0);

    if (::connect(socket, (struct sockaddr*)&addr, sizeof(addr)) != -1) {
      if (socket >= 0)
        // Success, break out here
        return std::make_shared<IPCEndpointUnix>(socket);
    }

    // Unsuccessful, kill this socket, we will need to regenerate it
    ::shutdown(socket, SHUT_RDWR);
    ::close(socket);


    // Connection failed. Sleep before trying again...
    std::chrono::nanoseconds sleepTime;
    if (std::chrono::microseconds::zero() <= dt) {
      const auto now = std::chrono::steady_clock::now();
      if (limit < now)
        break;
      sleepTime =
        std::min<std::chrono::nanoseconds>(
          std::chrono::milliseconds(delay),
          limit - now
        );
      if (delay < 233) {
        delay = fn_2 + fn_1; // Use Fibonacci numbers for the delay
        fn_2 = fn_1;
        fn_1 = delay;
      }
    }
    else
      sleepTime = std::chrono::milliseconds(delay);

    ThreadSleep(sleepTime);
  }

  return nullptr;
}
