// Copyright (C) 2012-2018 Leap Motion, Inc. All rights reserved.
#include "stdafx.h"
#include "FileMonitorUnix.h"
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <sys/inotify.h>
#include <atomic>
#include FILESYSTEM_HEADER

using namespace leap::ipc;

FileWatchUnix::FileWatchUnix(const std::filesystem::path& path) : FileWatch(path), m_key(-1)
{
}

FileWatchUnix::~FileWatchUnix()
{
}

//
// FileMonitorUnix
//

FileMonitorUnix::FileMonitorUnix():
  m_inotify(::inotify_init()),
  m_pipes{-1, -1},
  m_moveCookie(0)
{
  if (pipe(m_pipes) == -1) {
    // Got an error, not much we can do about it
  }
}

FileMonitorUnix::~FileMonitorUnix()
{
  Stop();
  ::close(m_pipes[0]);
}

FileMonitor* FileMonitor::New() {
  return new FileMonitorUnix();
}

void FileMonitorUnix::OnStop()
{
  // Closing the inotify file descriptor doesn't wake the poll, using a
  // secondary file descriptor to do the job.
  ::close(m_pipes[1]);
  ::close(m_inotify);
  m_inotify = -1;
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_watchers.clear();
}

void FileMonitorUnix::Run()
{
  if (m_inotify < 0)
    // Cannot proceed, root fd isn't valid
    return;

  // Stream wrapper for our file descriptor:
  const size_t BUF_SIZE = sizeof(struct inotify_event) + NAME_MAX + 1;
  char buf[BUF_SIZE];

  while (!ShouldStop()) {
    struct pollfd fds[2] = {
      {m_inotify, POLLIN, 0},
      {m_pipes[0], POLLIN, 0}
    };

    // wait for something to happen
    if (poll(fds, sizeof(fds) / sizeof(fds[0]), -1) <= 0)
      // Something went wrong in polling, maybe we were told to stop, end here
      break;

    if (!(fds[0].revents & POLLIN))
      // Event occurred on something other than the first fd, also end here
      break;

    const int readBytes = ::read(m_inotify, buf, BUF_SIZE);

    // Process events
    char* readPtr = buf;
    while (readPtr + sizeof(inotify_event) <= buf + readBytes) {
      inotify_event* event = reinterpret_cast<inotify_event*>(readPtr);
      readPtr += sizeof(inotify_event) + event->len;

      // Transform the event mask into a state:
      FileWatch::State states = FileWatch::State::NONE;
      if (event->mask & IN_MOVE_SELF) {
        states = states | FileWatch::State::RENAMED;
      }
      if (event->mask & IN_DELETE_SELF) {
        states = states | FileWatch::State::DELETED;
      }
      if (event->mask & (IN_MODIFY | IN_CREATE | IN_DELETE | IN_MOVE | IN_ATTRIB)) {
        if ((event->mask & IN_MOVED_TO) != IN_MOVED_TO || !event->cookie || event->cookie != m_moveCookie) {
          if (event->mask & IN_MOVED_FROM) {
            m_moveCookie = event->cookie;
          }
          states = states | FileWatch::State::MODIFIED;
        }
      }
      if (states == FileWatch::State::NONE)
        // Who cares, then.  Just circle around.
        continue;

      // Container of watchers we will need to notify:
      std::vector<std::shared_ptr<Watcher>> toBeNotified;

        // Try to find an interested watcher:
        {
          std::lock_guard<std::mutex> lock(m_mutex);
          auto found = m_watchers.find(event->wd);
          if (found != m_watchers.end())
            toBeNotified = found->second;
        }

        for (auto& cur : toBeNotified) {
          auto fileWatch = cur->fileWatch.lock();
          if (!fileWatch)
            // Already expired, circle around
            continue;

          try {
            cur->callback(fileWatch, states);
          } catch (...) {}
      }
    }
  }
}

std::shared_ptr<FileWatch> FileMonitorUnix::Watch(const std::filesystem::path& path,
                                                  const t_callbackFunc& callback,
                                                  FileWatch::State states)
{
  if (!std::filesystem::exists(path))
    // User is asking to watch something that doesn't exist, fail
    return std::shared_ptr<FileWatchUnix>();

  uint32_t mask = 0;
  if (states & FileWatch::State::RENAMED) {
    mask |= IN_MOVE_SELF;
  }
  if (states & FileWatch::State::DELETED) {
    mask |= IN_DELETE_SELF;
  }
  if (states & FileWatch::State::MODIFIED) {
    if (std::filesystem::is_directory(path)) {
      mask |= IN_CREATE;
      mask |= IN_DELETE;
      mask |= IN_MOVE;
    } else {
      mask |= IN_MODIFY;
    }
    mask |= IN_ATTRIB;
  }

  // If the user hasn't requested anything, trivially return
  if (mask == 0)
    return std::shared_ptr<FileWatch>();

  // Construct the watcher object, short-circuit if we hit a problem
  auto fileWatch = std::shared_ptr<FileWatchUnix>(
    new FileWatchUnix(path),
    [this] (FileWatchUnix* fileWatch) {
      const int wd = fileWatch->m_key;
      std::unique_ptr<FileWatchUnix> up(fileWatch);
      std::lock_guard<std::mutex> lock(m_mutex);

      // Grab the entry--don't bother searching, we expect to find this collection
      // at this point.  If we spuriously regenerate it, who cares.
      auto& watchers = m_watchers[wd];
      for (size_t i = watchers.size(); i--;)
        if (watchers[i]->fileWatch.expired()) {
          watchers[i] = watchers.back();
          watchers.pop_back();
        }

      // If all watchers are gone at this point, remove the whole registration:
      if (watchers.empty()) {
        // Remove from inotify
        ::inotify_rm_watch(m_inotify, wd);

        // Erase this entry
        m_watchers.erase(wd);
      }
    }
  );
  if (!fileWatch)
    return std::shared_ptr<FileWatch>();

  // Add to inotify
  std::lock_guard<std::mutex> lock(m_mutex);

  if (ShouldStop())
    return std::shared_ptr<FileWatch>();

  fileWatch->m_key = ::inotify_add_watch(m_inotify, fileWatch->Path().c_str(), mask);
  if (fileWatch->m_key < 0)
    return std::shared_ptr<FileWatch>();

  // Add to the set of watchers with this key:
  m_watchers[fileWatch->m_key].push_back(
    std::make_shared<Watcher>(Watcher{fileWatch, callback})
  );
  return fileWatch;
}

int FileMonitorUnix::WatchCount() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_watchers.size();
}
