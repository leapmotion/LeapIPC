// Copyright (c) 2010 - 2014 Leap Motion. All rights reserved. Proprietary and confidential.
#pragma once

#include <autowiring/CoreThread.h>
#include <memory>
#include FILESYSTEM_HEADER

namespace leap {
namespace ipc {

class FileWatch:
  public std::enable_shared_from_this<FileWatch>
{
public:
  enum class State : uint32_t {
    NONE     =  0U,

    // Receive notifications of file name changes (not file creation)
    RENAMED  = (1U << 0),

    // Receive notifications when file/directory deletions takes place (refers to the object being monitored)
    DELETED  = (1U << 1),

    // Receive notifications any time an adjustment is made to the time of last write.
    // When monitoring a directory, this is set when a file/directory within the
    // monitored directory is created, modified, or deleted.
    MODIFIED = (1U << 2),

    ALL      = (RENAMED | DELETED | MODIFIED)
  };

  FileWatch(const std::filesystem::wpath& path) :
    IsDirectory(std::filesystem::is_directory(path)),
    m_path(path)
  {}

  virtual ~FileWatch() {}

  // Indicates whether or not the file being monitored in a directory
  const bool IsDirectory;

  /// <summary>
  /// Retrieve file path being watched.
  /// </summary>
  const std::filesystem::wpath& Path() const { return m_path; }

protected:
  // Path of file/directory being monitored
  std::filesystem::wpath m_path;
};

inline FileWatch::State operator|(FileWatch::State a, FileWatch::State b) {
  return static_cast<FileWatch::State>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
inline bool operator&(FileWatch::State a, FileWatch::State b) {
  return !!(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

class FileMonitor :
  public CoreThread
{
  public:
    FileMonitor():
      CoreThread("FileMonitor")
    {}
    virtual ~FileMonitor() {}

    typedef std::function<void(std::shared_ptr<FileWatch>, FileWatch::State)> t_callbackFunc;

    /// <summary>
    /// Begin watching for activity on a particular file.
    /// </summary>
    virtual std::shared_ptr<FileWatch> Watch(const std::filesystem::wpath& path,
                                             const t_callbackFunc& callback,
                                             FileWatch::State states = FileWatch::State::ALL) = 0;

    /// <summary>
    /// Return the number of files being watched.
    /// </summary>
    virtual int WatchCount() const = 0;

    /// <summary>
    /// Creates a new FileMonitor instance
    /// </summary>
    static FileMonitor* New();
};

}}
