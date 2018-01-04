// Copyright (C) 2012-2018 Leap Motion, Inc. All rights reserved.
#include "stdafx.h"
#include "IPCEndpointUnix.h"

#include <unistd.h>
#if USE_NETWORK_SOCKETS
#include <netinet/in.h>
#include <netinet/tcp.h>
#elif __APPLE__
#include <sys/un.h>
#else
#include <poll.h>
#endif

using namespace leap::ipc;

IPCEndpointUnix::IPCEndpointUnix(int socket):
  m_socket{socket}
{
  SetDefaultOptions(socket);
#if !USE_NETWORK_SOCKETS
#if __APPLE__
  pid_t pid = 0;
  socklen_t pidlen = sizeof(pid);
  if (::getsockopt(m_socket, SOL_LOCAL, LOCAL_PEERPID , &pid, &pidlen) != -1) {
    m_pid = pid;
  }
#else
  struct ucred cred = {0};
  socklen_t credlen = sizeof(cred);
  if (::getsockopt(m_socket, SOL_SOCKET, SO_PEERCRED, &cred, &credlen) != -1) {
    m_pid = cred.pid;
  }
#endif
#endif
}

IPCEndpointUnix::~IPCEndpointUnix(void)
{
  Abort(Reason::Unspecified);
}

std::streamsize IPCEndpointUnix::ReadRaw(void* buffer, std::streamsize size) {
#if !USE_NETWORK_SOCKETS && !__APPLE__
  struct pollfd fds = { m_socket, POLLIN, 0 };
  if (fds.fd < 0 || poll(&fds, 1, -1) <= 0 || !(fds.revents & POLLIN)) {
    return -1;
  }
#endif
  return ::recv(m_socket, buffer, size, MSG_NOSIGNAL);
}

bool IPCEndpointUnix::WriteRaw(const void* pBuf, std::streamsize nBytes) {
  return (nBytes == ::send(m_socket, pBuf, nBytes, MSG_NOSIGNAL));
}

bool IPCEndpointUnix::Abort(Reason reason) {
  int socket = m_socket.exchange(-1);
  if (socket < 0) {
    return false;
  }
  ::shutdown(socket, SHUT_RDWR);
  ::close(socket);
  Close(reason);
  return true;
}

void IPCEndpointUnix::SetDefaultOptions(int socket) {
#if USE_NETWORK_SOCKETS
  const int so_enable = 1;
  ::setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, &so_enable, sizeof(so_enable));
#else
  const int so_size = 262144;
  ::setsockopt(socket, SOL_SOCKET, SO_SNDBUF, &so_size, sizeof(so_size));
#endif
#if __APPLE__
  const int so_nosigpipe = 1;
  ::setsockopt(socket, SOL_SOCKET, SO_NOSIGPIPE, &so_nosigpipe, sizeof(so_nosigpipe));
#endif
  const struct linger so_linger = { 1, 1 };
  ::setsockopt(socket, SOL_SOCKET, SO_LINGER, &so_linger, sizeof(so_linger));
}
