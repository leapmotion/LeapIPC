// Copyright (C) 2012-2017 Leap Motion, Inc. All rights reserved.
#include "stdafx.h"
#include "IPCListenerUnix.h"
#include "FileMonitor.h"
#include "IPCEndpointUnix.h"
#include <autowiring/ContextEnumerator.h>

#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#if USE_NETWORK_SOCKETS
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#else
#include <sys/un.h>
#endif
#include <sstream>
#include FILESYSTEM_HEADER

using namespace leap::ipc;

const char* leap::ipc::IPCTestScope(void) {
  // For Unix-based systems, specify a user-specific directory for the tests.
  static const std::string scopePath = [] {
    std::stringstream ss;
    ss <<
#if __ANDROID__
      "/data/local"
#endif
      "/tmp/testcontext."
      << ::getuid() << "." << ::getpid() << "/";
    return ss.str();
  }();
  return scopePath.c_str();
}

IPCListenerUnix::IPCListenerUnix(const char* pstrScope, const char* pstrNamespace)
{
  {
    int notifyPipe[2];
    if (pipe(notifyPipe))
      throw std::runtime_error("Failed to create a pipe to control the IPC listener");
    m_recvFd = notifyPipe[0];
    m_sendFd = notifyPipe[1];
    fcntl(m_sendFd, F_SETFL, O_NONBLOCK);
  }

  std::string name = pstrScope;
  if (name.back() != '/')
    throw std::invalid_argument("Unix domain socket scopes must end with a trailing slash");

  name += pstrNamespace;
  m_namespace = std::filesystem::path(name);
  if (m_namespace.empty())
    throw std::runtime_error("Cannot create an IPC listener on an empty namespace");
  if (!m_namespace.has_filename())
    throw std::runtime_error("Namespace must refer to a specific on-disk file and cannot be a directory");
  if (!m_namespace.has_parent_path())
    throw std::runtime_error("Namespace must not be directly in the root");
  if (!m_namespace.is_absolute())
    throw std::runtime_error("Namespace must not be an absolute path");
}

IPCListenerUnix::~IPCListenerUnix(void)
{
  ::close(m_sendFd);
  ::close(m_recvFd);
}

IPCListener* IPCListener::New(const char* pstrScope, const char* pstrNamespace) {
  return new IPCListenerUnix(pstrScope, pstrNamespace);
}

#if defined(__GNUC__) && !defined(__clang__)
  // gcc disregards (void) cast in this case
  #pragma GCC diagnostic ignored "-Wunused-result"
#endif

void IPCListenerUnix::OnStop(void) {
  (void)::write(m_sendFd, "_", 1);
}

IPCListenerUnix::IPCNamespace::IPCNamespace(FileMonitor* m_fileMonitor, const std::filesystem::path& ns, const int& sendFd) :
  ns(ns),
  m_socket{
    ::socket(
#if USE_NETWORK_SOCKETS
      PF_INET,
#else
      PF_LOCAL,
#endif
      SOCK_STREAM,
      0
    )
  },
  m_sendFd{sendFd}
{
  if (m_socket < 0) {
    // Nothing more to do, failed to create the socket, end here
    return;
  }

#if USE_NETWORK_SOCKETS
  struct sockaddr_in addr = {0};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = ::htonl(INADDR_LOOPBACK);
  addr.sin_port = ::htons(46438);

  const int so_enable = 1;
  ::setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &so_enable, sizeof(so_enable));
#else
  std::filesystem::path directory(ns.parent_path());
  const mode_t permissions   = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH; // 0775
  const mode_t nsPermissions = S_IRWXU | S_IRWXG | S_IRWXO;           // 0777

  if (!std::filesystem::exists(directory) && ::mkdir(directory.c_str(), permissions) == -1) {
    return;
  }
  ::chmod(directory.c_str(), permissions);

  if (::getuid() == 0) {
    // If we are root, make sure that the permissions are correct and we are the owner of the containing directory
    struct stat sb;
    if (::stat(directory.c_str(), &sb) == 0) {
      if (sb.st_mode != permissions) {
        ::chmod(directory.c_str(), permissions);
      }
      if (sb.st_uid != 0) {
        const int root = 0;    // root
#if __APPLE__
        const int admin = 80;  // admin
#else
        const int admin = 27;  // sudo
#endif
        if (::chown(directory.c_str(), root, admin) < 0) {
          throw std::runtime_error("Unable to set permissions on IPC directory");
        }
      }
    }
  }

  struct sockaddr_un addr = {0};
  addr.sun_family = AF_LOCAL;
  ns.string().copy(addr.sun_path, sizeof(addr.sun_path) - 1);
#endif

  IPCEndpointUnix::SetDefaultOptions(m_socket);

  if (
    ::bind(m_socket, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0 ||
    ::listen(m_socket, 8) < 0
  ) {
    return;
  }

  ok = true;
#if !USE_NETWORK_SOCKETS
  ::chmod(ns.c_str(), nsPermissions);
  if (m_fileMonitor) {
    m_watcher = m_fileMonitor->Watch(
      directory,
      [this] (std::shared_ptr<FileWatch> fileWatch, FileWatch::State states) mutable {
        if (
          !(states & FileWatch::State::MODIFIED) ||
          !std::filesystem::exists(this->ns)
        ) {
          ok = false;
          (void)::write(m_sendFd, ".", 1);
        }
      },
      FileWatch::State::ALL
    );
  }
#endif
}

IPCListenerUnix::IPCNamespace::~IPCNamespace(void) {
  if (m_socket >= 0) {
    ::shutdown(m_socket, SHUT_RDWR);
  }
  if (m_socket >= 0) {
    ::close(m_socket);
  }

#if !USE_NETWORK_SOCKETS
  m_watcher.reset();
  if (std::filesystem::exists(ns)) {
    ::unlink(ns.c_str());
  }
  std::filesystem::path dir = std::filesystem::path(ns).parent_path();
  if (std::filesystem::exists(dir)) {
    ::rmdir(dir.c_str());
  }
#endif
}

void IPCListenerUnix::Run(void) {
  while (!ShouldStop()) {
    IPCNamespace ns(m_fileMonitor.get(), m_namespace, m_sendFd);
    if (!ns) {
      // Something went seriously wrong! We may never succeed, but at least try
      WaitForEvent(std::chrono::seconds(5));
      continue;
    }

    while (!ShouldStop() && ns) {
      pollfd fds[2];
      fds[0].fd = m_recvFd;
      fds[0].events = POLLRDNORM;
      fds[1].fd = ns.m_socket;
      fds[1].events = POLLRDNORM | POLLRDBAND;

      int rs = poll(fds, 2, -1);
      if (rs <= 0)
        // Something went wrong, need to regenerate
        break;

      if (fds[0].revents & POLLRDNORM) {
        // We received a message from the other end of our pipe, consume it
        char msg;
        (void)::read(m_recvFd, &msg, 1);
        break;
      }

      // Other descriptor is present in the array, we can accept a connection
      int client = ::accept(ns.m_socket, nullptr, nullptr);
      if (client < 0)
        // Server socket is dead, need to regenerate
        break;

      // Create the context and inject the Unix IPC endpoint into it
      onClientConnected(std::make_shared<IPCEndpointUnix>(client));
    }
  }
}
