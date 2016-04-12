// Copyright (C) 2012-2016 Leap Motion, Inc. All rights reserved.
#include "stdafx.h"
#include <autowiring/autowiring.h>
#include <autowiring/CoreContext.h>
#include <leapipc/FileMonitor.h>
#include <gtest/gtest.h>
#include <array>
#include <fstream>
#include <thread>
#include CHRONO_HEADER
#include FILESYSTEM_HEADER

using namespace leap::ipc;

class FileMonitorTest:
  public testing::Test
{
public:
  void SetUp(void) override;
  void TearDown(void) override;

  // Directory where all temporary files will be created
  std::filesystem::path parent;

  static std::wstring MakeRandomName(void) {
    static uint32_t s_counter = 0;
    std::wostringstream oss;
    oss << std::chrono::profiling_clock::now().time_since_epoch().count()
        << "." << ++s_counter;
    return oss.str();
  }

  std::filesystem::path GetTemporaryName() {
    return parent / std::filesystem::path(MakeRandomName());
  }

  static bool SetFileContent(const std::filesystem::path& path, const std::string& content, std::ios_base::openmode mode = std::ios_base::out) {
    std::ofstream ofs(path.string(), mode);
    if (!ofs)
      return false;

    ofs.write(content.data(), content.size());
    ofs.close();
    return true;
  }
};

void FileMonitorTest::SetUp(void) {
  AutoCurrentContext ctxt;
  ctxt->Initiate();

  parent = L"com.leapmotion.FileMonitorTest." + MakeRandomName();
  ASSERT_TRUE(std::filesystem::create_directory(parent));
  parent = std::filesystem::canonical(parent);
  ASSERT_TRUE(std::filesystem::exists(parent));
}

void FileMonitorTest::TearDown(void) {
  AutoCurrentContext ctxt;
  ctxt->SignalShutdown();
  ctxt->Wait();

  // We don't care if this fails, some tests actually do remove the parent directory anyway
  std::filesystem::remove_all(parent);
}

// Monitor object, used during tests to avoid synchronization issues:
struct FileMonitorTestMonitorObject {
  std::mutex lock;
  std::condition_variable cond;
  bool dirModified{false};
  bool dirDeleted{false};

  // True if the watch directory still existed when the callback was received
  bool dirOnDiskExists{false};
};

TEST_F(FileMonitorTest, MonitorDirectory) {
  AutoCurrentContext serverContext;
  AutoRequired<FileMonitor> fm;
  serverContext->Initiate();

  std::filesystem::path one = GetTemporaryName();
  std::filesystem::path two = GetTemporaryName();

  // We must use a shared pointer here because watch lambdas are executed asynchronously--it is a real
  // risk that this function could already be exited even after a notification is on its way.
  auto fmtmo = std::make_shared<FileMonitorTestMonitorObject>();

  auto dirWatcher = fm->Watch(
    parent,
    [this, fmtmo] (std::shared_ptr<FileWatch>, FileWatch::State states) {
      std::lock_guard<std::mutex> lock(fmtmo->lock);
      if (states & FileWatch::State::MODIFIED)
        fmtmo->dirModified = true;
      if (states & FileWatch::State::DELETED)
        fmtmo->dirDeleted = true;
      try {
        // Will sometimes throw an exception if there's something funky going on with disk access
        fmtmo->dirOnDiskExists = std::filesystem::exists(parent);
      }
      catch (...) {
        fmtmo->dirOnDiskExists = false;
      }

      fmtmo->cond.notify_all();
    },
    FileWatch::State::MODIFIED | FileWatch::State::DELETED
  );
  ASSERT_NE(nullptr, dirWatcher) << "Failed to attach directory watcher";

  // Initial double-check:
  std::unique_lock<std::mutex> lock(fmtmo->lock);
  ASSERT_FALSE(
    fmtmo->cond.wait_for(
      lock,
      std::chrono::milliseconds(100),
      [&] { return fmtmo->dirModified || fmtmo->dirDeleted; }
    )
  ) << "Directory modification detected before anything had a chance to happen";

  // Create first file in parent directory
  ASSERT_TRUE(SetFileContent(one, "one"));
  ASSERT_TRUE(std::filesystem::exists(one));

  ASSERT_TRUE(fmtmo->cond.wait_for(lock, std::chrono::seconds(5), [&] { return fmtmo->dirModified; })) << "Directory modification indicator not set in time";
  ASSERT_FALSE(fmtmo->dirDeleted);
  fmtmo->dirModified = false;

  // Modify existing file in directory being monitor; don't expect any changes.
  SetFileContent(one, "plus", std::ios_base::out | std::ios_base::app);

  ASSERT_FALSE(fmtmo->cond.wait_for(lock, std::chrono::milliseconds(250), [&] { return fmtmo->dirModified; })) << "Directory modification indicator set when it shouldn't have been";
  ASSERT_FALSE(fmtmo->dirDeleted);

  // Create second file in parent directory
  SetFileContent(two, "two");
  ASSERT_TRUE(std::filesystem::exists(two));
  ASSERT_TRUE(fmtmo->cond.wait_for(lock, std::chrono::seconds(5), [&] { return fmtmo->dirModified; })) << "Secondary modification not detected in time";
  ASSERT_FALSE(fmtmo->dirDeleted);
  fmtmo->dirModified = false;

  // Remove first file
  std::filesystem::remove(one);
  ASSERT_FALSE(std::filesystem::exists(one));
  ASSERT_TRUE(fmtmo->cond.wait_for(lock, std::chrono::seconds(5), [&] { return fmtmo->dirModified; })) << "Directory modification not detected in time";
  ASSERT_FALSE(fmtmo->dirDeleted);
  fmtmo->dirModified = false;

  // Rename second file to first file
  std::filesystem::rename(two, one);
  ASSERT_TRUE(std::filesystem::exists(one));
  ASSERT_FALSE(std::filesystem::exists(two));
  ASSERT_TRUE(fmtmo->cond.wait_for(lock, std::chrono::seconds(5), [&] { return fmtmo->dirModified; }));
  ASSERT_FALSE(fmtmo->dirDeleted);
  fmtmo->dirModified = false;

  // Remove first file (which used to be second file)
  ASSERT_TRUE(std::filesystem::remove(one)) << "Failed to remove a test file";
  ASSERT_FALSE(std::filesystem::exists(one));
  ASSERT_TRUE(fmtmo->cond.wait_for(lock, std::chrono::seconds(5), [&] { return fmtmo->dirModified; }));
  ASSERT_FALSE(fmtmo->dirDeleted);
  fmtmo->dirModified = false;

  // Remove the file.  The underlying monitor might need to respond to this event
  ASSERT_TRUE(std::filesystem::is_empty(parent)) << "Test directory should have been empty";
  std::filesystem::remove_all(parent);

  // Standard state update check
  ASSERT_TRUE(fmtmo->cond.wait_for(lock, std::chrono::seconds(5), [&] { return fmtmo->dirDeleted; }));
  ASSERT_FALSE(fmtmo->dirModified);
  fmtmo->dirDeleted = false;

  // Verify that, by this point, the file is actually gone.  Technically it should be gone
  // from disk before control is ever handed to the callback routine.
  ASSERT_FALSE(fmtmo->dirOnDiskExists) <<
    "Directory still existed on disk at the time of the directory watching notification when the directory in question was already deleted";
}

// Another monitor object
struct OneDeletedTwoDeleted {
  std::mutex lock;
  std::condition_variable cond;
  bool oneDeleted{false};
  bool twoDeleted{false};

  // True if the watch directory still existed when the callback was received
  bool dirOnDiskExists{false};
};

TEST_F(FileMonitorTest, MonitorMultipleDeletes) {
  AutoCurrentContext serverContext;
  AutoRequired<FileMonitor> fm;
  serverContext->Initiate();

  std::filesystem::path one = GetTemporaryName();
  std::filesystem::path two  = GetTemporaryName();

  SetFileContent(one, "one");
  SetFileContent(two, "two");

  auto odtd = std::make_shared<OneDeletedTwoDeleted>();
  auto oneWatcher = fm->Watch(
    one,
    [odtd] (std::shared_ptr<FileWatch> fileWatch, FileWatch::State states) {
      if (states != FileWatch::State::DELETED)
        return;

      std::lock_guard<std::mutex> lk(odtd->lock);
      odtd->oneDeleted = true;
      odtd->cond.notify_all();
    },
    FileWatch::State::DELETED
  );

  auto twoWatcher = fm->Watch(
    two,
    [odtd] (std::shared_ptr<FileWatch> fileWatch, FileWatch::State states) {
      if (states != FileWatch::State::DELETED)
        return;

      std::lock_guard<std::mutex> lk(odtd->lock);
      odtd->twoDeleted = true;
      odtd->cond.notify_all();
    },
    FileWatch::State::DELETED
  );

  std::unique_lock<std::mutex> lk(odtd->lock);

  // Delay until the first variable is notified
  ASSERT_TRUE(std::filesystem::remove(two));
  ASSERT_TRUE(odtd->cond.wait_for(lk, std::chrono::seconds(5), [&] { return odtd->twoDeleted; })) << "File deletion was not detected in a timely fashion";
  ASSERT_FALSE(odtd->oneDeleted) << "File was prematurely detected as having been deleted";

  // Now see that the second one is notified, too:
  ASSERT_TRUE(std::filesystem::remove(one));
  ASSERT_TRUE(odtd->cond.wait_for(lk, std::chrono::seconds(5), [&] { return odtd->oneDeleted; })) << "Secondary file deletion was not detected in a timely fashion";
}

// Yet another monitor object
struct OneDeletedTwoDeletedAry {
  std::mutex lock;
  std::condition_variable cond;

  std::array<int, 2> oneDeletedAry;
  std::array<int, 2> twoDeletedAry;

  // True if the watch directory still existed when the callback was received
  bool dirOnDiskExists{false};
};


TEST_F(FileMonitorTest, MultipleWatchersSameFile) {
  AutoCurrentContext serverContext;
  AutoRequired<FileMonitor> fm;
  serverContext->Initiate();

  std::filesystem::path one = GetTemporaryName();
  std::filesystem::path two = GetTemporaryName();
  SetFileContent(one, "one");
  SetFileContent(two, "two");
  ASSERT_TRUE(std::filesystem::exists(one));
  ASSERT_TRUE(std::filesystem::exists(two));

  std::shared_ptr<FileWatch> oneWatcher[2];
  std::shared_ptr<FileWatch> twoWatcher[2];

  auto odtd = std::make_shared<OneDeletedTwoDeletedAry>();
  odtd->oneDeletedAry[0] = 0;
  odtd->oneDeletedAry[1] = 0;
  odtd->twoDeletedAry[0] = 0;
  odtd->twoDeletedAry[1] = 0;

  for (size_t i = 0; i < 2; i++) {
    oneWatcher[i] = fm->Watch(
      one,
      [odtd, i] (std::shared_ptr<FileWatch> fileWatch, FileWatch::State states) {
        std::lock_guard<std::mutex> lock(odtd->lock);
        ++odtd->oneDeletedAry[i];
        odtd->cond.notify_all();
      },
      FileWatch::State::DELETED
    );
    ASSERT_NE(nullptr, oneWatcher[i]) << "Failed to attach oneWatcher[" << i << "]";

    twoWatcher[i] = fm->Watch(
      two,
      [odtd, i] (std::shared_ptr<FileWatch> fileWatch, FileWatch::State states) {
        std::lock_guard<std::mutex> lock(odtd->lock);
        ++odtd->twoDeletedAry[i];
        odtd->cond.notify_all();
      },
      FileWatch::State::DELETED
    );
    ASSERT_NE(nullptr, twoWatcher[i]) << "Failed to attach twoWatcher[" << i << "]";
  }

  std::unique_lock<std::mutex> lock(odtd->lock);
  std::filesystem::remove(one);
  ASSERT_TRUE(odtd->cond.wait_for(lock, std::chrono::seconds(5), [odtd] { return odtd->oneDeletedAry[0] && odtd->oneDeletedAry[1]; })) << "File deletion notice not received in a timely fashion";
  ASSERT_EQ(0, odtd->twoDeletedAry[0]);
  ASSERT_EQ(0, odtd->twoDeletedAry[1]);

  std::filesystem::remove(two);
  odtd->cond.wait_for(lock, std::chrono::seconds(20), [&] { return odtd->twoDeletedAry[0] && odtd->twoDeletedAry[1]; });

  ASSERT_EQ(1, odtd->oneDeletedAry[0]) << "First registered watcher of 'one' was not fired, std::filesystem::exists(one) = " << std::filesystem::exists(one);
  ASSERT_EQ(1, odtd->oneDeletedAry[1]) << "Second registered watcher of 'one' was not fired, std::filesystem::exists(one) = " << std::filesystem::exists(one);
  ASSERT_EQ(1, odtd->twoDeletedAry[0]) << "First registered watcher of 'two' was not fired, std::filesystem::exists(two) = " << std::filesystem::exists(two);
  ASSERT_EQ(1, odtd->twoDeletedAry[1]) << "Second registered watcher of 'two' was not fired, std::filesystem::exists(two) = " << std::filesystem::exists(two);
}

TEST_F(FileMonitorTest, FileNotFound) {
  AutoCurrentContext serverContext;
  AutoRequired<FileMonitor> fm;
  serverContext->Initiate();

  std::filesystem::path missing = GetTemporaryName();

  auto watcher = fm->Watch(
    missing,
    [] (std::shared_ptr<FileWatch> fileWatch, FileWatch::State states) {},
    FileWatch::State::ALL
  );
  ASSERT_TRUE(nullptr == watcher.get()) << "An attempt to watch a nonexistent file should not have succeeded";
}

#if !defined(_MSC_VER)

#include <sys/stat.h>

// Monitor object, used during tests to avoid synchronization issues:
struct FileMonitorTestMonitorAttributes {
  std::mutex lock;
  std::condition_variable cond;
  bool attrChanged{false};
};

TEST_F(FileMonitorTest, ModifiedAttributes) {
  AutoCurrentContext serverContext;
  AutoRequired<FileMonitor> fm;
  serverContext->Initiate();

  std::filesystem::path one = GetTemporaryName();
  auto cleanup = MakeAtExit([one]{ std::filesystem::remove(one); });

  SetFileContent(one, "one");

  // We must use a shared pointer here because watch lambdas are executed asynchronously--it is a real
  // risk that this function could already be exited even after a notification is on its way.
  auto fmtma = std::make_shared<FileMonitorTestMonitorAttributes>();

  auto oneWatcher = fm->Watch(one,
                              [fmtma]
                              (std::shared_ptr<FileWatch> fileWatch, FileWatch::State states) {
                                std::lock_guard<std::mutex> lock(fmtma->lock);
                                if (states == FileWatch::State::MODIFIED) {
                                  fmtma->attrChanged = true;
                                  fmtma->cond.notify_all();
                                }
                              },
                              FileWatch::State::MODIFIED);

  // Initial check:
  std::unique_lock<std::mutex> lock(fmtma->lock);
  ASSERT_FALSE(
    fmtma->cond.wait_for(
      lock,
      std::chrono::milliseconds(100),
      [&] { return fmtma->attrChanged; }
    )
  ) << "File was modified before anything had a chance to happen";

  ::chmod(one.c_str(), 0511);

  ASSERT_TRUE(fmtma->cond.wait_for(lock, std::chrono::seconds(5), [&] { return fmtma->attrChanged; })) << "File modification indicator not set in time";
}
#endif

TEST_F(FileMonitorTest, WatchMissingFile) {
  AutoCurrentContext serverContext;
  AutoRequired<FileMonitor> fm;
  serverContext->Initiate();

  std::filesystem::path missing = GetTemporaryName();

  auto watcher = fm->Watch(missing, [] (std::shared_ptr<FileWatch> fileWatch, FileWatch::State states) {});

  ASSERT_EQ(nullptr, watcher);
}

TEST_F(FileMonitorTest, DISABLED_MonitorFileStateChanges) {
  AutoCreateContext serverContext;
  AutoRequired<FileMonitor> fm;
  serverContext->Initiate();

  std::filesystem::path original = GetTemporaryName();
  std::filesystem::path renamed  = GetTemporaryName();

  ASSERT_TRUE(SetFileContent(original, "")) << "Unable to create temporary file";

  std::mutex mutex;
  std::condition_variable cond;
  std::atomic<FileWatch::State> eventStates{FileWatch::State::NONE}, matchStates{FileWatch::State::NONE};

  auto watcher = fm->Watch(original,
                           [&mutex, &cond, &matchStates, &eventStates]
                           (std::shared_ptr<FileWatch> fileWatch, FileWatch::State states) {
                             std::unique_lock<std::mutex> lock(mutex);
                             if (states & matchStates) {
                               eventStates = states;
                               cond.notify_all();
                             }
                           },
                           FileWatch::State::ALL);
  // Validate basic behavior
  ASSERT_NE(nullptr, watcher);
  ASSERT_TRUE(watcher.unique());
  ASSERT_EQ(original, watcher->Path());
  ASSERT_EQ(1, fm->WatchCount());

  // Make sure that we don't receive any events before anything happens to the file
  std::unique_lock<std::mutex> lock(mutex);
  cond.wait_for(lock, std::chrono::milliseconds(10),
                [&eventStates] { return eventStates != FileWatch::State::NONE; });
  ASSERT_EQ(FileWatch::State::NONE, eventStates);
  // For next test, check for all (we should only receive MODIFIED)
  matchStates = FileWatch::State::ALL;
  lock.unlock();

  // ACTION: Write to the file
  SetFileContent(original, "content");
  // Wait for the notification, or timeout due to it not happening
  lock.lock();
  cond.wait_for(lock, std::chrono::milliseconds(500),
                [&eventStates] { return eventStates == FileWatch::State::MODIFIED; });
  ASSERT_EQ(FileWatch::State::MODIFIED, eventStates) << "State is not solely in the MODIFIED state";
  // For next test, only check for RENAMED and DELETED (we may receive MODIFIED, so ignore those)
  matchStates = FileWatch::State::RENAMED | FileWatch::State::DELETED;
  eventStates = FileWatch::State::NONE;
  lock.unlock();

  // ACTION: Rename the file
  std::filesystem::rename(original, renamed);
  // Wait for the notification, or timeout due to it not happening
  lock.lock();
  cond.wait_for(lock, std::chrono::milliseconds(500),
                [&eventStates] { return eventStates == FileWatch::State::RENAMED; });
  ASSERT_EQ(FileWatch::State::RENAMED, eventStates) << "State is not solely in the RENAMED state";
  // For next test, only check for DELETED events (we may receive RENAMED OR MODIFIED, so ignore those)
  matchStates = FileWatch::State::DELETED;
  eventStates = FileWatch::State::NONE;
  lock.unlock();

  // ACTION: Delete the file
  std::filesystem::remove(renamed);
  // Wait for the notification, or timeout due to it not happening
  lock.lock();
  cond.wait_for(lock, std::chrono::milliseconds(500),
                [&eventStates] { return eventStates == FileWatch::State::DELETED; });
  ASSERT_EQ(FileWatch::State::DELETED, eventStates) << "State is not solely in the DELETED state";
  lock.unlock();

  std::weak_ptr<FileWatch> watcherWeak = watcher;
  watcher.reset();

  auto start = std::chrono::system_clock::now();
  while(!watcherWeak.expired())
    ASSERT_TRUE(start + std::chrono::seconds(1) > std::chrono::system_clock::now()) << "Released watcher shared pointer took too long to expire";
  ASSERT_EQ(0, fm->WatchCount()) << "Did not properly cleanup after removing watcher";
}
