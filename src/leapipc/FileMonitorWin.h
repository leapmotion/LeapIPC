// Copyright (C) 2012-2017 Leap Motion, Inc. All rights reserved.
#pragma once
#include "AutoHandle.h"
#include "FileMonitor.h"
#include <unordered_map>
#include <atomic>

namespace leap {
namespace ipc {

class FileWatchWin :
  public FileWatch
{
public:
  FileWatchWin(const std::filesystem::path& path, const FileMonitor::t_callbackFunc& callback, DWORD dwNotifyFilter, HANDLE hDirectory);
  virtual ~FileWatchWin();

private:
  const FileMonitor::t_callbackFunc m_callback;
  const DWORD m_dwNotifyFilter;
  const std::unique_ptr<OVERLAPPED> m_pOverlapped{ new OVERLAPPED };
  CAutoHandle<HANDLE> m_hDirectory;

  // Filename (without full path)
  std::wstring m_filename;

  // Becomes false when this file watcher has experienced an error
  bool m_ok = true;

  /// <summary>
  /// Queues up another ReadDirectoryChanges operation
  /// </summary>
  void ReadDirectoryChanges(void);

  /// <summary>
  /// Called when a ReadDirectory operation has completed
  /// </summary>
  void OnReadDirectoryComplete(void);

  /// <summary>
  /// Called when a directory operation has failed
  /// </summary>
  void OnReadDirectoryDeleted(void);

  union {
    // Hold at least three entries
    BYTE m_padding[3*(sizeof(FILE_NOTIFY_INFORMATION) + MAX_PATH * sizeof(WCHAR))];
    FILE_NOTIFY_INFORMATION m_notify;
  };

  friend class FileMonitorWin;
};

class FileMonitorWin :
  public FileMonitor
{
public:
  FileMonitorWin();
  virtual ~FileMonitorWin();

  // FileMonitor overrides:
  std::shared_ptr<FileWatch> Watch(const std::filesystem::path& path, const t_callbackFunc& callback, FileWatch::State states) override;
  int WatchCount() const override;

protected:
  // CoreThread overrides:
  void OnStop() override;
  void Run() override;

  // Completion port, used to gather up all of our read operatoins
  const CAutoHandle<HANDLE> m_hCompletion;

  struct Entry {
    std::weak_ptr<FileWatchWin> watcherWeak;
    ULONG_PTR key;
  };

  // Map of IO completion keys to known state blocks:
  ULONG_PTR m_nextKey = 1;
  std::unordered_map<ULONG_PTR, Entry> m_outstanding;

private:
  std::atomic<int> m_numWatchers;
};

}}
