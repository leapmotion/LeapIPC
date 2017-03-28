// Copyright (C) 2012-2017 Leap Motion, Inc. All rights reserved.
#include "stdafx.h"
#include "IPCTestUtils.h"
#include <leapipc/IPCFileEndpoint.h>
#include <autowiring/autowiring.h>
#include <autowiring/CoreThread.h>
#include <cstdio>
#include <gtest/gtest.h>

using namespace leap::ipc;

class IPCFileEndpointTest :
  public testing::Test
{};

TEST_F(IPCFileEndpointTest, RawWriteToReadFromFile)
{
  std::string testMessage = "0123456789abcdef";
  const char* testFile = "IPCFileEndpoint_TestFile";

  // Write test message to file
  {
    std::shared_ptr<IPCFileEndpoint> ep = std::make_shared<IPCFileEndpoint>(testFile, false, true);
    ep->WriteRaw((void*)testMessage.c_str(), testMessage.size());
  }

  // Read test message from file
  {
    std::shared_ptr<IPCFileEndpoint> ep = std::make_shared<IPCFileEndpoint>(testFile, true, false);

    size_t bufsize = testMessage.size()+1;
    char* buf = new char[bufsize];
    buf[bufsize - 1] = 0;

    ep->ReadRaw((void*)buf, bufsize-1);

    ASSERT_EQ(strcmp(buf, testMessage.c_str()), 0);

    delete[] buf;
  }

  remove(testFile);
}