// Copyright (C) 2012-2018 Leap Motion, Inc. All rights reserved.
#include "stdafx.h"
#include "FileMonitorMac.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/event.h>

using namespace leap::ipc;

//
// FileWatchMac
//

FileWatchMac::FileWatchMac(const std::filesystem::path& path) : FileWatch(path)
{
  m_key = ::open(Path().c_str(), O_EVTONLY);
}

FileWatchMac::~FileWatchMac()
{
  if (m_key >= 0) {
    ::close(m_key);
  }
}

//
// FileMonitorMac
//

FileMonitorMac::FileMonitorMac() : m_kqueue(::kqueue())
{
}

FileMonitorMac::~FileMonitorMac()
{
  if (m_kqueue >= 0) {
    ::close(m_kqueue);
    m_kqueue = -1;
  }
}

FileMonitor* FileMonitor::New() {
  return new FileMonitorMac();
}

void FileMonitorMac::OnStop()
{
  if (m_kqueue >= 0) {
    ::close(m_kqueue);
    m_kqueue = -1;
  }
}

void FileMonitorMac::Run()
{
  while (!ShouldStop()) {
    struct kevent event{};
    if (::kevent(m_kqueue, nullptr, 0, &event, 1, nullptr) <= 0 ||
        event.filter != EVFILT_VNODE) {
      continue;
    }
    std::unique_lock<std::mutex> lock(m_mutex);
    auto found = m_watchers.find(event.ident);
    if (found == m_watchers.end()) {
      continue;
    }
    auto fileWatch = found->second->fileWatch.lock();
    if (!fileWatch) {
      continue;
    }
    auto callback = found->second->callback;
    lock.unlock();
    FileWatch::State states = FileWatch::State::NONE;
    if (event.fflags & NOTE_RENAME) {
      states = states | FileWatch::State::RENAMED;
    }
    if (event.fflags & NOTE_DELETE) {
      states = states | FileWatch::State::DELETED;
    }
    if (event.fflags & (NOTE_WRITE | NOTE_ATTRIB)) {
      states = states | FileWatch::State::MODIFIED;
    }
    try {
      callback(fileWatch, states);
    } catch (...) {}
  }
}

std::shared_ptr<FileWatch> FileMonitorMac::Watch(const std::filesystem::path& path,
                                                 const t_callbackFunc& callback,
                                                 FileWatch::State states)
{
  std::shared_ptr<FileWatchMac> fileWatch;

  if (states == FileWatch::State::NONE) {
    return fileWatch;
  }
  fileWatch = std::shared_ptr<FileWatchMac>(new FileWatchMac(path),
                                            [this] (FileWatchMac* fileWatch) {
                                              struct kevent event{0};
                                              const auto key = fileWatch->m_key;
                                              // Remove from kqueue
                                              EV_SET(&event, key, EVFILT_VNODE, EV_DELETE, 0, 0, 0);
                                              ::kevent(m_kqueue, &event, 1, nullptr, 0, nullptr);
                                              std::unique_lock<std::mutex> lock(m_mutex);
                                              m_watchers.erase(key);
                                              lock.unlock();
                                              delete fileWatch;
                                            });
  if (!fileWatch) {
    return fileWatch;
  }
  const int key = fileWatch->m_key;
  if (key < 0) {
    fileWatch.reset();
    return fileWatch;
  }
  struct kevent event{0};
  uint32_t fflags = 0;

  if (states & FileWatch::State::RENAMED) {
    fflags |= NOTE_RENAME;
  }
  if (states & FileWatch::State::DELETED) {
    fflags |= NOTE_DELETE;
  }
  if (states & FileWatch::State::MODIFIED) {
    fflags |= NOTE_WRITE;
    fflags |= NOTE_ATTRIB;
  }
  // Add to kqueue
  EV_SET(&event, key, EVFILT_VNODE, EV_ADD | EV_ENABLE | EV_CLEAR, fflags, 0, 0);
  if (::kevent(m_kqueue, &event, 1, nullptr, 0, nullptr) < 0) {
    fileWatch.reset();
    return fileWatch;
  }
  auto watcher = std::make_shared<Watcher>(Watcher{fileWatch, callback});
  std::unique_lock<std::mutex> lock(m_mutex);
  m_watchers[key] = watcher;
  return fileWatch;
}

int FileMonitorMac::WatchCount() const
{
  std::unique_lock<std::mutex> lock(m_mutex);
  return m_watchers.size();
}
