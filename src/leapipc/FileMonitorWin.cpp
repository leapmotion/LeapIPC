// Copyright (C) 2012-2018 Leap Motion, Inc. All rights reserved.
#include "stdafx.h"
#include "FileMonitorWin.h"
#include FILESYSTEM_HEADER
#include <codecvt>
#include <locale>

using namespace leap::ipc;

//
// FileWatchWin
//

FileWatchWin::FileWatchWin(const std::filesystem::path& path, const FileMonitor::t_callbackFunc& callback, DWORD dwNotifyFilter, HANDLE hDirectory) :
  FileWatch(path),
  m_callback(callback),
  m_dwNotifyFilter(dwNotifyFilter),
  m_hDirectory(hDirectory)
{
  std::string filename = path.filename().string();
  m_filename.assign(filename.begin(), filename.end());

  memset(m_pOverlapped.get(), 0, sizeof(*m_pOverlapped));
  m_pOverlapped->hEvent = CreateEvent(nullptr, true, false, nullptr);
}

FileWatchWin::~FileWatchWin(void) {
  CancelIoEx(m_hDirectory, m_pOverlapped.get());
}

void FileWatchWin::ReadDirectoryChanges(void) {
  m_ok = m_ok && ReadDirectoryChangesW(
    m_hDirectory,
    &m_notify,
    sizeof(m_padding),
    false,
    m_dwNotifyFilter,
    nullptr,
    m_pOverlapped.get(),
    nullptr
  );
}

void FileWatchWin::OnReadDirectoryComplete(void) {
  PFILE_NOTIFY_INFORMATION notify = nullptr, nextNotify = &m_notify;
  bool isMoving = false;

  while (nextNotify) {
    notify = nextNotify;
    nextNotify = notify->NextEntryOffset > 0 ?
         reinterpret_cast<PFILE_NOTIFY_INFORMATION>(reinterpret_cast<PBYTE>(notify) + notify->NextEntryOffset) :
         nullptr;

    State state = State::NONE;

    // Flag, set if this notification pertains directly to this entry
    bool isForSelf =
      static_cast<DWORD>(m_filename.size()*sizeof(WCHAR)) == notify->FileNameLength &&
      ::memcmp(m_filename.c_str(), notify->FileName, notify->FileNameLength) == 0;

    // In the case of single files that aren't being renamed, we only process this message if it's for us
    if(!IsDirectory && (!isMoving || notify->Action != FILE_ACTION_RENAMED_NEW_NAME)) {
      // If the message is not for us, move on
      if (!isForSelf)
        continue;
    }

    switch(notify->Action) {
    case FILE_ACTION_ADDED:
      state = State::MODIFIED;
      break;
    case FILE_ACTION_REMOVED:
      if (isForSelf) {
        // The filesystem will preserve the entry reference in the file system if a process attempts to
        // delete a directory for which it has an open handle, and the file system will also prevent any
        // changes from being made to the directory after it has been deleted in this way.  Thus, we must
        // ensure that we CLOSE our handle to the file after the directory has been deleted, so as to
        // prevent this type of zombie file from continuing to exist on the disk after its rightful deletion.
        m_hDirectory.Close();
        m_ok = false;
        state = State::DELETED;
      }
      else
        // Directories always say "modified," never "deleted," because at this point a removal event always
        // refers to a file within the directory and not the directory itself.  The directory itself being
        // deleted is handled by the above case.
        state = IsDirectory ? State::MODIFIED : State::DELETED;
      break;
    case FILE_ACTION_MODIFIED:
      if (!IsDirectory)
        state = State::MODIFIED;
      break;
    case FILE_ACTION_RENAMED_OLD_NAME:
      state = IsDirectory ? State::MODIFIED : State::RENAMED;

      // We are moving if and only if this notification is for us
      isMoving = isForSelf;
      break;
    case FILE_ACTION_RENAMED_NEW_NAME:
      if(isMoving) {
        m_filename = std::wstring(notify->FileName, notify->FileNameLength/sizeof(WCHAR));
        isMoving = false;
        continue;
      } else {
        state = IsDirectory ? State::MODIFIED : State::RENAMED;
      }
      break;
    default:
      // No idea what to do here
      continue;
    }

    try {
      m_callback(shared_from_this(), state);
    } catch (...) {}
  }
}

void FileWatchWin::OnReadDirectoryDeleted(void) {
  m_ok = false;
  m_hDirectory.Close();
  m_callback(shared_from_this(), State::DELETED);
}

//
// FileMonitorWin
//

FileMonitorWin::FileMonitorWin(void):
  m_hCompletion(CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, (ULONG_PTR)this, 0))
{
}

FileMonitorWin::~FileMonitorWin()
{
}

FileMonitor* FileMonitor::New(void) {
  return new FileMonitorWin();
}

std::shared_ptr<FileWatch> FileMonitorWin::Watch(const std::filesystem::path& path, const t_callbackFunc& callback, FileWatch::State states) {
  try {
    if(!std::filesystem::exists(path))
      return nullptr;
  } catch (...) {
    return nullptr;
  }
  std::filesystem::path directory = (std::filesystem::is_directory(path) || !path.has_parent_path()) ? path : path.parent_path();

  // Ugh.
  std::wstring wpath;
#if _MSC_VER >= 1900
  wpath = directory.wstring();
#else
  auto str = directory.string();
  wpath.assign(str.begin(), str.end());
#endif

  // Open the handle to the directory we were asked to watch with the
  // correct permissions so that we can actually conduct asynchronous
  // read operations.
  HANDLE hFile = CreateFileW(
    wpath.c_str(),
    GENERIC_READ,
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
    nullptr,
    OPEN_EXISTING,
    FILE_FLAG_OVERLAPPED | FILE_FLAG_BACKUP_SEMANTICS,
    nullptr
  );
  if (hFile == INVALID_HANDLE_VALUE)
    // No such file, what else can we do?
    return nullptr;

  // Compose the notification filter based on the user's request
  DWORD dwNotifyFilter = 0;
  if(states & FileWatch::State::RENAMED || states & FileWatch::State::DELETED)
    dwNotifyFilter |= FILE_NOTIFY_CHANGE_FILE_NAME;
  if(states & FileWatch::State::MODIFIED)
    dwNotifyFilter |= FILE_NOTIFY_CHANGE_LAST_WRITE;

  // Need a second-order shared pointer so we can preserve references
  Entry* pEntry;
  auto key = m_nextKey++;
  {
    std::lock_guard<std::mutex> lk(GetLock());
    pEntry = &m_outstanding[key];
    pEntry->key = key;
  }

  // Need an overlapped structure to track this operation.  We'll also be using this to decide
  // how to notify the true caller.
  auto watcher = std::shared_ptr<FileWatchWin>(
    new FileWatchWin(path, callback, dwNotifyFilter, hFile),
    [this, key](FileWatchWin* fileWatch) {
      --m_numWatchers;
      delete fileWatch;
      m_outstanding.erase(key);
    }
  );
  pEntry->watcherWeak = watcher;

  // Attach to the completion port with the FileWatchWin we just constructed
  if (!CreateIoCompletionPort(hFile, m_hCompletion, key, 0)) {
    // Something went wrong, can't attach a watcher at this point
    m_outstanding.erase(key);
    return nullptr;
  }

  // Initial pend, and then return to the controller
  watcher->ReadDirectoryChanges();
  ++m_numWatchers;
  return watcher;
}

int FileMonitorWin::WatchCount() const {
  return m_numWatchers;
}

void FileMonitorWin::OnStop() {
  // Time to go away
  PostQueuedCompletionStatus(m_hCompletion, 0, 0, nullptr);
}

void FileMonitorWin::Run()
{
  while(!ShouldStop()) {
    // Wait for something to happen
    DWORD dwBytesTransferred;
    ULONG_PTR key;
    LPOVERLAPPED lpOverlapped;
    BOOL status;

    // Wait until woken up by the underlying system or a sentry notification by our
    // OnStop handler
    status = GetQueuedCompletionStatus(
      m_hCompletion,
      &dwBytesTransferred,
      (PULONG_PTR) &key,
      &lpOverlapped,
      INFINITE
    );

    // If the overlapped structure doesn't exist, verify that we haven't been
    // told to stop
    if(!lpOverlapped)
      continue;

    // Dereference shared pointer, decide what to do with this entity:
    auto q = m_outstanding.find(key);
    if (q == m_outstanding.end())
      // Who knows, next
      continue;

    auto& entry = q->second;
    auto sp = entry.watcherWeak.lock();
    if (!sp)
      // Failed to lock in time, nothing to do here
      continue;

    auto teardown = [&] {
      switch (GetLastError()) {
      case ERROR_ACCESS_DENIED:
        // File is now gone, report to the user.
        sp->OnReadDirectoryDeleted();
        m_outstanding.erase(q);
        break;
      }
    };

    // Failure occurred?
    if (!status)
      teardown();

    // Process the notification if any bytes were transferred
    if(dwBytesTransferred)
      sp->OnReadDirectoryComplete();

    // Schedule another operation
    sp->ReadDirectoryChanges();

    // If we failed, we need to clean up here too
    if (!sp->m_ok)
      teardown();
  }
}
