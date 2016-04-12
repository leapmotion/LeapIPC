// Copyright (C) 2012-2016 Leap Motion, Inc. All rights reserved.
#pragma once
#include "FileMonitor.h"
#include <mutex>
#include <map>
#include <vector>
#include FILESYSTEM_HEADER

namespace leap {
namespace ipc {

class FileWatchUnix :
  public FileWatch
{
  public:
    FileWatchUnix(const std::filesystem::path& path);
    virtual ~FileWatchUnix();

  private:
    int m_key;

    friend class FileMonitorUnix;
};

class FileMonitorUnix :
  public FileMonitor
{
  public:
    FileMonitorUnix();
    virtual ~FileMonitorUnix();

  protected:
    // FileMonitor overrides:
    std::shared_ptr<FileWatch> Watch(const std::filesystem::path& path, const t_callbackFunc& callback, FileWatch::State states) override;
    int WatchCount() const override;

    // CoreThread overrides:
    void OnStop() override;
    void Run() override;

  private:
    struct Watcher {
      std::weak_ptr<FileWatchUnix> fileWatch;
      t_callbackFunc callback;
    };

    int m_inotify;
    int m_pipes[2];
    mutable std::mutex m_mutex;
    std::map<int, std::vector<std::shared_ptr<Watcher>>> m_watchers;
    uint32_t m_moveCookie;
};

}}
