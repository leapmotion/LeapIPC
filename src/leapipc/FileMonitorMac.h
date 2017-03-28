// Copyright (C) 2012-2017 Leap Motion, Inc. All rights reserved.
#pragma once
#include "FileMonitor.h"

#include <mutex>
#include <map>

namespace leap {
namespace ipc {

class FileWatchMac :
  public FileWatch
{
  public:
    FileWatchMac(const std::filesystem::path& path);
    virtual ~FileWatchMac();

  private:
    int m_key;

    friend class FileMonitorMac;
};

class FileMonitorMac :
  public FileMonitor
{
  public:
    FileMonitorMac();
    virtual ~FileMonitorMac();

  protected:
    // FileMonitor overrides:
    std::shared_ptr<FileWatch> Watch(const std::filesystem::path& path, const t_callbackFunc& callback, FileWatch::State states) override;
    int WatchCount() const override;

    // CoreThread overrides:
    void OnStop() override;
    void Run() override;

  private:
    struct Watcher {
      std::weak_ptr<FileWatchMac> fileWatch;
      t_callbackFunc callback;
    };

    int m_kqueue;
    mutable std::mutex m_mutex;
    std::map<int, std::shared_ptr<Watcher>> m_watchers;
};

}}
